# processor 호출당 시간과 전체 exact CPU를 분리하고, 병렬 부모의 대기 시간을 합산하지 않는다.
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOutput = Join-Path $taskRoot 'Saved/Profiling/Phase13/Before'
$taskRuns = foreach ($taskName in @('N700','F100','F300')) {
    $taskMeta = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "${taskName}_Exit.json") | ConvertFrom-Json
    $taskExport = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "${taskName}_Insights.log")
    $taskRegion = [regex]::Match($taskExport, "region 'LNPLoadBaselineCapture' \[([\d.]+) .. ([\d.]+)\]")
    if (-not $taskRegion.Success) { throw "$taskName capture region missing" }
    $taskCulture = [Globalization.CultureInfo]::InvariantCulture
    $taskSeconds = [double]::Parse($taskRegion.Groups[2].Value,$taskCulture) - [double]::Parse($taskRegion.Groups[1].Value,$taskCulture)
    if ($taskSeconds -lt 29 -or $taskSeconds -gt 31) { throw "$taskName capture duration invalid" }
    $taskStats = Import-Csv -LiteralPath (Join-Path $taskOutput "${taskName}_AllStats_LNPLoadBaselineCapture.csv")
    $taskProcessors = foreach ($taskRow in $taskStats) {
        if ($taskRow.Name -notmatch '^(LNPEnemy.*Processor|LNPPositionHistoryRecordProcessor|LNPPoiseProcessor|MassStateTreeProcessor|MassReplicationProcessor|LNPEntityAttackProcessor)_') { continue }
        $taskIncl = [double]::Parse($taskRow.Incl,$taskCulture)
        $taskExcl = [double]::Parse($taskRow.Excl,$taskCulture)
        $taskCount = [int]$taskRow.Count
        if ($taskCount -le 0 -or $taskExcl -gt $taskIncl + 0.000001) { throw "$taskName invalid timer statistics" }
        [pscustomobject]@{
            Name=($taskRow.Name -replace '_\d+$',''); Count=$taskCount
            InclusiveMsPerInvocation=1000*$taskIncl/$taskCount
            ExclusiveMsPerInvocation=1000*$taskExcl/$taskCount
            InclusiveCpuMsPerSecond=1000*$taskIncl/$taskSeconds
        }
    }
    $taskExact = foreach ($taskRow in $taskStats) {
        if ($taskRow.Name -notmatch '^LNPWorldCollision_(SweepCapsule|SweepSphere|Raycast|ProbeSupport)$') { continue }
        [pscustomobject]@{Name=$taskRow.Name; Count=[int]$taskRow.Count; CpuMsPerSecond=1000*[double]::Parse($taskRow.Excl,$taskCulture)/$taskSeconds}
    }
    $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "${taskName}_Host.log")
    $taskFrame = [regex]::Match($taskHost,'\[LoadBaseline\] FrameMs P50=([\d.]+) P95=([\d.]+) Max=([\d.]+)')
    if (-not $taskFrame.Success) { throw "$taskName frame summary missing" }
    [pscustomobject]@{
        Name=$taskName; Ground=$taskMeta.Ground; Flyers=$taskMeta.Flyers; CaptureSeconds=$taskSeconds
        FrameP50Ms=[double]::Parse($taskFrame.Groups[1].Value,$taskCulture)
        FrameP95Ms=[double]::Parse($taskFrame.Groups[2].Value,$taskCulture)
        Processors=@($taskProcessors); ExactAllConsumers=@($taskExact)
    }
}
$taskRuns | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Summary.json')
$taskRuns | Select-Object Name,Ground,Flyers,CaptureSeconds,FrameP50Ms,FrameP95Ms | Format-Table -AutoSize
foreach ($taskRun in $taskRuns) {
    Write-Output $taskRun.Name
    $taskRun.Processors | Sort-Object InclusiveMsPerInvocation -Descending | Select-Object -First 8 Name,InclusiveMsPerInvocation,ExclusiveMsPerInvocation | Format-Table -AutoSize
}
