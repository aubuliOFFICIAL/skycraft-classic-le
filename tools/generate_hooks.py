import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=json.loads((ROOT/'sheets/host_hooks.json').read_text())
ids=set()
out=['// Generated from sheets/host_hooks.json. Read-only layout facts; no SKSE code.', '#pragma once', 'namespace classic::hooks {']
for row in rows:
    assert set(row)=={'id','value','source'} and row['id'] and row['source']
    assert row['id'] not in ids and isinstance(row['value'],int)
    ids.add(row['id'])
    out.append('struct '+row['id']+' { static constexpr unsigned value = '+str(row['value'])+'u; };')
out.append('}')
(ROOT/'src/hooks.generated.h').write_text('\n'.join(out))
print(f'Host hook sheet: {len(rows)} rows checked, no empty cells or duplicate ids.')
