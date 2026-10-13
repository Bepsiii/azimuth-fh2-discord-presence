param([Parameter(Mandatory=$true)][string]$AzimuthFolder)
$ErrorActionPreference='Stop'
if(Get-Process azimuth -ErrorAction SilentlyContinue){throw 'Close Azimuth before uninstalling.'}
$root=(Resolve-Path -LiteralPath $AzimuthFolder).Path
if(-not (Test-Path -LiteralPath (Join-Path $root 'azimuth.exe'))){throw 'Select the folder containing azimuth.exe.'}
$bin=Join-Path $root 'data\bin';$backup=Join-Path $bin 'FH2DiscordPresence-backup'
$records=Get-Content -LiteralPath (Join-Path $backup 'restore.json') -Raw | ConvertFrom-Json
foreach($record in $records){
    if($record.name -notin @('azimuth-tls.dll','FH2DiscordPresence.dll','FH2_ExtraOptions.dll','FH2_DiscordPresence.ini')){throw 'Unexpected backup entry.'}
    $target=Join-Path $bin $record.name
    # Configuration may be edited by the user; keep it if newly created.
    if($record.name -eq 'FH2_DiscordPresence.ini'){continue}
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $record.installed_sha256){throw "Installed file changed: $($record.name). No changes made."}
    if($record.existed -and (Get-FileHash -LiteralPath (Join-Path $backup $record.name) -Algorithm SHA256).Hash -ne $record.original_sha256){throw 'Backup integrity check failed.'}
}
$before=@{}
try {
    foreach($record in $records){
        if($record.name -eq 'FH2_DiscordPresence.ini'){continue}
        $target=Join-Path $bin $record.name;$before[$record.name]=[IO.File]::ReadAllBytes($target)
        if($record.existed){Copy-Item -LiteralPath (Join-Path $backup $record.name) -Destination $target -Force}else{Remove-Item -LiteralPath $target}
    }
    $archive=Join-Path $bin ('FH2DiscordPresence-backup-restored-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff'))
    Move-Item -LiteralPath $backup -Destination $archive
} catch {foreach($name in $before.Keys){[IO.File]::WriteAllBytes((Join-Path $bin $name),$before[$name])};throw}
Write-Host 'Previous DLL setup restored. Configuration and backup archive retained.'
