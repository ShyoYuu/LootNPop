# sphere 대조의 패키지/부하/Nav 검증을 재사용하고 그리드 누락·fallback·후보 감소를 확인한다.
param([string]$InputDirectory = 'Saved/Profiling/Phase12/Grid')
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'AnalyzePhase12Sphere.ps1') -InputDirectory $InputDirectory -SpatialGrid
