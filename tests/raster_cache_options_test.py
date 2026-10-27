import argparse
import hashlib
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'scripts'))
from raster_cache_options import add_raster_cache_arguments,raster_cache_flags,raster_cache_record
from build_inputs import file_manifest
p=argparse.ArgumentParser();add_raster_cache_arguments(p)
inputs=file_manifest(root,['apps/clock/nova','apps/clock/faces'])
for glyph in (False,True):
 for stroke in (False,True):
  args=p.parse_args((['--native-glyph-coverage-cache'] if glyph else [])+(['--native-stroke-coverage-cache'] if stroke else []))
  flags=raster_cache_flags(args,'default');record=raster_cache_record(args,inputs)
  assert ('-DWATCH_NATIVE_COVERAGE_CACHE' in flags)==glyph
  assert ('-DWATCH_NATIVE_STROKE_CACHE' in flags)==stroke
  assert raster_cache_flags(args,'clock')==[]
  assert record['glyph_enabled']==glyph and record['stroke_enabled']==stroke
  assert record['input_sha256']['apps/clock/nova/native_stroke_cache.h']==hashlib.sha256((root/'apps/clock/nova/native_stroke_cache.h').read_bytes()).hexdigest()
assert 'apps/clock/nova/native_reuse_admission.h' in record['input_sha256']
assert record['geometry_admission']['observations']==3
print('PASS four explicit selections, disabled default, return-role exclusion, actual header source binding')

for enabled in (False,True):
 args=p.parse_args(['--picker-compositor-plan'] if enabled else [])
 assert ('-DWATCH_PICKER_COMPOSITOR_PLAN' in raster_cache_flags(args,'default'))==enabled
 assert raster_cache_flags(args,'clock')==[]
 record=raster_cache_record(args,inputs)
 assert record['picker_compositor_enabled']==enabled
 assert record['input_sha256']['apps/clock/faces/picker_compositor.h']==hashlib.sha256((root/'apps/clock/faces/picker_compositor.h').read_bytes()).hexdigest()
print('PASS optional picker compositor selection, default-off, return-role exclusion, exact helper source binding')

args=p.parse_args(['--native-glyph-coverage-cache','--native-stroke-coverage-cache','--picker-compositor-plan'])
assert set(raster_cache_flags(args,'default'))=={'-DWATCH_NATIVE_COVERAGE_CACHE','-DWATCH_NATIVE_STROKE_CACHE','-DWATCH_PICKER_COMPOSITOR_PLAN'}

for mask in range(16):
 options=['--native-glyph-coverage-cache','--native-stroke-coverage-cache','--picker-compositor-plan','--native-damage-replay']
 args=p.parse_args([option for i,option in enumerate(options) if mask&(1<<i)])
 record=raster_cache_record(args,inputs)
 assert record['native_damage_replay_enabled']==bool(mask&8)
 assert ('-DWATCH_NATIVE_DAMAGE_REPLAY' in raster_cache_flags(args,'default'))==bool(mask&8)
 assert raster_cache_flags(args,'clock')==[]
 for path in ['apps/clock/nova/native_damage_decl.h','apps/clock/faces/damage_replay.inc']:
  assert record['input_sha256'][path]==hashlib.sha256((root/path).read_bytes()).hexdigest()
print('PASS sixteen damage/cache selections, disabled default, return-role exclusion, exact owner source bindings')
