#include "commands/cmake.hpp"
#include "utils/llm_client.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace {

// 判断是否是需要忽略的目录
bool should_ignore(const fs::path& path) {
    std::string path_str = path.string();
    return path_str.find("/build") != std::string::npos ||
           path_str.find("/.git") != std::string::npos ||
           path_str.find("/.vscode") != std::string::npos;
}

// 判断是否是 C/C++ 源码文件
bool is_source_file(const fs::path& path) {
    std::string ext = path.extension().string();
    return ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".h" || ext == ".cc";
}

// 提取文件中的 #include 语句
std::string extract_includes(const fs::path& file_path) {
    std::ifstream file(file_path);
    std::string line;
    std::stringstream includes;
    while (std::getline(file, line)) {
        // 简单去除前导空格
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line.substr(start, 8) == "#include") {
            includes << line << "\n";
        }
    }
    return includes.str();
}

// 剥离大模型可能返回的 Markdown 代码块标记 (如 ```cmake 和 ```)
std::string clean_markdown(std::string text) {
    // 查找并删除开头的 ```cmake 或 ```
    size_t start_pos = text.find("```");
    if (start_pos != std::string::npos) {
        size_t newline_pos = text.find('\n', start_pos);
        if (newline_pos != std::string::npos) {
            text.erase(0, newline_pos + 1);
        }
    }
    // 查找并删除结尾的 ```
    size_t end_pos = text.rfind("```");
    if (end_pos != std::string::npos) {
        text.erase(end_pos);
    }
    
    // 清除首尾多余的空白符
    text.erase(0, text.find_first_not_of(" \n\r\t"));
    text.erase(text.find_last_not_of(" \n\r\t") + 1);
    
    return text;
}

} // 匿名命名空间结束

// ... 保持顶部 include 和 匿名命名空间里的辅助函数不变 ...

namespace cbot {
namespace commands {

void handle_cmake(const std::string& target_path) {
    fs::path work_dir;

    // 1. 规范化目标路径（自动处理相对路径、.. 以及不存在的路径报错）
    try {
        work_dir = fs::canonical(fs::path(target_path));
        if (!fs::is_directory(work_dir)) {
            std::cerr << "错误: 指定的路径不是一个有效的目录 -> " << work_dir.string() << std::endl;
            return;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "路径无效或不存在: " << e.what() << std::endl;
        return;
    }

    std::cout << "正在扫描目标项目结构: " << work_dir.string() << std::endl;

    std::stringstream project_context;
    project_context << "项目根目录名: " << work_dir.filename().string() << "\n\n";
    project_context << "项目源码与依赖结构:\n";

    int file_count = 0;

    // 2. 扫描文件系统，使用规范化后的 work_dir 作为基准
    try {
        for (const auto& entry : fs::recursive_directory_iterator(work_dir)) {
            if (entry.is_regular_file() && !should_ignore(entry.path()) && is_source_file(entry.path())) {
                fs::path relative_path = fs::relative(entry.path(), work_dir);
                project_context << "文件: " << relative_path.string() << "\n";
                
                std::string includes = extract_includes(entry.path());
                if (!includes.empty()) {
                    project_context << "包含的头文件:\n" << includes;
                }
                project_context << "---\n";
                file_count++;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "文件扫描错误: " << e.what() << std::endl;
        return;
    }

    if (file_count == 0) {
        std::cerr << "错误: 在目标目录下未发现任何 C/C++ 源码文件。" << std::endl;
        return;
    }

    std::cout << "扫描完成，共发现 " << file_count << " 个源码文件。正在请求大模型推断 CMake 配置..." << std::endl;

    // 3. 网络请求
    cbot::utils::LLMClient client; 
    
    // 使用 Raw String 注入带有模板的强约束提示词
    std::string system_prompt = R"(
你是一个顶级的 C++ 构建工程师。请根据用户提供的 C++ 项目目录结构和头文件依赖关系，编写 CMakeLists.txt。
必须【严格】按照以下模板和风格生成，绝不允许偏离模板，绝不允许引入项目中未使用的第三方库（如 fmt, spdlog 等）！

【核心要求】：
1. 项目源码都在 src/ 下，直接使用 file(GLOB_RECURSE) 收集，禁止二次过滤。
2. 依赖管理：轻量级库找不到就 FetchContent，重量级库找不到就 FATAL_ERROR 报错。
3. 链接风格：统一使用现代的 target_link_libraries(target PRIVATE namespace::lib) 格式。

【输出模板】（请严格复刻此结构，将依赖补充在指定位置）：
cmake_minimum_required(VERSION 3.15)
project(YourProjectName LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. 收集源码
file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# 2. 头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 3. 依赖管理 (请根据项目实际包含的头文件推断并在此处生成)
# 示例：轻量级库
# find_package(cpr QUIET)
# if(NOT cpr_FOUND)
#     include(FetchContent)
#     FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 1.10.5)
#     set(CPR_USE_SYSTEM_CURL ON)
#     FetchContent_MakeAvailable(cpr)
# endif()
# target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)

# 示例：重量级库
# find_package(OpenCV QUIET)
# if(NOT OpenCV_FOUND)
#     message(FATAL_ERROR "未找到 OpenCV，请手动安装。")
# endif()
# target_link_libraries(${PROJECT_NAME} PRIVATE opencv_core)

绝对不要输出任何 Markdown 格式（不要用 ``` 包裹），绝对不要输出任何解释性文字，只输出最终的纯文本代码！
)";

    auto response = client.generate_response(system_prompt, project_context.str());

    if (!response) {
        std::cerr << "CMakeLists.txt 生成失败：大模型无响应。" << std::endl;
        return;
    }

    // 4. 清洗和写入文件（确保写入到目标目录 work_dir 下）
    std::string clean_content = clean_markdown(response.value());
    fs::path cmake_path = work_dir / "CMakeLists.txt";

    // 交互式覆写确认机制
    if (fs::exists(cmake_path)) {
        std::cout << "\n⚠️ 警告: 目标目录 [" << work_dir.string() << "] 下已存在 CMakeLists.txt。" << std::endl;
        std::cout << "是否要让 AI 覆写它？(原文件会自动备份) [y/N]: ";
        
        std::string answer;
        std::getline(std::cin, answer);

        if (answer != "y" && answer != "Y") {
            std::cout << "\n操作已取消，目标目录的 CMakeLists.txt 保持不变。" << std::endl;
            std::cout << "以下是大模型生成的配置，供你手动复制或参考：\n";
            std::cout << std::string(50, '-') << "\n";
            std::cout << clean_content << "\n";
            std::cout << std::string(50, '-') << "\n";
            return;
        }

        // 备份机制（备份到目标目录下）
        fs::path backup_path = work_dir / "CMakeLists.txt.bak";
        try {
            fs::copy_file(cmake_path, backup_path, fs::copy_options::overwrite_existing);
            std::cout << "已将原文件安全备份为 CMakeLists.txt.bak" << std::endl;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "备份旧 CMakeLists.txt 失败: " << e.what() << std::endl;
        }
    }

    std::ofstream out_file(cmake_path);
    if (out_file.is_open()) {
        out_file << clean_content;
        out_file.close();
        std::cout << "成功: CMakeLists.txt 已智能生成并写入目标目录！" << std::endl;
    } else {
        std::cerr << "错误: 无法写入 CMakeLists.txt 文件。" << std::endl;
    }
}

} // namespace commands
} // namespace cbot