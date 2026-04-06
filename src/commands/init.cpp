#include "commands/init.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace {  // 匿名命名空间，仅在本文件内可见

// 硬编码的 CMakeLists.txt 模板
const std::string CMAKE_TEMPLATE = R"(cmake_minimum_required(VERSION 3.10)
project(%PROJECT_NAME% VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
)";

// 硬编码的 main.cpp 模板
const std::string MAIN_TEMPLATE = R"(#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Hello, cbot!" << std::endl;
    return 0;
}
)";

// 辅助函数：将字符串写入文件
/**
 * @brief 辅助函数，将字符串内容写入指定文件。
 * @param file_path 要创建或写入的文件的路径。
 * @param content 要写入文件的字符串内容。
 */
void create_file(const fs::path& file_path, const std::string& content) {
    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
        std::cerr << "错误: 无法创建文件 " << file_path.string() << std::endl;
        return;
    }
    ofs << content;
    ofs.close();
}

}  // namespace

namespace cbot {
namespace commands {

/**
 * @brief 处理 'init' 命令，初始化一个新的 C++ 项目结构。
 *
 * 该函数会在当前目录下创建一个以 `project_name` 命名的根目录，
 * 并在其中创建 `src` 和 `include` 子目录。
 * 同时，它会生成一个 `CMakeLists.txt` 文件和一个 `src/main.cpp` 文件，
 * 其中 `CMakeLists.txt` 会根据 `project_name` 进行定制。
 *
 * 如果目标目录已存在，则会输出错误信息并退出。
 *
 * @param project_name 新项目的名称。这将作为根目录名和 CMake 项目名。
 */
void handle_init(const std::string& project_name) {
    fs::path root_dir = fs::current_path() / project_name;

    try {
        if (fs::exists(root_dir)) {
            std::cerr << "错误: 目录 '" << project_name << "' 已存在，请换一个名称或删除该目录。"
                      << std::endl;
            return;
        }

        fs::create_directory(root_dir);
        fs::create_directory(root_dir / "src");
        fs::create_directory(root_dir / "include");

        std::string cmake_content = CMAKE_TEMPLATE;
        size_t pos = cmake_content.find("%PROJECT_NAME%");
        if (pos != std::string::npos) {
            cmake_content.replace(pos, 14, project_name);
        }
        create_file(root_dir / "CMakeLists.txt", cmake_content);
        create_file(root_dir / "src" / "main.cpp", MAIN_TEMPLATE);

        std::cout << "成功: 项目 '" << project_name << "' 初始化完成。" << std::endl;
        std::cout << "  - 生成路径: " << root_dir.string() << std::endl;

    } catch (const fs::filesystem_error& e) {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "发生未知异常: " << e.what() << std::endl;
    }
}

}  // namespace commands
}  // namespace cbot