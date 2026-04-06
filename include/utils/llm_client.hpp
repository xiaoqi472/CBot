#pragma once

#include <optional>
#include <string>

namespace cbot {
namespace utils {

class LLMClient {
   public:
    /**
     * @brief 构造函数，初始化 Gemini API 客户端。
     * @param model 使用的模型名称 (默认: gemini-2.5-flash)
     */
    explicit LLMClient(const std::string& model = "gemini-2.5-flash");
    /**
     * @brief 向 Gemini 模型发送生成请求
     * @param system_prompt 系统提示词（用于设定角色和行为准则）
     * @param user_prompt 用户提示词（用于发送具体任务或代码内容）
     * @return 返回大模型的文本响应。如果网络失败或解析错误，返回 std::nullopt。
     */
    std::optional<std::string> generate_response(const std::string& system_prompt,
                                                 const std::string& user_prompt);

   private:
    std::string api_key_;
    std::string endpoint_url_;  // 内部维护完整的 API 端点地址

    // 从环境变量 CBOT_API_KEY 获取密钥
    void load_api_key();
};

}  // namespace utils
}  // namespace cbot