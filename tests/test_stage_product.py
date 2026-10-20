"""Composition tests. Small synthetic ELFs exercise staging, not native execution."""
from pathlib import Path
import copy
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from stage_product import app_bundle, document, encoded, read_store, safe_path, sha, stage, write_new_store


def fake_elf(tag=b''):
    data = bytearray(52)
    data[:7], data[16:20] = b'\x7fELF\x01\x01\x01', b'\x03\x00\x5e\x00'
    return bytes(data) + tag


def app(role, version='0.15.0'):
    return dict(type='application', id='twatch-clock' if role == 'default' else 'twatch-clock-return',
                version=version, architecture='xtensa-esp32s3', file_name=role+'.elf', entry='app_main', requires=[])


def fixture(board='device-a', shift=0):
    grants=[dict(capability=cap, api=api, instance_id=instance) for cap,api,instance in [
        ('display.output',1,5+shift), ('input.touch.raw',1,6+shift), ('runtime.realtime-control',1,0),
        ('storage.key-value',1,1), ('storage.key-value',1,5)]]
    policies=[dict(manifest='default.json',grants=grants),dict(manifest='clock.json',grants=[])]
    store={'board.json':encoded({'board_id':board}), 'default.elf':fake_elf(b'old-home'),
           'clock.elf':fake_elf(b'old-return'), 'default.json':encoded(app('default','0.11.5')),
           'clock.json':encoded(app('clock','0.11.0')), 'settings.bin':b'user bytes'}
    drivers=[]
    for n in range(31):
        path=f'provider{n}/manifest.json'
        store[path]=encoded({'provides':[dict(capability=f'test.function{n}',api=1)]})
        drivers.append(dict(manifest=path,instance_id=n+1))
    for n in range(23):
        name=f'child{n}.json';policies.append(dict(manifest=name,grants=[]))
        store[name]=encoded({'id':f'child{n}'});store[f'child{n}.elf']=fake_elf(bytes([n]))
    store['boot.json']=encoded(dict(board='board.json',default_app='default.elf',provider_activation='demand-retained',
        drivers=drivers,app_capabilities=policies))
    apps={'default.elf':fake_elf(b'shared-home'), 'clock.elf':fake_elf(b'shared-return'),
          'default.json':encoded({**app('default'),'requires':[dict(capability=c,api=1) for c in ['power.sleep','input.power-key','display.power']]}),
          'clock.json':encoded(app('clock'))}
    services={};chosen=[]
    for artifact,cap in [('power_sleep','power.sleep'),('power_key','input.power-key'),('display_power','display.power')]:
        manifest=dict(type='driver',id='compat-'+artifact,version='0.1.0',driver_abi=2,architecture='xtensa-esp32s3',file_name='driver.elf',
            requires=[],provides=[dict(capability=cap,api=1)])
        services[artifact+'/manifest.json']=encoded(manifest)
        services[artifact+'/driver.elf']=fake_elf(artifact.encode())
        chosen.append(dict(artifact=artifact,directory=artifact,capability=cap,api=1))
    return store,apps,services,dict(schema=1,name=board,board_id=board,services=chosen)


class StageTest(unittest.TestCase):
    def setUp(self):
        self.store,self.apps,self.services,self.profile=fixture()
    def compose(self,capacity=40):
        return stage(self.store,self.apps,self.services,self.profile,provider_capacity=capacity)
    def test_same_bytes_different_hardware(self):
        first,_=self.compose()
        second=fixture('device-b',100)
        other,_=stage(*second,provider_capacity=40)
        for role in ['default','clock']:
            self.assertEqual(first[role+'.elf'],other[role+'.elf'])
        a=document(first['boot.json'])['app_capabilities'][0]['grants']
        b=document(other['boot.json'])['app_capabilities'][0]['grants']
        self.assertEqual(a[0]['instance_id'],5);self.assertEqual(b[0]['instance_id'],105)
    def test_complete_inventory_and_unchanged_bytes(self):
        new,report=self.compose()
        self.assertEqual((report['applications'],report['providers_before'],report['providers_after']),(25,31,34))
        for name,raw in self.store.items():
            if name not in report['changed_files']:self.assertEqual(new[name],raw)
        self.assertEqual(len(report['added_files']),6)
        self.assertFalse(report['flashable'])
        for role in ('default', 'clock'):
            self.assertEqual(report['manifest_sha256'][role + '.json'], sha(new[role + '.json']))
    def test_input_immutable_on_success_and_failure(self):
        before=copy.deepcopy((self.store,self.apps,self.services,self.profile))
        self.compose()
        with self.assertRaises(ValueError):self.compose(32)
        self.assertEqual(before,(self.store,self.apps,self.services,self.profile))
    def test_native_capacity(self):
        with self.assertRaisesRegex(ValueError,'34 provider slots'):self.compose(32)
        self.compose(34)
    def test_return_has_no_grants(self):
        boot=document(self.store['boot.json']);boot['app_capabilities'][1]['grants']=[dict(capability='board.battery',api=1,instance_id=9)]
        self.store['boot.json']=encoded(boot)
        new,_=self.compose()
        self.assertEqual(document(new['clock.json'])['requires'],[])
        self.assertEqual(document(new['boot.json'])['app_capabilities'][1]['grants'],[])
    def test_namespaces_and_shared_data_preserved(self):
        boot=document(self.store['boot.json']);row=boot['app_capabilities'][0]
        row['grants'] += [dict(capability='storage.shared-data',api=1,instance_id=i,file='context-rules.ctx') for i in [2,3]]
        self.store['boot.json']=encoded(boot)
        new,_=self.compose();got=document(new['boot.json'])['app_capabilities'][0]['grants']
        self.assertEqual(got[:len(row['grants'])],row['grants'])
        self.assertIn(dict(capability='storage.shared-data',api=1),document(new['default.json'])['requires'])
    def test_optional_services_not_invented(self):
        self.profile['services']=[]
        new,report=self.compose(31)
        caps={r['capability'] for r in document(new['default.json'])['requires']}
        self.assertNotIn('power.sleep',caps);self.assertNotIn('board.battery',caps)
        self.assertEqual(report['providers_after'],31)
    def test_idempotent(self):
        new,_=self.compose()
        again,report=stage(new,self.apps,self.services,self.profile,provider_capacity=40)
        self.assertEqual(new,again);self.assertEqual(report['added_files'],[])
    def test_wrong_board(self):
        self.profile['board_id']='some-other-board'
        with self.assertRaisesRegex(ValueError,'device board'):self.compose()
    def test_duplicate_grants(self):
        boot=document(self.store['boot.json']);row=boot['app_capabilities'][0];row['grants'].append(row['grants'][0])
        self.store['boot.json']=encoded(boot)
        with self.assertRaisesRegex(ValueError,'Duplicate Home grant'):self.compose()
    def test_no_device_specific_capability(self):
        boot=document(self.store['boot.json']);boot['app_capabilities'][0]['grants'][0]['capability']='x4.battery'
        self.store['boot.json']=encoded(boot)
        with self.assertRaisesRegex(ValueError,'Device-specific'):self.compose()
    def test_service_collision(self):
        self.store['power_sleep/driver.elf']=b'other implementation'
        with self.assertRaisesRegex(ValueError,'already occupied'):self.compose()
    def test_service_function(self):
        self.profile['services'][0]['capability']='motion.accel'
        with self.assertRaisesRegex(ValueError,'different function'):self.compose()
    def test_bad_elf(self):
        self.apps['default.elf']=b'not an elf'
        with self.assertRaisesRegex(ValueError,'shared ELF'):self.compose()
    def test_identity_preserved(self):
        manifest=document(self.apps['default.json']);manifest['id']='new-settings-namespace'
        self.apps['default.json']=encoded(manifest)
        with self.assertRaisesRegex(ValueError,'identity'):self.compose()
    def test_home_presentation_updates_without_changing_identity_or_other_fields(self):
        old = document(self.store['default.json'])
        old.update(display_name='twatch-clock', launcher_visible=True,
                   description='Keep description', icon='existing-icon',
                   optional=[{'capability':'storage.files','api':1}],
                   supported_file_types=['.txt'], runtime_lifecycle='foreground',
                   min_firmware_version='0.1.0')
        self.store['default.json'] = encoded(old)
        candidate = document(self.apps['default.json'])
        candidate.update(display_name='Home', launcher_visible=False)
        self.apps['default.json'] = encoded(candidate)
        new, _ = self.compose()
        got = document(new['default.json'])
        self.assertEqual(got['display_name'], 'Home')
        self.assertIs(got['launcher_visible'], False)
        for key, value in old.items():
            if key not in ('version', 'requires', 'display_name', 'launcher_visible'):
                self.assertEqual(got[key], value, key)
        self.assertEqual(document(new['boot.json'])['default_app'], 'default.elf')

    def test_product_return_visibility_and_all_metadata_preserved(self):
        for visible in (True, False):
            with self.subTest(visible=visible):
                old = document(self.store['clock.json'])
                old.update(display_name='Clock', launcher_visible=visible,
                           icon='solid:f017', description='Return to Home')
                self.store['clock.json'] = encoded(old)
                candidate = document(self.apps['clock.json'])
                candidate.update(display_name='Unwanted label', launcher_visible=not visible)
                self.apps['clock.json'] = encoded(candidate)
                new, _ = self.compose()
                got = document(new['clock.json'])
                self.assertEqual(got, {**old, 'version': candidate['version'], 'requires': []})

    def test_legacy_bundle_without_presentation_preserves_existing_metadata(self):
        old = document(self.store['default.json'])
        old.update(display_name='Home', launcher_visible=False)
        self.store['default.json'] = encoded(old)
        new, _ = self.compose()
        self.assertEqual(document(new['default.json'])['display_name'], 'Home')
        self.assertIs(document(new['default.json'])['launcher_visible'], False)

    def test_actual_shared_template_controls_staged_home(self):
        template = Path(__file__).resolve().parents[1] / 'apps/clock/default.json'
        self.apps['default.json'] = template.read_bytes()
        new, _ = self.compose()
        got = document(new['default.json'])
        self.assertEqual(got['display_name'], 'Home')
        self.assertIs(got['launcher_visible'], False)
        self.assertEqual(got['id'], 'twatch-clock')

    def test_requires_display_and_input(self):
        boot=document(self.store['boot.json']);boot['app_capabilities'][0]['grants'].pop(0)
        self.store['boot.json']=encoded(boot)
        with self.assertRaisesRegex(ValueError,'required Home'):self.compose()
    def test_safe_paths_and_duplicate_json(self):
        for path in ['../x','/x','a//x','a\\x','a'*32]:
            with self.assertRaises(ValueError):safe_path(path)
        with self.assertRaises(ValueError):document(b'{"id":1,"id":2}')
    def test_new_output_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'new';write_new_store(path,self.store)
            self.assertEqual(read_store(path),self.store)
            with self.assertRaises(ValueError):write_new_store(path,self.store)
            (path/'alias').symlink_to(path/'settings.bin')
            with self.assertRaises(ValueError):read_store(path)
    def test_bundle_checks_role_and_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);record={'build_variant':'shared-no-board-defines','artifacts':{}}
            for role in ['default','clock']:
                raw=self.apps[role+'.elf'];record['artifacts'][role]=dict(file_name=role+'.elf',bytes=len(raw),sha256=sha(raw),role='return-to-default' if role=='clock' else 'home')
            for name,data in self.apps.items():(root/name).write_bytes(data)
            (root/'build.json').write_bytes(encoded(record));self.assertEqual(app_bundle(root)[0],self.apps)
            record['artifacts']['clock']['role']='home';(root/'build.json').write_bytes(encoded(record))
            with self.assertRaisesRegex(ValueError,'return module'):app_bundle(root)
            record['artifacts']['clock']['role']='return-to-default';(root/'build.json').write_bytes(encoded(record))
            (root/'default.elf').write_bytes(fake_elf(b'other'))
            with self.assertRaisesRegex(ValueError,'receipt mismatch'):app_bundle(root)

if __name__=='__main__':unittest.main()
