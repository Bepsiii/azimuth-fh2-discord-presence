param(
    [Parameter(Mandatory=$true)][string]$Zig,
    [string]$Output,
    [switch]$Test
)
$ErrorActionPreference='Stop'
$compiler=(Resolve-Path -LiteralPath $Zig).Path
$buildDir=Join-Path $PSScriptRoot $(if($Test){'build-test'}else{'build'})
if(!$Output){$Output=Join-Path $buildDir $(if($Test){'presence-test.dll'}else{'azimuth-tls.dll'})}
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $Output) -Force | Out-Null
$sources=@(
    'dllmain.c','runtime.c','settings.c','support.c','support_runtime.c','logging.c','patch_engine.c',
    'verification.c','sha256.c','patches_generated.c','features/garage.c',
    'features/languages.c','features/road_placement.c','features/gamerpic.c',
    'features/treasure_map.c','features/autodrive.c','features/autodrive_game.c',
    'features/autodrive_notice.c','features/autodrive_ui.c','features/png_validation.c',
    'features/png_decode.c','features/discord_presence.c','features/presence_json.c','adapters/azimuth.c',
    'adapters/azimuth_host.c','adapters/azimuth_livery.c','adapters/azimuth_save.c'
)
$compilerDir=Split-Path -Parent $compiler
$flags=@('clang','-target','x86_64-w64-windows-gnu','-c','-ffreestanding','-fno-builtin',
    '-fno-stack-protector','-funwind-tables','-O2','-Wall','-Wextra','-Werror',
    '-I',$PSScriptRoot,'-isystem',(Join-Path $compilerDir 'lib\include'),
    '-isystem',(Join-Path $compilerDir 'lib\libc\include\any-windows-any'),'-DFH2_AZIMUTH=1')
if($Test){$flags+=@('-DFH2_TWEAKS_TEST=1','-DFH2_PRESENCE_TEST=1')}
$objects=@()
foreach($source in $sources){
    $object=Join-Path $buildDir (([IO.Path]::GetFileNameWithoutExtension($source))+'.obj')
    & $compiler @flags (Join-Path $PSScriptRoot $source) '-o' $object
    if($LASTEXITCODE){throw "Compilation failed: $source"}
    $objects+=$object
}
$importLibrary=Join-Path $buildDir 'kernel32.lib'
$probeObject=Join-Path $buildDir 'stack_probe.obj'
& $compiler 'clang' '-target' 'x86_64-w64-windows-gnu' '-x' 'assembler' '-c' (Join-Path $PSScriptRoot 'stack_probe.S') '-o' $probeObject
if($LASTEXITCODE){throw 'Stack probe compilation failed'}
$objects+=$probeObject
& $compiler 'dlltool' '-m' 'i386:x86-64' '-d' (Join-Path $PSScriptRoot 'kernel32.def') '-l' $importLibrary
if($LASTEXITCODE){throw 'Import library generation failed'}
$link=@('lld-link','/dll','/entry:DllMain','/machine:x64','/nodefaultlib','/dynamicbase','/nxcompat',
    ('/out:'+$Output),('/implib:'+(Join-Path $buildDir 'extra_options.lib')),'/include:_tls_used','/timestamp:0')
& $compiler @link @objects $importLibrary
if($LASTEXITCODE){throw 'DLL linking failed'}
Write-Host "Built: $Output"
