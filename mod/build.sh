#!/bin/sh
# Cross-builds GoTVR (xinput1_4.dll) with MinGW-w64 on Linux. Deps: MinHook (src), OpenXR SDK headers.
set -e
D=$(dirname "$0"); MH=${MH:-/home/claude/deps/minhook}; OXR=${OXR:-/home/claude/deps/oxr/include}
mkdir -p "$D/build"
for f in buffer hook trampoline hde/hde64; do x86_64-w64-mingw32-gcc -O2 -c "$MH/src/$f.c" -I"$MH/include" -o "$D/build/$(basename $f).o"; done
x86_64-w64-mingw32-g++ -O2 -std=c++17 -Wall -Wno-unused-function -Wno-cast-function-type -fno-exceptions -fno-rtti -I"$MH/include" -I"$OXR" -c "$D/src/gotvr.cpp" -o "$D/build/gotvr.o"
x86_64-w64-mingw32-g++ -shared -static -static-libgcc -static-libstdc++ -o "$D/build/xinput1_4.dll" "$D/build/gotvr.o" "$D/build/"buffer.o "$D/build/"hook.o "$D/build/"trampoline.o "$D/build/"hde64.o "$D/src/exports.def" -lbcrypt -ldxgi -ldxguid -luuid -Wl,--enable-stdcall-fixup -Wl,--kill-at
x86_64-w64-mingw32-strip "$D/build/xinput1_4.dll"
# GoTVRHost.exe: owns the OpenXR session out of process
x86_64-w64-mingw32-g++ -O2 -std=c++17 -Wall -Wno-unused-function -Wno-cast-function-type -fno-exceptions -fno-rtti -municode -mwindows -I"$OXR" -o "$D/build/GoTVRHost.exe" "$D/src/host.cpp" -static -static-libgcc -static-libstdc++ -ld3d12 -ldxgi -ldxguid -luuid -lshell32 -ladvapi32
x86_64-w64-mingw32-strip "$D/build/GoTVRHost.exe"
