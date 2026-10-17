#!/usr/bin/env python3
"""Qualify the production symmetry decoder against every original ring tuple."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FACES = ROOT / 'apps/clock/faces'
original = (FACES / 'calendar_assets.inc').read_text()
packed = (FACES / 'calendar_packed.inc').read_text()
pattern = r'static const fc_ring_pixel (fc_ring_\d+)\[\]=\{.*?\};'
def arrays(text, prefix):
    values = re.findall(pattern, text, re.S)
    assert values == ['fc_ring_96', 'fc_ring_98']
    return '\n'.join(match[0].replace(match[1], prefix + match[1].removeprefix('fc_ring_')) for match in
                     re.finditer(pattern, text, re.S))

preamble = '''#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef struct {uint16_t pixel,angle;uint8_t alpha;} fc_ring_pixel;
#define COUNT(x) (sizeof(x)/sizeof(*(x)))
'''
program = preamble + arrays(original, 'original_') + arrays(packed, 'compact_') + r'''
#include "calendar_ring_decode.inc"
static fc_ring_pixel restored[240u*240u];
static uint8_t seen[240u*240u];
static unsigned verify(const fc_ring_pixel *source,unsigned count,const fc_ring_pixel *octants,unsigned compact) {
    memset(seen,0,sizeof(seen));memset(restored,0,sizeof(restored));unsigned expanded=0;
    for(unsigned i=0;i<compact;i++)for(unsigned transform=0;transform<8u;transform++) {
        fc_ring_pixel row;
        if(!fc_ring_expand(&octants[i],transform,&row))continue;
        assert(row.pixel<240u*240u && !seen[row.pixel]);
        seen[row.pixel]=1;restored[row.pixel]=row;++expanded;
    }
    assert(expanded==count);
    for(unsigned i=0;i<count;i++) {
        const fc_ring_pixel *expected=&source[i],*actual=&restored[expected->pixel];
        assert(seen[expected->pixel]);assert(actual->pixel==expected->pixel);
        assert(actual->angle==expected->angle && actual->alpha==expected->alpha);
    }
    return expanded;
}
int main(void) {
    unsigned count=verify(original_96,COUNT(original_96),compact_96,COUNT(compact_96));
    count+=verify(original_98,COUNT(original_98),compact_98,COUNT(compact_98));
    assert(count==11140);assert(COUNT(compact_96)+COUNT(compact_98)==1399);
    fc_ring_pixel output,row={120u*240u+120u,17280u,255u};
    assert(fc_ring_expand(&row,0,&output));assert(!fc_ring_expand(&row,1,&output));
    assert(!fc_ring_expand(NULL,0,&output));assert(!fc_ring_expand(&row,0,NULL));
    assert(!fc_ring_expand(&row,8,&output));
    row.pixel=0;assert(!fc_ring_expand(&row,0,&output));
    row.pixel=120u*240u+121u;row.angle=17281;assert(!fc_ring_expand(&row,0,&output));
    row.angle=11519;assert(!fc_ring_expand(&row,0,&output));
    puts("Calendar octants: all 11,140 pixel/angle/alpha tuples exact, unique output, diagonal and invalid bounds PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='calendar-octants-') as directory:
    root = Path(directory); source = root / 'test.c'; source.write_text(program)
    for sanitized in (False, True):
        flags = ['-fsanitize=address,undefined','-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer','-no-pie'] if sanitized else []
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fmax-errors=3',
                        *flags,'-I'+str(FACES),str(source),'-o',str(root/'test')],check=True)
        subprocess.run([str(root/'test')],check=True,timeout=30,
                       env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Production decoder SHA-256',hashlib.sha256((FACES/'calendar_ring_decode.inc').read_bytes()).hexdigest())
