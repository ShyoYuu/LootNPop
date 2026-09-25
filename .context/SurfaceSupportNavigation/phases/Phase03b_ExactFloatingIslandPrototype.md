# Phase 3b — exact 전용 부유섬 프로토타입

> 상태: 기준 설계 — 구현 단위 1·2·3 완료(2026-09-26), 구현 단위 3b(이동 병렬화 측정) 착수 전
> 예상 범위: 2~3세션
> 선행 조건: Phase 3 MassWorldCollision 정확성 기준선(완료)

## 1. 목표

Support 캐시를 만들기 전에 두 가지를 실측한다(D-032).

1. 300m 내부형 구에서 부유섬이 플레이 공간으로 재미있는가 — 사용자가 직접 판단한다. 이 Phase는 기능 점검(§3.7)만 맡는다
2. PureEntity 접지·낙하·넉백을 Mass worker 동기 exact만으로 처리하면 몇 마리에서 프레임 예산을 넘는가

여기서 만든 exact 접지 경로는 버리지 않는다. Phase 6에서 캐시가 확신하지 못하는 구간의 exact 폴백으로 재사용한다(`../design/ValidationAndMigration.md` 소비자 전환 순서 2).

## 2. 현재 코드의 출발점

- `ULNPEnemyMovementProcessor`(PrePhysics, Movement 그룹)가 Actor 없는 적의 이동을 모두 처리한다. 지면은 `ULNPSurfaceCacheSubsystem::GetSurfacePoint` 하나로 구한다.
- SurfaceCache는 방향마다 `0.5R → 1.5R` 선분의 **첫 hit** 하나만 저장한다. 부유섬이 있는 방향에서는 섬 윗면이 기록되고 그 아래 지각은 사라진다. 섬 아래 지각을 걷는 적은 섬 윗면으로 순간이동한다. exact 전환이 필요한 직접적인 이유다.
- 접지 상태는 `FLNPEnemyVelocityFragment::Velocity == 0`으로 판단한다. 지면을 잃었는지 검사하는 단계가 없어 절벽 끝에서 떨어지지 않는다.
- 공중 착지는 반지름 비교다(`IntegrateAirborne`). 벽·섬 측벽·섬 밑면에 막히지 않는다.
- PureEntity는 벽·프랍과 충돌하지 않는다. 경사 검사만 캐시 두 점으로 한다.
- ActorPromoted 적과 플레이어는 Mover가 exact collision으로 처리하므로 이 Phase의 대상이 아니다.
- 캐시 소비자는 적 이동 외에도 배회 목표(`LNPEnemyStateTreeProcessors`), Mass 스폰·Pod 배치(`LNPMassSpawnSubsystem`), 탄도 가이드의 보조 probe, 부하 harness 링 배치가 있다.
- 월드 반지름은 `ULNPSettings::SphereRadius = 25000`이고 옥탄트는 `LVI_Octant_Meadow_00` 하나다.

## 3. 확정 결정

### 3.1 부유섬은 옥탄트의 일부다(D-048)

- 부유섬은 옥탄트 LVI 안에 레벨 디자이너가 직접 배치한다. 옥탄트와 섬을 따로 만들어 런타임에 조합하지 않는다. 조합 방식은 산 높이와 섬이 겹치지 않도록 높이 대역을 나누는 식의 제약을 늘리고 레벨 디자인 의도를 흐린다.
- 섬은 옥탄트 경계면에 걸치지 않는다. 경계면 지표가 기준 반지름에 수렴해야 한다는 이음매 제약(D-030)과 같은 층위의 제약이다. 꼭짓점(좌표축) 부근도 피한다(`../design/TerrainContract.md` §7).
- 섬 모양은 자유다. 계단·벽 등 다양한 형태가 가능하고, 보행면은 walkable이면 된다. 기본 보행면은 평면이 아니라 월드 구와 중심을 공유하는 **구면 곡면**으로 만든다.
- 플레이어가 섬에 오르는 주 수단은 스프링 런처와 훅 앵커다. 섬마다 Placement Marker로 둔다. 마커 `ElementClass`로 월드 장치를 스폰하는 경로가 그대로 쓰이는지는 구현 단위 1에서 확인한다.

새 `Meadow_00` 구성(프로덕션용 옥탄트다. 30,000cm에 섬을 넣는 첫 옥탄트라 여러 차례 개선을 전제로 한다):

| 섬 | 크기(초안) | 비고 |
|:---|:---|:---|
| 큰 섬 1 | 지름 약 6,000cm, 지각 위 약 4,000cm | 런처·훅 앵커로만 오른다. 지상 NPC가 오를 수 없는 안전 지대(D-011) 시험 |
| 작은 섬 A | 지름 약 2,000cm, 지각 위 약 1,500cm | 지각과 **경사로**로 연결. PureEntity가 걸어서 오르내리는 경로와 Phase 7 Walk 연결의 입력. 극 쪽은 블렌더 경사로(관통 터널·수직 구멍+방 간이 동굴 포함), 적도 쪽은 지각 일체형 언덕(D-051) |
| 작은 섬 B | 지름 약 2,000cm, 지각 위 약 2,500cm | 계단·벽 등 섬 위 형태 변화 시험 |

크기와 높이는 제작 중 사용자가 조정할 수 있다. 8 slot은 모두 이 옥탄트 definition 하나를 회전 배치한다.

### 3.2 반지름 30,000cm와 Meadow_00

- 기존 25,000cm `Meadow_00`은 최종적으로 삭제한다. 삭제 전 백업이 필요하면 이름을 `Meadow_00`과 헷갈리지 않게 바꾼다(예: `Legacy_Meadow_R25000`).
- 30,000cm 새 `Meadow_00`을 만들고, §3.1의 섬 세 개를 이 옥탄트에 둔다. pool에는 새 `Meadow_00`만 등록한다.
- `SphereRadius`를 30,000으로 올린다(D-046). PlayerStart는 지금처럼 남극(-Z) 극지방 주변에 둔다.

반지름을 바꾸면 함께 확인할 것:

- `TestMap03`의 PlayerStart와 조명·하늘
- SurfaceCache 셀 간격. 해상도가 반지름에 비례해 샘플 수가 약 1.44배가 된다(bake 시간과 메모리 기록)
- world collision envelope, Mass 스폰 설정, 월드 장치·LootPod 시드 배치
- int16 복제 캡. 섬은 지각 안쪽이라 여유가 늘 뿐이다

Phase 3 부하 기준선은 25,000cm 월드 수치다. 3b는 30,000cm 월드에서 legacy와 exact를 같이 다시 잰다.

### 3.3 PureEntity exact 이동 알고리즘(D-049)

매 프레임 exact를 쓴다(캐시 없는 최악 조건). 3b의 목적이 exact 전용 한계치라서 호출을 줄이는 최적화를 넣지 않는다. 호출을 줄이는 것은 Phase 6 캐시의 역할이다.

접지 상태 한 프레임(엔티티당 query 2회):

1. **수평 sweep**: 현재 캡슐 중심을 step 높이만큼 Up으로 올린 뒤 이동 목표까지 capsule sweep한다. 벽·프랍·섬 측벽에 막히면 막힌 지점에서 멈추고, 남은 이동은 hit 법선의 접평면으로 한 번 미끄러뜨린다.
2. **하향 probe**: `ProbeSupport`(sphere)로 step 높이 + 최대 하강 거리를 내려 찍는다.
   - walkable 법선이고 `Support` 역할이면 캡슐 중심을 그 위에 둔다.
   - 지지면이 없으면 속도 0인 공중 상태로 넘긴다. 이것이 절벽 끝의 낙하다.
   - 걸을 수 없는 경사는 수평 이동을 취소한다. 지금의 캐시 두 점 경사 검사를 대체한다.

공중 상태 한 프레임(엔티티당 query 1회):

- 중력을 적분하고 `이전 위치 → 제안 위치` capsule sweep의 earliest hit를 쓴다.
- walkable 법선이고 `Support` 역할이면 착지해 속도를 0으로 만든다.
- 걸을 수 없는 hit(섬 측벽·밑면)는 속도의 법선 성분을 지우고 미끄러진다. 멈추게 하면 섬 밑면에 달라붙는다.
- `UnknownExactSurface`는 Blocker로만 취급하고 착지하지 않는다(D-037).

구현 규약(구현 단위 2, `Source/LootNPop/Enemy/LNPEnemyExactMovement.*`):

- 이동 프로세서와 자동화가 같은 함수(`StepGrounded`·`StepAirborne`·`ProjectToSameLayer`)를 부른다. 공중 여부는 기존 규약(`FLNPEnemyVelocityFragment::Velocity == 0`이면 접지)을 그대로 쓴다. 지지면을 잃거나 착지하지 않은 채 속도가 0이 되면 중력 한 스텝 속도를 실어 공중에 남긴다.
- 하향 probe 구는 캡슐 바닥 구와 같다(반지름 = 캡슐 반지름). 더 작으면 probe가 빠졌는데 캡슐 옆면이 모서리 위에 걸려, 수직 낙하 sweep이 모서리에 착지하고 다음 프레임 다시 지지면을 잃는 진동이 생긴다.
- 수평 sweep이 막히면 벽 바깥으로 1cm 물러나 멈춘다. 벽에 딱 붙으면 같은 반지름의 probe가 벽을 먼저 맞혀 Rejected로 멈췄다(자동화에서 발견).
- sweep이 시작부터 겹치면(벽에 붙어 선 개체, 접지 자리에서 받은 넉백) 겹침 깊이만큼 법선 방향으로 풀고 파고드는 성분을 지운 뒤 한 번 더 sweep한다. sweep 없이 옮기면 측벽을 따라 내려가다 바닥을 뚫는다. `FLNPWorldHit::PenetrationDepth`를 이 용도로 추가했다.
- probe hit가 walkable Support가 아니면(가파른 경사·Blocker·Unknown) 수평 이동을 취소하고 제자리에 선다(`Rejected`). hit가 없으면 절벽 낙하(`LostSupport`)다.
- 단차 45cm(캡슐 반지름 이상, walkable 경사에서 올린 캡슐이 지면에 박히지 않는 값), 접지 유지 낙차 60cm, walkable dot 0.71.
- query 분류: 접지(수평·슬라이드·probe)와 배회 재투영은 `GroundRiskFallback`, 공중은 `AirborneMandatory`.
- 배회 재투영은 엔티티와 같은 반지름에서 위아래 300cm를 찍는다. 지각 위 섬 밑면 여유(최소 약 470cm)보다 작아야 섬 아래에서 찍은 구가 섬 밑면과 시작부터 겹치지 않는다.

### 3.4 CVar와 적용 범위

- `LNP.SurfaceNav.EnemyExactGround`(0 legacy / 1 exact)와 `LNP.SurfaceNav.EnemyExactLateralSweep`(0/1, 측정 요인 분리용).
- 이동 시뮬레이션은 서버 전용이라 CVar도 서버에서만 의미가 있다. 복제·클라이언트 경로는 바뀌지 않는다.
- 기본값은 구현과 2P 스모크 통과 뒤 1로 바꾼다. 섬 월드에서 legacy는 섬 윗면으로 순간이동하므로 비교용으로만 남기고, Phase 6에서 제거한다. → 2026-09-25 구현 단위 2 스모크 통과 뒤 `EnemyExactGround` 기본값 1(`EnemyExactLateralSweep`은 처음부터 1).

| 소비자 | 처리 |
|:---|:---|
| 적 이동(접지·공중·넉백·사망 팝) | exact 전환(3b 본체) |
| 배회 목표 | 캐시 방향을 쓰되, exact 모드에서는 엔티티의 현재 반지름에서 하향 probe로 다시 찍어 같은 층의 지면을 고른다. 섬 아래 적이 섬 윗면을 목표로 잡고 타임아웃을 반복하지 않게 한다 |
| Mass 스폰·Pod 배치 | 바꾸지 않는다. 캐시가 섬 윗면을 주므로 Pod가 섬 위에 생길 수 있다. 알려진 한계로 기록하고 Phase 5 Spawn stream에서 해결한다 |
| 부하 harness 배치 | exact probe로 지각과 섬 윗면에 직접 배치한다 |
| 탄도 가이드 보조 probe | 바꾸지 않는다. 판정 자체는 이미 exact다 |

### 3.5 Mass 페이즈와 동적 패널 순서(D-050)

적 이동 프로세서는 PrePhysics 페이즈이고, 움직이는 패널도 TG_PrePhysics Actor 틱이라 순서가 보장되지 않는다. 장기적으로 순서를 확정해 둔다.

- **C안 채택**: 패널 Actor 틱을 Mass PrePhysics 페이즈 tick function의 선행 조건으로 건다. 그러면 PrePhysics 페이즈의 모든 exact query는 모든 패널이 자세를 옮긴 뒤에 돈다.
- 가능성 검토(2026-09-25): `FMassProcessingPhaseManager::GetProcessingPhaseTickFunction(EMassProcessingPhase)`가 public이고 `FTickFunction&`를 돌려준다. `UMassSimulationSubsystem::GetMutablePhaseManager()`로 접근한다. `ULNPDynamicTerrainSubsystem::RegisterPanel`·`UnregisterPanel`이 이미 게시 틱에 같은 방식으로 선행 조건을 걸고 풀므로 같은 자리에 추가한다. 둘 다 TG_PrePhysics라 같은 틱 그룹 안의 순서다.
- 사이클 검토: 패널 틱은 선언된 선행 조건이 없고, Mover base 추종 틱이 패널 틱을 선행 조건으로 건다. Mass PrePhysics 페이즈를 패널 뒤로 보내도 패널이 Mass에 의존하지 않으므로 사이클이 생기지 않는다.
- 구현 단위 2에서 확인할 것: 페이즈 tick function이 월드 수명 중 재등록되며 선행 조건을 잃지 않는지, 실제 실행 순서(Insights), 클라이언트에서도 같은 순서인지.
- 위 확인에서 보장이 안 되면 B안(이동 프로세서를 PostPhysics로 이전)으로 간다. B안은 공격 위상 순서·Representation·복제 순서를 모두 다시 봐야 하므로 **별도 세션**으로 분리한다.

### 3.6 한계치 측정 시나리오

Phase 3 harness(`-LNPLoadBaseline=N`)를 확장해 재사용한다. 측정 build 규약은 Phase 3과 같다. 패키지 Development, 호스트 `-nullrhi`의 서버 CPU 프레임이고, 에디터 바이너리 `-game` 프레임은 판정에 쓰지 않는다.

| 항목 | 값 |
|:---|:---|
| 배치 | 링 일부가 큰 섬 아래 지각을 지나도록 링 중심을 옮기고, 적의 10%는 섬 윗면에 둔다. 한 시나리오에 지각·섬 아래·섬 위·가장자리가 모두 들어간다 |
| 공중 상태 유발 | 플레이어가 무적이라 적을 때리지 않는다. harness가 적마다 평균 10초에 한 번 합성 넉백을 주고, 섬 위 적은 가장자리 밖으로 민다 |
| 요인 | 적 수 N ∈ {300, 1000, 2000} × 접지 경로 {legacy, exact} 6회. 발사체 500발 고정. 수평 sweep 기여는 N=1000에서 lateral 0/1로 1회를 더한다 |
| 한계치 | exact 경로에서 서버 CPU 프레임 P95 ≤ 16.6ms를 지키는 최대 N. 2000에서도 통과하면 "≥2000"으로 기록한다. 300~2000 사이에서 넘으면 이분 탐색 2~3회로 좁힌다 |
| 기록 | 서버 CPU 프레임 P50/P95, 접지·공중별 exact query 수와 호출당 비용, 프레임당 exact 합계, 락 대기, `UnknownHits`, 착지·낙하 이벤트 수, 섬 가장자리 이탈 수, CPU 모델 |

이 수치가 Phase 4의 캐시 적중률 목표와 Phase 6 재측정의 기준선이다.

harness 구성(구현 단위 3, 사용자 결정): 링 중심은 slot 4 큰 섬 가장자리 아래 지각이다. 적 배치는 exact probe로 층을 고른다. 서버가 플레이어를 링 중심에 다시 스폰한다(Mover 텔레포트는 게스트에 전달되지 않는다). 합성 넉백은 고정 1,200cm/s다. 상세는 `LNPLoadBaseline.h`와 로그를 따른다.

결과(2026-09-26, `../history/Phase03b_Log.md` 구현 단위 3):

- **exact 한계치 500마리**(P95 15.85ms). 550은 17.01ms로 실패한다. legacy는 1000에서 15.61ms로 통과한다.
- 접지 개체 1마리당 프레임 약 1.46 query·약 8us다(GroundRiskFallback 평균 5.6~5.7us). 적 수에 선형이다. 수평 sweep은 exact 합의 약 20%다.
- 섬 아래 순간이동(LayerJumps)은 exact 0, legacy 12~368이다. `UnknownHits=0`, `EnvelopeEscapes=0`.
- 이동 프로세서가 단일 스레드 `ForEachEntityChunk`라 exact 합이 게임 스레드 임계 경로에 그대로 얹힌다.

### 3.6.1 이동 병렬화 측정(2026-09-26 사용자 결정)

단일 스레드 한계치(500)는 "캐시 없는 최악 조건"으로 그대로 남기고, 이동 프로세서를 병렬화한 exact 한계치를 한 줄 더 잰다.

- 이유: Phase 6에서 캐시와 병렬화가 한꺼번에 들어가면 D-032의 "캐시 도입 뒤 한계치 증가"를 캐시 몫으로 읽을 수 없다. 단일 exact → 병렬 exact(병렬화 몫) → 병렬 + 캐시(캐시 몫)로 나눈다. Phase 4 캐시 적중률 목표도 병렬 exact 기준으로 잡는다. worker 동시 exact query의 락 경합도 이 부하로 처음 검증한다.
- 한계: 병렬화는 CPU 총량을 줄이지 않는다. 총량 절감은 캐시의 몫이다. `-corelimit=4`라 이득은 4배 미만이다.
- 구현: `ULNPEnemyMovementProcessor`를 `ParallelForEachEntityChunk`로 바꾼다. 공유 상태인 `EntitiesToSignal`(StateTree 신호)는 청크별로 모으거나 스레드 안전하게 합친다. Actor용 deferred 명령은 병렬 실행의 청크별 command buffer를 쓴다. 신호 순서가 바뀌어도 동작이 같은지 확인한다. 병렬 여부는 CVar로 켜고 끈다(측정 요인 분리, 기본값은 측정 뒤 정한다).
- 측정: `Scripts/Profiling/RunLoadBaselineMatrix.ps1`에 병렬 CVar 시나리오를 추가한다. N ∈ {500, 1000, 2000} exact 병렬 + 한계치 이분 탐색 2~3회. 기록 항목은 §3.6과 같고 락 P95를 특히 본다.
- 완료 뒤 자동화 21개와 에디터 바이너리 `-game` 2P 스모크로 동작 회귀가 없는지 본다.

결과(2026-09-26, `../history/Phase03b_Log.md` 구현 단위 3b):

- **병렬 exact 한계치 750마리**(P95 16.40ms). 800은 18.32ms로 실패한다. 단일 스레드 500(15.85ms)은 최악 조건 기준선으로 남는다.
- 병렬화는 프레임 P50을 exact CPU 합의 약 51~58%만큼 줄인다(`-corelimit=4` 이상치 75%). exact 총량·query 수는 그대로다.
- 청크당 최대 103마리라 500마리도 잡이 10개다. worker 동시 query의 락 대기는 N=1000까지 P95 ≤ 0.2ms, N=2000에서 0.326ms다.
- CVar `LNP.SurfaceNav.EnemyParallelMovement` 기본값은 측정 뒤 1로 정했다(사용자 결정, 2026-09-26). 단일 스레드 기준선은 0으로 재현한다.

### 3.7 기능 점검

재미 평가는 사용자가 한다. 이 Phase는 아래 기능 항목만 점검한다.

- 카메라가 섬 밑면·측벽에 걸릴 때 카메라 충돌이 지형을 뚫지 않는가. LNP profile이 Camera 채널에 응답하는지 먼저 확인한다
- 탄도 가이드가 섬 밑면·측벽에서 끊기는가
- 런처·훅 앵커로 섬에 오르내릴 수 있는가
- 넉백으로 섬에서 떨어진 적이 아래 지각에 착지하는가

## 4. 구현 단위

1. **30,000cm 새 Meadow_00과 섬**: 기존 Meadow_00 백업 개명, 지각 mesh, 구면 곡면 섬 mesh, LVI, 월드 장치 마커, definition 교체, `SphereRadius` 30,000, `TestMap03` PlayerStart 보정. 검증: 8 slot 생성, `ExactOracle` PASS, audit `MISSING=0`, envelope 갱신
2. **exact 접지 경로**: 패널→Mass PrePhysics 선행 조건(§3.5), §3.3 알고리즘, CVar 두 개, 배회 목표 재투영. 검증: 자동화(지각 접지·절벽 낙하·섬 측벽 슬라이드·섬 밑면 충돌·경사로 오르기·Unknown 미착지), 2P 스모크
3. **측정**: harness 배치·합성 넉백 확장, §3.6 매트릭스, 결과를 `../history/Phase03b_Log.md`에 기록
3b. **이동 병렬화 측정**: §3.6.1
4. **기능 점검**: §3.7

## 5. 완료 조건

- [x] 8 slot 전부 30,000cm 새 Meadow_00으로 생성되고 `ExactOracle`·exact response audit가 통과함
- [x] 기존 25,000cm Meadow_00이 pool에서 빠지고, 남긴 백업은 다른 이름임(백업 없이 제자리 갱신, 원본은 git 이력)
- [x] Mass PrePhysics 페이즈가 모든 패널 틱 뒤에 실행됨을 확인함(아니면 B안 별도 세션으로 이관) — C안, 2P 스모크 호스트·게스트 위반 0
- [ ] PureEntity가 exact만으로 지각·섬 윗면에 접지하고, 가장자리에서 떨어지고, 넉백 뒤 올바른 층에 착지함 — 자동화 통과, harness에서 섬 이탈 뒤 아래 지각 착지 14~63회. 눈으로 보는 확인은 구현 단위 4
- [x] 섬 아래 지각의 적이 섬 윗면으로 순간이동하지 않음(legacy 대비 회귀 확인) — harness LayerJumps exact 0, legacy 12~368
- [ ] 벽·프랍·섬 측벽을 걸어서 통과하지 않음 — 자동화 통과, 실게임은 구현 단위 4
- [x] `UnknownHits=0`, 착지가 Unknown hit에 스냅하지 않음 — 자동화 통과, 패키지 매트릭스 10회 호스트·게스트 모두 0
- [x] §3.6 매트릭스와 exact 한계치 기록 — 500마리
- [x] §3.6.1 이동 병렬화 exact 한계치 기록 — 750마리
- [ ] §3.7 기능 점검 통과
- [ ] `LootNPopEditor Win64 Development`와 `LootNPop Win64 Development` 성공, 자동화 통과
- [ ] `-game` 리슨 서버 2P 스모크(D-031)
- [ ] `../Current.md`, `../Roadmap.md`, `../history/Phase03b_Log.md` 갱신

## 6. 제외 범위

- Support Atlas·Nav Grid·A*·Pod 재귀속(Phase 4~7)
- 동굴 키트(Phase 4 착수 전 전제)
- PureEntity의 움직이는 패널 탑승(Phase 6)
- Mass 스폰·Pod 배치의 다층 대응(Phase 5)
- 섬 간 지상 추격(D-011·D-012, Phase 7)
- 완전 비행 NPC(Phase 3c)
- 렌더링 비용(GPU) 최적화
- 부유섬의 재미 평가(사용자 담당)
