# 같은 패키지의 700/800 자연·추격 Gate와 CPU 귀속을 구분한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase13/GroundPopulationAttribution')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOutput = Join-Path $taskRoot $InputDirectory
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
function Read-Number([string]$Value) { [double]::Parse($Value, $taskCulture) }
$taskRuns = foreach ($taskGroup in @('Gate', 'Trace')) {
    $taskDirectory = Join-Path $taskOutput $taskGroup
    & (Join-Path $PSScriptRoot 'AnalyzePhase13Gate.ps1') -InputDirectory "$InputDirectory/$taskGroup" | Out-Host
    $taskSummary = @(Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskDirectory 'Summary.json') | ConvertFrom-Json)
    if ($taskSummary.Count -ne $(if ($taskGroup -eq 'Gate') { 8 } else { 4 })) { throw "Incomplete $taskGroup" }
    foreach ($taskRun in $taskSummary) {
        $taskMode = [regex]::Match($taskRun.Run, '^(N(700|800)(Chase)?)_Repeat[12]$')
        if (-not $taskMode.Success) { throw "Unexpected run: $($taskRun.Run)" }
        $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskDirectory "$($taskRun.Run)_Host.log")
        $taskGuest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskDirectory "$($taskRun.Run)_Guest.log")
        $taskLogs = $taskHost + "`n" + $taskGuest
        $taskProbes = [regex]::Matches($taskLogs, '\[Probe[^\]]*\][^\r\n]*-> PASS').Count
        if ($taskRun.HostExit -ne 0 -or $taskRun.GuestExit -ne 0 -or $taskProbes -ne 8 -or
            $taskLogs -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]') { throw 'Runtime validation failed' }
        if (-not $taskRun.RingPopulationEligible -or $taskRun.RequestedGround -ne [int]$taskMode.Groups[2].Value -or
            $taskRun.RequestedFlight -ne 0 -or $taskRun.ActivitySamples.Count -ne 30 -or
            ($taskHost -match '-LNPLoadBaselineChase') -ne $taskMode.Groups[3].Success -or
            ($taskHost -match '-trace=cpu') -ne ($taskGroup -eq 'Trace') -or
            $taskHost -match 'LNPLoadBaselineProduction|LNPLoadBaselineObserveWander|LNPLoadBaselineActivityDiagnostics|LNPLoadBaselineWanderPoseControl|LNPLoadBaselineSpawnRadius') { throw 'Population or mode validation failed' }
        $taskRequests = @(Import-Csv -LiteralPath (Join-Path $taskDirectory "$($taskRun.Run)_NavRequests.csv"))
        $taskTimings = @(Import-Csv -LiteralPath (Join-Path $taskDirectory "$($taskRun.Run)_NavTimings.csv"))
        $taskSubmitted = [regex]::Match($taskHost, '\[LoadBaseline\] Nav chase=[^\r\n]*submitted=(\d+)')
        if (-not $taskSubmitted.Success -or $taskRequests.Count -ne [int]$taskSubmitted.Groups[1].Value -or $taskRequests.Count -ne $taskTimings.Count) { throw 'Nav row mismatch' }
        $taskKeys = [Collections.Generic.HashSet[string]]::new()
        $taskLastTime = -1.0
        foreach ($taskRequest in $taskRequests) {
            $taskTime = Read-Number $taskRequest.time
            if ($taskTime -lt $taskLastTime -or -not $taskKeys.Add("$($taskRequest.owner)/$($taskRequest.ownerSerial)/$($taskRequest.requestSerial)")) { throw 'Duplicate or reversed request' }
            $taskLastTime = $taskTime
        }
        foreach ($taskTiming in $taskTimings) {
            if (-not $taskKeys.Remove("$($taskTiming.owner)/$($taskTiming.ownerSerial)/$($taskTiming.requestSerial)")) { throw 'Duplicate or unmatched timing' }
        }
        if ($taskKeys.Count) { throw 'Missing timing' }
        $taskPopulation = [regex]::Match($taskHost, 'frames=(\d+) EnemyEntities=(\d+) EnemyChunks=(\d+) LargestChunk=(\d+) PromotedActors avg=([\d.]+) max=(\d+) Projectiles avg=([\d.]+) min=(\d+)')
        if (-not $taskPopulation.Success) { throw 'Missing workload counters' }
        $taskFrames = [int]$taskPopulation.Groups[1].Value
        # Report의 평균은 0.01us 반올림값이므로 클래스 시간은 근사치다. exact 총량과 중복 합산하지 않는다.
        $taskExactClasses = @(foreach ($taskMatch in [regex]::Matches($taskHost,
            '\[WorldCollision\] (\w+)\s+count=(\d+) hits=(\d+) avg=([\d.]+)us max=([\d.]+)us lock=([\d.]+)ms')) {
            $taskCount = [uint64]$taskMatch.Groups[2].Value
            [pscustomobject]@{
                Class=$taskMatch.Groups[1].Value; Count=$taskCount; Hits=[uint64]$taskMatch.Groups[3].Value
                QueriesPerFrame=$taskCount/$taskFrames; AverageUs=(Read-Number $taskMatch.Groups[4].Value)
                ApproximateQueryMsPerFrame=$taskCount*(Read-Number $taskMatch.Groups[4].Value)/1000/$taskFrames
                LockProbeMsPerFrame=(Read-Number $taskMatch.Groups[6].Value)/$taskFrames
            }
        })
        $taskGroundCache = [regex]::Match($taskHost, 'EnemyGround: mode=cache-first steps=(\d+) cache_hits=(\d+) exact_fallbacks=(\d+) exact_only=(\d+) cache_rate=([\d.]+)%')
        $taskSeconds = $null
        $taskTimers = @()
        if ($taskGroup -eq 'Trace') {
            $taskExport = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskDirectory "$($taskRun.Run)_Insights.log")
            $taskRegion = [regex]::Match($taskExport, "region 'LNPLoadBaselineCapture' \[([\d.]+) .. ([\d.]+)\]")
            if (-not $taskRegion.Success) { throw 'Missing trace region' }
            $taskSeconds = (Read-Number $taskRegion.Groups[2].Value) - (Read-Number $taskRegion.Groups[1].Value)
            if ($taskSeconds -lt 29 -or $taskSeconds -gt 31) { throw 'Invalid trace region' }
            # 동일 이름의 timer ID를 합쳐 호출당 분모를 유지한다. 부모/하위 범위는 합산하지 않는다.
            $taskTimers = @(Import-Csv -LiteralPath (Join-Path $taskDirectory "$($taskRun.Run)_LNPLoadBaselineCapture.csv") |
                Group-Object { $_.Name -replace '_\d+$','' } | ForEach-Object {
                    $taskCount = 0; $taskIncl = 0.0; $taskExcl = 0.0
                    foreach ($taskRow in $_.Group) {
                        $taskCount += [int]$taskRow.Count
                        $taskIncl += Read-Number $taskRow.Incl
                        $taskExcl += Read-Number $taskRow.Excl
                    }
                    if ($taskCount -le 0 -or $taskExcl -gt $taskIncl + 0.000001) { throw 'Invalid timer' }
                    [pscustomobject]@{
                        Name=$_.Name; Count=$taskCount; InclusiveMsPerCall=1000*$taskIncl/$taskCount
                        ExclusiveMsPerCall=1000*$taskExcl/$taskCount; InclusiveMsPerFrame=1000*$taskIncl/$taskFrames
                        ExclusiveMsPerFrame=1000*$taskExcl/$taskFrames; ExclusiveMsPerSecond=1000*$taskExcl/$taskSeconds
                    }
                })
        }
        [pscustomobject]@{
            Group=$taskGroup; Case=$taskMode.Groups[1].Value; Run=$taskRun.Run; PackageSHA256=$taskRun.PackageSHA256
            Population=$taskRun.ActualPopulation; FrameP50Ms=$taskRun.FrameP50Ms; FrameP95Ms=$taskRun.FrameP95Ms
            FramePass16_6=$taskRun.RingFrameGatePassed; NavGate=$taskRun.NavGate; ExactP95Ms=$taskRun.ExactP95Ms
            NavSummary=[regex]::Match($taskHost, '\[LoadBaseline\] Nav chase=[^\r\n]*').Value
            NavStages=[regex]::Match($taskHost, '\[LoadBaseline\] Nav stagesMs[^\r\n]*').Value
            Frames=$taskFrames; Chunks=[int]$taskPopulation.Groups[3].Value; LargestChunk=[int]$taskPopulation.Groups[4].Value
            PromotedActorsAverage=(Read-Number $taskPopulation.Groups[5].Value); ProjectilesAverage=(Read-Number $taskPopulation.Groups[7].Value)
            ProjectilesMinimum=[int]$taskPopulation.Groups[8].Value; NavRequestRows=$taskRequests.Count; ProbePass=$taskProbes
            ActivitySamples=$taskRun.ActivitySamples.Count; CaptureSeconds=$taskSeconds; Timers=$taskTimers
            ExactClasses=$taskExactClasses; GroundCacheRatePercent=$(if ($taskGroundCache.Success) { Read-Number $taskGroundCache.Groups[5].Value } else { $null })
        }
    }
}
if (@($taskRuns.PackageSHA256 | Select-Object -Unique).Count -ne 1) { throw 'Package mismatch' }
foreach ($taskCase in @('N700','N800','N700Chase','N800Chase')) {
    if (@($taskRuns | Where-Object { $_.Case -eq $taskCase -and $_.Group -eq 'Gate' }).Count -ne 2 -or
        @($taskRuns | Where-Object { $_.Case -eq $taskCase -and $_.Group -eq 'Trace' }).Count -ne 1) { throw 'Missing case' }
}
$taskComparison = foreach ($taskChase in @('', 'Chase')) {
    $taskControl = $taskRuns | Where-Object { $_.Group -eq 'Trace' -and $_.Case -eq "N700$taskChase" }
    $taskLoaded = $taskRuns | Where-Object { $_.Group -eq 'Trace' -and $_.Case -eq "N800$taskChase" }
    foreach ($taskTimer in $taskLoaded.Timers | Where-Object Name -Match '^LNP|^Mass.*Processor') {
        $taskBefore = $taskControl.Timers | Where-Object Name -EQ $taskTimer.Name
        [pscustomobject]@{
            Mode=$(if ($taskChase) { 'Chase' } else { 'Natural' }); Name=$taskTimer.Name
            ControlInclusiveMsPerCall=$taskBefore.InclusiveMsPerCall; LoadedInclusiveMsPerCall=$taskTimer.InclusiveMsPerCall
            ControlExclusiveMsPerFrame=$taskBefore.ExclusiveMsPerFrame; LoadedExclusiveMsPerFrame=$taskTimer.ExclusiveMsPerFrame
            ExclusiveDeltaMsPerFrame=$taskTimer.ExclusiveMsPerFrame - $taskBefore.ExclusiveMsPerFrame
        }
    }
}
[pscustomobject]@{Runs=@($taskRuns); TimerComparison=@($taskComparison); SamePackage=$true; RuntimeAndCSVValidation=$true;
    ParallelParentIncludesWait=$true; ExactIncludesAllConsumers=$true; MaximumPopulationEstablished=$false} |
    ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Summary.json')
$taskRuns | Select-Object Group,Run,FrameP95Ms,ExactP95Ms,PromotedActorsAverage,NavRequestRows | Format-Table -AutoSize
$taskComparison | Sort-Object ExclusiveDeltaMsPerFrame -Descending | Select-Object -First 16 | Format-Table -AutoSize
