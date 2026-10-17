#!/usr/bin/env python3
"""Compare native face pixels with original atlases and production packed atlases."""
from pathlib import Path
import argparse
import hashlib
import os
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'tests/native_faces_test.c').read_text()
helper = r'''
static uint64_t atlas_frame_hash(const risc_display_surface_v1 *surface) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned y=0;y<surface->height;y++) {
        const uint8_t *row=(const uint8_t *)surface->pixels+y*surface->stride_bytes;
        for(unsigned x=0;x<surface->width*2u;x++) {
            hash^=row[x];hash*=UINT64_C(1099511628211);
        }
    }
    return hash;
}
'''
marker = 'static unsigned checks,captures;'
assert source.count(marker) == 1
source = source.replace(marker, helper + marker)
marker = '}assert(ink);checks++;'
assert source.count(marker) == 1
source = source.replace(marker, marker + r'''
   printf("ATLAS-FRAME %ux%u state=%u face=%u hash=%016llx\n",width,height,n,id,
          (unsigned long long)atlas_frame_hash(&frame));
''')
with tempfile.TemporaryDirectory(prefix='atlas-render-') as temporary:
    path = Path(temporary)
    harness = path / 'compare.c'
    harness.write_text(source)
    outputs = []
    for packed in (False, True):
        binary = path / ('packed' if packed else 'original')
        flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                 '-DWATCH_CONTEXTS_CLIENT', '-DWATCH_COMPACT_NOVA_ASSETS']
        if args.sanitize:
            flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        if packed:
            flags += ['-DWATCH_COMPRESSED_CALENDAR_ALPHA', '-DWATCH_COMPRESSED_FACE_ALPHA']
        flags += ['-I' + str(ROOT / part) for part in ('.', 'include', 'sdk/app', 'sdk/driver')]
        subprocess.run([os.environ.get('CC', 'cc'), *flags, str(harness),
                        str(ROOT / 'apps/clock/nova/nova.c'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], check=True, capture_output=True, text=True,
                                env={**{k: v for k, v in os.environ.items() if k != 'WATCH_NATIVE_IMAGES'}, 'ASAN_OPTIONS':'detect_leaks=0'})
        hashes = [line for line in result.stdout.splitlines() if line.startswith('ATLAS-FRAME ')]
        assert len(hashes) == 1632, f'Unexpected native frame matrix: {len(hashes)}'
        outputs.append(hashes)
    for original, packed in zip(*outputs):
        assert original == packed, f'Atlas pixel mismatch: {original} != {packed}'
    digest = hashlib.sha256(('\n'.join(outputs[0]) + '\n').encode()).hexdigest()
    print(f'Original versus packed atlases: all 1,632 native face/state pixel hashes at eight sizes exact; SHA-256 {digest}; sanitized={args.sanitize} PASS')
