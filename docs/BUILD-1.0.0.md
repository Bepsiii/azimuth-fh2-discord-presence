# Build

Requires PowerShell, Zig 0.15.2 and Python 3 for the optional Extra Options build.

```powershell
.\source\build-presence.ps1 -Zig C:\tools\zig\zig.exe
python .\third-party\ForzaHorizon2_ExtraOptions\build.py C:\tools\zig\zig.exe --azimuth --output .\source\build\FH2_ExtraOptions.dll --build-dir .\source\build\extra-options
.\source\build-presence.ps1 -Zig C:\tools\zig\zig.exe -Test
.\source\test-presence.ps1
.\source\test-loader.ps1 -Variant presence-only
.\source\test-loader.ps1 -Variant with-extra-options
```

Only the loader owns the 336-byte Azimuth TLS template. Presence starts through an exported function after dependencies are loaded; its DllMain only records the module handle. Dependency resolution uses Azimuth's native LoadLibraryEx flags (DLL directory and System32).
