"""Exact original fixture binding and unchanged Cast controls/data/visuals."""
from pathlib import Path
import hashlib,json

ROOT=Path(__file__).resolve().parents[1]
sha=lambda data:hashlib.sha256(data).hexdigest()
reference=json.loads((ROOT/'tests/cleanup_reference.json').read_text())
for name,digest in reference['unchanged_files'].items():
    assert sha((ROOT/name).read_bytes())==digest,name
definition=json.loads((ROOT/'definition.json').read_text())
definition['resources']['sharedBytes']=reference['definition']['resources']['sharedBytes']
assert definition==reference['definition'],'Only the declared shared-state size may change'
for name,entry in json.loads((ROOT/'tests/frozen/bindings.json').read_text()).items():
    data=(ROOT/'tests/frozen'/name).read_bytes()
    if entry['transformed']:
        data=(data.decode().replace('namespace approved_cast {\nusing namespace recital;','namespace recital {')
            .replace('recital::sine','sine').replace('#include "cast_core.h"','#include "core.h"')
            .replace('recital::Frame& frame','Frame& frame')).encode()
    assert sha(data)==entry['sha256'],name
print('PASS: frozen original core/renderers recover exact approved source; audio and display goldens unchanged; controls, presets, banks, generators, Cast visuals and layout identity preserved.')
