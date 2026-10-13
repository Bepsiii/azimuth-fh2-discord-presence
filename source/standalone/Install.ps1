param([Parameter(Mandatory=$true)][string]$AzimuthFolder,[switch]$WithoutExtraOptions)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $AzimuthFolder).Path
if(-not (Test-Path -LiteralPath (Join-Path $root 'azimuth.exe'))){throw 'Select the folder containing azimuth.exe.'}
if(Get-Process azimuth -ErrorAction SilentlyContinue){throw 'Close Azimuth before installing.'}
$bin=Join-Path $root 'data\bin'
if(-not (Test-Path -LiteralPath $bin)){throw 'Azimuth data/bin directory not found.'}
$backup=Join-Path $bin 'FH2DiscordPresence-backup'
if(Test-Path -LiteralPath $backup){throw 'A presence backup already exists. Uninstall the previous standalone version first.'}
$extra=(-not $WithoutExtraOptions) -and (Test-Path -LiteralPath (Join-Path $bin 'FH2_ExtraOptions.ini'))
$variant=if($extra){'with-extra-options'}else{'presence-only'}
$items=@([pscustomobject]@{source="loaders\$variant\azimuth-tls.dll";name='azimuth-tls.dll'},[pscustomobject]@{source='modules\FH2DiscordPresence.dll';name='FH2DiscordPresence.dll'})
if($extra){$items+=[pscustomobject]@{source='modules\FH2_ExtraOptions.dll';name='FH2_ExtraOptions.dll'}}
if(-not (Test-Path -LiteralPath (Join-Path $bin 'FH2_DiscordPresence.ini'))){$items+=[pscustomobject]@{source='FH2_DiscordPresence.ini';name='FH2_DiscordPresence.ini'}}
$checks=Get-Content -LiteralPath "$PSScriptRoot\checksums.json" -Raw | ConvertFrom-Json
foreach($item in $items){
    $source=Join-Path $PSScriptRoot $item.source
    $expected=@($checks | Where-Object path -eq $item.source)
    if($expected.Count -ne 1 -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $expected[0].sha256){throw "Package integrity check failed: $($item.source)"}
}
New-Item -ItemType Directory -Path $backup | Out-Null
$records=@();$written=@()
try {
    foreach($item in $items){
        $target=Join-Path $bin $item.name;$exists=Test-Path -LiteralPath $target
        $oldHash=$null
        if($exists){Copy-Item -LiteralPath $target -Destination (Join-Path $backup $item.name);$oldHash=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash}
        $records+=[pscustomobject]@{name=$item.name;existed=$exists;original_sha256=$oldHash;installed_sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $item.source) -Algorithm SHA256).Hash}
    }
    [IO.File]::WriteAllText((Join-Path $backup 'restore.json'),($records | ConvertTo-Json -Depth 4),(New-Object Text.UTF8Encoding($false)))
    foreach($item in $items){$written+=$item.name;Copy-Item -LiteralPath (Join-Path $PSScriptRoot $item.source) -Destination (Join-Path $bin $item.name) -Force}
} catch {
    foreach($name in $written){$record=$records | Where-Object name -eq $name;$target=Join-Path $bin $name;if($record.existed){Copy-Item -LiteralPath (Join-Path $backup $name) -Destination $target -Force}else{Remove-Item -LiteralPath $target -ErrorAction SilentlyContinue}}
    # Preserve the backup if installation fails, for manual recovery.
    throw
}
Write-Host "Installed FH2 Discord Presence 1.0.0 ($variant). Start Azimuth normally."
