# Provider-owned Watch safe-area prerequisite

This change consumes the existing `display.output` v1 `safe_insets` in the shared
Watch viewport's native layout. It does not select a board, infer a circle from
square dimensions, change the ABI, or fork the default application. Provisioning
selects the physical provider; the provider supplies its truthful display info.

Previously `watch_viewport_open` passed NULL to `watch_layout_native_open`, losing
the already-supported content bounds. It now copies top/right/bottom/left
explicitly into the layout's existing inset type. The existing layout validation
rejects empty or invalid safe rectangles. Zero insets preserve rectangular behavior.

## Conservative round bring-up, not complete round adaptation

The test provider advertises a 466x466 surface with 69-pixel insets, yielding a
328x328 conservative content rectangle. Shared stack layout puts header, content
and footer inside that rectangle; the tests check their corners against a circle.
The number 69 exists only in this synthetic fixture, not production selection.
A real provider must establish its usable bounds from its panel/orientation data.

The full native raster, damage coordinates and touch coordinates remain unchanged.
Insets describe content placement, not a touch exclusion zone: edge gestures must
not be clamped into controls. This does not make every current face, picker, Quick
control or application consume `layout.safe`, nor provide a circle-contour API.
Those caller adaptations and board providers remain subsequent port work. No
physical round display, target firmware or complete round UI is qualified here.

## Regression evidence

The new production-viewport fixture fails on PR15 baseline
`071d5012449b20ed2493425a59ec017985642ad7` at the expected safe-rectangle assertion.
The repaired fixture covers conservative round content, asymmetric rectangular
insets, empty/overflow rejection and physical edge-touch preservation. Existing
full-surface pixel/stride, lease ownership, rejection/retry, teardown and touch
checks remain active across eight rectangular/square dimensions. Existing CI runs
this fixture normally and with ASan/UBSan, plus shared layout and full application
qualification. Local sanitizer scripts disable LeakSanitizer; no LSan pass is claimed.

Reproduce with:

```sh
python scripts/test_viewport.py
python scripts/test_viewport.py --sanitize
python scripts/test_watch_layout.py
python -m unittest discover -s tests -v
```

Hardware reference for later providers: The-Badge
`76172dfaf0fa0ffedee91a61da3261afdde56ce3`, specifically README.md, main/display.c,
main/port_esp.c and main/idf_component.yml. Its 1.75C hardware is not the plain1.75
or existing2.06: CO5300/CST9217, AXP2101, 32MB flash/8MB PSRAM, no external RTC or
SD. The user's successful Badge firmware run is not RiscRTE qualification.
