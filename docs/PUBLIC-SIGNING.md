# Code signing policy — application pending

No signing provider has approved this project. No signed build is available.
This repository does not grant Windows trust or change a security policy.

The project intends to apply to the SignPath Foundation free open-source
program. Provider acceptance, upstream eligibility and a verifiable signing
pipeline are still required. No Foundation endorsement is claimed.

Source maintainer and proposed human signing approver: the repository owner.
AI-assisted changes require the human maintainer's review before a signing
request. MFA and signing permissions must be configured by that maintainer.
Signing must use a source-verifiable, reviewed revision with explicit human
approval. Never publish an unsigned prototype as a verified playable release.

## Privacy

This native bridge uses local shared memory with a Minecraft companion and
reads the player's installed game files. It does not include game content,
save files, credentials or account databases in source or release packages.
The companion and its launcher may contact their respective services for
Minecraft authentication or optional integrations; their applicable privacy
policies remain in effect. Source review/build checks do not start the games
or access a player's installed games. No telemetry submission is configured
by this compile-review workflow.

## Distribution

This public repository is unfinished MIT source. The playable mashup is
intended for Melty after installation, gameplay and media checks pass.
There is no public playable release yet. Game ownership and both original
installations are required at runtime. Neither game is redistributed.
