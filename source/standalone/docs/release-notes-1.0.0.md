# FH2 Discord Presence 1.0.0

Presence now runs in its own FH2DiscordPresence.dll, independent of Extra Options. Azimuth still requires the supplied small azimuth-tls.dll loader. Two loader variants support presence alone or presence plus a separate clean Extra Options 0.2.34 module.

Extract the ZIP, close Azimuth, and run Install.ps1 with -AzimuthFolder pointing to your launcher folder. Existing INI settings and gamerpic are preserved; Uninstall.ps1 restores the previous DLL setup. Extra Options remains recommended and must be installed first for the recommended combined setup.

Supports Azimuth 0.9.7 with the exact FH2 executable hash documented in README. Native loading, TLS compatibility, Extra Options coexistence, IPC, player detection and installation/restoration checks passed. **Stable release:** the standalone package was confirmed working in-game with Extra Options on 13 October 2026.

Complete corresponding GPL-3.0 source, including the separate Extra Options module, is supplied in the source ZIP. No game assets, Discord tokens or client secrets are included.

Promoted from prerelease using the same tested installer and DLLs. The packaged README records the validation status at packaging time; these release notes and the repository documentation record the completed live check.
