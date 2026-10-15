# 멀티플레이 로비 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-10-15
> 규칙: 결론만 기록하고 상세 근거와 구현은 링크된 문서가 소유한다. ID 접두어 `L-`는 `SurfaceSupportNavigation`의 `D-`와 구분하기 위한 것이다.

## 결정 목록

| ID | 결정 | 상태 | 상세 근거 |
|:---|:---|:---|:---|
| L-001 | 접속 수단은 IP 직접 접속을 먼저 구현한다. 온라인 백엔드(Steam·EOS·보류)는 Phase 5 착수 때 고른다. IP 경로는 백엔드를 도입한 뒤에도 개발·LAN·같은 PC 다중 인스턴스 테스트용으로 유지한다. | 확정(2026-10-15, 백엔드는 사용자 미정) | `research/OnlineBackends.md` |
| L-002 | 리슨 서버 월드는 맵 로드 시점부터 listen이다. 호스트 시작은 `?listen`으로 맵을 열거나 서버 트래블로 연다. 실행 중인 Standalone 월드를 Listen으로 바꾸지 않는다. Mass 템플릿 warm-up과 서버 전용 옵저버 복구가 `OnWorldBeginPlay` 시점의 NetMode를 읽기 때문이다. | 확정 | `design/Architecture.md` §2 |
| L-003 | 맵 전환은 non-seamless다. 로딩 화면·Ready RPC가 `PlayerController::BeginPlay`에 묶여 있어 컨트롤러가 유지되는 seamless travel에서는 다시 돌지 않는다. 이 게이트를 BeginPlay 밖으로 옮기기 전에는 seamless를 도입하지 않는다. | 확정 | `design/Architecture.md` §2 |
| L-004 | 리슨 서버 한 방의 최대 인원은 호스트 포함 5명이다(데디서버 정원은 별도, L-008). 정원 초과 접속은 PreLogin에서 사유와 함께 거부하고, 게스트는 메인 메뉴에서 사유를 본다. | 확정(2026-10-15, 사용자 결정) | `Roadmap.md` Phase 2 |
| L-005 | 대기 공간은 월드 생성이 없는 경량 로비 맵 `L_Lobby`의 메뉴 UI 대기실이다. 옥탄트 생성·SurfaceData·Mass 스폰은 게임 맵에서만 일어난다. | 확정(2026-10-15, 사용자 결정) | `design/Architecture.md` §3 |
| L-006 | 중간 참가는 기존 투-게이트 초기화(`RestartPlayer` 게이팅·`OnRep_ServerPhase`)를 그대로 쓴다. 메뉴·로비 전용 late-join 경로를 만들지 않는다. | 확정 | [TechDesign_InitSequence.md](../TechDesign_InitSequence.md) §4 |
| L-007 | 명령줄로 맵 URL을 받은 실행(회귀·성능 하네스)은 메뉴를 거치지 않고 지금처럼 동작해야 한다. 기본 맵을 `L_Frontend`로 바꿔도 하네스 명령줄은 바꾸지 않는다. | 확정 | `design/Validation.md` §3 |
| L-008 | 데디케이티드 서버는 이 폴더가 아닌 별도 작업(예: `.context/DedicatedServer/`)으로 진행하며, 이 작업의 Phase 4 이후에 시작한다. 로비 작업은 데디서버와 맞물리는 경계만 지킨다. ① 정원 5는 리슨 모드 값이며 하드코딩하지 않고 서버 모드별 설정으로 둔다. ② "호스트 = 로컬 플레이어" 가정(호스트가 시작, 호스트 나가기 = 방 종료 등)은 리슨 전용 분기에 모은다. ③ 세션 계층·백엔드 선택은 리슨 방과 함께 데디서버 광고·검색을 담을 수 있는지 함께 본다. | 확정(2026-10-15, 사용자 결정) | `Roadmap.md` §5 |

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다.
- 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향을 관련 설계 문서 및 현재 Phase 로그에 기록한다.
- 단순 구현 선택이나 임시 디버그 값은 이 원장에 추가하지 않는다.
