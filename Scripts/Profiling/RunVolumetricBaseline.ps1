# Phase 7c 공동·복층·두 긴 경사로 부하. 같은 seed·적 수로 exact-only/cache-first를 비교한다.
# 패키지는 BuildCookRun에 -map=/Game/Maps/TestMap03+/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression와
# -CookDir=<프로젝트>/Content/Maps/SurfaceNavigation/Fixtures를 함께 지정해 만든다.
param(
    [int]$Enemies = 100,
    [int]$Cache = 1,
    [string]$RunName = 'Volumetric_N100_cache1',
    [switch]$EditorGame
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskDir = Join-Path $taskRoot 'Saved/Profiling/Phase07c'
New-Item -ItemType Directory -Force -Path $taskDir | Out-Null
$taskExe = Join-Path $taskRoot 'Saved/SurfaceNavigationPhase3Package/Windows/LootNPop/Binaries/Win64/LootNPop.exe'
$taskPrefix = ''
$taskCaptureDir = Join-Path $taskRoot 'Saved/SurfaceNavigationPhase3Package/Windows/LootNPop/Saved/Logs'
if ($EditorGame) {
    $taskExe = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
    $taskPrefix = "`"$taskRoot/LootNPop.uproject`" -game "
    $taskCaptureDir = Join-Path $taskRoot 'Saved/Logs'
}
$taskCommon = "-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=$Enemies -LNPLoadBaselinePlayers=2 -LNPLoadBaselineQuit -LNPLoadBaselineChase -LNPLoadBaselineVolumetric"
$taskCvars = "-dpcvars=LNP.SurfaceNav.EnemySupportCache=$Cache,LNP.SurfaceNav.EnemyExactLateralSweep=1,LNP.SurfaceNav.EnemyParallelMovement=1"
$taskHostLog = Join-Path $taskDir "${RunName}_Host.log"
$taskGuestLog = Join-Path $taskDir "${RunName}_Guest.log"
$taskHost = Start-Process -FilePath $taskExe -ArgumentList "$taskPrefix TestMap03?Listen $taskCommon $taskCvars -abslog=$taskHostLog" -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 5
if ($taskHost.HasExited) { throw "$RunName host exited before guest startup: $($taskHost.ExitCode)" }
$taskGuest = Start-Process -FilePath $taskExe -ArgumentList "$taskPrefix 127.0.0.1 $taskCommon $taskCvars -abslog=$taskGuestLog" -WindowStyle Hidden -PassThru
$taskDeadline = (Get-Date).AddMinutes(8)
while ((Get-Date) -lt $taskDeadline -and (-not $taskHost.HasExited -or -not $taskGuest.HasExited)) {
    if ($taskHost.HasExited -and $taskHost.ExitCode -ne 0) {
        if (-not $taskGuest.HasExited) { Stop-Process -Id $taskGuest.Id -Force }
        throw "$RunName host exited $($taskHost.ExitCode)"
    }
    Start-Sleep -Seconds 2
}
foreach ($taskProcess in @($taskHost, $taskGuest)) {
    if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id -Force; throw "$RunName timed out" }
    if ($taskProcess.ExitCode -ne 0) { throw "$RunName process exited $($taskProcess.ExitCode)" }
}
$taskCapture = "NavRequests_N${Enemies}_Chase1_Seed1"
Copy-Item -LiteralPath (Join-Path $taskCaptureDir "$taskCapture.csv") -Destination (Join-Path $taskDir "${RunName}_Requests.csv")
Copy-Item -LiteralPath (Join-Path $taskCaptureDir "${taskCapture}_Timings.csv") -Destination (Join-Path $taskDir "${RunName}_RequestTimings.csv")
[pscustomobject]@{ Run=$RunName; Enemies=$Enemies; Cache=$Cache; EditorGame=[bool]$EditorGame; HostExit=$taskHost.ExitCode; GuestExit=$taskGuest.ExitCode } |
    ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskDir "${RunName}_Exit.json")
foreach ($taskLog in @($taskHostLog, $taskGuestLog)) {
    $taskText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskLog
    if ($taskText -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|-> FAIL|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]' -or
        ([regex]::Matches($taskText, '\[(ProbeFaceIndex|ProbeSourceKeys|ProbeSurfaceData)\].*-> PASS')).Count -ne 3 -or
        $taskText -notmatch '\[ProbePanels\].*-> SKIP') { throw "$RunName functional checks failed: $taskLog" }
}
Select-String -LiteralPath $taskHostLog -Pattern '\[LoadBaseline\] (Volumetric placement|Nav |NavGate|FrameMs|UnknownHits)|\[NavTiming\]|\[EnemyGround\]' | ForEach-Object { $_.Line }
