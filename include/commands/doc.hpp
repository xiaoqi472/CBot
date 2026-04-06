#pragma once
#include <string>
#include <vector>

namespace cbot {
namespace commands {

/**
 * @brief 处理 doc 子命令，为指定的多个 C++ 文件添加 Doxygen 注释
 * @param files 待处理的文件路径列表
 */
void handle_doc(const std::vector<std::string>& files);

}  // namespace commands
}  // namespace cbot
