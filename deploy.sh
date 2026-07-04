#!/bin/bash
# Senior Embedded Linux Architect Deploy Script

BOARD_IP=${1:-"10.203.129.238"}
BOARD_DIR="/root/code"

echo "=== Step 1: Check if build package exists ==="
if [ ! -d "package" ]; then
    echo "Error: package directory not found. Please run ./build.sh first."
    exit 1
fi

echo "=== Step 2: Deploying to board ${BOARD_IP}:${BOARD_DIR} ==="

# 在板子上预先建立好所有 package 内含有的子目录结构，以防止老旧 sshd 在 scp 时抛出 canonicalization error
ssh -o StrictHostKeyChecking=no root@${BOARD_IP} "mkdir -p ${BOARD_DIR} ${BOARD_DIR}/libs ${BOARD_DIR}/config ${BOARD_DIR}/assets ${BOARD_DIR}/scripts"

# 递归上传 package 各项文件到板端
scp -o StrictHostKeyChecking=no -r package/* root@${BOARD_IP}:${BOARD_DIR}/

echo "=== Step 3: Verifying deployment ==="
ssh -o StrictHostKeyChecking=no root@${BOARD_IP} "ls -la ${BOARD_DIR}"

echo "=== Deployment Completed Successfully ==="
echo "You can now run the app on the board using: ssh root@${BOARD_IP} 'sh /root/code/run.sh'"
