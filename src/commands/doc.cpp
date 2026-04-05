/**
 * @file doc.cpp
 * @brief Implements the 'doc' command for cbot, which uses an LLM to add Doxygen comments to C++ files.
 */
#include "commands/doc.hpp"
#include "utils/llm_client.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

/**
 * @brief Cleans potential Markdown fences (```cpp, ```) from the beginning and end of a string.
 *
 * This function is designed to process text responses from large language models (LLMs)
 * that might wrap their output in Markdown code blocks. It carefully removes
 * leading and trailing Markdown fences without affecting content in the middle.
 *
 * @param text The input string, potentially containing Markdown fences.
 * @return The cleaned string with leading/trailing Markdown fences removed.
 */
std::string clean_markdown(std::string text) {
    // 1. 去除首尾空白
    size_t s = text.find_first_not_of(" \n\r\t");
    if (s == std::string::npos) return "";
    text.erase(0, s);
    size_t e = text.find_last_not_of(" \n\r\t");
    text.erase(e + 1);

    // 2. 只剥离文本开头的围栏（``` 或 ```cpp 等），不触碰中间内容
    if (text.size() >= 3 && text.compare(0, 3, "```") == 0) {
        size_t newline = text.find('\n');
        if (newline != std::string::npos)
            text.erase(0, newline + 1);
        else
            return "";  // 整个文本只有一行围栏，清空
    }

    // 3. 重新去尾部空白
    e = text.find_last_not_of(" \n\r\t");
    if (e == std::string::npos) return "";
    text.erase(e + 1);

    // 4. 只剥离文本末尾的围栏：必须在行首（前一字符是 \n 或文本起始）
    if (text.size() >= 3 && text.compare(text.size() - 3, 3, "```") == 0) {
        size_t end_fence = text.size() - 3;
        if (end_fence == 0 || text[end_fence - 1] == '\n') {
            text.erase(end_fence);
            e = text.find_last_not_of(" \n\r\t");
            if (e == std::string::npos) return "";
            text.erase(e + 1);
        }
    }

    return text;
}

} // 匿名命名空间

namespace cbot {
namespace commands {

/**
 * @brief Handles the 'doc' command, processing a list of C++ files to add Doxygen comments using an LLM.
 *
 * This function iterates through the provided file paths, reads their content,
 * sends the code to a large language model (LLM) for Doxygen comment generation,
 * cleans the LLM's response, and then prompts the user for confirmation
 * before overwriting the original file with the commented version.
 *
 * @param files A constant reference to a vector of strings, where each string is a path to a C++ file to be processed.
 */
void handle_doc(const std::vector<std::string>& files) {
    cbot::utils::LLMClient client;

    // 极其严苛的提示词，封死大模型修改代码逻辑的可能
    std::string system_prompt = R"(
你是一个极其严谨的代码注释辅助工具。你的任务是为用户提供的 C++ 代码添加或更新标准的 Doxygen 格式注释块。

【关于注释的处理规则】：
- 如果某个函数/类/文件没有 Doxygen 注释，则为其添加。
- 如果已有 Doxygen 注释，必须将其与当前代码进行核对：参数名、返回值、函数行为描述若与代码不符，必须更新注释使其与代码一致。
- 如果已有注释与代码完全一致，保持不变。

【绝对红线——以下内容严禁触碰】：
1. 严禁修改任何代码行：包括但不限于函数签名、函数体逻辑、变量名、空行、缩进、#include 语句。
2. 你只能增删改 Doxygen 注释块（/** ... */），其余所有内容必须与输入完全一致，一个字符都不能差。
3. 【位置要求】：函数或类的 Doxygen 注释必须紧贴在对应定义/声明的上方，严禁将注释挪动到 #include 语句上方。
4. 如果需要对整个文件进行描述，请在文件最开头使用 @file 标记。
5. 必须输出包含原代码和注释的【完整文件内容】。
6. 绝对不要输出任何 Markdown 格式（不要用 ```cpp 包裹），绝对不要输出任何解释性文字！
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