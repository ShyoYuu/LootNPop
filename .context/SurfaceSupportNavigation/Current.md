# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: 10 대규모 추격 경로 비교 — 진행 중(단위 0·1·2·3·4 완료, 다음 단위 5 비교·채택 판정). 직전 Phase 9·14b 완료(2026-10-12)
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

- **단위 0~4 완료**(2026-10-13). 수치·판정은 `history/Phase10_Log.md` 2026-10-13이 소유한다.
  - 큐 대기 몰림은 5초 목표 전환이고 원인은 예산 처리량 한계다(단위 1). A* 기준선은 D-079 `improved`다(단위 2).
  - 단위 3 flow field(`LNPNavFlowField`, 반경 12,000, burst마다 field 버림): 기준 2만 미충족이다(burst P95 S3 −47%, S2 8→9).
  - 단위 4 계층형 A*(`LNPNavHierarchy`, Cluster = Tile, span 8 Portal, HPA* segment 정제, scheduler와 같은 cache 앞단). 예비 판정은 기준 1·2·3 미충족, 4·5 충족이다.
    - 기준 1: tick 합 S2 −18%·S3 −27%
    - 기준 2: burst P95 S2 8→8·S3 43→33
    - 기준 3: 보행 비율 P95 1.09~1.10
    - 기준 4: 7.47MiB
    - 기준 5: Pod당 dirty 1~3 Cluster
    - corridor 정제는 비율 1.015~1.027을 지키지만 CPU가 improved보다 9~13% 많다.
- **단위 5(비교와 채택 판정)**: 실행 문서 단위 5 절을 따른다. 채택 판정은 사용자 소유 결정이다. 결과를 표로 만든 뒤 사용자에게 확인을 받는다.
  1. 기존 재생 결과로 기준 1~4 비교표를 확정한다. 재생 결과는 `Saved/Profiling/Phase10/Unit4/Replay`(세 방식이 한 실행에 들어 있음)이고, 표는 `Saved/Profiling/Phase10/Unit4/Compare.md`다. 같은 실행 안의 improved와 비교하고, 필요하면 같은 명령으로 1~2회 더 돌려 흔들림을 본다.
  2. overlay 토글 재생으로 기준 5를 잰다. 현재 재생기는 overlay를 넣지 않는다(scheduler Tick·스냅·`BuildFieldPath`에 nullptr, Tile revision 0). 다음이 필요하다.
     - 캡처 창에는 overlay 변경이 없다(단위 1). 그래서 Pod 위치를 합성한다. 예: 목표 4점 반경 60m 안 지각 node에서 결정론 seed로 고른다.
     - burst마다 그 Pod들을 `LNPNavOverlay::BuildPodOverlay`로 켜고 끈다. 이전 overlay를 넘겨 revision을 이어 간다.
     - 세 방식 모두에 overlay를 넘긴다.
       - A*: `RequeueInvalidatedPaths`
       - flow field: 바뀐 Tile key가 `BlockByTile`에 있는 field만 다시 짓는다. 국소 갱신은 단위 3 설계 메모다.
       - 계층형: `LNPNavHierarchy::UpdateOverlay`의 dirty Cluster·확정 수를 예산 단위로 보고한다.
     - 재계산 범위(바뀐 Tile·dirty Cluster·영향 field·재계획 수)와 그 비용을 표로 낸다.
  3. 결과를 `history/Phase10_Log.md`와 `design/GroundNavigation.md`에 쓰고 `Decisions.md`에 새 결정 한 줄을 더한다. 미채택이면 Phase 10은 비교 자료와 단위 2 개선으로 끝난다(단위 6 없음).
  - 입력: `Saved/Profiling/Phase10/Unit1/{Production,N700Chase,N2000Chase}_Repeat1_NavRequests.csv`(옆에 `_Meta.txt` 필수).
  - 재생: `UnrealEditor-Cmd.exe LootNPop.uproject -unattended -nullrhi -nosound -corelimit=4 -ExecCmds="Automation RunTests LootNPop.SurfaceNavigation.Nav.RequestCostReplay" -TestExit="Automation Test Queue Empty" -LNPNavReplayCsv=<입력 CSV> -LNPNavReplayOutput=<폴더>/<케이스>_RequestCosts.csv`.
    - 변형 33개에서 케이스당 약 30~50초다. 계층 빌드는 span마다 한 번, tick 밖에서 한다.
    - bash에서는 경로를 슬래시로 쓴다(역슬래시 바로 뒤의 `$변수`는 확장되지 않는다).
  - 비교표: `python Scripts/Profiling/ComparePhase10Replay.py <재생 폴더>`.
    - burst = 묶음 ≥400 요청의 doneTick, 비율 = 같은 행 base 대비 `walk`, tick 합 = 경로 CPU 총합 근사.
    - `source`는 1 field·3 계층형 응답, 2 폴백이다.
    - `_FlowFields.csv`·`_Hierarchy.csv`가 있으면 요약 표도 낸다.
  - 기준값 3종(Unit1 입력 재생, 같은 실행, `Saved/Profiling/Phase10/Unit4/Compare.md`):

| 지표 | improved S2 / S3 | flowfield S2 / S3 | hierarchy S2 / S3 |
|:---|:---|:---|:---|
| burst 완료 tick P50/P95/최대 | 4/8/14 / 10/43/93 | 5/9/13 / 9/23/65 | 4/8/11 / 9/33/51 |
| tick 합(ms) | 229.5 / 816.3 | 144.6 / 488.6 | 188.7 / 593.6 |
| tick P95(us) | 285 / 999 | 74 / 957 | 179 / 947 |
| 확장 합(+ field 확정·link 확정) | 582,172 / 2,617,091 | 217,816 / 962,761 | 439,069 / 1,601,151 |
| 보행 길이 비율 P95 | 1.013 / 1.024 | 1.000 / 1.000 | 1.100 / 1.091 |
| 추가 메모리 | — | 0.39MiB | 7.47MiB(계층 4.47 + 고수준 scratch 3.00) |

  - 채택 기준 1·2의 분모는 improved다. tick 합·tick P95는 실행마다 수 % 흔들린다(S2 improved 213.9 → 229.5). S1은 Chase가 없어 세 방식이 요청별로 같다.
  - 계층형 변형은 다음과 같다. cache 없는 계층형은 improved보다 비싸다.
    - `hierarchy`: segment·span 8·cache
    - `hierarchySerial`
    - `hierarchyNoCache`
    - `hierarchyCorridor`·`hierarchyCorridorRing0`·`hierarchyCorridorNoCache`
    - `hierarchySpan4`·`hierarchySpan16`
    - `hierarchyExactStart`·`hierarchyKeep`
- 캡처가 다시 필요하면: `RunPhase13Gate.ps1 -Cases @('N700Chase','N2000Chase') -Repeats 1 -IslandSlot 5 -PackageDirectory Saved/SurfaceNavigationPhase10Package -OutputDirectory <폴더>`.
  - 합성 추격은 반드시 `-IslandSlot 5`다.
  - S2·S3 frame P95는 같은 runtime에서도 실행마다 ±2ms(S2 8.3~10.5) 흔들린다.
  - 단위 1 분석은 `Scripts/Profiling/AnalyzePhase10QueueWait.py <캡처> <재생> <출력.json>`.

## 인계 근거

- Phase 10 최신 패키지: `Saved/SurfaceNavigationPhase10Package`(단위 2 runtime). Phase 9 기준선 원본: `Saved/Profiling/Phase09/GateV011*`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
