#!/usr/bin/env bash
# Comet OS reproducible build: installs the toolchain (Debian/Ubuntu) if missing, then builds comet-os.iso
set -euo pipefail
cd "$(dirname "$0")"
need=(gcc ld nasm grub-mkrescue xorriso mformat)
missing=()
for t in "${need[@]}"; do command -v "$t" >/dev/null 2>&1 || missing+=("$t"); done
if [ ${#missing[@]} -gt 0 ]; then
  echo "Missing tools: ${missing[*]}"
  if command -v apt-get >/dev/null 2>&1; then
    echo "Installing build dependencies (needs sudo)..."
    sudo apt-get update
    sudo apt-get install -y build-essential gcc-multilib nasm grub-pc-bin grub-common xorriso mtools
  else
    echo "Install: gcc (with -m32 support), binutils, nasm, grub-pc-bin, xorriso, mtools"; exit 1
  fi
fi
echo '#include <stddef.h>' | gcc -m32 -x c -c - -o /dev/null 2>/dev/null || { echo "gcc -m32 not working: install gcc-multilib"; exit 1; }
make clean
make iso
ls -lh comet-os.iso
echo "Next: ./tools/vbox-create.sh   (creates and boots a VirtualBox VM)"
