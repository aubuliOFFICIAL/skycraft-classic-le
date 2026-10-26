import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/capture_rules.json').read_text());seen=set()
out=['// Generated owned game-only capture limits.','#pragma once','namespace classic::capture::spec {']
for r in rows:
    assert set(r)=={'id','value','source'} and r['id'].isidentifier() and r['source'] and r['id'] not in seen and isinstance(r['value'],int) and r['value']>0
    seen.add(r['id']);out.append(f'struct {r["id"]} {{static constexpr unsigned value={r["value"]};}};')
assert seen=={'IntervalMs','DurationMs','MaximumWidth','MaximumHeight'}
out.append('}');(root/'src/capture.generated.h').write_text('\n'.join(out)+'\n')
print('Game-only capture sheet: every cell and limit checked.')
