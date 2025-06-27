# #!/bin/bash

# set -e

# FILE="$1"
# OBJFILE=$(echo "$FILE" | sed 's/\.c$/.o/')

# ./configure --disable-asm

# bear -- make -B "$OBJFILE"

# clang --analyze \
#   -Xanalyzer -analyzer-checker=alpha.core \
#   -Xanalyzer -analyzer-checker=alpha.unix \  
#   -Xanalyzer -analyzer-checker=alpha.security \
#   $(jq -r --arg file "$FILE" '.[] | select(.file | endswith($file)) | .arguments[]' compile_commands.json)

#!/bin/bash

set -e

FILE="$1"
OBJFILE=$(echo "$FILE" | sed 's/\.c$/.o/')

# ./configure \
#   --disable-asm \
#   --enable-gpl \
#   --enable-libopenh264 \
#   # --enable-x11grab \
#   --extra-cflags="-I$(brew --prefix)/include -I/opt/X11/include" \
#   --extra-ldflags="-L$(brew --prefix)/lib -L/opt/X11/lib"

./configure

echo 'CFLAGS += -Wno-incompatible-function-pointer-types -Wno-string-plus-int -Wno-incompatible-pointer-types-discards-qualifiers -Werror -Wno-unused-but-set-variable -Wno-absolute-value -Wno-cast-qual -Wno-non-literal-null-conversion -Wno-int-in-bool-context -Wno-constant-conversion -Wno-shift-negative-value -Wno-incompatible-pointer-types -Wno-array-parameter -Wno-unused-const-variable -Wno-bool-operation -Wno-sometimes-uninitialized -Wno-deprecated-declarations -Wno-pointer-bool-conversion' >> config.mak

bear -- make -B "$OBJFILE"

ARGS=$(jq -r --arg file "$FILE" '.[] | select(.file | endswith($file)) | .arguments[]' compile_commands.json)

echo $ARGS

if [[ -z "$ARGS" ]]; then
  echo "Error: compile_commands.json has no entry for $FILE"
  exit 1
fi

clang --analyze \
  -Xanalyzer -analyzer-checker=alpha.core \
  -Xanalyzer -analyzer-checker=alpha.unix \
  -Xanalyzer -analyzer-checker=alpha.security \
  -I/Users/muyeedahmed/VulkanSDK/1.4.313.1/macOS/include \
  $ARGS
