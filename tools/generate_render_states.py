"""Preflight and generate one constant struct per HUD render-state row."""
import json,re,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--world',action='store_true');ap.add_argument('--composite',action='store_true');args=ap.parse_args()
assert not(args.world and args.composite)
sheet='world_composite_states' if args.composite else ('world_render_states' if args.world else 'render_states')
namespace='composite' if args.composite else ('world' if args.world else 'hud')
rows=json.loads((ROOT/f'sheets/{sheet}.json').read_text())
names=set();states=set()
out=[f'// Generated from sheets/{sheet}.json. Do not edit.', '#pragma once', '#include <array>', f'namespace classic::{namespace} {{']
for row in rows:
    assert set(row)=={'id','state','value','source'}
    assert row['id'] and row['source'] and row['state'] and row['value'] is not None
    assert re.fullmatch(r'[A-Za-z][A-Za-z0-9]*',row['id']) and re.fullmatch(r'D3DRS_[A-Z0-9_]+',row['state'])
    assert row['id'] not in names and row['state'] not in states
    names.add(row['id']);states.add(row['state'])
    value=row['value']
    assert isinstance(value,int) or re.fullmatch(r'D3D[A-Z0-9_]+',value)
    out.append(f'struct {row["id"]} {{ static constexpr D3DRENDERSTATETYPE state={row["state"]}; static constexpr DWORD value={value}; }};')
out.append('struct Setting { D3DRENDERSTATETYPE state; DWORD value; };')
out.append(f'inline constexpr std::array<Setting,{len(rows)}> states = {{{{')
out.extend(f'{{{row["id"]}::state,{row["id"]}::value}},' for row in rows)
out.extend(['}};', '}'])
(ROOT/f'src/{sheet}.generated.h').write_text('\n'.join(out))
print(f'HUD sheet: {len(rows)} rows checked; every cell filled; no duplicate states.')
