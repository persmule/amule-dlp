#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Reject malformed headers while accepting valid empty packets after HELLO."""
from pathlib import Path
import socket
import struct
import sys
import tempfile

from AllSearchIntegrationTest import free_ports, stop_daemon
from KnownHistoryIntegrationTest import start
from ServerTrustIntegrationTest import configure


def receive_exact(peer, size):
    result = b''
    while len(result) < size:
        chunk = peer.recv(size - len(result))
        assert chunk, 'peer closed while receiving a packet'
        result += chunk
    return result


def expect_hello_answer(peer):
    protocol, length, opcode = struct.unpack('<BIB', receive_exact(peer, 6))
    assert (protocol, opcode) == (0xe3, 0x4c), 'expected OP_HELLOANSWER'
    assert 1 <= length <= 65536, f'unexpected HELLOANSWER length: {length}'
    receive_exact(peer, length - 1)


def send_hello(peer):
    # HELLO: hash size, hash, high ID, TCP port, no tags, no server.
    hello = (b'\x10' + bytes.fromhex('0123456789abcdef0123456789abcdef') +
             struct.pack('<IHI', 0x0100007f, peer.getsockname()[1], 0) + bytes(6))
    frame = struct.pack('<BIB', 0xe3, len(hello) + 1, 0x01) + hello
    peer.sendall(frame)
    expect_hello_answer(peer)
    return frame


def run(binary):
    with tempfile.TemporaryDirectory(prefix='amule-packet-receive-') as directory:
        root = Path(directory)
        for name in ('Incoming', 'Temp'):
            (root / name).mkdir()
        ports = free_ports(3, udp_indices=(2,), peer_index=1)
        configure(root, ports, filter_lan=False)
        proc, ec = start(binary, root, ports)
        try:
            for length in (0, 0x7ffffff0, 0xffffffff):
                with socket.create_connection(('127.0.0.1', ports[1]), timeout=10) as peer:
                    peer.settimeout(10)
                    # Send only the header: rejection must precede any payload read.
                    peer.sendall(struct.pack('<BIB', 0xe3, length, 0x01))
                    try:
                        assert peer.recv(1) == b'', f'header {length:#x} was not rejected'
                    except ConnectionResetError:
                        pass
                assert proc.poll() is None, f'header {length:#x} killed the daemon'
            # Before HELLO, master also drops malformed packets. Exercise an
            # established client so this catches the old length-underflow bug.
            for protocol, opcode in ((0xc5, 0x01), (0xe3, 0x4c)):
                with socket.create_connection(('127.0.0.1', ports[1]), timeout=10) as peer:
                    peer.settimeout(10)
                    send_hello(peer)
                    peer.sendall(struct.pack('<BIB', protocol, 0, opcode))
                    try:
                        assert peer.recv(1) == b'', 'post-HELLO length-0 packet was not rejected'
                    except socket.timeout as error:
                        raise AssertionError('post-HELLO length-0 packet left the peer open') from error
                    except ConnectionResetError:
                        pass
                assert proc.poll() is None, 'post-HELLO length-0 packet killed the daemon'
            with socket.create_connection(('127.0.0.1', ports[1]), timeout=10) as peer:
                peer.settimeout(10)
                frame = send_hello(peer)
                # Length 1 contains only an unused eMule opcode, with no payload.
                peer.sendall(struct.pack('<BIB', 0xc5, 1, 0xff))
                # A subsequent HELLO answer proves the empty packet was processed
                # without closing the connection (rather than merely timing out).
                peer.sendall(frame)
                expect_hello_answer(peer)
                assert proc.poll() is None, 'valid empty packet killed the daemon'
        finally:
            ec.sock.close()
            stop_daemon(proc)


if __name__ == '__main__':
    run(str(Path(sys.argv[1]).resolve()))
