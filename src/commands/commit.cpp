#include "commands/commit.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "utils/llm_client.hpp"

namespace fs = std::filesystem;

namespace {

const size_t DIFF_SIZE_LIMIT = 8000;

// 执行 shell 命令并捕获输出，返回 {exit_code, stdout}
std::pair<int, std::string> run_command(const std::string& cmd) {
    std::array<char, 256> buf;
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {-1, ""};
    while (fgets(buf.data(), buf.size(), pipe))
        output += buf.data();
    int code = pclose(pipe);
    return {WEXITSTATUS(code), output};
}

// 检查当前目录是否在 git 仓库中
bool in_git_repo() {
    auto [code, out] = run_command("git rev-parse --is-inside-work-tree 2>/dev/null");
    return code == 0;
}

// 获取暂存区 diff
// 超过 DIFF_SIZE_LIMIT 则降级为 --stat
// 返回 {diff内容, 是否已降级}
std::pair<std::string, bool> get_staged_diff() {
    auto [code, diff] = run_command("git diff --staged 2>&1");
    if (code != 0) return {"", false};

    if (diff.size() > DIFF_SIZE_LIMIT) {
        auto [scode, stat] = run_command("git diff --staged --stat 2>&1");
        return {stat, true};
    }
    return {diff, false};
}

// 调起编辑器让用户修改 message，返回修改后的内容
std::string edit_in_editor(const std::string& initial) {
    // 创建临时文件
    fs::path tmp = fs::temp_directory_path() / "cbot_commit_msg.txt";
    {
        std::ofstream ofs(tmp);
        ofs << initial;
    }

    const char* editor = std::getenv("EDITOR");
    if (!editor) editor = std::getenv("VISUAL");
    std::string editor_cmd = editor ? std::string(editor) : "nano";

    std::string cmd = editor_cmd + " \"" + tmp.string() + "\"";
    int ret = std::system(cmd.c_str());

    std::string result = initial;
    if (ret == 0) {
        std::ifstream ifs(tmp);
        result = std::string((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
        // 去除首尾空白
        size_t s = result.find_first_not_of(" \n\r\t");
        size_t e = result.find_last_not_of(" \n\r\t");
        if (s != std::string::npos)
            result = result.substr(s, e - s + 1);
        else
            result = initial;
    }

    fs::remove(tmp);
    return result;
}

}  // namespace

namespace cbot {
namespace commands {

void handle_commit() {
    // 1. 检查是否在 git 仓库
    if (!in_git_repo()) {
        std::cerr << "❌ 当前目录不在 Git 仓库中。\n";
        return;
    }

    // 2. 获取 staged diff
    auto [diff, degraded] = get_staged_diff();

    if (diff.empty()) {
        std::cerr << "❌ 暂存区为空，请先使用 git add 暂存要提交的文件。\n";
        return;
    }

    if (degraded) {
        std::cout << "⚠️  diff 内容过大（超过 " << DIFF_SIZE_LIMIT
                  << " 字符），已降级为 --stat 模式，生成的提交信息可能不够精确。\n\n";
    }

    // 3. 构造 prompt，调用 LLM
    std::string system_prompt = R"(
你是一个专业的 Git 提交信息撰写助手。请根据用户提供的 git diff 或 git diff --stat 内容，推断此次修改的目的，生成一条符合 Angular 规范的中文 commit message。

【Angular 规范格式】：
<type>(<scope>): <subject>

<body>（可选，多行，解释"为什么"而非"做了什么"）

【type 取值】：
- feat: 新功能
- fix: bug 修复
- refactor: 重构（非新功能非修复）
- docs: 文档变更
- style: 代码格式（不影响逻辑）
- test: 测试相关
- chore: 构建/工具/依赖等杂项

【输出要求】：
- subject 使用中文，简洁描述本次变更（不超过 50 字）
- scope 为受影响的模块名（可省略）
- body 可选，如有必要用中文补充说明
- 只输出 commit message 本身，不输出任何解释或 Markdown 格式
)";

    std::cout << "正在分析变更并生成提交信息...\n";
    cbot::utils::LLMClient client;
    auto response = client.generate_response(system_prompt, diff);

    if (!response) {
        std::cerr << "❌ 大模型无响应，无法生成提交信息。\n";
        return;
    }

    std::string commit_msg = response.value();
    // 去除首尾空白
    size_t s = commit_msg.find_first_not_of(" \n\r\t");
    size_t e = commit_msg.find_last_not_of(" \n\r\t");
    if (s != std::string::npos)
        commit_msg = commit_msg.substr(s, e - s + 1);

    // 4. 打印预览
    std::cout << "\n────────────── 提交信息预览 ──────────────\n";
    std::cout << commit_msg << "\n";
    std::cout << "──────────────────────────────────────────\n\n";

    // 5. 交互确认
    while (true) {
        std::cout << "是否使用此提交信息？[y/e/N]: ";
        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y") {
            // 构造 git commit 命令，使用单引号包裹（message 中可能有特殊字符）
            // 先写入临时文件再用 -F 传入，避免 shell 转义问题
            fs::path tmp = fs::temp_directory_path() / "cbot_commit_msg_final.txt";
            {
                std::ofstream ofs(tmp);
                ofs << commit_msg;
            }
            std::string cmd = "git commit -F \"" + tmp.string() + "\"";
            int ret = std::system(cmd.c_str());
            fs::remove(tmp);

            if (ret == 0)
                std::cout << "✅ 提交成功！\n";
            else
                std::cerr << "❌ git commit 执行失败，请检查上方输出。\n";
            return;

        } else if (answer == "e" || answer == "E") {
            commit_msg = edit_in_editor(commit_msg);
            if (commit_msg.empty()) {
                std::cerr << "❌ 提交信息为空，操作已取消。\n";
                return;
            }
            // 重新打印预览
            std::cout << "\n────────────── 提交信息预览 ──────────────\n";
            std::cout << commit_msg << "\n";
            std::cout << "──────────────────────────────────────────\n\n";

        } else {
            std::cout << "操作已取消。\n";
            return;
        }
    }
}

}  // namespace commands
}  // namespace cbot
