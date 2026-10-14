#!/usr/bin/env python3
"""Losslessly encode the unchanged face atlas into independent 256-byte blocks."""
from pathlib import Path
import re,hashlib,json
root=Path(__file__).resolve().parent
source=(root/'calendar_assets.inc').read_text()
m=re.search(r'static const uint8_t calendar_alpha\[\]=\{(.*?)\};',source,re.S)
raw=bytes(map(int,re.findall(r'\d+',m[1])))
def encode(b):
 out=bytearray();lit=bytearray();i=0
 def flush():
  if lit:out.append(len(lit)-1);out.extend(lit);lit.clear()
 while i<len(b):
  j=i+1
  while j<len(b) and b[j]==b[i] and j-i<128:j+=1
  if j-i>=3:flush();out.extend([128+j-i-1,b[i]]);i=j
  else:
   lit.append(b[i]);i+=1
   if len(lit)==128:flush()
 flush();return out
packed=bytearray();offsets=[0]
for start in range(0,len(raw),256):packed.extend(encode(raw[start:start+256]));offsets.append(len(packed))
glyphs=[list(map(int,r.split(','))) for r in re.findall(r'\{(-?\d+(?:\s*,\s*-?\d+){6})\}',source[:m.start()])]
assert glyphs
maximum=max(g[3]*g[4] for g in glyphs)
def array(name,kind,data):return 'static const '+kind+' '+name+'[]={\n'+',\n'.join(','.join(map(str,data[i:i+32])) for i in range(0,len(data),32))+'\n};\n'
replacement='static const uint8_t calendar_alpha[]={0}; /* Identity marker; never indexed. */\n'
replacement+=f'#define CALENDAR_ALPHA_BYTES {len(raw)}u\n#define CALENDAR_ALPHA_GLYPH_MAX {maximum}u\n'
replacement+=array('calendar_alpha_blocks','uint32_t',offsets)+array('calendar_alpha_packed','uint8_t',packed)
(root/'calendar_packed.inc').write_text(source[:m.start()]+replacement+source[m.end():])
(root/'packed-calendar.json').write_text(json.dumps({'input':'calendar_assets.inc','input_sha256':hashlib.sha256(source.encode()).hexdigest(),'raw_alpha_sha256':hashlib.sha256(raw).hexdigest(),'raw_bytes':len(raw),'packed_bytes':len(packed),'index_bytes':len(offsets)*4,'glyph_scratch_bytes':maximum,'block_bytes':256,'lossless':True},indent=2)+'\n')
print(len(raw),len(packed)+len(offsets)*4,maximum)
