#!/bin/bash
set -e

make clean > /dev/null 2>&1 || true

./configure \
  --samples=../fate-suite \
  --disable-gpl \
  --disable-libx264 \
  --disable-asm \
  > /dev/null

make -j$(sysctl -n hw.ncpu) > /dev/null

make fate -j$(sysctl -n hw.ncpu)
