#include "commands/doc.hpp"
#include "utils/llm_client.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

// 复用：清洗大模型可能返回的 Markdown 标记
std::string clean_markdown(std::string text) {
    size_t start_pos = text.find("```");
    if (start_pos != std::string::npos) {
        size_t newline_pos = text.find('\n', start_pos);
        if (newline_pos != std::string::npos) text.erase(0, newline_pos + 1);
    }
    size_t end_pos = text.rfind("```");
    if (end_pos != std::string::npos) text.erase(end_pos);
    
    text.erase(0, text.find_first_not_of(" \n\r\t"));
    text.erase(text.find_last_not_of(" \n\r\t") + 1);
    return text;
}

} // 匿名命名空间

namespace cbot {
namespace commands {

void handle_doc(const std::vector<std::string>& files) {
    cbot::utils::LLMClient client;

    // 极其严苛的提示词，封死大模型修改代码逻辑的可能
    std::string system_prompt = R"(
你是一个极其严谨的代码注释辅助工具。你的唯一任务是为用户提供的 C++ 代码添加标准的 Doxygen 格式注释块。

【绝对红线要求】：
1. 绝对不允许修改任何原有代码的逻辑、变量名、空行或缩进结构！
2. 【位置要求】：函数或类的 Doxygen 注释必须紧贴在对应定义/声明的上方，严禁将函数注释挪动到文件开头或 #include 语句上方。
3. 如果需要对整个文件进行描述，请在文件最开头使用 @file 标记。
4. 必须输出包含原代码和新注释的【完整文件内容】。
5. 绝对不要输出任何 Markdown 格式（不要用 ```cpp 包裹），绝对不要输出任何解释性文字！
)";

    for (const auto& file_str : files) {
        fs::path file_path(file_str);

        // 1. 文件校验
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            std::cerr << "⚠️ 警告: 文件不存在或不是有效文件，跳过 -> " << file_str << std::endl;
            continue;
        }

        std::cout << "\n============================================\n";
        std::cout << "正在请求大模型处理: " << file_str << " ...\n";

        // 2. 读取原代码
        std::ifstream ifs(file_path);
        std::string original_code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        // 3. 调用 API
        auto response = client.generate_response(system_prompt, original_code);
        if (!response) {
            std::cerr << "❌ 处理失败: 大模型无响应，跳过此文件。" << std::endl;
            continue;
        }

        // 4. 清洗代码并展示给用户审核
        std::string processed_code = clean_markdown(response.value());
        
        std::cout << "处理完成，以下是添加注释后的代码内容：\n";
        std::cout << "---------------- 代码预览开始 ----------------\n";
        std::cout << processed_code << "\n";
        std::cout << "---------------- 代码预览结束 ----------------\n";

        // 5. 交互式确认覆写
        std::cout << "是否将上述代码覆写回原文件 [" << file_str << "] ？[y/N]: ";
        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y") {
            std::ofstream ofs(file_path, std::ios::trunc); // 截断模式覆写
            if (ofs.is_open()) {
                ofs << processed_code;
                ofs.close();
                std::cout << "✅ 成功: 文件已覆写。" << std::endl;
            } else {
                std::cerr << "❌ 错误: 无法写入文件。" << std::endl;
            }
        } else {
            std::cout << "🚫 操作已取消: 放弃修改此文件。" << std::endl;
        }
    }

    std::cout << "\n🎉 所有指定文件处理完毕！" << std::endl;
}

} // namespace commands
} // namespace cbot