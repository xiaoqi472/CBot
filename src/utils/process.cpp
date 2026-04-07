#include "utils/process.hpp"

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace cbot::utils {

namespace {

/**
 * @brief 将字符串向量转换为 C 风格的参数数组
 *
 * 此函数将一个 `std::vector<std::string>` 转换为一个 `std::vector<char*>`，
 * 其中包含指向每个字符串 C 风格表示的指针，并在末尾添加一个 `nullptr`，
 * 以便与 `execvp` 等函数兼容。
 *
 * @param args 包含命令行参数的字符串向量。
 * @return 一个 `char*` 指针向量，表示 C 风格的参数数组，以 `nullptr` 结尾。
 */
std::vector<char*> make_argv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    return argv;
}

}  // namespace

/**
 * @brief 以交互模式运行外部命令
 *
 * 此函数创建一个子进程来执行指定的命令。子进程的标准输入、输出和错误流
 * 将继承自父进程。父进程会等待子进程完成。
 *
 * @param args 包含命令及其参数的字符串向量。第一个元素应为命令本身。
 * @param cwd 子进程的可选工作目录。如果未指定，则使用当前工作目录。
 * @return 子进程的退出状态码。如果发生错误（例如 `fork` 失败），则返回 -1。
 */
int run_interactive(const std::vector<std::string>& args,
                    const std::optional<std::filesystem::path>& cwd) {
    if (args.empty())
        return -1;

    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        if (cwd.has_value()) {
            if (chdir(cwd->c_str()) != 0) {
                std::perror("chdir");
                _exit(127);
            }
        }
        auto argv = make_argv(args);
        execvp(argv[0], argv.data());
        std::perror(argv[0]);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/**
 * @brief 运行外部命令并捕获其标准输出和标准错误
 *
 * 此函数创建一个子进程来执行指定的命令，并通过管道捕获其标准输出和标准错误。
 * 父进程会等待子进程完成，并返回其退出状态码以及捕获到的输出。
 *
 * @param args 包含命令及其参数的字符串向量。第一个元素应为命令本身。
 * @param cwd 子进程的可选工作目录。如果未指定，则使用当前工作目录。
 * @return 一个 `ProcessResult` 结构体，包含子进程的退出状态码、捕获到的标准输出和标准错误。
 *         如果发生错误（例如 `fork` 或 `pipe` 失败），则退出状态码为 -1。
 */
ProcessResult run_capture(const std::vector<std::string>& args,
                          const std::optional<std::filesystem::path>& cwd) {
    if (args.empty())
        return {-1, "", ""};

    int stdout_pipe[2], stderr_pipe[2];

    // 问题二修复：stdout_pipe 成功而 stderr_pipe 失败时，关闭已打开的 fd
    if (pipe(stdout_pipe) != 0)
        return {-1, "", ""};
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return {-1, "", ""};
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return {-1, "", ""};
    }

    if (pid == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (cwd.has_value()) {
            if (chdir(cwd->c_str()) != 0) {
                std::perror("chdir");
                _exit(127);
            }
        }
        auto argv = make_argv(args);
        execvp(argv[0], argv.data());
        std::perror(argv[0]);
        _exit(127);
    }

    // 父进程：关闭写端
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // 问题一修复：用 poll 同时监听两个管道，避免顺序读导致的死锁
    std::string out, err;
    struct pollfd fds[2] = {
        {stdout_pipe[0], POLLIN, 0},
        {stderr_pipe[0], POLLIN, 0},
    };
    std::string* targets[2] = {&out, &err};
    char buf[4096];
    int open_count = 2;

    while (open_count > 0) {
        if (poll(fds, 2, -1) < 0)
            break;
        for (int i = 0; i < 2; i++) {
            if (fds[i].fd == -1)
                continue;
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                if (n > 0) {
                    targets[i]->append(buf, n);
                } else {
                    // n == 0：EOF；n < 0：读错误，均关闭该 fd
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --open_count;
                }
            }
        }
    }

    int status = 0;
    waitpid(pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {code, std::move(out), std::move(err)};
}

}  // namespace cbot::utils
