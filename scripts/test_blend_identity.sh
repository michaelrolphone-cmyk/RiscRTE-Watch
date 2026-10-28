#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BASE=${1:?clean comparison checkout}
OUT=${OUTPUT_DIR:?output directory}
mkdir -p "$OUT"
COMMON='-std=c11 -O2 -fPIC -fvisibility=hidden -DWATCH_NATIVE_STROKE_CACHE -DWATCH_NATIVE_COVERAGE_CACHE -DWATCH_CONTEXTS_CLIENT -DWATCH_COMPACT_NOVA_ASSETS -DWATCH_COMPRESSED_CALENDAR_ALPHA -DWATCH_COMPRESSED_FACE_ALPHA -DWATCH_PICKER_COMPOSITOR_PLAN'
SAN=''
if [ "${SANITIZE:-0}" = 1 ]; then SAN='-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer'; fi
for kind in reference candidate; do
 SRC=$ROOT; [ "$kind" = candidate ] || SRC=$BASE
 ${CC:-cc} $COMMON $SAN -shared -I"$SRC" -I"$SRC/include" -I"$SRC/sdk/app" -I"$SRC/sdk/driver" "-DNOVA_SOURCE=\"$SRC/apps/clock/nova/nova.c\"" "$ROOT/tests/blend_face_module.c" -o "$OUT/$kind.so"
done
${CC:-cc} $COMMON $SAN -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "-DNOVA_SOURCE=\"$ROOT/apps/clock/nova/nova.c\"" "$ROOT/tests/blend_identity_test.c" -o "$OUT/oracle"
ASAN_OPTIONS=detect_leaks=0 "$OUT/oracle"
${CC:-cc} $COMMON $SAN -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/picker_compositor_frames.c" -ldl -o "$OUT/frames"
ASAN_OPTIONS=detect_leaks=0 "$OUT/frames" "$OUT/reference.so" "$OUT/candidate.so"

${CC:-cc} $COMMON $SAN -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/blend_face_frames.c" -ldl -o "$OUT/face-frames"
ASAN_OPTIONS=detect_leaks=0 "$OUT/face-frames" "$OUT/reference.so" "$OUT/candidate.so"
