"""Guard the specific first-boot stack regression; physical boot remains required."""
from pathlib import Path
main=Path('src/main.cpp').read_text();tls=Path('src/cielo_https_proxy.cpp').read_text()
assert 'CIELO_LOOP_STACK_BYTES=16384' in main
assert 'SET_LOOP_TASK_STACK_SIZE(CIELO_LOOP_STACK_BYTES)' in main
assert 'loop_stack_min_free_bytes' in main
provision=tls[tls.index('static bool provisionTls'):tls.index('static bool sendAll')]
assert 'std::unique_ptr<ProvisionBuffers,ClearProvisionBuffers>' in provision
assert 'new(std::nothrow) ProvisionBuffers{}' in provision
assert 'unsigned char keyPem[' not in provision and 'unsigned char certPem[' not in provision
assert 'mbedtls_pk_write_key_pem(&pk,pem->key,sizeof(pem->key))' in provision
assert 'if(!pem)return false;' in provision
assert 'delete p;' in tls and 'bytes[i]=0' in tls
assert 'cfg.httpd.stack_size=12288' in tls  # a different task; not the crashing loopTask
print('PASS first-boot stack regression: explicit loop stack and bounded heap PEM workspace')

assert 'https_begin(prefs)' not in main
assert 'LAN_INICIADA' in main
