#pragma once

namespace cbot {
namespace commands {

/**
 * @brief 处理 build 子命令，自动创建 build 目录并执行 cmake & make
 */
void handle_build();

} // namespace commands
} // namespace cbot