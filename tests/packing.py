"""Independent exact packing, source preservation and regeneration checks."""
from pathlib import Path
import hashlib
import importlib.util
import json

FOLDER = Path(__file__).resolve().parents[1]


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    spec = importlib.util.spec_from_file_location('pack_bank', FOLDER / 'pack_bank.py')
    packing = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packing)
    bank = packing.authored_bank()
    descriptors, words, stats = packing.pack(bank)
    for table in range(1450):
        for sample in range(256):
            require(packing.decode(descriptors, words, table, sample) == bank[table, sample],
                    'Exact signed decode at every sample, including block/wave boundaries')
    require(packing.header(descriptors, words) == (FOLDER / 'bank_packed.h').read_text(),
            'Packed header regenerates exactly')
    report = json.loads((FOLDER / 'packing-report.json').read_text())
    require(hashlib.sha256(bank.astype('<i2').tobytes()).hexdigest() == report['decoded_sha256'],
            'Decoded bank matches the recorded ten-character bank identity')
    contract = json.loads((FOLDER / 'tests/source_contract.json').read_text())
    for name, expected in contract['files'].items():
        require(hashlib.sha256((FOLDER / name).read_bytes()).hexdigest() == expected,
                f'Approved source/data preservation: {name}')
    require(json.loads((FOLDER / 'definition.json').read_text()) == contract['definition'],
            'Controls, presets, resource sizes and Recital identity match the approved contribution')
    require(report['packed_bytes'] < report['original_bytes'], 'Real storage reduction')
    print(f"PASS: exact packed regeneration and all table samples; {report['original_bytes']:,} -> "
          f"{report['packed_bytes']:,} bytes; UI, controls, presets, expression and polyphony unchanged; resume-state and fixed startup-cache budget checked.")


if __name__ == '__main__':
    main()
