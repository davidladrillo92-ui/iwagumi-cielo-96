#!/usr/bin/env python3
"""Build/run a synthetic-only LittleFS lab. No flash-file or port arguments."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'tests/vendor/littlefs'


def main():
    if len(sys.argv) != 1:
        raise SystemExit('Este laboratorio no acepta imágenes, puertos ni argumentos.')
    manifest = json.loads((VENDOR / 'SOURCE.json').read_text())
    for name, sha in manifest['git_blobs'].items():
        data = (VENDOR / name).read_bytes()
        actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if actual != sha:
            raise SystemExit('Fuente LittleFS alterada: ' + name)
    fixture = json.loads((ROOT / 'tests/fixtures/v5_nvs_regression.json').read_text())
    pv, ch = [fixture['lights'][name]['points'] for name in ('pezverde', 'chihiros')]
    lines = ['#pragma once', '#include "cielo_profile_store.h"',
             'inline cielo96::ProfileImage fixtureV5(){cielo96::ProfileImage p{};',
             'p.generation=9;p.pvCount=%d;p.chCount=%d;' % (len(pv), len(ch))]
    for member, points in (('pv', pv), ('ch', ch)):
        lines += ['p.%s[%d]={%s};' % (member, i, ','.join(map(str, point))) for i, point in enumerate(points)]
    lines += ['cielo96::profileSeal(p);return p;}']
    with tempfile.TemporaryDirectory(prefix='cielo-synthetic-lfs-') as temporary:
        build = Path(temporary)
        (build / 'v5_fixture.h').write_text('\n'.join(lines) + '\n')
        flags = ['-DLFS_NO_MALLOC', '-DLFS_NO_DEBUG', '-DLFS_NO_WARN', '-DLFS_NO_ERROR', '-I' + str(VENDOR)]
        objects = []
        for name in ('lfs.c', 'lfs_util.c'):
            target = build / (name + '.o')
            subprocess.run(['gcc', '-std=c99', '-O1', '-g', '-fexceptions', '-Wall', '-Wextra', '-Werror',
                            *flags, '-c', str(VENDOR / name), '-o', str(target)], check=True)
            objects.append(str(target))
        target = build / 'migration_lab'
        subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        '-I' + str(ROOT / 'include'), '-I' + str(build),
                        str(ROOT / 'tests/littlefs_lab/migration_lab.cpp'), *objects, '-o', str(target)], check=True)
        result = subprocess.run([str(target)], text=True, capture_output=True, timeout=120)
        print(result.stdout, end='')
        if result.stderr:
            print(result.stderr, file=sys.stderr, end='')
        result.check_returncode()
        summary = json.loads(result.stdout.splitlines()[-1])
        assert summary['power_cut_cases'] > 0 and summary['identical_save_mutations'] == 0
        assert summary['nvs_changed'] is False and summary['foreign_file_preserved'] is True
        print('PASS: laboratorio sintético; no valida todavía el ATOM físico ni su VFS.')


if __name__ == '__main__':
    main()
