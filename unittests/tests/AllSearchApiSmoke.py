#!/usr/bin/env python3
"""Run the curl All-search checks against an isolated, offline core/API pair."""
import hashlib
import os
import re
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import time

from AllSearchIntegrationTest import C, connect_daemon, free_port, integer, stored_search


def run(daemon_binary, api_binary, checks):
    with tempfile.TemporaryDirectory(prefix='amule-all-api-') as temporary:
        root = Path(temporary)
        core, api = root / 'core', root / 'api'
        core.mkdir()
        api.mkdir()
        port, http = free_port(), free_port()
        (core / 'amule.conf').write_text(f'''[eMule]
Nick=regression
Port={free_port()}
UDPEnable=1
UDPPort={free_port()}
Address=127.0.0.1
Autoconnect=0
ConnectToKad=1
ConnectToED2K=0
NewVersionCheck=0
Reconnect=0
Serverlist=0
Ed2kServersUrl=
KadNodesUrl=
AddServerListFromServer=0
AddServerListFromClient=0
UPnPEnabled=0
TempDir={core}/Temp
IncomingDir={core}/Incoming
[ExternalConnect]
AcceptExternalConnections=1
ECAddress=127.0.0.1
ECPort={port}
ECPassword={hashlib.md5(b'regression').hexdigest()}
[WebServer]
Enabled=0
''')
        (core / 'nodes.dat').write_bytes(struct.pack('<III', 0, 1, 0))
        # A finished All search exercises discovery and progress serialization
        # without public peers; later checks start Kad with no contacts.
        (core / 'StoredSearches.met').write_bytes(stored_search([]))
        env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root / 'xdg'),
                   LC_ALL='C.UTF-8')
        processes = []
        with (root / 'fixture.log').open('w') as log:
            try:
                daemon = subprocess.Popen([daemon_binary, '-c', str(core)],
                                          env=env, stdout=log, stderr=log)
                processes.append(daemon)
                connect_daemon(daemon, port).sock.close()
                subprocess.run([api_binary, f'--config-dir={api}',
                                '--set-admin-pass=adminpass'], check=True,
                               env=env, stdout=log, stderr=log)
                config = api / 'amuleapi.conf'
                config.write_text(config.read_text().replace(
                    'Password=', 'Password=regression', 1))
                server = subprocess.Popen([api_binary, f'--config-dir={api}',
                                           '--host=127.0.0.1', f'--port={port}',
                                           f'--http-port={http}'],
                                          env=env, stdout=log, stderr=log)
                processes.append(server)
                url = f'http://127.0.0.1:{http}/api/v1'
                # Startup can include expensive password hashing. Let the
                # caller impose a deadline, while detecting process failure.
                while True:
                    if any(proc.poll() is not None for proc in processes):
                        raise RuntimeError('fixture process exited during startup')
                    if subprocess.run(['curl', '-sf', '--max-time', '2', url + '/health'],
                                      stdout=subprocess.DEVNULL).returncode == 0:
                        break
                    time.sleep(0.1)
                # Exercise the command parser and real EC_SEARCH_ALL request too.
                command = Path(daemon_binary).with_name('amulecmd')
                if command.exists():
                    ec = connect_daemon(daemon, port)
                    assert ec.call(C['EC_OP_KAD_START'])[0] == C['EC_OP_NOOP']
                    result = subprocess.run([
                        str(command), '-h', '127.0.0.1', '-p', str(port),
                        '-P', 'regression', '-c',
                        'search all offlinecommandregression --type Arc --avail 2'],
                        check=True, env=env, capture_output=True, text=True)
                    match = re.search(r'Search started \(id (\d+)\)', result.stdout)
                    assert match, result.stdout + result.stderr
                    sid = int(match.group(1))
                    progress = ec.progress(sid)
                    assert progress[C['EC_TAG_SEARCH_LIFECYCLE_KIND']][0] == C['EC_SEARCH_ALL']
                    assert progress[C['EC_TAG_SEARCH_KAD_ACTIVE']][0] == 1
                    ec.call(C['EC_OP_SEARCH_STOP'], [integer(C['EC_TAG_SEARCH_ID'], sid)])
                    ec.sock.close()
                    print('PASS: amulecmd search all starts a combined search with filters')
                subprocess.run(['bash', checks, '--fixture-ready'], check=True,
                               env=dict(env, API=url, ALL_SID='123'))
            except BaseException:
                log.flush()
                print((root / 'fixture.log').read_text()[-12000:], file=sys.stderr)
                raise
            finally:
                for proc in reversed(processes):
                    if proc.poll() is None:
                        proc.terminate()
                    try:
                        proc.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()


if __name__ == '__main__':
    run(*sys.argv[1:])
