#!/usr/bin/env python3
"""Actual C recurrence vs independent Python big-integer signed division."""
import argparse,json,random,subprocess,tempfile,pathlib
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1];rng=random.Random(0x466)
def trunc(n,d):return n//d if n>=0 else -((-n)//d)
cases=[]
for d in [1,2,3,7,127,255,256,257,511,65535,65536,65537,2147483647]+[rng.randrange(1,2**31) for _ in range(500)]:
 for n in [-d-1,-d,-d+1,-1,0,1,d-1,d,d+1,rng.randrange(-2**39,2**39)]:
  for count in (1,32,2048):cases.append((n,d,count,trunc(n+(count-1)*65536,d)))
with tempfile.TemporaryDirectory(prefix='alpha-coordinate-') as t:
 t=pathlib.Path(t);src=t/'check.c';binary=t/'check'
 src.write_text('#include "native_alpha_coordinate.h"\n#include <assert.h>\n#include <stdio.h>\nint main(void){long long n,e;int d;unsigned count;while(scanf("%lld %d %u %lld",&n,&d,&count,&e)==4){native_alpha_axis a=native_alpha_axis_begin(n,d);long long value=0;for(unsigned i=0;i<count;i++){value=native_alpha_axis_next(&a);assert(value==(n+(long long)i*65536)/d);}assert(value==e);}puts("PASS native alpha exact signed coordinate recurrence");}\n')
 cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'apps/clock/nova'),str(src),'-o',str(binary)]
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 subprocess.run(cmd,check=True);subprocess.run([str(binary)],input=''.join('%d %d %d %d\n'%x for x in cases),text=True,check=True)
a.output.write_text(json.dumps({'passed':True,'cases':len(cases),'coordinates':sum(c[2] for c in cases),'sanitized':a.sanitize,'oracle':'Python big integers independent of C recurrence; C signed division crosschecks every coordinate','not_pixel_or_target_qualification':True},indent=2)+'\n')
