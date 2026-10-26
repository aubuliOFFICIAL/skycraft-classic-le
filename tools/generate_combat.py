import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/combat_rules.json').read_text());seen=set()
out=['// Generated combat rules. Acceptance testing remains required.','#pragma once','namespace classic::combat {']
for row in rows:
    assert set(row)=={'id','value','source'} and row['id'].isidentifier() and row['id'] not in seen and row['source']
    assert isinstance(row['value'],(float,int));seen.add(row['id'])
    kind='float' if isinstance(row['value'],float) else 'unsigned';value=str(row['value'])+('f' if kind=='float' else 'u')
    out.append(f'struct {row["id"]} {{ static constexpr {kind} value={value}; }};')
out.append('}');(root/'src/combat.generated.h').write_text('\n'.join(out)+'\n');print('Combat rule sheet preflight clean; gameplay unverified.')
