# trace 없는 Gate 로그와 실제 활동 표본·연결 수신량을 요약한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase13/After')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOutput = Join-Path $taskRoot $InputDirectory
$taskCulture = [Globalization.CultureInfo]::InvariantCulture
$taskRows = foreach ($taskMetaFile in Get-ChildItem -LiteralPath $taskOutput -Filter '*_Exit.json') {
    $taskMeta = Get-Content -Raw -Encoding UTF8 -LiteralPath $taskMetaFile.FullName | ConvertFrom-Json
    $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$($taskMeta.Run)_Host.log")
    $taskGuest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$($taskMeta.Run)_Guest.log")
    $taskFrame = [regex]::Match($taskHost, '\[LoadBaseline\] FrameMs P50=([\d.]+) P95=([\d.]+) Max=([\d.]+)')
    $taskExact = [regex]::Match($taskHost, 'ExactPerFrame ms P50=([\d.]+) P95=([\d.]+)')
    $taskNet = [regex]::Match($taskGuest, 'GuestNetwork ReceivedBytes=(\d+) Seconds=([\d.]+) BytesPerSecond=([\d.]+)')
    if (-not $taskFrame.Success -or -not $taskExact.Success -or -not $taskNet.Success) { throw "$($taskMeta.Run) missing summary" }
    $taskSamples = foreach ($taskMatch in [regex]::Matches($taskHost, '\[LoadBaseline\] Activity T=([\d.]+) GroundDormant=(\d+) GroundWander=(\d+) GroundActive=(\d+) FlightDormant=(\d+) FlightWander=(\d+) FlightActive=(\d+) Bound=(\d+) GroundSegments=(\d+) FlightSegments=(\d+)')) {
        [pscustomobject]@{
            Time=[double]::Parse($taskMatch.Groups[1].Value,$taskCulture)
            GroundDormant=[int]$taskMatch.Groups[2].Value; GroundWander=[int]$taskMatch.Groups[3].Value; GroundActive=[int]$taskMatch.Groups[4].Value
            FlightDormant=[int]$taskMatch.Groups[5].Value; FlightWander=[int]$taskMatch.Groups[6].Value; FlightActive=[int]$taskMatch.Groups[7].Value
            Bound=[int]$taskMatch.Groups[8].Value; GroundSegments=[int]$taskMatch.Groups[9].Value; FlightSegments=[int]$taskMatch.Groups[10].Value
        }
    }
    [pscustomobject]@{
        Run=$taskMeta.Run; HostExit=$taskMeta.HostExit; GuestExit=$taskMeta.GuestExit; PackageSHA256=$taskMeta.PackageSHA256
        FrameP50Ms=[double]::Parse($taskFrame.Groups[1].Value,$taskCulture); FrameP95Ms=[double]::Parse($taskFrame.Groups[2].Value,$taskCulture)
        ExactP95Ms=[double]::Parse($taskExact.Groups[2].Value,$taskCulture)
        NavGate=[regex]::Match($taskHost, '\[LoadBaseline\] NavGate[^\r\n]*').Value
        GuestReceivedBytes=[uint64]$taskNet.Groups[1].Value; GuestCaptureSeconds=[double]::Parse($taskNet.Groups[2].Value,$taskCulture)
        GuestBytesPerSecond=[double]::Parse($taskNet.Groups[3].Value,$taskCulture)
        Replication=[regex]::Match($taskHost, '\[LoadBaseline\] Replication[^\r\n]*').Value
        WanderObserver=[regex]::Match($taskHost, '\[LoadBaseline\] WanderObserver[^\r\n]*').Value
        SpawnRows=@([regex]::Matches($taskHost, 'LNPMassSpawnSubsystem: set=[^\r\n]*enemy_requested=\d+ enemy_placed=\d+\.') | ForEach-Object Value)
        CullWanderSamples=@(foreach ($taskMatch in [regex]::Matches($taskHost, '\[LoadBaseline\] CullWander Ground=(\d+) Flight=(\d+) GroundSegments=(\d+) FlightSegments=(\d+)')) {
            [pscustomobject]@{Ground=[int]$taskMatch.Groups[1].Value; Flight=[int]$taskMatch.Groups[2].Value;
                GroundSegments=[int]$taskMatch.Groups[3].Value; FlightSegments=[int]$taskMatch.Groups[4].Value}
        })
        ActivitySamples=@($taskSamples)
    }
}
$taskRows | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Summary.json')
$taskRows | Select-Object Run,FrameP95Ms,ExactP95Ms,GuestBytesPerSecond | Format-Table -AutoSize
