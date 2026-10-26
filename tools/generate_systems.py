"""Generate one descriptive struct per system row; no gameplay implementation implied."""
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
rows = json.loads((ROOT / 'sheets/systems.json').read_text())
out = ['// Generated from sheets/systems.json. Do not edit.', '#pragma once',
       'namespace classic::spec {']
for row in rows:
    out.append('struct ' + row['id'].title() + ' {')
    for key, value in row.items():
        text = ','.join(value) if isinstance(value, list) else value
        out.append('static constexpr const char* ' + key + ' = ' + json.dumps(text) + ';')
    out.append('};')
out.append('}')
(ROOT / 'src/systems.generated.h').write_text('\n'.join(out) + '\n')
