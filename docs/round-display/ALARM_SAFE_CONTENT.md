# Shared Home alarm modal: safe content and physical input

This is the first actual Home content caller after provider inset propagation.
The alarm/countdown/error modal now renders inside `clock_viewport.layout.safe`
and uses the same rectangle for dismiss/back/retry hit testing. Full display
background clearing and physical key dismissal remain unchanged. Touch positions
are not scaled or clamped: a gutter touch does not activate a modal control.

The renderer validates the rectangle before clearing or offsetting pixels. It
uses the original stride and an offset canvas, not a new allocation or resampled
image. Only content width/height change; the provider still owns the full native
lease. The old renderer entry point remains a full-surface wrapper. Zero-inset
calls retain the previous pixels and control boundaries.

Tests run the production renderer over 160 combinations of alarm/countdown,
blocked/RTC-error, dismissing and uncertain output at conservative round,
asymmetric portrait/landscape and ordinary rectangular dimensions. They check
black gutters, unchanged row padding, invalid bounds without writes, every
physical coordinate's action and zero-inset parity. Actual Home lifecycle tests
also inject provider insets, reject just-above-dismiss, retry and bottom-gutter
taps, then accept the translated dismiss edge through the real event queue.
Normal and ASan/UBSan runs are included; the scripts explicitly disable LSan.

Reproduce using the exact System/Utilities pins in dependencies.lock.json:

```sh
python scripts/test_alarm_safe_area.py
python scripts/test_fast_wake_clock.py --system-apps .source/system --utilities .source/utilities
python scripts/test_fast_wake_clock.py --system-apps .source/system --utilities .source/utilities --native
python -m unittest discover -s tests -v
```

Existing Universal Watch CI includes the new renderer matrix and the inherited
actual Home tests. This is not complete round UI support: watch faces, picker,
Quick and other content callers remain separate. The tests use conservative
rectangles, not a new contour ABI or device-specific layout branch. Board drivers,
provisioning, exact product memory/boot and physical display/touch/sleep validation
remain port work. No device was flashed or hardware behavior qualified here.
