# 🤖 cbot: C++ 开发者的大模型副驾驶

`cbot` 是一个专为 C++ 开发者设计的轻量级命令行辅助工具。它将**确定性的本地文件操作**与**大语言模型（Gemini 2.5 Flash）的语义推理**结合，帮你从繁琐的配置和文档工作中解脱出来。

---

## ✨ 功能一览

### ⚡ `cbot init <project_name>` — 初始化项目

一键生成规范的 C++ 项目骨架：

```
MyProject/
├── CMakeLists.txt
├── src/
│   └── main.cpp
└── include/
```

### 🏗️ `cbot cmake [路径]` — 智能生成 CMakeLists.txt

扫描指定目录（默认当前目录）下的所有 C/C++ 源文件，提取 `#include` 依赖，交由 AI 推断并生成完整的 `CMakeLists.txt`。

**依赖处理策略：**
- 轻量级库（如 `cpr`、`nlohmann_json`）：找不到则自动通过 `FetchContent` 拉取
- 重量级库（如 `OpenCV`、`ROS`）：找不到则 `FATAL_ERROR` 报错，要求手动安装

**增量更新机制：**

首次生成的文件会包含一对管理标记：

```cmake
# === CBOT_MANAGED_BEGIN ===
... AI 生成的内容 ...
# === CBOT_MANAGED_END ===

# ---- 以下为用户自定义区（cbot cmake 不会修改此区域）----
# 在这里添加你自己的 target、compile flags 等
```

再次执行 `cbot cmake` 时，只替换两个标记之间的内容，**标记之外的用户自定义内容永远不会被覆盖**。对于没有管理标记的旧文件，会提示是否覆写，确认后自动备份为 `CMakeLists.txt.bak`。

### 🔨 `cbot build` — 一键编译

自动检测或创建 `build/` 目录，执行 `cmake .. && make -j4`，从生成配置到多线程编译一气呵成。

### 📝 `cbot doc <file1> [file2] ...` — AI 添加 Doxygen 注释

使用 **libclang** 在本地解析 C++ AST，精确提取所有函数/类定义的行号和注释状态，再将结构化信息连同完整源码一并发给 AI。AI **只输出标签化的注释块**，不接触任何代码行；本地按行号将注释精确插入或替换到原文件对应位置。

- **代码安全**：原文件的所有代码行完全不经过 AI，从机制上杜绝 AI 误改代码的可能
- **智能更新**：已有注释的函数会与当前代码核对，参数、返回值描述不符时自动更新
- **预览确认**：写入前在终端完整展示结果，必须手动确认后才会覆写原文件
- **批量处理**：支持一次传入多个文件

### 🎨 `cbot format [--init] [路径/文件...]` — 代码格式化

统一代码风格，降低团队协作和代码审查成本。

**初始化配置文件：**

```bash
cbot format --init
```

在当前目录生成 `.clang-format` 配置文件，以 `BasedOnStyle: Google` 为基础，并调整为更常见的 C++ 风格（4 空格缩进、指针左对齐）。若文件已存在，会询问是否覆写。

**格式化代码：**

```bash
cbot format                        # 递归格式化当前目录下所有 C/C++ 文件
cbot format src/main.cpp           # 格式化指定文件
cbot format src/ include/          # 格式化指定目录（递归）
cbot format src/foo.cpp mylib/     # 文件和目录混用
```

- 自动跳过 `build/`、`.git/`、`.vscode/` 等无关目录
- 格式化某个文件失败时，会询问 **[A]bort / [S]kip**：选择 Abort 则恢复所有已格式化文件的原始内容再退出，选择 Skip 则跳过当前文件继续

### 💬 `cbot commit` — AI 生成 Git 提交信息

自动读取暂存区变更，交由 AI 推断修改目的，生成符合 Angular 规范的中文 commit message。

```bash
git add .
cbot commit
```

**工作流程：**
1. 读取 `git diff --staged` 暂存区变更
2. AI 分析变更内容，生成符合 Angular 规范的提交信息（`feat:`、`fix:`、`refactor:` 等）
3. 终端打印预览，询问 **[y/e/N]**：
   - `y` — 直接执行提交
   - `e` — 调起编辑器（`$EDITOR`，默认 `nano`）修改后提交
   - `N` — 取消操作

- diff 超过 8000 字符时自动降级为 `--stat` 模式并提示用户，避免超出上下文限制

### 🌐 `cbot test_llm` — 测试 API 连通性

向 Gemini API 发送一条测试请求，验证 API Key 和网络是否正常。

---

## 🚀 快速开始

### 1. 环境依赖

- **编译器**：支持 C++17 的 GCC 或 Clang
- **系统库**：`OpenSSL`、`libcurl`（用于 HTTPS 通信）、`libclang-dev`（用于 `cbot doc` 的 C++ 代码解析）、`clang-format`（用于 `cbot format`）
- **API Key**：前往 [Google AI Studio](https://aistudio.google.com/) 获取免费的 Gemini API Key

```bash
# Ubuntu/Debian 安装系统依赖
sudo apt install libssl-dev libcurl4-openssl-dev libclang-dev clang-format
```

### 2. 编译

```bash
git clone https://github.com/xiaoqi472/CBot.git
cd CBot
mkdir build && cd build
cmake ..
make -j4
```

推荐将编译产物 `cbot` 添加到系统 `PATH`，以便全局使用。

### 3. 配置 API Key

```bash
export CBOT_API_KEY="你的_Gemini_API_Key"
# 国内环境需配置代理
export https_proxy="http://127.0.0.1:你的代理端口"
```

---

## 🛠️ 命令速查

| 命令 | 说明 | 示例 |
| :--- | :--- | :--- |
| `init <name>` | 初始化 C++ 项目骨架 | `cbot init MyProject` |
| `cmake [路径]` | 智能生成/增量更新 CMakeLists.txt | `cbot cmake` 或 `cbot cmake ../other` |
| `build` | 一键编译当前项目 | `cbot build` |
| `doc <文件...>` | AI 添加 Doxygen 注释 | `cbot doc src/main.cpp include/utils.hpp` |
| `format [--init] [路径/文件...]` | 格式化代码 | `cbot format` 或 `cbot format --init` |
| `commit` | AI 生成 Git 提交信息 | `cbot commit` |
| `test_llm` | 测试 Gemini API 连通性 | `cbot test_llm` |

---

## ⚠️ 注意事项

- **`CBOT_API_KEY` 必须设置**，否则所有调用 AI 的命令（`cmake`、`doc`、`test_llm`）均会失败。
- **`cbot cmake` 的管理标记不要手动删除**，否则下次运行将退回到整体覆写模式。用户自定义内容请统一写在 `CBOT_MANAGED_END` 标记之后。
- **`cbot doc` 会修改源文件**，虽然写入前有预览确认步骤，建议在 Git 工作区干净的状态下使用，以便随时回退。
- **`cbot doc` 依赖 libclang**，编译前需安装 `libclang-dev`，否则 cmake 会报错退出。
- **`cbot build` 使用 `make -j4`** 固定 4 线程并行编译，如需调整请直接进入 `build/` 目录手动执行。
- **重量级依赖（OpenCV、ROS 等）需要手动安装**，`cbot cmake` 只会生成 `find_package` 检测语句，不会自动下载。
- **`cbot format` 依赖 `clang-format`**，执行前会自动检测是否安装，未安装时会给出安装提示。
- **`cbot format` 会就地修改源文件**，建议在 Git 工作区干净的状态下使用。格式化失败时支持 Abort 回滚，但成功完成的格式化不可自动撤销。
- **`cbot format --init` 生成的 `.clang-format` 仅为推荐模板**，可根据团队规范自行修改，`cbot format` 执行时会自动读取该文件。
- **`cbot commit` 依赖暂存区有内容**，执行前请先 `git add`，否则会提示报错退出。
- **`cbot commit` 的 `[e]dit` 选项**默认调起 `$EDITOR` 环境变量指定的编辑器，未设置时 fallback 到 `nano`。

---

## ⚖️ 许可证

基于 MIT License 开源。欢迎拆解、重构和 Pull Request。

---
**Maintained by Zhang Wenbo**
