import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=json.loads((ROOT/'sheets/equipment_tests.json').read_text())
out=['// Generated equipment test specifications. These are not test results.','#pragma once','namespace classic::qa {']
for row in rows:
    assert set(row)=={'id','slot','mouseButton','holdMs','source','expected'}
    assert all(v is not None and v!='' for v in row.values())
    assert row['mouseButton'] in (1,3) and 0<row['holdMs']<=10000 and 30<=row['slot']<=38
    out.append(f'struct {row["id"]} {{ static constexpr unsigned slot={row["slot"]},mouse={row["mouseButton"]},holdMs={row["holdMs"]}; }};')
out.append('}')
(ROOT/'src/equipment_tests.generated.h').write_text('\n'.join(out))
print('Equipment test sheet: every cell filled; supported slots/buttons/timing verified.')
