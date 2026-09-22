# Codex 협업 규칙

프로젝트의 공통 협업 규칙과 상세 문서 인덱스는 [CLAUDE.md](CLAUDE.md)를 단일 기준으로 삼아 준수한다.

## Unreal Engine

- 엔진 소스 경로: `C:\Program Files\Epic Games\UE_5.8\Engine`
- 전체 빌드가 필요하면 다음 구성을 사용한다: `LootNPopEditor Win64 Development`
- 빌드 도구: `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat`

## PowerShell 인코딩

- 한글 출력 전 `[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false); $OutputEncoding = [Text.UTF8Encoding]::new($false)`를 설정한다.
- 문서는 `Get-Content -Raw -Encoding UTF8 -LiteralPath <경로>`로 읽는다.
- 출력이 깨지면 대용량 재출력 대신 작은 범위로 인코딩부터 확인한다.

## Unreal Engine 전체 빌드 실행 규칙

- `Build.bat`/UnrealBuildTool은 프로젝트 밖의 `%LOCALAPPDATA%\UnrealBuildTool`, `%LOCALAPPDATA%\Temp`, `C:\ProgramData\Epic\UnrealBuildAccelerator`, 엔진 캐시 등에 기록한다. 관리형 워크스페이스 샌드박스에서는 처음부터 권한 확장 실행을 사용하고, 동일 명령을 샌드박스 안에서 먼저 시도하거나 반복하지 않는다.
- `Using bundled DotNet SDK...`와 `Running UnrealBuildTool...`까지만 출력한 뒤 컴파일 진단 없이 종료 코드 1이 나오거나 `UnauthorizedAccessException`이 발생하면 코드/.NET 설치 문제가 아니라 샌드박스 외부 쓰기 거부를 우선 의심한다.
- 에디터가 닫혀 있을 때의 기준 명령은 `Build.bat LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`이다. 에디터가 열려 있으면 전체 빌드와 Live Coding을 동시에 사용하지 않는다.
- cook/package도 사용자 DDC, shader working directory와 엔진 캐시에 기록하므로 같은 권한 원칙을 적용한다.
