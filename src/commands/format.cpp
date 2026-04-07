#include "commands/format.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "utils/process.hpp"

namespace fs = std::filesystem;

namespace {

const std::string CLANG_FORMAT_TEMPLATE =
    "BasedOnStyle: Google\n"
    "IndentWidth: 4\n"
    "PointerAlignment: Left\n"
    "ColumnLimit: 100\n"
    "AllowShortFunctionsOnASingleLine: None\n"
    "AllowShortIfStatementsOnASingleLine: Never\n"
    "AllowShortLoopsOnASingleLine: false\n"
    "DerivePointerAlignment: false\n";

bool should_ignore(const fs::path& path) {
    std::string s = path.string();
    return s.find("/build") != std::string::npos || s.find("/.git") != std::string::npos ||
           s.find("/.vscode") != std::string::npos;
}

bool is_source_file(const fs::path& path) {
    std::string ext = path.extension().string();
    return ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".h";
}

bool check_clang_format_installed() {
    auto res = cbot::utils::run_capture({"clang-format", "--version"});
    if (res.exit_code != 0) {
        std::cerr << "错误: 未检测到 clang-format，请先安装:\n"
                  << "  sudo apt install clang-format\n";
        return false;
    }
    return true;
}

void collect_files(const fs::path& path, std::vector<fs::path>& out) {
    if (fs::is_regular_file(path)) {
        if (is_source_file(path))
            out.push_back(path);
        else
            std::cerr << "⚠️  " << path.string() << " 不是 C/C++ 源文件，跳过。\n";
    } else if (fs::is_directory(path)) {
        try {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file() && !should_ignore(entry.path()) &&
                    is_source_file(entry.path())) {
                    out.push_back(entry.path());
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "文件扫描错误: " << e.what() << "\n";
        }
    } else {
        std::cerr << "⚠️  " << path.string() << " 不存在，跳过。\n";
    }
}

std::string read_file(const fs::path& path) {
    std::ifstream ifs(path);
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

void write_file(const fs::path& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::trunc);
    ofs << content;
}

}  // namespace

namespace cbot {
namespace commands {

void handle_format(const std::vector<std::string>& targets, bool init_mode) {
    if (init_mode) {
        if (!check_clang_format_installed())
            return;

        fs::path config_path = fs::current_path() / ".clang-format";
        if (fs::exists(config_path)) {
            std::cout << "⚠️  .clang-format 已存在，是否覆写？[y/N]: ";
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "y" && answer != "Y") {
                std::cout << "操作已取消。\n";
                return;
            }
        }
        std::ofstream ofs(config_path);
        if (ofs.is_open()) {
            ofs << CLANG_FORMAT_TEMPLATE;
            std::cout << "✅ .clang-format 已写入当前目录。\n";
        } else {
            std::cerr << "❌ 无法写入 .clang-format。\n";
        }
        return;
    }

    if (!check_clang_format_installed())
        return;

    std::vector<fs::path> files;
    for (const auto& t : targets)
        collect_files(fs::path(t), files);

    if (files.empty()) {
        std::cout << "未发现任何 C/C++ 源文件。\n";
        return;
    }

    std::cout << "共发现 " << files.size() << " 个文件，开始格式化...\n";

    // key: 已读取备份的文件（包含已成功格式化和当前失败的）
    std::map<fs::path, std::string> backups;
    bool aborted = false;

    for (const auto& file : files) {
        backups[file] = read_file(file);

        auto res = cbot::utils::run_capture({"clang-format", "-i", file.string()});

        if (res.exit_code != 0) {
            std::cout << "❌ 格式化失败: " << file.string() << "\n";
            if (!res.stderr_out.empty())
                std::cerr << res.stderr_out;
            std::cout << "[A]bort（恢复已格式化的文件并退出）/ [S]kip（跳过此文件）: ";
            std::string answer;
            std::getline(std::cin, answer);

            if (answer == "s" || answer == "S") {
                // 恢复本文件（clang-format -i 失败时可能已部分修改）
                write_file(file, backups[file]);
                backups.erase(file);
                std::cout << "已跳过: " << file.string() << "\n";
            } else {
                aborted = true;
                break;
            }
        } else {
            std::cout << "  ✔ " << file.string() << "\n";
        }
    }

    if (aborted) {
        std::cout << "正在恢复已格式化的文件...\n";
        for (const auto& [path, content] : backups) {
            write_file(path, content);
            std::cout << "  已恢复: " << path.string() << "\n";
        }
        std::cout << "❌ 格式化已中止，所有文件已恢复原状。\n";
        return;
    }

    std::cout << "✅ 格式化完成！\n";
}

}  // namespace commands
}  // namespace cbot
