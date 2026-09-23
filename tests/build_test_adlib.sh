#!/bin/sh
# Build the offline AdLib driver test (tests/test_adlib.c) into build/test_adlib
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_adlib_obj
SDK=$(xcrun --show-sdk-path)
CXXF="-O2 -std=c++17 -nostdinc++ -isystem $SDK/usr/include/c++/v1"
for f in src/sound/opl.cpp src/sound/ymfm/ymfm_opl.cpp src/sound/ymfm/ymfm_adpcm.cpp src/sound/ymfm/ymfm_pcm.cpp; do
  clang++ $CXXF -c $f -o build/test_adlib_obj/$(basename $f .cpp).o
done
CF="-O2 -std=gnu11 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -fno-strict-aliasing -fwrapv -I/opt/homebrew/include"
clang $CF -c tests/test_adlib.c -o build/test_adlib_obj/test_adlib.o
clang $CF -c src/sound/adlib_drv.c -o build/test_adlib_obj/adlib_drv.o
clang $CF -c src/game/seg_0e9f.c -o build/test_adlib_obj/seg_0e9f.o
clang $CF -c src/core/loader.c -o build/test_adlib_obj/loader.o
clang $CF -c src/core/mem.c -o build/test_adlib_obj/mem.o
clang++ build/test_adlib_obj/*.o -o build/test_adlib
echo built build/test_adlib
