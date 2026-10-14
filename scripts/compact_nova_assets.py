"""Lossless PackBits encoding of existing OFL font and dot alpha bytes."""
from pathlib import Path
import re,hashlib,json
root=Path(__file__).resolve().parents[1];source=root/'apps/clock/nova/assets.inc';text=source.read_text();out=['/* Generated losslessly by scripts/compact_nova_assets.py. */'];proof={}
for name in ('nova_glyph_alpha','nova_dot_alpha','points_alpha'):
 text=(root/'apps/clock/faces/points_text_assets.inc').read_text() if name=='points_alpha' else source.read_text()
 m=re.search(r'static const uint8_t '+name+r'\[\]\s*=\s*\{(.*?)\};',text,re.S)
 raw=bytes(int(x,0) for x in re.findall(r'0x[0-9a-fA-F]+|\d+',m[1]));encoded=[];i=0
 while i<len(raw):
  n=1
  while i+n<len(raw) and raw[i+n]==raw[i] and n<128:n+=1
  if n>=3:encoded.extend((127+n,raw[i]));i+=n
  else:
   start=i;i+=1
   while i<len(raw) and i-start<128 and not(i+2<len(raw) and raw[i]==raw[i+1]==raw[i+2]):i+=1
   encoded.extend((i-start-1,*raw[start:i]))
 out.append(f'static uint8_t {name}[{len(raw)}];\nstatic const uint8_t {name}_packed[]={{')
 out.extend(' '+','.join(map(str,encoded[i:i+32]))+',' for i in range(0,len(encoded),32));out.append('};')
 proof[name]={'bytes':len(raw),'packed_bytes':len(encoded),'sha256':hashlib.sha256(raw).hexdigest()}
(root/'apps/clock/nova/compact_assets.inc').write_text('\n'.join(out)+'\n');(root/'apps/clock/nova/compact_assets.json').write_text(json.dumps(proof,indent=2)+'\n')
