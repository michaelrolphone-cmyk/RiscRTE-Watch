"""Explicit, default-off renderer reuse selection; no board defaults."""

def add_raster_cache_arguments(parser):
    parser.add_argument('--native-damage-replay', action='store_true', help='Optional exact completed-face damage replay; unknown draws and allocation failure use original renderer')
    parser.add_argument('--picker-compositor-plan', action='store_true', help='Optional bounded column-opacity and exact row-span plan for picker composition')
    parser.add_argument('--native-glyph-coverage-cache', action='store_true', help='Optional owned exact glyph alpha reuse; allocation failure renders normally')
    parser.add_argument('--native-stroke-coverage-cache', action='store_true', help='Optional owned exact four-sample stroke reuse; allocation failure renders normally')

def raster_cache_flags(args, role):
    if role != 'default':
        return []
    return ([ '-DWATCH_NATIVE_COVERAGE_CACHE' ] if args.native_glyph_coverage_cache else []) + ([ '-DWATCH_NATIVE_STROKE_CACHE' ] if args.native_stroke_coverage_cache else []) + (['-DWATCH_PICKER_COMPOSITOR_PLAN'] if args.picker_compositor_plan else []) + (['-DWATCH_NATIVE_DAMAGE_REPLAY'] if getattr(args, 'native_damage_replay', False) else [])

def raster_cache_record(args, inputs):
    paths=('apps/clock/nova/nova.c', 'apps/clock/nova/native_stroke_cache.h', 'apps/clock/nova/native_reuse_admission.h', 'apps/clock/faces/render.inc', 'apps/clock/faces/picker_compositor.h', 'apps/clock/nova/native_damage_decl.h', 'apps/clock/faces/damage_replay.inc')
    return {'native_damage_replay_enabled': getattr(args, 'native_damage_replay', False), 'native_damage_replay': {'command_capacity': 1024, 'tile_side': 16, 'unsupported_retry_frames': 4, 'unsupported_admission': 'observed direct-draw/overflow barrier; resets on actual selection or geometry change; original full rendering during bounded retry interval', 'matching': 'bounded lookahead 8 with three exact-command anchors; conservative remainder', 'owner': 'whole-face private completed RGB565 image; picker previews retain original renderer', 'allocation': 'optional invocation-owned store/image; OOM and unknown draws use original full path', 'physical_performance_verified': False}, 'picker_compositor_enabled': args.picker_compositor_plan, 'picker_compositor': {'optional_plan_bytes_per_capacity_column': 4, 'lifetime': 'picker-owned; allocation failure uses original composition', 'operations': 'per-column alpha/source index; exact rounded row spans; original blend and checkpoints'}, 'glyph_enabled': args.native_glyph_coverage_cache, 'stroke_enabled': args.native_stroke_coverage_cache,
            'selection_scope': 'default Home only; return-to-default has neither renderer',
            'allocation': 'independent optional lazy owned stores; failure uses original renderer; teardown releases',
            'glyph': {'coverage_bytes': 65536, 'metadata_entries': 128, 'host_64bit_total_bytes': 72712},
            'stroke': {'coverage_bytes': 65536, 'metadata_entries': 1024, 'hash_slots': 2048, 'bits_per_sample_count': 3, 'maximum_entry_bytes': 4096, 'host_64bit_total_bytes': 102408},
            'geometry_admission': {'kind': 'repeated actual canvas geometry', 'slots': 32, 'observations': 3, 'recent_canvas_constructions': 8, 'state_bytes_host': 516, 'bypass_preserves_cached_entries': True},
            'target_total_bytes': 'verify selected target layout; host values are not a target receipt',
            'input_sha256': {p: inputs[p] for p in paths}, 'physical_performance_verified': False}
