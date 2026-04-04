#include "commands/init.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace { // 匿名命名空间，仅在本文件内可见

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
void create_file(const fs::path& file_path, const std::string& content) {
    std::ofstream ofs(file_path);
    if (!ofs.is_open()) {
        std::cerr << "错误: 无法创建文件 " << file_path.string() << std::endl;
        return;
    }
    ofs << content;
    ofs.close();
}

} // 结束匿名命名空间

namespace cbot {
namespace commands {

void handle_init(const std::string& project_name) {
    fs::path root_dir = fs::current_path() / project_name;

    try {
        if (fs::exists(root_dir)) {
            std::cerr << "错误: 目录 '" << project_name << "' 已存在，请换一个名称或删除该目录。" << std::endl;
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

} // namespace commands
} // namespace cbot