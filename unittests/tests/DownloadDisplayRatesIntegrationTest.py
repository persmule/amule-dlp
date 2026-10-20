#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Download display snapshots must follow two local sources, including idle expiry."""
import fcntl
import hashlib
import os
from pathlib import Path
import select
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib

from AllSearchIntegrationTest import (C, connect_daemon, free_ports, retry_port_collisions,
                                      stop_daemon, tag)
from DaemonConfig import write_amule_conf
from DisplayRatesIntegrationTest import DisplayRateMirror, PartialUpdateEC
from ED2KLinkIntegrationTest import add_link
from UploadRefillIntegrationTest import packet, send


def run(binary, compress=False):
    with tempfile.TemporaryDirectory(prefix='amule-download-rates-') as directory:
        root = Path(directory)
        for name in ('Incoming', 'Temp'):
            (root / name).mkdir()
        ec_port, peer_port = free_ports(2, peer_index=1)
        stopped = threading.Event()
        active = threading.Event()
        active.set()
        ready = [threading.Event(), threading.Event()]
        rate = [128 * 1024]
        errors, listeners, workers, connections = [], [], [], []
        file_hash = hashlib.md5(b'display download fixture').digest()

        def reserve_source_port(address):
            listener = socket.socket()
            listener.bind((address, 0))
            listeners.append(listener)
            return listener

        # Discovered download sources reject 127/8 even with FilterLanIPs=0.
        # Bind both fixture sources to an address on this host; no external peer is used.
        local_address = None
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            for _, interface in socket.if_nameindex():
                try:
                    address = socket.inet_ntoa(fcntl.ioctl(
                        probe, 0x8915, struct.pack('256s', interface.encode()))[20:24])
                except OSError:
                    continue
                if not address.startswith('127.'):
                    local_address = address
                    break
        assert local_address, 'the download fixture requires a local non-loopback IPv4 address'
        sources = [reserve_source_port(local_address), reserve_source_port(local_address)]
        write_amule_conf(root, ec_port, Address=local_address, Port=peer_port, UDPEnable=0, FilterLanIPs=0,
                         MaxDownload=0, MaxUpload=0)

        def serve(listener, index):
            try:
                sock = socket.socket()
                sock.bind((local_address, 0))
                sock.connect((local_address, peer_port))
                connections.append(sock)
                with sock:
                    sock.settimeout(5)
                    pending = []
                    last_sent = 0
                    name = f'display-source-{index}'.encode()
                    tags = b'\x02\x01\x00\x01' + struct.pack('<H', len(name)) + name
                    hello = (b'\x10' + hashlib.md5(name).digest() + socket.inet_aton(local_address)
                             + struct.pack('<HI', listener.getsockname()[1], 1) + tags + bytes(6))
                    send(sock, 0x01, hello)
                    while not stopped.is_set():
                        if select.select([sock], [], [], 0.005)[0]:
                            _, opcode, data = packet(sock)
                            if opcode in (0x4c, 0x58):  # OP_HELLOANSWER / OP_REQUESTFILENAME
                                name = b'display.bin'
                                send(sock, 0x59, file_hash + struct.pack('<H', len(name)) + name)
                            elif opcode == 0x4f:  # OP_SETREQFILEID
                                send(sock, 0x50, file_hash + bytes(2))
                            elif opcode == 0x54:  # OP_STARTUPLOADREQ
                                send(sock, 0x55)
                            elif opcode == 0x56:  # OP_CANCELTRANSFER
                                pending.clear()
                            elif opcode == 0x47:  # OP_REQUESTPARTS
                                offsets = struct.unpack_from('<6I', data, 16)
                                pending.extend([start, end, None, 0] for start, end in zip(offsets[:3], offsets[3:])
                                               if end > start)
                                ready[index].set()
                        now = time.monotonic()
                        if pending and active.is_set() and now - last_sent >= 0.04:
                            start, end, packed, offset = pending[0]
                            count = int(rate[0] * 0.04)
                            if compress:
                                if packed is None:
                                    # Exercise a real compression gain, so counting inflated
                                    # bytes would visibly disagree with the total wire rate.
                                    size = end - start
                                    packed = zlib.compress(os.urandom(size // 2) + bytes(size - size // 2))
                                    pending[0][2] = packed
                                chunk = packed[offset:offset + count]
                                data = file_hash + struct.pack('<II', start, len(packed)) + chunk
                                sock.sendall(struct.pack('<BIB', 0xc5, len(data) + 1, 0x40) + data)
                                pending[0][3] += len(chunk)
                                if pending[0][3] == len(packed):
                                    pending.pop(0)
                            else:
                                count = min(count, end - start)
                                send(sock, 0x46, file_hash + struct.pack('<II', start, start + count) + b'x' * count)
                                pending[0][0] += count
                                if pending[0][0] == end:
                                    pending.pop(0)
                            last_sent = now
            except (EOFError, OSError) as error:
                if not stopped.is_set():
                    errors.append(repr(error))

        env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root / 'xdg'), LC_ALL='C.UTF-8')
        ec = partial = proc = None
        try:
            with (root / 'console.log').open('w') as log:
                proc = subprocess.Popen([binary, '-c', str(root), '--disable-fatal'], env=env,
                                        stdout=log, stderr=log)
            ec = connect_daemon(proc, ec_port, root, peer_port)
            addresses = ','.join(f'{s.getsockname()[0]}:{s.getsockname()[1]}' for s in sources)
            link = f'ed2k://|file|display.bin|8388608|{file_hash.hex()}|sources,{addresses}|/'
            assert add_link(ec, link) == C['EC_OP_NOOP']
            # Sources initiate the connection while the daemon stays offline. Their hello
            # attaches to the source identities in the link; no public network is joined.
            for index, listener in enumerate(sources):
                worker = threading.Thread(target=serve, args=(listener, index), daemon=True)
                worker.start()
                workers.append(worker)

            partial = PartialUpdateEC(ec_port)
            mirrors = [DisplayRateMirror(ec), DisplayRateMirror(partial)]
            deadline = time.monotonic() + 30
            while not all(event.is_set() for event in ready):
                assert time.monotonic() < deadline and not errors, (errors, (root / 'logfile').read_text()[-4000:])
                time.sleep(0.05)
            for speed in (128 * 1024, 256 * 1024):
                rate[0] = speed
                time.sleep(1.3)
                for mirror in mirrors:
                    total, peers, files, downloading = mirror.poll_downloads()
                    assert downloading == 2 and peers > speed, (total, peers, files, downloading)
                    assert abs(peers - files) < speed * 0.25, ('file/peer rates disagree', peers, files)
                    assert abs(total - peers) < speed * 0.5, ('total/peer rates disagree', total, peers)
                    assert all(fields[C['EC_TAG_PARTFILE_SPEED_SMOOTHED']] > 0
                               for fields in mirror.files.values()), 'missing smoothed ETA rate'
            active.clear()
            time.sleep(1.5)
            for mirror in mirrors:
                total, peers, files, downloading = mirror.poll_downloads()
                assert downloading == 2, 'idle expiry lost the source connections'
                assert sum(peer.get(C['EC_TAG_CLIENT_DOWNLOAD_STATE']) == 0
                           for peer in mirror.peers.values()) == 2, 'sources left DS_DOWNLOADING'
                assert total < 4096 and peers == files == 0, ('stale download rates', total, peers, files)
                assert all(fields[C['EC_TAG_PARTFILE_SPEED_SMOOTHED']] > 0
                           for fields in mirror.files.values()), 'idle display overwrote the smoothed ETA rate'
            # Pause clears both rates even though no further partfile Process ticks run.
            assert ec.call(C['EC_OP_PARTFILE_PAUSE'], [tag(C['EC_TAG_PARTFILE'], file_hash, 9)])[0] == C['EC_OP_NOOP']
            for mirror in mirrors:
                _, peers, files, downloading = mirror.poll_downloads()
                assert peers == files == downloading == 0, (peers, files, downloading)
                assert all(fields[C['EC_TAG_PARTFILE_SPEED_SMOOTHED']] == 0
                           for fields in mirror.files.values()), 'paused ETA rate did not reset'
            assert not errors, errors
            print(f'PASS (compressed={compress}): one-second total, two-source and file download rates, step change and idle expiry; legacy/partial EC')
        finally:
            stopped.set()
            for sock in connections + listeners:
                sock.close()
            for worker in workers:
                worker.join(timeout=1)
            if partial:
                partial.sock.close()
            if ec:
                ec.sock.close()
            if proc:
                stop_daemon(proc)


if __name__ == '__main__':
    for compressed in (False, True):
        retry_port_collisions(lambda: run(str(Path(sys.argv[1]).resolve()), compressed))
