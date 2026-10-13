#!/usr/bin/env python3
"""Exercise shared width-based geometry in normal and ASan/UBSan modes."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='watch-layout-') as name:
    for mode in ('normal', 'sanitized'):
        out = Path(name) / mode
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if mode == 'sanitized' else []
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
            '-Wall', '-Wextra', '-Werror', *flags, '-I' + str(ROOT / 'include'),
            str(ROOT / 'tests/test_watch_layout.c'), '-o', str(out)], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        subprocess.run([str(out)], env=env, check=True)
