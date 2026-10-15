# 멀티플레이 로비 아키텍처

> 상태: 기준 설계(Phase 1 범위) / 초안(Phase 3 이후)
> 마지막 갱신: 2026-10-15
> 소유 정보: 맵·클래스 구성, 접속 시퀀스, 기존 초기화 시퀀스와의 경계, 출발 시점의 코드 사실

## 1. 구성 요소

### 맵

| 맵 | 역할 | NetMode | GameMode | 월드 생성 |
|:---|:---|:---|:---|:---|
| `L_Frontend` | 실행 직후 메인 메뉴. 접속 실패·종료 뒤 돌아오는 곳 | Standalone | `ALNPFrontendGameMode` | 없음 |
| `L_Lobby` (Phase 3) | 메뉴 UI 대기실 | ListenServer / Client | `ALNPLobbyGameMode` | 없음(L-005) |
| `TestMap03` (게임 맵) | 실제 게임. 옥탄트 생성·SurfaceData·Mass 스폰 | ListenServer / Client / Standalone | 기존 `ALNPGameMode`(월드 세팅 오버라이드 `BP_LNPGameMode`) | 4단계 초기화 |

- 기본 맵(`GameDefaultMap`)을 `L_Frontend`로 바꾼다. 에디터 시작 맵(`EditorStartupMap`)은 TestMap03을 유지한다.
- 프론트엔드·로비 맵은 월드 세팅에서 각자 GameMode를 오버라이드한다. 전역 기본 GameMode(`/Script/Engine.GameMode`)는 바꾸지 않는다.

### 클래스(신규, 이름은 구현 시 확정)

| 클래스 | 역할 |
|:---|:---|
| `ULNPGameInstance` | 맵을 넘어 유지되는 접속 상태. 접속 진입점(주소로 접속·호스트 시작), `NetworkFailure`·`TravelFailure` 수신 → `L_Frontend` 복귀, 마지막 실패 사유 보관, 메뉴가 사유를 읽어 표시 |
| `ALNPFrontendGameMode` / 프론트엔드 PC | 월드 생성 없이 메인 메뉴 위젯을 띄우고 UI 입력 모드를 건다 |
| 메인 메뉴 위젯(CommonUI) | Host / Join(IP 입력) / Quit, 접속 중 표시·취소, 실패 사유 모달 |
| `ALNPLobbyGameMode` / `ALNPLobbyGameState` / 대기실 위젯 (Phase 3) | 참가자 목록·준비 상태 복제, 호스트 시작 → 서버 트래블 |
| 세션 계층 (Phase 4) | 방 생성·검색·참가를 백엔드에서 추상화. 결과로 접속 진입점을 부른다 |

메뉴 위젯은 기존 인게임 메뉴의 CommonUI 패턴(`ULNPUILayoutWidget`의 `MenuStack`, Back 전파, 게임패드 글리프)을 재사용한다([TechDesign_InGameMenu.md](../../TechDesign_InGameMenu.md)).

### 접속 진입점

메뉴·대기실·세션 계층은 GameInstance의 두 함수만 부른다.

- **호스트 시작**: 대상 맵을 listen 옵션으로 연다. Phase 1은 게임 맵, Phase 3부터는 `L_Lobby`.
- **주소로 접속**: 주소 문자열(`IP[:Port]`, 이후 백엔드 주소 형식)로 클라이언트 트래블한다.

백엔드가 바뀌어도 바뀌는 것은 "주소를 얻는 방법"(직접 입력 ↔ 세션 검색·초대)과 NetDriver뿐이다(`research/OnlineBackends.md`). 위젯에 백엔드별 접속 코드를 두지 않는다.

## 2. 출발 시점의 코드 사실(2026-10-15)

| 항목 | 사실 | 위치 |
|:---|:---|:---|
| 엔진·복제 | UE 5.8, Iris(`SetupIrisSupport`), 리슨 서버 전용(Server 타깃 없음) | `LootNPop.Build.cs:12` |
| 온라인 | OnlineSubsystem·Online Services·CommonUser·Steam·EOS 플러그인 없음, `[OnlineSubsystem]` 섹션 없음 | `LootNPop.Build.cs:71`(주석만) |
| 맵 설정 | `GameDefaultMap`·`EditorStartupMap` 모두 TestMap03, `GameInstanceClass` 미설정 | `Config/DefaultEngine.ini:8-10` |
| 트래블 | `ServerTravel`·`ClientTravel`·세션 API 사용 0건, 넷 관련 콘솔 명령 없음 | 소스 전체 |
| 서버 초기화 | `BeginPlay`가 시드를 정하고 WorldGeneration → SurfaceBaking → EntitySpawning → Complete | `GameMode/LNPGameMode.cpp:26-124` |
| 시드 | `OctantGenSeed`가 0이면 `FMath::Rand()`. 결과가 0이면 미설정과 구분되지 않아 클라이언트 RepNotify가 오지 않을 수 있다(추론, 미검증) | `LNPGameMode.cpp:37-40`, `LNPGameState.h:36-37` |
| 폰 스폰 게이팅 | `RestartPlayer`가 서버 초기화 완료와 클라이언트 Ready 둘 다 기다린다. Ready는 `OnClientReady`가 받아 늦게 온 클라이언트(중간 참가 포함)를 즉시 스폰한다 | `LNPGameMode.cpp:126-141`, `:188-201` |
| 클라이언트 투-게이트 | `OnRep_OctantGenSeed`가 월드 생성 시작, `OnRep_ServerPhase >= SurfaceBaking`과 월드 생성 완료가 합류하면 SurfaceData 로드 | `GameMode/LNPGameState.cpp:28-71` |
| 로딩·Ready | PC `BeginPlay`가 로딩 화면을 띄우고 HUD·UILayout을 붙인다. SurfaceData 게시 후 `ServerNotifyClientReady`. `ShowLoadingScreen`은 BP 이벤트이고 BP 구현 흔적이 없다 | `Player/LNPPlayerController.cpp:145-209`, `.h:48-52` |
| NetMode 의존 | Mass 템플릿 warm-up과 서버 전용 옵저버 복구가 `OnWorldBeginPlay`의 NetMode를 읽는다 → L-002 | `GameLogic/LNPMassSpawnSubsystem.cpp:124-144`, `:195-253` |
| 스폰 지점 | TestMap03에 PlayerStart 4개, `ShouldSpawnAtStartSpot()`이 항상 false라 매번 기본 추첨 | `GameMode/LNPGameMode.h:32` |
| 정원·이탈 | `MaxPlayers` 설정 없음, `PreLogin`·`PostLogin`·`Logout` 오버라이드 없음 | `LNPGameMode.h` |
| 인원 전제 | 적 타깃 후보 `PotentialTargets[4]`(가까운 4명까지 고려), 클라이언트당 대역폭 상한 150KB/s | `Enemy/LNPEnemyMassTypes.h:163`, `Config/DefaultEngine.ini` IpNetDriver |
| 중간 참가 검증 | 패키지 리슨 2P LateJoin(+60초) snapshot·패널·장치 대조 PASS | [Phase09_NetworkV010](../../SurfaceSupportNavigation/phases/Phase09_NetworkV010.md) |
| 인게임 메뉴 | Stats·Inventory·Settings 탭. "나가기"·"메인 메뉴로" 없음. 메뉴 열면 GameAndUI·커서 표시 | `LNPPlayerController.cpp:279-309` |

코드가 바뀌면 이 표가 아니라 해당 프로젝트 공통 설계 문서를 갱신하고, 이 표는 출발점 기록으로 둔다.

## 3. 시퀀스

### 3.1 호스트 시작(Phase 1)

1. 메뉴 Host → GameInstance 호스트 시작 → 게임 맵을 `?listen`으로 연다.
2. 새 월드가 처음부터 ListenServer로 로드된다. Mass warm-up·옵저버 복구가 올바른 NetMode를 본다(L-002).
3. 이후는 기존 서버 4단계 초기화와 같다. 호스트 PC는 기존 `RestartPlayer` 게이팅으로 스폰된다.

### 3.2 게스트 접속·중간 참가(Phase 1)

1. 메뉴 Join → IP 입력 → GameInstance 주소로 접속 → 클라이언트 트래블. 메뉴는 접속 중 상태와 취소를 보여 준다.
2. 접속에 성공하면 서버가 있는 맵(게임 맵)이 클라이언트에 로드된다.
3. 서버가 아직 초기화 중이면 즉시 참가, 끝났으면 중간 참가다. 두 경우 모두 기존 투-게이트가 처리한다(L-006). 별도 분기는 없다.

### 3.3 실패·종료 복귀(Phase 1)

| 상황 | 감지 | 처리 |
|:---|:---|:---|
| 잘못된 주소·응답 없음 | `TravelFailure` / `NetworkFailure`(연결 타임아웃) | `L_Frontend`로, "호스트에 연결할 수 없음" |
| 호스트 종료·연결 끊김 | 게스트 `NetworkFailure` | `L_Frontend`로, "호스트와 연결이 끊김" |
| 게스트가 메뉴에서 나가기 | 인게임 "메인 메뉴로" | 연결 종료 후 `L_Frontend` |
| 호스트가 메뉴에서 나가기 | 인게임 "메인 메뉴로" + 확인 | 리슨 종료(게스트는 위 "끊김" 경로), 호스트는 `L_Frontend` |
| 정원 초과·버전 불일치(Phase 2) | PreLogin 거부 사유 | `L_Frontend`로, 거부 사유 표시 |

- "호스트가 나가기 = 방 종료"는 리슨 전용 규칙이다. 판정·확인 UI는 리슨 분기에 두고, 일반 경로(게스트 나가기·연결 끊김)는 서버 종류와 무관하게 쓴다(L-008 ②).
- 실패 사유는 GameInstance가 보관하고, 프론트엔드 메뉴가 열릴 때 한 번 표시한 뒤 지운다.
- 엔진 기본 동작(실패 시 기본 맵으로 이동)에 기대지 않고 GameInstance가 명시적으로 돌려보낸다. 기본 맵이 `L_Frontend`라 결과는 같아도 사유 표시가 필요하다.

### 3.4 대기실 → 게임(Phase 3, 초안)

1. 호스트가 `L_Lobby?listen`을 연다. 게스트는 3.2와 같은 진입점으로 대기실에 들어온다.
2. 대기실 GameState가 참가자·준비 상태를 복제한다. 호스트가 시작한다.
3. 서버가 non-seamless 서버 트래블로 게임 맵을 연다(L-003). 게스트는 다시 로드하며, 게임 맵의 PC `BeginPlay`가 로딩·Ready를 새로 실행한다.
4. 게임 진행 중 새로 접속한 게스트는 서버가 게임 맵에 있으므로 3.2의 중간 참가로 들어간다.
5. 미확정: 서버 트래블 때 Mass·Iris·월드 서브시스템 정리, 게임 옵션(시드·편성) 전달 방법(URL 옵션 등). Phase 3 착수 시 확정한다.

## 4. 기존 시스템과의 경계

- 초기화·스폰 게이팅은 [TechDesign_InitSequence.md](../../TechDesign_InitSequence.md)가 소유한다. 이 작업은 그 앞(어떻게 서버 월드를 열고 접속하는가)과 뒤(실패·종료)를 다룬다.
- 명령줄 하네스(`TestMap03?Listen`, `127.0.0.1`, `-LNPLoadBaseline*`)는 맵 URL을 직접 받으므로 메뉴를 거치지 않는다(L-007). GameInstance의 실패 복귀가 하네스 종료(`-LNPLoadBaselineQuit`)를 방해하지 않아야 한다.
- 인게임 메뉴 확장("메인 메뉴로")은 [TechDesign_InGameMenu.md](../../TechDesign_InGameMenu.md)의 탭·Back 규칙을 따른다.
