"""Independent output oracle for the authorized companion-quality change.

The immutable decoder fixture retains the old golden workloads. The oracle
uses its original renderer and linear sampler with phase quantization, rather
than the production singer_sample helper or its new transition loop.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from support import SDK, MACHINE


def main():
    reference = (MACHINE / 'tests/approved_decoder_engine.h').read_text()
    expected = 'c62d10ebe1e7a8e24dfb61f4138e2d42a1dc215c17b60840591458d1028274eb'
    original = reference.split('Production uses ../engine.cpp.\n', 1)[1]
    if hashlib.sha256(original.encode()).hexdigest() != expected:
        raise RuntimeError('Frozen recital-decoder-001 renderer changed')
    oracle = reference.replace('inline i32 sample(const opxy::i16* table,u32 phase){',
        'static bool oracle_companion=false;\ninline i32 sample(const opxy::i16* table,u32 phase){\n'
        '    if(oracle_companion && table!=sine)phase=(phase+0x00800000u)&0xff000000u;')
    oracle = oracle.replace('const WaveKey wanted=key_for(parameters,pitch[0]);',
        'oracle_companion=false;\n    const WaveKey wanted=key_for(parameters,pitch[0]);')
    oracle = oracle.replace('for(unsigned s=0;s<singers;++s){',
        'for(unsigned s=0;s<singers;++s){\n        oracle_companion=s!=0;')
    compiler = shutil.which(os.environ.get('CXX', 'c++'))
    if not compiler:
        raise RuntimeError('Set CXX to a host C++11 compiler')
    results = {}
    with tempfile.TemporaryDirectory(prefix='recital-companion-oracle-') as scratch:
        scratch = Path(scratch)
        for name, engine in [('approved', reference), ('oracle', oracle), ('candidate', (MACHINE / 'engine.cpp').read_text())]:
            source = scratch / (name + '.cpp')
            source.write_text(engine + '\n#include "companion_workload.h"\n')
            binary = scratch / (name + ('.exe' if os.name == 'nt' else ''))
            subprocess.run([compiler, '-std=c++11', '-O2', '-Wall', '-Werror',
                '-I'+str(SDK / 'sdk/include'), '-I'+str(MACHINE), '-I'+str(MACHINE / 'tests'),
                str(source), '-o', str(binary)], check=True)
            results[name] = json.loads(subprocess.check_output([str(binary)], text=True))
    approved, oracle, candidate = (results[k] for k in ('approved', 'oracle', 'candidate'))
    if candidate != oracle:
        raise RuntimeError('Candidate differs from independent rounded-phase oracle: '+json.dumps(results))
    for key in ('solo_hash', 'control_state_hash', 'samples', 'solo_samples', 'ensemble_samples'):
        if candidate[key] != approved[key]:
            raise RuntimeError('Lead/solo output or non-output rendering state changed: '+key)
    if candidate['ensemble_hash'] == approved['ensemble_hash']:
        raise RuntimeError('Companion quality change was not exercised')
    print('PASS: independent companion oracle; exact lead/solo output and control state; only companion audio changes.')
    print(json.dumps(results, sort_keys=True))


if __name__ == '__main__':
    main()
