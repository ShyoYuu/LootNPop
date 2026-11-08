# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: 옥탄트 양산 — Pyramid_00 확정, Grove_00·Ridge_00 제작 계획. Roadmap의 기존 Phase는 모두 완료(직전 Phase 10 완료 2026-10-13)
> 마지막 갱신: 2026-11-08

## 현재 결정

**Bastion·Hollow 외곽 엄폐 보강(2026-11-08, 사용자 PIE 확인·완료).** 각 맵 외곽에 기존 `FBlock` 경로의 큰 정적 엄폐 덩어리를 6개씩 추가했다. 폭 6~9m·지면 위 약 3.2~5m이며, Bastion은 `Block_06~11`, Hollow는 `Block_02~07`이다. Hollow는 기존 `M_Hollow_Strata`를 사용한다. 중앙 지형·Pod·훅·패널을 재생성하지 않고 새 액터만 추가했으며 기존 453/117개 액터의 transform·GUID 불변을 확인했다. 후보 12곳의 지면 중심/가장자리 108개 광선과 머리 높이 190cm의 엄폐 측면 48개 광선을 확인했다. 모든 엄폐물의 bounds는 이음매 평면에서 35m 밖이며 가장 가까운 기존 배치 마커와의 중심 거리는 Bastion 약 42.2m·Hollow 약 65.4m 이상이다. `LNPSliceOctant.cpp`의 원본 배치도 함께 갱신했고 Live Coding은 성공했다. 사용자 Bastion 재베이크·DA 저장 후 `BastionLowPolyContent`·`BastionLowPolyNav`·`BastionEncounters` 기존 자동 검사 3개가 오류/경고 0으로 통과했다. Hollow 첫 재베이크 뒤 `HollowContent`가 새 엄폐물 두 개의 기존 지각/내부 지형 보행 접합부 차단을 검출했다. 두 개를 (-14000, ±17500)으로 옮긴 저장본에서 같은 검사가 오류/경고 0으로 통과했고 수정 좌표의 Live Coding도 성공했다. 기존 실패를 제외하거나 검사 기준을 바꾸지 않았다. 이동한 엄폐물의 머리 높이 광선 8개도 재확인했다. 수정본 Hollow 재베이크·DA 저장 후 `HollowContent`·`ProductionSurfaceDataDefinitions`·`ProductionSpawnComposition`도 오류/경고 0으로 통과했다. Bastion 3개를 포함해 최종 적용 상태의 기존 자동 검사 총 6개가 성공했다. 사용자가 PIE에서 확인한 뒤 문제없음으로 외곽 엄폐 배치를 채택했다. 복구본·배치·원래 액터/충돌 검사 기록은 `Saved/OuterCover/20261108/`에 있다.

**새 테마 계획 채택(2026-11-08).** [Grove_00](phases/Phase09_GrovePrototype.md)는 거목·뿌리 사이의 지상 순환과 측면 재교전, [Ridge_00](phases/Phase09_RidgePrototype.md)는 완만한 능선·얕은 분지에서의 시야 전환과 노출 관리를 목표로 한다. 각각 제작 순서·검증 기준·독립 세션 시작 프롬프트를 기록했다. 두 옥탄트의 실제 제작과 검증은 아직 미착수다.

**프로토타입 지면 재질 분리 완료(2026-11-08).** Pyramid 지각은 전용 `M_Pyramid_Ground`로 분리하고 기존 Linear RGB `(0.19, 0.24, 0.22)`를 보존했다. Acropolis의 `M_Acropolis_Ground`는 테라코타색 sRGB `#B77F70`으로 변경·컴파일·저장했다. 두 LVI의 지면 참조 분리와 저장 상태를 재조회했다. 지형·마커·기존 풀을 보존하며 조립 스크립트도 Pyramid 전용 재질을 사용한다. LVI 패키지 해시 변경에 따라 사용자가 Pyramid DA를 재베이크·저장했다. Atlas 유효 표본이 없는 Layer 경고 17건과 지각 제외 Layer 상세 190개는 2026-10-16 V058 베이크와 시간값을 제외하고 모두 일치한다. `ProductionSurfaceDataDefinitions`·`ProductionSpawnComposition`은 실제 혼합 풀에서, `ProductionPoolCurrentAndDeterministic`은 별도 프로세스의 기존 Pyramid 단독 검사용 풀에서 성공했다. 자동 검사 3개 오류/경고 0이며 저장본 원본 해시·5개 stream이 두 번의 재베이크와 일치한다. 기존 베이크 경고를 자동 검사 경고와 구분하며 형상/격자 설정을 변경하지 않았다. 이번 재질 변경에서 새 PIE 통행·전투는 반복하지 않았다. 복구본·해시·비교 로그·현재성 검사 보고서는 `Saved/OctantGroundSeparation/20261108/`에 있다.

**Pyramid_00 V058 사용자 승인·제작 확정(2026-10-16).** 승인된 V057 실내에 얕은 불규칙 외벽 파손 3곳·외부 발판 6곳을 추가했다. 파손부 훅 32개를 중간 층끼리 왕복·층 건너뛰기 사선이 이어지도록 재배치하고 좁은 상부 착지 4곳을 보강했다. 복층 홀 26곳마다 상층 훅을 추가했으며 최상층 두 홀에는 부분 중층을 만들었다. 기존 전투 훅 23개와 합쳐 총 81개다. Pod 23개·이동판 5개는 유지한다. 경로 108개의 native 접근·출발 보행·착지와 외부 발판 보행 표본을 통과했다. 총 242,374 tris, Support 191 Layers·100.06MB(비압축)로 이전보다 약 1.05% 늘었다. 형상·반입·충돌·베이크·Meadow 경계 2,208개 일치와 혼합 정의/편성·현재성/두 번 재베이크 재현성·중력 회전 자동 검사 4개가 오류/경고 없이 통과했다. 기존 다섯 옥탄트 풀 파일 SHA256는 유지한다. 작업 폴더 의존은 0개이며 구버전 리다이렉터 15개를 복구 보관했다. 사용자가 V058 공간과 동선을 승인했다. 개별 경로의 모든 관성 조건과 성능 실측을 검증한 것으로 확대하지 않는다. 최종 독립 원본·재현 코드·검사 범위는 [Pyramid 인계](../../Art/Octants/Pyramid_00/README.md)를 따른다.

**Phase 10 종료(2026-10-13, 사용자 결정).** flow field·계층형 A* 모두 미채택이다(D-080). 지상 NPC 경로는 D-079 개선 A*(scheduler cache·앞쪽 합류·Chase 거리 순서)로 확정이며, 이미 runtime 기본값이다. 판정표·흔들림·overlay 토글 수치는 `history/Phase10_Log.md` 2026-10-13 단위 5가 소유한다.

- 두 프로토타입(`LNPNavFlowField`·`LNPNavHierarchy`), 자동화(`Nav.FlowField`·`Nav.Hierarchy`), 재생 변형(`Nav.RequestCostReplay`, overlay 토글 포함), 비교 스크립트(`Scripts/Profiling/ComparePhase10Replay.py`)는 processor에 연결하지 않은 채 남긴다.
- 길찾기 최적화가 필요해지면 flow field를 첫 후보로 다시 잰다. 미측정 조건 두 가지(먼 추격자 없는 조건, 움직이는 목표·목표 양자화)부터 본다. 재생·캡처 명령과 입력은 로그 단위 5 "채택 판정"에 있다.

이어지는 기준선과 제약:

- 성능 기준선은 [V011](phases/Phase09_PerfMemoryV011.md)이다. 혼합 풀 리슨 2P의 frame P95는 7.65~9.28ms다. 측정은 `Scripts/Profiling/RunPhase13Gate.ps1 -Cases Production`(헤드리스, seed 1)으로 한다. harness의 `exact ≤2ms` FAIL은 Gate가 아니다. 합성 추격 케이스(`N700Chase`·`N2000Chase`)는 반드시 `-IslandSlot 5`다.
- production 경로 비용은 병목이 아니다(V011 경로 tick P95 0.014~0.021ms, 큐 대기 P95 6.4ms).
- 관통 Gate는 활성 개체로 판정한다(D-078).
- 벽보행 교전 거리(`AttackRange`, 현재 800)는 사용자가 플레이 테스트로 정한다. 0~5,000 범위는 재측정하지 않아도 된다. 사거리를 늘린 뒤에는 ㄷ자 지형 회귀만 확인한다(Phase 14b).
- `Meadow_00`은 사용자가 낮은 완성도로 플레이 풀 제외 의향을 밝혔다(2026-11-08). 현재 실제 풀에서는 미제외이며 이번 계획/재질 작업에서 풀을 수정하지 않는다. 이음매·회귀 검사 원본은 보존한다. definition별 slot mask를 다르게 주려면 D-043 제약 할당이 먼저다.
- 진단 CVar 기본값은 바꾸지 않았다(Phase 9와 같음).

## 바로 다음 행동

- 새 작업 세션은 [Grove 계획](phases/Phase09_GrovePrototype.md) 또는 [Ridge 계획](phases/Phase09_RidgePrototype.md)의 시작 프롬프트로 시작한다. 같은 에디터의 레벨 전환·베이크·풀 저장을 동시에 실행하지 않는다.
- Pyramid_00은 V058 공간·동선을 보존한다. 추가 피드백이 생기면 새 형상 버전에서 해당 구간만 수정한다.

## 인계 근거

- Phase 10 최신 패키지: `Saved/SurfaceNavigationPhase10Package`(단위 2 runtime, 현재 runtime과 같음). Phase 9 기준선 원본: `Saved/Profiling/Phase09/GateV011*`. Phase 10 재생 원본: `Saved/Profiling/Phase10/Unit{1,3,4,5}`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
