"""Stage one header identity per API, with this application's SDK suffixes last."""
from pathlib import Path
import shutil

def headers(output, system, utilities, home):
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
