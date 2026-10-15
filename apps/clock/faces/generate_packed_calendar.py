#!/usr/bin/env python3
"""Lossless RLE/LZ atlas with independent 256-byte blocks and optimal parsing.

Tags 0..127: 1..128 literal bytes. Tags 128..191: repeat one byte 3..66
 times. Tags 192..255: copy 3..66 bytes from a nonzero one-byte backward
 distance in the current block. Copies may overlap, so no history buffer is
 required beyond the decoder's existing 256-byte block scratch.
"""
from pathlib import Path
import hashlib
import json
import re

BLOCK_BYTES = 256
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
    replacement += f"#define CALENDAR_ALPHA_BYTES {len(raw)}u\n#define CALENDAR_ALPHA_GLYPH_MAX {maximum}u\n"
    replacement += array("calendar_alpha_blocks", "uint32_t", offsets)
    replacement += array("calendar_alpha_packed", "uint8_t", packed)
    (root / "calendar_packed.inc").write_text(source[:match.start()] + replacement + source[match.end():])
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
    }
    (root / "packed-calendar.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(len(raw), len(packed) + len(offsets) * 4, maximum)


if __name__ == "__main__":
    main()
