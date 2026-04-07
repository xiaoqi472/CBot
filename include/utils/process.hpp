#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cbot::utils {

struct ProcessResult {
    int exit_code;
    std::string stdout_out;
    std::string stderr_out;
};

// 交互式执行：stdin/stdout/stderr 直接透传到终端（适合 editor、git commit、make 等）
// 返回子进程退出码；失败启动返回 -1
int run_interactive(const std::vector<std::string>& args,
                    const std::optional<std::filesystem::path>& cwd = std::nullopt);

// 捕获式执行：收集 stdout + stderr，不透传到终端（适合版本检查、diff 读取等）
ProcessResult run_capture(const std::vector<std::string>& args,
                          const std::optional<std::filesystem::path>& cwd = std::nullopt);

}  // namespace cbot::utils
