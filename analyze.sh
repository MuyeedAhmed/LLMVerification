#!/bin/bash

set -e

FILE="$1"
OBJFILE="${FILE%.c}.o"

# Run ./configure if available
if [ -x "./configure" ]; then
  ./configure > /dev/null 2>&1

  # Suppress stale config.h warnings
  if [ -f config.h ]; then
    touch config.h
  fi

  # Detect compiler and update flags
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

# If Makefile exists, use Bear
if [ -f Makefile ] || [ -f makefile ]; then
  echo "Makefile found: cleaning and building with Bear"
  make clean || echo "Warning: make clean failed or not implemented"
  
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
  # Fallback: find -I includes and run clang manually
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
