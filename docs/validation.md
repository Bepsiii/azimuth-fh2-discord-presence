# RC2 validation - 12 October 2026

Target: Azimuth 0.9.7, Forza Horizon 2 Xbox One build 1.0.0.64.
Base: Extra Options 0.2.34, with the native Discord presence extension.

## Automated results

- Release and test DLLs compile with Zig 0.15.2, warnings treated as errors.
- Synthetic game memory: native local-player selection, second-player-list
  branch, traffic exclusion, ambiguous ownership, missing manager, empty player
  list, inaccessible/oversized vectors, invalid and NULL CAR IDs, invalid
  coordinates, known/unknown car names and UTF-8 display names.
- Recorded BMW Z4/Castelletto values reproduce the expected presence text.
- Pause, race and Storm Island labels pass formatting checks.
- JSON: root/nested fields, duplicate requested fields, escaped keys, Unicode,
  surrogate pairs, malformed strings/numbers/arrays, excessive nesting, trailing
  input, output limits and null fields.
- Native IPC: handshake, fragmented input, SET_ACTIVITY, ping/pong, matching
  acknowledgement and clear.
- Rejection: wrong nonce, nested READY lookalike, malformed JSON, oversized
  frames, explicit ERROR, broken connection and invalid opcode.
- Actual worker loop, with accelerated timing and private mock pipes: pause,
  update rate limit, reconnect, unchanged session timestamp, missing-acknowledgement
  timeout, unavailable-gameplay clear and changed car.
- Configuration: disabled presence, invalid/overflowing application ID,
  oversized strings, invalid boolean values and absent Extra Options INI.
- Release binary: x64 PE DLL, ASLR/NX, unwind tables, only Kernel32 imports,
  exactly the expected three exports, no test exports, 336-byte zero TLS
  template aligned to 16 bytes and no TLS callbacks.
- Rebuilding the included source into a second azimuth-tls.dll produces the
  same SHA-256. The output basename must match because PE exports record it.

Release DLL SHA-256:

    bcdcd05589962d198382dbf94ffb4e862ab702e2205eb78cb64d60c32cfa2fa8

The tests use the real native implementation with synthetic game objects and a
local mock of Discord. They do not prove compatibility with every live game
transition, machine, Discord build, or Azimuth version. The upstream gameplay
features have not received a new exhaustive live regression test in this review.
The Windows PowerShell build was executed; the updated Python build script was
reviewed but was not executed in this environment.

## Live evidence and remaining release check

The installed RC1 binary matched the previous release checksum and its log
contained real Discord SET_ACTIVITY acknowledgements. RC2 is a new binary and
has not yet been validated in the live game.

Before describing RC2 as stable:

1. Stop the PowerShell companion, close Azimuth, back up the installed DLL and
   install this DLL in data/bin. Keep the existing Extra Options INI and gamerpic.
2. In free roam, confirm the correct car, artwork and approximate location;
   change cars, enter a race/garage and pause. Updates can take about 15 seconds.
3. Restart Discord while Azimuth remains open. Confirm presence returns and
   the timer remains consistent. Close the game and confirm presence disappears.
4. Confirm the enabled Extra Options features you use still behave correctly.

If a check fails, restore the previous DLL and retain FH2_DiscordPresence.log
and FH2_ExtraOptions.log before restarting, since logs can be replaced at startup.

## Distribution contents

The ZIP includes the release DLL, optional presence INI, instructions, this
validation record, checksums, GPL-3.0 licence and corresponding source/tests.
It excludes compiler binaries, object files, test DLLs, captures, local settings,
logs, game executables, saves, textures and artwork files. Public application ID
and artwork keys are intentional; account tokens and client secrets are not used.
