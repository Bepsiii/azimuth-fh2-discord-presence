param([string]$Dll=(Join-Path $PSScriptRoot 'build-test\presence-test.dll'))
$ErrorActionPreference='Stop'
$dllPath=(Resolve-Path -LiteralPath $Dll).Path
$code=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'tests\PresenceChecks.cs') -Raw

Add-Type -TypeDefinition ($code.Replace('DLL_PATH',$dllPath))
$testDirectory=Join-Path $PSScriptRoot 'build-test\fixtures'
New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
[NativePresenceChecks]::CheckJson()
$json=[NativePresenceChecks]::CheckMemory($testDirectory)
$result=$json | ConvertFrom-Json
if($result.state -ne '2011 BMW Z4 sDrive35is' -or $result.details -ne 'Free roam | Near Castelletto'){throw 'Native replay output mismatch'}
'PASS: native local-player selection, traffic filtering, capture replay, pause/race/Island, ambiguity, JSON'
'PASS: actual worker loop, rate limit, pause, car change, clear, reconnect and acknowledgement timeout (accelerated clocks)'
[NativePresenceChecks]::CheckIpc()
[NativePresenceChecks]::CheckRejectedReplies()
[NativePresenceChecks]::CheckConfiguration($testDirectory)
[NativePresenceChecks]::CheckDefaults($PSScriptRoot)
