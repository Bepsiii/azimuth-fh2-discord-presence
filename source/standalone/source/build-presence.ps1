param([Parameter(Mandatory=$true)][string]$Zig,[switch]$Test)
$ErrorActionPreference='Stop'
$compiler=(Resolve-Path -LiteralPath $Zig).Path
$compilerDirectory=Split-Path -Parent $compiler
$out=Join-Path $PSScriptRoot $(if($Test){'build-test'}else{'build'})
New-Item -ItemType Directory -Path $out -Force | Out-Null
$flags=@('clang','-target','x86_64-w64-windows-gnu','-c','-ffreestanding','-fno-builtin','-fno-stack-protector','-funwind-tables','-O2','-Wall','-Wextra','-Werror','-I',$PSScriptRoot,'-isystem',(Join-Path $compilerDirectory 'lib\include'),'-isystem',(Join-Path $compilerDirectory 'lib\libc\include\any-windows-any'))
if($Test){$flags+=@('-DFH2_PRESENCE_TEST=1','-DFH2_TWEAKS_TEST=1')}
$objects=@()
foreach($source in 'bootstrap.c','support.c','support_runtime.c','sha256.c','features/discord_presence.c','features/presence_json.c'){
    $object=Join-Path $out ([IO.Path]::GetFileNameWithoutExtension($source)+'.obj')
    & $compiler @flags (Join-Path $PSScriptRoot $source) '-o' $object
    if($LASTEXITCODE){throw "Compile failed: $source"};$objects+=$object
}
$probe=Join-Path $out 'stack_probe.obj'
& $compiler clang -target x86_64-w64-windows-gnu -x assembler -c "$PSScriptRoot\stack_probe.S" -o $probe
if($LASTEXITCODE){throw 'Stack probe failed'};$objects+=$probe
$kernel=Join-Path $out 'kernel32.lib'
& $compiler dlltool -m i386:x86-64 -d "$PSScriptRoot\kernel32.def" -l $kernel
if($LASTEXITCODE){throw 'Import library failed'}
$dll=Join-Path $out $(if($Test){'presence-test.dll'}else{'FH2DiscordPresence.dll'})
& $compiler lld-link /dll /entry:DllMain /machine:x64 /nodefaultlib /dynamicbase /nxcompat /timestamp:0 ('/out:'+$dll) ('/implib:'+(Join-Path $out 'presence.lib')) @objects $kernel
if($LASTEXITCODE){throw 'Presence linking failed'}
if(-not $Test){
    [IO.File]::WriteAllText((Join-Path $out 'extra-options.def'),"LIBRARY FH2_ExtraOptions.dll`nEXPORTS`n Fh2TweaksStatus`n")
    & $compiler dlltool -m i386:x86-64 -d (Join-Path $out 'extra-options.def') -l (Join-Path $out 'extra-options.lib')
    if($LASTEXITCODE){throw 'Extra Options import library failed'}
    foreach($variant in 'presence-only','with-extra-options'){
        $variantDirectory=Join-Path $out $variant;New-Item -ItemType Directory -Path $variantDirectory -Force | Out-Null
        $object=Join-Path $variantDirectory 'loader.obj';$compileFlags=@($flags)
        if($variant -eq 'with-extra-options'){$compileFlags+='-DWITH_EXTRA_OPTIONS=1'}
        & $compiler @compileFlags "$PSScriptRoot\loader\loader.c" -o $object
        if($LASTEXITCODE){throw 'Loader compilation failed'}
        $imports=@((Join-Path $out 'presence.lib'),$kernel)
        if($variant -eq 'with-extra-options'){$imports+=(Join-Path $out 'extra-options.lib')}
        & $compiler lld-link /dll /entry:DllMain /machine:x64 /nodefaultlib /dynamicbase /nxcompat /include:_tls_used /timestamp:0 ('/out:'+(Join-Path $variantDirectory 'azimuth-tls.dll')) $object @imports
        if($LASTEXITCODE){throw 'Loader linking failed'}
    }
}
Write-Output "Built $dll"
