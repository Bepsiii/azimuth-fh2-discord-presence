# Azimuth FH2 Native Discord Presence - 0.1.0-rc2

This native x64 DLL adds automatic Discord Rich Presence to Azimuth 0.9.7 with
Forza Horizon 2 Xbox One build 1.0.0.64. It is built from the Extra Options
0.2.34 source and preserves its existing configurable features.

## Install

1. Close Azimuth and stop any PowerShell Rich Presence companion. Running both
   integrations can cause competing activity updates.
2. Open the launcher's **data/bin** folder.
3. Back up its existing **azimuth-tls.dll**, for example as **azimuth-tls.dll.backup**.
4. Copy the supplied **azimuth-tls.dll** into that folder.
5. Launch Azimuth with Discord's desktop app open. Allow up to 15 seconds after
   loading into gameplay for the presence to appear.

No external runtime, helper executable, user token, Developer Portal setup or
separate process is required. The shared Application ID and existing artwork
keys are embedded in the DLL.

The optional **FH2_DiscordPresence.ini** can also be copied beside the DLL. It is
not needed for the default settings. Restart the game after changing it.

**Keep your existing FH2_ExtraOptions.ini and Gamerpic.png.** This package does
not overwrite those files. If no Extra Options INI exists, gameplay tweaks are
disabled; the DLL operates as a presence-only helper. To enable Extra Options
features, use a supported 0.2.34 INI with Global/Enabled=1.

To remove this build, close Azimuth and restore the original DLL.

## Presence

- Year, manufacturer and full model name for known vehicles.
- The game's native activity: free roam, races, cross-country, road trips,
  bucket lists, photo mode, garage, Autoshow, paint shop and car meets.
- Paused status, including pause caused by losing focus.
- Approximate location, labelled **Near** the nearest mainland destination hub.
- Elapsed session time, including pauses.
- Large/small Discord artwork and reconnecting when Discord restarts.

Detection runs every two seconds; changed activity and clearing are published
at most once every 15 seconds on an established connection. Reconnecting sends
the current activity again. The worker reads memory without calling or modifying game
functions. Extra Options features can still modify gameplay according to the
existing mod settings.

The session timer starts with the first identified player car, even if Discord
is not open yet, and is retained across Discord reconnects. Missing or ambiguous
player data, including the internal NULL CAR, clears presence on the next update
slot. It does not guess from other cars in the scene.

Location uses six hub coordinates, not exact district boundaries. Specific race
names, race position and speed are not included. Online/Island labels are based
on native enum values; availability depends on the wrapper/game.

## Settings and logs

FH2_DiscordPresence.ini supports Enabled, ApplicationId, LargeImage, SmallImage,
ShowPaused and ShowLocation in its Discord section. Asset keys are normalised to
lowercase. Empty LargeImage or SmallImage disables that artwork field.
Enabled, ShowPaused and ShowLocation accept 0 or 1. Invalid IDs, invalid boolean
values or oversized setting strings stop presence and explain the problem in
the log. Correct the INI and restart Azimuth to retry.

All default users share application **1559239981548052682**, large artwork **fh2**
and small artwork **fh2-discord-artwork**. The application owner controls these
assets; changing/removing that application or its assets affects this release.
The INI allows another application ID/artwork set to be used.

**FH2_DiscordPresence.log**, beside the DLL, records startup, connection problems
and Discord's SET_ACTIVITY acknowledgement. The normal FH2_ExtraOptions.log
continues to record game validation and mod initialization. Activity sharing
must be enabled in Discord.
The presence log starts fresh at worker startup and stops growing around 256 KiB.
An acknowledged update confirms Discord accepted the request; it does not prove
that every requested image asset exists or that other users can see the activity.

The shared application ID and image keys are public identifiers. No Discord
account token, bot token or OAuth client secret is needed or included. Someone
else can reuse the public ID for activity on their own Discord profile; presence
is not a way to authenticate a genuine copy of this mod.

## Validation and release status

This is a release candidate, targeting Azimuth 0.9.7 / FH2 Xbox One 1.0.0.64.
RC1 was observed installed with matching binary hash and live Discord activity
acknowledgements. RC2 has passed the automated checks below, but still needs its
own live game test. Other Azimuth/game versions are unverified.

The native test DLL passed synthetic local-player selection, traffic filtering,
BMW Z4/Castelletto capture replay, pause/race/Island formatting, ambiguous-car
rejection and JSON field parsing. The same native IPC code passed a local mock
Discord handshake, fragmented response, activity update, ping/pong, acknowledgement
and clear. Tests also cover strict JSON structure/Unicode/size limits, nested
lookalike fields, duplicate fields, wrong nonces, malformed and oversized frames,
explicit errors, disconnects, invalid settings and application-ID overflow.
The actual worker loop is exercised with accelerated polling/retry intervals:
pause, car changes, rate limiting, clear, reconnect, retained timestamps and
missing-acknowledgement recovery. Missing Extra Options configuration is checked
to leave gameplay patches disabled.

Release ABI checks verify x64, only Kernel32 imports, the expected TLS exports
and the 336-byte, 16-byte-aligned TLS template. Unsupported game headers/file
hashes and getter signatures are rejected before the presence worker starts.
The packaged source is rebuilt separately and compared against the release DLL.
See [validation.md](validation.md) for results and the remaining live check.

## Changes since RC1

- Match Discord acknowledgements to the outstanding request nonce and recover
  if an acknowledgement never arrives; service IPC independently of game polling.
- Parse complete JSON objects with bounded nesting and UTF-8/Unicode checks.
- Require the native local player and recheck ownership and scene pointers.
- Apply the update interval to clears as well as normal updates.
- Preserve session timing when Discord is launched late or restarts.
- Validate configuration instead of silently truncating it; bound diagnostic logs.
- Avoid repeating Storm Island in activity text and expand 16 abbreviated
  manufacturer entries in the readable 305-entry car-name table.
- Separate parser and test source, fix the Python test-build flag and prevent
  the PowerShell test build from overwriting the normal release output.

## Source and attribution

The accompanying source corresponds to this DLL. Original Extra Options code:
Arthur / RokkuDayo, https://github.com/RokkuDayo/ForzaHorizon2_ExtraOptions.
This combined build is distributed under GPL-3.0; see [LICENSE](../LICENSE). Retain attribution,
the licence and corresponding source when distributing a modified release.

The new presence implementation is in source/features/discord_presence.c,
source/features/presence_json.c, their headers and source/features/presence_cars.h. Other changes add
startup integration, freestanding memory/stack helpers, native build/tests and
presence-only defaults when Extra Options configuration is absent.

Build with Zig 0.15.2, running these commands from the `source` folder:

    powershell -NoProfile -ExecutionPolicy Bypass -File build-presence.ps1 -Zig C:\path\to\zig.exe

Output: build/azimuth-tls.dll. To build and run the isolated tests:

    powershell -NoProfile -ExecutionPolicy Bypass -File build-presence.ps1 -Zig C:\path\to\zig.exe -Test
    powershell -NoProfile -ExecutionPolicy Bypass -File test-presence.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File tests\verify-release.ps1

The test build skips normal initialization and never connects to real Discord;
its mock uses a uniquely named local pipe. The compiler is not bundled.

No game executable, saves, database, artwork or textures are distributed. The
car-name lookup and six destination coordinates are embedded metadata.
