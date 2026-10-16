#Requires -Version 5.1
param([string]$ArchivePath,[string]$LayerInstallDirectory)
# CPU/file-only regression suite. Never starts emulator, UI, GPU, audio or pads.
$ErrorActionPreference='Stop'
Import-Module Microsoft.PowerShell.Utility
$kit=Split-Path $PSScriptRoot -Parent
. (Join-Path $kit 'AstroBot.Common.ps1')
$script:checks=0
function Check {param([bool]$OK,[string]$Name) if(-not $OK){throw "FAIL: $Name"};$script:checks++}
function Throws {param([scriptblock]$Action,[string]$Pattern,[string]$Name) $errorText=$null;try{& $Action | Out-Null}catch{$errorText=[string]$_};Check ($null -ne $errorText -and $errorText -match $Pattern) $Name}
$hashInputs=@((Join-Path $kit 'preset.json'),(Join-Path $kit 'provenance.json'))+@(Get-ChildItem -LiteralPath (Join-Path $kit 'profiles') -Filter '*.json' | ForEach-Object FullName)
$allLf=$true
foreach($file in $hashInputs){if([IO.File]::ReadAllBytes($file) -contains 13){$allLf=$false}}
Check $allLf 'Checksum-sensitive JSON stays LF across Windows Git checkouts'
$case=Join-Path $PSScriptRoot ('generated\'+[guid]::NewGuid().ToString('N'))
$source=Join-Path $case 'repo\tools\astro-bot'
$null=New-Item -ItemType Directory -Path $source -Force
$names=@('AstroBot.Common.ps1','Setup-AstroBot.ps1','Start-AstroBot.ps1','Package-AstroBot.ps1','Patch-RumbleFallback.ps1','Play-AstroBot.cmd','usage.md','provenance.json','preset.json')
foreach($name in $names){Copy-Item -LiteralPath (Join-Path $kit $name) -Destination (Join-Path $source $name)}
Copy-Item -LiteralPath (Join-Path $kit 'profiles') -Destination (Join-Path $source 'profiles') -Recurse
Copy-Item -LiteralPath (Join-Path $kit '..\..\LICENSE') -Destination (Join-Path $case 'repo\LICENSE')
$null=New-Item -ItemType Directory -Path (Join-Path $source '.local') -Force
$null=New-Item -ItemType Directory -Path (Join-Path $source 'runtime\_SaveData') -Force
'not public' | Set-Content -LiteralPath (Join-Path $source '.local\private.txt')
'not public' | Set-Content -LiteralPath (Join-Path $source 'runtime\_SaveData\private.txt')
$layer=Join-Path $case 'fixture-layer'
$null=New-Item -ItemType Directory -Path $layer
'CPU fixture, not an executable library' | Set-Content -LiteralPath (Join-Path $layer 'astro_frame.dll') -Encoding ASCII
'{"layer":{"name":"VK_LAYER_ASTRO_frame","library_path":".\\astro_frame.dll"}}' | Set-Content -LiteralPath (Join-Path $layer 'VK_LAYER_ASTRO_frame.json') -Encoding ASCII
$overlay=Join-Path $case 'overlay'
$zip=Join-Path $case 'overlay.zip'
& (Join-Path $source 'Package-AstroBot.ps1') -OutputDirectory $overlay -LayerInstallDirectory $layer -ArchivePath $zip -SourceCommit ('1'*40) | Out-Null
Check ((Test-Path -LiteralPath $zip) -and -not(Test-Path -LiteralPath (Join-Path $overlay 'runtime')) -and -not(Test-Path -LiteralPath (Join-Path $overlay '.local'))) 'Whitelist packaging excludes runtime, saves and local configuration'
Check ((Read-AstroJson (Join-Path $overlay 'overlay-info.json')).kind -like 'Overlay only*') 'Overlay metadata describes provenance accurately'
Check ((Read-AstroJson (Join-Path $overlay 'overlay-info.json')).source_commit -eq ('1'*40)) 'Overlay records the explicitly supplied exact fork source revision'
Throws {& (Join-Path $source 'Package-AstroBot.ps1') -OutputDirectory (Join-Path $case 'invalid-commit') -LayerInstallDirectory $layer -SourceCommit 'invalid'} 'SourceCommit' 'Malformed source revision is rejected before packaging'
$game=Join-Path $case 'fixture-game'
$null=New-Item -ItemType Directory -Path $game
'CPU fixture, not game data' | Set-Content -LiteralPath (Join-Path $game 'eboot.bin') -Encoding ASCII
$null=New-Item -ItemType Directory -Path (Join-Path $overlay 'runtime')
$null=New-Item -ItemType Directory -Path (Join-Path $overlay '.local')
'CPU fixture, not an emulator' | Set-Content -LiteralPath (Join-Path $overlay 'runtime\kyty_emulator.exe') -Encoding ASCII
'CPU fixture, not an emulator' | Set-Content -LiteralPath (Join-Path $overlay 'runtime\kyty_emulator_rumble.exe') -Encoding ASCII
[pscustomobject]@{GamePath=$game} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $overlay '.local\settings.json') -Encoding UTF8
# Fixture provenance uses hashes of synthetic text files, not a bypass in the
# production launcher. The original public provenance and profiles stay intact.
$fixtureInfo=Read-AstroJson (Join-Path $overlay 'provenance.json')
$fixtureInfo.game_sha256=(Get-FileHash -LiteralPath (Join-Path $game 'eboot.bin') -Algorithm SHA256).Hash
$fixtureInfo.release_executable_sha256=(Get-FileHash -LiteralPath (Join-Path $overlay 'runtime\kyty_emulator.exe') -Algorithm SHA256).Hash
$fixtureInfo.rumble_executable_sha256=(Get-FileHash -LiteralPath (Join-Path $overlay 'runtime\kyty_emulator_rumble.exe') -Algorithm SHA256).Hash
$fixtureInfo | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $overlay 'provenance.json') -Encoding UTF8
function global:Get-Process {throw 'Tests forbid runtime process discovery.'}
function global:Start-Process {throw 'Tests forbid process creation.'}
function Snapshot {
 param([string]$Root)
 @(Get-ChildItem -LiteralPath $Root -File -Recurse -Force | Sort-Object FullName | ForEach-Object { [pscustomobject]@{Name=$_.FullName;Hash=(Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash;Time=$_.LastWriteTimeUtc.Ticks} }) | ConvertTo-Json -Compress
}
$before=Snapshot $overlay
$environmentBefore=Get-ChildItem Env: | Sort-Object Name | Select-Object Name,Value | ConvertTo-Json -Compress
foreach($mode in @('Stable','1440p','90','120','Ultrawide','DeferredGI')){
 $text=& (Join-Path $overlay 'Start-AstroBot.ps1') -Mode $mode -PrepareOnly
 $plan=$text | ConvertFrom-Json
 Check (-not $plan.ProcessStarted -and $plan.PatchVerification -eq 'not started' -and $plan.RequestedRate -eq $plan.Vblank) "PrepareOnly $mode produces a consistent no-launch plan"
 if($mode -eq 'Stable'){Check ($plan.Environment.KYTY_DEFER_PROTECTS -eq (Read-AstroJson (Join-Path $overlay 'preset.json')).KYTY_DEFER_PROTECTS) 'Default launch honors the verified memory-protection preset'}
 $selected=Resolve-AstroMode $mode
 Check ($plan.Resolution -eq $selected.Resolution -and $plan.Aspect -eq $selected.Aspect) "$mode uses the expected resolution/aspect"
}
$rumble=& (Join-Path $overlay 'Start-AstroBot.ps1') -Mode Stable -Vibration Compatible -PrepareOnly | ConvertFrom-Json
Check ($rumble.Emulator.EndsWith('kyty_emulator_rumble.exe') -and $rumble.Environment.KYTY_DUALSENSE_HAPTICS -eq 'rumble') 'Compatible mode selects only the separate guarded runtime copy'
Throws {& (Join-Path $overlay 'Start-AstroBot.ps1') -Display 'Second monitor' -PrepareOnly} 'source build' 'Pinned PGO does not pretend to support monitor selection'
$environmentAfter=Get-ChildItem Env: | Sort-Object Name | Select-Object Name,Value | ConvertTo-Json -Compress
Check ($environmentBefore -ceq $environmentAfter -and $before -ceq (Snapshot $overlay)) 'Every preparation preserves the complete environment and all existing files'
$command=ConvertTo-AstroCommandLine @('plain','with space','D:\Tail\','say "hello"')
Check ($command -ceq '"plain" "with space" "D:\Tail\\" "say \"hello\""') 'Windows argument quoting preserves spaces, trailing slash and quotes'
$state=Get-AstroPatchState @('one','two') @('Successfully applied cheat: one')
Check ($state.Status -ne 'all expected mods logged') 'Partial patch success is never reported as complete'
$state=Get-AstroPatchState @('one','two') @('Successfully applied cheat: one','Successfully applied cheat: two')
Check ($state.Status -eq 'all expected mods logged') 'Exact runtime success messages confirm all requested mods'
Throws {Assert-AstroIdle @('D:\kit\runtime\kyty_emulator.exe') @([pscustomobject]@{Path='D:\kit\runtime\kyty_emulator.exe';Id=7})} 'already open' 'Duplicate runtime identity blocks launch with synthetic process data'
foreach($file in @(Get-ChildItem -LiteralPath $kit -Filter '*.ps1')){
 $tokens=$null;$errors=$null;$null=[Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
 Check ($errors.Count -eq 0) "$($file.Name) parses in Windows PowerShell 5.1"
}
# Optional offline integration uses a verified original archive and a built
# frame layer. It extracts files and computes the six-byte patch; never runs them.
if($ArchivePath){
 if(-not $LayerInstallDirectory){throw '-LayerInstallDirectory is required for offline setup integration.'}
 Import-Module Microsoft.PowerShell.Utility -Force
 $integration=Join-Path $case 'offline-overlay'
 & (Join-Path $source 'Package-AstroBot.ps1') -OutputDirectory $integration -LayerInstallDirectory $LayerInstallDirectory | Out-Null
 & (Join-Path $integration 'Setup-AstroBot.ps1') -ArchivePath $ArchivePath -IncludeRumbleFallback | Out-Null
 $info=Read-AstroJson (Join-Path $integration 'provenance.json')
 Assert-AstroHash (Join-Path $integration 'runtime\kyty_emulator.exe') $info.release_executable_sha256
 Assert-AstroHash (Join-Path $integration 'runtime\kyty_emulator_rumble.exe') $info.rumble_executable_sha256
 Check $true 'Offline setup preserves the original and produces the exact guarded six-byte rumble copy'
 Check (-not(Test-Path -LiteralPath (Join-Path $integration 'runtime\_SaveData')) -and -not(Test-Path -LiteralPath (Join-Path $integration 'runtime\_PipelineCache'))) 'Offline setup imports no saves or caches'
 & (Join-Path $integration 'Setup-AstroBot.ps1') -ArchivePath $ArchivePath -IncludeRumbleFallback | Out-Null
 Check $true 'Repeated verified offline setup is idempotent'
}
Write-Output "$script:checks CPU/file-only checks passed. No emulator/UI/GPU/audio/controller was started."
