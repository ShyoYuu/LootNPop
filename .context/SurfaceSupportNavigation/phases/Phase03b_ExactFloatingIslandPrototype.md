# Phase 3b — exact 전용 부유섬 프로토타입

> 상태: 초안 — §3 결정 사항 확정 대기
> 예상 범위: 2~3세션
> 선행 조건: Phase 3 MassWorldCollision 정확성 기준선(완료)

## 1. 목표

Support 캐시를 만들기 전에 두 가지를 실측한다(D-032).

1. 300m 내부형 구에서 부유섬이 플레이 공간으로 재미있는가
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

## 3. 결정 사항

각 항목은 선택지와 권장안이다. 확정되면 이 절을 결론만 남기고, 프로젝트 전체가 지킬 결정은 `../Decisions.md`에 추가한다.

### 3.1 greybox 부유섬 옥탄트 구성

| 항목 | 권장안 |
|:---|:---|
| definition 수 | 1개(`SkyIsland_00`). 8 slot 모두 같은 definition을 회전 배치한다 |
| 지각 | `BP_OctantGenerator` Radius 30,000, 기존 Meadow와 비슷한 노이즈. 기존 테마 PCG로 지각에만 프랍 배치 |
| 섬 | 3개. 소(지름 약 2,000cm, 지각 위 약 1,200cm), 중(4,000cm, 2,500cm), 대(6,000cm, 4,000cm) |
| 경사로 섬 | 중 섬 하나는 지각과 자연 경사로로 이어 붙인다. PureEntity가 걸어서 섬에 오르내리는 경로를 시험하고, Phase 7 Walk 연결의 입력이 된다 |
| 섬 형상 | 윗면은 평평하고 아래는 뾰족한 역원뿔형. 2~3종 mesh를 균일 scale로만 재사용한다. 비균등 scale은 Phase 4 fixture 검증 전이라 피한다 |
| 제작 도구 | 섬 mesh는 Blender MCP 또는 UE Modeling Tools. `LNPStaticTerrain`, complex-as-simple |
| 배치 제약 | 옥탄트 경계와 꼭짓점(좌표축) 부근을 피한다(D-030, `../design/TerrainContract.md` §7) |
| 플레이어 이동 수단 | 섬마다 Placement Marker로 스프링 런처 1개와 그래플 앵커 1~2개를 둔다. 마커 `ElementClass` 경로가 월드 장치에도 쓰이는지 구현 착수 때 확인한다 |

- 섬 윗면이 평평하면 지름 6,000cm 섬 가장자리에서 국지 Up과 약 6°가 어긋난다. walkable 한계(45°)보다 훨씬 작아 greybox에서는 문제가 없다.
- 대안: 섬을 옥탄트마다 다르게 두려면 definition 2~3개가 필요하다. 지금 선택 알고리즘은 slot 순서 greedy라 D-043 교체가 선행돼야 해서 권장하지 않는다.

### 3.2 반지름 30,000cm 전환과 기존 Meadow_00

| 선택지 | 내용 |
|:---|:---|
| A. Meadow_00을 pool에서 뺀다(권장) | 에셋은 참조용으로 남기고 `OctantPoolData`에는 `SkyIsland_00`만 둔다. 섬 없는 대조군은 같은 월드에서 섬이 없는 방향에 적을 두어 얻는다 |
| B. Meadow_00을 30,000cm로 다시 만든다 | 섬 없는 옥탄트를 대조군으로 보존한다. 제작 비용이 들고 pool에 definition이 둘이 되어 greedy 선택 문제가 생긴다 |

반지름을 바꾸면 함께 확인할 것:

- `TestMap03`의 PlayerStart 위치와 조명·하늘. 반지름 25,000 기준으로 배치돼 있다
- SurfaceCache 셀 간격. 해상도가 반지름에 비례해 늘어 샘플 수가 약 1.44배가 된다(bake 시간과 메모리 증가를 기록한다)
- world collision envelope, Mass 스폰 설정, 월드 장치·LootPod 시드 배치
- int16 복제 캡. 섬은 지각 안쪽이라 여유가 늘 뿐이다

Phase 3 부하 기준선은 25,000cm 월드에서 쟀다. 3b 수치는 30,000cm 월드에서 legacy와 exact를 같이 다시 재서 비교한다.

### 3.3 PureEntity exact 이동 알고리즘

**권장: 매 프레임 exact(캐시 없는 최악 조건).** 3b의 목적은 exact 전용 한계치이므로 호출을 줄이는 최적화를 넣지 않는다. 호출을 줄이는 것은 Phase 6 캐시의 역할이다.

접지 상태 한 프레임(엔티티당 query 2회):

1. **수평 sweep**: 현재 캡슐 중심을 step 높이만큼 Up으로 올린 뒤 이동 목표까지 capsule sweep한다. 벽·프랍·섬 측벽에 막히면 막힌 지점에서 멈추고, 남은 이동은 hit 법선의 접평면으로 한 번 미끄러뜨린다.
2. **하향 probe**: `ProbeSupport`(sphere)로 step 높이 + 최대 하강 거리를 내려 찍는다.
   - walkable 법선이고 `Support` 역할이면 캡슐 중심을 그 위에 둔다.
   - 지지면이 없으면 속도를 0으로 둔 채 공중 상태로 넘긴다. 이것이 절벽 끝의 낙하다.
   - 걸을 수 없는 경사는 수평 이동을 취소한다. 지금의 캐시 두 점 경사 검사를 대체한다.

공중 상태 한 프레임(엔티티당 query 1회):

- 중력을 적분하고 `이전 위치 → 제안 위치` capsule sweep의 earliest hit를 쓴다.
- walkable 법선이고 `Support` 역할이면 착지해 속도를 0으로 만든다.
- 걸을 수 없는 hit(섬 측벽·밑면)는 속도의 법선 성분을 지우고 미끄러진다. 멈추게 하면 섬 밑면에 달라붙는다.
- `UnknownExactSurface`는 Blocker로만 취급하고 착지하지 않는다(D-037).

선택지:

| 선택지 | 비용 | 문제 |
|:---|:---|:---|
| A. 하향 probe만(수평 sweep 없음) | 엔티티당 1회 | 지금처럼 벽·프랍·섬 측벽을 뚫고 걷는다. 동굴·경사로 섬에서 exact의 의미가 반감된다 |
| B. 하향 probe + 수평 sweep(권장) | 엔티티당 2회 | 없음. 수평 sweep 비용은 CVar로 끄고 켜서 따로 잰다 |
| C. 이동량이 작으면 probe를 건너뛴다 | 1회 미만 | 캐시 같은 최적화라 exact 한계치 측정의 의미가 흐려진다 |

### 3.4 CVar 전환 방식과 적용 범위

- `LNP.SurfaceNav.EnemyExactGround`(0 legacy / 1 exact)와 `LNP.SurfaceNav.EnemyExactLateralSweep`(0/1, 측정 요인 분리용).
- 이동 시뮬레이션은 서버 전용이라 CVar도 서버에서만 의미가 있다. 복제·클라이언트 경로는 바뀌지 않는다.
- **기본값 권장**: 구현과 2P 스모크 통과 뒤 1. 섬 월드에서 legacy는 섬 아래로 순간이동하므로 비교용으로만 남긴다. legacy는 Phase 6에서 제거한다.

적용 범위:

| 소비자 | 권장 |
|:---|:---|
| 적 이동(접지·공중·넉백·사망 팝) | exact 전환(3b 본체) |
| 배회 목표 | 캐시 방향을 쓰되, exact 모드에서는 엔티티의 현재 반지름에서 하향 probe로 다시 찍어 같은 층의 지면을 고른다. 안 그러면 섬 아래 적이 섬 윗면을 목표로 잡고 타임아웃을 반복한다 |
| Mass 스폰·Pod 배치 | 바꾸지 않는다. 캐시가 섬 윗면을 주므로 Pod가 섬 위에 생길 수 있다. 알려진 한계로 기록하고 Phase 5 Spawn stream에서 해결한다 |
| 부하 harness 배치 | exact probe로 지각과 섬 윗면에 직접 배치한다 |
| 탄도 가이드 보조 probe | 바꾸지 않는다. 판정 자체는 이미 exact다 |

### 3.5 Mass 페이즈와 동적 패널 순서

적 이동 프로세서는 PrePhysics 페이즈다. 움직이는 패널도 TG_PrePhysics Actor 틱에서 움직이므로 둘의 순서가 보장되지 않는다. `../design/RuntimeCollision.md`는 PrePhysics에 exact 소비자를 더할 때 이 순서를 다시 검토하라고 한다.

| 선택지 | 내용 |
|:---|:---|
| A. PrePhysics 유지, 락 대기 측정(권장) | 패널은 8개뿐이라 쓰기 겹침이 작을 것으로 본다. 락 P95가 0.2ms/frame을 넘을 때만 B·C를 한다. PureEntity의 패널 탑승(D-013)은 Phase 6 범위라 패널 자세를 반 프레임 늦게 보는 것은 3b에서 문제가 되지 않는다 |
| B. 이동 프로세서를 PostPhysics로 옮긴다 | 같은 프레임에 공격 위상을 읽는 순서 선언, Representation·복제 순서가 모두 흔들린다 |
| C. 패널 틱을 Mass PrePhysics 페이즈의 선행 조건으로 건다 | 순서는 확실해지지만 엔진 페이즈 tick function 선행 조건 API를 조사해야 한다 |

### 3.6 한계치 측정 시나리오

Phase 3 harness(`-LNPLoadBaseline=N`)를 확장해 재사용한다. 측정 build 규약은 Phase 3과 같다. 패키지 Development, 호스트 `-nullrhi`의 서버 CPU 프레임이고, 에디터 바이너리 `-game` 프레임은 판정에 쓰지 않는다.

| 항목 | 권장 |
|:---|:---|
| 배치 | 링 일부가 큰 섬 아래 지각을 지나도록 링 중심을 옮기고, 일부 적(예: 10%)은 섬 윗면에 둔다. 한 시나리오에 지각·섬 아래·섬 위·가장자리가 모두 들어간다 |
| 공중 상태 유발 | 플레이어가 무적이라 적을 때리지 않는다. harness가 일정 비율(예: 적마다 평균 10초에 한 번)로 합성 넉백을 주고, 섬 위 적은 가장자리 밖으로 밀어 떨어뜨린다 |
| 요인 | 적 수 N ∈ {300, 1000, 2000} × 접지 경로 {legacy, exact} 6회. 발사체 500발 고정. 수평 sweep 기여는 N=1000에서만 lateral 0/1로 1회를 더한다 |
| 한계치 | exact 경로에서 서버 CPU 프레임 P95 ≤ 16.6ms를 지키는 최대 N. 2000에서도 통과하면 2000 이상은 찾지 않고 "≥2000"으로 기록한다. 300~2000 사이에서 넘으면 이분 탐색 2~3회로 좁힌다 |
| 기록 | 서버 CPU 프레임 P50/P95, 접지·공중별 exact query 수와 호출당 비용, 프레임당 exact 합계, 락 대기, `UnknownHits`, 착지·낙하 이벤트 수, 섬 위 적의 가장자리 이탈 수, CPU 모델 |

이 수치가 Phase 4의 캐시 적중률 목표와 Phase 6 재측정의 기준선이다.

### 3.7 플레이 감각 평가

사용자 2P 플레이테스트(리슨 서버, 렌더링)로 판단한다. 수치 기준이 없는 항목이라 체크리스트로 남긴다.

- 섬을 아래에서 봤을 때 공간을 읽을 수 있는가(시야를 가리는 정도, 카메라가 섬 밑면에 걸리는 상황)
- 런처·그래플로 섬에 오르내리는 흐름이 자연스러운가
- 섬 위에 서면 근접 적에게서 안전한 지대가 되는가(D-011). 원거리 적과의 교전은 성립하는가(D-017)
- 넉백으로 섬에서 떨어지는 적과 플레이어가 납득할 만하게 보이는가
- 투사체 탄도 가이드가 섬 밑면·측벽에서 제대로 끊기는가

## 4. 구현 단위

1. **30,000cm greybox 옥탄트**: 지각 mesh, 섬 mesh, LVI, 월드 장치 마커, definition 등록, `SphereRadius` 30,000, `TestMap03` 배치 보정. 검증: 8 slot 생성, `ExactOracle` PASS, audit `MISSING=0`, envelope 갱신
2. **exact 접지 경로**: §3.3 알고리즘, CVar 두 개, 배회 목표 재투영. 검증: 자동화(지각 접지·절벽 낙하·섬 측벽 슬라이드·섬 밑면 충돌·경사로 오르기·Unknown 미착지), 2P 스모크
3. **측정**: harness 배치·합성 넉백 확장, §3.6 매트릭스, 결과를 `../history/Phase03b_Log.md`에 기록
4. **플레이테스트**: §3.7 체크리스트

## 5. 완료 조건

- [ ] 8 slot 전부 30,000cm greybox 옥탄트로 생성되고 `ExactOracle`·exact response audit가 통과함
- [ ] PureEntity가 exact만으로 지각·섬 윗면에 접지하고, 가장자리에서 떨어지고, 넉백 뒤 올바른 층에 착지함
- [ ] 섬 아래 지각의 적이 섬 윗면으로 순간이동하지 않음(legacy 대비 회귀 확인)
- [ ] 벽·프랍·섬 측벽을 걸어서 통과하지 않음
- [ ] `UnknownHits=0`, 착지가 Unknown hit에 스냅하지 않음
- [ ] §3.6 매트릭스와 exact 한계치 기록
- [ ] §3.7 플레이테스트 결과 기록
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
