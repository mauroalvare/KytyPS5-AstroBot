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
$guards=@{
 0x10796F0='5541574156415541545657534881ec98000000'
 0x1079795='807e04000f8494000000'
 0x1079833='4489e9e8c58c09ff83f8060f845bffffff'
 0x1079B71='41b964000000e8'
}
foreach($offset in $guards.Keys){
 $hex=$guards[$offset]
 for($i=0;$i -lt $hex.Length/2;$i++){
  if($bytes[$offset+$i] -ne [Convert]::ToByte($hex.Substring($i*2,2),16)){throw 'Rumble control-flow byte guard failed.'}
 }
}
for($i=0;$i -lt 6;$i++){$bytes[0x107983E+$i]=0x90}
$sha=[Security.Cryptography.SHA256]::Create()
try{$hash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
if($hash -ne $info.rumble_executable_sha256){throw 'The patched output hash is unexpected.'}
$plan=[pscustomobject]@{Input=$InputPath;Output=$OutputPath;ChangedBytes=6;SHA256=$hash;ControllerTested=$false;ProcessStarted=$false}
if($PrepareOnly){$plan | ConvertTo-Json;return}
if(Test-Path -LiteralPath $OutputPath){Assert-AstroHash $OutputPath $hash;Write-Output 'The matching optional rumble copy already exists.';return}
[IO.File]::WriteAllBytes([IO.Path]::GetFullPath($OutputPath),$bytes)
$plan | ConvertTo-Json
