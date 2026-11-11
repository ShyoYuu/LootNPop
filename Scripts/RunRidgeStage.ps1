param([string]$Version = 'V001', [switch]$SkipAssemble, [switch]$SkipTests)

# Ridge 작업본 반입·조립 → 베이크 → 혼합 정의/편성/Nav 투영 검사. 실제 풀 파일은 변경하지 않는다.
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProject = Join-Path $taskRoot 'LootNPop.uproject'
$taskOutput = Join-Path $taskRoot "Saved/SliceOctant/Ridge_00/$Version"
$taskPool = Join-Path $taskRoot 'Content/Maps/OctantPoolData.uasset'
$taskHash = (Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash

if (-not $SkipAssemble) {
    $taskLog = Join-Path $taskOutput 'Assembly.log'
    & $taskEditor $taskProject -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'AssembleRidgeOctant.py')" -unattended -nosplash -NullRHI -NoSound -NoSourceControl "-abslog=$taskLog" *> (Join-Path $taskOutput 'AssemblyConsole.log')
    if (-not (Select-String -LiteralPath $taskLog -Pattern 'RIDGE_ASSEMBLY_READY' -Quiet)) { throw 'Ridge assembly failed; inspect Assembly.log' }
}

$taskLog = Join-Path $taskOutput 'Bake.log'
& $taskEditor $taskProject -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'BakeRidgeOctant.py')" -unattended -nosplash -NullRHI -NoSound -NoSourceControl "-abslog=$taskLog" *> (Join-Path $taskOutput 'BakeConsole.log')
if (-not (Select-String -LiteralPath $taskLog -Pattern 'RIDGE_BAKE_READY' -Quiet)) { throw 'Ridge bake failed; inspect Bake.log' }

if (-not $SkipTests) {
    $taskReport = Join-Path $taskOutput 'MixedTests'
    $taskTestPool = "/Game/Maps/_WorkInProgress/Ridge_00/$Version/DA_RidgeMixedValidationPool.DA_RidgeMixedValidationPool"
    $taskTests = 'LootNPop.SurfaceNavigation.Runtime.ProductionSurfaceDataDefinitions+LootNPop.SurfaceNavigation.Runtime.ProductionSpawnComposition+LootNPop.SurfaceNavigation.Nav.ProductionSpawnProjection'
    & $taskEditor $taskProject '/Game/Maps/TestMap03' -unattended -nosplash -NullRHI -NoSound -NoSourceControl `
        "-ini:Game:[/Script/LootNPop.LNPSettings]:OctantPool=$taskTestPool" `
        "-ExecCmds=Automation RunTests $taskTests" '-TestExit=Automation Test Queue Empty' `
        "-ReportExportPath=$taskReport" "-abslog=$(Join-Path $taskOutput 'MixedTests.log')" *> (Join-Path $taskOutput 'MixedTestsConsole.log')
    $taskResult = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskReport 'index.json') | ConvertFrom-Json
    $taskSummary = @{ Succeeded = $taskResult.succeeded; Warnings = $taskResult.succeededWithWarnings; Failed = $taskResult.failed
        Tests = @($taskResult.tests | Select-Object fullTestPath, state, errors, warnings) }
    $taskSummary | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'mixed-test-summary.json')
    Write-Output ($taskSummary | ConvertTo-Json -Depth 8 -Compress)
    if ($taskResult.failed -gt 0 -or ($taskResult.succeeded + $taskResult.succeededWithWarnings) -lt 3) { throw 'Mixed validation failed; inspect MixedTests report' }
}
if ((Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash -ne $taskHash) { throw 'Production pool changed during Ridge stage' }
Write-Output 'RIDGE_STAGE_SUCCESS'
