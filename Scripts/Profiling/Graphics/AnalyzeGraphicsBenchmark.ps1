# 원시 CSV의 CaptureFrame=1 행만 집계한다. 예비 실행은 명시적으로 허용해야 읽으며 성능 결과와 구분한다.
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [switch]$AllowPreview
)

$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath $RunDirectory).Path
$taskManifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskRoot 'Run.json') | ConvertFrom-Json
if ($taskManifest.status -ne 'complete') { throw "Run is incomplete: $($taskManifest.status); $($taskManifest.reason)" }
if (-not $taskManifest.performance_eligible -and -not $AllowPreview) { throw 'Preview run cannot be reported as a performance baseline' }
# 엔진의 다른 카테고리에 중복 열 이름이 있어도 측정 열을 잃지 않도록 이름 대신 인덱스로 읽는다.
Add-Type -AssemblyName Microsoft.VisualBasic.Core
$taskParser = [Microsoft.VisualBasic.FileIO.TextFieldParser]::new($taskManifest.frames_csv)
$taskParser.SetDelimiters(',')
$taskParser.HasFieldsEnclosedInQuotes = $true
$taskHeader = @($taskParser.ReadFields())
$taskColumns = @{}
$taskIndices = @{}
foreach ($taskStat in @('CaptureFrame','FrameMs','GTMs','RTMs','GPUMs','DrawCalls','PathSeconds','Alive','Active','FrustumCandidates')) {
    $taskMatches = @($taskHeader | Where-Object { $_ -match ('(^|/)LNPGraphics_' + [regex]::Escape($taskStat) + '$') })
    if ($taskMatches.Count -ne 1) { throw "Expected one column for $taskStat; found $($taskMatches.Count)" }
    $taskColumns[$taskStat] = $taskMatches[0]
    $taskIndices[$taskStat] = [Array]::IndexOf($taskHeader, $taskMatches[0])
}
$taskRows = [Collections.Generic.List[object]]::new()
try {
    while (-not $taskParser.EndOfData) {
        $taskFields = $taskParser.ReadFields()
        if ($taskFields.Count -gt $taskIndices.CaptureFrame -and $taskFields[$taskIndices.CaptureFrame] -match '^1(\.0+)?$') {
            $taskRows.Add($taskFields)
        }
    }
} finally { $taskParser.Dispose() }
if ($taskRows.Count -ne $taskManifest.measured_frames -or $taskRows.Count -lt 2) {
    throw "Capture row mismatch: csv=$($taskRows.Count), manifest=$($taskManifest.measured_frames)"
}
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
$taskAverages = [ordered]@{}
foreach ($taskStat in @('FrameMs','GTMs','RTMs','GPUMs','DrawCalls')) {
    $taskValues = @($taskRows | ForEach-Object { [double]::Parse($_[$taskIndices[$taskStat]], $taskCulture) })
    if (@($taskValues | Where-Object { $_ -le 0 -or [double]::IsNaN($_) -or [double]::IsInfinity($_) }).Count -gt 0) {
        throw "Missing or invalid $taskStat samples; zero values must not be averaged as real timings"
    }
    $taskAverages[$taskStat] = ($taskValues | Measure-Object -Average).Average
    if ($taskStat -eq 'FrameMs') {
        $taskSorted = @($taskValues | Sort-Object)
        $taskP95 = $taskSorted[[Math]::Ceiling(0.95 * $taskSorted.Count) - 1]
    }
}
$taskPopulation = @(Import-Csv -LiteralPath (Join-Path $taskRoot 'Population.csv'))
if ($taskPopulation.Count -eq 0) { throw 'Population samples are missing' }
foreach ($taskRow in $taskPopulation) {
    if ([int]$taskRow.Alive -ne ([int]$taskRow.Dormant + [int]$taskRow.Wander + [int]$taskRow.Active) -or
        [int]$taskRow.Alive -ne ([int]$taskRow.Actor + [int]$taskRow.Instanced + [int]$taskRow.None) -or
        [int]$taskRow.FrustumCandidates -gt [int]$taskRow.Alive) { throw 'Population partition mismatch' }
}
$taskEnemyRows = @($taskManifest.initial_spawns | Where-Object enemy)
$taskInitial = [int](($taskEnemyRows | Measure-Object initialized -Sum).Sum)
if ($taskInitial -ne $taskManifest.initial_enemy_total) { throw 'Initial enemy breakdown does not match total' }
$taskSummary = [ordered]@{
    Profile=$taskManifest.profile; PerformanceEligible=[bool]$taskManifest.performance_eligible
    FrameCount=$taskRows.Count; Averages=$taskAverages; FrameP95Ms=$taskP95
    FrameBudgetRatio=($taskAverages.FrameMs / 16.6); InitialEnemies=$taskInitial
    InitialByType=@($taskEnemyRows | Group-Object entity_config | ForEach-Object {
        [pscustomobject]@{EntityConfig=$_.Name; Initialized=[int](($_.Group | Measure-Object initialized -Sum).Sum)}
    })
    PopulationSamples=$taskPopulation.Count; FrustumCandidatesIncludeOccluded=$true
    Manifest=(Join-Path $taskRoot 'Run.json'); ColumnMapping=$taskColumns
}
$taskSummary | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskRoot 'Summary.json')
$taskSummary | ConvertTo-Json -Depth 8
