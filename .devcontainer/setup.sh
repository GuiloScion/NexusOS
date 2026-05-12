#!/usr/bin/env bash
# NexusOS devcontainer setup
# Installs the toolchain needed to build and run NexusOS in a Codespace.

set -euo pipefail

echo "==> Updating apt indexes..."
sudo apt-get update -y

echo "==> Installing build toolchain..."
sudo apt-get install -y --no-install-recommends \
    build-essential \
    nasm \
    binutils \
    gdb \
    qemu-system-x86 \
    xorriso \
    grub-pc-bin \
    grub-common \
    mtools

echo "==> Verifying tools..."
for tool in nasm gcc ld objcopy make qemu-system-x86_64 gdb; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf "  %-22s %s\n" "$tool" "$($tool --version 2>&1 | head -n1)"
    else
        echo "  MISSING: $tool" >&2
        exit 1
    fi
done

echo ""
echo "==> NexusOS dev environment ready."
echo "    Build:  make"
echo "    Run:    make run     (Ctrl-A then X to quit QEMU)"
echo "    Debug:  make debug"
