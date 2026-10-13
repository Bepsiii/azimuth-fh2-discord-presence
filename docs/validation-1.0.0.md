# Validation 1.0.0

Passed Windows PowerShell 5.1 checks for strict JSON, local player/car selection, pause/race states, IPC fragmentation and invalid replies, reconnects and rate limits. Both production loader variants passed native LoadLibraryExW tests, including the required 336-byte TLS template and independent Extra Options loading. Both installers preserved existing INI settings and restored original DLLs in disposable fixtures.

The presence DLL imports only Kernel32 and has no game patches. It validates the game executable SHA256 and mapped image before polling.

On 13 October 2026, the user confirmed the standalone 1.0.0 package working in-game after reinstalling Extra Options and applying the presence installer. Version 1.0.0 is now released as stable. The installer and DLL downloads are the same checksum-verified packages used for that test.
