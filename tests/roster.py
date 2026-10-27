"""Roster acceptance, exact preservation and deterministic artwork authoring."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import numpy as np
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spectra import load_spectra, save_spectra, spectra_digest

FOLDER = Path(__file__).resolve().parents[1]


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def load(name):
    spec = importlib.util.spec_from_file_location(name, FOLDER / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    definition = json.loads((FOLDER / 'definition.json').read_text())
    labels = ['Mandy', 'Bart', 'Zelda', 'Pip', 'Emily', 'Luis',
              'Mabel', 'Rocco', 'Tamaki', 'Bertram']
    require(definition['parameters'][1]['mapping']['labels'] == labels, 'Requested paired logical order')
    for index, name in enumerate(labels):
        require(int(definition['presets'][name]['register'] * 10) == index, 'Named presets select their own character')
    require(int(definition['parameters'][1]['default'] * 10) == 5, 'Default remains Luis')
    for name in ('Low Oo', 'Solo Ah'):
        require(int(definition['presets'][name]['register'] * 10) == 5, 'Existing utility presets remain Luis')
    contract = json.loads((FOLDER / 'tests/source_contract.json').read_text())
    for name in ('bank_0.h', 'bank_1.h', 'vocal_spectra_magnitudes.npy', 'vocal_spectra_source_phases.npy', 'display.cpp', 'effort.h'):
        require(hashlib.sha256((FOLDER / name).read_bytes()).hexdigest() == contract['files'][name],
                f'Approved audio/animation preservation: {name}')
    panel = (FOLDER / 'panel.svg.fragment').read_text()
    require(load('generate_panel').generate() == panel, 'Exact artwork regeneration')
    ids = [int(number) for number in re.findall(r'id="troupe_node_(\d+)"', panel)]
    require(sorted(ids) == list(range(156)), 'Every native node has exactly one authored path')
    art = json.loads((FOLDER / 'panel_templates.json').read_text())
    require(len(set(re.sub(r'id="[^"]+"', '', path) for path in art['bodies'])) == 10,
            'Ten distinct authored silhouettes')
    with load_spectra(FOLDER / 'vocal_spectra', allow_pickle=False) as source:
        magnitudes, phases = load('generate_characters').generate(source['magnitudes'], source['source_phases'])
    bank = load('generate_bank')
    for index in range(4):
        bank.generate(magnitudes[index:index + 1], phases[index:index + 1], normalize_new=True)
    with load_spectra(FOLDER / 'additional_spectra', allow_pickle=False) as source:
        # ARM/x86 math libraries differ in the last few float64 bits. The
        # stored sources retain their exact hashes in packing.py; regenerated
        # integer audio must still match every committed sample below.
        relative = np.divide(np.abs(magnitudes - source['magnitudes']), np.abs(source['magnitudes']),
                             out=np.zeros_like(magnitudes), where=source['magnitudes'] != 0)
        require(np.allclose(magnitudes, source['magnitudes'], rtol=32 * np.finfo(np.float64).eps, atol=0)
                and np.array_equal(phases, source['source_phases']),
                f'Roundoff-bounded spectra and exact phases; max relative error {relative.max():.3g}')
    authored = np.array([np.fromstring(line.strip().strip('{},'), sep=',', dtype=np.int32)
                         for part in (2, 3) for line in (FOLDER / f'bank_{part}.h').read_text().splitlines()])
    # Match the authoring sequence: normalize spectra, then generate the bank.
    rebuilt, _, _ = bank.generate(magnitudes, phases)
    require(np.array_equal(rebuilt.reshape(-1, 256), authored),
            'Exact regeneration of every integer waveform sample for the four added characters')
    print('PASS: ten paired presets, preserved approved banks and animation, ten unique silhouettes, complete native artwork and reproducible new voices.')


if __name__ == '__main__':
    main()
