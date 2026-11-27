#!/usr/bin/env bash
# Creates a VirtualBox VM tuned for Comet OS and starts it.
set -euo pipefail
cd "$(dirname "$0")/.."
VM=${1:-"Comet OS"}
ISO="$PWD/comet-os.iso"
DISK="$PWD/build/comet-disk.vdi"
[ -f "$ISO" ] || { echo "Build the ISO first: ./build.sh"; exit 1; }
VBoxManage createvm --name "$VM" --ostype Other --register
# Comet OS talks to VBoxVGA (Bochs VBE ports), PS/2 input and a legacy IDE controller
VBoxManage modifyvm "$VM" --memory 1024 --vram 128 --cpus 1 --firmware bios --graphicscontroller vboxvga \
  --mouse ps2 --keyboard ps2 --usbohci off --usbehci off --usbxhci off --audio-driver none --boot1 dvd --boot2 disk --nic1 nat --nictype1 Am79C973
mkdir -p build
VBoxManage createmedium disk --filename "$DISK" --size 4096 --format VDI
VBoxManage storagectl "$VM" --name IDE --add ide --controller PIIX4
VBoxManage storageattach "$VM" --storagectl IDE --port 0 --device 0 --type hdd --medium "$DISK"
VBoxManage storageattach "$VM" --storagectl IDE --port 1 --device 0 --type dvddrive --medium "$ISO"
VBoxManage startvm "$VM"
