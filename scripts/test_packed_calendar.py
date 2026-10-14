#!/usr/bin/env python3
"""Compare actual production decoder against every original atlas byte/range."""
from pathlib import Path
import re,subprocess,tempfile,os,sys,time,argparse
parser=argparse.ArgumentParser();parser.add_argument('--mode',choices=['normal','sanitized','both'],default='both');args=parser.parse_args()
root=Path(__file__).resolve().parents[1];faces=root/'apps/clock/faces'
source=(faces/'calendar_assets.inc').read_text();raw=source[source.index('static const uint8_t calendar_alpha[]'):];raw=raw[:raw.index('};')+2].replace('calendar_alpha[]','original[]')
pre='#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n#include <assert.h>\n#include <stdio.h>\ntypedef struct { uint8_t ch; int8_t left,top; uint8_t width,height; uint16_t advance; uint32_t data; } face_glyph;\ntypedef struct { const face_glyph *glyphs; unsigned count; } face_font;\n'
program=pre+raw+'\n#include "calendar_packed.inc"\n#include "packed_calendar.inc"\n'+r'''
int main(void){
 (void)calendar_alpha;(void)calendar_alpha_glyph;
 uint8_t result[CALENDAR_ALPHA_GLYPH_MAX];
 for(unsigned start=0;start<sizeof(original);start+=257){
  unsigned n=sizeof(original)-start;if(n>4096)n=4096;
  assert(calendar_alpha_read(start,n,result));assert(!memcmp(result,original+start,n));
 }
 for(unsigned start=0;start<sizeof(original);start+=997){
  unsigned n=sizeof(original)-start;if(n>sizeof(result))n=sizeof(result);
  assert(calendar_alpha_read(start,n,result));assert(!memcmp(result,original+start,n));
 }
 assert(!calendar_alpha_read(sizeof(original),1,result));assert(!calendar_alpha_read(UINT32_MAX,1,result));assert(!calendar_alpha_read(0,1,NULL));
 puts("Packed alpha: every original byte in overlapping block-crossing ranges exact; glyph-size spans and invalid bounds PASS");
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.c').write_text(program)
 for sanitized in ((False,True) if args.mode=="both" else (args.mode=="sanitized",)):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform!='darwin' else [])] if sanitized else []
  subprocess.run(['cc','-std=c11','-O1','-g',*flags,'-I'+str(faces),str(p/'test.c'),'-o',str(p/'test')],check=True)
  started=time.monotonic()
  subprocess.run([str(p/'test')],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

  print("sanitized="+str(sanitized)+" elapsed_seconds="+str(round(time.monotonic()-started,3)),flush=True)
