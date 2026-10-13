$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$package=Join-Path $project 'releases\FH2-Discord-Presence-1.0.0'
foreach($extra in $false,$true){
    $fixture=Join-Path $PSScriptRoot ('build\install-fixture-'+[Guid]::NewGuid().ToString('N'))
    $bin=Join-Path $fixture 'data\bin';New-Item -ItemType Directory -Path $bin -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixture 'azimuth.exe'),'fixture')
    [IO.File]::WriteAllText((Join-Path $bin 'azimuth-tls.dll'),'original loader')
    [IO.File]::WriteAllText((Join-Path $bin 'FH2_DiscordPresence.ini'),'preserved presence settings')
    if($extra){[IO.File]::WriteAllText((Join-Path $bin 'FH2_ExtraOptions.ini'),'preserved extra settings')}
    $original=(Get-FileHash -LiteralPath (Join-Path $bin 'azimuth-tls.dll')).Hash
    & "$package\Install.ps1" -AzimuthFolder $fixture
    if(-not(Test-Path -LiteralPath (Join-Path $bin 'FH2DiscordPresence.dll'))){throw 'Presence module not installed'}
    if((Test-Path -LiteralPath (Join-Path $bin 'FH2_ExtraOptions.dll')) -ne $extra){throw 'Wrong Extra Options selection'}
    if([IO.File]::ReadAllText((Join-Path $bin 'FH2_DiscordPresence.ini')) -ne 'preserved presence settings'){throw 'Settings overwritten'}
    & "$package\Uninstall.ps1" -AzimuthFolder $fixture
    if((Get-FileHash -LiteralPath (Join-Path $bin 'azimuth-tls.dll')).Hash -ne $original){throw 'Original loader not restored'}
    if(Test-Path -LiteralPath (Join-Path $bin 'FH2DiscordPresence.dll')){throw 'Module left behind'}
}
'PASS: both installer variants preserve settings and restore the original loader'
