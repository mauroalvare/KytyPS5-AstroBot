#Requires -Version 5.1
param(
 [string]$GamePath,
 [ValidateSet('Stable','1440p','90','120','Ultrawide','DeferredGI')][string]$Mode='Stable',
 [ValidateSet('Native','Compatible')][string]$Vibration='Native',
 [string]$SourceExecutable,
 [string]$Display,
 [switch]$Windowed,
 [switch]$Diagnostics,
 [switch]$PrepareOnly
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'AstroBot.Common.ps1')
$info=Get-AstroProvenance $PSScriptRoot
$local=Join-Path $PSScriptRoot '.local'
$configPath=Join-Path $local 'settings.json'
$explicitGamePath=-not [string]::IsNullOrWhiteSpace($GamePath)
if(-not $explicitGamePath -and (Test-Path -LiteralPath $configPath)){$GamePath=(Read-AstroJson $configPath).GamePath}
if([string]::IsNullOrWhiteSpace($GamePath)){throw 'Supply -GamePath with your own game folder containing eboot.bin. A successful launch remembers it locally.'}
$game=[IO.Path]::GetFullPath($GamePath)
Assert-AstroHash (Join-Path $game 'eboot.bin') $info.game_sha256
$selected=Resolve-AstroMode $Mode
$index=Read-AstroJson (Join-Path $PSScriptRoot 'profiles\index.json')
$profileName="PPSA21564_01.007.000_$($selected.Lighting)_$($selected.Resolution)_$($selected.Rate)fps.json"
$entry=@($index.profiles | Where-Object file -EQ $profileName)
if($entry.Count -ne 1){throw 'The requested profile is absent or ambiguous.'}
if($entry[0].vblank -ne $selected.Rate){throw 'The guest patch and virtual vblank frequency disagree.'}
$patch=Join-Path $PSScriptRoot ('profiles\'+$profileName)
Assert-AstroHash $patch $entry[0].sha256
$patchDocument=Read-AstroJson $patch
if($patchDocument.id -ne 'PPSA21564' -or $patchDocument.version -ne '01.007.000' -or $patchDocument.process -ne 'eboot.bin'){throw 'Unexpected game patch target.'}
$expectedMods=@($patchDocument.mods | Where-Object enabled | ForEach-Object name)
Assert-AstroLayer $PSScriptRoot
$runtime=Join-Path $PSScriptRoot 'runtime'
if($SourceExecutable){
 $exe=[IO.Path]::GetFullPath($SourceExecutable)
 if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'The supplied source-build executable does not exist.'}
 $runtimeKind='User-supplied source build; not verified as the pinned upstream PGO release'
}else{
 if($Display){throw '-Display requires a source build with KYTY_DISPLAY support; the pinned PGO release has no monitor selector.'}
 $exeName=if($Vibration -eq 'Compatible'){'kyty_emulator_rumble.exe'}else{'kyty_emulator.exe'}
 $exe=Join-Path $runtime $exeName
 $expectedExeHash=if($Vibration -eq 'Compatible'){$info.rumble_executable_sha256}else{$info.release_executable_sha256}
 Assert-AstroHash $exe $expectedExeHash
 $runtimeKind=if($Vibration -eq 'Compatible'){$info.rumble_kind}else{$info.runtime_kind}
}
$exeHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$preset=Read-AstroJson (Join-Path $PSScriptRoot 'preset.json')
$environment=[ordered]@{}
foreach($property in $preset.PSObject.Properties){
 if($property.Name -notmatch '^(KYTY|TRACY)_[A-Z0-9_]+$' -or $property.Value -isnot [string]){throw 'Unexpected preset entry.'}
 $environment[$property.Name]=$property.Value
}
# This is the conservative native/dynamic baseline. Higher-resolution and FPS
# profiles remain experiments; a requested rate is not a measured FPS claim.
# Preserve the verified preset; memory-protection experiments are not defaults.
$environment['KYTY_HOST_INPUT_MIN_PRESS_MS']='0'
$environment['KYTY_AUDIO_TRACE']=[string][int][bool]$Diagnostics
$environment['KYTY_AUDIO_LEVELS']=[string][int][bool]$Diagnostics
$environment['SDL_JOYSTICK_HIDAPI']='1'
$environment['SDL_JOYSTICK_HIDAPI_PS5']='1'
$environment['SDL_JOYSTICK_ENHANCED_REPORTS']='1'
$environment['SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS']='1'
if($Vibration -eq 'Compatible'){$environment['KYTY_DUALSENSE_HAPTICS']='rumble'}
if($Display){$environment['KYTY_DISPLAY']=$Display}
$environment['VK_LAYER_PATH']=Join-Path $PSScriptRoot 'frame-layer'
$environment['VK_INSTANCE_LAYERS']='VK_LAYER_ASTRO_frame'
$environment['ASTRO_FRAME_ASPECT']=$selected.Aspect
$width=if($Mode -eq 'Ultrawide'){3440}elseif($Windowed){1280}else{2560}
$height=if($Windowed -and $Mode -ne 'Ultrawide'){720}else{1440}
$arguments=@('--game',$game,'--game-patch',$patch,'--screen-width',[string]$width,'--screen-height',[string]$height,
 '--vblank-frequency',[string]$selected.Rate,'--controller-vibration','100','--controller-volume','50','--printf-direction','Silent')
if(-not $Windowed){$arguments+='--fullscreen'}
$quoted=ConvertTo-AstroCommandLine $arguments
$plan=[pscustomobject]@{
 Mode=$Mode;Experimental=$selected.Experimental;Resolution=$selected.Resolution;RequestedRate=$selected.Rate;Vblank=$selected.Rate
 Emulator=$exe;ExecutableSha256=$exeHash;RuntimeKind=$runtimeKind;GamePath=$game;WorkingDirectory=$runtime
 Patch=$patch;ExpectedMods=$expectedMods;Fullscreen=(-not $Windowed);Aspect=$selected.Aspect;Vibration=$Vibration
 Arguments=$arguments;QuotedArguments=$quoted;Environment=$environment;ProcessStarted=$false;RuntimeVerified=$false;PatchVerification='not started'
}
# This branch performs only reads and returns JSON. It creates no directories,
# configuration, live files, environment mutations or emulator processes.
if($PrepareOnly){$plan | ConvertTo-Json -Depth 8;return}
$lock=$null;$savedEnvironment=[ordered]@{};$environmentApplied=$false
try{
 $null=New-Item -ItemType Directory -Path $local -Force
 $null=New-Item -ItemType Directory -Path $runtime -Force
 try{$lock=[IO.File]::Open((Join-Path $local 'launch.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}catch{throw 'Another kit launch is active.'}
 $processNames=@('kyty_emulator*',[IO.Path]::GetFileNameWithoutExtension($exe)) | Select-Object -Unique
 Assert-AstroIdle @($exe,(Join-Path $runtime 'kyty_emulator.exe'),(Join-Path $runtime 'kyty_emulator_rumble.exe')) @(Get-Process -Name $processNames -ErrorAction SilentlyContinue)
 if($explicitGamePath){[pscustomobject]@{GamePath=$game} | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8}
 $runId=(Get-Date).ToString('yyyyMMdd-HHmmss-fff')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)+'-'+$Mode
 $logs=Join-Path $local ('logs\'+$runId)
 $null=New-Item -ItemType Directory -Path $logs -Force
 $stdout=Join-Path $logs 'startup.txt';$stderr=Join-Path $logs 'errors.txt'
 if($Diagnostics){
  $environment['ASTRO_FRAME_INFO']=Join-Path $logs 'presentation.txt'
  $environment['ASTRO_FRAME_LOG']=Join-Path $logs 'frames.csv'
  $environment['ASTRO_FRAME_RENDER_DIAG']='1'
 }
 $clearNames=@(Get-ChildItem Env: | Where-Object Name -Match '^(KYTY|TRACY)_|^ASTRO_FRAME_|^VK_INSTANCE_LAYERS$|^VK_LAYER_PATH$' | ForEach-Object Name)
 foreach($name in @($clearNames+@($environment.Keys) | Select-Object -Unique)){$savedEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
 $environmentApplied=$true
 foreach($name in $clearNames){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
 foreach($name in $environment.Keys){[Environment]::SetEnvironmentVariable($name,[string]$environment[$name],'Process')}
 Write-Host "Astro Bot: $Mode; requested $($selected.Rate) Hz. Performance is not guaranteed."
 $process=Start-Process -FilePath $exe -ArgumentList $quoted -WorkingDirectory $runtime -NoNewWindow -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 $null=$process.Handle
 # Restore the calling shell immediately; the child already inherited its environment.
 foreach($name in $savedEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')}
 $environmentApplied=$false
 $state=[pscustomobject]@{RunId=$runId;ProcessId=$process.Id;Mode=$Mode;RuntimeKind=$runtimeKind;ExecutableSha256=$exeHash
  RequestedRate=$selected.Rate;Vblank=$selected.Rate;Resolution=$selected.Resolution;Vibration=$Vibration
  Started=[DateTimeOffset]::Now.ToString('o');LogDirectory=$logs;ExpectedMods=$expectedMods;AppliedMods=@();PatchVerification='pending';ExitCode=$null}
 $statePath=Join-Path $local 'last-run.json'
 $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $statePath -Encoding UTF8
 $deadline=[DateTime]::UtcNow.AddSeconds(120)
 while(-not $process.WaitForExit(250) -and [DateTime]::UtcNow -lt $deadline){
  $patchState=Get-AstroPatchState $expectedMods @(Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue)
  $state.AppliedMods=$patchState.Applied;$state.PatchVerification=$patchState.Status
  if($patchState.Status -eq 'all expected mods logged'){break}
 }
 $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $statePath -Encoding UTF8
 $process.WaitForExit();$process.Refresh();$gameExit=$process.ExitCode
 $patchState=Get-AstroPatchState $expectedMods @(Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue)
 $state.AppliedMods=$patchState.Applied;$state.PatchVerification=$patchState.Status;$state.ExitCode=$gameExit
 $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $statePath -Encoding UTF8
 if($patchState.Status -ne 'all expected mods logged'){Write-Warning "Some patch mods were not confirmed; inspect $stdout"}
 if($gameExit -ne 0){Get-Content -LiteralPath $stdout -Tail 60;Get-Content -LiteralPath $stderr -Tail 40;throw "Emulator exited with code $gameExit. Logs: $logs"}
}finally{
 if($environmentApplied){foreach($name in $savedEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$savedEnvironment[$name],'Process')}}
 if($null -ne $lock){$lock.Dispose()}
}
