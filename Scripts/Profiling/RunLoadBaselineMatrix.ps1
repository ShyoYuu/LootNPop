# Phase 3b/3c load baseline matrix runner (see .context/SurfaceSupportNavigation/history/Phase03b_Log.md, Phase03c_Log.md).
# Runs a packaged Development listen host + guest (both -nullrhi -corelimit=4) per scenario, sequentially.
# Usage: .\RunLoadBaselineMatrix.ps1                 # all scenarios
#        .\RunLoadBaselineMatrix.ps1 N500_exact ...  # selected scenarios only
# Requires the package at Saved/SurfaceNavigationPhase3Package (RunUAT BuildCookRun). Logs go to Saved/Profiling/<Phase> (default Phase03b).

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $root "Saved\SurfaceNavigationPhase3Package\Windows\LootNPop.exe"

$runs = @(
    @{ Name = "N300_legacy";      N = 300;  Exact = 0; Lateral = 1 },
    @{ Name = "N300_exact";       N = 300;  Exact = 1; Lateral = 1 },
    @{ Name = "N450_exact";       N = 450;  Exact = 1; Lateral = 1 },
    @{ Name = "N500_exact";       N = 500;  Exact = 1; Lateral = 1 },
    @{ Name = "N550_exact";       N = 550;  Exact = 1; Lateral = 1 },
    @{ Name = "N1000_legacy";     N = 1000; Exact = 0; Lateral = 1 },
    @{ Name = "N1000_exact";      N = 1000; Exact = 1; Lateral = 1 },
    @{ Name = "N1000_exact_lat0"; N = 1000; Exact = 1; Lateral = 0 },
    @{ Name = "N2000_legacy";     N = 2000; Exact = 0; Lateral = 1 },
    @{ Name = "N2000_exact";      N = 2000; Exact = 1; Lateral = 1 },
    # Phase03b 3.6.1: movement processor ParallelForEachEntityChunk.
    @{ Name = "N500_exact_par";   N = 500;  Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N650_exact_par";   N = 650;  Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N700_exact_par";   N = 700;  Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N750_exact_par";   N = 750;  Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N800_exact_par";   N = 800;  Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N850_exact_par";   N = 850;  Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N1000_exact_par";  N = 1000; Exact = 1; Lateral = 1; Parallel = 1 },
    @{ Name = "N2000_exact_par";  N = 2000; Exact = 1; Lateral = 1; Parallel = 1 },
    # Phase03c 3.8: flying enemies. Flyer-only runs put players on the big island top; mixed runs keep the ring center.
    @{ Name = "F100_par";         N = 0;    Flyers = 100; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    @{ Name = "F300_par";         N = 0;    Flyers = 300; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    @{ Name = "F500_par";         N = 0;    Flyers = 500; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    @{ Name = "N500_F100_par";    N = 500;  Flyers = 100; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    # Re-check of the 3b parallel limit after the spawn burial fix.
    @{ Name = "N500_exact_par_3c"; N = 500; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    @{ Name = "N700_exact_par_3c"; N = 700; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    @{ Name = "N750_exact_par_3c"; N = 750; Exact = 1; Lateral = 1; Parallel = 1; Phase = "Phase03c" },
    # Penetration check adds one query per flyer per frame: use for the penetration count only, not for timing.
    @{ Name = "F500_par_pen";     N = 0;    Flyers = 500; Exact = 1; Lateral = 1; Parallel = 1; Pen = 1; Phase = "Phase03c" }
)
# $args is shadowed inside Where-Object script blocks; capture it first.
$only = $args
if ($only.Count -gt 0) { $runs = $runs | Where-Object { $only -contains $_.Name } }

foreach ($r in $runs) {
    $logDir = Join-Path $root "Saved\Profiling\$(if ($r.Phase) { $r.Phase } else { 'Phase03b' })"
    New-Item -ItemType Directory -Force $logDir | Out-Null
    $flyers = if ($r.Flyers) { $r.Flyers } else { 0 }
    $common = "-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=$($r.N) -LNPLoadBaselineFlyers=$flyers -LNPLoadBaselineQuit"
    $parallel = if ($r.Parallel) { $r.Parallel } else { 0 }
    $pen = if ($r.Pen) { $r.Pen } else { 0 }
    $cvars = "-dpcvars=LNP.SurfaceNav.EnemyExactGround=$($r.Exact),LNP.SurfaceNav.EnemyExactLateralSweep=$($r.Lateral),LNP.SurfaceNav.EnemyParallelMovement=$parallel,LNP.SurfaceNav.LoadBaseline.FlightPenetrationCheck=$pen"
    $hostLog = Join-Path $logDir "$($r.Name)_Host.log"
    $guestLog = Join-Path $logDir "$($r.Name)_Guest.log"
    $h = Start-Process -FilePath $exe -ArgumentList "TestMap03?Listen $common $cvars -abslog=$hostLog" -PassThru
    Start-Sleep -Seconds 5
    $g = Start-Process -FilePath $exe -ArgumentList "127.0.0.1 $common -abslog=$guestLog" -PassThru
    $deadline = (Get-Date).AddMinutes(8)
    while ((Get-Date) -lt $deadline -and (-not $h.HasExited -or -not $g.HasExited)) { Start-Sleep -Seconds 5 }
    foreach ($p in @($h, $g)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; Write-Output "$($r.Name): killed $($p.Id) after timeout" } }
    Get-Process -Name LootNPop -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Output "$($r.Name): done"
}
Write-Output "MATRIX COMPLETE"
