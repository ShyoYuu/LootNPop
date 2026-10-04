# 동일 패키지·실제 부하·샘플 호출 불변식을 검증하고 대조 결과를 보존한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase12/Sphere', [switch]$SpatialGrid)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOutput = Join-Path $taskRoot $InputDirectory
$taskHash = $null
function Read-Fields([string]$Line) {
    $taskFields = @{}
    foreach ($taskMatch in [regex]::Matches($Line, '(\w+)=([^\s]+)')) {
        $taskFields[$taskMatch.Groups[1].Value] = $taskMatch.Groups[2].Value
    }
    return $taskFields
}
$taskGroups = @('Off','On','TraceOff','TraceOn','SampledOn')
if ($SpatialGrid) { $taskGroups += 'Validation' }
$taskRuns = foreach ($taskGroup in $taskGroups) {
    & (Join-Path $PSScriptRoot 'AnalyzePhase13Gate.ps1') -InputDirectory "$InputDirectory/$taskGroup" | Out-Host
    $taskRows = @(Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/Summary.json") | ConvertFrom-Json)
    if ($taskRows.Count -ne $(if ($taskGroup -in @('Off','On')) { 4 } elseif ($taskGroup -eq 'Validation') { 1 } else { 2 })) { throw 'Incomplete run group' }
    foreach ($taskRow in $taskRows) {
        $taskHost = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/$($taskRow.Run)_Host.log")
        $taskGuest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $taskOutput "$taskGroup/$($taskRow.Run)_Guest.log")
        if (-not $taskHash) { $taskHash = $taskRow.PackageSHA256 }
        if ($taskRow.PackageSHA256 -ne $taskHash -or $taskRow.HostExit -ne 0 -or $taskRow.GuestExit -ne 0 -or
            -not $taskRow.RingPopulationEligible -or $taskRow.ActivitySamples.Count -ne 30 -or
            [regex]::Matches($taskHost + $taskGuest, '\[Probe[^\]]*\][^\r\n]*-> PASS').Count -ne 8 -or
            ($taskHost + $taskGuest) -match 'Assertion failed|Ensure condition failed|Fatal error|Unhandled Exception|UnknownHits=[1-9]|EnvelopeEscapes=[1-9]|LayerJumps=[1-9]|violations=[1-9]|ProjectileGridMismatch') { throw 'Runtime or population validation failed' }
        $taskMode = [int]($taskGroup -notin @('Off','TraceOff'))
        $taskStride = if ($taskGroup -in @('SampledOn','Validation')) { 64 } else { 0 }
        $taskSphereMode = if ($SpatialGrid) { 1 } else { $taskMode }
        if ($taskHost -notmatch "LNP.Projectile.SphereBroadPhase=$taskSphereMode(?:\s|,|$)" -or
            $taskHost -notmatch "LNP.Projectile.ProfileStride=$taskStride(?:\s|,|$)") { throw 'Unexpected configuration' }
        if ($SpatialGrid -and ($taskHost -notmatch "LNP.Projectile.SpatialGrid=$taskMode(?:\s|,|$)" -or
            ($taskGroup -eq 'Validation' -and $taskHost -notmatch 'LNP.Projectile.ValidateSpatialGrid=1(?:\s|,|$)') -or
            ($taskGroup -ne 'Validation' -and $taskHost -match 'LNP.Projectile.ValidateSpatialGrid=1'))) { throw 'Unexpected grid configuration' }
        $taskRequests = @(Import-Csv -LiteralPath (Join-Path $taskOutput "$taskGroup/$($taskRow.Run)_NavRequests.csv"))
        $taskTimings = @(Import-Csv -LiteralPath (Join-Path $taskOutput "$taskGroup/$($taskRow.Run)_NavTimings.csv"))
        $taskSubmitted = [regex]::Match($taskHost, '\[LoadBaseline\] Nav chase=[^\r\n]*submitted=(\d+)')
        if (-not $taskSubmitted.Success -or $taskRequests.Count -ne [int]$taskSubmitted.Groups[1].Value -or
            $taskRequests.Count -ne $taskTimings.Count) { throw 'Nav capture row mismatch' }
        $taskKeys = [Collections.Generic.HashSet[string]]::new()
        $taskLastTime = -1.0
        foreach ($taskRequest in $taskRequests) {
            if ([double]$taskRequest.time -lt $taskLastTime -or
                -not $taskKeys.Add("$($taskRequest.owner)/$($taskRequest.ownerSerial)/$($taskRequest.requestSerial)")) { throw 'Duplicate or reversed Nav request' }
            $taskLastTime = [double]$taskRequest.time
        }
        foreach ($taskTiming in $taskTimings) {
            if (-not $taskKeys.Remove("$($taskTiming.owner)/$($taskTiming.ownerSerial)/$($taskTiming.requestSerial)")) { throw 'Unmatched Nav timing' }
        }
        $taskInclusive = $null
        $taskSphere = 0.0; $taskRejected = 0.0; $taskCapsule = 0.0; $taskCandidates = 0.0; $taskWindows = 0
        $taskRemote = 0.0; $taskRewind = 0.0
        $taskGridQueries = 0.0; $taskGridFallbacks = 0.0; $taskGridCandidates = 0.0; $taskFullCandidates = 0.0
        $taskValidationQueries = 0.0; $taskValidationHits = 0.0
        $taskGridBuildMs = 0.0; $taskGridQueryMs = 0.0; $taskProfileFrames = 0.0; $taskSamples = 0.0
        if ($taskGroup -in @('TraceOff','TraceOn','SampledOn')) {
            $taskTimers = @(Import-Csv -LiteralPath (Join-Path $taskOutput "$taskGroup/$($taskRow.Run)_LNPLoadBaselineCapture.csv") |
                Where-Object Name -match '^LNPProjectileHitDetectionProcessor(?:_\d+)?$')
            if (-not $taskTimers.Count) { throw 'Missing projectile timer' }
            $taskInclusive = 1000 * ($taskTimers | Measure-Object Incl -Sum).Sum / ($taskTimers | Measure-Object Count -Sum).Sum
        }
        if ($taskStride -eq 0) {
            if ($taskHost -match '\[ProjectileProfile\]') { throw 'Profiling enabled in timing control' }
        } else {
            $taskFirst = $taskRow.ActivitySamples[0].Time; $taskLast = $taskRow.ActivitySamples[-1].Time
            foreach ($taskWindow in @([regex]::Matches($taskHost, '\[ProjectileProfile\] server=1 [^\r\n]+') |
                ForEach-Object { Read-Fields $_.Value } | Where-Object { [double]$_.start -ge $taskFirst -and [double]$_.end -le $taskLast })) {
                $taskStages = @([regex]::Matches($taskHost, '\[ProjectileProfileStage\] server=1 [^\r\n]+') |
                    ForEach-Object { Read-Fields $_.Value } | Where-Object end -eq $taskWindow.end)
                if ($taskStages.Count -ne $(if ($SpatialGrid) { 10 } else { 8 })) { throw 'Incomplete profile stages' }
                $taskCaps = @($taskStages | Where-Object stage -eq Capsule)[0]
                if ([double]$taskCaps.calls + [double]$taskWindow.sphereRejected -ne [double]$taskWindow.sphereTests) { throw 'Sphere call count mismatch' }
                $taskSphere += [double]$taskWindow.sphereTests; $taskRejected += [double]$taskWindow.sphereRejected
                $taskCapsule += [double]$taskCaps.calls; $taskCandidates += [double]$taskWindow.candidates
                $taskRemote += [double]$taskWindow.remote; $taskRewind += [double]$taskWindow.rewindApplied
                if ($SpatialGrid) {
                    $taskGrid = @([regex]::Matches($taskHost, '\[ProjectileGridProfile\] server=1 [^\r\n]+') |
                        ForEach-Object { Read-Fields $_.Value } | Where-Object end -eq $taskWindow.end)
                    if ($taskGrid.Count -ne 1 -or [double]$taskGrid[0].validationErrors -ne 0 -or
                        [double]$taskGrid[0].queries -ne [double]$taskWindow.samples -or
                        [double]$taskGrid[0].fallbacks -gt [double]$taskGrid[0].queries -or
                        [double]$taskGrid[0].candidates -gt [double]$taskGrid[0].fullCandidates) { throw 'Invalid grid profile' }
                    $taskGridQueries += [double]$taskGrid[0].queries; $taskGridFallbacks += [double]$taskGrid[0].fallbacks
                    $taskGridCandidates += [double]$taskGrid[0].candidates; $taskFullCandidates += [double]$taskGrid[0].fullCandidates
                    $taskValidationQueries += [double]$taskGrid[0].validationQueries; $taskValidationHits += [double]$taskGrid[0].validationHits
                    $taskGridBuildMs += [double](@($taskStages | Where-Object stage -eq GridBuild)[0].ms)
                    $taskGridQueryMs += [double](@($taskStages | Where-Object stage -eq GridQuery)[0].ms)
                    $taskProfileFrames += [double]$taskWindow.frames; $taskSamples += [double]$taskWindow.samples
                }
                ++$taskWindows
            }
            if ($taskWindows -lt 26 -or $taskSphere -le 0) { throw 'Incomplete profile capture' }
            if ($taskGroup -eq 'Validation' -and ($taskValidationQueries -le 0 -or $taskValidationHits -le 0)) { throw 'No positive runtime validation coverage' }
            if ($SpatialGrid -and $taskGroup -ne 'Validation' -and $taskValidationQueries -ne 0) { throw 'Validation contaminated timing' }
        }
        [pscustomobject]@{
            Group=$taskGroup; Run=$taskRow.Run; FrameP95=$taskRow.FrameP95Ms
            FrameGate=[regex]::Match($taskHost, 'NavGate frame=(PASS|FAIL)').Groups[1].Value
            PathGate=[regex]::Match($taskHost, 'NavGate frame=[^\s]+ path=(PASS|FAIL)').Groups[1].Value
            ProjectileInclusiveMs=$taskInclusive; ProfileWindows=$taskWindows; Candidates=$taskCandidates
            SphereTests=$taskSphere; SphereRejected=$taskRejected; CapsuleCalls=$taskCapsule
            RejectedFraction=$(if ($taskSphere -gt 0) { $taskRejected/$taskSphere } else { $null })
            RemoteSamples=$taskRemote; RewindApplied=$taskRewind
            GridQueries=$taskGridQueries; GridFallbacks=$taskGridFallbacks; GridCandidates=$taskGridCandidates
            GridFullCandidates=$taskFullCandidates; ValidationQueries=$taskValidationQueries; ValidationHits=$taskValidationHits
            GridCandidatesPerSample=$(if ($taskGridQueries -gt 0) { $taskGridCandidates/$taskGridQueries } else { $null })
            GridCandidateReduction=$(if ($taskFullCandidates -gt 0) { 1-$taskGridCandidates/$taskFullCandidates } else { $null })
            GridBuildMsPerFrame=$(if ($taskProfileFrames -gt 0) { $taskGridBuildMs/$taskProfileFrames } else { $null })
            GridQueryRawNsPerSample=$(if ($taskSamples -gt 0) { 1.e6*$taskGridQueryMs/$taskSamples } else { $null })
        }
    }
}
[pscustomobject]@{PackageSHA256=$taskHash; Runs=@($taskRuns)} | ConvertTo-Json -Depth 6 |
    Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskOutput 'Summary.json')
$taskRuns | Select-Object Group,Run,FrameP95,ProjectileInclusiveMs,RejectedFraction,CapsuleCalls | Format-Table -AutoSize
