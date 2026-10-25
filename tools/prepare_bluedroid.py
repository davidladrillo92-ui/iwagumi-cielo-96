"""Build a private copy of the pinned Arduino BLE library; never edit the SDK."""
from pathlib import Path
import hashlib,json,shutil

def prepare(framework,project):
    framework,project=Path(framework),Path(project)
    source=framework/'libraries/BLE'
    vendor=project/'vendor/bluedroid'
    pins=json.loads((vendor/'UPSTREAM_SHA256.json').read_text())
    for name,digest in pins.items():
        if hashlib.sha256((source/'src'/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Arduino BLE 2.0.17 source mismatch: '+name)
    destination=project/'.cielo-generated/CieloBluedroid'
    if destination.exists():shutil.rmtree(destination)
    shutil.copytree(source,destination)
    for name in ('BLEClient.cpp','BLEClient.h','BLERemoteCharacteristic.cpp','BLERemoteCharacteristic.h'):shutil.copy2(vendor/name,destination/'src'/name)
    for name in ('library.properties','library.json'):
        path=destination/name
        if path.exists():path.unlink()
    (destination/'library.json').write_text(json.dumps({'name':'CieloBluedroid','version':'2.0.17-cielo.1','frameworks':['arduino'],'platforms':['espressif32']}))
    return destination

if 'Import' in globals():
    Import('env')
    framework=env.PioPlatform().get_package_dir('framework-arduinoespressif32')
    prepare(framework,env.subst('$PROJECT_DIR'))
