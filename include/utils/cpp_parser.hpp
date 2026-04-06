#pragma once
#include <string>
#include <vector>

namespace cbot {
namespace utils {

struct DeclInfo {
    std::string name;             // 函数/类名
    std::string kind;             // "function" / "method" / "class" / "constructor" / "destructor"
    std::string signature;        // 完整签名（含参数类型）
    unsigned declaration_line;    // 声明行号（1-based）
    bool has_comment;             // 是否已有 Doxygen 注释
    unsigned comment_start_line;  // 注释起始行（has_comment=true 时有效）
    unsigned comment_end_line;    // 注释结束行（has_comment=true 时有效）
};

// 使用 libclang 解析指定 C++ 文件，返回所有函数/类定义信息
// include_dirs: 额外头文件搜索路径（辅助解析，不影响声明提取准确性）
std::vector<DeclInfo> parse_declarations(const std::string& file_path,
                                         const std::vector<std::string>& include_dirs = {});

}  // namespace utils
}  // namespace cbot
