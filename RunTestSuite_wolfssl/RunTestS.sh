#!/bin/bash
set -e

./autogen.sh  > /dev/null 2>&1 || true

make clean > /dev/null 2>&1 || true

./configure --enable-test > /dev/null 2>&1 || true

make > /dev/null 2>&1 || true

./wolfcrypt/testwolfcrypt


