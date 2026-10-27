#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BASELINE=${1:?provide clean Watch baseline checkout 518e459908950a80ac0b44dc40c1c1a670d88e19}
[ "$(git -C "$BASELINE" rev-parse HEAD)" = 518e459908950a80ac0b44dc40c1c1a670d88e19 ]
[ -z "$(git -C "$BASELINE" status --porcelain -- apps/clock/nova apps/clock/faces include sdk)" ]
OUT=${OUTPUT_DIR:-${TMPDIR:-/tmp}/watch-picker-compositor-$$}
mkdir -p "$OUT"
COMMON='-std=c11 -O2 -fPIC -fvisibility=hidden -DWATCH_NATIVE_STROKE_CACHE -DWATCH_NATIVE_COVERAGE_CACHE -DWATCH_CONTEXTS_CLIENT -DWATCH_COMPACT_NOVA_ASSETS -DWATCH_COMPRESSED_CALENDAR_ALPHA -DWATCH_COMPRESSED_FACE_ALPHA'
SAN=''
if [ "${SANITIZE:-0}" = 1 ]; then SAN='-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer'; fi
${CC:-cc} $COMMON $SAN -shared -I"$BASELINE" -I"$BASELINE/include" -I"$BASELINE/sdk/app" -I"$BASELINE/sdk/driver" "-DNOVA_SOURCE=\"$BASELINE/apps/clock/nova/nova.c\"" "$ROOT/tests/picker_compositor_module.c" -o "$OUT/reference.so"
${CC:-cc} $COMMON $SAN -DWATCH_PICKER_COMPOSITOR_PLAN -shared -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "-DNOVA_SOURCE=\"$ROOT/apps/clock/nova/nova.c\"" "$ROOT/tests/picker_compositor_module.c" -o "$OUT/planned.so"
${CC:-cc} -std=c11 -O2 $SAN -DWATCH_CONTEXTS_CLIENT -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/picker_compositor_frames.c" -ldl -o "$OUT/frames"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUT/frames" "$OUT/reference.so" "$OUT/planned.so"
${CC:-cc} $COMMON $SAN -Wall -Wextra -Werror -DWATCH_PICKER_COMPOSITOR_PLAN -DWATCH_FACE_RENDER_TEST -I"$ROOT" -I"$ROOT/include" -I"$ROOT/sdk/app" -I"$ROOT/sdk/driver" "$ROOT/tests/picker_compositor_test.c" -o "$OUT/compositor-test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0} "$OUT/compositor-test"
python "$ROOT/tests/raster_cache_options_test.py"
