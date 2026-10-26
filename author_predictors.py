"""Author bounded integer predictors; residuals preserve every original sample.

This offline fit is an input-authoring step. pack_bank.py consumes the frozen
integer predictors.json without refitting, so packed regeneration is exact
and independent of floating-point least-squares implementations.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
from pack_bank import authored_bank

FOLDER = Path(__file__).resolve().parent


def main():
    bank = authored_bank()
    rows, accepted = [], [0, 0, 0]
    for wave in bank:
        row = {}
        for order in (2, 3, 4):
            history = np.stack([wave[order - index - 1:255 - index] for index in range(order)], axis=1)
            coefficients = np.rint(np.linalg.lstsq(history.astype(float), wave[order:].astype(float), rcond=None)[0] * 16384).astype(np.int64)
            if np.max(np.abs(coefficients)) < 131072 and np.sum(np.abs(coefficients)) <= 15 * 16384:
                row[str(order)] = coefficients.tolist()
                accepted[order - 2] += 1
        rows.append(row)
    data = dict(schema=1, shift=14, coefficient_bits=18, absolute_sum_limit=15 * 16384,
                bank_sha256=hashlib.sha256(bank.astype('<i2').tobytes()).hexdigest(), predictors=rows)
    (FOLDER / 'predictors.json').write_text(json.dumps(data, separators=(',', ':')) + '\n')
    print('Authored bounded integer predictors:', accepted, '; no waveform samples changed.')


if __name__ == '__main__':
    main()
