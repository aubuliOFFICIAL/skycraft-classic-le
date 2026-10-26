import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/interaction_tests.json').read_text());seen=set();out=['#pragma once','#include <array>','namespace classic::interactiontest {']
for r in rows:
 assert set(r)=={'id','action','reference','pitchRadians','heightUnits','source'} and r['id'].isidentifier() and r['action'] not in seen and r['source'] and 15<r['action']<32 and r['reference']>=0 and -1.5<=r['pitchRadians']<=1.5 and 0<=r['heightUnits']<=150
 seen.add(r['action']);out.append('struct '+r['id']+' {static constexpr unsigned action='+str(r['action'])+',reference='+str(r['reference'])+';static constexpr float pitch='+str(float(r['pitchRadians']))+'f,height='+str(float(r['heightUnits']))+'f;};')
out.append('struct Test {unsigned action,reference;float pitch,height;};');out.append(f'inline constexpr std::array<Test,{len(rows)}> tests={{{{')
out += [f'{{{r["id"]}::action,{r["id"]}::reference,{r["id"]}::pitch,{r["id"]}::height}},' for r in rows];out += ['}};','}'];(root/'src/interaction_tests.generated.h').write_text('\n'.join(out)+'\n')
print('Interaction test sheet clean; gameplay unverified.')
