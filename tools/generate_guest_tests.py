import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/guest_tests.json').read_text());seen=set();out=['#pragma once','#include <array>','namespace classic::guesttest {']
for r in rows:
 assert set(r)=={'id','action','select','key','kind','extraKey','holdMs','source'} and r['id'].isidentifier() and r['action'] not in seen and r['source'] and r['kind'] in (1,2) and 0<=r['extraKey']<512
 assert 15<r['action']<32 and 0<=r['select']<512 and 0<r['key']<512 and 50<=r['holdMs']<=5000
 seen.add(r['action']);out.append('struct '+r['id']+' {')
 for k in ('action','select','key','kind','extraKey','holdMs'):out.append(f'static constexpr unsigned {k}={r[k]};')
 out.append('};')
out.append('struct Test {unsigned action,select,key,kind,extraKey,holdMs;};');out.append(f'inline constexpr std::array<Test,{len(rows)}> tests={{{{')
out += [f'{{{r["id"]}::action,{r["id"]}::select,{r["id"]}::key,{r["id"]}::kind,{r["id"]}::extraKey,{r["id"]}::holdMs}},' for r in rows];out += ['}};','}'];(root/'src/guest_tests.generated.h').write_text('\n'.join(out)+'\n')
print('Guest QA sheet: all cells and action references checked.')
