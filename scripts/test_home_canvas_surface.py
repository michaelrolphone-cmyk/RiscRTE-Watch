#!/usr/bin/env python3
"""Compile the Home raster adapter against the exact owning Canvas provider."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False);cases=[]
with tempfile.TemporaryDirectory() as temp:
 for sanitized in (False,True):
  binary=pathlib.Path(temp)/('test-san' if sanitized else 'test')
  cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror']
  for inc in (ROOT/'include',a.system/'sdk/app',a.system/'Services/canvas_host',a.system/'lib/PortableApps/include'):
   cmd+=['-I'+str(inc)]
  cmd+=[str(ROOT/'tests/test_home_canvas_surface.c'),'-o',str(binary)]
  if sanitized:cmd+=['-fsanitize=address,undefined']
  subprocess.run(cmd,check=True)
  result=subprocess.run([str(binary)],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
  print(result.stdout);cases.append({'sanitized':sanitized,'command':cmd,'stdout':result.stdout,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
sources=[ROOT/'include/HomeCanvasSurface.h',ROOT/'tests/test_home_canvas_surface.c',pathlib.Path(__file__),a.system/'sdk/app/RiscCanvasV1.h']
sources+=list((a.system/'Services/canvas_host').glob('*.h'))
(a.output/'results.json').write_text(json.dumps({'cases':cases,'sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},'scope':'Actual Canvas provider with controlled physical capabilities; no hardware acceptance.'},indent=2)+'\n')
