# 명시된 패키지 root와 전이 의존성만 cook한다. UE 5.8의 CookMaps 인자는 일반 패키지도 AddFlexPathToCook으로 처리한다.
param([string]$ArchiveDirectory = 'Saved/Graphics/Phase00Package')

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
$taskArchive = if ([IO.Path]::IsPathRooted($ArchiveDirectory)) { $ArchiveDirectory } else { Join-Path $taskRoot $ArchiveDirectory }
$taskProfiles = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot 'SpawnProfiles.json') | ConvertFrom-Json
$taskCookRoots = $taskProfiles.cook_roots -join '+'
$taskArguments = @(
    'BuildCookRun', ('-project={0}' -f (Join-Path $taskRoot 'LootNPop.uproject')),
    '-noP4', '-platform=Win64', '-clientconfig=Development', '-build', '-cook', '-stage',
    '-pak', '-iostore', '-package', '-archive', ('-archivedirectory={0}' -f $taskArchive),
    ('-map={0}' -f $taskCookRoots), '-unattended', '-utf8output', '-nocompileeditor'
)
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' @taskArguments
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed: $LASTEXITCODE" }
$taskStagedPath = Join-Path $taskArchive 'Windows/LootNPop/Scripts/Profiling/Graphics/AcropolisViewpoints.json'
if (-not (Test-Path -LiteralPath $taskStagedPath)) { throw "Viewpoints NonUFS dependency was not staged: $taskStagedPath" }
Write-Output "Graphics benchmark package ready: $taskArchive"
