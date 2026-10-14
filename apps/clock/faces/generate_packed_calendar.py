#!/usr/bin/env python3
"""Losslessly encode the unchanged face atlas into independent 4096-byte blocks."""
from pathlib import Path
import re,hashlib,json,zlib
root=Path(__file__).resolve().parent
source=(root/'calendar_assets.inc').read_text()
m=re.search(r'static const uint8_t calendar_alpha\[\]=\{(.*?)\};',source,re.S)
raw=bytes(map(int,re.findall(r'\d+',m[1])))
def encode(b):
 compressor=zlib.compressobj(9,zlib.DEFLATED,-15)
 return compressor.compress(b)+compressor.flush()
packed=bytearray();offsets=[0]
for start in range(0,len(raw),4096):packed.extend(encode(raw[start:start+4096]));offsets.append(len(packed))
glyphs=[list(map(int,r.split(','))) for r in re.findall(r'\{(-?\d+(?:\s*,\s*-?\d+){6})\}',source[:m.start()])]
assert glyphs
maximum=max(g[3]*g[4] for g in glyphs)
def array(name,kind,data):return 'static const '+kind+' '+name+'[]={\n'+',\n'.join(','.join(map(str,data[i:i+32])) for i in range(0,len(data),32))+'\n};\n'
replacement='static const uint8_t calendar_alpha[]={0}; /* Identity marker; never indexed. */\n'
replacement+=f'#define CALENDAR_ALPHA_BYTES {len(raw)}u\n#define CALENDAR_ALPHA_GLYPH_MAX {maximum}u\n'
replacement+=array('calendar_alpha_blocks','uint32_t',offsets)+array('calendar_alpha_packed','uint8_t',packed)
(root/'calendar_packed.inc').write_text(source[:m.start()]+replacement+source[m.end():])
(root/'packed-calendar.json').write_text(json.dumps({'input':'calendar_assets.inc','input_sha256':hashlib.sha256(source.encode()).hexdigest(),'raw_alpha_sha256':hashlib.sha256(raw).hexdigest(),'raw_bytes':len(raw),'packed_bytes':len(packed),'index_bytes':len(offsets)*4,'glyph_scratch_bytes':maximum,'block_bytes':4096,'lossless':True,'encoding':'raw-deflate-tinf1.2.1'},indent=2)+'\n')
print(len(raw),len(packed)+len(offsets)*4,maximum)
