#!/bin/bash

set -e

FILE="$1"
OBJFILE="${FILE%.c}.o"

if [ -x "./configure" ]; then
  ./configure > /dev/null 2>&1
  CC=${CC:-cc}
  compiler=$($CC --version 2>/dev/null)

  if echo "$compiler" | grep -qi "clang"; then
      echo "Detected Clang"
      echo 'CFLAGS += -Wno-everything -Werror' >> config.mak
  elif echo "$compiler" | grep -qi "gcc"; then
      echo "Detected GCC"
      echo 'CFLAGS += -w -Werror' >> config.mak
  else
      echo "Unknown compiler: using default flags"
      echo 'CFLAGS += -Werror' >> config.mak
  fi
fi

if [ -f Makefile ] || [ -f makefile ]; then
  echo "Makefile found: building with bear"
  bear -- make -B "$OBJFILE"

  ARGS=$(jq -r --arg file "$FILE" '.[] | select(.file | endswith($file)) | .arguments[]' compile_commands.json)

  if [[ -z "$ARGS" ]]; then
    echo "Error: compile_commands.json has no entry for $FILE"
    exit 1
  fi

  clang --analyze \
    -Xanalyzer -analyzer-checker=alpha.core \
    -Xanalyzer -analyzer-checker=alpha.unix \
    -Xanalyzer -analyzer-checker=alpha.security \
    $ARGS
else
  INCLUDES=""
  for dir in $(find . -type d \( -name include -o -name inc \)); do
    INCLUDES+=" -I$dir"
  done

  clang --analyze \
    -Xanalyzer -analyzer-checker=alpha.core \
    -Xanalyzer -analyzer-checker=alpha.unix \
    -Xanalyzer -analyzer-checker=alpha.security \
    $INCLUDES \
    "$FILE"
fi
set -e

FILE="$1"
OBJFILE="${FILE%.c}.o"

if [ -x "./configure" ]; then
  ./configure > /dev/null 2>&1
  CC=${CC:-cc}
  compiler=$($CC --version 2>/dev/null)

  if echo "$compiler" | grep -qi "clang"; then
      echo "Detected Clang"
      echo 'CFLAGS += -Wno-everything -Werror' >> config.mak
  elif echo "$compiler" | grep -qi "gcc"; then
      echo "Detected GCC"
      echo 'CFLAGS += -w -Werror' >> config.mak
  else
      echo "Unknown compiler: using default flags"
      echo 'CFLAGS += -Werror' >> config.mak
  fi
fi

if [ -f Makefile ] || [ -f makefile ]; then
  echo "Makefile found. Make"
  bear -- make -B "$OBJFILE"

  ARGS=$(jq -r --arg file "$FILE" '.[] | select(.file | endswith($file)) | .arguments[]' compile_commands.json)

  if [[ -z "$ARGS" ]]; then
    echo "Error: compile_commands.json has no entry for $FILE"
    exit 1
  fi
  echo "clang output:"
  clang --analyze \
    -Xanalyzer -analyzer-checker=alpha.core \
    -Xanalyzer -analyzer-checker=alpha.unix \
    -Xanalyzer -analyzer-checker=alpha.security \
    $ARGS
else
  INCLUDES=""
  for dir in $(find . -type d \( -name include -o -name inc \)); do
    INCLUDES+=" -I$dir"
  done
  echo "clang output:"
  clang --analyze \
    -Xanalyzer -analyzer-checker=alpha.core \
    -Xanalyzer -analyzer-checker=alpha.unix \
    -Xanalyzer -analyzer-checker=alpha.security \
    $INCLUDES \
    "$FILE"
fi