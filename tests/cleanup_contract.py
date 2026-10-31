"""Exact original fixture binding and unchanged Cast controls/data/visuals."""
from pathlib import Path
import hashlib,json

ROOT=Path(__file__).resolve().parents[1]
sha=lambda data:hashlib.sha256(data).hexdigest()
# Undo only the authorized identity spelling when checking historical hashes.
original_names=lambda data:data.replace(b'troupe',b'recital')
reference=json.loads((ROOT/'tests/cleanup_reference.json').read_text())
for name,digest in reference['unchanged_files'].items():
    assert sha(original_names((ROOT/name).read_bytes()))==digest,name
definition=json.loads((ROOT/'definition.json').read_text())
assert definition['name']=='Troupe','The displayed machine name is Troupe'
assert definition['id']=='troupe','The internal machine identity is troupe'
# Recover the historical names after the requested numbering; values stay exact.
historical_labels=reference['definition']['parameters'][1]['mapping']['labels']
numbered_labels=[str(number) for number in range(1,11)]
assert definition['parameters'][1]['mapping']['labels']==numbered_labels,'Voices are numbered 1–10 in the existing order'
definition['parameters'][1]['mapping']['labels']=historical_labels
historical_names=dict(zip(numbered_labels,historical_labels))
definition['presets']={historical_names.get(name,name):values for name,values in definition['presets'].items()}
# Preserve the historical record while allowing the explicitly requested renames.
definition['name']=reference['definition']['name']
definition['id']=reference['definition']['id']
definition['resources']['sharedBytes']=reference['definition']['resources']['sharedBytes']
assert definition==reference['definition'],'Only identity, voice/preset labels and declared shared-state size may change'
for name,entry in json.loads((ROOT/'tests/frozen/bindings.json').read_text()).items():
    data=original_names((ROOT/'tests/frozen'/name).read_bytes())
    if entry['transformed']:
        data=(data.decode().replace('namespace approved_cast {\nusing namespace recital;','namespace recital {')
            .replace('recital::sine','sine').replace('#include "cast_core.h"','#include "core.h"')
            .replace('recital::Frame& frame','Frame& frame')).encode()
    assert sha(data)==entry['sha256'],name
print('PASS: Troupe identity; frozen original core/renderers recover exact approved source after undoing the namespace rename; audio and display goldens, controls, presets, banks, generators and Cast visuals preserved.')
