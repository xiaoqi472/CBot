#include "commands/selfmgmt.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "utils/process.hpp"

namespace fs = std::filesystem;

namespace {

const std::string INSTALL_DIR =
    std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.local/cbot";
const std::string BIN_PATH = "/usr/local/bin/cbot";
const std::string COMPLETION_PATH = "/etc/bash_completion.d/cbot";

// 检查是否为通过 install.sh 安装的环境
bool is_managed_install() {
    return fs::exists(INSTALL_DIR + "/.git") && fs::exists(INSTALL_DIR + "/build/cbot");
}

}  // namespace

namespace cbot {
namespace commands {

void handle_update() {
    if (!is_managed_install()) {
        std::cerr << "❌ 未检测到通过 install.sh 安装的 cbot（" << INSTALL_DIR << " 不存在）。\n";
        std::cerr << "   手动安装的用户请自行进入源码目录执行 git pull && make。\n";
        return;
    }

    std::cout << "正在更新 cbot...\n\n";

    // 1. git pull
    std::cout << "── 拉取最新代码 ──\n";
    int ret = cbot::utils::run_interactive({"git", "-C", INSTALL_DIR, "pull", "--ff-only"});
    if (ret != 0) {
        std::cerr << "❌ git pull 失败，请检查网络或手动处理冲突。\n";
        return;
    }

    // 2. 重新编译
    std::cout << "\n── 重新编译 ──\n";
    ret = cbot::utils::run_interactive({"cmake", "-S", INSTALL_DIR, "-B", INSTALL_DIR + "/build",
                                        "-DCMAKE_BUILD_TYPE=Release", "-q"});
    if (ret != 0) {
        std::cerr << "❌ cmake 配置失败，请检查上方输出。\n";
        return;
    }

    unsigned int jobs = std::thread::hardware_concurrency();
    if (jobs == 0)
        jobs = 1;

    ret = cbot::utils::run_interactive(
        {"cmake", "--build", INSTALL_DIR + "/build", "--", "-j" + std::to_string(jobs)});
    if (ret != 0) {
        std::cerr << "❌ 编译失败，请检查上方输出。\n";
        return;
    }

    // 3. 更新 bash 补全（如果有变更）
    std::string completion_src = INSTALL_DIR + "/cbot-completion.bash";
    if (fs::exists(completion_src) && fs::exists(COMPLETION_PATH)) {
        cbot::utils::run_interactive({"sudo", "cp", completion_src, COMPLETION_PATH});
    }

    std::cout << "\n✅ cbot 已更新至最新版本。\n";
}

void handle_uninstall() {
    if (!is_managed_install()) {
        std::cerr << "❌ 未检测到通过 install.sh 安装的 cbot（" << INSTALL_DIR << " 不存在）。\n";
        std::cerr << "   手动安装的用户请自行删除软链接和相关文件。\n";
        return;
    }

    std::cout << "即将卸载 cbot，以下内容将被删除：\n";
    std::cout << "  - 软链接: " << BIN_PATH << "\n";
    std::cout << "  - bash 补全: " << COMPLETION_PATH << "\n";
    std::cout << "\n是否继续？[y/N]: ";

    std::string answer;
    std::getline(std::cin, answer);
    if (answer != "y" && answer != "Y") {
        std::cout << "操作已取消。\n";
        return;
    }

    // 删除软链接
    if (fs::exists(BIN_PATH) || fs::is_symlink(BIN_PATH)) {
        cbot::utils::run_interactive({"sudo", "rm", "-f", BIN_PATH});
        std::cout << "  ✔ 已删除软链接\n";
    }

    // 删除 bash 补全
    if (fs::exists(COMPLETION_PATH)) {
        cbot::utils::run_interactive({"sudo", "rm", "-f", COMPLETION_PATH});
        std::cout << "  ✔ 已删除 bash 补全\n";
    }

    // 询问是否删除源码和编译产物
    std::cout << "\n是否同时删除源码和编译产物（" << INSTALL_DIR << "）？[y/N]: ";
    std::getline(std::cin, answer);
    if (answer == "y" || answer == "Y") {
        cbot::utils::run_interactive({"rm", "-rf", INSTALL_DIR});
        std::cout << "  ✔ 已删除 " << INSTALL_DIR << "\n";
    } else {
        std::cout << "  源码目录已保留：" << INSTALL_DIR << "\n";
    }

    std::cout << "\n✅ cbot 已卸载完成。\n";
}

}  // namespace commands
}  // namespace cbot
