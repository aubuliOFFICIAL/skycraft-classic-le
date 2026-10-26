"""Build only a diagnostic DLL, never a gameplay release. Verify input sheet first."""
import json,argparse,os,hashlib
import subprocess
from pathlib import Path
import pefile

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--source-review',action='store_true',help='Compile-only review without reading games; never installs or starts anything')
args=parser.parse_args()
subprocess.run(['python',str(ROOT/'tools/generate_hooks.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_render_states.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_render_states.py'),'--world'],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_render_states.py'),'--composite'],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_input.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_equipment_tests.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_scene.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_combat.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_combat_weapons.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_capture.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_helpers.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_cursor.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_native_tests.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_guest_tests.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_building.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_interaction.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_campaign.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_native_ui.py')],check=True)
subprocess.run(['python',str(ROOT/'tools/generate_raid.py')],check=True)
HOST = Path('C:/Program Files (x86)/Steam/steamapps/common/Skyrim/TESV.exe')
rows = json.loads((ROOT / 'sheets/bootstrap.json').read_text())
required = {'id','hostExe','hostMachine','proxyName','exportName','expectedPointerBits',
            'logDirectory','logFile','gameplayImplemented','source'}
errors = []
for row in rows:
    for key in required:
        if key not in row or row[key] is None or row[key] == '':
            errors.append(f'{row.get("id")}.{key} unfilled')
    if row.get('gameplayImplemented') is not False:
        errors.append('Diagnostic scope must not claim gameplay')
row = rows[0]
hostMachine=None
if not args.source_review:
    host = pefile.PE(str(HOST), fast_load=False)
    hostMachine=host.FILE_HEADER.Machine
    if hostMachine != row['hostMachine']:
        errors.append('Host architecture differs from sheet')
    imports = {(e.dll.decode().lower(), i.name.decode())
               for e in host.DIRECTORY_ENTRY_IMPORT for i in e.imports if i.name}
    if (row['proxyName'].lower(), row['exportName']) not in imports:
        errors.append('Host does not import proxy export')
report = {'scope':'source compile review only' if args.source_review else 'diagnostic bootstrap only', 'unfilledOrUnverifiedCells':errors,
          'hostMachineVerified':hostMachine, 'gameplayTested':False}
(ROOT / 'build').mkdir(exist_ok=True)
(ROOT / 'build/bootstrap-preflight.json').write_text(json.dumps(report, indent=2))
if errors:
    raise SystemExit(json.dumps(report, indent=2))
out = ['// Generated from sheets/bootstrap.json', '#pragma once', 'namespace classic::spec {']
for row in rows:
    out.append('struct ' + row['id'] + ' {')
    for key, value in row.items():
        if isinstance(value, bool):
            out.append(f'static constexpr bool {key} = {str(value).lower()};')
        elif isinstance(value, int):
            out.append(f'static constexpr unsigned {key} = {value};')
        else:
            out.append(f'static constexpr const char* {key} = {json.dumps(value)};')
    out.append('};')
out.append('}')
(ROOT / 'src/bootstrap.generated.h').write_text('\n'.join(out))
(ROOT / 'src/dinput8.def').write_text('LIBRARY ' + row['proxyName'] + '\nEXPORTS\n    ' + row['exportName'] + '\n')
compilers = list((ROOT / 'tools').glob('llvm-mingw-*/bin/i686-w64-mingw32-clang++.exe'))
if len(compilers) != 1:
    raise SystemExit('Compiler download/extraction not complete; clean diagnostic preflight saved.')
destination=ROOT/'build'/('source-review-dinput8.dll' if args.source_review else 'dinput8.dll')
cmd = [str(compilers[0]), '-std=c++20', '-shared', '-O2', '-static-libgcc', '-static-libstdc++',
       '-ffile-prefix-map='+str(ROOT)+'=/src','-Wl,--no-insert-timestamp',
       str(ROOT/'src/bootstrap.cpp'),str(ROOT/'src/runtime.cpp'),str(ROOT/'src/scene.cpp'),str(ROOT/'src/world_render.cpp'),str(ROOT/'src/capture.cpp'),str(ROOT/'src/helper.cpp'),str(ROOT/'src/device_input.cpp'),str(ROOT/'src/raid.cpp'),str(ROOT/'src/dinput8.def'),'-lshell32','-lgdi32','-o',str(destination)]
subprocess.run(cmd, check=True)
dll = pefile.PE(str(destination))
assert dll.FILE_HEADER.Machine == row['hostMachine']
assert row['exportName'].encode() in {e.name for e in dll.DIRECTORY_ENTRY_EXPORT.symbols}
print('Diagnostic DLL built, x86 machine and DirectInput export verified. Not tested inside Skyrim.')
if args.source_review:
    (ROOT/'build/source-compile-review.json').write_text(json.dumps({'scope':'compile-only; not installed or executed','sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),'gameplayTested':False,'hostFilesRead':False},indent=2)+'\n')
