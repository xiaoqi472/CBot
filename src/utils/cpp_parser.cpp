#include "utils/cpp_parser.hpp"

#include <clang-c/Index.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace cbot {
namespace utils {

namespace {

struct VisitorData {
    std::vector<DeclInfo>* decls;
    CXTranslationUnit tu;
};

std::string cx_to_str(CXString cx) {
    const char* cstr = clang_getCString(cx);
    std::string result = cstr ? cstr : "";
    clang_disposeString(cx);
    return result;
}

std::string kind_to_str(CXCursorKind kind) {
    switch (kind) {
        case CXCursor_FunctionDecl:
            return "function";
        case CXCursor_CXXMethod:
            return "method";
        case CXCursor_ClassDecl:
            return "class";
        case CXCursor_Constructor:
            return "constructor";
        case CXCursor_Destructor:
            return "destructor";
        default:
            return "unknown";
    }
}

CXChildVisitResult visitor(CXCursor cursor, CXCursor /*parent*/, CXClientData client_data) {
    CXCursorKind kind = clang_getCursorKind(cursor);

    // 只处理目标节点类型
    bool is_target =
        (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod ||
         kind == CXCursor_ClassDecl || kind == CXCursor_Constructor || kind == CXCursor_Destructor);
    if (!is_target)
        return CXChildVisit_Recurse;

    // 只处理主文件中的声明（排除 #include 引入的）
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    if (!clang_Location_isFromMainFile(loc))
        return CXChildVisit_Continue;

    // 函数/方法/构造/析构：只取有函数体的定义，跳过纯声明
    if (kind != CXCursor_ClassDecl) {
        if (!clang_isCursorDefinition(cursor))
            return CXChildVisit_Continue;
    }

    auto* data = reinterpret_cast<VisitorData*>(client_data);

    DeclInfo info;
    info.kind = kind_to_str(kind);
    info.name = cx_to_str(clang_getCursorSpelling(cursor));
    info.signature = cx_to_str(clang_getCursorDisplayName(cursor));
    info.has_comment = false;
    info.comment_start_line = 0;
    info.comment_end_line = 0;

    // 获取声明行号
    unsigned line, col, offset;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &col, &offset);
    info.declaration_line = line;

    // 检查是否有 Doxygen 注释
    CXString raw_comment = clang_Cursor_getRawCommentText(cursor);
    const char* raw_cstr = clang_getCString(raw_comment);
    if (raw_cstr && raw_cstr[0] != '\0') {
        info.has_comment = true;

        CXSourceRange comment_range = clang_Cursor_getCommentRange(cursor);
        CXSourceLocation start = clang_getRangeStart(comment_range);
        CXSourceLocation end = clang_getRangeEnd(comment_range);

        unsigned start_line, end_line, c, o;
        CXFile f;
        clang_getSpellingLocation(start, &f, &start_line, &c, &o);
        clang_getSpellingLocation(end, &f, &end_line, &c, &o);

        info.comment_start_line = start_line;
        info.comment_end_line = end_line;
    }
    clang_disposeString(raw_comment);

    data->decls->push_back(info);

    // 类内部的方法需要递归进去
    return CXChildVisit_Recurse;
}

}  // namespace

std::vector<DeclInfo> parse_declarations(const std::string& file_path,
                                         const std::vector<std::string>& include_dirs) {
    std::vector<DeclInfo> decls;

    CXIndex index = clang_createIndex(0, 0);

    // 构造编译参数
    std::vector<const char*> args;
    args.push_back("-std=c++17");
    args.push_back("-x");
    args.push_back("c++");

    std::vector<std::string> include_flags;
    for (const auto& dir : include_dirs) {
        include_flags.push_back("-I" + dir);
        args.push_back(include_flags.back().c_str());
    }

    // 解析文件，忽略非致命错误（保证 AST 仍可提取声明）
    CXTranslationUnit tu = clang_parseTranslationUnit(
        index, file_path.c_str(), args.data(), static_cast<int>(args.size()), nullptr, 0,
        CXTranslationUnit_KeepGoing |
            CXTranslationUnit_SkipFunctionBodies * 0  // 不跳过函数体（需要区分定义和声明）
    );

    if (!tu) {
        std::cerr << "❌ libclang 无法解析文件: " << file_path << std::endl;
        clang_disposeIndex(index);
        return decls;
    }

    VisitorData data{&decls, tu};
    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, visitor, &data);

    // 按声明行号升序排列
    std::sort(decls.begin(), decls.end(), [](const DeclInfo& a, const DeclInfo& b) {
        return a.declaration_line < b.declaration_line;
    });

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);

    return decls;
}

}  // namespace utils
}  // namespace cbot
