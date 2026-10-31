"""Generate authored, phase-coherent vocal waves offline; no stock inputs.

The vocal_spectra arrays retain the original six characters' harmonic magnitudes
and LF-source phases. Vowels and resonances are not revoiced. All waves of
one character share a phase guide, preventing cancellation while interpolating
pitch or vowels. A smooth quadratic phase dispersion bounds crest factor;
it changes no harmonic magnitudes. No FFT or resonator runs in the callback.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
from spectra import load_spectra, save_spectra, spectra_digest

FOLDER = Path(__file__).resolve().parent
CHARACTERS = ('2', '5', '6', '9', '10', '1',
              '7', '8', '3', '4')


def generate(magnitudes, source_phases, normalize_new=False):
    count = magnitudes.shape[0]
    if magnitudes.shape != (count, 29, 5, 129) or source_phases.shape != (count, 129):
        raise ValueError('Unexpected authored spectrum dimensions')
    if not np.all(np.isfinite(magnitudes)) or np.any(magnitudes < 0) or not np.all(np.isfinite(source_phases)):
        raise ValueError('Invalid authored spectrum')
    amplitudes = magnitudes.copy()
    amplitudes[..., 0] = amplitudes[..., -1] = 0
    # Remove only sub-quantization residue from the original i16 wave FFT.
    amplitudes[amplitudes < 8] = 0
    rms = np.sqrt(np.sum(amplitudes[..., 1:-1] ** 2, axis=-1) * 2) / 256
    logs = np.log(np.maximum(rms, 1))
    padded = np.pad(logs, ((0, 0), (1, 1), (0, 0)), mode='edge')
    smooth = (padded[:, :-2] + 2 * padded[:, 1:-1] + padded[:, 2:]) / 4
    # Gentle keyboard-level correction, independently for each vowel/type.
    # Preserve the original spectral ratios and limit any anchor to +/-0.5 dB.
    adjustment_db = np.clip((smooth - logs) * 20 / np.log(10), -.5, .5)
    amplitudes *= 10 ** (adjustment_db[..., None] / 20)
    harmonic_squared = np.arange(129, dtype=float) ** 2
    bank, dispersions = [], []
    for kind in range(count):
        best = None
        # Fixed deterministic offline search. Shared phase avoids destructive
        # interference even across the four corners of a pitch/vowel cell.
        for dispersion in np.linspace(-.08, .08, 321):
            guide = np.exp(1j * (source_phases[kind] + dispersion * harmonic_squared))
            waves = np.fft.irfft(amplitudes[kind] * guide, n=256, axis=-1)
            peak = float(np.max(np.abs(waves)))
            if best is None or peak < best[0]:
                best = (peak, waves, float(dispersion))
        if normalize_new:
            # Authored new voices get one uniform scale across all anchors.
            # Quantization is the same as the preserved reference pipeline.
            scale = min(1., 7800 / best[0])
            best = (best[0] * scale, best[1] * scale, best[2])
            magnitudes[kind] *= scale
        if best[0] > 8191.5:
            raise ValueError('Authored phase guide exceeds the fixed wave peak')
        bank.append(np.rint(best[1]).astype('<i2'))
        dispersions.append(best[2])
    return np.array(bank), adjustment_db, dispersions


def main():
    source = FOLDER / 'vocal_spectra'
    with load_spectra(source, allow_pickle=False) as authored:
        magnitudes, phases = authored['magnitudes'], authored['source_phases']
    with load_spectra(FOLDER / 'additional_spectra', allow_pickle=False) as added:
        magnitudes = np.concatenate((magnitudes, added['magnitudes']))
        phases = np.concatenate((phases, added['source_phases']))
    bank, adjustment, dispersions = generate(magnitudes, phases)
    flat = bank.reshape(1450, 256)
    # Keep the existing bank header, anchor layout and sine table unchanged.
    for part in range(2):
        (FOLDER / f'bank_{part}.h').write_text(''.join(
            '{' + ','.join(map(str, row.tolist())) + '},\n'
            for row in flat[part * 435:(part + 1) * 435]))
    for part in range(2):
        (FOLDER / f'bank_{part + 2}.h').write_text(''.join(
            '{' + ','.join(map(str, row.tolist())) + '},\n'
            for row in flat[870 + part * 290:870 + (part + 1) * 290]))
    report = dict(tables=1450, frames=256, table_bytes=bank.nbytes,
                  peak_i16=int(np.max(np.abs(bank.astype(np.int32)))),
                  sha256=hashlib.sha256(bank.tobytes()).hexdigest(),
                  authored_spectra_sha256=spectra_digest(source),
                  authored_spectra_format='numpy-npy-pair/1',
                  sample_rate=44100, firmware_or_stock_inputs=False,
                  characters=list(CHARACTERS), phase_dispersion=dispersions,
                  anchor_level_adjustment_db=[float(adjustment.min()), float(adjustment.max())],
                  resonance_frequency_basis='original harmonic times anchor fundamental',
                  shared_harmonic_phase_per_character=True)
    (FOLDER / 'bank-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
