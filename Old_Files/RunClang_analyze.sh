# #!/bin/bash

# set -e

# FILE="$1"
# OBJFILE=$(echo "$FILE" | sed 's/\.c$/.o/')

# ./configure --disable-asm

# bear -- make -B "$OBJFILE"

# clang --analyze \
#   -Xanalyzer -analyzer-checker=alpha.core \
#   -Xanalyzer -analyzer-checker=alpha.unix \  > /dev/null 2>&1
#   -Xanalyzer -analyzer-checker=alpha.security \
#   $(jq -r --arg file "$FILE" '.[] | select(.file | endswith($file)) | .arguments[]' compile_commands.json)

#!/bin/bash

set -e

FILE="$1"
OBJFILE=$(echo "$FILE" | sed 's/\.c$/.o/')

./configure

# echo 'CFLAGS += -Wno-incompatible-function-pointer-types -Wno-string-plus-int -Wno-incompatible-pointer-types-discards-qualifiers -Werror -Wno-unused-but-set-variable -Wno-absolute-value -Wno-cast-qual -Wno-non-literal-null-conversion -Wno-int-in-bool-context -Wno-constant-conversion -Wno-shift-negative-value -Wno-incompatible-pointer-types -Wno-array-parameter -Wno-unused-const-variable -Wno-bool-operation -Wno-sometimes-uninitialized -Wno-deprecated-declarations -Wno-pointer-bool-conversion' >> config.mak
echo 'CFLAGS += -Wno-everything -Werror' >> config.mak

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
