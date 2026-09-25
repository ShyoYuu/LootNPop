# Phase 3 — MassWorldCollision 정확성 기준선

> 상태: 완료(2026-09-25)
> 예상 범위: 3~5세션
> 선행 조건: Phase 2 Octant Definition과 베이크 스키마

## 목표

production collision response와 worker exact query의 안전성을 먼저 검증한 뒤, `LNPWorldExact` 기반 공통 query wrapper, 투사체 exact 판정, Placement Marker 기반 동적 패널을 만든다. Phase 3b와 Phase 6이 재사용할 정확성·비용 기준선을 남긴다.

Phase 3는 실제 Support Atlas payload를 생성하거나 소비하지 않는다.

## 필수 문서

- `../Current.md`
- `../Roadmap.md`
- `../design/RuntimeCollision.md`
- `../design/DynamicTerrain.md`
- `../design/TerrainContract.md`
- `../design/RegressionMap.md`
- `../design/ValidationAndMigration.md`
- `../research/ChaosSceneQueries.md`

## Gate -1 — 전환 전 계약

### A. production exact collision response

- [x] production 옥탄트·정적 프랍·동적 지형 후보의 현재 profile과 `LNPWorldExact` response 목록 생성 (`LNP.SurfaceNav.AuditExactResponse`)
- [x] `Support`, `Blocker`, `Support+Blocker` 역할에 맞는 profile 적용 — 지각·프랍 HISM·스프링 런처, LootPod는 B의 collision proxy로 해소(audit MISSING=0)
- [x] Destructible Support-only, Blocker-only, Support+Blocker profile 분리(D-039)
- [x] production 옥탄트 8-slot에서 line/sphere/capsule query oracle 통과 — `LNP.SurfaceNav.ExactOracle`, `-game` 단독·리슨 서버·클라이언트 PASS
- [x] 신규 exact 경로와 legacy 경로를 비교할 개발 CVar 제공 — 투사체 `LNP.SurfaceNav.ProjectileExact`. 기본값 전환과 함께 제거
- [x] audit 통과 전 신규 exact 경로를 production 기본값으로 만들지 않음(D-036) — audit·oracle 통과 뒤 전환

Component Tag의 production 전환은 Phase 4까지 나눌 수 있다. 이 Gate는 exact collision response를 먼저 안전하게 만드는 범위다.

### B. hit identity와 lifetime

- [x] slot→Level Instance/Loaded Level weak reference를 match lifecycle 동안 보존 (`ULNPOctantSpawnSubsystem::GetSlotLevel`·`FindSlotForLevel`)
- [x] exact hit에서 component/shape identity, face index, ISM·HISM instance index 추출 가능성 검증 (`LNP.SurfaceNav.ProbeHitIdentity`)
- [x] 게임 스레드에서 immutable hit identity registry 구축·게시 (`ULNPHitIdentitySubsystem`)
- [x] worker 결과는 UObject 역참조 없는 POD만 노출 (`FLNPExactHitIdentity`) — worker 호출 검증은 Gate 0
- [ ] component 하나의 disconnected sheet 둘이 서로 다른 face identity로 구분되는지 검증 — **Phase 4 착수 전으로 이관**(fixture와 함께, `../design/RegressionMap.md`)
- [x] 정적 지형, 동적 패널, 미등록 hit를 각각 분류 — 동적 패널은 자동화 `WorldCollision.DynamicMarkerHit`와 `LNP.SurfaceNav.ProbePanels`(리슨 2P 호스트·게스트 각 8/8)로 Dynamic·`(slot, MarkerId)` 확인. 수명주기별 hit counter는 `Report`의 `HitLifetime`
- [x] LootPod collision proxy(D-047) 구현. ISM instance 제거 뒤 index→엔티티 재매핑과 generation 증가를 hit identity 사례로 검증 (`LootNPop.SurfaceNavigation.HitIdentity.LootPodProxySwapRemap`)
- [x] registry generation과 match reset·stream unload lifecycle gate 검증 — match 중 reset·unload 경로가 없어 구조로 충족. 해제 시 generation 증가와 stale snapshot 안전은 자동화. 경로를 도입할 때 gate 필수(`../design/RuntimeCollision.md`)
- [ ] 내부형 double-sided shell의 hit normal과 walkable 판정 검증 — **Phase 4 착수 전으로 이관**

식별할 수 없는 hit는 임의 Surface로 스냅하지 않고 `UnknownExactSurface`로 반환하고 counter를 기록한다(D-037).

Phase 3에는 실제 Support Layer가 없으므로 `(slot, LocalLayerId)` binding은 구현하지 않는다. Phase 4가 face/instance→Layer 표를 생성하고 Phase 5 runtime snapshot이 registry에 결합한다.

## Gate 0 — worker 동기 scene query

스파이크: `SurfaceNavigation/LNPExactQuerySpike.*`(CVar `LNP.SurfaceNav.ExactSpike.*`). 측정값은 `../history/Phase03_Log.md` 2026-09-24 Gate 0 절.

- [x] Mass worker에서 line, sphere, capsule query 실행 — `gamethread=0`
- [x] `bTickPhysicsAsync=True`와 동적 kinematic body 조건에서 ensure·check 부재 — PIE·에디터 바이너리 `-game`, body 62개. data race 전용 도구(TSan)는 쓰지 않았다
- [x] Editor와 `-game` 리슨 서버 2P에서 결과 일치 — 옥탄트 slot 칸 차이 0. `-game`은 에디터 바이너리다
- [x] wrapper 총시간과 scene read-lock 대기를 분리 계측 — 락은 질의 직전 read lock 획득 시간으로 근사
- [x] query 종류별 count, P50/P95, 최악 프레임 기록
- [x] 미해석 hit 수와 dynamic/static 분류 오류 기록 — 둘 다 0
- [x] 게임 락(`FRWLOCK`) 조건의 락 대기 — 패키지 리슨 서버: 게임 스레드 쓰기 없음 P95 0.1ms/frame, body 62개 겹침 3.4ms/frame. 락 대기는 쓰기 겹침이 만든다
- [x] 실패 시 D-025 재논의 후 구현 진행 중단 — 실패 없음. **Gate 0 통과(2026-09-24)**. 쓰기 겹침은 배치 제약으로 남긴다(구현 단위 3·4)

## 구현 단위

### 1. MassWorldCollision API

- [x] `RaycastWorld`
- [x] `SweepSphereWorld`
- [x] `SweepCapsuleWorld`
- [x] `ProbeSupport` — Up 기준 walkable 법선 + registry `Support` 역할
- [x] `LNPWorldExact` query params와 self/owner 제외 규칙 — 제외는 게임 스레드에서 구한 Actor unique ID만
- [x] worker-safe POD result와 hit identity 결과 — `FLNPWorldHit`, 자동화에서 ParallelFor worker 호출 검증
- [x] debug draw queue와 Unreal Insights marker — 화면 확인은 아직
- [x] correctness-mandatory와 optional query counter 분리

### 2. 투사체·탄도 가이드

- [x] 서버 투사체의 `PreviousPos → CurrentPos` exact segment — line trace(`LNPProjectileMotion::TraceWorld`), 형상 결정은 `design/RuntimeCollision.md`
- [x] Mass target hit time과 world hit time 비교 후 earliest hit 하나만 채택 — 캐릭터 판정 선분을 월드 착탄점으로 자른다
- [x] 클라이언트 ghost에 같은 world 판정 적용
- [x] 게임 스레드 `PredictArc`에 같은 충돌 함수 적용
- [x] `IsUnderSurface`와 반지름 기반 착탄 제거 — exact가 유일한 경로
- [x] 정적 bounds와 marker swept bounds를 합친 world collision envelope 안전망 — 정적·런타임 source 정점 기준, 움직이는 패널은 경로 swept 반지름으로 등록

### 3. Placement Marker와 동적 패널

- [x] marker class, `MarkerId`, path authoring 데이터 최소 계약 구현 — `ALNPPlacementMarker`, 스플라인 루트
- [x] 서버만 loaded LVI marker를 `(slot, MarkerId)`로 수집 — slot 0~7 + persistent level
- [x] 공통 서버 스폰 함수로 복제 Actor 생성 — `SpawnPlacedActor`, 월드 장치도 사용
- [x] path revision, 상태, server epoch, 시작 시각 초기 복제 — 패널은 상태가 없고 server world time이 epoch다(`design/DynamicTerrain.md` §2)
- [x] transform tick 배치 — 패널 Actor 틱(TG_PrePhysics): Mover NPP 시뮬레이션 뒤·base 추종 틱 앞, worker exact query(StartPhysics~) 앞. snapshot은 패널 틱 뒤 게시
- [x] late join에서 같은 자세 복원 — 사용자 2P
- [x] 2P Mover movement base 직렬화와 예측 안정성 확인 — 호스트·게스트 떨림·미끄러짐 없음, 이탈 관성 0.99
- [x] PureEntity용 DynamicSupport POD snapshot의 기반 구조 확인 — `FLNPDynamicSupportFrame`(소비자는 Phase 6)

### 4. 부하 기준선

2026-09-24 사용자와 합의한 고정값이다.

| 항목 | 값 |
|:---|:---|
| 적 수 | 300(일상 전투), 1000(스트레스) 두 단계. 두 점으로 규모에 따른 비용 기울기를 본다 |
| CombatMode 비율 | PureEntity 90% / ActorPromoted 10% — 목표 하한의 보수적 구성. 300→Actor 30, 1000→Actor 100 |
| 동시 투사체 | 500발. 속도는 실제 무기 DA 값을 그대로 쓰고 별도 분포를 만들지 않는다 |
| 플레이어 | 2(리슨 서버 호스트 + 게스트) |
| 동적 body | 구현 단위 3의 움직이는 패널 8개(Meadow_00 마커 1개 × 8 slot) |
| 측정 build | 패키지 Development 리슨 서버 2P(게스트 `-nullrhi`), `bTickPhysicsAsync=True`. CPU 모델을 보고서에 적는다. 에디터 바이너리 `-game`은 에디터 전용 디버그 드로우가 프레임을 오염시켜 프레임 판정에 쓰지 않는다(2026-09-25 개정) |
| 시간 | warm-up 10초, capture 30초 |
| 성공 기준 | 서버 CPU 프레임(호스트 `-nullrhi`) P95 ≤ 16.6ms, exact query 합계 P95 ≤ 2ms/frame(worker 합산), scene read-lock 대기 P95 ≤ 0.2ms/frame. 렌더링 호스트 프레임은 기준선으로만 기록한다(2026-09-25 사용자 결정: GPU 비용은 Phase 3 범위 밖) |

Phase 3b와 Phase 6은 같은 harness와 seed를 사용한다.

- [x] harness — `-LNPLoadBaseline=N`(`SurfaceNavigation/LNPLoadBaseline.*`): 고정 seed 링 스폰, 발사체 500발 유지, 플레이어 무적, warm-up·capture 자동, 호스트·게스트 각자 보고
- [x] 300·1000 기준선 — exact P95 1.25·1.67ms, 락 P95 0.095·0.128ms로 통과. 프레임 기준은 호스트가 300마리부터 실패(P95 28·43ms). 원인은 exact가 아닌 서버 적 시뮬레이션이다(`../history/Phase03_Log.md` 2026-09-25)
- [x] 호스트 프레임 실패의 Insights 분해와 처리 방침 결정 — 원인은 에디터 전용 디버그 드로우(에디터 바이너리)와 GPU 대기(패키지, RTX 4060 Laptop)였다. 패키지 호스트 `-nullrhi` 1000마리 P95 6.05ms로 서버 CPU 기준 통과. 렌더링 호스트는 300·1000마리 P95 26·37ms 기준선으로 기록(`../history/Phase03_Log.md` 2026-09-25 분해 절)

## 자동화·회귀

- [x] 회귀 맵의 벽·섬 측벽·동굴·정적 프랍·동적 패널 exact hit — `WorldCollision.RegressionMap`(맵 fixture를 테스트 월드로 복제, 10개 사례)
- [x] Decoration miss와 Pawn 제외 — 같은 자동화, `UnknownHits=0`
- [x] world/entity earliest hit — `WorldCollision.EarliestHit`. 판정은 `LNPProjectileMotion::ClipSegmentToWorld` 하나로 서버·Ghost가 공유
- [ ] 내부형 shell normal — Phase 4 착수 전으로 이관
- [ ] 한 component의 분리된 sheet face identity와 ISM/HISM instance identity — Phase 4 착수 전으로 이관(ISM instance identity는 LootPod proxy 자동화로 확인)
- [x] production 8-slot exact collision — `LNP.SurfaceNav.ExactOracle`(콘솔, 생성된 월드 필요)
- [x] 패널 탑승 2P와 이동 중 late join — 수동 플레이 + `LNP.DynamicTerrain.LogRiders`·`LogDepartures` 계측
- [x] Editor 자동화 — `LootNPop.SurfaceNavigation` 17/17
- [x] `-game` 리슨 서버 2P 스모크(D-031) — 무인 300마리·발사체 500: 호스트·게스트 `UnknownHits=0`·`EnvelopeEscapes=0`·`ProbePanels` 8/8·ensure 0. 사용자 플레이 스모크는 구현 단위 2·3

## 완료 조건

- [x] Gate -1과 Gate 0이 독립적으로 통과하고 증거가 로그에 남음(Gate -1 B 두 항목은 Phase 4 착수 전 이관)
- [x] production 콘텐츠에서 `LNPWorldExact` 기본 전환 후 관통 회귀가 없음 — 구현 단위 2 사용자 2P 스모크
- [x] worker가 UObject를 역참조하지 않고 exact hit 의미를 해석함
- [x] 투사체·ghost·탄도 가이드가 같은 world collision 함수를 사용함
- [x] 동적 패널이 late join 포함 2P Mover의 valid movement base로 동작함
- [x] 고정 부하 시나리오의 query 비용·락 대기 기준선 확보
- [x] `LootNPopEditor Win64 Development`와 `LootNPop Win64 Development` 성공
- [x] `Current.md`, `Roadmap.md`, `../history/Phase03_Log.md` 갱신

## 제외 범위

- 실제 Support Atlas rasterization과 runtime query
- PureEntity exact 접지 프로토타입(Phase 3b)
- 완전 비행 NPC(병렬 Phase 3c)
- Nav Grid와 A*
- Conditional Patch 구현
- Support 기반 투사체 horizon
