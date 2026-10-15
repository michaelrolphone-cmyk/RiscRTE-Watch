#!/usr/bin/env python3
"""Qualify both production atlas decoders on generated and malformed blocks."""
from pathlib import Path
import importlib.util
import itertools
import os
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FACES = ROOT / 'apps/clock/faces'


def read_encoder(filename):
    spec = importlib.util.spec_from_file_location(filename, FACES / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.encode


def reference_decode(packed):
    result = bytearray()
    position = 0
    while position < len(packed):
        tag = packed[position]
        position += 1
        if tag < 128:
            length = tag + 1
            result.extend(packed[position:position + length])
            position += length
        else:
            length = (tag & 63) + 3
            value = packed[position]
            position += 1
            if tag < 192:
                result.extend([value] * length)
            else:
                assert 0 < value <= len(result)
                for _ in range(length):
                    result.append(result[-value])
    assert position == len(packed)
    return bytes(result)


encoders = [read_encoder('generate_packed_alpha.py'), read_encoder('generate_packed_calendar.py')]
short_cases = [bytes(values) for length in range(9)
               for values in itertools.product(range(2), repeat=length)]
rng = random.Random(0xA71A5)
cases = []
for length in (1, 2, 3, 4, 65, 66, 67, 127, 128, 129, 253, 254, 255, 256):
    cases.extend([bytes(length), bytes([255]) * length, bytes(range(length)),
                  (b'abc' * 86)[:length], (bytes(range(17)) * 16)[:length]])
for _ in range(64):
    length = rng.randrange(1, 257)
    alphabet = rng.choice((2, 4, 16, 256))
    cases.append(bytes(rng.randrange(alphabet) for _ in range(length)))
for raw in short_cases + cases:
    streams = [encoder(raw) for encoder in encoders]
    assert streams[0] == streams[1], 'Atlas encoders diverged'
    assert reference_decode(streams[0]) == raw
print(f'Both encoders: {len(short_cases) + len(cases)} deterministic lossless synthetic blocks PASS', flush=True)

PREAMBLE = r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static unsigned fixture_bytes;
static uint32_t fixture_blocks[2];
static uint8_t fixture_packed[512];
#define UPPER_ALPHA_BYTES fixture_bytes
#define UPPER_ALPHA_GLYPH_MAX 256
#define PREFIX_alpha_blocks fixture_blocks
#define PREFIX_alpha_packed fixture_packed
#define PREFIX_alpha_read fixture_read
#define PREFIX_alpha_glyph fixture_glyph
#include "DECODER"
static void load(unsigned bytes,const uint8_t *packed,unsigned length) {
    assert(length<=sizeof(fixture_packed));
    fixture_bytes=bytes;fixture_blocks[0]=0;fixture_blocks[1]=length;
    memset(fixture_packed,0,sizeof(fixture_packed));
    if(length)memcpy(fixture_packed,packed,length);
}
static void reject(unsigned bytes,const uint8_t *packed,unsigned length) {
    uint8_t result[256];load(bytes,packed,length);
    assert(!fixture_read(0,bytes,result));
}
int main(void) {
    (void)fixture_glyph;
    uint8_t result[256];
'''
MALFORMED = r'''
    /* Literal, run, and backward-copy endpoint errors. */
    reject(1,NULL,0);
    reject(1,(const uint8_t[]){0},1);
    reject(2,(const uint8_t[]){1,7},2);
    reject(3,(const uint8_t[]){128},1);
    reject(3,(const uint8_t[]){192},1);
    reject(2,(const uint8_t[]){128,7},2);
    reject(2,(const uint8_t[]){0,7},2);
    reject(3,(const uint8_t[]){192,0},2);
    reject(3,(const uint8_t[]){192,1},2);
    reject(4,(const uint8_t[]){0,7,192,0},4);
    reject(4,(const uint8_t[]){0,7,192,2},4);
    reject(3,(const uint8_t[]){0,7,192,1},4);
    /* A backreference's source becomes its output when ranges overlap. */
    load(67,(const uint8_t[]){0,7,255,1},4);
    assert(fixture_read(0,67,result));
    for(unsigned i=0;i<67;i++)assert(result[i]==7);
    load(68,(const uint8_t[]){1,7,9,255,2},5);
    assert(fixture_read(0,68,result));
    for(unsigned i=0;i<68;i++)assert(result[i]==(i%2?9:7));
    /* Malformed block index, invalid request, and zero-length request. */
    fixture_blocks[0]=6;assert(!fixture_read(0,1,result));
    fixture_blocks[0]=0;fixture_blocks[1]=513;assert(!fixture_read(0,1,result));
    assert(!fixture_read(UINT32_MAX,1,result));
    assert(!fixture_read(0,UINT32_MAX,result));
    assert(!fixture_read(0,0,NULL));
    assert(fixture_read(fixture_bytes,0,result));
    puts("Production atlas decoder: token boundaries, overlap, malformed streams, and bounds PASS");
}
'''

with tempfile.TemporaryDirectory(prefix='atlas-codec-') as temporary:
    path = Path(temporary)
    for prefix, decoder in (('face', 'packed_alpha.inc'), ('calendar', 'packed_calendar.inc')):
        program = PREAMBLE.replace('UPPER', prefix.upper()).replace('PREFIX', prefix).replace('DECODER', decoder)
        for raw in cases:
            packed = encoders[0](raw)
            literal = lambda data: ','.join(map(str, data))
            program += '{ const uint8_t raw[]={' + literal(raw) + '};\n'
            program += 'const uint8_t packed[]={' + literal(packed) + '};\n'
            program += 'load(sizeof(raw),packed,sizeof(packed));\n'
            program += 'for(unsigned i=0;i<sizeof(raw);i++) {\n'
            program += 'assert(fixture_read(i,sizeof(raw)-i,result));\n'
            program += 'assert(!memcmp(result,raw+i,sizeof(raw)-i)); } }\n'
        program += MALFORMED
        source = path / f'{prefix}.c'
        source.write_text(program)
        for sanitized in (False, True):
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                     '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            binary = path / prefix
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                            '-Wall', '-Wextra', '-Werror', *flags, '-I' + str(FACES),
                            str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=60,
                           env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
