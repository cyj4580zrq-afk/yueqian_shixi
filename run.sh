#!/bin/sh
# 5. 自动配置工作目录、LD_LIBRARY_PATH 动态库与资源加载环境
# Senior Embedded Linux C++ Architect Standard

# 动态计算当前 run.sh 脚本所在的绝对物理路径
SCRIPT_DIR=$(cd "$(dirname "$0")"; pwd)
cd "$SCRIPT_DIR"

echo "=== Working Directory: $SCRIPT_DIR ==="
export LD_LIBRARY_PATH="$SCRIPT_DIR/libs":$LD_LIBRARY_PATH
echo "=== LD_LIBRARY_PATH: $LD_LIBRARY_PATH ==="

# 安全检查可执行程序
if [ ! -f "./app_ui" ]; then
    echo "Error: app_ui executable not found in $SCRIPT_DIR!"
    exit 1
fi

# 启动客户端并传递程序所在根目录作为默认运行根环境
./app_ui
