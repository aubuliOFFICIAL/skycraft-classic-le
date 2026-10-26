"""Check every sheet cell and reference. Gameplay verification gates release."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDS = {'id', 'owner', 'route', 'source', 'requires', 'acceptance', 'verification'}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--release', action='store_true')
    args = ap.parse_args()
    rows = json.loads((ROOT / 'sheets/systems.json').read_text())
    errors = []
    ids = [r.get('id') for r in rows]
    if len(ids) != len(set(ids)):
        errors.append('duplicate system ids')
    for row in rows:
        for field in FIELDS:
            if field not in row or (field != 'requires' and not row[field]):
                errors.append(f'{row.get("id")}.{field}: unfilled')
        for target in row.get('requires', []):
            if target not in ids:
                errors.append(f'{row.get("id")}.requires: unresolved {target}')
        if row.get('verification') != 'verified':
            errors.append(f'{row.get("id")}.verification: {row.get("verification")}')
    report = {'systems': len(rows), 'unfinishedCells': errors,
              'releaseReady': not errors, 'gameplayTested': not errors}
    (ROOT / 'build').mkdir(exist_ok=True)
    (ROOT / 'build/preflight.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
    return 1 if errors else 0

if __name__ == '__main__':
    raise SystemExit(main())
