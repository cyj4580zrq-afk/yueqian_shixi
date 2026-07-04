#!/bin/bash
set -e

BOARD_IP="10.203.129.239"
BOARD_PASS="123456"

echo "=================================================="
echo " 开始配置 WSL 与开发板的双向免密 SSH 登录"
echo "=================================================="

# 1. 在 WSL 生成 SSH Key (如果不存在)
if [ ! -f ~/.ssh/id_rsa ]; then
    echo "[1/4] 正在 WSL 中生成 SSH 密钥对..."
    mkdir -p ~/.ssh
    chmod 700 ~/.ssh
    ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
else
    echo "[1/4] WSL 中的 SSH 密钥对已存在，跳过生成。"
fi

# 2. 将 WSL 公钥拷贝到开发板 (使用 sshpass 免手动输入密码)
echo "[2/4] 正在将 WSL 公钥拷贝到开发板..."
sshpass -p "$BOARD_PASS" ssh-copy-id -o StrictHostKeyChecking=no root@$BOARD_IP

# 3. 在开发板中生成 SSH Key (如果不存在)
echo "[3/4] 正在开发板中生成 SSH 密钥对..."
sshpass -p "$BOARD_PASS" ssh -o StrictHostKeyChecking=no root@$BOARD_IP "mkdir -p ~/.ssh && [ ! -f ~/.ssh/id_rsa ] && ssh-keygen -t rsa -N '' -f ~/.ssh/id_rsa || true"

# 4. 拉取开发板的公钥并追加到 WSL 的 authorized_keys
echo "[4/4] 正在拉取开发板的公钥并配置到 WSL..."
sshpass -p "$BOARD_PASS" scp root@$BOARD_IP:~/.ssh/id_rsa.pub /tmp/board_id_rsa.pub
cat /tmp/board_id_rsa.pub >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
rm -f /tmp/board_id_rsa.pub

echo "--------------------------------------------------"
echo " 配置完成！正在测试 WSL 免密登录开发板..."
ssh -o StrictHostKeyChecking=no root@$BOARD_IP "echo '[SUCCESS] WSL -> 开发板 免密登录成功！'"

echo "=================================================="
echo " 请在开发板上尝试免密 scp 到 WSL，测试命令："
echo " scp cmd.wav cyj@10.203.129.239:/home/cyj/workspace/ai_assistant/bin/wav/cmd.wav"
echo "=================================================="
