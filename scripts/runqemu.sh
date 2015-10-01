#!/bin/bash

qemu-system-x86_64 -enable-kvm -cpu host -boot d -m 1536 -kernel ./output/images/bzImage -append "boot=live ipv6.disable=1 " -initrd ./output/images/rootfs.cpio.xz
