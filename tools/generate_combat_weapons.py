import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/combat_weapons.json').read_text())
out=['// Generated weapon impact metadata, never game assets.','#pragma once','#include <array>','namespace classic::impact {']
seen=set()
for row in rows:
    assert set(row)=={'id','guestKind','nativeForm','skill','source'} and row['id'].isidentifier() and row['source']
    assert row['guestKind'] not in seen and row['nativeForm']>0 and -1<=row['skill']<=8
    seen.add(row['guestKind'])
    out.append(f'struct {row["id"]} {{ static constexpr unsigned guestKind={row["guestKind"]},nativeForm={row["nativeForm"]};static constexpr int skill={row["skill"]}; }};')
assert seen==set(range(6)),'Unresolved protocol weapon kind'
out.append('struct Weapon {unsigned kind,form;int skill;};')
out.append('inline constexpr std::array<Weapon,6> weapons{{'+','.join('{'+f'{r["id"]}::guestKind,{r["id"]}::nativeForm,{r["id"]}::skill'+'}' for r in rows)+'}};')
out.append('}');(root/'src/combat_weapons.generated.h').write_text('\n'.join(out)+'\n')
print('Combat weapon sheet: all cells filled, all protocol weapon references resolve.')
