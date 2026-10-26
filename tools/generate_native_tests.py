import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/native_tests.json').read_text());seen=set();actions=set();out=['#pragma once','#include <array>','namespace classic::nativetest {']
for r in rows:
    assert set(r)=={'id','action','scanCode','holdMs','source'} and r['id'].isidentifier() and r['id'] not in seen and r['action'] not in actions and r['source']
    assert 10<r['action']<32 and 0<r['scanCode']<256 and 50<=r['holdMs']<=1000
    seen.add(r['id']);actions.add(r['action']);out.append('struct '+r['id']+' {')
    for k in ('action','scanCode','holdMs'):out.append(f'static constexpr unsigned {k}={r[k]};')
    out.append('};')
out.append('struct Test {unsigned action,scan,hold;};');out.append(f'inline constexpr std::array<Test,{len(rows)}> tests={{{{')
out += [f'{{{r["id"]}::action,{r["id"]}::scanCode,{r["id"]}::holdMs}},' for r in rows];out += ['}};','}'];(root/'src/native_tests.generated.h').write_text('\n'.join(out)+'\n')
print('Native tests: every input row, column and action reference checked.')
