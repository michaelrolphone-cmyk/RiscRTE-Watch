"""Bind actual shared-build input bytes; a lock declaration is not verification."""
import hashlib
import subprocess
from pathlib import Path


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def file_manifest(root, paths):
    root = Path(root).resolve()
    files = set()
    for relative in paths:
        path = root / relative
        if not path.exists():
            raise RuntimeError(f'Missing build input: {path}')
        if path.is_dir():
            files.update(p for p in path.rglob('*') if p.is_file())
        else:
            files.add(path)
    result = {}
    for path in sorted(files):
        if path.is_symlink():
            raise RuntimeError(f'Unbound source symlink: {path}')
        result[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def verify_dependency(root, expected, paths):
    root = Path(root).resolve()
    actual = git(root, 'rev-parse', 'HEAD').decode().strip()
    if actual != expected:
        raise RuntimeError(f'Dependency revision mismatch at {root}: {actual} != {expected}')
    dirty = git(root, 'status', '--porcelain', '--untracked-files=all', '--', *paths)
    if dirty:
        raise RuntimeError(f'Dirty dependency build inputs at {root}:\n{dirty.decode()}')
    entries = git(root, 'ls-tree', '-rz', expected, '--', *paths).split(b'\0')
    files = {}
    for entry in entries:
        if not entry:
            continue
        metadata, relative = entry.split(b'\t', 1)
        mode, kind, oid = metadata.split()
        name = relative.decode()
        path = root / name
        if kind != b'blob' or mode not in (b'100644', b'100755') or path.is_symlink():
            raise RuntimeError(f'Unsupported dependency input: {path}')
        raw = path.read_bytes()
        actual_oid = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
        if actual_oid != oid.decode():
            raise RuntimeError(f'Dependency bytes differ from pinned commit: {path}')
        files[name] = hashlib.sha256(raw).hexdigest()
    if not files:
        raise RuntimeError(f'No verified dependency inputs at {root}')
    return {'commit': actual, 'files_sha256': files}
