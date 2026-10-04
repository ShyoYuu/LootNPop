# 교차 실행의 trace 없는 Gate와 trace CPU 귀속을 분리한다. 병렬 부모 inclusive는 대기를 포함한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase13/GroundAttribution')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOutput = Join-Path $taskRoot $InputDirectory
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
function Read-Number([string]$Value) { [double]::Parse($Value, $taskCulture) }
$taskRuns = foreach ($taskGroup in @('Control', 'Current', 'ControlTrace', 'CurrentTrace')) {
    $taskDirectory = Join-Path $taskOutput $taskGroup
    foreach ($taskMetaFile in Get-ChildItem -LiteralPath $taskDirectory -Filter '*_Exit.json') {
        $taskMeta = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskMetaFile.FullName | ConvertFrom-Json
        if ($taskMeta.HostExit -ne 0 -or $taskMeta.GuestExit -ne 0) { throw "Failed process: $taskMetaFile" }
        $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskDirectory "$($taskMeta.Run)_Host.log")
        $taskFrame = [regex]::Match($taskHost, '\[LoadBaseline\] FrameMs P50=([\d.]+) P95=([\d.]+)')
        $taskPopulation = [regex]::Match($taskHost, 'frames=(\d+) EnemyEntities=(\d+) EnemyChunks=(\d+) LargestChunk=(\d+) PromotedActors avg=([\d.]+) max=(\d+) Projectiles avg=([\d.]+) min=(\d+)')
        if (-not $taskFrame.Success -or -not $taskPopulation.Success -or $taskPopulation.Groups[2].Value -ne '700') { throw "Invalid ground capture: $taskMetaFile" }
        $taskRequestCount = $null
        $taskTimingCount = $null
        $taskRequestPath = Join-Path $taskDirectory "$($taskMeta.Run)_NavRequests.csv"
        if (Test-Path -LiteralPath $taskRequestPath) {
            $taskRequests = @(Import-Csv -LiteralPath $taskRequestPath)
            $taskTimings = @(Import-Csv -LiteralPath (Join-Path $taskDirectory "$($taskMeta.Run)_NavTimings.csv"))
            $taskRequestCount = $taskRequests.Count
            $taskTimingCount = $taskTimings.Count
            $taskSubmitted = [regex]::Match($taskHost, '\[LoadBaseline\] Nav chase=[^\r\n]*submitted=(\d+)')
            if (-not $taskSubmitted.Success -or $taskRequestCount -ne [int]$taskSubmitted.Groups[1].Value -or $taskRequestCount -ne $taskTimingCount) {
                throw "Nav capture row mismatch: $taskMetaFile"
            }
            $taskKeys = [Collections.Generic.HashSet[string]]::new()
            $taskLastTime = -1.0
            foreach ($taskRequest in $taskRequests) {
                $taskTime = Read-Number $taskRequest.time
                if ($taskTime -lt $taskLastTime -or -not $taskKeys.Add("$($taskRequest.owner)/$($taskRequest.ownerSerial)/$($taskRequest.requestSerial)")) {
                    throw "Duplicate or reversed request: $taskMetaFile"
                }
                $taskLastTime = $taskTime
            }
            foreach ($taskTiming in $taskTimings) {
                if (-not $taskKeys.Remove("$($taskTiming.owner)/$($taskTiming.ownerSerial)/$($taskTiming.requestSerial)")) {
                    throw "Duplicate or unmatched timing: $taskMetaFile"
                }
            }
            if ($taskKeys.Count) { throw "Missing timing: $taskMetaFile" }
        }
        $taskTimers = @()
        $taskSeconds = $null
        if ($taskGroup.EndsWith('Trace')) {
            $taskExport = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskDirectory "$($taskMeta.Run)_Insights.log")
            $taskRegion = [regex]::Match($taskExport, "region 'LNPLoadBaselineCapture' \[([\d.]+) .. ([\d.]+)\]")
            if (-not $taskRegion.Success) { throw "Missing trace region: $taskMetaFile" }
            $taskSeconds = (Read-Number $taskRegion.Groups[2].Value) - (Read-Number $taskRegion.Groups[1].Value)
            if ($taskSeconds -lt 29 -or $taskSeconds -gt 31) { throw "Invalid trace duration: $taskMetaFile" }
            $taskTimers = @(foreach ($taskRow in Import-Csv -LiteralPath (Join-Path $taskDirectory "$($taskMeta.Run)_LNPLoadBaselineCapture.csv")) {
                if ($taskRow.Name -notmatch 'Processor_\d+$|^LNPWorldCollision_|^LNPNavPathSubsystem_Tick$') { continue }
                $taskCount = [int]$taskRow.Count
                if ($taskCount -le 0) { throw 'Empty timer' }
                [pscustomobject]@{
                    Name=($taskRow.Name -replace '_\d+$',''); Count=$taskCount
                    InclusiveMsPerCall=1000*(Read-Number $taskRow.Incl)/$taskCount
                    ExclusiveMsPerCall=1000*(Read-Number $taskRow.Excl)/$taskCount
                    ExclusiveMsPerSecond=1000*(Read-Number $taskRow.Excl)/$taskSeconds
                }
            })
        }
        [pscustomobject]@{
            Group=$taskGroup; Run=$taskMeta.Run; PackageSHA256=$taskMeta.PackageSHA256
            FrameP50Ms=(Read-Number $taskFrame.Groups[1].Value); FrameP95Ms=(Read-Number $taskFrame.Groups[2].Value)
            NavGate=[regex]::Match($taskHost, '\[LoadBaseline\] NavGate[^\r\n]*').Value
            Frames=[int]$taskPopulation.Groups[1].Value; Chunks=[int]$taskPopulation.Groups[3].Value
            LargestChunk=[int]$taskPopulation.Groups[4].Value; PromotedActorsAverage=(Read-Number $taskPopulation.Groups[5].Value)
            ProjectilesAverage=(Read-Number $taskPopulation.Groups[7].Value); ProjectilesMinimum=[int]$taskPopulation.Groups[8].Value
            NavRequestRows=$taskRequestCount; NavTimingRows=$taskTimingCount; CaptureSeconds=$taskSeconds; Timers=$taskTimers
        }
    }
}
$taskControl = $taskRuns | Where-Object Group -EQ ControlTrace
$taskCurrent = $taskRuns | Where-Object Group -EQ CurrentTrace
$taskComparison = foreach ($taskTimer in $taskCurrent.Timers) {
    $taskBefore = $taskControl.Timers | Where-Object Name -EQ $taskTimer.Name
    [pscustomobject]@{
        Name=$taskTimer.Name
        ControlInclusiveMsPerCall=if ($taskBefore) { $taskBefore.InclusiveMsPerCall } else { $null }
        CurrentInclusiveMsPerCall=$taskTimer.InclusiveMsPerCall
        InclusiveDeltaMsPerCall=$taskTimer.InclusiveMsPerCall - $(if ($taskBefore) { $taskBefore.InclusiveMsPerCall } else { 0 })
        ControlExclusiveMsPerCall=if ($taskBefore) { $taskBefore.ExclusiveMsPerCall } else { $null }
        CurrentExclusiveMsPerCall=$taskTimer.ExclusiveMsPerCall
    }
}
[pscustomobject]@{Runs=@($taskRuns); TimerComparison=@($taskComparison)} | ConvertTo-Json -Depth 7 |
    Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Summary.json')
$taskRuns | Select-Object Group,Run,FrameP95Ms,Chunks,PromotedActorsAverage,NavRequestRows | Format-Table -AutoSize
$taskComparison | Sort-Object InclusiveDeltaMsPerCall -Descending | Select-Object -First 12 | Format-Table -AutoSize
