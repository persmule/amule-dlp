#!/usr/bin/env python3
# Copyright (c) 2026 aMule Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise connector config paths offline, with every output in a temporary directory."""
import configparser
import os
from pathlib import Path
import subprocess
import sys
import tempfile


PASSWORD = '0123456789abcdef0123456789abcdef'


def run(binary, root, *args, expected_error=None, expect_failure=False):
    env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(root / 'xdg'))
    result = subprocess.run([binary, *args], cwd=root, env=env,
                            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors='replace', timeout=15)
    if expect_failure:
        assert result.returncode == 1, (args, result.returncode, result.stdout)
    elif expected_error is None:
        assert result.returncode == 0, (args, result.stdout)
    else:
        assert expected_error in result.stdout, (args, result.stdout)
    return result.stdout


def read_config(path):
    config = configparser.ConfigParser(interpolation=None)
    text = path.read_text()
    # wxFileConfig also writes ungrouped keys (Locale) before the first section.
    config.read_string(text[text.index('['):])
    return config


def check_path(binary, root, argument, expected):
    source = root / 'amule.conf'
    source.write_text(f'[ExternalConnect]\nECPort=48123\nECPassword={PASSWORD}\n'
                      '[WebServer]\nPort=48125\n')
    args = [] if argument is None else [f'--config-file={argument}']
    run(binary, root, *args, f'--create-config-from={source.as_posix()}')
    config = read_config(expected)
    assert config['EC']['Port'] == '48123', (expected, config.items('EC'))
    assert config['EC']['Password'].lower() == PASSWORD, expected
    if os.name != 'nt':
        assert expected.stat().st_mode & 0o777 == 0o600, expected
    if Path(binary).stem == 'amuleweb':
        assert config['WebServer']['Port'] == '48125', expected
    # Read the same selected file, then save it without starting a connection.
    config['EC']['Host'] = 'config-path-test.invalid'
    with expected.open('w') as stream:
        config.write(stream)
    if os.name != 'nt':
        expected.chmod(0o644)
    run(binary, root, *args, '--write-config', '--port=48124')
    config = read_config(expected)
    assert config['EC']['Port'] == '48124', expected
    assert config['EC']['Host'] == 'config-path-test.invalid', expected
    if os.name != 'nt':
        assert expected.stat().st_mode & 0o777 == 0o600, expected


def main():
    binaries = [str(Path(arg).resolve()) for arg in sys.argv[1:]]
    assert binaries, 'Pass amulecmd and/or amuleweb paths'
    with tempfile.TemporaryDirectory(prefix='amule-config-path-') as scratch:
        for index, binary in enumerate(binaries):
            base = Path(scratch).resolve() / str(index)
            # The portable marker prevents touching the real user config on Windows/macOS.
            cases = ['default', 'bare', 'absolute', 'dot-relative', 'parent-relative', 'subdirectory',
                     'new-directory']
            if os.name == 'nt':
                cases += ['backslash', 'drive-relative', 'root-relative']
            if sys.platform.startswith('linux'):
                cases += ['home', 'user-default', 'user-bare', 'new-user-directory']
            for case in cases:
                root = base / case
                root.mkdir(parents=True)
                portable = root / 'config'
                portable.mkdir()
                (portable / 'remote.conf').write_text('')
                expected = root / 'selected.conf'
                if case == 'default':
                    argument, expected = None, portable / 'remote.conf'
                elif case == 'bare':
                    argument, expected = 'custom.conf', portable / 'custom.conf'
                elif case == 'absolute':
                    argument = expected.as_posix()
                elif case == 'dot-relative':
                    argument = './selected.conf'
                elif case == 'parent-relative':
                    argument, expected = '../selected.conf', root.parent / 'selected.conf'
                elif case == 'subdirectory':
                    (root / 'subdir').mkdir()
                    argument, expected = 'subdir/selected.conf', root / 'subdir/selected.conf'
                elif case in ('new-directory', 'new-user-directory'):
                    argument = './new/nested/selected.conf'
                    expected = root / 'new/nested/selected.conf'
                    if case == 'new-user-directory':
                        (portable / 'remote.conf').unlink()
                elif case == 'backslash':
                    argument = str(expected)
                elif case == 'drive-relative':
                    argument = root.drive + 'selected.conf'
                elif case == 'root-relative':
                    argument = str(expected)[len(root.drive):]
                elif case == 'home':
                    argument = '~/selected.conf'
                else:
                    (portable / 'remote.conf').unlink()
                    argument = None if case == 'user-default' else 'custom.conf'
                    expected = root / '.aMule' / (argument or 'remote.conf')
                check_path(binary, root, argument, expected)
                if case == 'new-user-directory':
                    assert not (root / '.aMule').exists(), 'Created an unrelated default directory'
                print(f'{Path(binary).name}: {case} passed', flush=True)

            root = base / 'new-write-config'
            root.mkdir()
            (root / 'config').mkdir()
            (root / 'config/remote.conf').write_text('')
            run(binary, root, '--config-file=./new/nested/selected.conf', '--write-config',
                '--host=config-path-test.invalid', '--port=48124')
            config = read_config(root / 'new/nested/selected.conf')
            assert config['EC']['Port'] == '48124'
            assert config['EC']['Host'] == 'config-path-test.invalid'
            print(f'{Path(binary).name}: new write-config passed', flush=True)

            # A regular file cannot serve as the selected file's parent directory.
            for option in ('create-config-from', 'write-config'):
                root = base / ('failed-' + option)
                root.mkdir()
                (root / 'config').mkdir()
                (root / 'config/remote.conf').write_text('')
                blocker = root / 'blocker'
                blocker.write_text('keep this file')
                source = root / 'amule.conf'
                source.write_text('[ExternalConnect]\nECPort=48123\n')
                action = (f'--create-config-from={source.as_posix()}'
                          if option == 'create-config-from' else '--write-config')
                run(binary, root, '--config-file=./blocker/selected.conf', action,
                    expect_failure=True)
                assert blocker.read_text() == 'keep this file'
                print(f'{Path(binary).name}: failed {option} reported', flush=True)

                # Also exercise Flush() failing after a successful parent-directory check.
                (root / 'directory-target').mkdir()
                run(binary, root, '--config-file=./directory-target', action,
                    expect_failure=True)
                assert (root / 'directory-target').is_dir()

            if Path(binary).stem == 'amuleweb':
                root = base / 'amule-config-file'
                root.mkdir()
                source = root / 'amule.conf'
                template = 'config-path-test-missing-template'
                source.write_text(f'[WebServer]\nTemplate={template}\n')
                # Stop at template validation, before any EC or HTTP connection is opened.
                for argument in [source.as_posix(), './amule.conf']:
                    run(binary, root, f'--amule-config-file={argument}',
                        expected_error=f'Cannot find template: {template}')
                print('amuleweb: amule-config-file passed', flush=True)


if __name__ == '__main__':
    main()
