#pragma once

#include <string>

namespace cbot {
namespace commands {

/**
 * @brief 处理 init 子命令，初始化标准 C++ 项目结构
 * @param project_name 用户输入的项目名称
 */
void handle_init(const std::string& project_name);

}  // namespace commands
}  // namespace cbot