"""Lossless raw DEFLATE encoding of existing OFL font and dot alpha bytes."""
from pathlib import Path
import re,hashlib,json,zlib
root=Path(__file__).resolve().parents[1];source=root/'apps/clock/nova/assets.inc';text=source.read_text();out=['/* Generated losslessly by scripts/compact_nova_assets.py. */'];proof={}
for name in ('nova_glyph_alpha','nova_dot_alpha','points_alpha'):
 text=(root/'apps/clock/faces/points_text_assets.inc').read_text() if name=='points_alpha' else source.read_text()
 m=re.search(r'static const uint8_t '+name+r'\[\]\s*=\s*\{(.*?)\};',text,re.S)
 raw=bytes(int(x,0) for x in re.findall(r'0x[0-9a-fA-F]+|\d+',m[1]));compressor=zlib.compressobj(9,zlib.DEFLATED,-15);encoded=compressor.compress(raw)+compressor.flush()
 out.append(f'static uint8_t {name}[{len(raw)}];\nstatic const uint8_t {name}_packed[]={{')
 out.extend(' '+','.join(map(str,encoded[i:i+32]))+',' for i in range(0,len(encoded),32));out.append('};')
 proof[name]={'bytes':len(raw),'packed_bytes':len(encoded),'sha256':hashlib.sha256(raw).hexdigest()}
(root/'apps/clock/nova/compact_assets.inc').write_text('\n'.join(out)+'\n');(root/'apps/clock/nova/compact_assets.json').write_text(json.dumps(proof,indent=2)+'\n')
