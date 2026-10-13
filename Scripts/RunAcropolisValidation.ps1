param()

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskEditor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$taskProject = Join-Path $taskRoot 'LootNPop.uproject'
$taskMap = '/Game/Maps/Acropolis_00/LVI_Octant_Acropolis_00'
$taskOutput = Join-Path $taskRoot 'Saved/SliceOctant/Acropolis_00/V016'
$taskPool = Join-Path $taskRoot 'Content/Maps/OctantPoolData.uasset'
$taskHash = (Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash
$taskCases = @(
    @{
        Name = 'MixedTests'
        Pool = 'DA_AcropolisMixedValidationPool'
        Tests = 'LootNPop.SurfaceNavigation.Runtime.ProductionSurfaceDataDefinitions+LootNPop.SurfaceNavigation.Runtime.ProductionSpawnComposition'
    },
    @{
        Name = 'CurrentBakeTests'
        Pool = 'DA_AcropolisOnlyValidationPool'
        Tests = 'LootNPop.SurfaceNavigation.Bake.ProductionPoolCurrentAndDeterministic'
    }
)
$taskResults = @()
foreach ($taskCase in $taskCases) {
    $taskReport = Join-Path $taskOutput $taskCase.Name
    $taskLog = Join-Path $taskOutput ($taskCase.Name + '.log')
    $taskConsole = Join-Path $taskOutput ($taskCase.Name + 'Console.log')
    $taskTestPool = '/Game/Maps/_WorkInProgress/Acropolis_00/Validation/' + $taskCase.Pool + '.' + $taskCase.Pool
    Write-Output ('Starting ' + $taskCase.Name)
    # -ini 값은 이 프로세스의 설정 오버라이드다. 실제 설정 파일과 production 풀은 저장하지 않는다.
    & $taskEditor $taskProject $taskMap -unattended -nosplash -NullRHI -NoSound -NoSourceControl `
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
        throw ($taskCase.Name + ' failed; inspect exported native report')
    }
}
if ((Get-FileHash -LiteralPath $taskPool -Algorithm SHA256).Hash -ne $taskHash) {
    throw 'Production pool changed during validation'
}
$taskResults | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'formal-validation.json')
Write-Output 'ACROPOLIS_VALIDATION_SUCCESS'
