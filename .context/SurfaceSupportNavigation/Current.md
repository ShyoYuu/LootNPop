# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: 없음 — Roadmap의 모든 Phase 완료(직전 Phase 10 완료 2026-10-13). 다음 작업은 사용자 결정 대기
> 마지막 갱신: 2026-10-13

## 현재 결정

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

- Roadmap에 남은 Phase가 없다. 다음 작업(예: 옥탄트 양산, 새 Phase 정의)은 사용자가 정한다. 새 Phase를 정하면 `Roadmap.md`에 행을 추가하고 실행 문서를 착수 직전에 만든다.

## 인계 근거

- Phase 10 최신 패키지: `Saved/SurfaceNavigationPhase10Package`(단위 2 runtime, 현재 runtime과 같음). Phase 9 기준선 원본: `Saved/Profiling/Phase09/GateV011*`. Phase 10 재생 원본: `Saved/Profiling/Phase10/Unit{1,3,4,5}`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
