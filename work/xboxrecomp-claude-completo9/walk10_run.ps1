param([string]$Tag,[int]$Seconds=330,[int]$Walk=0,[int]$Profile=1)
$ErrorActionPreference='Stop'
$root=(Get-Location).Path
$exe=(Resolve-Path 'build/claude9/exe9_hot/obiwan_recomp.exe').Path
if (@(Get-Process obiwan_recomp -ErrorAction SilentlyContinue | Where-Object Path -eq $exe).Count) { throw 'exe9_hot is busy' }
$out=Join-Path $root "build/claude9/runs/$Tag"
New-Item -ItemType Directory -Force "$out/saves" | Out-Null
& compact.exe /C /I /Q $out | Out-Null
foreach($v in @('APU_FE_NOTIFY','FB_WINDOW','TITLE_KEVENTS','PB_EXEC','USB','AC97_READY','DISPATCH_GATE','GPU_NATIVE_BC','FIX_FF_GPU','MEDIR')) {
 [Environment]::SetEnvironmentVariable('RECOMP_'+$v,'1','Process')
}
foreach($v in @('CLASSIFY','FF_DIRTY','INDEX')) {
 [Environment]::SetEnvironmentVariable('RECOMP_FIX_WALK_'+$v,[string]$Walk,'Process')
}
$env:RECOMP_VBLANK='paced'; $env:RECOMP_MKB='0'; $env:RECOMP_SDL_PADS='0'; $env:RECOMP_AUDIO_BUF_COUNT='6'
$env:RECOMP_WATCHDOG_SECS=[string]($Seconds+5); $env:RECOMP_PROF=[string]$Profile
$env:RECOMP_PROF_SECS='200'; $env:RECOMP_PERFRT=[string]$Profile; $env:RECOMP_FPSCLAMP='1000'
$env:RECOMP_PAD_SCRIPT='@'+(Resolve-Path 'build/claude9/runs/nivel1_menu.pad').Path
$env:RECOMP_PAD_SCRIPT_RELAUNCH='@'+(Resolve-Path 'build/claude9/runs/nivel1_juego.pad').Path
# pk starts at zero at FROM: EVERY=3000 selects frame FROM+1500, then +3000.
$env:RECOMP_PRESENT_DUMP="$out/f"; $env:RECOMP_PRESENT_DUMP_FROM='1500'
$env:RECOMP_PRESENT_DUMP_N='4501'; $env:RECOMP_PRESENT_DUMP_EVERY='3000'
$data=Join-Path $root 'private/game/usa'
$args='/d /s /c ""{0}" "{1}" "{2}" > "{3}" 2>&1"' -f $exe,$data,"$out/saves","$out.log"
$proc=Start-Process -FilePath $env:ComSpec -ArgumentList $args -WindowStyle Hidden -PassThru
$ownIds=@()
$absent=0
$end=(Get-Date).AddSeconds($Seconds)
try {
 while ((Get-Date) -lt $end) {
  $live=@(Get-Process obiwan_recomp -ErrorAction SilentlyContinue | Where-Object Path -eq $exe)
  foreach($p in $live) {
   if ($ownIds -notcontains $p.Id) { $ownIds += $p.Id; Add-Content "$out/own_pids.txt" $p.Id }
  }
  if ($live.Count) { $absent=0 } else { $absent++ }
  if ($absent -ge 5) { throw 'Game exited before the measurement finished' }
  Start-Sleep -Seconds 2
 }
} finally {
 foreach($ownId in $ownIds) { Stop-Process -Id $ownId -ErrorAction SilentlyContinue }
 Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue
}
Write-Output "DONE $Tag"
