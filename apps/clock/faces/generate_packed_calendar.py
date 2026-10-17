#!/usr/bin/env python3
"""Lossless RLE/LZ atlas with independent 512-byte blocks and optimal parsing.

Tags 0..127: 1..128 literal bytes. Tags 128..191: repeat one byte 3..66
 times. Tags 192..255: copy 3..66 bytes from a nonzero one-byte backward
 distance in the current block. Copies may overlap, so no history buffer is
 required beyond the decoder's existing 512-byte block scratch.
"""
from pathlib import Path
import hashlib
import json
import re

BLOCK_BYTES = 512
MATCH_MAX = 66


def encode(block):
    """Choose a minimum-byte tokenization of this block, deterministically."""
    size = len(block)
    matches = []
    seen = {}
    for start in range(size):
        key = block[start:start + 3]
        longest, distance = 0, 0
        if len(key) == 3:
            for previous in reversed(seen.get(key, [])):
                # Token distance remains one byte even in a larger block.
                if start - previous > 255:
                    break
                length = 3
                while (length < MATCH_MAX and start + length < size
                       and block[previous + length] == block[start + length]):
                    length += 1
                if length > longest:
                    longest, distance = length, start - previous
                    if longest == MATCH_MAX:
                        break
            seen.setdefault(key, []).append(start)
        matches.append((longest, distance))

    costs = [size * 2 + 1] * size + [0]
    choices = [None] * size
    for start in range(size - 1, -1, -1):
        for length in range(1, min(128, size - start) + 1):
            cost = length + 1 + costs[start + length]
            if cost < costs[start]:
                costs[start] = cost
                choices[start] = ("literal", length, 0)
        longest, distance = matches[start]
        for length in range(3, longest + 1):
            cost = 2 + costs[start + length]
            if cost < costs[start]:
                costs[start] = cost
                choices[start] = ("copy", length, distance)
        longest = 1
        while (longest < MATCH_MAX and start + longest < size
               and block[start + longest] == block[start]):
            longest += 1
        for length in range(3, longest + 1):
            cost = 2 + costs[start + length]
            if cost <= costs[start]:
                costs[start] = cost
                choices[start] = ("repeat", length, block[start])

    packed = bytearray()
    start = 0
    while start < size:
        kind, length, value = choices[start]
        if kind == "literal":
            packed.append(length - 1)
            packed.extend(block[start:start + length])
        else:
            packed.extend(((128 if kind == "repeat" else 192) + length - 3, value))
        start += length
    assert len(packed) == costs[0]
    return packed


def array(name, kind, data):
    rows = [",".join(map(str, data[i:i + 32])) for i in range(0, len(data), 32)]
    return "static const " + kind + " " + name + "[]={\n" + ",\n".join(rows) + "\n};\n"


def compact_rings(source):
    """Retain one octant only after proving exact integer reconstruction."""
    reports = {}
    def replace(match):
        name = match[1]
        rows = [tuple(map(int, values)) for values in
                re.findall(r"\{(\d+),(\d+),(\d+)\}", match[2])]
        original = {pixel: (angle, alpha) for pixel, angle, alpha in rows}
        assert len(original) == len(rows) and rows
        octant = [row for row in rows if row[0] % 240 >= row[0] // 240 >= 120]
        restored = {}
        for pixel, angle, alpha in octant:
            x, y = pixel % 240, pixel // 240
            for xx, yy, aa in ((x, y, angle), (y, x, (34560 - angle) % 46080)):
                for ox, oy, oa in ((xx, yy, aa), (239 - xx, yy, -aa % 46080),
                                  (xx, 239 - yy, (23040 - aa) % 46080),
                                  (239 - xx, 239 - yy, (aa + 23040) % 46080)):
                    key, value = oy * 240 + ox, (oa, alpha)
                    assert key not in restored or restored[key] == value
                    restored[key] = value
        assert restored == original, "Ring symmetry is not exact: " + name
        reports[name] = {"original_rows": len(rows), "octant_rows": len(octant),
                         "every_pixel_angle_alpha_exact": True}
        lines = [",".join("{%d,%d,%d}" % row for row in octant[i:i + 8])
                 for i in range(0, len(octant), 8)]
        return "static const fc_ring_pixel " + name + "[]={\n" + ",\n".join(lines) + "\n};"
    pattern = r"static const fc_ring_pixel (fc_ring_\d+)\[\]=\{(.*?)\};"
    packed, count = re.subn(pattern, replace, source, flags=re.S)
    assert count == 2 and set(reports) == {"fc_ring_96", "fc_ring_98"}
    return "#define WATCH_CALENDAR_RING_OCTANTS 1\n" + packed, reports


def main():
    root = Path(__file__).resolve().parent
    source = (root / "calendar_assets.inc").read_text()
    match = re.search(r"static const uint8_t calendar_alpha\[\]=\{(.*?)\};", source, re.S)
    assert match
    raw = bytes(map(int, re.findall(r"\d+", match[1])))
    packed = bytearray()
    offsets = [0]
    for start in range(0, len(raw), BLOCK_BYTES):
        packed.extend(encode(raw[start:start + BLOCK_BYTES]))
        offsets.append(len(packed))
    glyphs = [list(map(int, row.split(","))) for row in
              re.findall(r"\{(-?\d+(?:\s*,\s*-?\d+){6})\}", source[:match.start()])]
    assert glyphs
    maximum = max(glyph[3] * glyph[4] for glyph in glyphs)
    replacement = "static const uint8_t calendar_alpha[]={0}; /* Identity marker; never indexed. */\n"
    replacement += f"#define CALENDAR_ALPHA_BLOCK_BYTES {BLOCK_BYTES}u\n"
    replacement += f"#define CALENDAR_ALPHA_BYTES {len(raw)}u\n#define CALENDAR_ALPHA_GLYPH_MAX {maximum}u\n"
    replacement += array("calendar_alpha_blocks", "uint32_t", offsets)
    replacement += array("calendar_alpha_packed", "uint8_t", packed)
    tail, rings = compact_rings(source[match.end():])
    (root / "calendar_packed.inc").write_text(source[:match.start()] + replacement + tail)
    metadata = {
        "input": "calendar_assets.inc",
        "input_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "raw_alpha_sha256": hashlib.sha256(raw).hexdigest(),
        "raw_bytes": len(raw),
        "packed_bytes": len(packed),
        "index_bytes": len(offsets) * 4,
        "glyph_scratch_bytes": maximum,
        "block_bytes": BLOCK_BYTES,
        "codec": "block-rle-lz-v1",
        "lossless": True,
        "ring_octants": rings,
    }
    (root / "packed-calendar.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(len(raw), len(packed) + len(offsets) * 4, maximum)


if __name__ == "__main__":
    main()
