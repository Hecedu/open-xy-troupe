"""Validate spectral identity and phase-coherent interpolation independently."""
from pathlib import Path
import importlib.util
import numpy as np
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spectra import load_spectra, save_spectra, spectra_digest

folder = Path(__file__).resolve().parents[1]


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    bank = np.array([np.fromstring(line.strip().strip('{},'), sep=',')
                     for part in range(4) for line in (folder / f'bank_{part}.h').read_text().splitlines()])
    require(bank.shape == (1450, 256), 'Fixed bank dimensions')
    bank = bank.reshape(10, 29, 5, 256)
    require(np.max(np.abs(bank)) <= 8192, 'Anchor and convex-morph peak bound')
    require(np.max(np.abs(np.mean(bank, axis=-1))) < 1, 'No DC skirt')
    spectra = np.fft.rfft(bank)
    amplitudes = np.abs(spectra)
    with load_spectra(folder / 'vocal_spectra', allow_pickle=False) as source:
        original = source['magnitudes'].copy()
        phases = source['source_phases'].copy()
    with load_spectra(folder / 'additional_spectra', allow_pickle=False) as source:
        original = np.concatenate((original, source['magnitudes']))
        phases = np.concatenate((phases, source['source_phases']))
    original[..., 0] = original[..., -1] = 0
    observed = amplitudes.copy()
    observed[..., 0] = observed[..., -1] = 0
    correlation = np.sum(original * observed, axis=-1) / np.sqrt(np.sum(original ** 2, axis=-1) * np.sum(observed ** 2, axis=-1))
    require(np.min(correlation) > .999999, 'Every character/vowel/pitch retains its harmonic balance')
    levels = 10 * np.log10(np.sum(observed ** 2, axis=-1) / np.sum(original ** 2, axis=-1))
    require(np.max(np.abs(levels)) < .505, 'Only bounded half-dB anchor calibration')
    # Significant harmonics retain their relative amplitudes within 0.05 dB;
    # allow the new independent i16 rounding (<=128 FFT units per bin).
    scale = np.sqrt(np.sum(observed ** 2, axis=-1) / np.sum(original ** 2, axis=-1))
    significant = original > np.max(original, axis=-1, keepdims=True) * .02
    error = np.abs(20 * np.log10(np.maximum(observed, 1) / np.maximum(original * scale[..., None], 1)))
    require(np.max(error[significant]) < .05, 'Resonance shapes and character spectra preserved')
    require(np.max(np.abs(observed - original * scale[..., None])) < 128, 'Only quantization-scale harmonic changes')
    worst_loss = 0
    for left, right in ((spectra[:, :, :-1], spectra[:, :, 1:]), (spectra[:, :-1], spectra[:, 1:])):
        for fraction in (.125, .25, .5, .75, .875):
            expected = np.linalg.norm((1 - fraction) * np.abs(left) + fraction * np.abs(right), axis=-1)
            actual = np.linalg.norm((1 - fraction) * left + fraction * right, axis=-1)
            loss = 20 * np.log10(actual / expected)
            worst_loss = min(worst_loss, float(np.min(loss)))
    require(worst_loss > -.002, 'No destructive vowel/pitch blend holes')
    for x in (.25, .5, .75):
        for y in (.25, .5, .75):
            corners = (spectra[:, :-1, :-1], spectra[:, :-1, 1:], spectra[:, 1:, :-1], spectra[:, 1:, 1:])
            weights = ((1-x)*(1-y), (1-x)*y, x*(1-y), x*y)
            actual = np.linalg.norm(sum(c*w for c, w in zip(corners, weights)), axis=-1)
            expected = np.linalg.norm(sum(np.abs(c)*w for c, w in zip(corners, weights)), axis=-1)
            require(np.min(20*np.log10(actual/expected)) > -.002, 'Coherent four-corner pitch/vowel morph')
    spec = importlib.util.spec_from_file_location('vocal_bank_generator', folder / 'generate_bank.py')
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    rebuilt, _, _ = generator.generate(original, phases)
    require(np.array_equal(bank, rebuilt), 'Authored spectra reproduce checked waves exactly')
    print(f'PASS: 1450 anchors; min spectral correlation {correlation.min():.9f}; max anchor change {np.max(np.abs(levels)):.4f} dB; worst blend cancellation {worst_loss:.6f} dB; exact regeneration.')


if __name__ == '__main__':
    main()
