"""Author four additional synthetic voices from the preserved harmonic spectra.

Only offline spectral shaping: keyboard fundamentals, runtime arithmetic,
polyphony and the six reference voices are unchanged. The named voices are
synthetic character designs, not recordings or demographic models.
"""
from pathlib import Path
import numpy as np
from spectra import load_spectra, save_spectra, spectra_digest

FOLDER = Path(__file__).resolve().parent
NAMES = ('Mabel', 'Rocco', 'Zelda', 'Pip')


def generate(magnitudes, source_phases):
    if magnitudes.shape != (6, 29, 5, 129) or source_phases.shape != (6, 129):
        raise ValueError('Expected the six preserved reference spectra')
    h = np.arange(129, dtype=float)
    safe_h = np.maximum(h, 1)
    pitches = 440 * 2 ** ((np.arange(29) * 3 + 24 - 69) / 12)
    # Base identity, resonance-frequency ratio, spectral tilt, odd emphasis,
    # nasal resonance. No keyboard transposition is applied to any voice.
    recipes = ((1, .88, .28, .18, 0),       # Mabel: dark alto, husky edges.
               (2, .78, .40, .38, 0),       # Rocco: low chest, reedy/gravelly.
               (5, 1.08, -.08, .24, .95),   # Zelda: forward nasal brightness.
               (0, 1.04, .95, -.05, 0))     # Pip: pure, fundamental-led light male.
    result, phases = [], []
    for index, (base, tract, tilt, odd, nasal) in enumerate(recipes):
        authored = np.zeros((29, 5, 129))
        for pitch, f0 in enumerate(pitches):
            for vowel in range(5):
                original = magnitudes[base, pitch, vowel]
                # Undo the broad source slope before shifting the resonance
                # envelope, then restore it at the played fundamental.
                envelope = original * safe_h ** 1.25
                shifted = np.interp(h / tract, h, envelope, left=0, right=0)
                amplitudes = shifted / safe_h ** (1.25 + tilt)
                amplitudes *= 1 + odd * np.where(h.astype(int) % 2, 1, -1)
                if nasal:
                    frequency = h * f0
                    amplitudes *= 1 + nasal * np.exp(-.5 * ((frequency - 1900) / 420) ** 2)
                    amplitudes *= 1 - .35 * np.exp(-.5 * ((frequency - 850) / 180) ** 2)
                if index == 3:
                    # A soft flute-like voice still articulates every vowel.
                    amplitudes[1] += original[1] * .8
                amplitudes[(h * f0 > 19000) | (h == 0) | (h == 128)] = 0
                # Match the base's RMS before the conservative new-voice
                # crest allowance. Every pitch/vowel remains audible.
                norm = np.linalg.norm(amplitudes)
                if not np.isfinite(norm) or norm <= 0:
                    raise ValueError('Empty authored spectrum')
                authored[pitch, vowel] = amplitudes * np.linalg.norm(original) / norm
        result.append(authored)
        phases.append(source_phases[base].copy())
    return np.array(result), np.array(phases)


def main():
    with load_spectra(FOLDER / 'vocal_spectra', allow_pickle=False) as reference:
        magnitudes, phases = generate(reference['magnitudes'], reference['source_phases'])
    # A uniform per-character scale leaves vowel/pitch relative levels intact.
    # New timbres use a little crest headroom; the six originals are untouched.
    from generate_bank import generate as waves
    for character in range(4):
        single = magnitudes[character:character + 1]
        _, _, _ = waves(single, phases[character:character + 1], normalize_new=True)
    save_spectra(FOLDER / 'additional_spectra', magnitudes=magnitudes,
                        source_phases=phases)
    print('Authored Mabel, Rocco, Zelda and Pip; reference spectra untouched.')


if __name__ == '__main__':
    main()
