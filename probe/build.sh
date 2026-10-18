#!/bin/sh
# Builds xinput1_4.dll (GoTVR probe) with clang + lld-link (no Windows SDK / CRT needed).
set -e
llvm-dlltool -m i386:x86-64 -d kernel32.def -l kernel32.lib
llvm-dlltool -m i386:x86-64 -d bcrypt.def -l bcrypt.lib
clang --target=x86_64-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -Wall -Wno-unused-function -c probe.c -o probe.obj
lld-link /dll /nodefaultlib /entry:DllMain /subsystem:windows /machine:x64 /def:exports.def /out:xinput1_4.dll probe.obj kernel32.lib bcrypt.lib
