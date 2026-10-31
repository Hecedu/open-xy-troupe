"""Exact original fixture binding and unchanged Cast controls/data/visuals."""
from pathlib import Path
import ast,hashlib,json,re

ROOT=Path(__file__).resolve().parents[1]
sha=lambda data:hashlib.sha256(data).hexdigest()
# Undo only the authorized identity spelling when checking historical hashes.
original_names=lambda data:data.replace(b'troupe',b'recital')
reference=json.loads((ROOT/'tests/cleanup_reference.json').read_text())
def canonical_ast(value):
    if isinstance(value,ast.AST):
        # Include optional fields explicitly; ast.dump defaults vary by Python.
        fields={name:canonical_ast(getattr(value,name,None)) for name in value._fields
                if name!='type_params' or getattr(value,name,None)}
        return [type(value).__name__,fields]
    if isinstance(value,list):
        return [canonical_ast(item) for item in value]
    return repr(value)

def without_labels(name,data):
    text=original_names(data).decode()
    if name=='expression.h':
        return re.sub(r'\s+','',re.sub(r'//[^\n]*','',text)).encode()
    tree=ast.parse(text)
    if tree.body and isinstance(tree.body[0],ast.Expr) and isinstance(tree.body[0].value,ast.Constant) and isinstance(tree.body[0].value.value,str):
        tree.body[0].value=ast.Constant(None)
    for node in ast.walk(tree):
        if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id in ('CHARACTERS','NAMES') for t in node.targets):
            expected=('2','5','6','9','10','1','7','8','3','4') if name=='generate_bank.py' else ('7','8','3','4')
            assert ast.literal_eval(node.value)==expected,'Generator labels retain internal voice order'
            node.value=ast.Constant(None)
        if isinstance(node,ast.Call) and isinstance(node.func,ast.Name) and node.func.id=='print' and node.args and isinstance(node.args[0],ast.Constant) and isinstance(node.args[0].value,str) and node.args[0].value.startswith('Authored ') and node.args[0].value.endswith('reference spectra untouched.'):
            node.args[0]=ast.Constant(None)
    return json.dumps(canonical_ast(tree),sort_keys=True,separators=(',',':')).encode()
for name,digest in reference['unchanged_files'].items():
    data=(ROOT/name).read_bytes()
    if name in reference['numbered_label_sources']:
        numbered=reference['numbered_label_sources'][name]
        assert sha(data)==numbered['sha256'],name
        assert sha(without_labels(name,data))==numbered['algorithm_sha256'],name
    else:
        assert sha(original_names(data))==digest,name
definition=json.loads((ROOT/'definition.json').read_text())
assert definition['name']=='Troupe','The displayed machine name is Troupe'
assert definition['id']=='troupe','The internal machine identity is troupe'
# The comparison definition records the explicit label-only migration.
numbered_labels=[str(number) for number in range(1,11)]
assert definition['parameters'][1]['mapping']['labels']==numbered_labels,'Voices are numbered 1–10 in the existing order'
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
