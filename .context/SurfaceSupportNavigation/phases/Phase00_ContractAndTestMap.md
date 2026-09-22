# Phase 0 — 지형 계약과 회귀 테스트 맵

> 상태: 완료
> 예상 범위: 1세션
> 선행 조건: 없음

## 목표

후속 베이커·런타임 충돌·Nav·Enemy 이동 구현이 같은 의미와 검증 공간을 사용하도록 authoring 계약과 공통 회귀 테스트 맵을 확정한다.

## 입력

- 현재 `SurfaceCache` 구현과 collision 설정
- 기존 옥탄트 Level Instance 제작 규약
- `../design/Architecture.md`
- `../design/ValidationAndMigration.md`
- `../../TechDesign_SurfaceCache.md`
- `../../TechDesign_WorldGeneration.md`

## 산출물

- 정적·동적 지형 의미 규약
- Support·Atlas·Nav·Component 용어 확정
- collision profile 및 channel 초안
- 부유섬·동굴 authoring 제한
- 모든 후속 Phase가 재사용할 회귀 테스트 맵
- 각 테스트 지점의 기대 query·이동 결과

## 조사 작업

- [x] 현재 지형·프랍·투사체 collision channel과 profile 조사
- [x] 지형 관련 GameplayTag, Actor tag, component tag 관례 조사
- [x] 현재 SurfaceCache 좌표계와 옥탄트 slot transform 확인
- [x] 기존 테스트 맵과 개발용 debug command 재사용 가능성 확인
- [x] 맵·에셋의 기존 사용자 변경 확인

## 의미 계약 초안

최소한 다음 범주를 서로 구분할 수 있어야 한다.

- [x] 정적 Support Surface
- [x] 정적 Blocking Surface
- [x] Support와 Blocker를 겸하는 일반 지형
- [x] Dynamic Support
- [x] 이동 중에는 사용할 수 없는 상태형 Traversal
- [x] 안정 상태에서 Walk Link가 되는 기믹 지형
- [x] 파괴로 사라지는 Support
- [x] 파괴로 사라지는 Blocker
- [x] 베이크·Nav·투사체 판정에서 제외할 장식 geometry

## 테스트 맵 사례

- [x] 기본 지각
- [x] 같은 방사 방향의 부유섬 하나
- [x] 같은 방사 방향의 부유섬 둘
- [x] 섬 가장자리·측벽·밑면
- [x] 단순 동굴 입구·천장·바닥
- [x] 나무·바위 blocker
- [x] 정해진 위치로 쓰러지는 기둥
- [x] 움직이는 패널
- [x] 파괴 가능한 바닥 조각
- [x] 옥탄트 seam

## 기대 결과 기록

각 사례에는 다음을 기록한다.

- 고정된 debug 위치와 방향
- 예상 Support Layer 개수
- 예상 exact trace 또는 sweep hit
- 예상 NavComponent
- 지상 NPC가 추격 가능한 대상 위치
- 원거리 타게팅 가능 여부
- 동적 상태 변경 전후의 예상 link와 revision

## 검증

- 에디터와 PIE에서 테스트 맵을 열 수 있다.
- 좌표와 기대 결과가 머신에 의존하지 않는다.
- 지원하는 형상과 의도적으로 지원하지 않는 형상이 문서에 명시된다.
- Phase 1~8이 동일한 테스트 사례를 재사용할 수 있다.

## 완료 조건

- [x] 의미 계약이 관련 설계 문서에 반영됨
- [x] collision profile 초안이 실제 프로젝트 설정과 충돌하지 않음
- [x] 회귀 테스트 맵과 필요한 fixture가 생성됨
- [x] 테스트 지점과 기대 결과가 기록됨
- [x] `Current.md`가 Phase 1 착수 상태로 전환됨
- [x] `../history/Phase00_Log.md`에 결과와 검증 증거가 기록됨

## 제외 범위

- 실제 SurfaceData 직렬화
- Support Atlas 베이커 구현
- MassWorldCollision 구현
- Nav Grid 생성
- Mesh Terrain 제작 방식 최종 선택
