# Phase 6 부하 기준 매트릭스 실행기(`.context/SurfaceSupportNavigation/history/Phase06_Log.md`).
# 패키지 Development 리슨 호스트와 게스트를 둘 다 `-nullrhi -corelimit=4`로 시나리오마다 순차 실행한다.
# 사용법: .\RunLoadBaselineMatrix.ps1                       # 모든 시나리오
#         .\RunLoadBaselineMatrix.ps1 N700_exact N700_cache # 선택한 시나리오만
# `Saved/SurfaceNavigationPhase3Package`의 BuildCookRun 결과가 필요하다. 로그는 `Saved/Profiling/Phase06`에 남긴다.

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $root "Saved\SurfaceNavigationPhase3Package\Windows\LootNPop.exe"

$runs = @(
    # Phase 3b/3c의 병렬 exact 경계와 같은 수에서 cache-first 절감량을 직접 비교한다.
    @{ Name = "N500_exact";  N = 500;  Cache = 0 },
    @{ Name = "N500_cache";  N = 500;  Cache = 1 },
    @{ Name = "N700_exact";  N = 700;  Cache = 0 },
    @{ Name = "N700_cache";  N = 700;  Cache = 1 },
    @{ Name = "N750_exact";  N = 750;  Cache = 0 },
    # cache-first의 새 한계치를 찾기 위한 상향 탐색점.
    @{ Name = "N800_cache";  N = 800;  Cache = 1 },
    @{ Name = "N850_cache";  N = 850;  Cache = 1 },
    @{ Name = "N1000_cache"; N = 1000; Cache = 1 },
    @{ Name = "N1500_cache"; N = 1500; Cache = 1 },
    @{ Name = "N2000_cache"; N = 2000; Cache = 1 }
)
# `$args`는 Where-Object script block 안에서 가려지므로 먼저 별도 변수에 보관한다.
$only = $args
if ($only.Count -gt 0) { $runs = $runs | Where-Object { $only -contains $_.Name } }

foreach ($r in $runs) {
    $logDir = Join-Path $root "Saved\Profiling\Phase06"
    New-Item -ItemType Directory -Force $logDir | Out-Null
    $common = "-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=$($r.N) -LNPLoadBaselinePlayers=2 -LNPLoadBaselineQuit"
    $cvars = "-dpcvars=LNP.SurfaceNav.EnemySupportCache=$($r.Cache),LNP.SurfaceNav.EnemyExactLateralSweep=1,LNP.SurfaceNav.EnemyParallelMovement=1"
    $hostLog = Join-Path $logDir "$($r.Name)_Host.log"
    $guestLog = Join-Path $logDir "$($r.Name)_Guest.log"
    $serverProcess = Start-Process -FilePath $exe -ArgumentList "TestMap03?Listen $common $cvars -abslog=$hostLog" -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 5
    $clientProcess = Start-Process -FilePath $exe -ArgumentList "127.0.0.1 $common -abslog=$guestLog" -WindowStyle Hidden -PassThru
    $deadline = (Get-Date).AddMinutes(8)
    while ((Get-Date) -lt $deadline -and (-not $serverProcess.HasExited -or -not $clientProcess.HasExited)) { Start-Sleep -Seconds 5 }
    foreach ($process in @($serverProcess, $clientProcess)) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; Write-Output "$($r.Name): killed $($process.Id) after timeout" }
    }
    Get-Process -Name LootNPop -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Output "$($r.Name): done"
}
Write-Output "MATRIX COMPLETE"
