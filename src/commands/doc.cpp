/**
 * @file doc.cpp
 * @brief 实现 cbot doc 命令：通过 libclang 解析 C++ AST，结合 LLM 生成 Doxygen
 * 注释并精确写回原文件。
 */
#include "commands/doc.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "utils/cpp_parser.hpp"
#include "utils/llm_client.hpp"

namespace fs = std::filesystem;

namespace {

// 构造发给 LLM 的 user prompt：声明列表 + 完整源码
std::string build_llm_prompt(const std::string& source_code,
                             const std::vector<cbot::utils::DeclInfo>& decls) {
    std::ostringstream oss;
    oss << "需要生成/更新注释的声明列表：\n";
    int idx = 1;
    for (const auto& d : decls) {
        oss << idx++ << ". [" << d.name << ":" << d.declaration_line << "] " << d.signature;
        if (d.has_comment)
            oss << "  （已有注释，请核对并按需更新）";
        else
            oss << "  （无注释，请新增）";
        oss << "\n";
    }
    oss << "\n--- 源文件完整内容 ---\n";
    oss << source_code;
    return oss.str();
}

// 解析 LLM 返回的标签化注释块
// 格式: [name:line]\n/** ... */
// 返回: map<declaration_line, comment_text>
std::map<unsigned, std::string> parse_llm_comments(const std::string& response) {
    std::map<unsigned, std::string> result;

    std::istringstream stream(response);
    std::string line;
    unsigned current_line = 0;
    std::ostringstream current_comment;
    bool in_comment = false;

    while (std::getline(stream, line)) {
        // 检测 [name:line] 标签行：以 '[' 开头，含 ':' 和 ']'
        if (!line.empty() && line.front() == '[') {
            // 先保存上一个注释块
            if (in_comment && current_line > 0) {
                std::string comment = current_comment.str();
                size_t e = comment.find_last_not_of(" \n\r\t");
                if (e != std::string::npos)
                    comment = comment.substr(0, e + 1);
                if (!comment.empty())
                    result[current_line] = comment;
            }

            // 解析行号：取最后一个 ':' 到 ']' 之间的数字
            size_t colon = line.rfind(':');
            size_t bracket = line.rfind(']');
            if (colon != std::string::npos && bracket != std::string::npos && colon < bracket) {
                std::string num_str = line.substr(colon + 1, bracket - colon - 1);
                try {
                    current_line = static_cast<unsigned>(std::stoul(num_str));
                    current_comment.str("");
                    current_comment.clear();
                    in_comment = true;
                } catch (...) {
                    current_line = 0;
                    in_comment = false;
                }
            }
            continue;
        }

        if (in_comment) {
            current_comment << line << "\n";
        }
    }

    // 保存最后一个注释块
    if (in_comment && current_line > 0) {
        std::string comment = current_comment.str();
        size_t e = comment.find_last_not_of(" \n\r\t");
        if (e != std::string::npos)
            comment = comment.substr(0, e + 1);
        if (!comment.empty())
            result[current_line] = comment;
    }

    return result;
}

// 将注释写回原文件内容，从后往前处理避免行号偏移
// 原文件代码行完全不经过 LLM，安全性 100%
std::string apply_comments(const std::string& original_source,
                           const std::vector<cbot::utils::DeclInfo>& decls,
                           const std::map<unsigned, std::string>& comments) {
    // 按行拆分（保留空行）
    std::vector<std::string> lines;
    std::istringstream stream(original_source);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    // 按 declaration_line 从大到小排序，从文件末尾往前处理
    std::vector<cbot::utils::DeclInfo> sorted = decls;
    std::sort(sorted.begin(), sorted.end(),
              [](const cbot::utils::DeclInfo& a, const cbot::utils::DeclInfo& b) {
                  return a.declaration_line > b.declaration_line;
              });

    for (const auto& decl : sorted) {
        auto it = comments.find(decl.declaration_line);
        if (it == comments.end())
            continue;  // LLM 未提供此声明的注释，保持原样

        // 将注释文本拆成行
        std::vector<std::string> comment_lines;
        std::istringstream cs(it->second);
        std::string cl;
        while (std::getline(cs, cl)) {
            comment_lines.push_back(cl);
        }

        if (decl.has_comment) {
            // 替换已有注释块（行号 1-based → 0-based）
            unsigned start = decl.comment_start_line - 1;
            unsigned end = decl.comment_end_line;  // exclusive
            if (start < lines.size() && end <= lines.size()) {
                lines.erase(lines.begin() + start, lines.begin() + end);
                lines.insert(lines.begin() + start, comment_lines.begin(), comment_lines.end());
            }
        } else {
            // 在声明行前插入新注释
            unsigned insert_pos = decl.declaration_line - 1;  // 0-based
            if (insert_pos <= lines.size()) {
                lines.insert(lines.begin() + insert_pos, comment_lines.begin(),
                             comment_lines.end());
            }
        }
    }

    // 重新拼接，保留原文件末尾换行
    std::ostringstream result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result << lines[i];
        if (i + 1 < lines.size())
            result << "\n";
    }
    if (!original_source.empty() && original_source.back() == '\n')
        result << "\n";

    return result.str();
}

}  // namespace

namespace {

// 从文件所在目录向上查找含 CMakeLists.txt 的项目根
// 找不到则回退到文件自身所在目录
fs::path find_project_root(const fs::path& file_path) {
    fs::path dir = file_path.parent_path();
    while (true) {
        if (fs::exists(dir / "CMakeLists.txt"))
            return dir;
        fs::path parent = dir.parent_path();
        if (parent == dir)
            return file_path.parent_path();  // 到达文件系统根，回退
        dir = parent;
    }
}

}  // namespace

namespace cbot {
namespace commands {

void handle_doc(const std::vector<std::string>& files) {
    cbot::utils::LLMClient client;

    std::string system_prompt = R"(
你是一个 C++ Doxygen 注释专家。我将提供源文件完整代码和需要注释的声明列表。

【输出格式——必须严格遵守】：
- 对每个声明，先输出 [函数名:行号] 标签（完全照抄列表中的格式，包括大小写和行号数字）
- 紧接着输出对应的 Doxygen 注释块（/** ... */），注释内容使用中文
- 两个声明之间空一行
- 只输出标签和注释块，不输出任何代码、解释文字，不使用 Markdown 格式

输出示例：
[foo:42]
/**
 * @brief 计算两数之和
 * @param a 第一个操作数
 * @param b 第二个操作数
 * @return 两数之和
 */

[Bar:67]
/**
 * @brief Bar 类的封装
 */
)";

    for (const auto& file_str : files) {
        fs::path file_path(file_str);

        std::cout << "\n============================================\n";
        std::cout << "正在处理: " << file_str << "\n";

        // 1. 文件校验
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            std::cerr << "⚠️ 警告: 文件不存在或不是有效文件，跳过。\n";
            continue;
        }

        // 2. libclang 解析：获取所有函数/类定义及行号
        std::string abs_path = fs::absolute(file_path).string();
        fs::path root = find_project_root(fs::absolute(file_path));
        std::vector<std::string> include_dirs;
        if (fs::exists(root / "include"))
            include_dirs.push_back((root / "include").string());
        if (fs::exists(root / "src"))
            include_dirs.push_back((root / "src").string());
        include_dirs.push_back(fs::absolute(file_path).parent_path().string());
        auto decls = cbot::utils::parse_declarations(abs_path, include_dirs);

        if (decls.empty()) {
            std::cout << "⚠️ 未发现任何函数/类定义，跳过此文件。\n";
            continue;
        }

        std::cout << "发现 " << decls.size() << " 个声明，正在请求大模型生成注释...\n";

        // 3. 读取原始代码
        std::ifstream ifs(file_path);
        std::string original_code((std::istreambuf_iterator<char>(ifs)),
                                  std::istreambuf_iterator<char>());
        ifs.close();

        // 4. 构造 prompt，调用 LLM（LLM 只看代码，只输出注释）
        std::string user_prompt = build_llm_prompt(original_code, decls);
        auto response = client.generate_response(system_prompt, user_prompt);
        if (!response) {
            std::cerr << "❌ 大模型无响应，跳过此文件。\n";
            continue;
        }

        // 5. 解析 LLM 返回的注释块
        auto comments = parse_llm_comments(response.value());
        if (comments.empty()) {
            std::cerr << "❌ 未能从响应中解析出注释块，跳过此文件。\n";
            std::cerr << "原始响应:\n" << response.value() << "\n";
            continue;
        }

        // 6. 将注释精确写回原文件内容（代码行 100% 来自原文件）
        std::string new_content = apply_comments(original_code, decls, comments);

        // 7. 预览
        std::cout << "处理完成，以下是更新后的文件内容：\n";
        std::cout << "---------------- 预览开始 ----------------\n";
        std::cout << new_content << "\n";
        std::cout << "---------------- 预览结束 ----------------\n";

        // 8. 用户确认后写入
        std::cout << "是否将上述内容写入 [" << file_str << "] ？[y/N]: ";
        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y") {
            std::ofstream ofs(file_path, std::ios::trunc);
            if (ofs.is_open()) {
                ofs << new_content;
                ofs.close();
                std::cout << "✅ 成功: 文件已写入。\n";
            } else {
                std::cerr << "❌ 错误: 无法写入文件。\n";
            }
        } else {
            std::cout << "🚫 操作已取消。\n";
        }
    }

    std::cout << "\n🎉 所有指定文件处理完毕！\n";
}

}  // namespace commands
}  // namespace cbot
