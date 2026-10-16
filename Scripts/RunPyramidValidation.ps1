param([switch]$Assemble)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProject = Join-Path $taskRoot 'LootNPop.uproject'
$taskOutput = Join-Path $taskRoot 'Saved/SliceOctant/Pyramid_00/V058'
$taskPool = Join-Path $taskRoot 'Content/Maps/OctantPoolData.uasset'
$taskHash = (Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash
$taskBake = Join-Path $PSScriptRoot 'BakePyramidOctant.py'
$taskBakeLog = Join-Path $taskOutput 'Bake.log'
$taskBakeConsole = Join-Path $taskOutput 'BakeConsole.log'

if ($Assemble) {
    $taskRecovery = Join-Path $taskOutput 'BeforeAssembly'
    New-Item -ItemType Directory -Force -Path $taskRecovery | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot 'Content/Maps/Pyramid_00/LVI_Octant_Pyramid_00.umap'),(Join-Path $taskRoot 'Content/Maps/Pyramid_00/DA_OctantSurface_Pyramid_00.uasset') -Destination $taskRecovery
    $taskAssemblyScript = Join-Path $PSScriptRoot 'AssemblePyramidOctant.py'
    $taskAssemblyLog = Join-Path $taskOutput 'Assembly.log'
    & $taskEditor $taskProject -run=pythonscript "-script=$taskAssemblyScript" -unattended -nosplash -NullRHI -NoSound -NoSourceControl "-abslog=$taskAssemblyLog" *> (Join-Path $taskOutput 'AssemblyConsole.log')
    if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $taskAssemblyLog -Pattern 'PYRAMID_ASSEMBLY_READY' -Quiet)) {
        throw 'Pyramid assembly failed; inspect Assembly.log'
    }
}

$taskPlacement = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'placement.json') | ConvertFrom-Json
if (@($taskPlacement.pods | Where-Object { -not $_.native_candidate }).Count -gt 0) {
    $taskPlacementScript = Join-Path $PSScriptRoot 'PlacePyramidPodsFromBake.py'
    $taskPlacementLog = Join-Path $taskOutput 'NativePlacement.log'
    & $taskEditor $taskProject -run=pythonscript "-script=$taskPlacementScript" -unattended -nosplash -NullRHI -NoSound -NoSourceControl "-abslog=$taskPlacementLog" *> (Join-Path $taskOutput 'NativePlacementConsole.log')
    if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $taskPlacementLog -Pattern 'PYRAMID_NATIVE_PLACEMENT_READY' -Quiet)) {
        throw 'Native placement failed; inspect NativePlacement.log'
    }
}

& $taskEditor $taskProject -run=pythonscript "-script=$taskBake" -unattended -nosplash -NullRHI -NoSound -NoSourceControl "-abslog=$taskBakeLog" *> $taskBakeConsole
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $taskBakeLog -Pattern 'PYRAMID_READY_FOR_VALIDATION' -Quiet)) {
    throw 'Pyramid bake failed; inspect Bake.log'
}

$taskCases = @(
    @{
        Name = 'MixedTests'
        Pool = 'DA_PyramidMixedValidationPool'
        Tests = 'LootNPop.SurfaceNavigation.Runtime.ProductionSurfaceDataDefinitions+LootNPop.SurfaceNavigation.Runtime.ProductionSpawnComposition+LootNPop.DynamicTerrain.MovingPanel.GravityRotation'
    },
    @{
        Name = 'CurrentBakeTests'
        Pool = 'DA_PyramidOnlyValidationPool'
        Tests = 'LootNPop.SurfaceNavigation.Bake.ProductionPoolCurrentAndDeterministic'
    }
)
$taskResults = @()
foreach ($taskCase in $taskCases) {
    $taskReport = Join-Path $taskOutput $taskCase.Name
    $taskLog = Join-Path $taskOutput ($taskCase.Name + '.log')
    $taskConsole = Join-Path $taskOutput ($taskCase.Name + 'Console.log')
    $taskTestPool = '/Game/Maps/_WorkInProgress/Pyramid_00/Validation/' + $taskCase.Pool + '.' + $taskCase.Pool
    & $taskEditor $taskProject '/Game/Maps/Pyramid_00/LVI_Octant_Pyramid_00' -unattended -nosplash -NullRHI -NoSound -NoSourceControl `
        "-ini:Game:[/Script/LootNPop.LNPSettings]:OctantPool=$taskTestPool" `
        "-ExecCmds=Automation RunTests $($taskCase.Tests)" `
        '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$taskReport" "-abslog=$taskLog" *> $taskConsole
    $taskResult = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskReport 'index.json') | ConvertFrom-Json
    $taskResults += @{
        Name = $taskCase.Name
        Succeeded = $taskResult.succeeded
        Failed = $taskResult.failed
        Warnings = $taskResult.succeededWithWarnings
        Tests = @($taskResult.tests | Select-Object fullTestPath, state, errors, warnings)
    }
    Write-Output ($taskResults[-1] | ConvertTo-Json -Depth 8 -Compress)
    if ($taskResult.failed -gt 0 -or $taskResult.succeeded -lt 1) {
        throw ($taskCase.Name + ' failed; inspect exported report')
    }
}
if ((Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash -ne $taskHash) {
    throw 'Production pool changed during validation'
}
$taskResults | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'formal-validation.json')
Write-Output 'PYRAMID_VALIDATION_SUCCESS'
