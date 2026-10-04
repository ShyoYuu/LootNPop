# 기존 인구·Nav·trace 검증을 재사용하고 투사체 상세 계측의 완결성과 분모를 확인한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase12/Baseline')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOutput = Join-Path $taskRoot $InputDirectory
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
function Read-Number([string]$Value) { [double]::Parse($Value, $taskCulture) }
function Read-Fields([string]$Line) {
    $taskFields = @{}
    foreach ($taskMatch in [regex]::Matches($Line, '(\w+)=([^\s]+)')) {
        $taskFields[$taskMatch.Groups[1].Value] = $taskMatch.Groups[2].Value
    }
    return $taskFields
}
& (Join-Path $PSScriptRoot 'AnalyzePhase13GroundPopulation.ps1') -InputDirectory $InputDirectory | Out-Host
$taskBase = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Summary.json') | ConvertFrom-Json
$taskPackageHash = $taskBase.Runs[0].PackageSHA256
foreach ($taskGroup in @('DiagnosticOff','Diagnostic128')) {
    & (Join-Path $PSScriptRoot 'AnalyzePhase13Gate.ps1') -InputDirectory "$InputDirectory/$taskGroup" | Out-Host
    $taskExit = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/N700_Repeat1_Exit.json") | ConvertFrom-Json
    if ($taskExit.HostExit -ne 0 -or $taskExit.GuestExit -ne 0 -or $taskExit.PackageSHA256 -ne $taskPackageHash) { throw 'Diagnostic package or process mismatch' }
    $taskDiagnostic = @(Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/Summary.json") | ConvertFrom-Json)
    $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/N700_Repeat1_Host.log")
    $taskGuest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/N700_Repeat1_Guest.log")
    if ($taskDiagnostic.Count -ne 1 -or -not $taskDiagnostic[0].RingPopulationEligible -or $taskDiagnostic[0].ActivitySamples.Count -ne 30 -or
        [regex]::Matches($taskHost + $taskGuest, '\[Probe[^\]]*\][^\r\n]*-> PASS').Count -ne 8 -or
        ($taskHost + $taskGuest) -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]') { throw 'Diagnostic runtime validation failed' }
}
foreach ($taskGateLog in Get-ChildItem -LiteralPath (Join-Path $taskOutput 'Gate') -Filter '*_Host.log') {
    $taskGateText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskGateLog.FullName
    if ($taskGateText -notmatch 'LNP.Projectile.ProfileStride=0(?:\s|$)' -or $taskGateText -match '\[ProjectileProfile\]') { throw 'Detailed profiling was active during Gate' }
}
$taskProfiles = foreach ($taskGroup in @('Trace','DiagnosticOff','Diagnostic128')) {
    foreach ($taskLog in Get-ChildItem -LiteralPath (Join-Path $taskOutput $taskGroup) -Filter '*_Host.log') {
        $taskText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskLog.FullName
        $taskStride = if ($taskGroup -eq 'Trace') { 64 } elseif ($taskGroup -eq 'Diagnostic128') { 128 } else { 0 }
        if ($taskText -notmatch "LNP.Projectile.ProfileStride=$taskStride(?:\s|$)") { throw 'Unexpected profile stride' }
        $taskSummary = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/Summary.json") | ConvertFrom-Json
        $taskName = $taskLog.BaseName -replace '_Host$',''
        $taskRun = @($taskSummary | Where-Object Run -eq $taskName)[0]
        if (-not $taskRun.RingPopulationEligible -or $taskRun.ActivitySamples.Count -ne 30) { throw 'Invalid activity capture' }
        $taskFirst = Read-Number ([regex]::Matches($taskText, '\[LoadBaseline\] Activity T=([\d.]+)')[0].Groups[1].Value)
        $taskLast = Read-Number ([regex]::Matches($taskText, '\[LoadBaseline\] Activity T=([\d.]+)')[-1].Groups[1].Value)
        $taskWindows = @([regex]::Matches($taskText, '\[ProjectileProfile\] server=1 [^\r\n]+') | ForEach-Object { Read-Fields $_.Value } |
            Where-Object { (Read-Number $_.start) -ge $taskFirst -and (Read-Number $_.end) -le $taskLast })
        if ($taskStride -eq 0) {
            if ($taskText -match '\[ProjectileProfile\]') { throw 'Disabled profiling emitted diagnostics' }
            continue
        }
        if ($taskWindows.Count -lt 26) { throw 'Incomplete profile windows' }
        $taskFrames = 0.0; $taskProjectiles = 0.0; $taskSamples = 0.0; $taskCandidates = 0.0
        $taskSelf = 0.0; $taskTeamExcluded = 0.0; $taskEnemyTeam = 0.0; $taskRemote = 0.0; $taskApplied = 0.0; $taskClock = 0.0
        $taskStageSums = @{}
        foreach ($taskWindow in $taskWindows) {
            if ([int]$taskWindow.stride -ne $taskStride -or (Read-Number $taskWindow.frames) -le 0 -or
                (Read-Number $taskWindow.samples) -le 0) { throw 'Invalid profile window' }
            $taskStages = @([regex]::Matches($taskText, '\[ProjectileProfileStage\] server=1 [^\r\n]+') |
                ForEach-Object { Read-Fields $_.Value } | Where-Object end -eq $taskWindow.end)
            if ($taskStages.Count -ne 8 -or @($taskStages.stage | Select-Object -Unique).Count -ne 8) { throw 'Incomplete stage block' }
            $taskTargets = @($taskStages | Where-Object stage -eq Targets)[0]
            $taskWorld = @($taskStages | Where-Object stage -eq World)[0]
            if ($taskTargets.calls -ne $taskWindow.candidates -or $taskWorld.calls -ne $taskWindow.projectiles) { throw 'Stage count mismatch' }
            foreach ($taskStage in $taskStages) {
                if (-not $taskStageSums.ContainsKey($taskStage.stage)) { $taskStageSums[$taskStage.stage] = @{ Calls=0.0; Ms=0.0 } }
                $taskStageSums[$taskStage.stage].Calls += Read-Number $taskStage.calls
                $taskStageSums[$taskStage.stage].Ms += Read-Number $taskStage.ms
            }
            $taskFrames += Read-Number $taskWindow.frames; $taskProjectiles += Read-Number $taskWindow.projectiles
            $taskSamples += Read-Number $taskWindow.samples; $taskCandidates += Read-Number $taskWindow.candidates
            $taskSelf += Read-Number $taskWindow.self; $taskTeamExcluded += Read-Number $taskWindow.teamExcluded
            $taskEnemyTeam += Read-Number $taskWindow.enemyTeamPairs; $taskRemote += Read-Number $taskWindow.remote
            $taskApplied += Read-Number $taskWindow.rewindApplied
            $taskClock += (Read-Number $taskWindow.clockNs) * (Read-Number $taskWindow.frames)
        }
        if ([math]::Abs($taskSamples * $taskStride / $taskProjectiles - 1.0) -gt 0.01) { throw 'Unexpected sample coverage' }
        $taskStages = foreach ($taskName in @('CollectEnemies','CollectPlayers','World','Targets','Filter','Rewind','History','Capsule')) {
            $taskStage = $taskStageSums[$taskName]
            $taskScale = if ($taskName -in @('CollectEnemies','CollectPlayers','World')) { 1.0 } else { $taskProjectiles / $taskSamples }
            [pscustomobject]@{
                Stage=$taskName; Calls=$taskStage.Calls; RecordedMs=$taskStage.Ms
                RawNsPerCall=$(if ($taskStage.Calls -gt 0) { 1.e6*$taskStage.Ms/$taskStage.Calls } else { $null })
                InstrumentedMsPerFrameEstimate=$taskStage.Ms*$taskScale/$taskFrames
                # 시계 간격의 참고치만 표시한다. 중첩 범위에서 이 값을 빼 실제 비용으로 단정하지 않는다.
                ClockIntervalMsPerFrame=$taskStage.Calls*($taskClock/$taskFrames)*1.e-6*$taskScale/$taskFrames
                Sampled=$taskScale -gt 1
            }
        }
        [pscustomobject]@{
            Group=$taskGroup; Run=$taskLog.BaseName -replace '_Host$',''; Stride=$taskStride; Windows=$taskWindows.Count
            Frames=$taskFrames; Projectiles=$taskProjectiles; Samples=$taskSamples
            CandidatesPerSample=$taskCandidates/$taskSamples; SelfExcluded=$taskSelf; TeamExcluded=$taskTeamExcluded
            EnemyTeamPairFraction=$taskEnemyTeam/$taskCandidates; RemoteSamples=$taskRemote; RewindApplied=$taskApplied
            ClockPairNs=$taskClock/$taskFrames; Stages=@($taskStages)
        }
    }
}
$taskOffLog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'DiagnosticOff/N700_Repeat1_Insights.log')
$taskOffRegion = [regex]::Match($taskOffLog, "region 'LNPLoadBaselineCapture' \[([\d.]+) .. ([\d.]+)\]")
if (-not $taskOffRegion.Success) { throw 'Missing diagnostic trace export' }
$taskDiagnosticTimers = foreach ($taskGroup in @('DiagnosticOff','Diagnostic128')) {
    $taskRows = @(Import-Csv -LiteralPath (Join-Path $taskOutput "$taskGroup/N700_Repeat1_LNPLoadBaselineCapture.csv") |
        Where-Object Name -match '^LNPProjectileHitDetectionProcessor(?:_\d+)?$')
    if (-not $taskRows.Count) { throw 'Missing projectile timer' }
    $taskCalls = 0.0; $taskMs = 0.0
    foreach ($taskRow in $taskRows) { $taskCalls += Read-Number $taskRow.Count; $taskMs += 1000*(Read-Number $taskRow.Incl) }
    [pscustomobject]@{Group=$taskGroup; InclusiveMsPerCall=$taskMs/$taskCalls}
}
[pscustomobject]@{
    PackageSHA256=$taskPackageHash; Profiles=@($taskProfiles); DiagnosticTimers=@($taskDiagnosticTimers)
    SampledTimesIncludeClockAndScopeOverhead=$true; StageTimesAreNested=$true
    HistoryWorkloadRepresented=(@($taskProfiles | Where-Object RewindApplied -gt 0).Count -gt 0)
} | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'ProjectileSummary.json')
$taskProfiles | Select-Object Group,Run,Windows,CandidatesPerSample,EnemyTeamPairFraction,RemoteSamples,RewindApplied,ClockPairNs | Format-Table -AutoSize
foreach ($taskProfile in $taskProfiles) {
    Write-Output "$($taskProfile.Group)/$($taskProfile.Run)"
    $taskProfile.Stages | Select-Object Stage,RawNsPerCall,InstrumentedMsPerFrameEstimate | Format-Table -AutoSize
}
