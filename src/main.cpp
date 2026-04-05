#include <iostream>
#include <vector>
#include <string>
#include "commands/init.hpp"
#include "utils/llm_client.hpp" 
#include "commands/cmake.hpp" 
#include "commands/doc.hpp" 
#include "commands/build.hpp" 


/**
 * @brief cbot 命令行工具的入口点。
 *
 * 该函数解析命令行参数，并根据子命令（如 init, test_llm, cmake, doc, build）
 * 路由到相应的处理函数。
 *
 * @param argc 命令行参数的数量。
 * @param argv 包含命令行参数的 C 风格字符串数组。
 * @return 0 表示成功执行，1 表示发生错误或用法不正确。
 */
int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);

    if (args.size() < 2) {
        std::cerr << "用法: cbot <command> [args...]" << std::endl;
        std::cerr << "可用命令:\n";
        std::cerr << "  init <project_name>   初始化标准 C++ 项目结构\n";
        std::cerr << "  cmake                 智能生成 CMakeLists.txt\n";
        std::cerr << "  doc <file1> [file2] ...   递归生成 Doxygen 注释\n";
        std::cerr << "  test_llm              测试 Gemini API 连通性\n"; // [新增] 帮助信息
        std::cerr << "  build                  构建项目\n";
        return 1;
    }

    std::string command = args[1];

    if (command == "init") {
        if (args.size() != 3) {
            std::cerr << "用法: cbot init <project_name>" << std::endl;
            return 1;
        }
        cbot::commands::handle_init(args[2]);
    } 
    // [新增] test_llm 路由分支
    else if (command == "test_llm") {
        std::cout << "正在连接 Gemini API..." << std::endl;
        
        // 实例化客户端，默认使用 gemini-2.5-flash
        cbot::utils::LLMClient client; 
        
        std::string sys_prompt = "你是一个C++专家。只用一句话回答问题。";
        std::string usr_prompt = "解释一下什么是 RAII？";
        
        auto response = client.generate_response(sys_prompt, usr_prompt);
        if (response) {
            std::cout << "\n[大模型返回成功]: \n" << response.value() << std::endl;
        } else {
            std::cerr << "\n[大模型调用失败]" << std::endl;
        }
    }
    // 【修改点】增加对目标路径参数的解析
    else if (command == "cmake") {
        std::string target_path = "."; // 默认当前目录
        if (args.size() >= 3) {
            target_path = args[2];     // 用户指定了路径
        }
        cbot::commands::handle_cmake(target_path);
    }
    else if (command == "doc") {
        if (args.size() < 3) {
            std::cerr << "用法: cbot doc <file1> [file2] ..." << std::endl;
            return 1;
        }
        // 将命令行传入的多个文件路径打包为 vector
        std::vector<std::string> target_files(args.begin() + 2, args.end());
        cbot::commands::handle_doc(target_files);
    }
    else if (command == "build") {
        cbot::commands::handle_build();
    }
    else {
        std::cerr << "未知命令: " << command << std::endl;
        return 1;
    }

    return 0;
}