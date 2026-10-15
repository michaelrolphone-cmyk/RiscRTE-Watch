# Lossless face atlas storage

The selected shared Home build enables WATCH_COMPRESSED_FACE_ALPHA. Only the original face alpha atlas is encoded; glyph metadata, fonts, layout, colors and features remain unchanged. The raw source stays in assets_text.inc. generate_packed_alpha.py deterministically emits independent 256-byte RLE/LZ blocks and their offsets. Each block uses literals, repeated bytes and block-local backward copies, with minimum-byte parsing. No external/compressed ELF loader is involved.

The renderer decodes one glyph into a fixed 6,853-byte BSS scratch buffer; each block uses at most 256 stack bytes. There are no allocations, media calls, provider calls or callbacks during decoding. Home rendering is single-owner and nonrecursive. Boundary checks fail closed on impossible compiled atlas spans. The legacy presentation option retains its original raw atlas.

The packed face bytes plus index occupy 124,940 bytes for 262,358 original alpha bytes. The current codec saves another 29,225 bytes compared with the previous literal/run-only format, without changing the scratch buffers or decoded coverage.

Qualification: actual C decoder matches every original byte and every 257-byte boundary-crossing range, larger glyph-size ranges and rejects invalid bounds; normal and ASan/UBSan. All 362 preexisting 240-pixel golden frames remain exact. Native-height render/input and actual Home 240x240/410x502 startup, sleep, unavailable-media and fault regressions pass in both modes. Synthetic streams also cover malformed tokens, overlapping copies and block endpoints. This is not a hardware timing measurement; decoding adds bounded CPU work and 6,853 bytes of scratch memory.
