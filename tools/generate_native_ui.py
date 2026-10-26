import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/native_ui_tests.json').read_text());seen=set();out=['#pragma once','#include <array>','namespace classic::uitest {']
for r in rows:
 assert set(r)=={'id','action','scanCode','event','context','source'} and r['id'].isidentifier() and r['action'] not in seen and r['source'] and 23<r['action']<32 and 0<r['scanCode']<256 and r['event'] in ('Accept','Cancel') and r['context']==1
 seen.add(r['action']);out.append('struct '+r['id']+' {static constexpr unsigned action='+str(r['action'])+',scan='+str(r['scanCode'])+',context='+str(r['context'])+';static constexpr const char* event='+json.dumps(r['event'])+';};')
out.append('struct Test {unsigned action,scan,context;const char* event;};');out.append(f'inline constexpr std::array<Test,{len(rows)}> tests={{{{')
out += [f'{{{r["id"]}::action,{r["id"]}::scan,{r["id"]}::context,{r["id"]}::event}},' for r in rows];out += ['}};','}'];(root/'src/native_ui_tests.generated.h').write_text('\n'.join(out)+'\n')
print('Native UI test sheet clean; real menu transfer remains unverified.')
