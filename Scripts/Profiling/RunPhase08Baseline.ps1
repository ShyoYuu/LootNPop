# Phase 8 최종 패키지로 Meadow 700마리 자연/합성 추격을 각각 두 번 측정한다.
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskPackage = Join-Path $taskRoot 'Saved/SurfaceNavigationPhase8Package/Windows/LootNPop'
$taskExe = Join-Path $taskPackage 'Binaries/Win64/LootNPop.exe'
$taskOutput = Join-Path $taskRoot 'Saved/Profiling/Phase08'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
foreach ($taskRepeat in 1..2) {
    foreach ($taskChase in 0..1) {
        $taskName = "N700_Chase${taskChase}_Repeat${taskRepeat}"
        $taskCommon = '-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=700 -LNPLoadBaselinePlayers=2 -LNPLoadBaselineQuit'
        if ($taskChase) { $taskCommon += ' -LNPLoadBaselineChase' }
        $taskCvars = '-dpcvars=LNP.SurfaceNav.EnemySupportCache=1,LNP.SurfaceNav.EnemyExactLateralSweep=1,LNP.SurfaceNav.EnemyParallelMovement=1'
        $taskHostLog = Join-Path $taskOutput "${taskName}_Host.log"
        $taskGuestLog = Join-Path $taskOutput "${taskName}_Guest.log"
        $taskHost = Start-Process -FilePath $taskExe -ArgumentList "TestMap03?Listen $taskCommon $taskCvars -abslog=$taskHostLog" -WindowStyle Hidden -PassThru
        Start-Sleep -Seconds 5
        if ($taskHost.HasExited) { throw "$taskName host exited before guest startup" }
        $taskGuest = Start-Process -FilePath $taskExe -ArgumentList "127.0.0.1 $taskCommon $taskCvars -abslog=$taskGuestLog" -WindowStyle Hidden -PassThru
        $taskDeadline = (Get-Date).AddMinutes(8)
        while ((Get-Date) -lt $taskDeadline -and (-not $taskHost.HasExited -or -not $taskGuest.HasExited)) {
            Start-Sleep -Seconds 2
        }
        foreach ($taskProcess in @($taskHost, $taskGuest)) {
            if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id -Force; throw "$taskName timed out" }
            if ($taskProcess.ExitCode -ne 0) { throw "$taskName process exited $($taskProcess.ExitCode)" }
        }
        $taskCapture = "NavRequests_N700_Chase${taskChase}_Seed1"
        foreach ($taskSuffix in @('', '_Timings')) {
            Copy-Item -LiteralPath (Join-Path $taskPackage "Saved/Logs/${taskCapture}${taskSuffix}.csv") -Destination (Join-Path $taskOutput "${taskName}${taskSuffix}.csv")
        }
        foreach ($taskLog in @($taskHostLog, $taskGuestLog)) {
            $taskText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskLog
            if ($taskText -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|-> FAIL|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]' -or
                ([regex]::Matches($taskText, '\[(ProbeFaceIndex|ProbeSourceKeys|ProbeSurfaceData)\].*-> PASS')).Count -ne 3) {
                throw "$taskName functional checks failed: $taskLog"
            }
        }
        [pscustomobject]@{ Run=$taskName; HostExit=$taskHost.ExitCode; GuestExit=$taskGuest.ExitCode } |
            ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "${taskName}_Exit.json")
        Select-String -LiteralPath $taskHostLog -Pattern '\[LoadBaseline\] (FrameMs|NavGate|UnknownHits)' | ForEach-Object { $_.Line }
    }
}
