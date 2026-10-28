#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${TMPDIR:-/tmp}/watch-native-stroke-$$
trap 'rm -f "$OUT"' EXIT HUP INT TERM
COMMON="-std=c11 -DWATCH_NATIVE_STROKE_CACHE -DWATCH_CONTEXTS_CLIENT -DWATCH_FACE_RENDER_TEST -DWATCH_COMPACT_NOVA_ASSETS -DWATCH_COMPRESSED_CALENDAR_ALPHA -DWATCH_COMPRESSED_FACE_ALPHA"
${CC:-cc} $COMMON -O2 -Wall -Wextra -Werror -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/native_stroke_cache_test.c" -o "$OUT"
"$OUT"
if [ "${SANITIZE:-0}" = 1 ]; then
 ${CC:-cc} $COMMON -O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/native_stroke_cache_test.c" -o "$OUT"
 ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUT"
fi
