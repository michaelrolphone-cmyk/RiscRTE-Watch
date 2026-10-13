# Width-based layout with useful extra height

The owner accepts existing width-based sizing and requests additional vertical
space for content, rather than stretching a square to fit a taller screen.
`WatchLayout.h` supplies the shared geometry foundation. The geometry foundation is now used by the active Home display/touch adapter.
Home reflow and host qualification are recorded in `HOME_NATIVE_HEIGHT.md`; this
is not a finished device build.

The native provider keeps its real dimensions. App-side logical width is 240;
logical height is `ceil(native_height * 240 / native_width)`. Both coordinate
axes use the same width scale. Thus 240x240 stays exactly 240x240, while 410x502
offers 240x294 logical units. Typography, circles and control sizes keep their
proportions. Top and bottom anchors separate; list content gains 54 units rather
than magnifying its rows. A sample 28-unit row viewport grows from five rows to
seven without changing row height.

For a physical pixel `(x,y)`, inverse layout coordinates are
`(floor(x*240/native_width), floor(y*240/native_width))`. A half-open logical
rectangle maps with **ceil at both native boundaries**, clipped at the physical
edge. The final partial row is therefore rendered and hit consistently: native
row 501 on a 410x502 display maps to logical row 293. There are no forced
gesture gutters or contacts clamped to the old square's edges.

Safe insets are rounded inward; drawing and input can share the resulting
rectangles. Invalid or unbounded geometry fails without modifying output values.
The foundation allocates no memory and does no provider or hardware operation.

## Qualification

`python scripts/test_watch_layout.py` runs normal and ASan/UBSan checks. Each mode
verifies 5,033,052 physical pixel mappings over square, portrait, landscape,
fractional-scale and maximum-bound cases. Tests cover exact 240 identity, every
last row/column, no overlap/dead strips, inverse hit agreement, top/bottom anchors,
insets, invalid geometry and integer boundary cases. Leak detection is disabled;
address and undefined-behavior checks remain enabled.

## Integration sequence

1. Preserve and qualify the missing ordered raster-input and RTC recovery fixes.
2. Use runtime logical height in the viewport's display/touch facade, preserving
   the native provider and its truthful timing metadata.
3. Reflow original watch Springboard, Settings, Quick Actions, modal controls and
   keyboard with common geometry. Do not substitute a generic app grid.
4. Reflow Home faces, picker and overlays. Keep circular geometry proportional;
   use taller layouts for headings, footers, calendars and schedule content.
5. Preserve all 240-pixel golden frames and input paths; qualify 410x502 render/hit
   agreement, source/ELF cohort, capacity and actual device presentation.

Each app is migrated and qualified separately. Existing square compatibility is
an intermediate bring-up route, not completion of this layout work.
