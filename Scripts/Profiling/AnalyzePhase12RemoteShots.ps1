param(
    [string]$InputLog = 'Saved/Logs/LootNPop.log',
    [string]$OutputDirectory = 'Saved/Profiling/Phase12/RemoteShots'
)
$ErrorActionPreference = 'Stop'
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$taskArchive = Join-Path $OutputDirectory 'Editor.log'
if ([IO.Path]::GetFullPath($InputLog) -ne [IO.Path]::GetFullPath($taskArchive)) {
    # 실행 중인 에디터 로그는 공유 읽기로 복사하고 고정된 사본을 분석한다.
    $taskSourceStream = [IO.File]::Open([IO.Path]::GetFullPath($InputLog), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $taskArchiveStream = [IO.File]::Create([IO.Path]::GetFullPath($taskArchive))
        try { $taskSourceStream.CopyTo($taskArchiveStream) } finally { $taskArchiveStream.Dispose() }
    } finally { $taskSourceStream.Dispose() }
}
$taskText = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskArchive
# 반복 PIE의 월드 시각은 초기화되므로 마지막 리슨 세션만 분석한다. 사본은 전체 로그를 유지한다.
$taskSessions = [regex]::Matches($taskText, 'LogWorldPartition: UWorldPartition::Initialize Context : World NetMode = Listen Server')
if ($taskSessions.Count) { $taskText = $taskText.Substring($taskSessions[-1].Index) }
function Read-Fields([string]$Line) {
    $taskFields = @{}
    foreach ($taskMatch in [regex]::Matches($Line, '(\w+)=([^\s]+)')) {
        $taskFields[$taskMatch.Groups[1].Value] = $taskMatch.Groups[2].Value
    }
    return $taskFields
}
function Read-Number([string]$Value) { return [double]::Parse($Value, $taskCulture) }
$taskWindows = @([regex]::Matches($taskText, '\[ProjectileProfile\] server=1 [^\r\n]+') |
    ForEach-Object { Read-Fields $_.Value })
if (-not $taskWindows.Count) { throw 'No server projectile profile windows' }
$taskStages = @{}
$taskCacheWindows = @{}
foreach ($taskMatch in [regex]::Matches($taskText, '\[ProjectileRewindCacheProfile\] server=1 [^\r\n]+')) {
    $taskCacheWindow = Read-Fields $taskMatch.Value
    if ($taskCacheWindows.ContainsKey($taskCacheWindow.end)) { throw 'Duplicate cache window key' }
    $taskCacheWindows[$taskCacheWindow.end] = $taskCacheWindow
}
foreach ($taskMatch in [regex]::Matches($taskText, '\[ProjectileProfileStage\] server=1 [^\r\n]+')) {
    $taskStage = Read-Fields $taskMatch.Value
    if (-not $taskStages.ContainsKey($taskStage.end)) { $taskStages[$taskStage.end] = @{} }
    if ($taskStages[$taskStage.end].ContainsKey($taskStage.stage)) { throw 'Duplicate stage key; select one PIE session' }
    $taskStages[$taskStage.end][$taskStage.stage] = $taskStage
}
foreach ($taskWindow in $taskWindows) {
    $taskBlock = $taskStages[$taskWindow.end]
    if ($taskBlock.Count -ne 10 -or [int]$taskWindow.stride -ne 64 -or [int]$taskWindow.frames -le 0) { throw 'Invalid profile block' }
    if ($taskBlock.Targets.calls -ne $taskWindow.candidates -or $taskBlock.World.calls -ne $taskWindow.projectiles -or
        $taskBlock.History.calls -ne $taskWindow.rewindApplied -or
        (Read-Number $taskWindow.sphereTests) -ne (Read-Number $taskWindow.sphereRejected) + (Read-Number $taskBlock.Capsule.calls)) {
        throw 'Profile count invariant failed'
    }
    if ($taskCacheWindows.Count) {
        $taskCacheWindow = $taskCacheWindows[$taskWindow.end]
        if (-not $taskCacheWindow -or (Read-Number $taskCacheWindow.hits) -gt (Read-Number $taskCacheWindow.requests) -or
            (Read-Number $taskCacheWindow.validationErrors) -gt 0) { throw 'Cache count or equivalence validation failed' }
    }
}
function Summarize-Windows($Windows) {
    $taskTotals = [ordered]@{}
    foreach ($taskField in @('frames','projectiles','samples','candidates','remote','rewindApplied','sphereTests','sphereRejected')) {
        $taskTotals[$taskField] = ($Windows | ForEach-Object { Read-Number $_[$taskField] } | Measure-Object -Sum).Sum
    }
    $taskDuration = ($Windows | ForEach-Object { (Read-Number $_.end) - (Read-Number $_.start) } | Measure-Object -Sum).Sum
    $taskCacheTotals = [ordered]@{}
    foreach ($taskField in @('requests','hits','validationQueries','validationErrors')) {
        $taskCacheTotals[$taskField] = ($Windows | ForEach-Object {
            if ($taskCacheWindows.ContainsKey($_.end)) { Read-Number $taskCacheWindows[$_.end][$taskField] } else { 0 }
        } | Measure-Object -Sum).Sum
    }
    $taskRows = foreach ($taskName in @('CollectEnemies','CollectPlayers','World','Targets','Filter','Rewind','History','Capsule','GridBuild','GridQuery')) {
        $taskCalls = 0.0; $taskMs = 0.0
        foreach ($taskWindow in $Windows) {
            $taskStage = $taskStages[$taskWindow.end][$taskName]
            $taskCalls += Read-Number $taskStage.calls
            $taskMs += Read-Number $taskStage.ms
        }
        [pscustomobject]@{
            Stage = $taskName; Calls = $taskCalls; RecordedMs = $taskMs
            RawNsPerCall = $(if ($taskCalls -gt 0) { 1.e6 * $taskMs / $taskCalls } else { $null })
        }
    }
    [pscustomobject]@{ Windows = $Windows.Count; RecordedWindowSeconds = $taskDuration; Totals = $taskTotals; CacheTotals = $taskCacheTotals; Stages = @($taskRows) }
}
$taskRemoteWindows = @($taskWindows | Where-Object { [int]$_.remote -gt 0 })
if (-not $taskRemoteWindows.Count) { throw 'No remote projectile samples' }
$taskErrors = @([regex]::Matches($taskText, '[^\r\n]*(?:Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|\[ProjectileGridMismatch\]|\[ProjectileRewindCacheMismatch\])[^\r\n]*') | ForEach-Object Value)
$taskSummary = [pscustomobject]@{
    InputSHA256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $taskArchive).Hash
    Map = '/Game/Maps/TestMap03'; Stride = 64
    AllServerWindows = Summarize-Windows $taskWindows
    RemotePositiveWindows = Summarize-Windows $taskRemoteWindows
    FirstRemoteWindowStart = Read-Number $taskRemoteWindows[0].start
    LastRemoteWindowEnd = Read-Number $taskRemoteWindows[-1].end
    JoinedClient = $taskText.Contains('LogNet: Join succeeded:')
    PIEStopped = $taskText.Contains('Shutting down PIE online subsystems')
    RuntimeErrors = $taskErrors
    # 표본은 고유 발사 수가 아니라 투사체 프레임이며, 각 창은 호스트/NPC 사격도 포함한다.
    RemoteSamplesAreProjectileFrameSamples = $true
    StageTimesAreNestedAndIncludeInstrumentation = $true
    RemotePositiveWindowsContainOtherProjectileWork = $true
    FrameGateMeasured = $false
    CacheCountersCoverAllQueries = $true
}
$taskSummary | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $OutputDirectory 'Summary.json')
$taskSummary | ConvertTo-Json -Depth 8
