#!/usr/bin/env python3
"""Render every shared face directly at capability-native dimensions."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
for sanitized in (False,True):
 with tempfile.TemporaryDirectory(prefix="native-faces-") as temp:
  flags=["-std=c11","-O1","-g","-Wall","-Wextra","-Werror","-DWATCH_CONTEXTS_CLIENT","-DWATCH_COMPACT_NOVA_ASSETS","-DWATCH_COMPRESSED_CALENDAR_ALPHA","-DWATCH_COMPRESSED_FACE_ALPHA"]
  if sanitized:flags += ["-fsanitize=address,undefined","-fno-sanitize-recover=all","-fno-omit-frame-pointer","-no-pie"]
  flags += ["-I"+str(root/p) for p in (".","include","sdk/app","sdk/driver")]
  binary=Path(temp)/"test"
  subprocess.run([os.environ.get("CC","cc"),*flags,str(root/"tests/native_faces_test.c"),str(root/"apps/clock/nova/nova.c"),"-o",str(binary)],check=True)
  subprocess.run([str(binary)],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})
