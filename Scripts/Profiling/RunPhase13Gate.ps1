# Phase 13 패키지의 trace 없는 대조·회귀 측정. 실제 개체 수와 합격 여부는 로그를 따른다.
param(
    [string[]]$Cases = @('Production', 'AlwaysActive', 'N700', 'N700Chase', 'F200'),
    [int]$Repeats = 2,
    [switch]$Trace,
    [string]$PackageDirectory = 'Saved/SurfaceNavigationPhase13Package',
    [string]$OutputDirectory = '',
    [int]$FirstRepeat = 1
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskExe = Join-Path (Join-Path $taskRoot $PackageDirectory) 'Windows/LootNPop/Binaries/Win64/LootNPop.exe'
$taskOutput = Join-Path $taskRoot 'Saved/Profiling/Phase13/After'
if ($Trace) { $taskOutput = Join-Path $taskRoot 'Saved/Profiling/Phase13/AfterTrace' }
if ($OutputDirectory) { $taskOutput = Join-Path $taskRoot $OutputDirectory }
if ($Repeats -lt 1 -or $FirstRepeat -lt 1) { throw 'Repeat counts must be positive.' }
if (-not (Test-Path -LiteralPath $taskExe)) { throw "Package missing: $taskExe" }
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
if (Get-Process -Name UnrealEditor,UnrealEditor-Cmd,LootNPop,UnrealInsights -ErrorAction SilentlyContinue) {
    throw 'Close editor, games and Insights before measurement.'
}
$taskArguments = @{
    Production = '-LNPLoadBaselineProduction'
    AlwaysActive = '-LNPLoadBaselineProduction -LNPLoadBaselineAlwaysActive'
    N700 = '-LNPLoadBaseline=700'
    N700Chase = '-LNPLoadBaseline=700 -LNPLoadBaselineChase'
    N800 = '-LNPLoadBaseline=800'
    N800Chase = '-LNPLoadBaseline=800 -LNPLoadBaselineChase'
    F200 = '-LNPLoadBaselineFlyers=200'
    F300 = '-LNPLoadBaselineFlyers=300'
    ProductionWideSpawn = '-LNPLoadBaselineProduction -LNPLoadBaselineSpawnRadius=1500'
    WanderObserve = '-LNPLoadBaselineProduction -LNPLoadBaselineObserveWander -LNPLoadBaselineSpawnRadius=1500'
    WanderPoseControl = '-LNPLoadBaselineProduction -LNPLoadBaselineObserveWander -LNPLoadBaselineSpawnRadius=1500 -LNPLoadBaselineWanderPoseControl'
}
foreach ($taskCase in $Cases) {
    if (-not $taskArguments.ContainsKey($taskCase)) { throw "Unknown case: $taskCase" }
    foreach ($taskRepeat in $FirstRepeat..($FirstRepeat + $Repeats - 1)) {
        $taskName = "${taskCase}_Repeat${taskRepeat}"
        $taskHostLog = Join-Path $taskOutput "${taskName}_Host.log"
        $taskGuestLog = Join-Path $taskOutput "${taskName}_Guest.log"
        if ((Test-Path -LiteralPath $taskHostLog) -or (Test-Path -LiteralPath $taskGuestLog)) {
            throw "Preserve previous evidence before rerunning $taskName."
        }
        $taskCommon = '-nullrhi -nosound -corelimit=4 -unattended -FixedSeed -LNPLoadBaselineSeed=1 -LNPLoadBaselinePlayers=2 -LNPLoadBaselineQuit'
        $taskCommon += " $($taskArguments[$taskCase])"
        $taskCommon += ' -dpcvars=LNP.SurfaceNav.EnemySupportCache=1,LNP.SurfaceNav.EnemyExactLateralSweep=1,LNP.SurfaceNav.EnemyParallelMovement=1'
        Write-Output "$taskName started"
        $taskTraceArguments = ''
        $taskTracePath = Join-Path $taskOutput "${taskName}_Host.utrace"
        if ($Trace) { $taskTraceArguments = "-trace=cpu,frame,bookmark,region,task -tracefile=$taskTracePath" }
        $taskHost = Start-Process -FilePath $taskExe -ArgumentList "TestMap03?Listen $taskCommon $taskTraceArguments -abslog=$taskHostLog" -WindowStyle Hidden -PassThru
        $taskGuest = $null
        try {
            Start-Sleep -Seconds 5
            if ($taskHost.HasExited) { throw "$taskName host exited before guest startup" }
            # 관찰 대조의 게스트만 60fps로 제한해 이동 예측 reliable 입력 큐의 headless 폭주를 피한다.
            $taskGuestLimit = if ($taskCase -in @('WanderObserve', 'WanderPoseControl')) { ' -ExecCmds="t.MaxFPS 60"' } else { '' }
            $taskGuest = Start-Process -FilePath $taskExe -ArgumentList "127.0.0.1 $taskCommon $taskGuestLimit -abslog=$taskGuestLog" -WindowStyle Hidden -PassThru
            $taskDeadline = (Get-Date).AddMinutes(8)
            while ((Get-Date) -lt $taskDeadline -and (-not $taskHost.HasExited -or -not $taskGuest.HasExited)) {
                Start-Sleep -Seconds 2
            }
            foreach ($taskProcess in @($taskHost, $taskGuest)) {
                if (-not $taskProcess.HasExited) { throw "$taskName timed out" }
                if ($taskProcess.ExitCode -ne 0) { throw "$taskName exited $($taskProcess.ExitCode)" }
            }
            foreach ($taskLog in @($taskHostLog, $taskGuestLog)) {
                $taskText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskLog
                if ($taskText -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]' -or
                    ([regex]::Matches($taskText, '\[(ProbeFaceIndex|ProbeSourceKeys|ProbeSurfaceData)\].*-> PASS')).Count -ne 3 -or
                    $taskText -notmatch '\[LoadBaseline\] FrameMs') { throw "$taskName functional checks failed: $taskLog" }
            }
            # 같은 패키지의 후속 실행이 덮어쓰는 경로 요청·timing 원본도 실행별로 보존한다.
            $taskHostText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskHostLog
            $taskNavCapture = [regex]::Match($taskHostText, '\[LoadBaseline\] Nav capture saved: [^\r\n]*[/\\]([^/\\\r\n]+\.csv)')
            if ($taskNavCapture.Success) {
                $taskNavName = $taskNavCapture.Groups[1].Value
                $taskPackageLogs = Join-Path (Join-Path $taskRoot $PackageDirectory) 'Windows/LootNPop/Saved/Logs'
                Copy-Item -LiteralPath (Join-Path $taskPackageLogs $taskNavName) -Destination (Join-Path $taskOutput "${taskName}_NavRequests.csv")
                Copy-Item -LiteralPath (Join-Path $taskPackageLogs ($taskNavName -replace '\.csv$', '_Timings.csv')) -Destination (Join-Path $taskOutput "${taskName}_NavTimings.csv")
            }
            [pscustomobject]@{Run=$taskName; HostExit=$taskHost.ExitCode; GuestExit=$taskGuest.ExitCode; PackageSHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $taskExe).Hash} |
                ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "${taskName}_Exit.json")
            Select-String -LiteralPath $taskHostLog -Pattern '\[LoadBaseline\] (FrameMs|NavGate|Gate|Replication|UnknownHits)' | ForEach-Object { $_.Line }
            Select-String -LiteralPath $taskGuestLog -Pattern '\[LoadBaseline\] GuestNetwork' | ForEach-Object { $_.Line }
            if ($Trace) {
                $taskCsv = (Join-Path $taskOutput "${taskName}_{region}.csv").Replace('\','/')
                $taskRsp = Join-Path $taskOutput "${taskName}_Export.rsp"
                "TimingInsights.ExportTimerStatistics $taskCsv -region=LNPLoadBaselineCapture -sortBy=TotalInclusiveTime" |
                    Set-Content -Encoding UTF8 -LiteralPath $taskRsp
                $taskInsights = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealInsights.exe'
                $taskExportLog = Join-Path $taskOutput "${taskName}_Insights.log"
                $taskExport = Start-Process -FilePath $taskInsights -ArgumentList "-OpenTraceFile=$taskTracePath -AutoQuit -NoUI -ExecOnAnalysisCompleteCmd=@=$taskRsp -ABSLOG=$taskExportLog -unattended" -WindowStyle Hidden -PassThru -Wait
                if ($taskExport.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $taskCsv.Replace('{region}','LNPLoadBaselineCapture'))) {
                    throw "$taskName Insights export failed"
                }
            }
        }
        finally {
            foreach ($taskProcess in @($taskHost, $taskGuest)) {
                if ($taskProcess -and -not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id -Force }
            }
        }
    }
}
