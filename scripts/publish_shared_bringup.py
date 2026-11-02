#!/usr/bin/env python3
"""Publish only this workflow's qualified Bringup closure; verify every public byte."""
import gzip,hashlib,io,json,os,pathlib,shutil,tarfile,urllib.request,urllib.error
root=pathlib.Path('closure');out=pathlib.Path('release-assets');out.mkdir()
repo=os.environ['GITHUB_REPOSITORY'];head=os.environ['BRINGUP_HEAD']
assert repo=='michaelrolphone-cmyk/RiscRTE-Watch'
assert os.environ['BRINGUP_BRANCH']=='work/shared-bringup-20261102' and os.environ['BRINGUP_PR']=='23'
assert (root/'source/Watch.head').read_text().strip()==head
assembly=json.loads((root/'assembly/assembly.json').read_text())
assert assembly['packed'] and assembly['usb_candidate_included'] is False
assert assembly['baseline_tag']=='home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
canonical=root/'application/bringup.elf'
assert sha(canonical)==assembly['shared_elf_sha256']
for mode in ('normal','sanitized'):
 receipt=json.loads((root/f'admission/{mode}/receipt.json').read_text())
 assert receipt['sanitized']==(mode=='sanitized')
 assert receipt['product_binding']['bringup_source_commit']==head
 assert set(receipt['products'])=={'watch','wave','x4'}
 for lane,result in receipt['products'].items():
  for row in result['results']:
   evidence=row['result']
   assert evidence['prepared'] and evidence['cohort_validated']
   assert evidence['hardware_calls']==evidence['storage_calls']==0
   if lane=='wave':assert evidence['sd_elf_validated']
for lane,proof in assembly['products'].items():
 assert proof['store_roundtrip'] and proof['sd_roundtrip']
 assert proof['unrelated_canonical_bytes_preserved'] and proof['unrelated_sd_bytes_preserved']
 assert proof['shared_elf_sha256']==sha(canonical)
 for name,key in [('store.bin','store_sha256'),('sd-apps.zip','sd_archive_sha256')]:
  p=root/f'assembly/{lane}/{name}';assert sha(p)==proof[key]
  shutil.copyfile(p,out/f'product-{lane}-{name}')
for name in ('bringup.elf','bringup.json','build.json','SOURCE-NOTICE.txt'):
 shutil.copyfile(root/'application'/name,out/name)
shutil.copyfile(root/'assembly/assembly.json',out/'assembly.json')
shutil.copyfile(root/'baseline/baseline.json',out/'baseline.json')
shutil.copyfile(pathlib.Path('docs/shared-bringup/UPGRADE.md'),out/'UPGRADE.md')
for p in sorted((root/'source').iterdir()):shutil.copyfile(p,out/('source-'+p.name))
buf=io.BytesIO()
with tarfile.open(fileobj=buf,mode='w') as tar:
 for name in ('qualification','admission'):
  for p in sorted((root/name).rglob('*')):
   if not p.is_file():continue
   assert not p.is_symlink()
   data=p.read_bytes();info=tarfile.TarInfo(str(p.relative_to(root)))
   info.size=len(data);info.mode=0o644;info.mtime=0
   tar.addfile(info,io.BytesIO(data))
(out/'qualification.tar.gz').write_bytes(gzip.compress(buf.getvalue(),mtime=0))
assets=[{'name':p.name,'size':p.stat().st_size,'sha256':sha(p)} for p in sorted(out.iterdir())]
(out/'release-manifest.json').write_text(json.dumps({'source':head,'shared_elf_sha256':sha(canonical),'assets':assets,'hardware_tested':False},indent=2)+'\n')
tag='bringup-shared-'+head
token=os.environ['GITHUB_TOKEN'];api='https://api.github.com/repos/'+repo
def request(url,method='GET',data=None,ctype='application/json'):
 headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
 if data is not None:headers['Content-Type']=ctype
 with urllib.request.urlopen(urllib.request.Request(url,data=data,method=method,headers=headers),timeout=180) as r:return r.read()
body=('One byte-identical Bringup ELF for X4, T-Watch and Waveshare. Preserves native diagnostic UI, optional battery/RTC/motion/haptic behavior, and adds shared semantic presentation. '
 'Three fixed-size product stores/SD packages are a Bringup-only overlay of published Home/Contexts '+assembly['baseline_tag']+'. '
 'All unrelated canonical bytes, including existing USB, are unchanged. Lossless ordinary-application storage envelopes may differ only where recorded in assembly.json to retain SPIFFS reserve. Unpublished shared USB changes are not included. '
 'Runtime graph/ELF admission and normal/sanitized host tests passed. Hardware operation is not claimed. These are companion application stores, not full flash images; use the exact published baseline native firmware and existing data-preserving installation process.')
try:release=json.loads(request(api+'/releases/tags/'+tag))
except urllib.error.HTTPError as e:
 if e.code!=404:raise
 release=json.loads(request(api+'/releases','POST',json.dumps({'tag_name':tag,'target_commitish':head,'name':'Shared Bringup diagnostics — three-product closure','body':body,'draft':False,'prerelease':True}).encode()))
assert release['tag_name']==tag and release['target_commitish']==head and not release['draft']
existing={x['name']:x for x in release['assets']}
for p in sorted(out.iterdir()):
 if p.name not in existing:
  from urllib.parse import quote
  url=release['upload_url'].split('{')[0]+'?name='+quote(p.name)
  existing[p.name]=json.loads(request(url,'POST',p.read_bytes(),'application/octet-stream'))
 assert existing[p.name]['size']==p.stat().st_size
 with urllib.request.urlopen(existing[p.name]['browser_download_url'],timeout=180) as r:
  data=r.read()
 assert len(data)==p.stat().st_size and hashlib.sha256(data).hexdigest()==sha(p)
 print('VERIFIED RELEASE ASSET '+json.dumps({'name':p.name,'size':len(data),'sha256':sha(p),'url':existing[p.name]['browser_download_url']}),flush=True)
receipt={'repository':repo,'head':head,'tag':tag,'url':release['html_url'],'assets':[{'name':p.name,'size':p.stat().st_size,'sha256':sha(p),'url':existing[p.name]['browser_download_url']} for p in sorted(out.iterdir())]}
pathlib.Path('publication.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('PUBLISHED '+release['html_url'],flush=True)
