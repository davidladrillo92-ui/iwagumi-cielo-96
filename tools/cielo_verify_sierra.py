#!/usr/bin/env python3
"""Read-only CIELO verifier, Python3.9/macOS Sierra, standard library only."""
import argparse
import hashlib
import ipaddress
import json
import os
import pathlib
import plistlib
import re
import secrets
import subprocess
import sys
import time
import urllib.request
import urllib.parse

VERSION = '1.0.0'
MAX_BYTES = 131072
PRIVATE = tuple(ipaddress.ip_network(s) for s in ('10.0.0.0/8','172.16.0.0/12','192.168.0.0/16'))


def local_url(value):
    u = urllib.parse.urlsplit(value)
    try:
        host = ipaddress.IPv4Address(u.hostname or '')
        port = u.port
    except ValueError:
        raise ValueError('LOCAL_IPV4_REQUIRED') from None
    if (u.scheme != 'http' or u.username or u.password or u.query or u.fragment or
            u.path not in ('', '/') or not any(host in n for n in PRIVATE)):
        raise ValueError('LOCAL_HTTP_ROOT_REQUIRED')
    return 'http://' + str(host) + (':' + str(port) if port else '')


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError('REDIRECT_REFUSED')


def read_json(path):
    with pathlib.Path(path).open('rb') as f:
        raw = f.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError('INPUT_TOO_LARGE')
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError('JSON_OBJECT_REQUIRED')
    return value


def get_json(base, path):
    # Never inherit proxies; only a validated literal private IPv4 is contacted.
    base = local_url(base)
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    request = urllib.request.Request(base + path, method='GET', headers={'Accept':'application/json'})
    with opener.open(request, timeout=8) as response:
        raw = response.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError('RESPONSE_TOO_LARGE')
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError('JSON_OBJECT_REQUIRED')
    return value


def integer(v):
    return v if type(v) is int and v >= 0 else None


def checksum(v):
    return v.lower() if isinstance(v, str) and re.fullmatch('[0-9a-fA-F]{8}',v) else None


def profile_checksum(d):
    if d.get('schema') != 'iwagumi-cielo-profile' or d.get('schema_version') != 2 or d.get('curve_unit') != 'minute':
        raise ValueError('PROFILE_SCHEMA')
    lights = d['lights']
    all_rows = []
    for name, length, limit in [('pezverde',5,255),('chihiros',4,100)]:
        rows = lights[name]['points']
        if not isinstance(rows,list) or not 2 <= len(rows) <= 64:
            raise ValueError('PROFILE_SIZE')
        for r in rows:
            if not isinstance(r,list) or len(r)!=length or any(type(v) is not int for v in r):
                raise ValueError('PROFILE_POINT')
            if not 0<=r[0]<=1440 or any(not 0<=v<=limit for v in r[1:4]) or (length==5 and not 0<=r[4]<=100):
                raise ValueError('PROFILE_RANGE')
        if any(a[0]>=b[0] for a,b in zip(rows,rows[1:])) or rows[-1][0]!=1440:
            raise ValueError('PROFILE_ORDER')
        all_rows.append(rows)
    pv,ch = all_rows
    if pv[0][4]!=0 or pv[-1][4]!=0 or ch[-1][1:]!=[0,0,0]:
        raise ValueError('PROFILE_OFF')
    h = 2166136261
    wire = '|'.join(';'.join(','.join(map(str,r)) for r in rows) for rows in all_rows)
    for b in wire.encode('ascii'):
        h = ((h ^ b)*16777619)&0xffffffff
    result = '{:08x}'.format(h)
    if d.get('integrity',{}).get('algorithm')!='fnv1a32' or checksum(d.get('integrity',{}).get('checksum'))!=result:
        raise ValueError('PROFILE_CHECKSUM')
    return result


def section(d, key):
    value=d.get(key)
    return value if isinstance(value,dict) else {}


def analyze(report, profile=None):
    findings = []
    def check(name, result):
        findings.append({'check':name,'result':result})
    fw = report.get('firmware')
    fw = fw if isinstance(fw,str) and re.fullmatch(r'11\.1\.0-RC5-RECOVERY3\.[0-9]+-[A-Z0-9.-]+',fw) else None
    check('firmware_3_12', 'PASS' if fw and fw.startswith('11.1.0-RC5-RECOVERY3.12-') else 'PENDING')
    critical = section(report,'alerts').get('current_critical')
    check('no_current_critical','PASS' if critical is False else 'FAIL' if critical is True else 'UNKNOWN')
    evidence = section(report,'diagnostic_evidence')
    known = evidence.get('schema_version')==1
    check('diagnostic_schema','PASS' if known else 'PENDING')
    storage = section(report,'curve_storage')
    check('littlefs_loaded','PASS' if storage.get('loaded_backend')=='LITTLEFS_AB' and storage.get('fs_available') is True else 'UNKNOWN')
    save = section(evidence,'curve_save') if known else {}
    result = save.get('last_result')
    check('last_curve_save','PASS' if result in ('VERIFIED','UNCHANGED') else 'FAIL' if result in ('FAILED','UNCERTAIN') else 'NOT_TESTED')
    curves = section(report,'curves')
    actual = checksum(curves.get('wire_checksum'))
    valid = curves.get('content_valid')
    check('active_profile_valid','PASS' if valid is True else 'FAIL' if valid is False else 'UNKNOWN')
    expected = None
    if profile is not None:
        try:
            expected = profile_checksum(profile)
            check('profile_identity','UNKNOWN' if actual is None else 'PASS' if expected==actual else 'FAIL')
        except (ValueError,KeyError,TypeError,AttributeError):
            check('profile_identity','INVALID_INPUT')
    else:
        check('profile_identity','NOT_TESTED')
    ble = section(report,'bluetooth')
    blockage = section(ble,'blockage_diagnostic').get('kind')
    check('no_current_ble_blockage','PASS' if blockage=='NONE' else 'UNKNOWN' if blockage is None else 'FAIL')
    check('optical_confirmation','NOT_TESTED')
    check('telegram_delivery','NOT_TESTED')
    # Explicitly allowlisted output; never copy input text, paths, logs or secrets.
    return {'firmware':fw,'boot_id':integer(report.get('boot_id')),
            'uptime_sec':integer(report.get('uptime_sec')),'generation':integer(curves.get('generation')),
            'active_profile_checksum':actual,'expected_profile_checksum':expected,'findings':findings}


def inspect_observer(home, expected_hash):
    app = pathlib.Path(home)/'Library'/'Application Support'/'IWAGUMI CIELO Vigilancia'
    script = app/'cielo_observer.py'
    agent = pathlib.Path(home)/'Library'/'LaunchAgents'/'es.iwagumi.cielo.vigilancia.plist'
    out = {'script_present':script.is_file(),'source_sha256':None,'source_matches_repo_4_0_1':None,
           'launch_agent_present':agent.is_file(),'agent_script_matches':None,
           'service_registered':None,'running_version_verified':False,'database_opened':False}
    if script.is_file():
        # Hash only; never import/execute the installed script or inspect its config.
        out['source_sha256'] = hashlib.sha256(script.read_bytes()).hexdigest()
        out['source_matches_repo_4_0_1'] = out['source_sha256']==expected_hash
    if agent.is_file():
        try:
            with agent.open('rb') as f:
                args = plistlib.load(f).get('ProgramArguments',[])
            out['agent_script_matches'] = str(script) in args if isinstance(args,list) else False
        except (OSError,ValueError,plistlib.InvalidFileException):
            out['agent_script_matches'] = False
    if sys.platform=='darwin':
        try:
            p = subprocess.run(['/bin/launchctl','list','es.iwagumi.cielo.vigilancia'],
                               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=4)
            out['service_registered'] = p.returncode==0
        except (OSError,subprocess.TimeoutExpired):
            pass
    return out


def same_snapshot(a, r, b):
    boots = [integer(x.get('boot_id')) for x in (a,r,b)]
    gens = [integer(a.get('profileGeneration')),integer(section(r,'curves').get('generation')),integer(b.get('profileGeneration'))]
    if None in boots or None in gens:
        return 'UNKNOWN'
    return 'PASS' if len(set(boots))==len(set(gens))==1 else 'RETRY_SNAPSHOT_CHANGED'


def main(argv=None):
    p = argparse.ArgumentParser(description='CIELO: comprobacion de solo lectura para Sierra')
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument('--url');g.add_argument('--report')
    p.add_argument('--profile');p.add_argument('--output-dir',default=str(pathlib.Path(__file__).resolve().parent/'RESULTADOS'))
    args = p.parse_args(argv)
    package = pathlib.Path(__file__).resolve().parent
    baseline = json.loads((package/'IDENTIDAD.json').read_text(encoding='utf-8'))
    result = {'schema':'cielo-verificacion-sierra-v1','tool_version':VERSION,'checks_are_read_only':True,
              'observer':inspect_observer(pathlib.Path.home(),baseline['observer_sha256'])}
    try:
        profile = read_json(args.profile) if args.profile else None
        if args.report:
            report = read_json(args.report);consistent='NOT_LIVE_INPUT'
        else:
            base = local_url(args.url)
            a=get_json(base,'/api/status')
            report=get_json(base,'/api/diagnostico/maestro?share=1')
            b=get_json(base,'/api/status');consistent=same_snapshot(a,report,b)
        result['atom']=analyze(report,profile)
        result['snapshot_consistency']=consistent
    except (OSError,ValueError,KeyError,TypeError):
        result['atom']={'result':'READ_FAILED','meaning':'No se obtuvo una entrada valida; no se ha modificado CIELO.'}
    output=pathlib.Path(args.output_dir);output.mkdir(mode=0o700,parents=True,exist_ok=True)
    name='CIELO_COMPROBACION_'+time.strftime('%Y%m%d_%H%M%S')+'_'+secrets.token_hex(3)+'.json'
    target=output/name
    fd=os.open(str(target),os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False,indent=2);f.write('\n')
    print('Comprobacion guardada en RESULTADOS/'+name)
    print('No se han cambiado luces ni servicios. Telegram y luz fisica requieren comprobacion aparte.')
    return 1 if result.get('atom',{}).get('result')=='READ_FAILED' else 0


if __name__=='__main__':
    sys.exit(main())
