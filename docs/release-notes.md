# 0.1.0-rc2

Native Discord Rich Presence for Forza Horizon 2 through Azimuth. Displays the
player's car, activity, paused status, approximate location and session timer.
Uses the shared Discord application and artwork; no separate helper or user
token is required.

**Prerequisite:** install [Extra Options 0.2.34](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions)
first. It is required for the recommended, supported setup. This presence build
replaces its DLL while retaining its configuration and gamerpic.

Targets Azimuth 0.9.7 and FH2 Xbox One build 1.0.0.64. This release includes the
Extra Options 0.2.34 integration and replaces data/bin/azimuth-tls.dll. Back up
that DLL first and retain the existing Extra Options INI and gamerpic.

RC2 improves Discord acknowledgement/reconnect handling, player identification,
JSON parsing, update rate limiting, session timing and configuration validation.
It also corrects manufacturer labels and improves source and build organisation.

Automated native, mock-IPC, worker-loop, configuration and binary checks pass.
The bundled source reproduces the release DLL. RC2 still needs its own live game
verification; other wrapper/game versions are unverified. See [validation.md](validation.md).

The release ZIP contains the DLL, optional INI, instructions, licence, source,
tests and checksums. It is distributed under GPL-3.0, retaining attribution to
Arthur / RokkuDayo's original Extra Options project.
