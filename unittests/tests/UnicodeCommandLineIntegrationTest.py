#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise real daemon argument decoding without reaching network initialization."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = str(Path(sys.argv[1]).resolve())
    if sys.platform == 'win32':
        # Console chcp does not change the ANSI code page used by the CRT's argv.
        print(f'Windows ANSI code page: {ctypes.windll.kernel32.GetACP()}', flush=True)
    with tempfile.TemporaryDirectory(prefix='amule-argv-') as scratch:
        base = Path(scratch)
        for index, name in enumerate(('profile ascii', 'prof д', 'profile 漢 😀')):
            root = base / name
            root.mkdir()
            # The existing directory forces a predictable exit in InitCommon, before
            # preferences, registry self-healing, worker threads or sockets are initialized.
            blocked_log = base / f'log д {index}'
            blocked_log.mkdir()
            profile = str(root) + (os.sep if index == 2 else '')
            config_args = ([f'--config-dir={profile}'] if index == 1 else ['-c', profile])
            link = 'ed2k://|file|file д 漢 😀.bin|1|0123456789ABCDEF0123456789ABCDEF|/'
            print(f'Unicode arguments case {index + 1}: starting ({ascii(profile)})', flush=True)
            try:
                result = subprocess.run(
                    [binary, *config_args, '--log-file', str(blocked_log), link],
                    cwd=base, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
            except subprocess.TimeoutExpired as error:
                output = (error.output or b'').decode('utf-8', errors='replace')
                raise AssertionError(
                    f'Unicode arguments case {index + 1}: daemon exceeded 15 seconds\n{output}'
                ) from error
            output = result.stdout.decode('utf-8', errors='replace')
            assert result.returncode != 0, output
            assert 'ERROR: invalid log file path' in output, output
            assert str(blocked_log) in output, output
            links = root / 'ED2KLinks'
            assert links.is_file(), f'Profile argument was not preserved: {root}\n{output}'
            assert links.read_text(encoding='utf-8-sig').splitlines() == [link + ':0'], output
            print(f'Unicode arguments case {index + 1}: passed', flush=True)


if __name__ == '__main__':
    main()
