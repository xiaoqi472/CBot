#pragma once
#include <string>

namespace cbot {
namespace commands {

/**
 * @brief 处理 cmake 子命令，智能分析项目结构并生成 CMakeLists.txt
 * @param target_path 目标项目的相对或绝对路径，默认为当前目录 "."
 */
void handle_cmake(const std::string& target_path = ".");

} // namespace commands
} // namespace cbot