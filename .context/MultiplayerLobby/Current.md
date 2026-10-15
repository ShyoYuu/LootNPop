# 멀티플레이 로비 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 1 프론트엔드와 IP 직접 접속 — 착수 전
> 마지막 갱신: 2026-10-15

## 현재 결정

**작업 계획 수립(2026-10-15).** 코드 변경은 아직 없다. 사용자 결정: 최대 인원 5(L-004), 대기 공간은 메뉴 UI 대기실(L-005), 온라인 백엔드는 미정이며 Phase 5 착수 때 고른다(L-001). 데디서버는 Phase 4 이후 별도 작업으로 하고, 이 작업은 맞물리는 경계(정원은 모드별 설정, 호스트 가정은 리슨 전용 분기)만 지킨다(L-008).

출발점(상세는 `design/Architecture.md` §2):

- 메뉴·프론트엔드 맵·GameInstance·Travel 코드·OnlineSubsystem 모두 없다. 실행은 명령줄 `TestMap03?Listen` + `127.0.0.1`뿐이다.
- 중간 참가는 이미 동작한다. 패키지 리슨 2P LateJoin(+60초) PASS 기록이 있다([Phase09_NetworkV010](../SurfaceSupportNavigation/phases/Phase09_NetworkV010.md)).
- Phase 1에서 함께 다룰 함정: `OctantGenSeed` 0 충돌, 맵 로드 시점 NetMode 의존(L-002).

## 바로 다음 행동

- [Phase 1](phases/Phase01_FrontendAndDirectConnect.md) 단위 0(사전 정리)부터 시작한다. `history/Phase01_Log.md`를 만들고 거기에 기록한다.

## 인계 근거

- 기존 패키지 2P 하네스(로컬, Git 미포함): `Saved/SliceOctant/NetworkV010/run-package-2p.ps1`, 쿡 스크립트 `Saved/SliceOctant/NetworkV010/cook-after-gate.ps1`. 커밋된 하네스: `Scripts/Profiling/RunPhase13Gate.ps1`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
