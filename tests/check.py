"""Run machine-owned Python checks without a device or private fixtures."""
from pathlib import Path
import subprocess
import sys

TESTS = Path(__file__).resolve().parent
for name in ('bank_quality', 'packing', 'roster', 'native_voice_indices', 'companion_sampling'):
    print('Checking ' + name, flush=True)
    subprocess.run([sys.executable, str(TESTS / (name + '.py'))], check=True)
print('PASS: Recital data, artwork, source contracts and native adapter checks.')
