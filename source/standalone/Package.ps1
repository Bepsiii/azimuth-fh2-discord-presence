$ErrorActionPreference='Stop'
$stage=Join-Path $PSScriptRoot 'releases\FH2-Discord-Presence-1.0.0'
if(Test-Path -LiteralPath $stage){throw 'Package directory already exists.'}
New-Item -ItemType Directory -Path "$stage\modules","$stage\loaders\presence-only","$stage\loaders\with-extra-options" -Force | Out-Null
foreach($name in 'FH2DiscordPresence.dll','FH2_ExtraOptions.dll'){Copy-Item -LiteralPath "$PSScriptRoot\source\build\$name" -Destination "$stage\modules\$name"}
foreach($variant in 'presence-only','with-extra-options'){Copy-Item -LiteralPath "$PSScriptRoot\source\build\$variant\azimuth-tls.dll" -Destination "$stage\loaders\$variant\azimuth-tls.dll"}
Copy-Item -LiteralPath "$PSScriptRoot\Install.ps1","$PSScriptRoot\Uninstall.ps1","$PSScriptRoot\README.md","$PSScriptRoot\LICENSE","$PSScriptRoot\source\FH2_DiscordPresence.ini" -Destination $stage
$checks=@(Get-ChildItem -LiteralPath "$stage\modules","$stage\loaders" -Recurse -File | ForEach-Object {[pscustomobject]@{path=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
$checks+=[pscustomobject]@{path='FH2_DiscordPresence.ini';sha256=(Get-FileHash -LiteralPath "$stage\FH2_DiscordPresence.ini" -Algorithm SHA256).Hash}
[IO.File]::WriteAllText("$stage\checksums.json",(ConvertTo-Json -InputObject $checks -Depth 4),(New-Object Text.UTF8Encoding($false)))
Compress-Archive -LiteralPath $stage -DestinationPath "$PSScriptRoot\releases\FH2-Discord-Presence-1.0.0.zip"
Write-Host "Packaged $stage"
