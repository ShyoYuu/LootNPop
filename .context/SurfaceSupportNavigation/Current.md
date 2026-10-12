# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: 10 대규모 추격 경로 비교 — 진행 중(단위 0). 직전 Phase 9·14b 완료(2026-10-12)
> 마지막 갱신: 2026-10-13

## 현재 결정

**Phase 10 착수(2026-10-13).** 실행 문서는 [Phase 10](phases/Phase10_ChasePathComparison.md)이다. 순서는 다음과 같다.

1. 단위 0: harness 재정비
2. 단위 1·2: 목표 전환 큐 대기 원인 분석과 A* 기준선 개선
3. 단위 3: flow field
4. 단위 4: 계층형 A*
5. 단위 5: 비교 판정
6. 단위 6: 채택 시 통합

채택 기준 5개와 시나리오 3종(production 자연, 지상 700 합성 추격, 지상 2,000 합성 추격)은 실행 문서가 소유한다.

- production에서 경로 비용은 병목이 아니다. V011 경로 tick P95는 0.014~0.021ms, 큐 대기 P95는 6.4ms다. 큐 대기 문제는 전원 활성 다대소 몰림(Phase 7 합성 추격 P95 약 125ms)에서만 드러난다.
- Phase 7의 재생 CSV(`N700_chase_WindowFix*`)는 Meadow 단독 풀에서 캡처해 현재 배치와 맞지 않는다. 쓰지 않는다.

이어지는 기준선과 제약:

- 성능 기준선은 [V011](phases/Phase09_PerfMemoryV011.md)이다. 혼합 풀 리슨 2P의 frame P95는 7.65~9.28ms다. 측정은 `Scripts/Profiling/RunPhase13Gate.ps1 -Cases Production`(헤드리스, seed 1)으로 한다. harness의 `exact ≤2ms` FAIL은 Gate가 아니다.
- 관통 Gate는 활성 개체로 판정한다(D-078).
- 벽보행 교전 거리(`AttackRange`, 현재 800)는 사용자가 플레이 테스트로 정한다. 0~5,000 범위는 재측정하지 않아도 된다. 사거리를 늘린 뒤에는 ㄷ자 지형 회귀만 확인한다(Phase 14b).
- `Meadow_00`은 풀에 유지하고, 재제작은 옥탄트 양산 단계에서 한다. definition별 slot mask를 다르게 주려면 D-043 제약 할당이 먼저다.
- 진단 CVar 기본값은 바꾸지 않았다(Phase 9와 같음).

## 진행 방식(2026-10-13 사용자 요청)

구현 단위마다 새 세션으로 넘기며 계속 진행한다. 단위마다 검증이 끝나면 커밋한다. 사용자 확인이 필요한 경우는 셋뿐이다.

- 채택 판정 같은 사용자 소유 결정
- 에디터 종료·PIE 같은 사용자 조작
- 계획 범위 밖 변경

## 바로 다음 행동

- **단위 0 완료**(2026-10-13). 캡처 3종과 재생 기준값은 `history/Phase10_Log.md` 2026-10-13이 소유한다. 큐 대기 P95: Production 6.4ms, N700Chase 93ms, N2000Chase 986ms.
  - 캡처 명령: `RunPhase13Gate.ps1 -Cases @('N700Chase','N2000Chase') -Repeats 1 -IslandSlot 5 -PackageDirectory Saved/SurfaceNavigationPhase10Package -OutputDirectory <폴더>`. 합성 추격은 반드시 `-IslandSlot 5`로 실행한다.
  - 재생 명령: 로그의 `Nav.RequestCostReplay` 실행 인자를 따른다. 입력 CSV 옆에 `_Meta.txt`가 있어야 한다.
- **단위 1(원인 분석, 계측만)**
  1. scheduler tick 기록에 대기 수·시작 수·차감 예산·scratch 점유·시작을 막은 사유를 추가한다(캡처 중에만).
  2. H1~H5를 정량화한다. `Saved/Profiling/Phase10/Capture`의 기존 캡처와 재생 `*_Requests.csv`를 먼저 활용한다.
  3. 실제 플레이 몰림 원인(Tile 경계·drift 재요청, Pod 깨움, 타겟 교체, overlay 재계획)별로 발생 횟수를 센다.

## 인계 근거

- Phase 9 최신 패키지: `Saved/SurfaceNavigationPhase9V011BPackage`. 측정 원본: `Saved/Profiling/Phase09/GateV011*`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
