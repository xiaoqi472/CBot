#include "commands/build.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib> // 用于 std::system

namespace fs = std::filesystem;

namespace cbot {
namespace commands {

void handle_build() {
    fs::path current_path = fs::current_path();
    fs::path build_path = current_path / "build";

    // 1. 检查并创建 build 目录
    if (!fs::exists(build_path)) {
        std::cout << "--- 正在创建 build 目录 ---" << std::endl;
        if (!fs::create_directory(build_path)) {
            std::cerr << "错误: 无法创建 build 目录。" << std::endl;
            return;
        }
    } else {
        std::cout << "--- 发现已有 build 目录 ---" << std::endl;
    }

    // 2. 构造 shell 命令
    // 使用 cd 进入目录并串联命令，确保执行环境正确
    // 这里的命令逻辑是：cd build && cmake .. && make -j4
    std::string cmd = "cd " + build_path.string() + " && cmake .. && make -j4";

    std::cout << "执行命令: " << cmd << "\n" << std::endl;

    // 3. 调用系统 shell 执行
    int result = std::system(cmd.c_str());

    // 4. 检查执行结果
    if (result == 0) {
        std::cout << "\n✅ 编译成功！" << std::endl;
    } else {
        std::cerr << "\n❌ 编译失败，请检查上方错误输出。" << std::endl;
    }
}

} // namespace commands
} // namespace cbot