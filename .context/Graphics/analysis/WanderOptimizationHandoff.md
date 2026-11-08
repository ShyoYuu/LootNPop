# 배회 프로세서 최적화 인계

> 상태: 분석 인계 · 최적화 미구현
> 갱신: 2026-11-08
> 목적: 사용자가 다른 세션에서 배회 프로세서 자체를 최적화할 때 재조사 없이 출발할 근거
> 범위: 현재 코드 구조, 패키지 진단 관측, 개선 후보, 미확정 비용, 검증 조건

## 1. 인계 결론과 현재 작업 상태

확인된 순간 부하는 **기한 만료의 프레임 집중 → 최대4회 후보 검사 → 많은 선택 실패와 반복 충돌 질의** 경로다. 최신 진단에서 만료 상위20개 프레임은 약3초 간격으로 반복되고 Wander 평균24.2314ms를 기록했다. 나머지2958개 프레임 평균은1.9387ms다.

재시도 시점 분산은 프레임 집중을 완화할 후보지만, 총 검사량·후보 실패율·매 프레임의 상시 비용을 함께 줄이는 해법은 아니다. 사용자는 분산 외에도 프로세서 전반을 다른 세션에서 최적화하기로 했다. 이 문서는 특정 최적화 설계를 확정하거나 구현을 완료한 문서가 아니다.

- 게임플레이 최적화는 아직 하지 않았다. 추가한 것은 기본 비활성 진단 계수와CSV 후처리의 메타데이터 경계 처리다.
- 최신 코드 확인 시 HEAD는 `143048ccf125d7121aa75ad7e2900f530c6cf0e9`이고 변경 중인 작업 트리다. 진단 패키지의 `PackagePreflight/SourceCommit.txt`도 같은 HEAD를 기록한다. 커밋만으로 현재 계측 코드를 재현할 수 없으므로 아래 패치·파일 해시도 확인한다.
- 계측된 `LNPEnemyWanderProcessor.cpp` SHA256은 `E67DAC36B1F6F7DDE5F794BA4610F2AD0D7EEE260EA194EBF2E5467F764224A5`다. 이후 줄 번호가 바뀌면 함수·분기를 기준으로 찾는다.
- CPU 트레이스 두 실행의 실제 종료 코드는 모두777003(`CrashReporterCrashed`)이다. 하네스complete·읽을 수 있는CSV/트레이스와 정상 프로세스 종료는 구분한다. 원인은 미확정이다.
- 모든 진단은 `DiagnosticOnly=true`·`BaselineEligible=false`다. 기존 정식3회 Medium GT 편차7.2288%/허용5% 미통과와 Phase0 미완료 상태는 유지한다.

## 2. 먼저 볼 코드와 실행 구조

| 코드 | 확인 위치·역할 |
|:---|:---|
| [LNPEnemyWanderProcessor.cpp](../../../Source/LootNPop/Enemy/LNPEnemyWanderProcessor.cpp) | 중심 구현. GT 전용 설정35행, 예약/구성원 수집93행, 정렬114행, 재선택178행, 후보220행, 지상 경로263행, 예약 선분283행, 적용349행 |
| [LNPEnemySurfaceMovement.cpp](../../../Source/LootNPop/Enemy/LNPEnemySurfaceMovement.cpp) | `TryResolveCachedGround`185행. Surface 정체성·Generation과 Support Atlas의 HighConfidence 확인 |
| [LNPSurfaceCrawl.cpp](../../../Source/LootNPop/Enemy/LNPSurfaceCrawl.cpp) | `ValidateWanderSegment`737행. 구형Sweep와 경로 표면 부착 검사 |
| [LNPMassWorldCollision.cpp](../../../Source/LootNPop/SurfaceNavigation/LNPMassWorldCollision.cpp) | `SweepSphereWorld`114행/`SweepCapsuleWorld`121행. 계측된 충돌 질의 래퍼 |
| [LNPWanderReplication.cpp](../../../Source/LootNPop/Replication/LNPWanderReplication.cpp) | `Encode`/`ApplyTo`. 서버도 양자화된 구간을 다시 적용해 사용 |
| [측정 규약](../design/Measurement.md) | 진단 계수의 정의·CSV 읽기·시각/프레임 대응 절차 |

`Execute`는 Server/Standalone에서 GT에 실행된다. 첫 순회에서 Pod별 `Members`·`Reservations`를 매번 구성하고 구성원 Entity.Index를 정렬한다. 이 수집은 이후의 Wander·안전 조건 필터보다 먼저 수행된다. 두 번째 순회에서 개체 상태를 검사하고, 해당하면 기존 구간 적용 또는 재선택을 수행한다.

재선택의 흐름은 다음과 같다.

```text
구간 무효 또는 Now >= StopUntil
  → 같은 Pod의 현재 위치와 초기 겹침 확인
  → 현재 위치에서 정지하는 기본 구간 생성(StopUntil = Now + 3초)
  → 최대4개 목표 후보 검사
      비행: 이동 선분 Sphere Sweep
      벽보행: 짧은 접평면 경로와 표면 부착 검사
      지상: 목표 표면 투영 → Nav 연결 → 경로 표본별 표면/레이어/Capsule 검사
      공통: 다른 개체의 예약 선분과 겹침 → 타일 리비전 목록 확인
  → 성공하면 목표·도착/정지 기한 교체, 전부 실패하면 기본 정지 구간 유지
  → 복제 코덱 Encode → 양자화 결과를 서버 구간에 ApplyTo
```

성공한 지상/비행의 정지는 도착 후3초, 벽보행은9초다. 전부 실패한 경우에는 이동 없이 기본3초 뒤 재선택한다. 같은 프레임의 실패들은 유사한 다음 기한을 갖는 구조다. 다만 동일 개체들이 반복 실패했는지는ID별 이력이 없어 미확정이다.

## 3. 측정으로 확인한 것

공통 조건은 패키지 Development·Medium 초기 적2223명·시드1·Revision3 카메라·180초 워밍업/60초 캡처다. 전투 부하가 아닌 배회 관찰이다. 서로 다른 패키지·진단 설정의 두 실행을 전후 성능 비교로 취급하지 않는다.

### 3.1 최신 계수·이벤트 연결 진단

원자료 루트: `Saved/Profiling/Graphics/Phase00/WanderDiagnosticsTrace_20261108_041500_fa2a4b/`

사용자 확인 캡처 로그 시각은185.032992초, 끝은245.032992초다. GT 프레임/Wander 이벤트는 앞뒤0.1초를 더해 수동 내보냈다. CSV2978개 행 모두 `시작Time + PathSeconds`의 GT 프레임 포함 관계로 연결됐고, 각 프레임의 Wander가정확히1회다. 이벤트 나열 순서만으로 연결하지 않았다.

첫CSV 프레임의 Wander는185.028342~185.029988초로 시작 로그보다 앞선다. 따라서 CSV 프레임 기준은2978회·총6.219306초이고, 로그 시각 구간에 완전히 든 이벤트는2977회·6.217660초다. 첫 실행1.646ms 차이를 누락/중복으로 혼동하지 않는다.

| CSV 프레임 집단 | 프레임 수 | Expired 평균 | 후보 시도 평균 | Accepted 평균 | Failed 평균 | Wander 평균 ms | GT 평균 ms |
|:---|---:|---:|---:|---:|---:|---:|---:|
| 기한 만료 상위20개 | 20 | 103.75 | 411.95 | 1.25 | 102.50 | 24.2314 | 32.9936 |
| 나머지 | 2958 | 2.7052 | 7.6139 | 1.5325 | 1.3871 | 1.9387 | 10.4406 |

- 만료 상위20개의 월드 시각 간격:3.0004~3.0193초. Wander 시간 상위20개와19개가 겹친다. 전체 Wander 시간의7.7923%다.
- 전체 프레임의 후보 시도/Expired/Failed와 Wander 시간의 Pearson 상관:0.9385/0.9127/0.9283. 후보 시도를 앞뒤 한 프레임의 Wander와 잘못 대응시키면-0.0089/-0.0035로 낮아진다.
- 실행 시간 P50/P95/P99/최대:1.646/2.994/15.838/29.543ms(nearest-rank).
- 최댓값:200.710157~200.739700초, CaptureIndex778(0부터), Expired105·후보416·Failed103·GT38.2768ms.
- 5/10/16.6ms 이상 실행:76/53/24회. 16.6ms 이상에는 만료 상위20개 외에4개가 더 있다. 가장 큰 집중만으로 모든 고비용 실행을 설명하지 않는다.
- 계수 합: Invalid681+Expired10077−OverlapRejected47 = Accepted4558+Failed6153 = 10711. 후보 시도30761회. CodecRejected는0이다.

계수 합은 **개체-실행 합계**이며 고유 개체 수가 아니다. `Failed`는4개 후보 중 하나도 채택하지 못했다는 뜻이며 복제 인코딩 거부와 다르다. 실패 사유별·적 종류별·개체ID별 분류는 없다.

### 3.2 이전 충돌 질의 분포 진단

원자료 루트: `Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/`

측정 로그 구간184.502898~244.502898초에서 GT 프레임2983개/Wander2982회·Sphere79413회·Capsule215378회의 포함 관계를 확인했다. 이 실행에는 최신 재선택 계수가 없다.

| 집단 | Wander 실행 수 | 실행당 평균 충돌 질의 수 |
|:---|---:|---:|
| Wander5ms 미만 | 2886 | 55.05 |
| Wander5ms 이상 | 96 | 1415.69 |

고비용 실행에서 질의 수가 약25.7배 많았다. 질의 한 번의 평균 시간은 더 높아지지 않았다. 이 자료는 **질의 횟수 증가**를 뒷받침하며 충돌 엔진의 한 질의가 갑자기 느려졌다는 근거는 아니다.

최대 실행203.296430~203.323773초·27.343ms에는 Capsule3954회와 Sphere292회가 포함된다. 누적 시간은 Capsule 약12ms·Sphere 약2ms·그 외 약13ms다. 이벤트CSV의 시각 반올림이 누적되므로 세부 시간은 근사치다.

전체 구간도 Wander6.4767초 중 두Sweep 타이머 밖의 잔여 시간이약4.8418초로 약75%다. 이 잔여 시간을 맵 생성·정렬·표면 투영·Nav 검사·예약 검사 각각에 귀속하지 못했다. 재선택이 거의 없는 프레임에도 상시 비용이 있다는 점과, 재선택 순간 부하를 함께 다뤄야 한다.

## 4. 재시도 분산 외의 최적화 검토 지점

아래는 **코드에서 확인한 작업 구조와 검토 후보**다. 별도 구간별 타이밍·호출 수를 측정하지 않은 항목의 절감량이나 우선순위는 확정하지 않았다. 기존 검사가 지키는 이동 안전성을 유지하며 실제 기여도를 확인한다.

| 지점 | 현재 구조·낭비 가능성 | 다음 세션에서 확인할 것 |
|:---|:---|:---|
| 매 프레임 맵·배열 생성 | 지역 `Members`/`Reservations`를 전체 대상 첫 순회에서 구성하고 Pod별 정렬 | 구성/정렬 시간과 할당량. 불변 멤버십 재사용·저장 공간 재사용의 이익 및 생성/삭제/Pod 변경 무효화 |
| 반복 Pod 탐색 | 재선택 때 `Group.IndexOfByKey`로 부채꼴 인덱스를 찾고 예약 배열을 반복 순회 | Pod별 인원 분포와 선형 탐색 비용. 전역 전체 적의 제곱 비용이라고 단정하지 않는다 |
| 다중 경로 표본 | 지상 후보의 `Steps = ceil(거리 / max(반지름×0.5,1))`. 표본마다 Support Atlas 조회와Capsule Sweep | 후보 길이·Steps·실제 도달 단계·거부 단계의 분포. 표본 밀도 변경 시 얇은 장애물/곡면/표면 경계 안전성 |
| 표면 거부 뒤에도Sweep | 271행에서 표면/레이어 검사가 실패해도275행 `bClear &= !SweepCapsuleWorld(...)`의 우변은 평가됨 | 이 경로의 발생 횟수. 이미거부된 표본의 추가 충돌 질의를 생략할 수 있는지. 현재 발생 비용은 미계측 |
| 겹침 검사 끝까지 순회 | 초기 `bInitiallyOverlapping |= ...` 및 후보 `bOverlap |= ...`는 겹침 발견 후에도 Pod 예약 배열을 끝까지 확인 | 최초 겹침 발견 위치·나머지 반복 수. 뒤 반복에 필요한 부수 효과가 없는지 검토 |
| 늦은 후보 거부 | 지상 경로를 검사한 뒤 공통 예약 선분 겹침을 확인. 늦게 거부되면 해당 후보의 앞선 작업이 소모됨 | 실패 사유·거부 단계별 비용. 값싼 사전 기각이나 검사 순서 변경 가능성과 동일한 판정 보존 |
| 후보당 임시Nodes | 후보 반복 안에서 `TArray<int32> Nodes`를 만들고 지상Nav 경로를 수집 | 실제 할당·노드 수·재사용 이익. 빈TArray 생성만으로 항상 힙 할당한다고 가정하지 않는다 |
| 현재 위치의 표면 조회 | 지상은 구간 적용 때도 `TryResolveCachedGround`를 실행. 정지한 실패 구간도 통과 | 이동/정지별 호출 수·비용과 캐시 가능성. Generation·Overlay 리비전·동적 지형 변화 무효화 |
| 동적 지형 위험 판별 | 개체마다 `DynamicFrame->Supports`를 순회하며 해당 구역 위험을 판별 | 실제 Supports 수와 대상 수. Execute 단위의 공통 정보 재사용 가능성 및 구면 slot 판정 보존 |
| 반복 실패의 총량 | 최대4개 후보를 매번 검사하고 실패해도 이후 같은 방식 재시도 | 표면/연결/충돌/예약/타일별 거부 수와 비용, 적 종류·Pod·거리별 편향. 실패 자체를 줄이는 후보 생성·재사용 가능성 |
| GT 직렬 실행 | GT 전용이며 공유 예약의Goal을 순차 갱신하고 월드 충돌 질의를 수행 | 읽기 전용 준비 작업과 GT 의존 작업의 경계. 단순병렬화가 기존 선택 순서·예약 일관성을 보존하는지 |

특히 표면 검사 뒤의 `&=`는 단락 평가가 아니다. 이미 `bClear=false`여도 Sweep을 호출한다는 것은 코드로 확인했다. 그것이 전체 비용에서 차지하는 비중은 아직 확인하지 않았다.

재시도 분산으로 기대하는 것은 큰프레임 비용의 평준화다. 평균 실행 시간·총 후보 수·총 충돌 질의 수를 줄이는 목표는 위 항목과 별도로 검증한다. 실패 재시도 간격을 늘려 처리량이 줄어든 것을 동일 행동의 계산 효율 향상으로 해석하지 않는다.

## 5. 아직 모르는 것과 보존해야 할 동작

미확정 항목은 다음과 같다.

- 후보 거부의 세부 사유·거부 단계·지상/비행/벽보행별 비용과 개체별 반복 실패 이력.
- 맵·정렬·Support Atlas·Nav·예약 검사·코덱·Transform별 실제 시간/할당량.
- 초기 실패가 언제 어떻게 동기화됐는지, 정식3회 Medium GT 편차의 원인이 무엇인지.
- 순수CPU 실행 시간과OS 선점의 비중. 현재 이벤트는 벽시계 시간이다.

현재 안전성·복제 계약을 바꿀 때는 해당 변경을 명시하고 검증한다.

- 구면 Up과 표면의 slot/layer/generation 정체성, Overlay 타일 리비전, 동적 지형 위험 시 정확 충돌 확인.
- 벽보행의 표면 법선·짧은 이동·정지 규칙, 지상/비행의 서로 다른 검사.
- 같은 Pod의 현재 위치/예약 선분 검사. 먼저 선택한 개체가 `Reservations`의Goal을 갱신하므로 이후 선택과의 순차 의존성이 있다.
- 재선택 난수 소비 순서와 후보 선택 결과. 캐시·기각 순서·병렬화가 배치/행동 재현성에 영향을 줄 수 있다.
- 서버도 `Encode` 후 `ApplyTo`한 양자화 구간을 사용한다. 코덱은 좌표int16 범위·이동/정지uint16 밀리초 범위를 검사한다. 재시도 기한 변경도 이 경로를 통과한다.
- 피격·전투·안전 상태 이탈 시 구간 무효화와Active 전환, 클라이언트의 복제 구간 재생, 공격/이동 프로세서 순서.

기존 검증 코드는 `Source/LootNPop/Tests/`의 `LNPWanderReplicationTest.cpp`, `LNPEnemySurfaceMovementTest.cpp`, `LNPSurfaceCrawlTest.cpp`, `LNPSurfaceCrawlReplicationTest.cpp`에 있다. 관련 테스트 이름은 `LootNPop.SurfaceNavigation.WanderReplication.*`, `EnemyMovement.*`, `SurfaceCrawl.*`다. 이 인계 단위에서 이를 새로 실행하지는 않았다. 기존 테스트가 모든 배회 후보 선택 동작을 검증한다고 보장하지 않는다.

## 6. 원자료·재현 시 주의할 구간

이 문서의 경로는 프로젝트 루트 기준이다. `Saved/`는 git 제외 로컬 자료라 다른PC에는 없을 수 있다. 원자료가 없는 환경에서 임의로 긴 실행을 시작하지 않고 필요한 재현 범위를 사용자와 정한다.

| 자료 | 위치·주요 파일 |
|:---|:---|
| 이전GT·작업 관계 | `Saved/Profiling/Graphics/Phase00/MediumCPUTrace_20261107_102250_f65aca/`의 `CpuCostBreakdown.json`, `WaitCallPathObservation.json`, `TaskRelationObservation.json`, `AnalysisValidation.json` |
| 이전Wander 질의 분포 | 같은 루트의 `WanderEventAnalysis.json`, `WanderEventBreakdown.csv`, `AnalyzeWanderEvents.mjs`, `WanderEventAnalysisValidation.json`, `Exports/Measured_*Events.csv` |
| 최신 긴 진단 | `Saved/Profiling/Graphics/Phase00/WanderDiagnosticsTrace_20261108_041500_fa2a4b/`의 `CPU.utrace`, `Launch.json`, `Exit.json`, `SourceChanges.patch`, `CaptureInterval.json`, `ArtifactHashes.json` |
| 최신CSV | 최신 루트 하위 `Medium_67AAF5BB44826B99980D29BDAF137D08/`의 `Run.json`, `Frames.csv`, `Population.csv`, `Summary.json` |
| 최신 계수·연결 | 최신 루트의 `CounterValidation.json`, `CounterDistribution.json`, `CounterFrames.csv`, `WanderCounterJoinAnalysis.json`, `WanderCounterJoinValidation.json`, `WanderCounterJoinedFrames.csv`, `WanderCounterJoinArithmeticCheck.json`, `AnalyzeWanderCounterJoin.mjs` |
| 최신 내보내기 | 최신 루트의 `Exports/Padded_GameThreadFrameEvents.csv`, `Padded_WanderEvents.csv`, `ExportRequest.json` |
| 계측 빌드·0/1 검증 | `Saved/Profiling/Graphics/Phase00/WanderDiagnostics_20261108/` 및 그 안의 `PackagePreflight/`에 있는 빌드 로그·소스 패치·패키지 식별·`FunctionalValidation.json`, `PreflightSummary.json` |
| 진단 패키지 | `Saved/Graphics/Phase00WanderDiagnosticsPackage_20261108/Windows/LootNPop/Binaries/Win64/LootNPop.exe` |
| 원래 기준선 패키지 | `Saved/Graphics/Phase00Package/` — 계측 패키지와 별도이며 덮어쓰지 않았음 |

진단 패키지 실행 파일SHA256은 `E3290C0FA9D7767DFD6A18936A6583B311D49AECD1778F0B4B3B9D30653E8AD4`, 카메라 Revision3 SHA256은 `46DF16A132082BCD1CB10C312C5BA662D0DC9B9795B7DB14211B1DAFA3B877BC`다. 컨테이너별 해시는 `PackagePreflight/PackageIdentity.json`과 최신 `Launch.json`을 따른다. 원래 패키지의 주요8개 파일 해시는 짧은 검증 전후 일치했다.

CSV 진단은 기본값0의 `LNP.Enemy.Wander.Diagnostics`를1로 켠다. 월드 시각과계수8개를 프로세서 실행마다0으로 초기화한 뒤 `Set`으로 기록한다. 같은CSV 프레임에 여러 번 실행되면 마지막 값만 남는다. 현재 검증은 단일월드·프레임당1회 조건이다. 짧은 기본0/진단1 실행은 각각247/274개 측정 행·종료0으로 기능 검증했다. CodecRejected는 긴/짧은 진단 모두0으로 해당 거부 분기는 미관측이다.

프레임 대응에서는 `LNPGraphics_GTMs`가 직전 프레임 값인 점에 주의한다. 최신2977개 인접 행에서 `LNPGraphics_GTMs[i] == GameThreadTime[i-1]`였다. 같은 행 계수와GT를 비교할 때는 `GameThreadTime`을 사용했다. `LNPWander_WorldSeconds`는 월드 게임 시각으로 트레이스 시각이 아니다.

Insights에서는 GT만 켜고 실제 실행된 접미사 있는 Wander 타이머를 선택한다. 접미사 없는0회 타이머나 전체 스레드 통계를 혼용하지 않는다. 이벤트는 `Export Timing Events (Selection)`으로 내보낸다. 엔진 CSV 후행 메타데이터 `[HasHeaderRowAtEnd],`는 프레임 표가 아니며 `-ExecCmds`의 중첩 인용이 일반CSV 규칙을 깨뜨릴 수 있다. 기존 집계기는 이 표식에서 프레임 읽기를 끝낸다.

이전 트레이스 분석 때의 Insights 메모리 태그·타이머 메타데이터 경고도 원자료에 남아 있다. 분석 가능한 이벤트와 실제 정상 종료 여부를 별도로 기록했다.

## 7. 최적화 세션에서 돌아올 때 남길 것

1. 변경한 작업과 행동 계약, 제거/감소한 호출의 근거, 남은 실패 사유를 정리한다. 순간 부하 분산·총량 감소·상시 비용 감소를 구분한다.
2. 변경 전후 소스/패키지/경로의 식별값을 남긴다. 진단 계측과기준선 수집은 분리하며 변경한 바이너리로 별도 패키지를 만든다.
3. 동일 조건에서 Wander 평균/P95/P99/최대·고비용 횟수·후보/실패/질의 수·활성/생존 개체 분포를 비교한다. 행동을 늦춰 총 시도 수가 달라지면 그 차이를 밝힌다.
4. 이동/정지·초기 겹침·표면/리비전 변경·동적 지형·전투/피격 전환·복제 재생의 관련 검증 결과를 남긴다. 수동 PIE가 빠른 항목은 사용자에게 바로 요청한다.
5. 이 결과만으로 기존 Medium GT 재현성 통과나 Phase0 완료로 바꾸지 않는다. 그래픽스 세션에 돌아온 뒤 정식 비교/반복 측정이 필요한 범위를 별도 확인한다.

## 8. 배회 외에 관측한 후속 조사 후보

사용자가 각 항목도 서로 다른 세션에서 개선하기로 해 상세 근거·검토 지점·검증 조건을 각각 독립 문서로 분리했다.

- [그래플 앵커 효과 최적화 인계](GrappleAnchorOptimizationHandoff.md)
- [GT 렌더 동기화 비용 분석·최적화 인계](RenderSyncOptimizationHandoff.md)
- [Nav 최근접 노드 탐색 최적화 인계](NavNearestNodeOptimizationHandoff.md)

이전 트레이스의 최대 틱 완료 `ProcessUntilTasksComplete` 약29.60ms는 Wander 실행27.34ms를 포함해 순수 대기가 아니다. 그 Wander Task13145894의 선행 작업 중 SpatialGrid Task13145892(약265.7us)를 확인했지만 이 대표작업이27ms 급 비용을 설명하지는 않는다. 원본 관계는 `TaskRelationObservation.json`을 따른다.

각 세션의 변경이 공통코드/패키지에 섞일 수 있으므로 전후 식별값과 개별 효과를 구분해 그래픽스 세션에 인계한다.

네 개선 완료 후의 통합 검증·새 반복 측정·Phase0 마무리는 [개선 후 재개 절차](../phases/Phase00_PostOptimization.md)를 따른다.
