# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: 10 대규모 추격 경로 비교 — 진행 중(단위 0·1·2·3 완료, 다음 단위 4 계층형 A*). 직전 Phase 9·14b 완료(2026-10-12)
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

- **단위 0·1·2·3 완료**(2026-10-13). 수치·판정은 `history/Phase10_Log.md` 2026-10-13이 소유한다.
  - 큐 대기 몰림은 5초 목표 전환이고 원인은 예산 처리량 한계다(단위 1). A* 기준선은 D-079 `improved`다(단위 2).
  - 단위 3 flow field(`LNPNavFlowField`, 반경 12,000, burst마다 field 버림): improved 대비 tick 합 S2·S3 −38%, burst 완료 tick P95 S2 8→9·S3 43→23(−47%), 보행 비율 P95 1.000, field 최대 0.39MiB. 기준 2만 미충족이다. burst 꼬리는 반경 밖 A* 폴백이고, S2는 요청당 고정비(스냅 + 응답 경로 조립)가 improved보다 크다.
  - Nav edge는 역방향 대칭이다(역방향 없음·비용 불일치 0). 목표에서 역으로 펴는 탐색은 정방향 이웃 목록을 그대로 쓸 수 있다.
- **단위 4(계층형 A* 프로토타입)**: 실행 문서 단위 4 절을 따른다. `LNPNavHierarchy.{h,cpp}`는 순수 함수이며 processor에 연결하지 않는다.
  1. Cluster = Layer별 16×16 Tile(기존 `TileId`). 경계 span을 Portal로 압축하고, Cluster 내부 Portal 간 비용을 사전 계산한다. seam link·Layer portal은 고수준 edge다.
  2. query는 고수준 A* → corridor 안 저수준 A*다. 고수준 탐색·corridor 탐색 모두 확장 수를 같은 예산 단위로 센다. 사전 계산 비용·메모리는 따로 보고한다(기준 4: 추가 ≤ 8MiB).
  3. Pod overlay가 바뀌면 해당 Cluster만 dirty로 다시 계산한다(기준 5). 단위 5 overlay 토글 재생을 위해 dirty Cluster 수·재계산 비용을 셀 수 있게 둔다.
  4. `Nav.RequestCostReplay`에 `hierarchy` 변형을 추가한다. 단위 3의 `FFlowFieldReplay`처럼 테스트 코드 드라이버로 붙이고, 요청 결과는 같은 `_Requests.csv` 열(`walk`, `source` 포함)을 채운다. 결정론 직렬 대조 변형도 둔다.
  - 입력: `Saved/Profiling/Phase10/Unit1/{Production,N700Chase,N2000Chase}_Repeat1_NavRequests.csv`(옆에 `_Meta.txt` 필수). 단위 2 캡처(`Unit2/Capture`)가 아니라 이 입력을 계속 쓴다.
  - 재생: `UnrealEditor-Cmd.exe LootNPop.uproject -unattended -nullrhi -nosound -corelimit=4 -ExecCmds="Automation RunTests LootNPop.SurfaceNavigation.Nav.RequestCostReplay" -TestExit="Automation Test Queue Empty" -LNPNavReplayCsv=<입력 CSV> -LNPNavReplayOutput=<폴더>/<케이스>_RequestCosts.csv`. 변형 23개에서 케이스당 약 20~40초다. bash에서는 경로를 슬래시로 쓴다(역슬래시 바로 뒤의 `$변수`는 확장되지 않는다).
  - 비교표: `python Scripts/Profiling/ComparePhase10Replay.py <재생 폴더>`. burst = 묶음 ≥400 요청의 doneTick, 비율 = 같은 행 base 대비 `walk`, tick 합 = 경로 CPU 총합 근사. `_FlowFields.csv`가 있으면 field 요약 표도 낸다.
  - 기준값(Unit1 입력 재생, `Saved/Profiling/Phase10/Unit3/Compare.md`):

| 지표 | improved S1 / S2 / S3 | flowfield S2 / S3 |
|:---|:---|:---|
| burst 완료 tick P50/P95/최대 | — / 4/8/14 / 10/43/93 | 5/9/13 / 9/23/65 |
| tick 합(ms) | 11.9 / 213.9 / 755.7 | 132.6 / 466.4 |
| tick P95(us) | 89 / 259 / 895 | 69 / 893 |
| 확장 합(flowfield는 + field 확정) | 9,746 / 582,172 / 2,617,091 | 217,816 / 962,761 |
| 보행 길이 비율 P95 | 1.000 / 1.013 / 1.024 | 1.000 / 1.000 |

  - 채택 기준 1·2의 분모는 improved다. tick 합·tick P95는 실행마다 수 % 흔들리므로 같은 실행 안의 improved와 비교한다. S1은 Chase가 없어 경로 방식 차이가 Background 처리에만 나타난다.
- 캡처가 다시 필요하면: `RunPhase13Gate.ps1 -Cases @('N700Chase','N2000Chase') -Repeats 1 -IslandSlot 5 -PackageDirectory Saved/SurfaceNavigationPhase10Package -OutputDirectory <폴더>`. 합성 추격은 반드시 `-IslandSlot 5`다. S2·S3 frame P95는 같은 runtime에서도 실행마다 ±2ms(S2 8.3~10.5) 흔들린다. 단위 1 분석은 `Scripts/Profiling/AnalyzePhase10QueueWait.py <캡처> <재생> <출력.json>`.

## 인계 근거

- Phase 10 최신 패키지: `Saved/SurfaceNavigationPhase10Package`(단위 2 runtime). Phase 9 기준선 원본: `Saved/Profiling/Phase09/GateV011*`.
- 로컬 측정 산출물은 Git에 넣지 않는다.
