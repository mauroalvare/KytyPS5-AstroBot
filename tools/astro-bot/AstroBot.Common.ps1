#Requires -Version 5.1
# Helpers are data/file operations only; loading this file starts no processes.
Import-Module Microsoft.PowerShell.Utility
function Read-AstroJson { param([string]$Path) Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json }
function Assert-AstroHash {
 param([string]$Path,[string]$Expected)
 if(-not(Test-Path -LiteralPath $Path -PathType Leaf)){throw "Missing file: $Path"}
 if((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Expected){throw "SHA256 verification failed: $Path"}
}
function Get-AstroProvenance {
 param([string]$Root)
 $info=Read-AstroJson (Join-Path $Root 'provenance.json')
 Assert-AstroHash (Join-Path $Root 'preset.json') $info.preset_sha256
 Assert-AstroHash (Join-Path $Root 'profiles\index.json') $info.profile_index_sha256
 return $info
}
function Resolve-AstroMode {
 param([string]$Mode)
 $resolution='native';$rate=60;$lighting='nonRT';$experimental=$false
 switch($Mode){
  'Stable' {}
  '1440p' {$resolution='2560x1440';$experimental=$true}
  '90' {$resolution='2560x1440';$rate=90;$experimental=$true}
  '120' {$resolution='2560x1440';$rate=120;$experimental=$true}
  'Ultrawide' {$resolution='3440x1440';$experimental=$true}
  'DeferredGI' {$resolution='2560x1440';$lighting='deferredGI';$experimental=$true}
  default {throw 'Unknown mode.'}
 }
 return [pscustomobject]@{Resolution=$resolution;Rate=$rate;Lighting=$lighting;Experimental=$experimental;Aspect=$(if($Mode -eq 'Ultrawide'){'source'}else{'16:9'})}
}
function ConvertTo-AstroCommandLine {
 param([string[]]$Arguments)
 $quoted=foreach($argument in $Arguments){
  $escaped=[regex]::Replace($argument,'(\\*)"','$1$1\"')
  $escaped=[regex]::Replace($escaped,'(\\+)$','$1$1')
  '"'+$escaped+'"'
 }
 return ($quoted -join ' ')
}
function Assert-AstroLayer {
 param([string]$Root)
 $manifest=Join-Path $Root 'frame-layer\VK_LAYER_ASTRO_frame.json'
 $dll=Join-Path $Root 'frame-layer\astro_frame.dll'
 $infoPath=Join-Path $Root 'overlay-info.json'
 if(-not(Test-Path -LiteralPath $infoPath)){ $infoPath=Join-Path $Root '.local\layer-info.json' }
 if(-not(Test-Path -LiteralPath $infoPath)){throw 'Frame-layer verification metadata is missing. Run Setup-AstroBot.ps1 first.'}
 $info=Read-AstroJson $infoPath
 foreach($relative in @('frame-layer/astro_frame.dll','frame-layer/VK_LAYER_ASTRO_frame.json')){
  $entry=@($info.files | Where-Object path -EQ $relative)
  if($entry.Count -ne 1){throw "Missing frame-layer metadata: $relative"}
  Assert-AstroHash (Join-Path $Root $relative) $entry[0].sha256
 }
 $layer=Read-AstroJson $manifest
 if($layer.layer.name -ne 'VK_LAYER_ASTRO_frame' -or $layer.layer.library_path -notin @('.\astro_frame.dll','./astro_frame.dll','astro_frame.dll')){throw 'Unexpected Vulkan layer manifest.'}
}
function Assert-AstroIdle {
 param([string[]]$ExecutablePaths,[object[]]$Processes)
 foreach($process in $Processes){
  try{$path=[string]$process.Path}catch{throw 'Cannot inspect a running emulator; close it before starting another kit run.'}
  if(-not $path){throw 'Cannot inspect a running emulator; close it before starting another kit run.'}
  foreach($expected in $ExecutablePaths){
   if([IO.Path]::GetFullPath($path) -ieq [IO.Path]::GetFullPath($expected)){throw "An emulator for this kit is already open (PID $($process.Id))."}
  }
 }
}
function Get-AstroPatchState {
 param([string[]]$Expected,[string[]]$Lines)
 $applied=@($Expected | Where-Object {$Lines -contains ('Successfully applied cheat: '+$_)})
 return [pscustomobject]@{Applied=$applied;Status=$(if($Expected.Count -gt 0 -and $applied.Count -eq $Expected.Count){'all expected mods logged'}else{'pending or incomplete'})}
}
