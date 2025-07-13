#!/bin/bash
set -e

./autogen.sh > /dev/null 2>&1 || true

make distclean || make clean > /dev/null 2>&1 || true

./configure --enable-crypttests > /dev/null 2>&1 || true

make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" > /dev/null 2>&1 || true

./wolfcrypt/test/testwolfcrypt
