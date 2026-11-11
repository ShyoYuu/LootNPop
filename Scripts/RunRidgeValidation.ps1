$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskOutput = Join-Path $taskRoot 'Saved/SliceOctant/Ridge_00/Production'
$taskPool = Join-Path $taskRoot 'Content/Maps/OctantPoolData.uasset'
$taskHash = (Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash
$taskFixtures = @('Content/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt.uasset', 'Content/SurfaceNavigationTests/MeshTerrain/SM_BOptionExtracted.uasset', 'Content/SurfaceNavigationTests/Schema/DA_MinimalOctantSurfaceData.uasset')
foreach ($taskFixture in $taskFixtures) {
    if (git -C $taskRoot status --porcelain -- $taskFixture) { throw ('Fixture already modified: ' + $taskFixture) }
}
$taskCaseName = 'SurfaceNavigationTests'
$taskTests = 'LootNPop.SurfaceNavigation'
$taskTestPool = '/Game/Maps/OctantPoolData.OctantPoolData'
$taskReport = Join-Path $taskOutput $taskCaseName
if (Test-Path -LiteralPath (Join-Path $taskReport 'index.json')) { throw 'Full validation report already exists; preserve it before another run' }
try {
    & $taskEditor (Join-Path $taskRoot 'LootNPop.uproject') '/Game/Maps/TestMap03' -unattended -nosplash -NullRHI -NoSound -NoSourceControl `
        "-ini:Game:[/Script/LootNPop.LNPSettings]:OctantPool=$taskTestPool" `
        "-ExecCmds=Automation RunTests $taskTests" '-TestExit=Automation Test Queue Empty' `
        "-ReportExportPath=$taskReport" "-abslog=$taskOutput/$taskCaseName.log" *> "$taskOutput/$($taskCaseName)Console.log"
    $taskExit = $LASTEXITCODE
} finally {
    # 전체 검사에서 재저장하는 세 시험편만 시작 시 깨끗했던 Git 원본으로 복원한다.
    git -C $taskRoot restore --worktree -- $taskFixtures
}
$taskResult = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskReport 'index.json') | ConvertFrom-Json
$taskSummary = @{
    ExitCode = $taskExit
    Succeeded = $taskResult.succeeded
    SucceededWithWarnings = $taskResult.succeededWithWarnings
    Failed = $taskResult.failed
    NotRun = $taskResult.notRun
    Tests = @($taskResult.tests | Select-Object fullTestPath, state, errors, warnings)
}
$taskSummaryFile = 'formal-validation.json'
$taskSummary | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput $taskSummaryFile)
if ((Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash -ne $taskHash) { throw 'Production pool changed during validation' }
Write-Output ($taskSummary | Select-Object ExitCode, Succeeded, SucceededWithWarnings, Failed, NotRun | ConvertTo-Json -Compress)
if ($taskExit -ne 0 -or $taskResult.failed -gt 0 -or ($taskResult.succeeded + $taskResult.succeededWithWarnings) -lt 1) { throw 'SurfaceNavigation validation failed; inspect native report' }
if (-not ($taskResult.tests | Where-Object fullTestPath -eq 'LootNPop.SurfaceNavigation.Nav.ProductionSpawnProjection')) { throw 'ProductionSpawnProjection was not run' }
Write-Output 'RIDGE_VALIDATION_SUCCESS'
