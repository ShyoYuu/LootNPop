# Phase 3 — MassWorldCollision 정확성 기준선

> 상태: 진행 중
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
- [ ] production 옥탄트 8-slot에서 line/sphere/capsule query oracle 통과
- [ ] 신규 exact 경로와 legacy 경로를 비교할 개발 CVar 제공
- [ ] audit 통과 전 신규 exact 경로를 production 기본값으로 만들지 않음(D-036)

Component Tag의 production 전환은 Phase 4까지 나눌 수 있다. 이 Gate는 exact collision response를 먼저 안전하게 만드는 범위다.

### B. hit identity와 lifetime

- [x] slot→Level Instance/Loaded Level weak reference를 match lifecycle 동안 보존 (`ULNPOctantSpawnSubsystem::GetSlotLevel`·`FindSlotForLevel`)
- [x] exact hit에서 component/shape identity, face index, ISM·HISM instance index 추출 가능성 검증 (`LNP.SurfaceNav.ProbeHitIdentity`)
- [x] 게임 스레드에서 immutable hit identity registry 구축·게시 (`ULNPHitIdentitySubsystem`)
- [x] worker 결과는 UObject 역참조 없는 POD만 노출 (`FLNPExactHitIdentity`) — worker 호출 검증은 Gate 0
- [ ] component 하나의 disconnected sheet 둘이 서로 다른 face identity로 구분되는지 검증
- [ ] 정적 지형, 동적 패널, 미등록 hit를 각각 분류 — 정적 지형·런처·proxy와 미등록은 완료, 동적 패널은 구현 단위 3 이후
- [x] LootPod collision proxy(D-047) 구현. ISM instance 제거 뒤 index→엔티티 재매핑과 generation 증가를 hit identity 사례로 검증 (`LootNPop.SurfaceNavigation.HitIdentity.LootPodProxySwapRemap`)
- [ ] registry generation과 match reset·stream unload lifecycle gate 검증
- [ ] 내부형 double-sided shell의 hit normal과 walkable 판정 검증

식별할 수 없는 hit는 임의 Surface로 스냅하지 않고 `UnknownExactSurface`로 반환하고 counter를 기록한다(D-037).

Phase 3에는 실제 Support Layer가 없으므로 `(slot, LocalLayerId)` binding은 구현하지 않는다. Phase 4가 face/instance→Layer 표를 생성하고 Phase 5 runtime snapshot이 registry에 결합한다.

## Gate 0 — worker 동기 scene query

- [ ] Mass worker에서 line, sphere, capsule query 실행
- [ ] `bTickPhysicsAsync=True`와 동적 kinematic body 조건에서 ensure·check·data race 부재
- [ ] Editor와 `-game` 리슨 서버 2P에서 결과 일치
- [ ] wrapper 총시간과 scene read-lock 대기를 분리 계측
- [ ] query 종류별 count, P50/P95, 최악 프레임 기록
- [ ] 미해석 hit 수와 dynamic/static 분류 오류 기록
- [ ] 실패 시 D-025 재논의 후 구현 진행 중단

## 구현 단위

### 1. MassWorldCollision API

- [ ] `RaycastWorld`
- [ ] `SweepSphereWorld`
- [ ] `SweepCapsuleWorld`
- [ ] `ProbeSupport`
- [ ] `LNPWorldExact` query params와 self/owner 제외 규칙
- [ ] worker-safe POD result와 hit identity 결과
- [ ] debug draw queue와 Unreal Insights marker
- [ ] correctness-mandatory와 optional query counter 분리

### 2. 투사체·탄도 가이드

- [ ] 서버 투사체의 `PreviousPos → CurrentPos` exact segment/sphere sweep
- [ ] Mass target hit time과 world hit time 비교 후 earliest hit 하나만 채택
- [ ] 클라이언트 ghost에 같은 world 판정 적용
- [ ] 게임 스레드 `PredictArc`에 같은 충돌 함수 적용
- [ ] `IsUnderSurface`와 반지름 기반 착탄 제거
- [ ] 정적 bounds와 marker swept bounds를 합친 world collision envelope 안전망

### 3. Placement Marker와 동적 패널

- [ ] marker class, `MarkerId`, path authoring 데이터 최소 계약 구현
- [ ] 서버만 loaded LVI marker를 `(slot, MarkerId)`로 수집
- [ ] 공통 서버 스폰 함수로 복제 Actor 생성
- [ ] path revision, 상태, server epoch, 시작 시각 초기 복제
- [ ] transform tick을 Mover·Mass query·DynamicSupport snapshot보다 앞에 배치
- [ ] late join에서 같은 자세 복원
- [ ] 2P Mover movement base 직렬화와 예측 안정성 확인
- [ ] PureEntity용 DynamicSupport POD snapshot의 기반 구조 확인

### 4. 부하 기준선

측정 전에 다음 값을 실행 문서나 보고서에 고정한다.

- 적 수와 90% 이상 PureEntity인 CombatMode 비율
- 동시 투사체 수와 속도 분포
- 플레이어·ActorPromoted·동적 body 수
- warm-up과 capture 시간
- Development/Test build, CPU, async physics 설정
- 성공 기준 frame budget과 P50/P95

Phase 3b와 Phase 6은 같은 harness와 seed를 사용한다.

## 자동화·회귀

- [ ] 회귀 맵의 벽·섬 측벽·동굴·정적 프랍·동적 패널 exact hit
- [ ] Decoration miss와 Pawn 제외
- [ ] world/entity earliest hit
- [ ] 내부형 shell normal
- [ ] 한 component의 분리된 sheet face identity와 ISM/HISM instance identity
- [ ] production 8-slot exact collision
- [ ] 패널 탑승 2P와 이동 중 late join
- [ ] Editor 자동화
- [ ] `-game` 리슨 서버 2P 스모크(D-031)

## 완료 조건

- [ ] Gate -1과 Gate 0이 독립적으로 통과하고 증거가 로그에 남음
- [ ] production 콘텐츠에서 `LNPWorldExact` 기본 전환 후 관통 회귀가 없음
- [ ] worker가 UObject를 역참조하지 않고 exact hit 의미를 해석함
- [ ] 투사체·ghost·탄도 가이드가 같은 world collision 함수를 사용함
- [ ] 동적 패널이 late join 포함 2P Mover의 valid movement base로 동작함
- [ ] 고정 부하 시나리오의 query 비용·락 대기 기준선 확보
- [ ] `LootNPopEditor Win64 Development`와 `LootNPop Win64 Development` 성공
- [ ] `Current.md`, `Roadmap.md`, `../history/Phase03_Log.md` 갱신

## 제외 범위

- 실제 Support Atlas rasterization과 runtime query
- PureEntity exact 접지 프로토타입(Phase 3b)
- 완전 비행 NPC(병렬 Phase 3c)
- Nav Grid와 A*
- Conditional Patch 구현
- Support 기반 투사체 horizon
