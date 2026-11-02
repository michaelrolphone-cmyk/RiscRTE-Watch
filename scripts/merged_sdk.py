"""Stage one header identity per API, with this application's SDK suffixes last."""
from pathlib import Path
import shutil
import hashlib,json

def verify_shared_headers(home):
    home=Path(home)
    lock=json.loads((home/'dependencies.lock.json').read_text())
    for name, pin in lock.get('shared_sdk_headers', {}).items():
        rel=Path(name)
        if rel.is_absolute() or '..' in rel.parts or rel.parent != Path('sdk/app'):
            raise ValueError('Invalid shared SDK header path: '+name)
        source=home/rel
        if source.is_symlink() or not source.is_file():
            raise ValueError('Missing shared SDK header: '+name)
        if hashlib.sha256(source.read_bytes()).hexdigest()!=pin['sha256']:
            raise ValueError('Shared SDK header differs from published pin: '+name)


def headers(output, system, utilities, home):
    verify_shared_headers(home)
    target = Path(output)/'sdk/include'
    target.mkdir(parents=True, exist_ok=True)
    for source in (system/'lib/PortableApps/include', utilities/'lib/Alarm/include',
                   utilities/'lib/Contexts/include', home/'sdk/driver', home/'sdk/app'):
        for path in source.glob('*.h'):
            shutil.copyfile(path, target/path.name)
    # The common status extension is append-only; use the selected shared
    # writer contract rather than the extracted pre-BUSY constant list.
    shutil.copyfile(system/'lib/PortableApps/include/RiscKeyValueV1.h',target/'RiscKeyValueV1.h')
    shutil.copyfile(utilities/'Apps/alarm_writer.h', target/'alarm_writer.h')
    shutil.copytree(system/'lib/PortableApps/time', target.parent/'time', dirs_exist_ok=True)
    return target

def sources(output, system, home):
    """Copy the pinned source tree; overlay only exact owner-published bytes."""
    home=Path(home);target=Path(output)/'sdk/source'
    lock=json.loads((home/'dependencies.lock.json').read_text())
    if target.exists():shutil.rmtree(target)
    shutil.copytree(system/'lib/PortableApps/src',target)
    pins=lock.get('shared_source_overlays',{})
    actual={str(p.relative_to(home)) for p in (home/'sdk/source').glob('*') if p.is_file()}
    if actual!=set(pins):raise ValueError('Unpinned or missing shared source overlay')
    for name,pin in pins.items():
        rel=Path(name)
        if rel.is_absolute() or '..' in rel.parts or rel.parent!=Path('sdk/source'):
            raise ValueError('Invalid shared source overlay path: '+name)
        source=home/rel
        if source.is_symlink() or not source.is_file():
            raise ValueError('Missing shared source overlay: '+name)
        raw=source.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=pin['sha256']:
            raise ValueError('Shared source overlay differs from published pin: '+name)
        if hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()!=pin['git_blob']:
            raise ValueError('Shared source overlay differs from owning Git blob: '+name)
        shutil.copyfile(source,target/rel.name)
    return target
