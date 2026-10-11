#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Offline regression checks for integration fixture port selection and diagnostics."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import AllSearchIntegrationTest
import LogfilePathIntegrationTest


class ReusingSocket:
    """Model an allocator that reuses a port immediately after socket close."""
    sockets = []

    def __init__(self):
        self.port = None
        self.closed = False
        self.sockets.append(self)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.closed = True

    def bind(self, address):
        assert address == ('127.0.0.1', 0)
        occupied = {sock.port for sock in self.sockets if not sock.closed}
        self.port = next(port for port in range(40000, 40004) if port not in occupied)

    def getsockname(self):
        return '127.0.0.1', self.port


class FailedDaemon:
    def __init__(self, *args, stdout, **kwargs):
        stdout.buffer.write(b'\xff ERROR: cannot open log file\n')
        stdout.flush()

    def wait(self, **kwargs):
        return 1

    def poll(self):
        return 1


class IntegrationFixtureUnitTest(unittest.TestCase):
    def test_ports_remain_reserved_until_all_are_chosen(self):
        ReusingSocket.sockets = []
        with patch.object(AllSearchIntegrationTest.socket, 'socket', ReusingSocket):
            ports = AllSearchIntegrationTest.free_ports(4)
        self.assertEqual(ports, (40000, 40001, 40002, 40003))
        self.assertTrue(all(sock.closed for sock in ReusingSocket.sockets))

    def test_invalid_console_bytes_preserve_startup_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'fixture'
            with patch.object(LogfilePathIntegrationTest.subprocess, 'Popen', FailedDaemon):
                LogfilePathIntegrationTest.run('unused-daemon', root, failure=True,
                                               expected=root / 'logfile')


if __name__ == '__main__':
    unittest.main()
