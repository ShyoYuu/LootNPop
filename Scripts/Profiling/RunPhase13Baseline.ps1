# Phase 13 변경 전 패키지의 지상·비행 공통 비용을 Insights로 분해한다.
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskPackage = Join-Path $taskRoot 'Saved/SurfaceNavigationPhase8Package/Windows/LootNPop'
$taskExe = Join-Path $taskPackage 'Binaries/Win64/LootNPop.exe'
$taskInsights = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe'
$taskOutput = Join-Path $taskRoot 'Saved/Profiling/Phase13/Before'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
if (Get-Process -Name UnrealEditor,LootNPop,UnrealInsights -ErrorAction SilentlyContinue) {
    throw 'Close the editor, games and Insights before measurement.'
}
Get-FileHash -Algorithm SHA256 -LiteralPath $taskExe |
    Select-Object Path,Hash | ConvertTo-Json |
    Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Package.json')
foreach ($taskCase in @(@{Name='N700'; Ground=700; Flyers=0}, @{Name='F100'; Ground=0; Flyers=100}, @{Name='F300'; Ground=0; Flyers=300})) {
    $taskName = $taskCase.Name
    $taskHostLog = Join-Path $taskOutput "${taskName}_Host.log"
    $taskGuestLog = Join-Path $taskOutput "${taskName}_Guest.log"
    $taskTrace = Join-Path $taskOutput "${taskName}_Host.utrace"
    if (Test-Path -LiteralPath $taskTrace) { throw "Preserve the previous capture before rerunning $taskName." }
    $taskCommon = "-nullrhi -nosound -corelimit=4 -unattended -LNPLoadBaseline=$($taskCase.Ground) -LNPLoadBaselineFlyers=$($taskCase.Flyers) -LNPLoadBaselinePlayers=2 -LNPLoadBaselineQuit"
    $taskCommon += ' -dpcvars=LNP.SurfaceNav.EnemySupportCache=1,LNP.SurfaceNav.EnemyExactLateralSweep=1,LNP.SurfaceNav.EnemyParallelMovement=1'
    Write-Output "$taskName started"
    $taskHost = Start-Process -FilePath $taskExe -ArgumentList "TestMap03?Listen $taskCommon -trace=cpu,frame,bookmark,region,task -tracefile=$taskTrace -abslog=$taskHostLog" -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 5
    $taskGuest = Start-Process -FilePath $taskExe -ArgumentList "127.0.0.1 $taskCommon -abslog=$taskGuestLog" -WindowStyle Hidden -PassThru
    $taskDeadline = (Get-Date).AddMinutes(8)
    while ((Get-Date) -lt $taskDeadline -and (-not $taskHost.HasExited -or -not $taskGuest.HasExited)) { Start-Sleep -Seconds 2 }
    foreach ($taskProcess in @($taskHost, $taskGuest)) {
        if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id -Force; throw "$taskName timed out" }
        if ($taskProcess.ExitCode -ne 0) { throw "$taskName exited $($taskProcess.ExitCode)" }
    }
    foreach ($taskLog in @($taskHostLog, $taskGuestLog)) {
        $taskText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskLog
        if ($taskText -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|-> FAIL|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]' -or
            ([regex]::Matches($taskText, '\[(ProbeFaceIndex|ProbeSourceKeys|ProbeSurfaceData)\].*-> PASS')).Count -ne 3 -or
            $taskText -notmatch '\[LoadBaseline\] FrameMs') { throw "$taskName functional checks failed: $taskLog" }
    }
    [pscustomobject]@{Run=$taskName; Ground=$taskCase.Ground; Flyers=$taskCase.Flyers; HostExit=$taskHost.ExitCode; GuestExit=$taskGuest.ExitCode} |
        ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "${taskName}_Exit.json")
    $taskRsp = Join-Path $taskOutput "${taskName}_Export.rsp"
    $taskCsv = (Join-Path $taskOutput "${taskName}_AllStats_{region}.csv").Replace('\','/')
    "TimingInsights.ExportTimerStatistics $taskCsv -region=LNPLoadBaselineCapture -sortBy=TotalInclusiveTime" |
        Set-Content -Encoding UTF8 -LiteralPath $taskRsp
    $taskExportLog = Join-Path $taskOutput "${taskName}_Insights.log"
    $taskExport = Start-Process -FilePath $taskInsights -ArgumentList "-OpenTraceFile=$taskTrace -AutoQuit -NoUI -ExecOnAnalysisCompleteCmd=@=$taskRsp -ABSLOG=$taskExportLog -unattended" -WindowStyle Hidden -PassThru -Wait
    if ($taskExport.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $taskCsv.Replace('{region}','LNPLoadBaselineCapture'))) {
        throw "$taskName Insights export failed"
    }
    Write-Output "$taskName host/guest/export exit 0"
    Select-String -LiteralPath $taskHostLog -Pattern '\[LoadBaseline\] (FrameMs|Enemy|Flight|UnknownHits|Exact)' | ForEach-Object { $_.Line }
}
