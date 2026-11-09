# 포트폴리오 영상용으로 벤치마크 경로를 한 번 재생하며 화면을 녹화한다. 이 실행의 수치는 성능 기준선으로 쓰지 않는다.
# 게임이 덮는 모니터 한 대를 Desktop Duplication(ddagrab)으로 녹화하고, 측정 구간만 잘라 남긴 뒤 원본은 지운다.
param(
    [ValidateSet('Low', 'Medium', 'High')][string]$Profile = 'High',
    [string]$PackageDirectory = 'Saved/Graphics/RenderSyncDiagPackage_20261108',
    [string]$RunOutputDirectory = 'Saved/Profiling/Graphics/Phase00/Video',
    [string]$VideoDirectory = 'Portfolio/Graphics/Phase00',
    [ValidateRange(0, 600)][int]$WarmupSeconds = 60,
    [ValidateRange(10, 600)][int]$DurationSeconds = 60,
    [string]$StatCommand = 'stat unit',        # 빈 문자열이면 수치 표시 없음
    [string]$FfmpegPath = 'D:\Tools\ffmpeg\bin\ffmpeg.exe',
    [int]$OutputIndex = 0,
    [double]$TrimLeadSeconds = 0.5             # 측정 시작 직후 1번 구도 복귀 순간을 피한다
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
$Abs = { param($p) if ([IO.Path]::IsPathRooted($p)) { $p } else { Join-Path $Root $p } }
if (-not (Test-Path -LiteralPath $FfmpegPath)) { throw "ffmpeg missing: $FfmpegPath" }
$Stamp = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')
$RunDir = Join-Path (& $Abs $RunOutputDirectory) "Record_${Profile}_$Stamp"
$VideoDir = & $Abs $VideoDirectory
New-Item -ItemType Directory -Force -Path $RunDir, $VideoDir | Out-Null
$Raw = Join-Path $RunDir 'Raw.mkv'
$Final = Join-Path $VideoDir ("Phase00_{0}_Route_{1}.mp4" -f $Profile, $Stamp)

# 게임 창이 다른 창 아래에 뜨지 않도록 열린 창을 모두 최소화한다.
(New-Object -ComObject Shell.Application).MinimizeAll()
Start-Sleep -Seconds 2

$Info = [Diagnostics.ProcessStartInfo]::new($FfmpegPath)
$Info.Arguments = ('-hide_banner -nostats -loglevel warning -y -f lavfi -i "ddagrab=output_idx={0}:framerate=60:draw_mouse=0" ' +
    '-fps_mode cfr -r 60 -c:v h264_nvenc -preset p5 -rc vbr -cq 17 -b:v 0 "{1}"') -f $OutputIndex, $Raw
$Info.UseShellExecute = $false
$Info.RedirectStandardInput = $true
$Info.CreateNoWindow = $true
$Ffmpeg = [Diagnostics.Process]::Start($Info)
$RecordStartUtc = [DateTime]::UtcNow
$Meta = [ordered]@{ Profile = $Profile; WarmupSeconds = $WarmupSeconds; DurationSeconds = $DurationSeconds
    StatCommand = $StatCommand; RecordStartUtc = $RecordStartUtc.ToString('o'); BaselineEligible = $false }
try {
try {
    $Extra = @()
    if ($StatCommand) { $Extra += ('-ExecCmds="{0}"' -f $StatCommand) }
    $Manifest = & (Join-Path $PSScriptRoot 'RunGraphicsBenchmark.ps1') -Profile $Profile -PackageDirectory $PackageDirectory `
        -OutputDirectory $RunDir -WarmupSeconds $WarmupSeconds -DurationSeconds $DurationSeconds -ExtraArguments $Extra
} finally {
    if (-not $Ffmpeg.HasExited) { $Ffmpeg.StandardInput.Write('q'); $Ffmpeg.StandardInput.Close() }
    if (-not $Ffmpeg.WaitForExit(60000)) { $Ffmpeg.Kill() }
}
$Log = Get-ChildItem -LiteralPath $RunDir -Filter 'Runtime.log' -Recurse | Select-Object -First 1
$Cap = Select-String -LiteralPath $Log.FullName -Pattern '^\[(\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d:\d{3})\].*\[GraphicsBenchmark\] Capture started' | Select-Object -First 1
if (-not $Cap) { throw "Capture start line missing: $($Log.FullName)" }
$CaptureUtc = [DateTime]::ParseExact($Cap.Matches[0].Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', $null, 'AssumeUniversal,AdjustToUniversal')
$Offset = ($CaptureUtc - $RecordStartUtc).TotalSeconds + $TrimLeadSeconds
$Length = $DurationSeconds - $TrimLeadSeconds - 0.5
& $FfmpegPath -hide_banner -nostats -loglevel warning -y -ss ($Offset.ToString('0.000', [Globalization.CultureInfo]::InvariantCulture)) -i $Raw `
    -t ($Length.ToString('0.000', [Globalization.CultureInfo]::InvariantCulture)) -an -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -movflags +faststart $Final
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $Final)) { throw 'Trim failed' }
} finally {
    # 원본에는 게임 창이 뜨기 전·닫힌 뒤의 화면이 들어 있으므로 성공·실패와 관계없이 남기지 않는다.
    if (Test-Path -LiteralPath $Raw) { Remove-Item -LiteralPath $Raw }
}
$Meta.CaptureStartUtc = $CaptureUtc.ToString('o'); $Meta.TrimOffsetSeconds = $Offset; $Meta.TrimLengthSeconds = $Length
$Meta.Video = $Final; $Meta.RunManifest = "$Manifest"
$Meta | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $RunDir 'Record.json')
Write-Output "VIDEO=$Final"
