# Source review build

This is a compile check of the native x86 bridge, not a gameplay test or a
complete mashup package. No games are downloaded, copied, read or launched.
The guest and collision helper require separate builds and runtime testing.

On a Windows development machine with Python, run:

```
python -m pip install pefile==2024.8.26
python tools/setup_review_toolchain.py
python tools/build_bootstrap.py --source-review
```

The compiler is pinned by its official release SHA-256 in the tools sheet.
Generated rule/layout headers are regenerated after row/reference checks.
The resulting DLL is written only to build/source-review-dinput8.dll. The
report explicitly records that gameplay and installed-host compatibility
were not tested. This command does not install, execute, trust or sign it.

A standard public GitHub-hosted Windows runner can perform the same check.
No larger/paid runner, credentials, game files or signing secrets are needed.
The workflow does not upload DLL artifacts or create playable releases.

Full local gameplay builds additionally validate the player's actual Skyrim
executable and require the installed games. Passing the compile review does
not clear pending gameplay acceptance in the systems sheet.
