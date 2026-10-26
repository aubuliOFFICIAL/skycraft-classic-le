# Third-party notices — development prototype

No redistributable release bundle exists yet. Packaging must include the complete licenses and bundled dependency notices for the exact versions it actually ships.

- SkyCraft v0.1.2, chasmlol: MIT. Original copyright remains in LICENSE and the pinned upstream checkout. Its Minecraft guest JAR and protocol are reused.
- Prism Launcher11.1.1: GPL-3.0. Local testing installation. Distribution must retain its license and satisfy corresponding-source duties.
- Fabric API0.161.0+26.3: Apache-2.0. Retain its license and notices if shipped.
- PyFFI2.2.3, Python File Format Interface/Niftools contributors: BSD-3-Clause. Reads player NIF data in RAM without extracting content to disk.
- NumPy2.5.4 and SciPy1.18.1: BSD licenses and bundled Windows-wheel dependency notices. Complete wheel notices are required if shipped.
- Python3.13 and setuptools: retain their applicable notices when packaging the collision helper. No player install recipe is tested yet.
- Capstone and LLVM-MinGW: local development/inspection tools, not game content.

SKSE and libSkyrim are private ABI references only; their implementation and binaries are not linked or included. e4mc is not used by this solo prototype.

Minecraft and Skyrim are not distributed. Every player supplies both games; content is read from their own installed copies while the bridge runs. No affiliation or endorsement by Mojang, Microsoft, Bethesda or ZeniMax is implied.
