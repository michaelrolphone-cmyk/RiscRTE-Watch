#!/usr/bin/env python3
"""Build one hardware-neutral Bringup ELF including preserved native diagnostics."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--system',type=Path,required=True)
p.add_argument('--compiler',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
cc=str(a.compiler.resolve());S=a.system.resolve();out=a.output.resolve()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
compiler=subprocess.check_output([cc,'--version'],text=True)
assert '15.2.0' in compiler and 'esp-15.2.0_20251204' in compiler
exports={'app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'}
mapping=out/'exports.map';mapping.write_text('{global:'+(';'.join(sorted(exports)))+';local:*;};\n')
elf=out/'bringup.elf'
includes=[S/'sdk/usb',S/'sdk/app',ROOT/'include',ROOT/'sdk/app',ROOT/'sdk/driver']
cmd=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls',
 '-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared',
 '-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),
 '-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DBRINGUP_SHARED',
 *['-I'+str(x) for x in includes],str(ROOT/'apps/bringup/main.c'),str(ROOT/'apps/clock/viewport.c'),
 '-lgcc','-o',str(elf)]
subprocess.run(cmd,check=True)
# Retain dynamic loader symbols/relocations; discard only non-runtime symbol metadata.
subprocess.run([cc.removesuffix('gcc')+'strip','--strip-unneeded',str(elf)],check=True)
symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
imp={r.split()[-1] for r in symbols.splitlines() if ' U ' in ' '+r}
exp={r.split()[-1] for r in symbols.splitlines() if len(r.split())>=3 and r.split()[-2] in ('T','D','B','R')}
assert exp==exports
assert imp<={'risc_runtime_get_api','memcpy','memset','strlen','snprintf','strcmp','strcpy'}
validator=out/'validate-elf'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(S/'test/native_apps/stubs'),
 '-I'+str(S/'lib/elf_loader/include'),str(S/'lib/elf_loader/src/esp_elf_validate.c'),
 str(S/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(elf)],check=True)
manifest=ROOT/'apps/bringup/manifest.json';shutil.copyfile(manifest,out/'bringup.json')
record={'schema':1,'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True,cwd=ROOT).strip(),
 'sha256':sha(elf),'bytes':elf.stat().st_size,'manifest_sha256':sha(manifest),
 'compiler':compiler,'compiler_sha256':sha(Path(cc)),'command':cmd,
 'imports':sorted(imp),'exports':sorted(exp),'physical_testing':False,
 'source_sha256':{str(x.relative_to(ROOT)):sha(x) for x in
  [ROOT/'apps/bringup/main.c',ROOT/'apps/bringup/shared_ui.h',ROOT/'apps/bringup/native_ui.h',
   ROOT/'apps/clock/viewport.c',manifest,Path(__file__)]}}
(out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
(out/'SOURCE-NOTICE.txt').write_text('Source: michaelrolphone-cmyk/RiscRTE-Watch; preserved native Bringup diagnostics plus shared capability controller. See source archive and build.json for exact provenance.\n')
print(json.dumps(record,indent=2))
