#!/usr/bin/env python3
"""Build the shared, hardware-neutral Bringup application; no device access."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from build_inputs import verify_dependency,file_manifest
from merged_sdk import headers
from compact_current_elf import compact
from normalize_xtensa_relocations import normalize
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('system','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--cc',required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
lock=json.loads((ROOT/'dependencies.lock.json').read_text())
for n,paths in [('system',['lib/PortableApps','lib/elf_loader','test/native_apps']),('utilities',['Apps/alarm_writer.h','lib/Alarm/include','lib/Contexts/include'])]:verify_dependency(getattr(a,n),lock[n]['commit'],paths)
compiler=subprocess.check_output([a.cc,'--version'],text=True)
if '15.2.0' not in compiler or 'esp-15.2.0_20251204' not in compiler:raise ValueError('Exact shared GCC15 cohort required')
inputs=file_manifest(ROOT,['apps/bringup','apps/clock/viewport.c','include','sdk','scripts/build_bringup.py'])
inc=headers(out,a.system,a.utilities,ROOT);mapping=out/'exports.map';mapping.write_text('{ global: app_main; local: *; };\n')
elf=out/'bringup.elf'
subprocess.run([a.cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(ROOT/'include'),'-I'+str(inc),str(ROOT/'apps/bringup/main.c'),str(ROOT/'apps/clock/viewport.c'),'-lgcc','-o',str(elf)],check=True)
normalize(elf);proof=compact(elf,a.cc,debug_path=out/'debug/bringup.elf')
manifest={'type':'application','id':'watch-bringup','display_name':'Hardware Test','version':'0.1.1','architecture':'xtensa-esp32s3','file_name':'bringup.elf','entry':'app_main','requires':[{'capability':c,'api':1} for c in ['display.output','input.touch.raw']],'optional':[{'capability':c,'api':v} for c,v in [('board.battery',1),('rtc.clock',2),('motion.accel',1),('sound.haptic',1)]]}
if inputs!=file_manifest(ROOT,['apps/bringup','apps/clock/viewport.c','include','sdk','scripts/build_bringup.py']):raise ValueError('Build input changed')
(out/'bringup.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'build.json').write_text(json.dumps({'source_sha256':inputs,'compiler':compiler.splitlines()[0],'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'compaction':proof,'hardware_tested':False},indent=2)+'\n')
