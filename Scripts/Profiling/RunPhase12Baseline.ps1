# 정확성 수정본의 trace 없는 기준선과 샘플 계측을 분리한다. 기존 증거는 덮어쓰지 않는다.
param([string]$OutputDirectory = 'Saved/Profiling/Phase12/Baseline', [switch]$DiagnosticsOnly)
$ErrorActionPreference = 'Stop'
$taskRunner = Join-Path $PSScriptRoot 'RunPhase13Gate.ps1'
$taskArguments = @{
    PackageDirectory = 'Saved/SurfaceNavigationPhase12BaselinePackage'
    OutputDirectory = "$OutputDirectory/Gate"
}
if (-not $DiagnosticsOnly) {
    foreach ($taskPair in @(@('N700', 'N800'), @('N700Chase', 'N800Chase'))) {
        & $taskRunner @taskArguments -Cases $taskPair[0] -Repeats 1
        & $taskRunner @taskArguments -Cases $taskPair[1] -Repeats 2
        & $taskRunner @taskArguments -Cases $taskPair[0] -Repeats 1 -FirstRepeat 2
    }
    $taskArguments.OutputDirectory = "$OutputDirectory/Trace"
    & $taskRunner @taskArguments -Cases N700,N800,N800Chase,N700Chase -Repeats 1 -Trace -ProjectileProfileStride 64
}
# 동일 700 자연 조건에서 상세 계측 Off와 더 성긴 샘플링으로 계측 영향·경향을 확인한다.
$taskArguments.OutputDirectory = "$OutputDirectory/DiagnosticOff"
& $taskRunner @taskArguments -Cases N700 -Repeats 1 -Trace
$taskArguments.OutputDirectory = "$OutputDirectory/Diagnostic128"
& $taskRunner @taskArguments -Cases N700 -Repeats 1 -Trace -ProjectileProfileStride 128
