# 엘리트 NPC 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: E0 설계 문서화 완료 — 다음 세션에서 E1(스폰 구성·Roam 배치) 착수
> 마지막 갱신: 2026-11-05

## 현재 결정

사용자가 정한 것(2026-11-05):

- 두뇌는 Mass StateTree(매크로) + Actor StateTreeComponent(전투 패턴)로 나눈다. BT는 쓰지 않는다(EL-001).
- 코드에는 등급 어휘를 넣지 않는다. 축은 귀속 방식 `Pod / Arena / Roam`이다(EL-002).
- Pod 적은 전부 PureEntity로 바꾸고 검증 오류로 강제한다(EL-003).
- 아레나 이탈 시 귀환 + 완전 리셋, 엘리트는 세션당 1회(EL-006).

## 지금 코드의 출발점 (2026-11-05 조사)

- 스폰 구성 `ULNPMassSpawnConfig`(`Source/LootNPop/DataAsset/LNPMassSpawnConfig.h`)에는 Pod 세트 배열만 있다. 적은 반드시 Pod에 딸려 스폰된다.
- CombatMode는 적 타입 에셋 `ULNPEnemyConfig::CombatMode`(`Enemy/LNPEnemyConfig.h:569`)에 있다. 기본값은 `ActorPromoted`다.
- Pod 귀속이 무효인 적은 `ULNPEnemyPathProcessor`가 1초마다 `SelectHomePod`로 가까운 Pod에 다시 붙인다(`Enemy/LNPEnemyPathProcessor.cpp:284-322`). 엘리트는 이 경로에서 빼야 한다.
- 무귀속 적은 항상 Active라는 규칙과 테스트가 이미 있다(D-067, `Tests/LNPEnemyActivityTest.cpp:166`).
- 적 처치 보상 경로, 아레나 마커, 다중 공격 패턴, 적 사망 델리게이트는 모두 없다.
- 레벨 Actor를 런타임에 스캔하지 않는다. 수동 배치 마커는 에디터에서 Spawn 스트림(`SurfaceNavigation/LNPSpawnData.h`, codec 3)에 베이크된다.

## 바로 다음 행동

- E1 착수 전에 `phases/PhaseE1_UnboundSpawn.md`를 만든다(`Roadmap.md` §3 E1 행 기준).
- E1 첫 작업: `FLNPUnboundEnemySpawnEntry`와 `ULNPMassSpawnConfig::UnboundEnemies` 추가, `IsDataValid` 규칙(Architecture §2.2), `LNPMassSpawnPlanTest`에 "엘리트 엔트리를 넣어도 Pod 계획 불변" 테스트를 먼저 쓴다.
- `DA_MassSpawnConfig`의 Pod 편성에서 `DA_EnemyEntityConfig_ActorPromoted_Melee01`을 PureEntity로 바꾸는 작업은 E1에서 검증 규칙과 같이 한다. 기본 편성 보존 규칙의 예외이며, 사용자가 명시적으로 요청했다.

## 인계 근거

- 빌드·PIE 검증 방법: 프로젝트 공통 워크플로를 따른다. 커밋한 뒤에는 `-DisableAdaptiveUnity`로 전체 unity 구성을 한 번 확인한다.
- 검증 편성은 별도 에셋 + `LNP.Spawn.ConfigOverride`로 쓴다. 이 오버라이드는 호스트와 게스트 모두에 줘야 한다(`OnWorldBeginPlay`가 양쪽에서 템플릿을 등록한다).
