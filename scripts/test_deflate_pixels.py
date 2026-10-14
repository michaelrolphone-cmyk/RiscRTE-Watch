#!/usr/bin/env python3
"""Compare all production faces against raw atlases; report host render time."""
from pathlib import Path
import argparse,subprocess,tempfile,time,hashlib
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);a=p.parse_args()
source=r'''
#include "nova.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>
static unsigned char pixels[240*240*2];
int main(void){
 risc_display_surface_v1 surface={0,pixels,240,240,480,sizeof(pixels),RISC_DISPLAY_FORMAT_RGB565};
 nova_watch_state state={.time={.year=2026,.month=10,.day=14,.weekday=3,.hour=17,.minute=42,.second=38},.time_valid=true,.battery_valid=true,.battery_percent=63,.subsecond_ms=300,.animation_ms=2500,.hour_24=true};
 double total=0,maximum=0;
 for(unsigned pass=0;pass<2;pass++){
  if(pass){state.time_valid=false;state.battery_valid=false;}
  for(unsigned face=0;face<WATCH_FACE_COUNT;face++){clock_t start=clock();assert(nova_watch_face_render(&surface,&state,face));double elapsed=(double)(clock()-start)/CLOCKS_PER_SEC;total+=elapsed;if(elapsed>maximum)maximum=elapsed;assert(fwrite(pixels,1,sizeof(pixels),stdout)==sizeof(pixels));}
 }
 fprintf(stderr,"render CPU total_seconds=%.6f max_frame_seconds=%.6f\n",total,maximum);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);(d/'test.c').write_text(source);results=[]
 for compressed in [False,True]:
  flags=['-DWATCH_COMPACT_NOVA_ASSETS','-DWATCH_COMPRESSED_CALENDAR_ALPHA','-DWATCH_COMPRESSED_FACE_ALPHA'] if compressed else []
  subprocess.run(['cc','-std=c11','-O2',*flags,*['-I'+str(x) for x in [a.sdk,root/'include',root/'apps/clock/nova']],str(d/'test.c'),str(root/'apps/clock/nova/nova.c'),'-o',str(d/'test')],check=True)
  started=time.monotonic();result=subprocess.check_output([str(d/'test')],timeout=30);elapsed=time.monotonic()-started;results.append(result)
  print('compressed=',compressed,'seconds=',round(elapsed,4),'bytes=',len(result),'sha256=',hashlib.sha256(result).hexdigest())
 assert results[0]==results[1]
print('Every production face: valid/unknown RTC and battery pixels exactly match raw atlases PASS; host durations are not device timings')
