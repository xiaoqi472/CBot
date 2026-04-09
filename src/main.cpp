#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "commands/build.hpp"
#include "commands/cmake.hpp"
#include "commands/commit.hpp"
#include "commands/doc.hpp"
#include "commands/format.hpp"
#include "commands/init.hpp"
#include "commands/selfmgmt.hpp"
#include "utils/llm_client.hpp"

using Handler = std::function<int(const std::vector<std::string>&)>;

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);

    if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
        std::cout << "用法: cbot <command> [args...]\n";
        std::cout << "可用命令:\n";
        std::cout << "  init <project_name>            初始化标准 C++ 项目结构\n";
        std::cout << "  cmake [路径]                   智能生成 CMakeLists.txt\n";
        std::cout << "  build                          一键编译项目\n";
        std::cout << "  doc <file1> [file2] ...        AI 添加 Doxygen 注释\n";
        std::cout << "  format [--init] [路径/文件...] 代码格式化\n";
        std::cout << "  commit                         AI 生成 Git 提交信息\n";
        std::cout << "  update                         更新 cbot 至最新版本\n";
        std::cout << "  uninstall                      卸载 cbot\n";
        std::cout << "  test_llm                       测试 Gemini API 连通性\n";
        return 0;
    }

    std::map<std::string, Handler> commands = {
        {"init", [](const auto& a) {
            if (a.size() != 3) {
                std::cerr << "用法: cbot init <project_name>\n";
                return 1;
            }
            cbot::commands::handle_init(a[2]);
            return 0;
        }},
        {"test_llm", [](const auto&) {
            std::cout << "正在连接 Gemini API...\n";
            cbot::utils::LLMClient client;
            auto resp = client.generate_response("你是一个C++专家。只用一句话回答问题。", "解释一下什么是 RAII？");
            if (resp) {
                std::cout << "\n[大模型返回成功]: \n" << resp.value() << '\n';
            } else {
                std::cerr << "\n[大模型调用失败]\n";
            }
            return 0;
        }},
        {"cmake", [](const auto& a) {
            cbot::commands::handle_cmake(a.size() >= 3 ? a[2] : ".");
            return 0;
        }},
        {"doc", [](const auto& a) {
            if (a.size() < 3) {
                std::cerr << "用法: cbot doc <file1> [file2] ...\n";
                return 1;
            }
            cbot::commands::handle_doc({a.begin() + 2, a.end()});
            return 0;
        }},
        {"build", [](const auto&) {
            cbot::commands::handle_build();
            return 0;
        }},
        {"format", [](const auto& a) {
            bool init_mode = false;
            std::vector<std::string> targets;
            for (size_t i = 2; i < a.size(); ++i) {
                if (a[i] == "--init") {
                    init_mode = true;
                } else {
                    targets.push_back(a[i]);
                }
            }
            if (!init_mode && targets.empty()) targets.push_back(".");
            cbot::commands::handle_format(targets, init_mode);
            return 0;
        }},
        {"commit", [](const auto&) {
            cbot::commands::handle_commit();
            return 0;
        }},
        {"update", [](const auto&) {
            cbot::commands::handle_update();
            return 0;
        }},
        {"uninstall", [](const auto&) {
            cbot::commands::handle_uninstall();
            return 0;
        }},
    };

    const std::string& command = args[1];
    auto it = commands.find(command);
    if (it == commands.end()) {
        std::cerr << "未知命令: " << command << '\n';
        return 1;
    }
    return it->second(args);
}