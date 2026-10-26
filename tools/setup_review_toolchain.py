"""Download the pinned official compiler for source-review CI, never a game."""
import hashlib,json,urllib.request,zipfile
from pathlib import Path,PurePosixPath
root=Path(__file__).resolve().parents[1]
rows=json.loads((root/'sheets/source_review_tools.json').read_text());assert len(rows)==1
r=rows[0]
assert set(r)=={'id','version','archive','sha256','url','source'} and all(r.values())
assert r['id']=='LlvmMingw' and r['url']=='https://github.com/mstorsjo/llvm-mingw/releases/download/'+r['version']+'/'+r['archive']
assert len(r['sha256'])==64 and all(c in '0123456789abcdef' for c in r['sha256'])
target=root/'tools';target.mkdir(exist_ok=True)
archive=target/r['archive']
if not archive.exists():urllib.request.urlretrieve(r['url'],archive)
assert hashlib.sha256(archive.read_bytes()).hexdigest()==r['sha256'],'Compiler digest mismatch; do not extract or execute'
prefix=r['archive'][:-4]
with zipfile.ZipFile(archive) as z:
 for entry in z.infolist():
  p=PurePosixPath(entry.filename)
  assert not p.is_absolute() and '..' not in p.parts and p.parts[0]==prefix
 z.extractall(target)
print('Verified official compiler archive extracted for compile-only source review.')
