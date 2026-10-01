# Phase 7b 단위 5: Phase 6과 같은 패키지·seed·투사체·CPU 조건에서 추격 경로 비용을 측정한다.
# 기본: N700_natural, N700_chase, N800_chase. 인자로 시나리오 이름을 주면 해당 실행만 선택한다.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $root 'Saved\SurfaceNavigationPhase3Package\Windows\LootNPop.exe'
$logDir = Join-Path $root 'Saved\Profiling\Phase07b'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$runs = @(
    @{ Name = 'N700_natural'; N = 700; Chase = $false },
    @{ Name = 'N700_chase'; N = 700; Chase = $true },
    @{ Name = 'N800_chase'; N = 800; Chase = $true },
    # Gate 미통과 시 한계치를 찾는 하향 탐색점. 기본 실행에는 포함하지 않는다.
    @{ Name = 'N500_chase'; N = 500; Chase = $true },
    @{ Name = 'N600_chase'; N = 600; Chase = $true }
)
$selected = $args
if ($selected.Count -eq 0) { $selected = @('N700_natural', 'N700_chase', 'N800_chase') }
$runs = $runs | Where-Object { $selected -contains $_.Name }
foreach ($run in $runs) {
    $common = "-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=$($run.N) -LNPLoadBaselinePlayers=2 -LNPLoadBaselineQuit"
    $chase = if ($run.Chase) { '-LNPLoadBaselineChase' } else { '' }
    $cvars = '-dpcvars=LNP.SurfaceNav.EnemySupportCache=1,LNP.SurfaceNav.EnemyExactLateralSweep=1,LNP.SurfaceNav.EnemyParallelMovement=1'
    $hostLog = Join-Path $logDir "$($run.Name)_Host.log"
    $guestLog = Join-Path $logDir "$($run.Name)_Guest.log"
    $hostProcess = Start-Process -FilePath $exe -ArgumentList "TestMap03?Listen $common $chase $cvars -abslog=$hostLog" -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 5
    $guestProcess = Start-Process -FilePath $exe -ArgumentList "127.0.0.1 $common -abslog=$guestLog" -WindowStyle Hidden -PassThru
    $deadline = (Get-Date).AddMinutes(8)
    while ((Get-Date) -lt $deadline -and (-not $hostProcess.HasExited -or -not $guestProcess.HasExited)) { Start-Sleep -Seconds 5 }
    foreach ($taskProcess in @($hostProcess, $guestProcess)) {
        if (-not $taskProcess.HasExited) {
            Stop-Process -Id $taskProcess.Id -Force
            throw "$($run.Name): process $($taskProcess.Id) timed out"
        }
        if ($taskProcess.ExitCode -ne 0) { throw "$($run.Name): process exited $($taskProcess.ExitCode)" }
    }
    $capture = Join-Path $root "Saved\SurfaceNavigationPhase3Package\Windows\LootNPop\Saved\Logs\NavRequests_N$($run.N)_Chase$([int]$run.Chase)_Seed1.csv"
    Copy-Item -LiteralPath $capture -Destination (Join-Path $logDir "$($run.Name)_Requests.csv")
    Select-String -LiteralPath $hostLog -Pattern '\[LoadBaseline\] (Nav |NavGate|FrameMs|Nav finished)' | ForEach-Object { $_.Line }
}
