"""Reproduce the approved five-singer composition with ten selectable looks.

The original six silhouettes and all shared face/mouth/eye paths are retained
verbatim in panel_templates.json. Only node identities expand with the roster.
"""
from pathlib import Path
import json
import re

FOLDER = Path(__file__).resolve().parent


def generate():
    art = json.loads((FOLDER / 'panel_templates.json').read_text())
    count, singers, mouths = len(art['bodies']), 5, 17
    body = singers * mouths
    eyes = body + singers * count
    closed, projection, background, face = eyes + 5, eyes + 10, eyes + 15, eyes + 16

    def node(template, index):
        return re.sub(r'id="troupe_node_\d+"', f'id="troupe_node_{index}"', template)

    lines = ['<g id="troupe_panel">', node(art['background'], background)]
    for singer in range(singers):
        lines.extend(node(path, singer * mouths + index) for index, path in enumerate(art['mouths']))
    for singer in range(singers):
        lines.extend(node(path, body + singer * count + index) for index, path in enumerate(art['bodies']))
    lines.extend(node(art['face'], face + singer) for singer in range(singers))
    for singer in range(singers):
        lines.extend((node(art['open_eyes'], eyes + singer),
                      node(art['closed_eyes'], closed + singer),
                      node(art['projection'], projection + singer)))
    lines.extend(art['palette'])
    lines.append('</g>')
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    (FOLDER / 'panel.svg.fragment').write_text(generate())
    print('Generated ten looks, five singers, shared faces, eyes and 17 mouths; 156 nodes.')
