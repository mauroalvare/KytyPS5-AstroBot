#Requires -Version 5.1
param([string]$ArchivePath,[switch]$IncludeRumbleFallback,[switch]$SkipRuntime,[string]$VulkanHeaders)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AstroBot.Common.ps1')
$info=Get-AstroProvenance $PSScriptRoot
$local=Join-Path $PSScriptRoot '.local'
$runtime=Join-Path $PSScriptRoot 'runtime'
$null=New-Item -ItemType Directory -Path $local -Force
# Release overlays already contain the built DLL/manifest and overlay-info.json.
# A source checkout can build them with clang-cl, Ninja, CMake and Vulkan headers.
$layerDll=Join-Path $PSScriptRoot 'frame-layer\astro_frame.dll'
if(-not(Test-Path -LiteralPath $layerDll)){
 $source=Join-Path $PSScriptRoot 'frame-layer'
 if(-not(Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt'))){throw 'The overlay has no built frame layer. Use the complete release overlay or build from a source checkout.'}
 foreach($command in @('cmake','ninja','clang-cl')){if(-not(Get-Command $command -ErrorAction SilentlyContinue)){throw "Missing $command. Use an x64 Visual Studio developer shell with LLVM clang-cl installed."}}
 $build=Join-Path $local 'frame-layer-build'
 $configure=@('-S',$source,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_COMPILER=clang-cl','-DASTRO_FRAME_BUILD_GPU_TESTS=OFF',"-DCMAKE_INSTALL_PREFIX=$source")
 if($VulkanHeaders){$configure+="-DASTRO_VULKAN_HEADERS=$([IO.Path]::GetFullPath($VulkanHeaders))"}
 & cmake @configure
 if($LASTEXITCODE -ne 0){throw 'Frame-layer CMake configuration failed.'}
 & cmake --build $build --config Release --parallel 1
 if($LASTEXITCODE -ne 0){throw 'Frame-layer compilation failed.'}
 & cmake --install $build --config Release
 if($LASTEXITCODE -ne 0){throw 'Frame-layer installation failed.'}
 $files=foreach($relative in @('frame-layer/astro_frame.dll','frame-layer/VK_LAYER_ASTRO_frame.json')){
  [pscustomobject]@{path=$relative;sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $relative) -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 [pscustomobject]@{kind='Locally built frame layer';files=@($files)} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $local 'layer-info.json') -Encoding UTF8
}
Assert-AstroLayer $PSScriptRoot
if($SkipRuntime){
 if($IncludeRumbleFallback){throw '-IncludeRumbleFallback needs the pinned upstream runtime; omit it with -SkipRuntime.'}
 Write-Output 'Frame layer is ready. Use Start-AstroBot.ps1 -SourceExecutable with your own build; no upstream runtime downloaded.'
 return
}
$runtimeInfoPath=Join-Path $local 'runtime-info.json'
$installed=$false
if(Test-Path -LiteralPath $runtimeInfoPath){
 $runtimeInfo=Read-AstroJson $runtimeInfoPath
 if($runtimeInfo.archive_sha256 -eq $info.archive_sha256){
  foreach($entry in $runtimeInfo.files){Assert-AstroHash (Join-Path $runtime $entry.path) $entry.sha256}
  Assert-AstroHash (Join-Path $runtime 'kyty_emulator.exe') $info.release_executable_sha256
  $installed=$true
 }
}
if(-not $installed){
 if(-not $ArchivePath){
  $download=Join-Path $local 'download'
  $null=New-Item -ItemType Directory -Path $download -Force
  $ArchivePath=Join-Path $download 'KytyPS5-U59-Windows-x64.zip'
  if(-not(Test-Path -LiteralPath $ArchivePath)){
   $partial=$ArchivePath+'.partial'
   $oldProtocol=[Net.ServicePointManager]::SecurityProtocol
   try{[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;Invoke-WebRequest -Uri $info.archive_url -OutFile $partial -UseBasicParsing}
   finally{[Net.ServicePointManager]::SecurityProtocol=$oldProtocol}
   Assert-AstroHash $partial $info.archive_sha256
   Move-Item -LiteralPath $partial -Destination $ArchivePath
  }
 }
 $ArchivePath=[IO.Path]::GetFullPath($ArchivePath)
 Assert-AstroHash $ArchivePath $info.archive_sha256
 $null=New-Item -ItemType Directory -Path $runtime -Force
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $zip=[IO.Compression.ZipFile]::OpenRead($ArchivePath)
 $runtimeRoot=[IO.Path]::GetFullPath($runtime).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
 $copied=[Collections.Generic.List[object]]::new()
 try{
  foreach($entry in $zip.Entries){
   if($entry.FullName.EndsWith('/')){continue}
   $relative=$entry.FullName.Replace('\','/')
   # Runtime data and per-user configuration never come from the release ZIP.
   if($relative -match '(^|/)(_(SaveData|PipelineCache|Shaders|Buffers|TempData|DownloadData|Textures)|Kyty\.ini|imgui\.ini)(/|$)'){continue}
   $target=[IO.Path]::GetFullPath((Join-Path $runtime $relative))
   if(-not $target.StartsWith($runtimeRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'The release archive contains an invalid path.'}
   $null=New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force
   [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
   $copied.Add([pscustomobject]@{path=$relative;sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()})
  }
 }finally{$zip.Dispose()}
 Assert-AstroHash (Join-Path $runtime 'kyty_emulator.exe') $info.release_executable_sha256
 [pscustomobject]@{kind=$info.runtime_kind;release_url=$info.release_url;archive_sha256=$info.archive_sha256;files=@($copied.ToArray())} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $runtimeInfoPath -Encoding UTF8
}
if($IncludeRumbleFallback){& (Join-Path $PSScriptRoot 'Patch-RumbleFallback.ps1') -InputPath (Join-Path $runtime 'kyty_emulator.exe') | Out-Host}
Write-Output 'Setup complete. The runtime is the pinned upstream PGO release, not a rebuilt executable from this fork.'
Write-Output 'No game was launched. Supply your own game folder to Start-AstroBot.ps1 -GamePath.'
