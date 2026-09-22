# Phase 0 작업 기록

> 상태: 완료
> 대상 Phase: 지형 계약과 회귀 테스트 맵
> 규칙: 날짜별 실제 수행 내용과 검증 증거만 추가한다. 최신 상태는 `../Current.md`가 소유한다.

## 2026-09-21 — 문서 체계 준비

### 수행

- 최초 통합 계획을 관심사별 설계·조사 문서로 분할했다.
- 세션 상시 로딩 문서를 `README.md`, `Current.md`, `Decisions.md`로 제한했다.
- Phase 0과 Phase 1 실행 문서를 만들었다.
- 최초 통합 계획을 `InitialPlan.md`로 보관했다.

### 검증

- 필수 문서와 보관 원본을 포함한 Markdown 파일 19개 생성 확인
- Markdown 상대 링크 누락 0건
- trailing whitespace·tab·잔여 치환 문자 0건
- 상시 로딩 문서 세 개 합계 276줄
- `CLAUDE.md` 비변경 확인
- 코드와 에셋은 변경하지 않음
- 빌드는 문서 변경이므로 실행하지 않음

### 다음 작업

- 기존 collision profile·tag·좌표 규약 조사
- 회귀 테스트 맵 위치와 fixture 제작 방식 확정

## 2026-09-21 — 지형 계약과 회귀 맵 완료

### 조사

- 프로젝트 collision 설정에는 커스텀 channel과 LNP 지형 profile이 없었다.
- `ULNPSurfaceCacheSubsystem`과 `ULNPWorldDeviceSpawnSubsystem`은 `ECC_WorldStatic`, 플레이어 조준은 `ECC_Visibility`를 사용하고 있었다.
- 지형 의미를 나타내는 GameplayTag·Actor Tag·Component Tag 관례와 Surface 전용 debug command는 없었다.
- 기존 `TestMap03`은 World Partition 본 게임 맵이고 공통 Surface 회귀 fixture로 분리하기에 적합하지 않아 독립 비-WP 맵을 선택했다.
- SurfaceCache는 월드 원점 방사 방향, 옥탄트는 원점 배치 후 8개 고정 회전을 사용하는 것을 코드와 기존 설계에서 확인했다.
- 작업 시작 시 사용자 변경 `Content/SpawnData/DA_MassSpawnConfig.uasset`을 확인했으며 수정하지 않았다.

### 수행

- 역할(`Support`, `Blocker`)과 수명주기(`Static`, `Dynamic`, `StatefulTraversal`, `Destructible`)를 직교 Component Tag로 분리했다.
- `Decoration`을 다른 의미와 배타적인 제외 태그로 정의했다.
- `LNPSurfaceSupport`, `LNPWorldExact` trace channel과 7개 LNP collision profile을 `DefaultEngine.ini`에 추가했다. 기존 소비 코드는 변경하지 않았다.
- 부유섬·단순 동굴 authoring 제한과 validation 오류 조건을 `design/TerrainContract.md`에 기록했다.
- `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`을 생성했다.
- 기본 지각, 부유섬 1개·2개, 가장자리 안·밖, 단순 동굴, 나무·바위·장식, 상태형 기둥, 이동 패널, 파괴 바닥, seam 사례를 37개 fixture actor로 구성했다.
- 고정 probe 위치, 예상 Support Layer, exact hit, 상징 NavComponent, 추격·타게팅, 동적 link·revision oracle을 `design/RegressionMap.md`에 기록했다.
- Engine 기본 shape만 사용하는 재생성·검증 스크립트 `Scripts/GenerateSurfaceRegressionMap.py`를 추가했다.

### 검증

- headless editor에서 맵 재로드 성공
- fixture actor 37개 확인
- 사례 Component Tag 10종 확인
- profile별 actor 수 확인: `LNPStaticTerrain` 17, `LNPStaticBlocker` 5, `LNPDynamicTerrain` 1, `LNPStatefulTraversal` 1, `LNPDestructibleTerrain` 1, `LNPDecoration` 12
- 각 profile과 필수 의미 태그 조합 일치 확인
- Python 검증 commandlet 종료 코드 0, 오류 0건
- game world 기동 종료 코드 0
- 로그에서 `Bringing World /Game/Maps/SurfaceNavigation/L_SurfaceRegression... up for play`와 `Load map complete` 확인
- 소스 코드 변경이 없어 C++ 전체 빌드는 생략했다.

### 알려진 비차단 사항

- headless game 기동 시 프로젝트에 활성화된 실험 Toolset의 Python startup script가 game target에 없는 editor-only Python type을 참조하는 기존 오류가 출력됐다. 맵 로드와 play world 기동은 완료됐고 프로세스 종료 코드는 0이었다.
- 신규 trace channel은 Phase 0에서 예약·fixture 적용만 했으며 실제 SurfaceCache·투사체 소비자 전환은 후속 Phase에서 정확성 테스트와 함께 수행한다.
