# Combined UI fixed-store build

The 07f3972 UI checkpoint and its System Apps launcher exceeded the T-Watch fixed store when combined. Calendar text alpha is losslessly encoded in independent 256-byte blocks using the bounded face-atlas codec. All 239,208 source alpha bytes and glyph metadata remain unchanged. The current RLE/LZ packed bytes plus index occupy 93,162 bytes, compared with 120,367 in the previous literal/run-only format. The optional build flag WATCH_COMPRESSED_CALENDAR_ALPHA selects a bounded 9,858-byte glyph scratch buffer; no heap or provider I/O is introduced. Original uncompressed builds remain supported.

Qualification: test_packed_calendar.py compares every byte and crossing range plus invalid bounds under normal and ASan/UBSan execution. test_home_native_height.py verifies the existing 362-frame square golden corpus and 410x502 geometry. test_fast_wake_clock.py exercises actual cold-start, deep-wake, later return, alarm and Contexts behavior in both dimensions.
