#Requires -Version 5.1
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$LayerInstallDirectory,[string]$ArchivePath,[ValidatePattern('^[a-fA-F0-9]{40}$')][string]$SourceCommit)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AstroBot.Common.ps1')
$info=Get-AstroProvenance $PSScriptRoot
$output=[IO.Path]::GetFullPath($OutputDirectory)
$layer=[IO.Path]::GetFullPath($LayerInstallDirectory)
if(Test-Path -LiteralPath $output){if(@(Get-ChildItem -LiteralPath $output -Force).Count -gt 0){throw 'OutputDirectory must be new or empty; runtime/user data are never packaged.'}}
if($ArchivePath -and (Test-Path -LiteralPath $ArchivePath)){throw 'The output ZIP already exists.'}
$names=@('Setup-AstroBot.ps1','Start-AstroBot.ps1','AstroBot.Common.ps1','Patch-RumbleFallback.ps1','Play-AstroBot.cmd','usage.md','provenance.json','preset.json')
foreach($name in $names){if(-not(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)){throw "Missing kit source: $name"}}
foreach($name in @('astro_frame.dll','VK_LAYER_ASTRO_frame.json')){if(-not(Test-Path -LiteralPath (Join-Path $layer $name) -PathType Leaf)){throw "Missing built layer: $name"}}
$manifest=Read-AstroJson (Join-Path $layer 'VK_LAYER_ASTRO_frame.json')
if($manifest.layer.name -ne 'VK_LAYER_ASTRO_frame' -or $manifest.layer.library_path -notin @('.\astro_frame.dll','./astro_frame.dll','astro_frame.dll')){throw 'Unexpected layer manifest.'}
$license=Join-Path $PSScriptRoot '..\..\LICENSE'
if(-not(Test-Path -LiteralPath $license -PathType Leaf)){throw 'The repository LICENSE is missing.'}
$null=New-Item -ItemType Directory -Path $output -Force
foreach($name in $names){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $output $name)}
$null=New-Item -ItemType Directory -Path (Join-Path $output 'profiles')
$index=Read-AstroJson (Join-Path $PSScriptRoot 'profiles\index.json')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'profiles\index.json') -Destination (Join-Path $output 'profiles\index.json')
foreach($profile in $index.profiles){
 if($profile.file -notmatch '^PPSA21564_01\.007\.000_[A-Za-z0-9_]+\.json$'){throw 'Invalid profile filename.'}
 $path=Join-Path $PSScriptRoot ('profiles\'+$profile.file)
 Assert-AstroHash $path $profile.sha256
 Copy-Item -LiteralPath $path -Destination (Join-Path $output ('profiles\'+$profile.file))
}
$null=New-Item -ItemType Directory -Path (Join-Path $output 'frame-layer')
foreach($name in @('astro_frame.dll','VK_LAYER_ASTRO_frame.json')){Copy-Item -LiteralPath (Join-Path $layer $name) -Destination (Join-Path $output ('frame-layer\'+$name))}
Copy-Item -LiteralPath $license -Destination (Join-Path $output 'LICENSE.txt')
$files=foreach($file in @(Get-ChildItem -LiteralPath $output -File -Recurse)){
 $relative=$file.FullName.Substring($output.TrimEnd('\','/').Length+1).Replace('\','/')
 [pscustomobject]@{path=$relative;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
[pscustomobject]@{schema=1;kind='Overlay only: no emulator, game, saves or caches';base_commit=$info.base_commit;source_commit=$SourceCommit;source_url=$(if($SourceCommit){$info.repository+'/tree/'+$SourceCommit}else{$null});upstream_archive_sha256=$info.archive_sha256;files=@($files)} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'overlay-info.json') -Encoding UTF8
if($ArchivePath){
 $ArchivePath=[IO.Path]::GetFullPath($ArchivePath)
 $null=New-Item -ItemType Directory -Path (Split-Path -Parent $ArchivePath) -Force
 Compress-Archive -LiteralPath @(Get-ChildItem -LiteralPath $output | ForEach-Object FullName) -DestinationPath $ArchivePath
 Write-Output "Created overlay ZIP: $ArchivePath"
}else{Write-Output "Created overlay directory: $output"}
