param([ValidateSet('presence-only','with-extra-options')][string]$Variant='presence-only')
$ErrorActionPreference='Stop'
$build=Join-Path $PSScriptRoot 'build'
$fixture=Join-Path $build ('loader-fixtures\'+$Variant)
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
Copy-Item -LiteralPath "$build\$Variant\azimuth-tls.dll" -Destination $fixture -Force
Copy-Item -LiteralPath "$build\FH2DiscordPresence.dll" -Destination $fixture -Force
if($Variant -eq 'with-extra-options'){Copy-Item -LiteralPath "$build\FH2_ExtraOptions.dll" -Destination $fixture -Force}
Add-Type -Path "$PSScriptRoot\tests\LoaderChecks.cs"
[LoaderChecks]::Check((Join-Path $fixture 'azimuth-tls.dll'),($Variant -eq 'with-extra-options'))
