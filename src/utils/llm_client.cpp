#include "utils/llm_client.hpp"
#include <iostream>
#include <cstdlib>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace cbot {
namespace utils {

LLMClient::LLMClient(const std::string& model) {
    // 自动拼接 Gemini 完整的 API 端点
    endpoint_url_ = "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
    load_api_key();
}

void LLMClient::load_api_key() {
    const char* env_key = std::getenv("CBOT_API_KEY");
    if (env_key) {
        api_key_ = env_key;
    } else {
        std::cerr << "警告: 未检测到环境变量 CBOT_API_KEY。" << std::endl;
        std::cerr << "请使用 export CBOT_API_KEY='你的 Gemini 密钥' 进行设置。" << std::endl;
    }
}

std::optional<std::string> LLMClient::generate_response(const std::string& system_prompt, const std::string& user_prompt) {
    if (api_key_.empty()) {
        std::cerr << "错误: API Key 为空，无法发起请求。" << std::endl;
        return std::nullopt;
    }

    // 构造符合 Gemini 规范的 JSON 请求体
    json payload = {
        {"contents", {{
            {"role", "user"},
            {"parts", {{ {"text", user_prompt} }}}
        }}},
        {"generationConfig", {
            {"temperature", 0.1} // 保持低随机性，确保代码任务的稳定性
        }}
    };

    // 如果提供了系统提示词，则加入 systemInstruction 字段 (Gemini 专属结构)
    if (!system_prompt.empty()) {
        payload["systemInstruction"] = {
            {"parts", {{ {"text", system_prompt} }}}
        };
    }

    // 发起 HTTP POST 请求，Gemini 使用 x-goog-api-key 传递鉴权
    cpr::Response r = cpr::Post(
        cpr::Url{endpoint_url_},
        cpr::Header{
            {"x-goog-api-key", api_key_},
            {"Content-Type", "application/json"}
        },
        cpr::Body{payload.dump()}
    );

    // 异常处理
    if (r.status_code != 200) {
        std::cerr << "网络请求失败，状态码: " << r.status_code << std::endl;
        std::cerr << "错误信息: " << r.text << std::endl;
        return std::nullopt;
    }

    // 解析返回的 JSON
    try {
        json response_json = json::parse(r.text);
        if (response_json.contains("candidates") && !response_json["candidates"].empty()) {
            return response_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        } else {
            std::cerr << "错误: 无法从响应中提取 candidates 字段。" << std::endl;
            std::cerr << "原始响应: " << r.text << std::endl;
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON 解析失败: " << e.what() << std::endl;
    }

    return std::nullopt;
}

} // namespace utils
} // namespace cbot