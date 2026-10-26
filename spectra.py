"""Read authored spectral source arrays without archive or pickle payloads."""
from contextlib import contextmanager
from pathlib import Path
import hashlib
import numpy as np

@contextmanager
def load_spectra(prefix, *, allow_pickle=False):
    if allow_pickle:
        raise ValueError('Authored spectra never use pickle')
    prefix = Path(prefix)
    yield {key: np.load(prefix.with_name(prefix.name + '_' + key + '.npy'), allow_pickle=False)
           for key in ('magnitudes', 'source_phases')}

def save_spectra(prefix, *, magnitudes, source_phases):
    prefix = Path(prefix)
    for key, values in (('magnitudes', magnitudes), ('source_phases', source_phases)):
        np.save(prefix.with_name(prefix.name + '_' + key + '.npy'), values, allow_pickle=False)

def spectra_digest(prefix):
    prefix = Path(prefix)
    digest = hashlib.sha256()
    for key in ('magnitudes', 'source_phases'):
        digest.update(prefix.with_name(prefix.name + '_' + key + '.npy').read_bytes())
    return digest.hexdigest()
