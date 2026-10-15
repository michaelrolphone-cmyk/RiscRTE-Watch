#!/usr/bin/env python3
"""Compare actual production decoder against every original atlas byte/range."""
from pathlib import Path
import re,subprocess,tempfile,os
root=Path(__file__).resolve().parents[1];faces=root/'apps/clock/faces'
source=(faces/'assets_text.inc').read_text();raw=source[source.index('static const uint8_t face_alpha[]'):];raw=raw[:raw.index('};')+2].replace('face_alpha[]','original[]')
glyphs=[list(map(int,row.split(','))) for row in re.findall(r'\{(-?\d+(?:\s*,\s*-?\d+){6})\}',source[:source.index('static const uint8_t face_alpha[]')])]
glyph_ranges='static const uint32_t glyph_ranges[][2]={' + ','.join('{%d,%d}' % (g[6],g[3]*g[4]) for g in glyphs) + '};\n'

pre='#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n#include <assert.h>\n#include <stdio.h>\ntypedef struct { uint8_t ch; int8_t left,top; uint8_t width,height; uint16_t advance; uint32_t data; } face_glyph;\ntypedef struct { const face_glyph *glyphs; unsigned count; } face_font;\n'
program=pre+raw+glyph_ranges+'\n#include "assets_packed.inc"\n#include "packed_alpha.inc"\n'+r'''
int main(void){
 (void)face_alpha;(void)face_alpha_glyph;
 uint8_t result[FACE_ALPHA_GLYPH_MAX];
 for(unsigned start=0;start<sizeof(original);start++){
  unsigned n=sizeof(original)-start;if(n>257)n=257;
  assert(face_alpha_read(start,n,result));assert(!memcmp(result,original+start,n));
 }
 for(unsigned start=0;start<sizeof(original);start+=997){
  unsigned n=sizeof(original)-start;if(n>sizeof(result))n=sizeof(result);
  assert(face_alpha_read(start,n,result));assert(!memcmp(result,original+start,n));
 }
 for(unsigned i=0;i<sizeof(glyph_ranges)/sizeof(*glyph_ranges);i++){
  unsigned start=glyph_ranges[i][0],n=glyph_ranges[i][1];
  assert(n<=sizeof(result));assert(face_alpha_read(start,n,result));
  assert(!memcmp(result,original+start,n));
 }
 assert(face_alpha_read(sizeof(original),0,result));
 assert(!face_alpha_read(0,UINT32_MAX,result));
 assert(!face_alpha_read(sizeof(original),1,result));assert(!face_alpha_read(UINT32_MAX,1,result));assert(!face_alpha_read(0,1,NULL));
 puts("Packed alpha: every original byte and 257-byte boundary-crossing range exact; every glyph, zero-length reads and invalid bounds PASS");
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.c').write_text(program)
 for sanitized in (False,True):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  subprocess.run(['cc','-std=c11','-O1','-g',*flags,'-I'+str(faces),str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
