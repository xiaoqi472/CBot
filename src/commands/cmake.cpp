/**
 * @file cmake.hpp
 * @brief 包含处理 CMake 相关命令的函数。
 */

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

/**
 * @brief 判断给定的路径是否是需要忽略的目录。
 *
 * 忽略的目录包括 "build", ".git", ".vscode"。
 *
 * @param path 要检查的路径。
 * @return 如果路径包含任何一个忽略的目录，则返回 true；否则返回 false。
 */
bool should_ignore(const fs::path& path) {
    std::string path_str = path.string();
    return path_str.find("/build") != std::string::npos ||
           path_str.find("/.git") != std::string::npos ||
           path_str.find("/.vscode") != std::string::npos;
}

/**
 * @brief 判断给定的路径是否是 C/C++ 源码文件。
 *
 * 支持的扩展名包括 ".cpp", ".hpp", ".c", ".h", ".cc"。
 *
 * @param path 要检查的文件路径。
 * @return 如果是 C/C++ 源码文件，则返回 true；否则返回 false。
 */
bool is_source_file(const fs::path& path) {
    std::string ext = path.extension().string();
    return ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".h" || ext == ".cc";
}

/**
 * @brief 从指定文件中提取所有 #include 语句。
 *
 * 逐行读取文件，查找以 "#include" 开头（忽略前导空格）的行。
 *
 * @param file_path 要读取的文件路径。
 * @return 包含所有提取到的 #include 语句的字符串，每条语句后跟一个换行符。
 */
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

/**
 * @brief 剥离大模型可能返回的 Markdown 代码块标记 (如 ```cmake 和 ```)。
 *
 * 该函数会去除文本开头和结尾的空白字符，然后检查并移除文本开头和结尾的 Markdown 代码块围栏。
 * 它只会移除位于行首的围栏。
 *
 * @param text 包含潜在 Markdown 围栏的字符串。
 * @return 剥离了 Markdown 围栏和多余空白的字符串。
 */
std::string clean_markdown(std::string text) {
    // 1. 去除首尾空白
    size_t s = text.find_first_not_of(" \n\r\t");
    if (s == std::string::npos) return "";
    text.erase(0, s);
    size_t e = text.find_last_not_of(" \n\r\t");
    text.erase(e + 1);

    // 2. 只剥离文本开头的围栏（``` 或 ```cmake 等），不触碰中间内容
    if (text.size() >= 3 && text.compare(0, 3, "```") == 0) {
        size_t newline = text.find('\n');
        if (newline != std::string::npos)
            text.erase(0, newline + 1);
        else
            return "";  // 整个文本只有一行围栏，清空
    }

    // 3. 重新去尾部空白
    e = text.find_last_not_of(" \n\r\t");
    if (e == std::string::npos) return "";
    text.erase(e + 1);

    // 4. 只剥离文本末尾的围栏：必须在行首（前一字符是 \n 或文本起始）
    if (text.size() >= 3 && text.compare(text.size() - 3, 3, "```") == 0) {
        size_t end_fence = text.size() - 3;
        if (end_fence == 0 || text[end_fence - 1] == '\n') {
            text.erase(end_fence);
            e = text.find_last_not_of(" \n\r\t");
            if (e == std::string::npos) return "";
            text.erase(e + 1);
        }
    }

    return text;
}

/**
 * @brief 检查文件内容是否包含 cbot 管理标记。
 *
 * 管理标记为 "# === CBOT_MANAGED_BEGIN ===" 和 "# === CBOT_MANAGED_END ==="。
 *
 * @param content 要检查的文件内容字符串。
 * @return 如果内容同时包含开始和结束管理标记，则返回 true；否则返回 false。
 */
bool has_managed_block(const std::string& content) {
    return content.find("# === CBOT_MANAGED_BEGIN ===") != std::string::npos &&
           content.find("# === CBOT_MANAGED_END ===") != std::string::npos;
}

/**
 * @brief 将新内容替换进现有文件内容中由管理标记包围的区域。
 *
 * 标记外的用户自定义内容将保持不变。
 *
 * @param existing 现有文件的完整内容。
 * @param new_content 要插入到管理区域的新内容。
 * @return 替换了管理区域内容后的完整字符串。
 */
std::string splice_managed_content(const std::string& existing, const std::string& new_content) {
    const std::string begin_marker = "# === CBOT_MANAGED_BEGIN ===";
    const std::string end_marker   = "# === CBOT_MANAGED_END ===";

    size_t begin_pos = existing.find(begin_marker);
    size_t end_pos   = existing.find(end_marker);

    // 保留 BEGIN 那一整行（含换行），替换到 END 标记之前
    size_t begin_line_end = existing.find('\n', begin_pos);
    if (begin_line_end == std::string::npos)
        begin_line_end = begin_pos + begin_marker.size();

    std::string before = existing.substr(0, begin_line_end + 1);
    std::string after  = existing.substr(end_pos); // 从 END 行开始（含）

    return before + new_content + "\n" + after;
}

/**
 * @brief 将给定的内容包裹在 cbot 管理标记中，并附上用户自定义区提示。
 *
 * @param content 要包裹的字符串内容。
 * @return 包裹了管理标记和用户自定义区提示的完整字符串。
 */
std::string wrap_with_markers(const std::string& content) {
    return "# === CBOT_MANAGED_BEGIN ===\n" +
           content + "\n"
           "# === CBOT_MANAGED_END ===\n"
           "\n"
           "# ---- 以下为用户自定义区（cbot cmake 不会修改此区域）----\n";
}

} // 匿名命名空间结束

namespace cbot {
namespace commands {

/**
 * @brief 处理 CMake 命令，根据项目结构生成或更新 CMakeLists.txt 文件。
 *
 * 该函数会执行以下步骤：
 * 1. 规范化目标路径，并检查其是否为有效目录。
 * 2. 递归扫描目标目录下的 C/C++ 源码文件，收集项目结构和头文件依赖信息。
 *    会忽略 "build", ".git", ".vscode" 等目录。
 * 3. 将收集到的项目上下文发送给大模型，请求生成 CMakeLists.txt 配置。
 * 4. 清洗大模型返回的内容，并根据以下规则写入 CMakeLists.txt：
 *    - 如果文件不存在，则创建新文件并包裹管理标记。
 *    - 如果文件存在且包含管理标记，则只更新标记内的内容（增量更新）。
 *    - 如果文件存在但不包含管理标记，则提示用户是否覆写，若覆写则备份原文件并写入新内容（包裹管理标记）。
 *
 * @param target_path 用户指定的项目目标路径。
 */
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

    if (fs::exists(cmake_path)) {
        // 读取现有文件，判断是否有 cbot 管理标记
        std::ifstream ifs(cmake_path);
        std::string existing((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        if (has_managed_block(existing)) {
            // 增量更新：只替换标记内的内容，标记外的用户自定义内容保持不动
            std::string updated = splice_managed_content(existing, clean_content);
            std::ofstream out(cmake_path);
            if (out.is_open()) {
                out << updated;
                out.close();
                std::cout << "✅ 成功: 已增量更新 CMakeLists.txt（标记外的自定义内容已保留）" << std::endl;
            } else {
                std::cerr << "错误: 无法写入 CMakeLists.txt 文件。" << std::endl;
            }
            return;
        }

        // 无标记的旧文件，走原有覆写流程
        std::cout << "\n⚠️ 警告: 目标目录 [" << work_dir.string() << "] 下已存在 CMakeLists.txt（无 cbot 管理标记）。" << std::endl;
        std::cout << "是否要让 AI 覆写它？(原文件会自动备份，覆写后将添加管理标记以支持后续增量更新) [y/N]: ";

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

    // 写入（新文件或覆写旧文件），包裹管理标记
    std::string final_content = wrap_with_markers(clean_content);
    std::ofstream out_file(cmake_path);
    if (out_file.is_open()) {
        out_file << final_content;
        out_file.close();
        std::cout << "✅ 成功: CMakeLists.txt 已智能生成并写入目标目录！" << std::endl;
    } else {
        std::cerr << "错误: 无法写入 CMakeLists.txt 文件。" << std::endl;
    }
}

} // namespace commands
} // namespace cbot