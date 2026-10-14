#!/usr/bin/env python3
"""Decode every NOVA/Points byte with production tinf and check truncated input."""
from pathlib import Path
import re,subprocess,tempfile,os
root=Path(__file__).resolve().parents[1];nova=root/'apps/clock/nova';parts=['#include <stdint.h>','#include <string.h>','#include <assert.h>', '#include "AssetInflate.h"', '#include "compact_assets.inc"'];calls=[]
for name in ('nova_glyph_alpha','nova_dot_alpha','points_alpha'):
 source=(root/'apps/clock/faces/points_text_assets.inc' if name=='points_alpha' else nova/'assets.inc').read_text()
 m=re.search(r'static const uint8_t '+name+r'\[\]\s*=\s*\{(.*?)\};',source,re.S)
 parts.append('static const uint8_t expected_'+name+'[]={'+m[1]+'};')
 calls.append('unsigned n_'+name+'=sizeof('+name+');assert(tinf_uncompress('+name+',&n_'+name+','+name+'_packed,sizeof('+name+'_packed))==TINF_OK);assert(n_'+name+'==sizeof(expected_'+name+'));assert(!memcmp('+name+',expected_'+name+',n_'+name+'));')
 calls.append('for(unsigned cut=0;cut<sizeof('+name+'_packed);cut+=113){unsigned n=sizeof('+name+');assert(tinf_uncompress('+name+',&n,'+name+'_packed,cut)!=TINF_OK);}')
parts.append('int main(void){'+''.join(calls)+'return 0;}')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text('\n'.join(parts))
 for flags in [[],['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']]:
  subprocess.run(['cc','-std=c11','-O1','-g',*flags,'-I'+str(nova),'-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Deflate assets: all NOVA/Points alpha bytes exact, truncated streams rejected; normal and ASan/UBSan PASS')
