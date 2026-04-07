#include "commands/build.hpp"
#include <iostream>
#include <filesystem>
#include <thread>
#include <cstdlib>

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

    // 2. 动态获取 CPU 核心数，至少为 1
    unsigned int jobs = std::thread::hardware_concurrency();
    if (jobs == 0) jobs = 1;

    // 3. 构造 shell 命令
    std::string cmd = "cd " + build_path.string() + " && cmake .. && make -j" + std::to_string(jobs);

    std::cout << "执行命令: " << cmd << "\n" << std::endl;

    // 4. 调用系统 shell 执行
    int result = std::system(cmd.c_str());

    // 5. 检查执行结果
    if (result == 0) {
        std::cout << "\n✅ 编译成功！" << std::endl;
    } else {
        std::cerr << "\n❌ 编译失败，请检查上方错误输出。" << std::endl;
    }
}

} // namespace commands
} // namespace cbot