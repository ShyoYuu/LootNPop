# 최종 기본 경로의 Gate·부하 검증을 재사용하고 현재 월드 query 비용을 별도로 보존한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase12/Final')
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
$taskHash = $taskBase.Runs[0].PackageSHA256
& (Join-Path $PSScriptRoot 'AnalyzePhase13Gate.ps1') -InputDirectory "$InputDirectory/Sampled" | Out-Host
$taskSampled = @(Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Sampled/Summary.json') | ConvertFrom-Json)
if ($taskSampled.Count -ne 2 -or (@($taskSampled.Run | Sort-Object) -join ',') -ne 'N700_Repeat1,N800Chase_Repeat1') { throw 'Incomplete sampled runs' }
foreach ($taskGroup in @('Gate','Trace','Sampled')) {
    foreach ($taskLog in Get-ChildItem -LiteralPath (Join-Path $taskOutput $taskGroup) -Filter '*_Host.log') {
        $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskLog.FullName
        $taskGuest = Get-Content -Raw -Encoding UTF8 -LiteralPath ($taskLog.FullName -replace '_Host\.log$','_Guest.log')
        $taskStride = if ($taskGroup -eq 'Sampled') { 64 } else { 0 }
        foreach ($taskSetting in @("ProfileStride=$taskStride", 'SphereBroadPhase=1', 'SpatialGrid=1', 'RewindCache=0', 'ValidateSpatialGrid=0', 'ValidateRewindCache=0')) {
            if ($taskHost -notmatch "LNP\.Projectile\.$taskSetting(?:\s|,|$)" -or $taskGuest -notmatch "LNP\.Projectile\.$taskSetting(?:\s|,|$)") { throw "Unexpected configuration: $taskLog/$taskSetting" }
        }
        if ($taskStride -eq 0 -and ($taskHost + $taskGuest) -match '\[ProjectileProfile\]') { throw 'Profiling contaminated timing control' }
        if (($taskHost + $taskGuest) -match 'ProjectileGridMismatch|ProjectileRewindCacheMismatch') { throw 'Projectile validation mismatch' }
        # 기존 부하 harness가 켜는 추가 읽기 락 진단도 동일 조건인지 확인한다.
        if ($taskHost -notmatch '\[WorldCollision\] ===== TestMap03 NetMode=2 LockProbe=1 =====') { throw 'Unexpected world lock probe mode' }
    }
}
$taskProfiles = foreach ($taskRun in $taskSampled) {
    $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "Sampled/$($taskRun.Run)_Host.log")
    $taskGuest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "Sampled/$($taskRun.Run)_Guest.log")
    if ($taskRun.PackageSHA256 -ne $taskHash -or $taskRun.HostExit -ne 0 -or $taskRun.GuestExit -ne 0 -or
        -not $taskRun.RingPopulationEligible -or $taskRun.ActivitySamples.Count -ne 30 -or
        [regex]::Matches($taskHost + $taskGuest, '\[Probe[^\]]*\][^\r\n]*-> PASS').Count -ne 8 -or
        ($taskHost + $taskGuest) -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]') { throw 'Invalid sampled workload' }
    $taskWindows = @([regex]::Matches($taskHost, '\[ProjectileProfile\] server=1 [^\r\n]+') |
        ForEach-Object { Read-Fields $_.Value } | Where-Object {
            (Read-Number $_.start) -ge $taskRun.ActivitySamples[0].Time -and (Read-Number $_.end) -le $taskRun.ActivitySamples[-1].Time
        })
    if ($taskWindows.Count -lt 26) { throw 'Incomplete profile windows' }
    $taskFrames = 0.0; $taskProjectiles = 0.0; $taskSamples = 0.0; $taskRemote = 0.0; $taskRewind = 0.0
    $taskStageSums = @{}
    foreach ($taskWindow in $taskWindows) {
        $taskStages = @([regex]::Matches($taskHost, '\[ProjectileProfileStage\] server=1 [^\r\n]+') |
            ForEach-Object { Read-Fields $_.Value } | Where-Object end -eq $taskWindow.end)
        if ([int]$taskWindow.stride -ne 64 -or (Read-Number $taskWindow.frames) -le 0 -or
            (Read-Number $taskWindow.samples) -le 0 -or $taskStages.Count -ne 10 -or
            @($taskStages.stage | Select-Object -Unique).Count -ne 10) { throw 'Invalid profile block' }
        $taskWorld = @($taskStages | Where-Object stage -eq World)[0]
        $taskTargets = @($taskStages | Where-Object stage -eq Targets)[0]
        $taskCapsule = @($taskStages | Where-Object stage -eq Capsule)[0]
        if ($taskWorld.calls -ne $taskWindow.projectiles -or $taskTargets.calls -ne $taskWindow.candidates -or
            (Read-Number $taskCapsule.calls) + (Read-Number $taskWindow.sphereRejected) -ne (Read-Number $taskWindow.sphereTests)) { throw 'Profile call mismatch' }
        foreach ($taskStage in $taskStages) {
            if (-not $taskStageSums.ContainsKey($taskStage.stage)) { $taskStageSums[$taskStage.stage] = @{ Calls=0.0; Ms=0.0 } }
            $taskStageSums[$taskStage.stage].Calls += Read-Number $taskStage.calls
            $taskStageSums[$taskStage.stage].Ms += Read-Number $taskStage.ms
        }
        $taskFrames += Read-Number $taskWindow.frames; $taskProjectiles += Read-Number $taskWindow.projectiles
        $taskSamples += Read-Number $taskWindow.samples; $taskRemote += Read-Number $taskWindow.remote
        $taskRewind += Read-Number $taskWindow.rewindApplied
    }
    if ([math]::Abs($taskSamples * 64 / $taskProjectiles - 1.0) -gt 0.01) { throw 'Unexpected sample coverage' }
    $taskStages = foreach ($taskName in @('CollectEnemies','CollectPlayers','World','Targets','Filter','Rewind','History','Capsule','GridBuild','GridQuery')) {
        $taskStage = $taskStageSums[$taskName]
        $taskScale = if ($taskName -in @('CollectEnemies','CollectPlayers','World','GridBuild')) { 1.0 } else { $taskProjectiles / $taskSamples }
        [pscustomobject]@{
            Stage=$taskName; Calls=$taskStage.Calls; RecordedMs=$taskStage.Ms
            RawNsPerCall=$(if ($taskStage.Calls -gt 0) { 1.e6*$taskStage.Ms/$taskStage.Calls } else { $null })
            InstrumentedMsPerFrameEstimate=$taskStage.Ms*$taskScale/$taskFrames; Sampled=$taskScale -gt 1
        }
    }
    [pscustomobject]@{Run=$taskRun.Run; Windows=$taskWindows.Count; Frames=$taskFrames; Projectiles=$taskProjectiles;
        Samples=$taskSamples; RemoteSamples=$taskRemote; RewindApplied=$taskRewind; Stages=@($taskStages)}
}
[pscustomobject]@{
    PackageSHA256=$taskHash; Profiles=@($taskProfiles); DefaultConfigurationVerified=$true
    SampledTimesIncludeClockAndScopeOverhead=$true; StageTimesAreNested=$true
    WorldCollisionLockProbeEnabled=$true
    RemoteGameplayVerified=$false; Phase13Complete=$false
} | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'ProjectileSummary.json')
foreach ($taskProfile in $taskProfiles) {
    Write-Output $taskProfile.Run
    $taskProfile.Stages | Select-Object Stage,RawNsPerCall,InstrumentedMsPerFrameEstimate | Format-Table -AutoSize
}
