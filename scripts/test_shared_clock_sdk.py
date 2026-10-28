#!/usr/bin/env python3
"""Verify canonical helper provenance and actual shared Watch SDK staging."""
import argparse,hashlib,json,tempfile,shutil
from pathlib import Path
from merged_sdk import headers
p=argparse.ArgumentParser()
for n in ('runtime','system','utilities'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
h=root/'sdk/app/RiscRuntimeClockV1.h';proof=json.loads((root/'sdk/app/RiscRuntimeClockV1.provenance.json').read_text())
assert h.read_bytes()==(a.runtime/proof['path']).read_bytes()
assert hashlib.sha256(h.read_bytes()).hexdigest()==proof['sha256']
with tempfile.TemporaryDirectory(prefix='watch-shared-clock-sdk-') as d:
 d=Path(d);out=headers(d/'staged',a.system,a.utilities,root)
 assert (out/h.name).read_bytes()==h.read_bytes()
 # The equality guard fails before staging a modified local variant.
 fake=d/'home';(fake/'sdk/app').mkdir(parents=True)
 shutil.copyfile(h,fake/'sdk/app'/h.name);shutil.copyfile(root/'sdk/app/RiscRuntimeClockV1.provenance.json',fake/'sdk/app/RiscRuntimeClockV1.provenance.json')
 (fake/'sdk/app'/h.name).write_bytes(h.read_bytes()+b'\n/* altered */\n')
 try:headers(d/'rejected',a.system,a.utilities,fake)
 except ValueError:pass
 else:raise AssertionError('mutated helper accepted')
print('PASS canonical Runtime helper equality, actual merged SDK selection and local-variant rejection')
