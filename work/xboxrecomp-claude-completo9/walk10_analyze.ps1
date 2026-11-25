param([string[]]$Tags)
$ErrorActionPreference='Stop'
$ci=[Globalization.CultureInfo]::InvariantCulture
foreach($tag in $Tags) {
 $log="build/claude9/runs/$tag.log"
 $rows=@()
 foreach($line in [IO.File]::ReadLines((Join-Path $pwd $log))) {
  if ($line -match '\[PORTPERF\] pid (\d+) ms ([\d.]+) frames (\d+) .*walk_ms ([\d.]+) present_ms [\d.]+ segs (\d+)') {
   $ms=[double]::Parse($Matches[2],$ci)
   $rows += [pscustomobject]@{pid=[int]$Matches[1]; fps=1000*[double]$Matches[3]/$ms; walk=1000*[double]::Parse($Matches[4],$ci)/$ms; segs=1000*[double]$Matches[5]/$ms}
  }
 }
 # Same policy as psum.py: discard the first 20 PORTPERF windows of the complete log.
 $r=@($rows | Select-Object -Skip 20)
 if (!$r.Count) { continue }
 $sorted=@($r.fps | Sort-Object)
 $w=($r.walk | Measure-Object -Average).Average
 $s=($r.segs | Measure-Object -Average).Average
 $lastPid=$rows[-1].pid
 $game=@($rows | Where-Object pid -eq $lastPid | Select-Object -Skip 20)
 [pscustomobject]@{tag=$tag;n=$r.Count;mean=($r.fps|Measure-Object -Average).Average;min=$sorted[0];p5=$sorted[[math]::Floor($sorted.Count/20)];walk_ms_s=$w;segs_s=$s;us_segment=1000*$w/$s;game_n=$game.Count;game_mean=($game.fps|Measure-Object -Average).Average;game_walk=($game.walk|Measure-Object -Average).Average;game_segs=($game.segs|Measure-Object -Average).Average} | ConvertTo-Json -Compress
}
