#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run one password scenario in an isolated native Windows console.

The parent launches this worker with CREATE_NEW_CONSOLE and a hidden window.
CONIN$ lets amuleapi exercise ReadConsoleW even though its output is captured.
No desktop keyboard input is sent, and the runner's console is never detached.
"""
import ctypes
from ctypes import wintypes
import json
import msvcrt
import os
import subprocess
import sys
import threading
import time


class KeyEvent(ctypes.Structure):
    _fields_ = [('down', wintypes.BOOL), ('repeat', wintypes.WORD),
                ('virtual_key', wintypes.WORD), ('scan', wintypes.WORD),
                ('character', wintypes.WCHAR), ('control', wintypes.DWORD)]


class Event(ctypes.Union):
    _fields_ = [('key', KeyEvent), ('padding', ctypes.c_byte * 16)]


class InputRecord(ctypes.Structure):
    _fields_ = [('kind', wintypes.WORD), ('event', Event)]


def run(binary, config_dir, scenario):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    user = ctypes.WinDLL('user32', use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.GetConsoleMode.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.GetConsoleMode.restype = wintypes.BOOL
    kernel.SetConsoleMode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.SetConsoleMode.restype = wintypes.BOOL
    kernel.WriteConsoleInputW.argtypes = [wintypes.HANDLE, ctypes.POINTER(InputRecord),
                                        wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
    kernel.WriteConsoleInputW.restype = wintypes.BOOL
    user.VkKeyScanW.argtypes = [wintypes.WCHAR]
    user.VkKeyScanW.restype = wintypes.SHORT
    assert ctypes.sizeof(KeyEvent) == 16 and ctypes.sizeof(InputRecord) == 20

    def checked(result):
        if not result:
            raise ctypes.WinError(ctypes.get_last_error())

    # Read/write access for native console input and synthetic KEY_EVENT records.
    handle = kernel.CreateFileW('CONIN$', 0xC0000000, 3, None, 3, 0, None)
    if handle == wintypes.HANDLE(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    descriptor = msvcrt.open_osfhandle(handle, os.O_RDWR)
    process = None
    try:
        def mode():
            value = wintypes.DWORD()
            checked(kernel.GetConsoleMode(handle, ctypes.byref(value)))
            return value.value

        # Start with echo, line input and processed input enabled so restoration
        # checks cannot pass merely because echo was already disabled.
        before = mode() | 0x7
        checked(kernel.SetConsoleMode(handle, before))
        process = subprocess.Popen([binary, '--config-dir=' + config_dir, '--set-admin-pass-stdin'],
                                   stdin=descriptor, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = bytearray()

        def drain():
            while True:
                byte = process.stderr.read(1)
                if not byte:
                    return
                output.extend(byte)

        reader = threading.Thread(target=drain, daemon=True)
        reader.start()

        def prompt(text):
            deadline = time.monotonic() + 10
            while text not in output:
                assert process.poll() is None, bytes(output)
                assert time.monotonic() < deadline, ('missing prompt', text, bytes(output))
                time.sleep(0.01)
            assert not mode() & 0x4, 'console echo was not disabled'

        def type_keys(text):
            records = (InputRecord * (2 * len(text)))()
            for i, character in enumerate(text):
                for offset, down in enumerate((True, False)):
                    record = records[2 * i + offset]
                    record.kind = 1  # KEY_EVENT
                    if character in ('\x03', '\x1a'):
                        virtual_key, control = (0x43 if character == '\x03' else 0x5A), 0x8
                    elif 0xD800 <= ord(character) <= 0xDFFF:
                        # Inject UTF-16 units directly, as ReadConsoleW receives them.
                        virtual_key, control = 0, 0
                    else:
                        mapping = user.VkKeyScanW(character)
                        assert mapping != -1, 'unmappable console test key'
                        virtual_key = mapping & 0xFF
                        control = 0x10 if mapping & 0x100 else 0  # SHIFT_PRESSED
                    record.event.key = KeyEvent(down, 1, virtual_key, 0, character, control)
            written = wintypes.DWORD()
            checked(kernel.WriteConsoleInputW(handle, records, len(records), ctypes.byref(written)))
            assert written.value == len(records), 'partial console input write'

        prompt(b'New admin password: ')
        secret = b'terminal-secret'
        if scenario == 'cancel':
            type_keys('\x03')  # ReadConsoleW receives Ctrl-C while processed input is off.
        elif scenario == 'eof':
            type_keys('\x1a')  # Explicit Windows end-of-input control character.
        elif scenario == 'invalid-surrogate':
            type_keys('\ud83d\r')
        else:
            emoji = '\ud83d\ude00'
            first = secret.decode()
            second = first if scenario != 'mismatch' else 'other-secret'
            if scenario == 'emoji-delete':
                first += emoji + '\b'
            elif scenario == 'emoji-keep':
                first += emoji
                second += emoji
            type_keys(first + '\r')
            prompt(b'Confirm password: ')
            type_keys(second + '\r')
        result = process.wait(timeout=10)
        if scenario == 'invalid-surrogate':
            assert result == 1, ('invalid UTF-16 must fail cleanly', result)
        reader.join(timeout=5)
        assert not reader.is_alive(), 'stderr reader did not finish'
        assert mode() == before, 'console input mode was not restored'
        assert secret not in bytes(output) + process.stdout.read(), 'password leaked to output'
        print(json.dumps({'returncode': result}))
    finally:
        if process is not None:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            process.stdout.close()
            process.stderr.close()
        os.close(descriptor)


if __name__ == '__main__':
    run(*sys.argv[1:])
