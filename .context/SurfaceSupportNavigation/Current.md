# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: 옥탄트 양산 — Pyramid_00 제작·검증. Roadmap의 기존 Phase는 모두 완료(직전 Phase 10 완료 2026-10-13)
> 마지막 갱신: 2026-10-16

## 현재 결정

**Pyramid_00 V058 사용자 승인·제작 확정(2026-10-16).** 승인된 V057 실내에 얕은 불규칙 외벽 파손 3곳·외부 발판 6곳을 추가했다. 파손부 훅 32개를 중간 층끼리 왕복·층 건너뛰기 사선이 이어지도록 재배치하고 좁은 상부 착지 4곳을 보강했다. 복층 홀 26곳마다 상층 훅을 추가했으며 최상층 두 홀에는 부분 중층을 만들었다. 기존 전투 훅 23개와 합쳐 총 81개다. Pod 23개·이동판 5개는 유지한다. 경로 108개의 native 접근·출발 보행·착지와 외부 발판 보행 표본을 통과했다. 총 242,374 tris, Support 191 Layers·100.06MB(비압축)로 이전보다 약 1.05% 늘었다. 형상·반입·충돌·베이크·Meadow 경계 2,208개 일치와 혼합 정의/편성·현재성/두 번 재베이크 재현성·중력 회전 자동 검사 4개가 오류/경고 없이 통과했다. 기존 다섯 옥탄트 풀 파일 SHA256는 유지한다. 작업 폴더 의존은 0개이며 구버전 리다이렉터 15개를 복구 보관했다. 사용자가 V058 공간과 동선을 승인했다. 개별 경로의 모든 관성 조건과 성능 실측을 검증한 것으로 확대하지 않는다. 최종 독립 원본·재현 코드·검사 범위는 [Pyramid 인계](../../Art/Octants/Pyramid_00/README.md)를 따른다.

**Phase 10 종료(2026-10-13, 사용자 결정).** flow field·계층형 A* 모두 미채택이다(D-080). 지상 NPC 경로는 D-079 개선 A*(scheduler cache·앞쪽 합류·Chase 거리 순서)로 확정이며, 이미 runtime 기본값이다. 판정표·흔들림·overlay 토글 수치는 `history/Phase10_Log.md` 2026-10-13 단위 5가 소유한다.

- 두 프로토타입(`LNPNavFlowField`·`LNPNavHierarchy`), 자동화(`Nav.FlowField`·`Nav.Hierarchy`), 재생 변형(`Nav.RequestCostReplay`, overlay 토글 포함), 비교 스크립트(`Scripts/Profiling/ComparePhase10Replay.py`)는 processor에 연결하지 않은 채 남긴다.
- 길찾기 최적화가 필요해지면 flow field를 첫 후보로 다시 잰다. 미측정 조건 두 가지(먼 추격자 없는 조건, 움직이는 목표·목표 양자화)부터 본다. 재생·캡처 명령과 입력은 로그 단위 5 "채택 판정"에 있다.

이어지는 기준선과 제약:

- 성능 기준선은 [V011](phases/Phase09_PerfMemoryV011.md)이다. 혼합 풀 리슨 2P의 frame P95는 7.65~9.28ms다. 측정은 `Scripts/Profiling/RunPhase13Gate.ps1 -Cases Production`(헤드리스, seed 1)으로 한다. harness의 `exact ≤2ms` FAIL은 Gate가 아니다. 합성 추격 케이스(`N700Chase`·`N2000Chase`)는 반드시 `-IslandSlot 5`다.
- production 경로 비용은 병목이 아니다(V011 경로 tick P95 0.014~0.021ms, 큐 대기 P95 6.4ms).
- 관통 Gate는 활성 개체로 판정한다(D-078).
- 벽보행 교전 거리(`AttackRange`, 현재 800)는 사용자가 플레이 테스트로 정한다. 0~5,000 범위는 재측정하지 않아도 된다. 사거리를 늘린 뒤에는 ㄷ자 지형 회귀만 확인한다(Phase 14b).
- `Meadow_00`은 풀에 유지하고, 재제작은 옥탄트 양산 단계에서 한다. definition별 slot mask를 다르게 주려면 D-043 제약 할당이 먼저다.
- 진단 CVar 기본값은 바꾸지 않았다(Phase 9와 같음).

## 바로 다음 행동

- Pyramid_00은 V058로 확정했다. 최종 Blender 원본·참조 에셋·배치·검사/반입 코드를 채택하고 구버전·중간 작업물은 Git 제외 `_WorkInProgress`로 분리했다. 추가 피드백이 생기면 새 버전에서 해당 구간만 수정한다. 새 옥탄트나 Roadmap Phase는 아직 정의하지 않았다.

## 인계 근거

- Phase 10 최신 패키지: `Saved/SurfaceNavigationPhase10Package`(단위 2 runtime, 현재 runtime과 같음). Phase 9 기준선 원본: `Saved/Profiling/Phase09/GateV011*`. Phase 10 재생 원본: `Saved/Profiling/Phase10/Unit{1,3,4,5}`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
