import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/raid.json').read_text());out=['#pragma once','#include <array>','namespace classic::raid::spec {'];seen=set()
for r in rows:
    assert set(r)=={'id','cell','enemies','chest','reachUnits','title','briefing','completeText','source'} and r['id'].isidentifier() and r['id'] not in seen and r['source']
    assert all(isinstance(r[k],int) and r[k]>0 for k in ('cell','chest','reachUnits')) and len(r['enemies'])==2 and len(set(r['enemies']))==2 and all(isinstance(v,int) and v>0 for v in r['enemies'])
    assert all(r[k] for k in ('title','briefing','completeText'));seen.add(r['id']);out.append('struct '+r['id']+' {')
    for k in ('cell','chest','reachUnits'):out.append(f'static constexpr unsigned {k}={r[k]};')
    for k in ('title','briefing','completeText'):out.append(f'static constexpr const char* {k}={json.dumps(r[k])};')
    out.append('static constexpr std::array<unsigned,2> enemies={'+','.join(map(str,r['enemies']))+'};');out.append('};')
assert seen=={'EntryChamber'}
ui=json.loads((root/'sheets/raid_ui.json').read_text());assert len(ui)==1;r=ui[0]
assert set(r)=={'id','x','y','width','height','fontSize','padding','background','textColor','controls','source'} and r['id']=='Panel' and r['source'] and r['controls']
assert all(isinstance(r[k],int) and r[k]>0 for k in ('x','y','width','height','fontSize','padding','background','textColor')) and r['width']<=1024 and r['height']<=256 and r['padding']*2<r['height']
out.append('struct Panel {')
for k in ('x','y','width','height','fontSize','padding','background','textColor'):out.append(f'static constexpr unsigned {k}={r[k]}u;')
out.append('static constexpr const char* controls='+json.dumps(r['controls'])+';');out.extend(['};','}']);(root/'src/raid.generated.h').write_text('\n'.join(out)+'\n')
print('Raid and UI sheets clean; every goal, enemy reference and panel cell checked. Live raid acceptance still required.')
