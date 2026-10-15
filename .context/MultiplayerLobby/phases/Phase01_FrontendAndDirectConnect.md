# Phase 1 — 프론트엔드와 IP 직접 접속

> 상태: 착수 전
> 선행: 없음(중간 참가는 기존 초기화 시퀀스로 이미 동작, L-006)
> 순서: 단위 0 → 1 → 2 → 3 → 4. 단위 완료·검증 뒤 사용자 진행 확인을 받는다.

## 배경

지금은 명령줄(`TestMap03?Listen`, `127.0.0.1`)로만 멀티플레이를 띄운다. 일반 사용자가 패키지 exe를 실행해 방을 만들고 참가하는 경로가 없다. 이 Phase는 대기실 없이 가장 짧은 경로를 만든다.

- 호스트: 메뉴에서 Host → 게임 맵이 곧바로 listen으로 열리고 게임이 시작된다.
- 게스트: 메뉴에서 IP 입력 → 진행 중인 게임에 참가한다(서버가 아직 초기화 중이면 즉시 참가).
- 어느 쪽이든 실패·종료하면 메뉴로 돌아와 사유를 본다.

## 목표와 범위

범위:

- `L_Frontend` 맵과 메인 메뉴(Host / Join / Quit)
- `ULNPGameInstance` 접속 진입점(호스트 시작·주소로 접속)과 실패 복귀
- 인게임 메뉴 "메인 메뉴로"
- `OctantGenSeed` 0 충돌 정리

범위 밖: 플레이어 이름·정원·스폰 지점·이탈 정리(Phase 2), 대기실(Phase 3), 방 목록(Phase 4), 로딩 화면 제작(체감 문제가 있으면 후속으로 기록만).

## 단위 0 — 사전 정리

1. **시드 0 회피.** `ALNPGameMode::BeginPlay`가 `OctantGenSeed`를 정할 때 결과가 0이 아니도록 보장한다(`LNPGameMode.cpp:37-40`). 0은 "미설정"과 구분되지 않아 클라이언트 RepNotify가 오지 않을 수 있다. Config로 0이 아닌 값을 준 경우·`-FixedSeed` 경로는 그대로 둔다.
2. **쿡 맵 목록.** 패키징은 `-map=` 인자로 맵을 넘기고 Config에 `MapsToCook`이 없다. `L_Frontend`(Phase 3에서 `L_Lobby`)를 포함시키는 방법을 정한다 — 스크립트 `-map` 추가와 `DefaultGame.ini` 패키징 설정 중 하나를 고르고 근거를 로그에 남긴다.
3. **기본 맵 전환 영향 확인(L-007).** `GameDefaultMap`을 `L_Frontend`로 바꿨을 때 명령줄 하네스(호스트 `TestMap03?Listen`, 게스트 `127.0.0.1`, `-LNPLoadBaselineQuit`)가 그대로 동작하는지 확인한다. 게스트는 접속 실패 시 기본 맵으로 떨어지므로 하네스 결과 해석에 영향이 없는지도 본다.

완료 조건: 시드 수정 빌드·기존 자동화 통과, 쿡 맵 방법 결정, 하네스 영향 없음 확인(또는 필요한 대응 목록).

## 단위 1 — 프론트엔드 골격

1. `ULNPGameInstance`를 만들고 `DefaultEngine.ini` `GameInstanceClass`로 지정한다.
2. `L_Frontend` 맵: 월드 세팅 GameMode 오버라이드 `ALNPFrontendGameMode`(월드 생성·Mass 스폰 없음, 폰 없음), 프론트엔드 PC가 메인 메뉴를 띄우고 UI 입력 모드·커서를 건다.
3. 메인 메뉴(CommonUI 활성화 위젯): Host / Join / Quit. Join은 IP 입력 화면(마지막 주소를 로컬 설정에 저장해 기본값으로)과 접속 버튼. 게임패드 조작·Back 전파는 인게임 메뉴 규칙을 따른다([TechDesign_InGameMenu.md](../../TechDesign_InGameMenu.md)).
4. 실패 사유 모달: GameInstance가 보관한 사유가 있으면 메뉴 진입 시 한 번 표시한다.
5. `GameDefaultMap` → `L_Frontend`. `EditorStartupMap`은 TestMap03 유지.

완료 조건: 패키지 exe를 인자 없이 실행하면 메인 메뉴가 뜨고, 키보드·마우스·게임패드로 조작되며, Quit이 동작한다. 기존 PIE(TestMap03)가 영향받지 않는다.

## 단위 2 — 호스트·게스트 접속

1. GameInstance **호스트 시작**: 게임 맵을 listen 옵션으로 연다(L-002). 포트는 기본값(7777)을 쓴다.
2. GameInstance **주소로 접속**: `IP[:Port]` 문자열을 검증(빈 값·형식)한 뒤 클라이언트 트래블한다.
3. 메뉴의 접속 중 상태와 취소. 취소는 진행 중인 연결을 끊고 메뉴로 돌아온다.
4. 메뉴는 이 두 함수만 부른다. 위젯에 트래블 코드를 두지 않는다(Phase 4 세션 계층의 교체 지점).

완료 조건: 같은 PC 2인스턴스에서 S-01·S-02·S-03 통과, 호스트 로그의 Mass warm-up `NetMode=2`(`design/Validation.md` §5).

## 단위 3 — 실패·종료 처리

1. GameInstance가 엔진의 네트워크 실패·트래블 실패 이벤트를 구독해 종류·사유를 기록하고 `L_Frontend`로 돌려보낸다(`design/Architecture.md` §3.3 표).
2. 인게임 메뉴에 "메인 메뉴로"를 추가한다. 호스트는 "방이 닫히고 모든 게스트가 나간다"는 확인을 거친다.
3. 실패 복귀가 명령줄 하네스 종료(`-LNPLoadBaselineQuit`)를 방해하지 않는지 확인한다.
4. 메뉴 ↔ 게임 반복 전환 시 남는 상태(GameInstance 플래그, 위젯, 입력 모드)를 정리한다.

완료 조건: S-04·S-05·S-06·S-07·S-08 통과. 실패 사유 로그가 남는다.

## 단위 4 — 검증과 마무리

1. 패키지 같은 PC 2인스턴스로 S-01~S-08을 한 번에 묶어 사용자 플레이 테스트를 요청한다.
2. LAN 2 PC로 S-01·S-03·S-06을 1회 확인한다(방화벽 허용 프롬프트 포함).
3. 기존 명령줄 리슨 2P 하네스 Normal·LateJoin 회귀 PASS.
4. 공통 스모크(`design/Validation.md` §4) 통과.
5. 문서: `TechDesign_InitSequence.md`에 진입 경로(메뉴 → listen 맵 열기)와 실패 복귀 한 절, `TechDesign_InGameMenu.md`에 "메인 메뉴로" 반영. `Current.md`·`Roadmap.md` 갱신.

## 완료 조건 체크리스트

- [ ] 단위 0: 시드 0 회피, 쿡 맵 방법 결정, 하네스 영향 확인
- [ ] 단위 1: 패키지 실행 시 메인 메뉴, 게임패드 조작, Quit
- [ ] 단위 2: Host로 게임 시작, IP로 즉시·중간 참가(S-01~S-03)
- [ ] 단위 3: 실패·종료 복귀와 사유 표시(S-04~S-08)
- [ ] 단위 4: LAN 2 PC, 하네스 회귀, 공통 스모크, 공통 설계 문서 갱신

## 다음 Phase 인계 항목

- 플레이어 수 5에서 PlayerStart 4개가 겹치는 문제, `Logout` 정리 부재(Phase 2).
- 로딩 화면이 없어 게스트가 초기화(패키지 기준 수십 초) 동안 빈 화면을 보는지 — 체감 문제면 별도 항목으로 기록.
- Phase 3에서 호스트 시작 대상이 `L_Lobby`로 바뀐다. 접속 진입점의 대상 맵을 바꿀 수 있게 둔다.
