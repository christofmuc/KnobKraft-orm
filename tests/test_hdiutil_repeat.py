import os
from pathlib import Path
import shutil
import subprocess

import pytest


SCRIPT = Path(__file__).resolve().parents[1] / 'The-Orm' / 'hdiutil_repeat.sh'
BASH = (str(Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/bin/bash.exe')
        if os.name == 'nt' else shutil.which('bash'))
pytestmark = pytest.mark.skipif(not BASH or not Path(BASH).is_file(), reason='bash is required')


def run_hdiutil(tmp_path, command, failures, force_failures=0):
    stub = tmp_path / 'hdiutil'
    stub.write_text('''#!/usr/bin/env bash
printf '%s\\n' "$*" >> "$CALL_LOG"
if [ "$1" = info ]; then exit 0; fi
if [[ " $* " == *" -force "* ]]; then
  count_file="$FORCE_COUNT"
  failures="$FORCE_FAILURES"
else
  count_file="$NORMAL_COUNT"
  failures="$NORMAL_FAILURES"
fi
count=0
if [ -f "$count_file" ]; then read -r count < "$count_file"; fi
count=$((count+1))
printf '%s\\n' "$count" > "$count_file"
[ "$count" -gt "$failures" ]
''', encoding='utf-8', newline='\n')
    stub.chmod(0o755)
    sleep = tmp_path / 'sleep'
    sleep.write_text('#!/usr/bin/env bash\nexit 0\n', encoding='utf-8', newline='\n')
    sleep.chmod(0o755)
    env = os.environ.copy()
    env.update(CALL_LOG=str(tmp_path / 'calls'), NORMAL_COUNT=str(tmp_path / 'normal'),
               FORCE_COUNT=str(tmp_path / 'force'), NORMAL_FAILURES=str(failures),
               FORCE_FAILURES=str(force_failures))
    wrapper = '''stub_dir="$1"
case "$stub_dir" in [A-Za-z]:*) stub_dir="$(cygpath -u "$stub_dir")";; esac
export PATH="$stub_dir:$PATH"
shift
exec bash "$@"
'''
    result = subprocess.run([BASH, '-c', wrapper, '--', str(tmp_path), str(SCRIPT),
                             command, '/Volumes/Test image'], env=env, capture_output=True,
                            text=True, timeout=20)
    return result, (tmp_path / 'calls').read_text().splitlines()


def test_detach_recovers_from_transient_forced_failure(tmp_path):
    result, calls = run_hdiutil(tmp_path, 'detach', 10, 2)
    assert result.returncode == 0, result.stderr
    assert calls == ['detach /Volumes/Test image'] * 10 + ['detach /Volumes/Test image -force'] * 3


def test_successful_normal_detach_never_forces(tmp_path):
    result, calls = run_hdiutil(tmp_path, 'detach', 2)
    assert result.returncode == 0, result.stderr
    assert calls == ['detach /Volumes/Test image'] * 3


def test_permanent_detach_failure_remains_a_failure(tmp_path):
    result, calls = run_hdiutil(tmp_path, 'detach', 10, 10)
    assert result.returncode == 1
    assert len(calls) == 21
    assert calls[-1] == 'info'
    assert 'Failed to detach temporary image' in result.stderr


def test_create_retries_without_forced_detach(tmp_path):
    result, calls = run_hdiutil(tmp_path, 'create', 2)
    assert result.returncode == 0, result.stderr
    assert calls == ['create /Volumes/Test image'] * 3
