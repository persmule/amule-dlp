#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify API and command-line guards against a loopback EC peer without All support."""
import json, os, socketserver, struct, subprocess, sys, tempfile, threading, time, urllib.request, zlib
from pathlib import Path
from AllSearchIntegrationTest import C, exact, free_port, integer, string
requests = []
class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            while True:
                flags, size = struct.unpack('!II', exact(self.request, 8))
                body = exact(self.request, size)
                if flags & 1: body = zlib.decompress(body)
                op = body[0]
                requests.append(op)
                if op == C['EC_OP_AUTH_REQ']:
                    reply, tags = C['EC_OP_AUTH_SALT'], [integer(C['EC_TAG_PASSWD_SALT'], 123)]
                elif op == C['EC_OP_AUTH_PASSWD']:
                    reply, tags = C['EC_OP_AUTH_OK'], [string(C['EC_TAG_SERVER_VERSION'], 'legacy-fixture')]
                else:
                    reply, tags = C['EC_OP_NOOP'], []
                payload = bytes([reply]) + struct.pack('!H', len(tags)) + b''.join(tags)
                self.request.sendall(struct.pack('!II', 0x20, len(payload)) + payload)
        except (EOFError, OSError): pass
class Server(socketserver.ThreadingTCPServer):
    daemon_threads = True

def http(url, body=None, token=None):
    headers = {'Content-Type':'application/json'}
    if token: headers['Authorization'] = 'Bearer '+token
    request = urllib.request.Request(url, None if body is None else json.dumps(body).encode(), headers)
    try: response = urllib.request.urlopen(request, timeout=3)
    except urllib.error.HTTPError as response: return response.code, json.load(response)
    with response: return response.status, json.load(response)


def run(api, cmd):
    with tempfile.TemporaryDirectory(prefix='amule-missing-capability-') as tmp:
        cfg = Path(tmp)
        env = dict(os.environ, HOME=tmp, XDG_CONFIG_HOME=tmp+'/xdg', LC_ALL='C.UTF-8')
        subprocess.run([api, '--config-dir='+tmp, '--set-admin-pass=adminpass'], env=env, check=True, stdout=subprocess.DEVNULL)
        config=cfg/'amuleapi.conf'
        config.write_text(config.read_text().replace('Password=', 'Password=regression', 1))
        with Server(('127.0.0.1', 0), Handler) as legacy:
            thread=threading.Thread(target=legacy.serve_forever, daemon=True); thread.start()
            port=legacy.server_address[1]; hp=free_port(); url=f'http://127.0.0.1:{hp}/api/v1'
            with (cfg/'api.log').open('w') as log:
                proc=subprocess.Popen([api, '--config-dir='+tmp, '--host=127.0.0.1', '--port='+str(port), '--http-port='+str(hp), '--disable-ec-encryption'], env=env, stdout=log, stderr=log)
                try:
                    deadline=time.monotonic()+30
                    while time.monotonic()<deadline:
                        assert proc.poll() is None, (cfg/'api.log').read_text()
                        try:
                            if http(url+'/health')[1].get('ec_connected'): break
                        except OSError: pass
                        time.sleep(.1)
                    else: raise AssertionError((cfg/'api.log').read_text())
                    token=http(url+'/auth/login?include_token=true', {'password':'adminpass'})[1]['token']
                    status, result=http(url+'/search', {'query':'legacyguard','type':'all'}, token)
                    assert status==503 and result['error']['code']=='ec_unsupported', (status, result)
                    print('PASS: API returns 503 ec_unsupported without All capability')
                    status, result=http(url+'/status', token=token)
                    assert status==200 and result['search_all_supported'] is False, (status,result)
                    print('PASS: REST reports absent All capability as false')
                    out=subprocess.run([cmd, '-h','127.0.0.1','-p',str(port),'-P','regression','--disable-ec-encryption','-c','search all legacyguard'], env=env, capture_output=True, text=True, timeout=15)
                    assert 'does not support All searches' in out.stdout+out.stderr, out
                    assert C['EC_OP_SEARCH_START'] not in requests, requests
                    print('PASS: amulecmd reports unsupported All without sending SEARCH_START')
                except BaseException:
                    log.flush();print((cfg/'api.log').read_text(),file=sys.stderr);raise
                finally:
                    proc.terminate();proc.wait(timeout=10);legacy.shutdown()


if __name__ == "__main__":
    run(*sys.argv[1:])
