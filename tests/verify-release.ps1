param([string]$Dll=(Join-Path $PSScriptRoot '..\build\azimuth-tls.dll'))
$ErrorActionPreference='Stop'
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Dll).Path)
function U16([int]$offset){[BitConverter]::ToUInt16($bytes,$offset)}
function U32([int]$offset){[BitConverter]::ToUInt32($bytes,$offset)}
function U64([int]$offset){[BitConverter]::ToUInt64($bytes,$offset)}
function Require([bool]$condition,[string]$message){if(!$condition){throw $message}}
Require ((U16 0) -eq 0x5a4d) 'Missing DOS signature'
$pe=U32 0x3c
Require ((U32 $pe) -eq 0x4550) 'Missing PE signature'
Require ((U16 ($pe+4)) -eq 0x8664) 'DLL must be x64'
Require (((U16 ($pe+22)) -band 0x2000) -ne 0) 'PE is not a DLL'
Require ((U32 ($pe+8)) -eq 0) 'Build timestamp is not deterministic'
$optional=$pe+24
Require ((U16 $optional) -eq 0x20b) 'PE32+ required'
Require (((U16 ($optional+70)) -band 0x140) -eq 0x140) 'ASLR/NX flags missing'
$imageBase=U64 ($optional+24)
$sectionOffset=$optional+(U16 ($pe+20))
$sections=@()
for($i=0;$i -lt (U16 ($pe+6));$i++){
    $at=$sectionOffset+40*$i
    $sections+=@{Rva=(U32 ($at+12));Size=[Math]::Max((U32 ($at+8)),(U32 ($at+16)));Raw=(U32 ($at+20))}
}
function FileOffset([uint64]$rva){
    foreach($section in $sections){
        if($rva -ge $section.Rva -and $rva -lt $section.Rva+$section.Size){return [int]($section.Raw+$rva-$section.Rva)}
    }
    throw ('Unmapped RVA: '+$rva)
}
function CString([int]$offset){
    $end=$offset
    while($end -lt $bytes.Length -and $bytes[$end]){$end++}
    Require ($end -lt $bytes.Length) 'Unterminated PE string'
    return [Text.Encoding]::ASCII.GetString($bytes,$offset,$end-$offset)
}
$exportsAt=FileOffset (U32 ($optional+112))
Require ((U32 ($exportsAt+20)) -eq 3) 'Unexpected export function count'
Require ((U32 ($exportsAt+24)) -eq 3) 'Unexpected export name count'
$namesAt=FileOffset (U32 ($exportsAt+32))
$names=@(for($i=0;$i -lt 3;$i++){CString (FileOffset (U32 ($namesAt+4*$i)))})
Require (($names -join ',') -eq 'Fh2TweaksStatus,azimuth_tls_attached,azimuth_tls_index') 'Unexpected exports/test entrypoints leaked'
$importsAt=FileOffset (U32 ($optional+120))
$imports=@()
while((U32 ($importsAt+12)) -ne 0){
    $imports+=CString (FileOffset (U32 ($importsAt+12)))
    $importsAt+=20
}
Require ($imports.Count -eq 1 -and $imports[0] -ieq 'KERNEL32.dll') 'Unexpected runtime dependency'
$tlsAt=FileOffset (U32 ($optional+112+9*8))
$tlsStart=U64 $tlsAt
$tlsEnd=U64 ($tlsAt+8)
Require (($tlsEnd-$tlsStart) -eq 336) 'Wrong TLS template size'
Require (($tlsStart % 16) -eq 0) 'TLS template is not aligned'
Require ((U32 ($tlsAt+32)) -eq 0) 'Unexpected TLS zero fill'
Require (((U32 ($tlsAt+36)) -band 0x00f00000) -eq 0x00500000) 'Wrong TLS alignment flag'
$template=FileOffset ($tlsStart-$imageBase)
for($i=0;$i -lt 336;$i++){Require ($bytes[$template+$i] -eq 0) 'TLS template must be zero'}
$callbacks=FileOffset ((U64 ($tlsAt+24))-$imageBase)
Require ((U64 $callbacks) -eq 0) 'Unexpected TLS callback'
Require ((U32 ($optional+112+3*8)) -ne 0) 'Missing x64 unwind directory'
'PASS: release x64 ABI, TLS size/alignment/callbacks, exports, Kernel32-only imports, ASLR/NX, unwind tables and deterministic timestamp'
(Get-FileHash -LiteralPath $Dll -Algorithm SHA256).Hash.ToLowerInvariant()
