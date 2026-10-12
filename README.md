# Azimuth FH2 Discord Rich Presence

Discord Rich Presence for Forza Horizon 2 through Azimuth. Shows the current
car, activity, paused status, approximate location and elapsed session time.

**Supported target:** Azimuth 0.9.7 with FH2 Xbox One build 1.0.0.64.

## Requirements

- Azimuth 0.9.7 and FH2 Xbox One build 1.0.0.64.
- **[Extra Options 0.2.34](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions), installed first.**
- Discord's desktop app with activity sharing enabled.

Extra Options is the prerequisite for the recommended, supported installation.
This release includes its code, so the presence DLL replaces the Extra Options
DLL. Keep the original mod's configuration and gamerpic.

## Download and install

[Download the RC2 release](https://github.com/Bepsiii/azimuth-fh2-discord-presence/releases/tag/v0.1.0-rc2).

1. Install Extra Options 0.2.34 using the upstream mod's instructions.
2. Close Azimuth and stop any PowerShell presence companion.
3. Extract the release ZIP and back up the existing `data/bin/azimuth-tls.dll`.
4. Copy the supplied `azimuth-tls.dll` into `data/bin`, then launch the game
   with Discord's desktop app open.

Keep the existing `FH2_ExtraOptions.ini` and `Gamerpic.png`. The optional
`FH2_DiscordPresence.ini` can go beside the DLL. Allow about 15 seconds for
activity updates. Restore the backed-up DLL to uninstall.

The release uses shared Discord artwork and needs no separate helper or user
token. It includes the Extra Options 0.2.34 integration. RC2 is a prerelease;
its automated checks pass, and its final live game checks are documented.

## Repository

| Folder | Contents |
| --- | --- |
| [source](source/) | C code, build scripts, tests and the optional presence INI |
| [docs](docs/) | Detailed usage, build instructions, validation and release notes |
| [releases](releases/) | The checksum-verified RC2 download package |

[Full instructions](docs/presence.md) · [Build from source](docs/building.md) ·
[Validation](docs/validation.md) · [Release notes](docs/release-notes.md)

## Licence and attribution

GPL-3.0; see [LICENSE](LICENSE). Based on
[Extra Options by Arthur / RokkuDayo](https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions).
