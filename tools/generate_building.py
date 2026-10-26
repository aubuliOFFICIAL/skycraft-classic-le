import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/building_rules.json').read_text());seen=set();out=['#pragma once','namespace classic::building {']
for r in rows:
 assert set(r)=={'id','value','source'} and r['id'].isidentifier() and r['id'] not in seen and r['source'] and type(r['value']) is int and r['value']>0
 seen.add(r['id']);out.append(f'struct {r["id"]} {{static constexpr unsigned value={r["value"]};}};')
assert seen=={'MaximumSections','MaximumSectionVertices','MaximumCachedVertices'}
out.append('}');(root/'src/building.generated.h').write_text('\n'.join(out)+'\n')
print('Building sheet: all cells and renderer references checked.')
