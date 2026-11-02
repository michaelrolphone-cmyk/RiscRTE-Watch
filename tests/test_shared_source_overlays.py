import hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('merged_sdk',ROOT/'scripts/merged_sdk.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class OverlayTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
  self.home=self.root/'home';self.system=self.root/'system';self.out=self.root/'out'
  (self.home/'sdk/source').mkdir(parents=True);(self.system/'lib/PortableApps/src').mkdir(parents=True)
  (self.system/'lib/PortableApps/src/base.c').write_text('base')
  self.raw=b'overlay';self.name='sdk/source/base.c';(self.home/self.name).write_bytes(self.raw)
  self.pin={'sha256':hashlib.sha256(self.raw).hexdigest(),'git_blob':hashlib.sha1(b'blob 7\0'+self.raw).hexdigest()}
  self.lock={'shared_source_overlays':{self.name:self.pin}};self.write()
 def tearDown(self):self.tmp.cleanup()
 def write(self):(self.home/'dependencies.lock.json').write_text(json.dumps(self.lock))
 def stage(self):return module.sources(self.out,self.system,self.home)
 def test_exact(self):self.assertEqual((self.stage()/'base.c').read_bytes(),self.raw)
 def test_tamper(self):
  (self.home/self.name).write_text('changed')
  with self.assertRaises(ValueError):self.stage()
 def test_unpinned(self):
  (self.home/'sdk/source/extra.c').write_text('extra')
  with self.assertRaises(ValueError):self.stage()
 def test_git_identity(self):
  self.pin['git_blob']='0'*40;self.write()
  with self.assertRaises(ValueError):self.stage()
 def test_stale_staging(self):
  target=self.stage();(target/'stale.inc').write_text('old')
  self.assertFalse((self.stage()/'stale.inc').exists())
 def test_missing(self):
  (self.home/self.name).unlink()
  with self.assertRaises(ValueError):self.stage()
 def test_symlink(self):
  (self.home/self.name).unlink();(self.home/self.name).symlink_to(self.system/'lib/PortableApps/src/base.c')
  with self.assertRaises(ValueError):self.stage()
if __name__=='__main__':unittest.main()
