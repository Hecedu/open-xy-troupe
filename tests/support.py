"""Portable paths and compiler invocation for machine-owned checks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

MACHINE = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get('OPEN_XY_SDK', str(MACHINE.parent / 'open-xy'))).resolve()

def run_cpp(name, sanitize=False):
    compiler = shutil.which(os.environ.get('CXX', 'c++'))
    if not compiler:
        raise RuntimeError('Set CXX to a host C++11 compiler')
    if not (SDK / 'sdk/include/opxy/audio.hpp').is_file():
        raise RuntimeError('Set OPEN_XY_SDK to the Open-XY SDK checkout')
    with tempfile.TemporaryDirectory(prefix='recital-test-') as scratch:
        binary = Path(scratch) / (name + ('.exe' if os.name == 'nt' else ''))
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if sanitize else []
        subprocess.run([compiler, '-std=c++11', '-O2', '-Wall', '-Werror', *flags,
                        '-I' + str(SDK / 'sdk/include'), '-I' + str(MACHINE),
                        str(MACHINE / 'tests' / (name + '.cpp')), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
