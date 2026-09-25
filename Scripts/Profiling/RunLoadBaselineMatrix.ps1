# Phase 3b load baseline matrix runner (see .context/SurfaceSupportNavigation/history/Phase03b_Log.md).
# Runs a packaged Development listen host + guest (both -nullrhi -corelimit=4) per scenario, sequentially.
# Usage: .\RunLoadBaselineMatrix.ps1                 # all scenarios
#        .\RunLoadBaselineMatrix.ps1 N500_exact ...  # selected scenarios only
# Requires the package at Saved/SurfaceNavigationPhase3Package (RunUAT BuildCookRun). Logs go to Saved/Profiling/Phase03b.

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $root "Saved\SurfaceNavigationPhase3Package\Windows\LootNPop.exe"
$logDir = Join-Path $root "Saved\Profiling\Phase03b"
New-Item -ItemType Directory -Force $logDir | Out-Null

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
    @{ Name = "N2000_exact";      N = 2000; Exact = 1; Lateral = 1 }
)
# $args is shadowed inside Where-Object script blocks; capture it first.
$only = $args
if ($only.Count -gt 0) { $runs = $runs | Where-Object { $only -contains $_.Name } }

foreach ($r in $runs) {
    $common = "-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=$($r.N) -LNPLoadBaselineQuit"
    $cvars = "-dpcvars=LNP.SurfaceNav.EnemyExactGround=$($r.Exact),LNP.SurfaceNav.EnemyExactLateralSweep=$($r.Lateral)"
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
