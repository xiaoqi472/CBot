#pragma once
#include <string>
#include <vector>

namespace cbot {
namespace commands {

void handle_format(const std::vector<std::string>& targets, bool init_mode);

}  // namespace commands
}  // namespace cbot
