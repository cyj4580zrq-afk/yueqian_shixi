#!/bin/bash
# Senior Embedded Linux Architect Build Script

set -e

echo "=== Step 1: Clean build environment ==="
make clean

echo "=== Step 2: Compile Board UI Client & Host Server ==="
make install

echo "=== Step 3: Copying board run script ==="
if [ -f "run.sh" ]; then
    cp run.sh package/run.sh
    chmod +x package/run.sh
    echo "Dynamic run.sh copied to package successfully."
else
    echo "Warning: root-level run.sh not found. Generating default run script."
    cat << 'EOF' > package/run.sh
#!/bin/sh
# Run script for RK1808 Board
SCRIPT_DIR=$(cd "$(dirname "$0")"; pwd)
cd "$SCRIPT_DIR"
export LD_LIBRARY_PATH="$SCRIPT_DIR/libs:$LD_LIBRARY_PATH"
./app_ui
EOF
    chmod +x package/run.sh
fi

echo "=== Build & Package Completed Successfully ==="
ls -la package/
