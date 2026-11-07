# 패키지 한 개의 고정 조건 관찰 경로를 실행한다. 짧은 예비 실행은 성능 기준선으로 쓰지 않는다.
param(
    [ValidateSet('Low', 'Medium', 'High')][string]$Profile = 'Low',
    [string]$PackageDirectory = 'Saved/Graphics/Phase00Package',
    [string]$OutputDirectory = 'Saved/Profiling/Graphics/Phase00/Runs',
    [ValidateRange(1, 3)][int]$Repeats = 1,
    [ValidateRange(0, 600)][int]$WarmupSeconds = 60,
    [ValidateRange(1, 600)][int]$DurationSeconds = 60
)

$ErrorActionPreference = 'Stop'
$Profile = [Globalization.CultureInfo]::InvariantCulture.TextInfo.ToTitleCase($Profile.ToLowerInvariant())
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
$taskPackage = if ([IO.Path]::IsPathRooted($PackageDirectory)) { $PackageDirectory } else { Join-Path $taskRoot $PackageDirectory }
$taskOutput = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $taskRoot $OutputDirectory }
$taskExe = Join-Path $taskPackage 'Windows/LootNPop/Binaries/Win64/LootNPop.exe'
$taskPath = Join-Path $taskPackage 'Windows/LootNPop/Scripts/Profiling/Graphics/AcropolisViewpoints.json'
if (-not (Test-Path -LiteralPath $taskExe)) { throw "Package executable missing: $taskExe" }
if (-not (Test-Path -LiteralPath $taskPath)) { throw "Viewpoint artifact missing: $taskPath" }
$taskExeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $taskExe).Hash
$taskPathHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $taskPath).Hash
$taskContentHashes = @(Get-ChildItem -LiteralPath (Join-Path $taskPackage 'Windows') -Recurse -File |
    Where-Object { $_.Extension -in @('.pak', '.utoc', '.ucas') } | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{
            Path = [IO.Path]::GetRelativePath($taskPackage, $_.FullName)
            SHA256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        }
    })
if ($taskContentHashes.Count -eq 0) { throw 'Cooked package containers are missing' }

for ($taskRun = 1; $taskRun -le $Repeats; $taskRun++) {
    $taskName = '{0}_{1}_{2}' -f $Profile, [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss'), [Guid]::NewGuid().ToString('N').Substring(0, 6)
    $taskRunDirectory = Join-Path $taskOutput $taskName
    New-Item -ItemType Directory -Force -Path $taskRunDirectory | Out-Null
    $taskArguments = @(
        '-LNPGraphicsBenchmark', '-LNPGraphicsQuit', '-d3d12', '-unattended',
        "-LNPGraphicsProfile=$Profile", "-LNPGraphicsWarmup=$WarmupSeconds", "-LNPGraphicsDuration=$DurationSeconds",
        ('-LNPGraphicsOutput="{0}"' -f $taskRunDirectory),
        ('-LNPGraphicsPath="{0}"' -f $taskPath),
        ('-abslog="{0}"' -f (Join-Path $taskRunDirectory 'Runtime.log'))
    )
    [pscustomobject]@{
        Profile = $Profile; Run = $taskRun; Arguments = $taskArguments
        Executable = $taskExe; ExecutableSHA256 = $taskExeHash; ViewpointsSHA256 = $taskPathHash
        PackageContentSHA256 = $taskContentHashes
        StartedUtc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskRunDirectory 'Launch.json')

    # 그래픽스 검증 창은 렌더링·구도 확인을 위해 표시한다. 최소화/백그라운드 대조 측정으로 바꾸지 않는다.
    $taskProcess = Start-Process -FilePath $taskExe -ArgumentList $taskArguments -PassThru -WindowStyle Normal
    $taskTimeout = 300 + $WarmupSeconds + $DurationSeconds
    if (-not $taskProcess.WaitForExit($taskTimeout * 1000)) {
        Stop-Process -Id $taskProcess.Id
        throw "Benchmark timed out: $taskRunDirectory"
    }
    $taskProcess.Refresh()
    $taskManifests = @(Get-ChildItem -LiteralPath $taskRunDirectory -Filter 'Run.json' -Recurse -File)
    if ($taskManifests.Count -ne 1) { throw "Expected one run manifest: $taskRunDirectory" }
    $taskManifest = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskManifests[0].FullName | ConvertFrom-Json
    [pscustomobject]@{ ExitCode = $taskProcess.ExitCode; Status = $taskManifest.status; Manifest = $taskManifests[0].FullName } |
        ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskRunDirectory 'Exit.json')
    if ($taskProcess.ExitCode -ne 0 -or $taskManifest.status -ne 'complete') {
        throw "Benchmark failed: $taskRunDirectory; reason=$($taskManifest.reason)"
    }
    Write-Output $taskManifests[0].FullName
}
