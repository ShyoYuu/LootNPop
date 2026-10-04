# sphere/선분 재사용을 켠 같은 바이너리에서 서버 그리드만 Off→On→On→Off로 비교한다.
param([string]$OutputDirectory = 'Saved/Profiling/Phase12/Grid')
$ErrorActionPreference = 'Stop'
$taskRunner = Join-Path $PSScriptRoot 'RunPhase13Gate.ps1'
$taskArguments = @{ PackageDirectory = 'Saved/SurfaceNavigationPhase12GridPackage'; ProjectileSphereBroadPhase = 1 }
foreach ($taskCase in @('N700', 'N800Chase')) {
    $taskArguments.OutputDirectory = "$OutputDirectory/Off"
    & $taskRunner @taskArguments -Cases $taskCase -Repeats 1 -ProjectileSpatialGrid 0
    $taskArguments.OutputDirectory = "$OutputDirectory/On"
    & $taskRunner @taskArguments -Cases $taskCase -Repeats 2 -ProjectileSpatialGrid 1
    $taskArguments.OutputDirectory = "$OutputDirectory/Off"
    & $taskRunner @taskArguments -Cases $taskCase -Repeats 1 -FirstRepeat 2 -ProjectileSpatialGrid 0
}
foreach ($taskMode in @('Off', 'On')) {
    $taskArguments.OutputDirectory = "$OutputDirectory/Trace$taskMode"
    & $taskRunner @taskArguments -Cases N700,N800Chase -Repeats 1 -Trace -ProjectileSpatialGrid ([int]($taskMode -eq 'On'))
}
$taskArguments.OutputDirectory = "$OutputDirectory/SampledOn"
& $taskRunner @taskArguments -Cases N700,N800Chase -Repeats 1 -Trace -ProjectileSpatialGrid 1 -ProjectileProfileStride 64
# 전수 정밀 hit 후보 누락 검사 비용은 성능 대조에 섞지 않는다.
$taskArguments.OutputDirectory = "$OutputDirectory/Validation"
& $taskRunner @taskArguments -Cases N700 -Repeats 1 -ProjectileSpatialGrid 1 -ProjectileProfileStride 64 -ValidateProjectileSpatialGrid
