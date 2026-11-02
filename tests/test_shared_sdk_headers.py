#!/usr/bin/env python3
"""Verify the declared shared-header closure before every Home SDK staging."""
import hashlib,json,tempfile,unittest,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from merged_sdk import headers,verify_shared_headers
class HeaderPins(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.root=Path(self.tmp.name);self.home=self.root/'home'
  (self.home/'sdk/app').mkdir(parents=True)
  self.name='sdk/app/PortableResidentSettingsEffects.h'
  self.source=self.home/self.name;self.source.write_bytes(b'/* pinned shared header */\n')
  self.lock={'shared_sdk_headers':{self.name:{'sha256':hashlib.sha256(self.source.read_bytes()).hexdigest()}}}
  self.save()
 def save(self):(self.home/'dependencies.lock.json').write_text(json.dumps(self.lock))
 def test_exact(self):verify_shared_headers(self.home)
 def test_changed(self):
  self.source.write_bytes(b'changed')
  with self.assertRaises(ValueError):verify_shared_headers(self.home)
 def test_missing(self):
  self.source.unlink()
  with self.assertRaises(ValueError):verify_shared_headers(self.home)
 def test_traversal(self):
  self.lock['shared_sdk_headers']['../outside']=self.lock['shared_sdk_headers'].pop(self.name);self.save()
  with self.assertRaises(ValueError):verify_shared_headers(self.home)
 def test_symlink(self):
  target=self.root/'target';self.source.rename(target);self.source.symlink_to(target)
  with self.assertRaises(ValueError):verify_shared_headers(self.home)
 def test_staging(self):
  system=self.root/'system';utilities=self.root/'utilities'
  for p in [system/'lib/PortableApps/include',system/'lib/PortableApps/time',utilities/'Apps']:
   p.mkdir(parents=True)
  (system/'lib/PortableApps/include/RiscKeyValueV1.h').write_text('/* KV */')
  (utilities/'Apps/alarm_writer.h').write_text('/* alarm */')
  out=headers(self.root/'out',system,utilities,self.home)
  self.assertEqual((out/self.source.name).read_bytes(),self.source.read_bytes())
 def test_real_published_pin(self):
  verify_shared_headers(Path(__file__).resolve().parents[1])
if __name__=='__main__':unittest.main()
