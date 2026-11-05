# 엘리트 NPC 검증 규약

> 상태: 초안
> 소유: Phase별 검증 시나리오, 공통 회귀, 로그 판정 기준
> 마지막 갱신: 2026-11-05

## 1. 공통 준비

- 검증 편성은 별도 에셋(`DA_MassSpawnConfig_EliteTest` 등)으로 만들고 `LNP.Spawn.ConfigOverride` 또는 `-LNPSpawnConfig=`로 지정한다. 기본 `DA_MassSpawnConfig`는 검증용으로 바꾸지 않는다.
- 오버라이드는 호스트·게스트 모두에 준다(양쪽에서 템플릿을 등록한다).
- 같은 시드로 반복하려면 `OctantGenSeed`를 고정한다.
- 빌드: 프로젝트 공통 워크플로. 커밋 뒤 `-DisableAdaptiveUnity`로 한 번 더 확인한다.

## 2. 공통 회귀 (모든 Phase)

| 항목 | 확인 방법 | 통과 기준 |
|:---|:---|:---|
| Pod·Pod 적 배치 불변 | `LNPMassSpawnPlanTest`, 같은 시드 PIE의 스폰 로그 비교 | 엘리트 엔트리 유무와 무관하게 Pod·적 위치가 같다 |
| Pod 루팅·보상 | PIE에서 Pod 하나 루팅 | Popped 후 Dice 3~4개, Confetti |
| 기존 PureEntity 전투 | Pod 적과 교전 | 근접·원거리·경직·패링이 이전과 같다 |
| 빌드 | Build.bat | 깨끗한 트리에서 통과 |

## 3. Phase별 시나리오

### E1 스폰 구성·Roam 배치

- 데이터 검증: Pod 편성에 ActorPromoted를 넣으면 오류, Unbound에 Pod 모드·빈 ID·중복 ID를 넣으면 오류.
- 계획 테스트: Unbound Roam 2기 추가 전후의 Pod 계획 동일, 같은 시드 두 번 결과 동일, Roam 위치가 모든 Pod에서 `MinDistanceFromPods` 이상.
- PIE 로그: `[UnboundSpawn]` 줄 수 = 배치 수. 30초 동안 `[SurfaceNavHome]`에 엘리트 엔티티가 나오지 않는다(재귀속 없음).
- 임시 기간 확인: 엘리트가 `ST_EnemyEntity` IdleTask로 이상 동작(교착·낙하)을 하지 않는다.

### E2 사망 드랍

- 1P: Roam 엘리트 처치 → 사망 지점에 Dice가 `DeathRewards` 범위 수만큼.
- 2P: 게스트가 Dice를 보고 줍는다. 호스트 쪽에서 중복 스폰 없음(서버 로그 1회).
- Pod 보상 회귀: `SpawnRewardSet` 추출 후 Pod Popped 보상 수·후보가 같다.
- 보상이 빈 적(Pod PureEntity)은 드랍하지 않는다.

### E3 아레나 마커·베이크

- `LNPSpawnDataTest`: codec v4 왕복, 정렬·중복 ID 검증, 옛 v3 페이로드 Decode 실패.
- 재베이크 후 각 옥탄트 베이크 리포트에 아레나 앵커 수가 나온다.
- PIE: 아레나 엘리트가 마커 반경 안에 스폰된다. 태그가 없는 조합에서는 `[UnboundPlan] placed=0/N` 경고만 나고 진행된다.

### E4 Mass 매크로 두뇌

- Arena: 아레나 밖에서 사격 → 교전하지 않음, 깎인 HP는 리셋 지연 뒤 회복(`[UnboundReset]`).
- Arena: 안에서 교전 → 밖으로 나감 → 앵커로 귀환 → 리셋 1회.
- Arena: 슬롯이 잠깐 끊겨도(2P에서 타겟 전환) 리셋되지 않는다.
- Roam: 5분 동안 옥탄트 이음매를 넘어 순회한다. 교착 시 타임아웃 복구.
- 의도 소유권: Engage 중 `ULNPEnemyMovementProcessor`가 의도를 쓰지 않는다(디버그 로그 또는 브레이크포인트). Actor 강등 시 Chase 폴백으로 내려가고 엔티티 이동이 이어진다.

### E5 Actor 전투 두뇌 프레임워크

- 시험 엘리트(패턴 2개: 근거리·중거리)가 거리 대역에 따라 패턴을 고른다. StateTree 디버거로 상태 전이 확인.
- 패턴 중 경직 누적 → 패턴 어빌리티가 취소되고 Staggered 상태로 간다.
- 패턴 공격을 패링 → 패링 브레이크·넉백이 기존과 같다. 저스트 회피도 성립한다.
- 2P: 게스트 화면에서 패턴 몽타주가 재생된다.

### E6 첫 엘리트 콘텐츠·2P

- 아레나 보스: 2페이즈 전환 1회, 리셋 후 1페이즈 복귀, 처치 시 드랍.
- 배회형: 마주침 → 교전 → 놓침 → 배회 재개.
- 대역폭: [Guide_NetBandwidth.md](../../Guide_NetBandwidth.md) 절차로 Promoted Actor 1기당 B/s 측정. 700~900 B/s 범위를 넘으면 원인을 분리한다.
- 패키지 프레임: 에디터 `-game`이 아닌 패키지로 판정한다.

## 4. 로그 문자열 (구현 시 추가)

| 문자열 | 위치 | 의미 |
|:---|:---|:---|
| `[UnboundPlan]` | `BuildUnboundPlan` | 엔트리별 배치 결과 |
| `[UnboundSpawn]` | `SetupSpawnedEntities` | 엘리트 스폰 1기 |
| `[UnboundReset]` | `ULNPEnemyHomeProcessor` | 해제 리셋 1회 |
| `[CombatBrain]` | `StartCombatBrain`/`StopCombatBrain` | 전투 두뇌 시작·종료 |
| `[DeathReward]` | `FLNPEnemyDeathRewardCommand` | 사망 드랍 수 |
