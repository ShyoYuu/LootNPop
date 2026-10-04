# 같은 바이너리의 Off→On→On→Off 대조. 자연 700과 추격 800의 양끝 조건을 검증한다.
param([string]$OutputDirectory = 'Saved/Profiling/Phase12/Sphere')
$ErrorActionPreference = 'Stop'
$taskRunner = Join-Path $PSScriptRoot 'RunPhase13Gate.ps1'
$taskArguments = @{ PackageDirectory = 'Saved/SurfaceNavigationPhase12SpherePackage' }
foreach ($taskCase in @('N700', 'N800Chase')) {
    $taskArguments.OutputDirectory = "$OutputDirectory/Off"
    & $taskRunner @taskArguments -Cases $taskCase -Repeats 1 -ProjectileSphereBroadPhase 0
    $taskArguments.OutputDirectory = "$OutputDirectory/On"
    & $taskRunner @taskArguments -Cases $taskCase -Repeats 2 -ProjectileSphereBroadPhase 1
    $taskArguments.OutputDirectory = "$OutputDirectory/Off"
    & $taskRunner @taskArguments -Cases $taskCase -Repeats 1 -FirstRepeat 2 -ProjectileSphereBroadPhase 0
}
# trace도 상세 샘플을 끄고 processor 시간을 비교한 뒤, On 샘플로 정밀 검사 감소를 확인한다.
foreach ($taskMode in @('Off', 'On')) {
    $taskArguments.OutputDirectory = "$OutputDirectory/Trace$taskMode"
    & $taskRunner @taskArguments -Cases N700,N800Chase -Repeats 1 -Trace -ProjectileSphereBroadPhase ([int]($taskMode -eq 'On'))
}
$taskArguments.OutputDirectory = "$OutputDirectory/SampledOn"
& $taskRunner @taskArguments -Cases N700,N800Chase -Repeats 1 -Trace -ProjectileProfileStride 64 -ProjectileSphereBroadPhase 1
