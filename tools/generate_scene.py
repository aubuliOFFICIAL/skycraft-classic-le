"""Preflight factual layout cells and generate one C++ constant struct per row.
This does not verify physics correctness or mark the collision system complete.
"""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rows=json.loads((root/'sheets/scene_layout.json').read_text())
seen=set()
out=['// Generated from sheets/scene_layout.json. Independent factual layout adapter.',
     '#pragma once','namespace classic::scene::layout {']
for row in rows:
    assert set(row)=={'id','value','source'} and row['source']
    assert row['id'].isidentifier() and row['id'] not in seen
    assert type(row['value']) is int and row['value']>=0
    seen.add(row['id'])
    out.append(f'struct {row["id"]} {{ static constexpr unsigned value={row["value"]}u; }};')
out.append('}')
(root/'src/scene.generated.h').write_text('\n'.join(out)+'\n')
print(f'Scene diagnostic preflight: {len(rows)} complete rows, no duplicate references. Physics unverified.')
