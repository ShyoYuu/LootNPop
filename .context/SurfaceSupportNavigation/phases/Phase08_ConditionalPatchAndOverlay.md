# Phase 8 — Conditional Patch와 파괴 Overlay

> 상태: 진행 중
> 착수: 2026-10-03
> 순서: Phase 8 완료 뒤 Phase 13. 구현 단위마다 사용자 진행 확인을 받는다.

## 목표와 범위

`design/DynamicTerrain.md` §4~§8과 D-028·D-041을 구현한다. 서버가 마커 상태로 사전 베이크된 Support·Nav 점유·Walk Link를 활성화하며, 런타임 베이크는 하지 않는다. 상태형 요소는 서버 스폰 복제 Actor이고 임의 잔해는 보행면을 만들지 않는다.

## 구현 단위와 검증 기준

1. **Nav 점유 합성 기반**: 기존 Pod와 활성 마커별 node ref 목록을 합성한다. 겹친 blocker 제거, 중복 상태, stale generation, slot 분리, 변경 Tile revision과 기존 Pod 우회를 자동화로 확인한다. 아직 Actor 상태나 patch stream을 연결하지 않는다.
2. **Patch authoring·베이크·codec**: 마커 로컬 source와 상태 transform을 정의하고 base 좌표계의 patch Layer·Nav·edge를 생성한다. stream/header·DataVersion·stale 입력을 함께 바꾼다. round-trip·잘못된 경계/clearance·production 재베이크를 검증한다. 이 단위 착수 때 실제 베이커 구조를 읽고 데이터 형식을 확정한다.
3. **런타임 활성화와 경로 무효화**: slot별 활성 patch Support·Nav view, 명시적 Walk edge, ReachabilityGroup 재구축과 ConnectivityGraphVersion을 연결한다. immutable snapshot은 Mass phase 경계에서 게시한다. link 닫힘·지역 revision·경로 재계획·무관 지역 유지·Pod 재귀속을 검증한다.
4. **상태형 요소와 파괴**: 안정 상태 기둥/다리, 파괴 전 바닥과 blocker를 서버 스폰 경로에 연결한다. 전이 중에는 계획적 traversal을 허용하지 않는다. 복제 상태·결정론적 자세·exact identity·collision을 함께 검증한다.
5. **회귀 콘텐츠와 최종 Gate**: 동적 회귀 맵에서 길 열림/닫힘과 patch 착지·낙하를 검사한다. 전체 자동화, cook/stale 검증, 패키지 1P·`-game` 리슨 2P·late join, 700마리 반복 성능 Gate를 수행하고 문서 인계 뒤 Phase 13 진행을 확인한다.

단위 2 이후의 구체적 파일/API와 검증 입력은 해당 단위 착수 때 실제 구현을 기준으로 보완한다. 모든 단위의 검증에 최종 2P Gate를 중복 수행하지 않고, 초기 순수 데이터 단위는 자동화로 검증한 뒤 연결된 기능 전체를 단위 5에서 검증한다.

## 진행 체크리스트

- [x] 단위 1: Nav 점유 합성 기반(전체 빌드·Nav 실검증 15개 성공, 비용 재생 1개 SKIP; `../history/Phase08_Log.md`)
- [x] 단위 2: Patch authoring·베이크·codec(DataVersion 7, 전체 빌드·3개 재베이크·실검증 86개 성공/비용 재생 1개 SKIP; `../history/Phase08_Log.md`)
- [x] 단위 3: 런타임 활성화와 경로 무효화(전체 빌드·최종 실검증 88개 성공/비용 재생 1개 SKIP; `../history/Phase08_Log.md`)
- [x] 단위 4: 상태형 요소와 파괴(전체 빌드·실검증 89종 성공, fixture 수정 후 선별 재검증 포함/비용 재생 1개 SKIP; `../history/Phase08_Log.md`)
- [ ] 단위 5: 회귀 콘텐츠와 최종 Gate
