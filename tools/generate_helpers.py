import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];rows=json.loads((root/'sheets/helpers.json').read_text())
out=['#pragma once','namespace classic::helpers {'];seen=set()
for r in rows:
    assert set(r)=={'id','relativePath','arguments','hideWindow','source'} and r['id'].isidentifier() and r['id'] not in seen and r['source']
    assert r['relativePath'] and not Path(r['relativePath']).is_absolute() and '..' not in Path(r['relativePath']).parts
    assert r['arguments']=='--watch' and r['hideWindow'] is True
    seen.add(r['id']);out.append(f'struct {r["id"]} {{static constexpr const wchar_t* path=L{json.dumps(r["relativePath"])};static constexpr const wchar_t* arguments=L{json.dumps(r["arguments"])};}};')
assert seen=={'Collision'}
out.append('}');(root/'src/helpers.generated.h').write_text('\n'.join(out)+'\n')
print('Helper sheet clean; relative path, exact arguments and hidden launch verified.')
