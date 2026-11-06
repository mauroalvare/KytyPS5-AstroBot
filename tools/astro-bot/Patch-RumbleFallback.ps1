#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$InputPath,[string]$OutputPath,[switch]$PrepareOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AstroBot.Common.ps1')
$info=Read-AstroJson (Join-Path $PSScriptRoot 'provenance.json')
Assert-AstroHash $InputPath $info.release_executable_sha256
if(-not $OutputPath){$OutputPath=Join-Path (Split-Path -Parent ([IO.Path]::GetFullPath($InputPath))) 'kyty_emulator_rumble.exe'}
if([IO.Path]::GetFullPath($InputPath) -ieq [IO.Path]::GetFullPath($OutputPath)){throw 'The original executable must remain unchanged.'}
$bytes=[IO.File]::ReadAllBytes([IO.Path]::GetFullPath($InputPath))
# These raw file offsets match only the pinned upstream release. Speaker ports
# bypass this gate; falling through uses its existing PCM peak-envelope route.
# U59 int21 (kyty_emulator.exe 6ba3fb46...): Queue prologue, the speaker-port test, the
# PS5 pad-type gate (call SDL_GetGamepadTypeForID; cmp eax,6; je haptic path) and the
# SDL_RumbleGamepad(..., 100) call of the fallback that the gate otherwise skips.
$guards=@{
 0x11D87F0='554157415641554154565753b8a8400000'
 0x11D88C1='807e04004889b5f03f00000f8405010000'
 0x11D89D7='4489f1e8e129fcfe83f8060f84eafeffff'
 0x11D9043='4889c141b964000000e8'
}
foreach($offset in $guards.Keys){
 $hex=$guards[$offset]
 for($i=0;$i -lt $hex.Length/2;$i++){
  if($bytes[$offset+$i] -ne [Convert]::ToByte($hex.Substring($i*2,2),16)){throw 'Rumble control-flow byte guard failed.'}
 }
}
# NOP the six-byte je so every non-speaker port takes the rumble fallback.
for($i=0;$i -lt 6;$i++){$bytes[0x11D89E2+$i]=0x90}
$sha=[Security.Cryptography.SHA256]::Create()
try{$hash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
if($hash -ne $info.rumble_executable_sha256){throw 'The patched output hash is unexpected.'}
$plan=[pscustomobject]@{Input=$InputPath;Output=$OutputPath;ChangedBytes=6;SHA256=$hash;ControllerTested=$false;ProcessStarted=$false}
if($PrepareOnly){$plan | ConvertTo-Json;return}
if(Test-Path -LiteralPath $OutputPath){Assert-AstroHash $OutputPath $hash;Write-Output 'The matching optional rumble copy already exists.';return}
[IO.File]::WriteAllBytes([IO.Path]::GetFullPath($OutputPath),$bytes)
$plan | ConvertTo-Json
