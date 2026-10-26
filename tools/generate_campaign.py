import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];r=json.loads((root/'sheets/campaign_rules.json').read_text());assert len(r)==1;r=r[0]
assert set(r)=={'id','raidOverlay','nativeCameraStates','source'} and r['id']=='Story' and type(r['raidOverlay']) is bool and r['source'] and r['nativeCameraStates'] and len(set(r['nativeCameraStates']))==len(r['nativeCameraStates']) and all(type(n) is int and 0<=n<13 for n in r['nativeCameraStates'])
out=['#pragma once','#include <array>','namespace classic::campaign {','struct Story {static constexpr bool raidOverlay='+str(r['raidOverlay']).lower()+';inline static constexpr std::array<unsigned,'+str(len(r['nativeCameraStates']))+'> nativeCameraStates={'+','.join(map(str,r['nativeCameraStates']))+'};};','}'];(root/'src/campaign.generated.h').write_text('\n'.join(out)+'\n')
print('Campaign sheet clean; full story progression acceptance remains pending.')
