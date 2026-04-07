#!/usr/bin/env bash
set -e

REPO_URL="https://github.com/xiaoqi472/CBot.git"
INSTALL_DIR="$HOME/.local/cbot"
BIN_PATH="/usr/local/bin/cbot"

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

info()    { echo -e "${GREEN}[cbot]${NC} $1"; }
warning() { echo -e "${YELLOW}[cbot]${NC} $1"; }
error()   { echo -e "${RED}[cbot]${NC} $1"; exit 1; }

# 检查是否为 Debian/Ubuntu 系
if ! command -v apt &>/dev/null; then
    error "当前仅支持 Debian/Ubuntu 系发行版（需要 apt）。"
fi

info "检查并安装系统依赖..."
DEPS=(git cmake make g++ libssl-dev libcurl4-openssl-dev libclang-dev clang-format)
MISSING=()
for dep in "${DEPS[@]}"; do
    if ! dpkg -s "$dep" &>/dev/null; then
        MISSING+=("$dep")
    fi
done

if [ ${#MISSING[@]} -gt 0 ]; then
    info "需要安装以下依赖: ${MISSING[*]}"
    sudo apt update -qq
    sudo apt install -y "${MISSING[@]}"
else
    info "所有依赖已满足。"
fi

# 拉取源码
if [ -d "$INSTALL_DIR/.git" ]; then
    info "检测到已有安装目录，正在更新源码..."
    git -C "$INSTALL_DIR" pull --ff-only
else
    info "正在克隆仓库到 $INSTALL_DIR ..."
    rm -rf "$INSTALL_DIR"
    git clone "$REPO_URL" "$INSTALL_DIR"
fi

# 编译
info "正在编译 cbot..."
mkdir -p "$INSTALL_DIR/build"
cmake -S "$INSTALL_DIR" -B "$INSTALL_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$INSTALL_DIR/build" -- -j"$(nproc)"

# 安装软链接
info "正在安装到 $BIN_PATH ..."
sudo ln -sf "$INSTALL_DIR/build/cbot" "$BIN_PATH"

# 安装 bash 补全
COMPLETION_SRC="$INSTALL_DIR/cbot-completion.bash"
COMPLETION_DST="/etc/bash_completion.d/cbot"
if [ -f "$COMPLETION_SRC" ]; then
    info "正在安装 bash 补全..."
    sudo cp "$COMPLETION_SRC" "$COMPLETION_DST"
    # shellcheck disable=SC1090
    source "$COMPLETION_DST" 2>/dev/null || true
fi

echo ""
info "✅ 安装完成！cbot 已可全局使用。"
echo ""
echo "  使用前请设置 API Key："
echo "    export CBOT_API_KEY=\"你的_Gemini_API_Key\""
echo "  建议将上述行写入 ~/.bashrc 以永久生效。"
echo ""
echo "  运行 cbot --help 查看可用命令。"
