import json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
rows=json.loads((ROOT/'sheets/input_bindings.json').read_text())
out=['// Generated from sheets/input_bindings.json','#pragma once','#include <array>','namespace classic::input {']
seen=set()
for row in rows:
    assert set(row)=={'id','virtualKey','code','kind','source'} and row['source']
    assert re.fullmatch('[A-Za-z][A-Za-z0-9]*',row['id']) and row['id'] not in seen
    assert 0<row['virtualKey']<256 and 0<row['code']<512 and row['kind'] in ('key','mouse')
    seen.add(row['id']);kind=1 if row['kind']=='key' else 2
    out.append(f'struct {row["id"]} {{ static constexpr unsigned vk={row["virtualKey"]},code={row["code"]},kind={kind}; }};')
out.append('struct Binding { unsigned vk,code,kind; };')
out.append(f'inline constexpr std::array<Binding,{len(rows)}> bindings = {{{{')
out.extend(f'{{{r["id"]}::vk,{r["id"]}::code,{r["id"]}::kind}},' for r in rows)
out.extend(['}};','}'])
(ROOT/'src/input.generated.h').write_text('\n'.join(out))
print(f'Input sheet: {len(rows)} rows checked, all cells filled.')
rows=json.loads((ROOT/'sheets/native_input_bindings.json').read_text());seen=set()
out=['#pragma once','#include <array>','namespace classic::nativeinput {']
for r in rows:
    assert set(r)=={'id','event','scanCode','source'} and r['id'].isidentifier() and r['id'] not in seen and r['event'] and r['source'] and 0<r['scanCode']<256
    seen.add(r['id']);out.append(f'struct {r["id"]} {{static constexpr const char* event={json.dumps(r["event"])};static constexpr unsigned code={r["scanCode"]};}};')
assert seen=={'Activate','Favorites','Wait'}
out.append('struct Binding {const char* event;unsigned code;};')
out.append(f'inline constexpr std::array<Binding,{len(rows)}> bindings={{{{')
out.extend(f'{{{r["id"]}::event,{r["id"]}::code}},' for r in rows)
out.append('}};')
out.append('}');(ROOT/'src/native_input.generated.h').write_text('\n'.join(out)+'\n')
print('Native binding sheet clean; original runtime values must be restored.')
