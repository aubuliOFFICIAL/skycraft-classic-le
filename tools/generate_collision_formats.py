import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rows=json.loads((root/'sheets/collision_formats.json').read_text());seen=set();out=['# Generated from sheets/collision_formats.json.']
for row in rows:
    assert set(row)=={'id','value','source'} and row['id'].isidentifier() and row['id'] not in seen and row['source']
    assert isinstance(row['value'],(int,float)) and row['value']>0
    seen.add(row['id']);out.extend([f'class {row["id"]}:',f'    value = {row["value"]!r}'])
(root/'tools/collision_formats_generated.py').write_text('\n'.join(out)+'\n')
print('Collision file-format sheet preflight clean; gameplay verification remains pending.')
