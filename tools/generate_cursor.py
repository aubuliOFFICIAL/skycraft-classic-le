import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/cursor.json').read_text());out=['#pragma once','namespace classic::cursor {'];seen=set()
for r in rows:
    assert set(r)=={'id','width','height','color','outline','source'} and r['id'].isidentifier() and r['source'] and r['id'] not in seen
    assert 0<r['width']<=64 and 0<r['height']<=64 and 0<=r['color']<=0xffffffff and 0<=r['outline']<=0xffffffff
    seen.add(r['id']);out.append('struct '+r['id']+' {')
    for k in ('width','height','color','outline'):out.append(f'static constexpr unsigned {k}={r[k]}u;')
    out.append('};')
assert seen=={'InventoryPointer'};out.append('}');(root/'src/cursor.generated.h').write_text('\n'.join(out)+'\n')
print('Cursor sheet clean; every cell and pointer reference checked.')
