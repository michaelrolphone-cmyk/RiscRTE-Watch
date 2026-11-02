#!/usr/bin/env python3
"""Bringup-only overlay of published Home/Contexts. Existing USB is byte-preserved."""
import argparse,copy,json,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('system','baseline','application','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from compose_shared_app_cohort import encode,digest,safe,write_tree
from assemble_shared_home_cohort import inventory,fingerprint,pack_candidate,providers
from assemble_shared_usb_cohort import decode_store,storage_record
from image_envelope import encode_image,MAGIC
from current_bootfs import build
from read_only_spiffs import read_image
lock=json.loads((ROOT/'docs/shared-bringup/baseline.json').read_text())
for asset in lock['assets']:
 raw=(a.baseline/asset['name']).read_bytes()
 assert len(raw)==asset['bytes'] and digest(raw)==asset['sha256'],'Published baseline changed'
elf=(a.application/'bringup.elf').read_bytes()
manifest=(a.application/'bringup.json').read_bytes();common=json.loads(manifest)
proof=json.loads((a.application/'build.json').read_text())
assert proof['sha256']==digest(elf) and proof['manifest_sha256']==digest(manifest)
assert proof['exports']==sorted(['app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'])
assert common['requires']==[{'capability':'ui.scene','api':1}]
expected_grants=[
 {'api':1,'capability':'display.output','instance_id':5},
 {'api':1,'capability':'input.touch.raw','instance_id':6},
 {'api':1,'capability':'board.battery','instance_id':4},
 {'api':2,'capability':'rtc.clock','instance_id':8},
 {'api':1,'capability':'motion.accel','instance_id':7},
 {'api':1,'capability':'haptic.effect','instance_id':9}]
assert not a.output.exists();a.output.mkdir(parents=True)
results={}
for lane in ('watch','wave','x4'):
 raw=(a.baseline/f'product-{lane}-store-store.bin').read_bytes()
 stored=read_image(raw,len(raw));before=decode_store(stored)
 with zipfile.ZipFile(a.baseline/f'product-{lane}-sd-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 out=dict(before);packed=dict(stored);after_sd=dict(sd)
 boot=json.loads(before['boot.json']);original_boot=copy.deepcopy(boot)
 available=providers(before,boot)
 assert ('ui.scene',1,0) in available
 rows=[r for r in boot['app_capabilities'] if r['manifest']=='bringup.json']
 if lane!='x4':
  expected={'grants':expected_grants,'manifest':'bringup.json'}
  if lane=='wave':expected['image_source']={'path':'Apps/watch-bringup/bringup.elf','volume_instance':12}
  assert rows==[expected],'Original Bringup authority changed'
  old=json.loads(before['bringup.json'])
  assert old['id']=='watch-bringup' and old['file_name']=='bringup.elf' and old['launcher_visible'] is False
  new=copy.deepcopy(old)
  for key in ('requires','optional','version','architecture','entry'):new[key]=copy.deepcopy(common[key])
  row=copy.deepcopy(rows[0]);row['grants'].append({'api':1,'capability':'ui.scene','instance_id':0})
  boot['app_capabilities'][boot['app_capabilities'].index(rows[0])]=row
 else:
  assert not rows and 'bringup.json' not in before and 'bringup.elf' not in before
  new=copy.deepcopy(common);new['id']='watch-bringup';new['launcher_visible']=False
  row={'manifest':'bringup.json','grants':[{'api':1,'capability':'ui.scene','instance_id':0}]}
  for decl in common['optional']:
   matches=[k for k in available if k[:2]==(decl['capability'],decl['api'])]
   assert len(matches)<=1,'Ambiguous optional provider must not be guessed'
   if matches:row['grants'].append({'capability':matches[0][0],'api':matches[0][1],'instance_id':matches[0][2]})
  boot['app_capabilities'].append(row)
 for g in row['grants']:assert (g['capability'],g['api'],g['instance_id']) in available
 # Runtime role only changes if the selected baseline actually uses resident policy.
 if 'resident_shell' in boot:
  rs=boot['resident_shell'];assert isinstance(rs.get('foreground'),list)
  oldrs=copy.deepcopy(rs)
  if 'bringup.elf' in rs.get('legacy',[]):rs['legacy'].remove('bringup.elf')
  if 'bringup.elf' not in rs['foreground']:rs['foreground'].append('bringup.elf')
  assert {k:v for k,v in rs.items() if k not in ('foreground','legacy')}=={k:v for k,v in oldrs.items() if k not in ('foreground','legacy')}
 assert {k:v for k,v in boot.items() if k not in ('app_capabilities','resident_shell')}=={k:v for k,v in original_boot.items() if k not in ('app_capabilities','resident_shell')}
 assert [r for r in boot['app_capabilities'] if r['manifest']!='bringup.json']==[r for r in original_boot['app_capabilities'] if r['manifest']!='bringup.json']
 out['boot.json']=encode(boot);out['bringup.json']=encode(new)
 packed['boot.json']=out['boot.json'];packed['bringup.json']=out['bringup.json']
 if lane=='wave':
  assert 'Apps/watch-bringup/bringup.elf' in sd
  after_sd['Apps/watch-bringup/bringup.elf']=elf
 else:out['bringup.elf']=elf;packed['bringup.elf']=elf
 allowed={'boot.json','bringup.json'}|({'bringup.elf'} if lane!='wave' else set())
 assert not(set(before)-set(out))
 assert all(out[n]==b for n,b in before.items() if n not in allowed)
 assert all(packed[n]==b for n,b in stored.items() if n not in allowed)
 assert set(after_sd)==set(sd)
 assert all(after_sd[n]==b for n,b in sd.items() if not(lane=='wave' and n=='Apps/watch-bringup/bringup.elf'))
 # No other app may be re-encoded to make this overlay fit. In particular the
 # denied unpublished USB candidate is neither read nor included here.
 try:build(packed,len(raw))
 except Exception as error:
  if type(error).__name__!='SpiffsFullError' or lane!='watch':raise
  packed['bringup.elf']=encode_image(elf)
  build(packed,len(raw))
 assert decode_store(packed)==out
 encoding=storage_record(out,packed)
 path=a.output/lane
 for name,entries in [('before',before),('before-stored',stored),('before-sd',sd),('after',out),('stored',packed),('after-sd',after_sd)]:
  write_tree(path/name,entries)
 result={'inventory_sha256':fingerprint(inventory(before,sd)),'shared_elf_sha256':digest(elf),
  'installed_manifest_sha256':digest(out['bringup.json']),'baseline_store_sha256':digest(raw),
  'image_storage':encoding,'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
  'changed_stored':[n for n in sorted(packed) if packed[n]!=stored.get(n)],
  'changed_sd':[n for n in sorted(after_sd) if after_sd[n]!=sd.get(n)],
  'unrelated_canonical_bytes_preserved':True,'unrelated_stored_bytes_preserved':True,
  'unrelated_sd_bytes_preserved':True,'provider_and_saved_data_ownership_preserved':True,
  'installed_policy':row,'resident_policy':boot.get('resident_shell')}
 result.update(pack_candidate(path,packed,after_sd,len(raw),build,read_image))
 results[lane]=result
 print('BRINGUP PRODUCT '+lane+' '+json.dumps(result),flush=True)
(a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':True,
 'shared_elf_sha256':digest(elf),'baseline_tag':lock['tag'],'physical_testing':False,
 'usb_candidate_included':False,'scope':'Bringup only; all published Home, Contexts and existing USB bytes unchanged'}))
