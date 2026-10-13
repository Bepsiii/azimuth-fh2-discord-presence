# FH2 Discord Presence 1.0.0

Discord Rich Presence for Forza Horizon 2 running under Azimuth on Windows x64. Shows the current car, location, activity and elapsed session time.

The presence runs in its own `FH2DiscordPresence.dll`. Azimuth 0.9.7 loads a fixed `azimuth-tls.dll`, so this package supplies a small loader alongside the separate module. Copying only the presence DLL into bin does not activate it.

## Installation

Close Azimuth, extract the release ZIP, then run in Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -AzimuthFolder "C:\path\to\azimuth-windows"
```

Extra Options 0.2.34 is recommended. If `data/bin/FH2_ExtraOptions.ini` exists, the installer preserves your settings and installs a separate clean Extra Options module with the combined loader. Otherwise it installs the presence-only loader. Use `-WithoutExtraOptions` to select presence-only explicitly. Existing presence settings and Gamerpic.png are preserved.

The installer backs up the previous DLL setup. To restore it, close Azimuth and run `Uninstall.ps1` with the same `-AzimuthFolder`. Restoring an earlier merged presence DLL restores that earlier presence implementation too. Do not manually mix loaders or load the old merged DLL as Extra Options.

Supported target: Azimuth 0.9.7 and the FH2 executable with SHA256 `3ec04c6163fb5614e3d351ba0570d39b846041a257715d6340eceb03ccb7f0a3`. Other executable builds are rejected before memory polling starts. Inspect `data/bin/FH2_DiscordPresence.log` for diagnostics.

Discord must be running locally. The application ID is public and provides no account access. This mod requires no Discord token, password or client secret. Game data is read within the Azimuth process; the presence module does not patch game memory.

## Validation

Automated checks cover bounded JSON and IPC, fragmented messages, reconnects, rate limits, car/player selection, paused/race states, invalid configuration, native DLL loading, TLS compatibility and coexistence with the separate Extra Options module. The 1.0.0 release is marked prerelease pending an in-game check of the standalone package.

## Source and licences

GPL-3.0. [Extra Options by Arthur / RokkuDayo](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions) is included as separate source under `third-party/ForzaHorizon2_ExtraOptions`, with its original licence. Presence car labels derive from the game's identifiers; no game binaries or assets are supplied. See `docs/BUILD.md` for building and testing.
