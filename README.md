# SkyCraft Classic source review

Unfinished MIT mod prototype for original Skyrim1.9.32 and Minecraft Java26.3.
Not a playable package. No signed or playable Melty release has been uploaded or published.
Requires both player-owned games; includes no game files, assets, saves,
account data, launchers, downloaded toolchains, local logs or gameplay media.
The protocol and adapted render source retain upstream MIT attribution.

The native compile-review command uses no game files; see docs/SOURCE-BUILD.md.
A clean GitHub runner passed this compile check:
https://github.com/aubuliOFFICIAL/skycraft-classic-le/actions/runs/40247759618
This is not gameplay or full-package acceptance. The full local gameplay build
checks the installed Skyrim executable. The guest builds against installed
Minecraft jars and the pinned upstream MIT guest. This is not a full release.

Windows blocked the latest DLL. Theft, world transitions, building isolation,
saves, story progression and other release checks remain incomplete.
See docs/PUBLIC-SIGNING.md for the pending signing policy.
