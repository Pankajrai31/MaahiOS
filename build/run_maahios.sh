#!/bin/bash
# MaahiOS Unified Build and Run Script
# Usage: bash run_maahios.sh

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

BUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$BUILD_DIR")"

echo -e "${YELLOW}=== MaahiOS Build & Run ===${NC}"
echo "Build directory: $BUILD_DIR"

# Step 1: Build the OS
echo -e "\n${YELLOW}[1/3] Building MaahiOS...${NC}"
cd "$BUILD_DIR"

if bash build.sh; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed - not launching QEMU${NC}"
    exit 1
fi

# Step 2: Check if QEMU is already running
echo -e "\n${YELLOW}[2/3] Checking for existing QEMU instances...${NC}"

# Check if qemu-system-i386 is running
if pgrep -x "qemu-system-i386" > /dev/null 2>&1; then
    echo -e "${RED}⚠ QEMU is already running!${NC}"
    echo "Please close the existing QEMU instance before running this script."
    exit 1
fi

# Step 3: Launch QEMU
echo -e "\n${YELLOW}[3/3] Launching QEMU...${NC}"
echo "Boot ISO: $BUILD_DIR/boot.iso"
echo "Serial log: $BUILD_DIR/serial.log"
echo ""
echo -e "${GREEN}QEMU will run indefinitely. Press Ctrl+C or close the window to stop.${NC}"
echo ""

# Remove old serial.log if exists
rm -f "$BUILD_DIR/serial.log"

# Launch QEMU (no timeout, runs until closed)
qemu-system-i386 \
    -cdrom "$BUILD_DIR/boot.iso" \
    -m 512M \
    -serial file:"$BUILD_DIR/serial.log"

echo -e "\n${YELLOW}QEMU exited.${NC}"
