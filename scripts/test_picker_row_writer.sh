#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BASELINE=${1:?provide unchanged current Watch 0ed6624 checkout}
[ "$(git -C "$BASELINE" rev-parse HEAD)" = 0ed6624e864d9ef31a22de948f91e34a15ef29c8 ]
[ -z "$(git -C "$BASELINE" status --porcelain -- apps/clock/nova apps/clock/faces include sdk)" ]
OUT=${OUTPUT_DIR:-${TMPDIR:-/tmp}/watch-picker-row-$$}
mkdir -p "$OUT"
COMMON='-std=c11 -Os -fPIC -fvisibility=hidden -DWATCH_NATIVE_STROKE_CACHE -DWATCH_NATIVE_COVERAGE_CACHE -DWATCH_NATIVE_DAMAGE_REPLAY -DWATCH_PICKER_COMPOSITOR_PLAN -DWATCH_CONTEXTS_CLIENT -DWATCH_COMPACT_NOVA_ASSETS -DWATCH_COMPRESSED_CALENDAR_ALPHA -DWATCH_COMPRESSED_FACE_ALPHA'
SAN=''
if [ "${SANITIZE:-0}" = 1 ]; then SAN='-g1 -fsanitize=address,undefined -fno-omit-frame-pointer'; fi
for role in reference candidate; do
 R="$BASELINE"; [ "$role" != candidate ] || R="$ROOT"
 ${CC:-cc} $COMMON $SAN -shared -I"$R" -I"$R/include" -I"$R/sdk/app" -I"$R/sdk/driver" "-DNOVA_SOURCE=\"$R/apps/clock/nova/nova.c\"" "$ROOT/tests/picker_compositor_module.c" -o "$OUT/$role.so"
done
for name in picker_compositor_test picker_row_writer_test; do
 ${CC:-cc} $COMMON $SAN -Wall -Wextra -Werror -DWATCH_FACE_RENDER_TEST -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/$name.c" -o "$OUT/$name"
 ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUT/$name"
done
${CC:-cc} -std=c11 -Os $SAN -DWATCH_CONTEXTS_CLIENT -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/picker_compositor_frames.c" -ldl -o "$OUT/frames"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUT/frames" "$OUT/reference.so" "$OUT/candidate.so"
${CC:-cc} -std=c11 -Os $SAN -Wall -Wextra -Werror -DWATCH_PICKER_COMPOSITOR_PLAN -DWATCH_COMPACT_NOVA_ASSETS -DWATCH_COMPRESSED_CALENDAR_ALPHA -DWATCH_COMPRESSED_FACE_ALPHA -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/picker_row_plain_test.c" -o "$OUT/plain"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUT/plain"
