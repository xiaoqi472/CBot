# 🤖 cbot: C++ 开发者的大模型副驾驶

`cbot` 是一个专为 C++ 开发者设计的轻量级命令行辅助工具。它拒绝臃肿，将**确定性的文件操作**与**大语言模型（Gemini 2.5 Flash）的语义推理**结合，帮你从繁琐的配置和文档中解脱出来。

---

## ✨ 核心特性

* **⚡ 快速初始化 (`init`)**：一键生成规范的 C++ 项目骨架（src/include/CMakeLists.txt），告别手动创建文件夹的低级重复。
* **🏗️ 智能构建生成 (`cmake`)**：
    * **语义分析**：通过分析 `#include` 自动推断库依赖。
    * **防御性策略**：轻量级库（如 `cpr`, `json`）自动通过 `FetchContent` 拉取；重量级库（如 `OpenCV`, `ROS`）强制本地校验，找不到就报错。
* **🔨 一键构建 (`build`)**：自动检测或创建 `build` 目录，无缝执行 `cmake .. && make -j4`，从生成配置到多线程编译一气呵成。
* **📝 自动化文档 (`doc`)**：
    * **精确打击**：支持针对特定文件列表添加标准的 Doxygen 注释。
    * **人工审核**：在覆写前，将 AI 生成的代码完整输出在终端，确认无误后再写入，绝对安全。
* **🌐 混合架构**：本地 C++17 高效文件扫描 + 云端 Gemini API 深度逻辑推理。

---

## 🚀 快速开始

### 1. 环境准备
* **编译器**：支持 C++17 的 GCC 或 Clang。
* **依赖库**：`OpenSSL`, `libcurl`（用于 HTTPS 通信）。
* **API Key**：前往 [Google AI Studio](https://aistudio.google.com/) 获取免费的 Gemini API Key。

### 2. 编译安装
```bash
git clone https://github.com/xiaoqi472/CBot.git
cd cbot
mkdir build && cd build
cmake ..
make -j4
```

*(推荐：将编译后的 `cbot` 添加到系统 `PATH` 中，以便在任何目录下全局使用)*

### 3. 配置环境变量
为了安全起见，`cbot` 从环境变量中读取密钥：
```bash
export CBOT_API_KEY="你的_Gemini_API_Key"
# 如果你在国内环境，记得配置终端代理
export https_proxy="http://127.0.0.1:你的代理端口" 
```

---

## 🛠️ 使用指南

| 命令 | 说明 | 示例 |
| :--- | :--- | :--- |
| `init` | 初始化项目 | `cbot init MyProject` |
| `cmake` | 生成构建文件 | `cbot cmake [项目路径]` |
| `build` | 一键编译当前项目 | `cbot build` |
| `doc` | 添加 Doxygen 注释 | `cbot doc src/main.cpp include/utils.hpp` |
| `test_llm` | 测试 API 连通性 | `cbot test_llm` |

---

## 🥚 已知彩蛋：程序“自噬”现象 (Self-Eating Bug)

在本项目开发过程中，我们发现了一个有趣的现象。如果你尝试运行以下命令：
```bash
cbot doc src/commands/doc.cpp
```
你会发现 `doc` 模块在处理**自身源码**时，会发生“头部截断”和“尾部缺失”。

**技术原理：**
由于 `clean_markdown` 函数采用全局字符串查找 ` ``` ` 标记来清洗 AI 返回的代码块，而 `doc.cpp` 的源码中恰好包含了这些字符串（作为匹配逻辑和提示词）。结果导致程序把自己的源码误当成了 Markdown 标记，将其前后的内容全部“洗掉”了。

> **开发者注**：这并不是 Bug，而是 `cbot` 拥有强烈的“自我净化”意识。我们决定保留这一特性，作为它拥有灵魂的证据。（*好吧，这就是个 Bug，但是开发者没改，开发者害怕改完之后其它地方的 Markdown 标记反而清理不干净了...*）

---

## 🚧 路线图 (Roadmap)

- [✔] 增加 `cbot build` 模块，实现一键自动化编译。
- [ ] 增加 `--force` 参数，支持在 CI/CD 中静默运行。
- [ ] 升级 `cbot build`，让 AI 自动读取并分析报错信息。
- [ ] 完善 `dry-run` 模式。
- [ ] 解决“自噬” Bug（或许吧，如果它不再尝试净化自己的话）。

---

## ⚖️ 许可证

基于 MIT License 开源。欢迎各种暴力拆解、重构和 Pull Request。

---
**Maintained by Zhang Wenbo** *Last updated: 2026-04-05*
