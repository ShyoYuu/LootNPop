# LootNPop 멀티플레이 네트워킹

> **UE 5.8 · Iris Replication + MassReplication 하이브리드 · GAS 클라이언트 예측 · Mover 2.0(Network Prediction)**
>
> 구형(Sphere) 월드에서 수백 규모 Mass 엔티티(적 NPC·발사체)와 소수 플레이어의 액션 전투를
> 리슨 서버 기준으로 동기화한다. 전 구간 구현·PIE 2/3인 검증 완료 (지연 `net.PktLag=150/250`·손실 시뮬레이션 포함).

---

## 1. 아키텍처 원칙

### 1.1 서버 권위 + 클라이언트 예측 분리

| 구분 | 담당 |
|:---|:---|
| **서버 전용** | GE 적용(HP 감소), 패링 판정, 엔티티 스폰/소멸 최종 결정 |
| **클라이언트 예측** | HitStop, 충격 VFX, 사운드, 공격 몽타주 즉시 재생, 쿨다운 반영 |

클라이언트 예측은 전부 코스메틱 범위로 한정한다. 예측이 틀려도 실제 HP는 서버가 결정하므로 게임플레이 무결성이 보장되고 치팅이 불가능하다.

### 1.2 MassEntity 네트워크 분류 — 두 경로

| 경로 | 대상 | 생성 주체 | 동기화 방식 |
|:---|:---|:---|:---|
| **A — 공격자 소유** | WeaponTrace, Projectile | 서버·클라이언트 각자 독립 생성 | 복제 없음 (발사체 관전은 Spawn-Only 방송, §3.3) |
| **B — 서버 소유** | Enemy NPC, LootPod, 플레이어 폰 | 서버 시뮬레이션 | MassReplication bubble + Actor 복제 이중화 (§3.5) |

### 1.3 크로스 머신 식별자 규칙

> **`FMassEntityHandle`은 절대 복제 스키마에 포함하지 않는다.** 각 머신의 EntityManager 슬롯 인덱스일 뿐이라 네트워크로 보내면 수신 측에서 엉뚱한 엔티티를 가리킨다.

| 용도 | 사용 식별자 |
|:---|:---|
| 서버 소유 엔티티 | `FMassNetworkID` (서버 발급, 세션 내 전역 고유) |
| 플레이어 | `APlayerState::GetPlayerId()` |
| 팀/타입 | `FGameplayTag` |
| Ghost 발사체 | `FLNPGhostKey { InstigatorPlayerID, KeyOrSalvo, SpawnIndex }` (§3.2) |

---

## 2. 복제 스택 구성

### 2.1 Iris + MassReplication 하이브리드

`LootNPop.Build.cs`에 `IrisCore`, `NetCore`, `MassReplication` 모듈, `DefaultEngine.ini`에 `net.Iris.UseIrisReplication=1`.

| 경로 | 대상 | 비고 |
|:---|:---|:---|
| **Iris 네이티브** | Actor 기반 객체 전체 (GameState, PlayerState/ASC, 캐릭터, LootPod Actor) | `DOREPLIFETIME` 선언이 그대로 Iris Descriptor의 입력이 됨 — 엔진(`AActor::RegisterReplicationFragments`)이 자동 변환 |
| **Iris 레거시 호환** | Mass 엔티티 bubble (Enemy·Player·LootPod) | MassReplication은 IrisCore 무의존 — 자체 공간 Relevancy·LOD 감쇠·컴팩트 직렬화로 충분히 효율적 |

### 2.2 GAS 복제 모드

| ASC | 모드 | 근거 |
|:---|:---|:---|
| 플레이어 (`ALNPPlayerState`) | `Mixed` | GE·Attribute는 소유자에게 완전 복제, 타 클라이언트는 태그로 추론 |
| 적 NPC | `Minimal` | Gameplay Tag 집합만 복제 — HP Bar·상태 표시에 충분, GE 인스턴스 복제 생략으로 대역폭 절약 |

---

## 3. 핵심 테크닉

### 3.1 Lag Compensation — 서버 위치 되감기

공격자 RTT/2만큼 과거의 피격 대상 위치로 서버 히트 판정을 수행해 "분명히 맞았는데 빗나감"을 해소한다.

- **위치 히스토리:** `FLNPPositionHistoryFragment` — 50ms 간격 × 5샘플 링버퍼(최대 200ms 되감기). Enemy·플레이어 폰 아키타입에 공통 부착, 서버 전용 프로세서(`ULNPPositionHistoryRecordProcessor`)가 기록.
- **판정:** 공격자 `GetPingInMilliseconds() × 0.0005`로 RTT/2(초)를 구해 200ms로 클램프한 뒤, 히스토리를 보간 조회한 캡슐 중심(`RewoundCenter`)으로 판정한다.
- **근접 vs 원거리의 되감기 시점 차이:**

| 판정 | RewindSeconds 산출 | 근거 |
|:---|:---|:---|
| 근접 (WeaponTrace) | 매 프레임 재계산 | 활성 창이 1초 미만이라 체감 차이 없음 |
| 원거리 (Projectile) | **발사 시점 1회 캐싱** (`FLNPProjectileFragment::CachedRewindSeconds`) | 최대 5초를 비행하는 발사체가 매 프레임 되감으면 "대상이 이미 피했는데 과거 잔상을 쫓아가 맞는" 체감이 됨 — "조준한 순간의 지연"만 보정. 매 프레임 Ping 조회 비용도 제거 |

- **패링 반사 후:** 반사된 발사체의 Lag Compensation 기준은 방어자다 — 반사 시점에 방어자 RTT/2로 재캐싱.
- **비용 특성:** 히스토리 버퍼는 피격 대상(Enemy·Player) 쪽에만 존재하고 발사체 자신은 기록하지 않으므로, 발사체가 수백 개 생성·소멸해도 기록 비용이 늘지 않는다.

**패링 타이밍 RTT 역보정 —** 패링은 위치가 아닌 **입력 타이밍** 판정이므로 위치 되감기와 별개로 처리한다. 서버가 Guard 입력을 수신한 시각에서 방어자 RTT/2를 빼 실제 입력 시각을 복원해 패링 창 만료 시각(`ParryWindowExpiryTime`)을 계산한다. 보정 클램프는 패링 창의 절반(0.075s)으로 보수적 설정.

### 3.2 클라이언트 예측 — GAS LocalPredicted + Ghost Projectile

모든 어빌리티(근접·원거리 공격, 대시, 가드)는 `LocalPredicted`로 실행된다. `CommitAbility`로 쿨다운·코스트 GE가 클라이언트에 즉시 적용되고, 서버 불승인 시 GAS가 자동 롤백한다.

**히트 판정 이중 경로 (근접·원거리 공통 구조):**

```
[공격자 클라이언트]                                [서버]
  엔티티 독립 생성 (bIsLocalInstigator=true)         엔티티 독립 생성
  복제된 Actor 캡슐 대상 로컬 판정                    Mass 쿼리 + Lag Compensation 판정
  → HitStop·VFX 즉시 재생 (코스메틱)                 → GE 적용 (실제 HP 감소)
                                                   → GameplayCue로 전 클라 전파
```

`HasAuthority()` 분기 없이 서버·클라이언트가 각자 엔티티를 생성하고, 프로세서 내부에서 권한별 경로를 분리한다(§3.8).

**Ghost Projectile — 예측 발사체의 전역 고유 식별:**

- 소유 클라이언트가 예측 스폰한 발사체(Ghost)는 `FLNPGhostKey { InstigatorPlayerID, KeyOrSalvo, SpawnIndex }`로 식별한다. GAS PredictionKey는 클라이언트별 로컬 카운터라 단독으로는 전역 고유가 아니므로(플레이어 간 충돌, 리슨 호스트·NPC 발사는 항상 키=0) 공격자 ID를 묶고, 예측 키가 없는 발사는 서버 발급 SalvoID(≥65536, uint16 예측 키 공간과 분리)로 대체한다.
- **오예측 롤백:** 서버가 `CommitAbility`를 거부하면 `FPredictionKey`의 rejected delegate로 해당 발사 이벤트의 Ghost만 일괄 파괴.
- **수명:** per-entry 만료(`ProjectileLifetime + 1s`) — 정상 비행 Ghost는 자체 수명·지형 판정이 먼저 소멸시키고, 스윕은 stale 엔트리 GC만 담당.
- **임팩트 VFX 중복 방지:** 로컬 판정이 Ghost를 파괴할 때 키를 잠시 기록하고, 서버 확정 GameplayCue 핸들러는 기록 유무만 확인(`ConsumeRecentLocalImpact`)하는 **단일 경로** — 공격자/관전자/리슨 호스트 분기가 필요 없다.

**공격자 HitStop 정책:** 물리적 타격감이 필요한 **근접 전용**. 공격자 본인 화면은 예측 경로로 즉시(서버 왕복 대기 없음), 제3자 화면은 공격자 ASC에 실행되는 `GameplayCue.LNP.Melee.AttackerHitStop`으로 전파하되 핸들러가 `IsLocallyControlled()`면 no-op — 본인 화면과 이중 재생을 방지한다.

### 3.3 발사체 관전 동기화 — Spawn-Only 방송 + 독립 시뮬레이션

발사체는 "빠르고, 수명이 짧고, 수가 많은" 엔티티라 매 프레임 위치 복제가 부적합하다 — 탄속이 빠르면 복제 지연을 물리적으로 이길 수 없어 보간·외삽 아티팩트만 추가된다. 대신 **발사 시점 1회 Reliable Multicast로 스폰 파라미터(위치·속도·수명·Ghost 키)만 방송**하고, 각 클라이언트가 시각 전용 로컬 엔티티를 독립 시뮬레이션한다.

- **신뢰성 기준:** *지속 위치 복제는 Unreliable, 존재를 결정하는 1회성 이벤트는 Reliable* — 발사·반사 방송은 유실 시 발사체가 통째로 안 보이므로 Reliable.
- **관전용 Ghost도 로컬 충돌 판정** — 캐릭터 충돌 시 즉시 소멸+VFX로 "관통 후 뒤늦게 터지는" 잔상을 제거. 서버 확정 Cue와의 VFX 중복은 로컬 재생 기록(§3.2)으로 방지.
- **패링 반사 = 소멸 + 재스폰:** 공격자 클라이언트는 패링을 모른 채 히트 오예측으로 Ghost를 이미 파괴했을 수 있어, in-place 속도 반전으로는 화면 공백이 생긴다. 서버는 권위 엔티티의 속도·진영을 반전하며 식별자를 재발급(SalvoID·방어자 ID)하고, 클라이언트는 구 키 Ghost 파괴 + 서버 확정 반사 지점에서 새 Ghost 스폰으로 처리 — 스폰 경로가 발사 방송과 공용이라 Dead Reckoning(§3.4)이 자동 적용된다.

### 3.4 Dead Reckoning — 관전 Ghost 외삽 스폰

방송이 관전자에게 도착한 시점엔 실제 서버 발사체는 이미 지연만큼 더 날아가 있다. Lag Compensation이 판정을 **과거로** 되감는 것과 반대로, 관전 Ghost는 스폰 위치를 **미래로** 외삽한다:

```
지연 = UpstreamDelay(발사 방송은 공격자 RTT/2, 반사 방송은 0) + 수신자 RTT/2   (클램프 200ms)
스폰 위치 = SpawnPos + Velocity × 지연,  잔여 수명도 지연만큼 차감
```

외삽 시점에 이미 수명이 다한 발사체는 스폰을 생략한다. PIE 3인 로그에서 ≈184ms 외삽이 정상 동작함을 실측.

### 3.5 MassReplication 이중 복제 — Enemy · Player · LootPod

서버 소유 엔티티(경로 B)는 두 레이어로 복제된다:

| 레이어 | 수신 대상 | 역할 |
|:---|:---|:---|
| **Mass bubble** (`FMassNetworkID`) | 모든 클라이언트 | 엔티티 존재·위치 — Low LOD 시각화(ISMC/Niagara) 기반 |
| **Actor 복제** (거리 Relevancy) | 근접 클라이언트만 | High LOD — HP Bar·애니메이션·히트 판정 타겟 |

**서버 LOD 정책:** 서버도 기본적으로 Mass 엔티티만 시뮬레이션한다. `UMassRepresentationProcessor`가 플레이어 거리 기준으로 Actor를 동적 스폰/소멸하며, 모든 플레이어에게서 멀면 서버에도 Actor가 없다 — 수백 규모 적을 처리하는 핵심 부하 절감 포인트.

**통합 단일 버블 — 모든 타입이 하나의 복제 스트림을 공유:**

세 타입 모두 `ALNPMassClientBubbleInfo` / `ULNPMassReplicator`(`Replication/LNPMassReplication.h`, `LNPMassReplicator.h`) 하나로 복제된다. 엔진의 파괴 처리 경로(`CalculateClientReplication`의 클라 장부 순회)가 타입 무구분이라, 버블 클래스가 2개 이상이면 타 타입 파괴 엔트리를 자기 버블 핸들로 제거 시도해 크래시(2P 루팅 게이지 완료 시 실측)하기 때문 — 상세는 `EngineAnalysis_MassReplication.md` §7.1. 이종 아키타입 스폰은 엔진의 TemplateID 그룹핑이 자동 처리하고, 타입 분기는 `FLNPEnemyFragment` Optional 요구(서버)·`FMassEntityView` 조건부 접근(클라)으로 해결한다.

| 타입 | 페이로드 | 갱신 |
|:---|:---|:---|
| Enemy | PositionYaw + EnemyTypeTag | 지속 (서버 AI가 매 Tick 이동) |
| 플레이어 폰 | 존재 + 스폰 시점 PositionYaw 1회 | **없음** — 위치는 Mover가 이미 고빈도·예측·보간 복제 중이라 bubble에 실으면 같은 데이터의 저품질 사본만 회선에 추가됨 |
| LootPod | 존재 + 스폰 시점 PositionYaw 1회 | 없음 (정적) — 상태·게이지는 Actor 복제 담당 |

스폰 위치를 1회 싣는 이유: 퍼펫 링크 시 엔진이 엔티티 Transform으로 Actor 위치를 초기화하므로 비워두면 원점으로 튄다. 구 Player 복제 클래스를 참조하던 `DA_PlayerEntityConfig`는 `DefaultEngine.ini`의 CoreRedirects로 통합 클래스에 연결돼 있다 (에디터에서 재저장하면 영구 반영 후 제거 가능).

**자세 인코딩 — 접평면 로컬 Yaw (2026-08-05):**

엔진 기본 핸들러는 월드 Yaw만 싣고 클라이언트에서 `FQuat(FVector::UpVector, Yaw)`로 복원하므로, 구 내벽 월드에서는 적도 부근 엔티티가 통째로 눕는다 (`EngineAnalysis_MassReplication.md` §7.9). 그래서 `ULNPMassReplicator`·`FLNPMassClientBubbleHandler`가 엔진 핸들러를 쓰지 않고 직접 처리한다 — Yaw를 **접평면 기준 로컬 각도**로 인코딩하고, 기저(`MakeFromZ(-Position.Normalize())`)는 이미 복제 중인 위치에서 양쪽이 각자 재구성한다. **추가 대역폭 0.** 헬퍼는 `LNP::Replication`(`LNPMassReplication.h`)에 인코딩·디코딩 쌍으로 모여 있다.

극점 주변 약 3.5m 링에서 서버·클라 기저가 갈릴 수 있으나(§7.9), 남극은 PlayerStart 영역이라 적이 배치되지 않아 실플레이 공간 밖이다 — **알면서 수용한 트레이드오프.** 실제로 NPC 방향이 튀는 것이 관측되면 `FQuat::FindBetweenNormals`로 특이점을 링에서 점으로 줄이거나, 압축 쿼터니언 복제로 제거한다.

**가시 거리 — 복제와 시각화는 반드시 짝을 맞춘다:**

트레잇 프로퍼티 `ReplicationCullDistance`(`ULNPLootPodTrait`·`ULNPEnemyTrait`)가 `LNP::Replication::ConfigureParams`를 통해 `FMassReplicationParameters::LODDistance[Off]`에 주입된다. 이 값은 같은 EntityConfig의 `MassCrowdVisualizationTrait → VisibleLODDistance[Off]`와 **반드시 같아야 한다** — 크면 렌더링되지도 않을 엔티티에 대역폭을 쓰고, 작으면 "서버엔 보이는데 클라엔 안 보임"이 된다. 엔진 기본값 5,000cm를 그대로 두어 실제로 후자가 발생했다 (2026-08-05 수정).

| 타입 | 값 | 근거 |
|:---|---:|:---|
| LootPod | 60,000 | 월드 전체(반지름 25,000 × 2 = 50,000 초과). Pod은 스폰 1회 페이로드 후 갱신이 0이라, 거리 컬링을 두면 경계 왕복마다 Add/Remove가 반복돼 **오히려 비싸다** |
| Enemy | 12,000 | 갱신을 지속하므로 이 값이 곧 대역폭 |

개수 캡 `LODMaxCountPerViewer`는 엔진 기본 Low=300이 거리보다 먼저 걸리므로(`AdjustLODFromCount`가 캡에 맞춰 거리를 줄임) 넉넉히 열어 폭주 방지 안전망으로만 남겼다. 가시 범위 제어는 `ReplicationCullDistance` 하나로 일원화한다.

Pod과 Enemy의 값이 다르면 `FMassReplicationSharedFragment`가 타입별로 분리되는데, 엔진이 `ForEachSharedFragment`로 순회하는 정상 구성이며 **버블·리플리케이터는 여전히 하나**라 §7.1 불변식은 유지된다.

**템플릿 빌드 시점 — Mass 템플릿 warm-up은 `OnWorldBeginPlay` 이후여야 한다 (2026-08-19):**

`UMassReplicationTrait::BuildTemplate`은 `World.IsNetMode(NM_Standalone)`이면 조기 반환해 복제 프래그먼트(`FMassNetworkIDFragment` 등)와 `FMassReplicationParameters` 공유 프래그먼트를 **하나도 추가하지 않는다.** 문제는 `-game` 리슨 서버의 `GetNetMode()`가 월드 초기화 중에 `NM_Standalone`을 돌려준다는 점이다 — `UEngine::LoadMap`이 `InitWorld()`(월드 서브시스템 `Initialize()`) → `Listen(URL)`(NetDriver 생성) 순서로 진행하는데, `UWorld::AttemptDeriveFromURL()`은 `NextURL`과 `PendingNetGame->URL`만 볼 뿐 **현재 실행 URL의 `?Listen` 옵션은 보지 않기** 때문이다. PIE는 바로 뒤의 `WITH_EDITOR` 분기가 `PlayInEditorNetMode`를 돌려주므로 이 함정이 구조적으로 드러나지 않는다 (클라이언트는 `PendingNetGame`이 살아 있어 양쪽 모두 `NM_Client`로 정상).

`FMassEntityConfig::GetOrCreateEntityTemplate`은 결과를 `ConfigGuid` 키로 월드 수명 내내 캐시하므로, 한 번 잘못 만들어진 템플릿은 `Listen()` 이후에도 소급 수정되지 않는다 — 서버 엔티티가 복제 쿼리에 아예 잡히지 않아 게스트 버블이 영원히 빈다. 그래서 `ULNPMassSpawnSubsystem`의 템플릿 warm-up은 `Initialize()`가 아니라 `OnWorldBeginPlay()`에서 수행한다(NetDriver 생성 이후이자 `GameMode::StartPlay` 이전). 크래시도 경고도 없는 무음 실패라, warm-up 직후 Pod 템플릿에 `FMassNetworkIDFragment`가 있는지 확인하는 Error 로그를 트립와이어로 남겼다.

부수 효과로 `UMassRepresentationSubsystem`이 `Initialize()`에서 `SpawnVisualizer()`를 마친 뒤에 템플릿이 만들어지므로, `MassVisualizationTrait`가 무효 `StaticMeshDescHandle`을 받아 `StaticMeshInstance` LOD를 `None`으로 강등하는 경로도 함께 막힌다.

**퍼펫(Puppet) 링크 — bubble 엔티티 ↔ 복제 Actor 자동 연결:** 엔진 정식 경로인 `UMassAgentComponent`의 NetID 핸드셰이크를 사용한다. 서버가 컴포넌트의 복제 프로퍼티 `NetID`에 `FMassNetworkID`를 싣고, 클라이언트 `OnRep_NetID`가 bubble이 스폰한 엔티티를 `FindEntity(NetID)`로 찾아 연결한다(액터·엔티티 도착 순서 레이스는 엔진이 orphan 대기로 처리). 퍼펫 초기화가 EntityConfig 템플릿 조성을 엔티티에 더해주므로 서버와 동일한 아키타입이 클라이언트에서 성립하고, 클라이언트 예측 판정(근접 HitStop·관전 Ghost 충돌)이 로컬 Mass 쿼리만으로 동작한다. 이 과정에서 발견한 엔진 타이밍 갭은 §4.1 참조.

**Actor 복제 설정:** Enemy `NetUpdateFrequency=30`·`NetCullDistanceSquared=30000²`, LootPod `NetUpdateFrequency=10`·`20000²`. LootPod 게이지(`CurrentGaugePercent`)는 매 프레임 변하므로 2% 이상 변화 시에만 마킹(0/1 경계값은 항상 반영).

### 3.6 플레이어 이동·시선 — Mover 2.0

- **이동:** Mover 2.0(Network Prediction 백엔드)이 클라이언트 예측·서버 검증·시뮬레이티드 프록시 재현을 내장한다. `SprintModifier`·`GuardModifier`, 대시 쿨다운(`FLNPDashCooldownModifier`), 그리고 구형 중력의 곡률 보정 누산값(`CurvatureDelta`)까지 SyncState에 포함해 롤백 시 자동 복구된다 — 적도 부근 구면 이동에서도 원격 캐릭터의 Up 벡터가 항상 구 중심을 향함을 검증.
- **시뮬레이티드 프록시 = ForwardPredict** (`Config/DefaultNetworkPrediction.ini`, 2026-08-19 전환). 원격 캐릭터도 로컬 캐릭터와 동일하게 풀 시뮬레이션 후 롤백 보정한다. 기본값이던 `Interpolated`는 수신 프레임 사이를 보간해 재생하므로 구조적으로 항상 과거를 보여준다 — 전환 경위와 근거는 §4.4.
- **입력 의도 전달:** 질주·가드·대시는 전부 `FLNPModifierInputs`(InputCmd)를 탄다. 예측 파이프라인 바깥에서 상태를 바꾸면 권위가 재현하지 못해 로컬에서만 튀었다가 롤백된다 — 이동 문서 §7.1·§7.6.
- **시선(발사 피치·Aim Offset):** 신규 RPC·복제 프로퍼티 **0개**로 구현 — 이미 서버에 도착하고 있던 Mover InputCmd의 `ControlRotation`을 소비부만 연결했다 (§4.3). `GetBaseAimRotation()` 오버라이드 단일 진입점으로 서버 발사 방향과 관전자 화면 상체 자세(Aim Offset)가 함께 동기화된다.
  - `UMoverComponent::bSyncInputsForSimProxy`는 **제거했다**(2026-08-19). 보간 프록시가 InputCmd를 못 받는 것을 우회하려고 SyncState에 InputContainer를 동봉하던 옵션인데(엔진 주석에도 "intended to be temporary"로 명시), ForwardPredict에서는 프록시가 실제로 시뮬레이션되어 `CachedLastUsedInputCmd`가 일반 경로에서 채워진다. 매 프레임 실리던 InputContainer 페이로드가 함께 사라졌다.
- **무기 장착:** 서버 권위 전용. 클라이언트는 `Server_Equip*()` RPC만 보내고 로컬 선반영을 하지 않는다.
  복제되는 단일 원본은 `ULNPEquipmentComponent::WeaponSlot`이며, 비주얼은 거기서 파생된다 — §3.9.

### 3.7 코스메틱 전파 — GameplayCue 일원화

| 상황 | 수단 |
|:---|:---|
| GAS 이벤트(GE 적용, 피격 판정)가 트리거인 코스메틱 | **GameplayCue** — 수신 대상을 GAS가 자동 관리, Multicast RPC 대비 코드 단순 |
| GAS와 독립적인 애니메이션 (공격 시작 등) | **Multicast RPC** — 회피 타이밍 정보인 근접 몽타주는 Reliable, 순수 연출인 원거리는 Unreliable |

운용 중인 Cue: `Character.HitReact`(HitReact 몽타주+피격자 HitStop), `Melee.Impact`, `Melee.AttackerHitStop`(제3자 화면 공격자 HitStop), `Projectile.Impact`, `Guard.Block`, `Parry.Success`.

- **핸들러는 C++ 우선:** `UGameplayCueNotify_Static::OnExecute`는 `BlueprintNativeEvent`라 C++ `_Implementation` 오버라이드로 그래프 없이 동작한다. 블루프린트 에셋은 부모 클래스+태그+에셋 참조만 지정하는 얇은 래퍼.
- **타입 안전 컨텍스트:** float 필드에 값을 인코딩하는 대신 `FGameplayEffectContext`를 상속한 `FLNPProjectileImpactContext`(`GetScriptStruct`/`Duplicate`/`NetSerialize` 오버라이드)로 Ghost 키를 전달 — 수신 측이 자기 Ghost를 정확히 정리한다.

### 3.8 Mass 프로세서 공통 권한 패턴

모든 히트 판정 프로세서는 3-구역 패턴을 따른다:

```
Execute()
├── 공통 시뮬레이션 (서버·클라 동일): 위치 갱신, 수명 감소 등 순수 수학
├── 서버 전용 (bIsServer): GE 적용, 엔티티 소멸, 게임 상태 변경, Lag Compensation
└── 로컬 예측 (bIsLocalInstigator): HitStop, VFX, 사운드
```

`bIsServer = (GetNetMode() < NM_Client)` — Standalone·리슨/데디케이티드 서버 모두 true라 싱글 플레이에서도 서버 로직이 동일하게 실행된다. AI·게이지 등 게임 로직 프로세서는 전부 `NM_Client` 조기 반환.

### 3.9 장비 상태 복제 — 원본은 PlayerState 컴포넌트에 (2026-08-20)

**증상.** 2인 standalone에서 게스트 화면의 호스트 캐릭터 근접 히트박스 디버그 드로우가 얇게 그려졌다.
실측 로그:

```
게스트 → 호스트 캐릭터 (role=1 SimulatedProxy):  weaponDef=DA_Pistol    parryR=15.00
서버   → 같은 캐릭터   (role=3 Authority):       weaponDef=DA_LongSword parryR=50.00
```

**원인.** "장착 중인 무기"를 표현하는 데이터가 둘로 갈라져 있었다. 개념적 원본은 GAS 부여 핸들과
가방 인스턴스를 들고 있는 `ULNPEquipmentComponent::WeaponSlot`인데 **이쪽이 복제되지 않았고**,
캐시 역할인 `ALNPCharacterBase::EquippedWeaponData`만 복제됐다. 게다가 `ULNPEquipmentComponent::BeginPlay`가
권위 게이트 없이 `EquipWeapon(DefaultWeapon)`을 호출해, 프록시에는 그 `DefaultWeapon`이 영구히 남았다.
근접 판정 ANS는 모든 머신에서 실행되므로 롱소드를 휘두르는 캐릭터를 권총 데이터로 판정하고 있었다.

**해결.** 원본과 캐시의 방향을 뒤집었다. 상세 표와 도착 순서 규칙은
[TechDesign_Inventory.md §4.1](TechDesign_Inventory.md)에 있고, 네트워킹 관점의 요점만 적는다.

- `ULNPEquipmentComponent`를 복제 활성화하고 `WeaponSlot`을 `ReplicatedUsing=OnRep_WeaponSlot`으로.
  PlayerState는 `bAlwaysRelevant`이므로 프록시를 포함한 전 클라이언트가 받는다.
- 구조체에서 실제로 복제되는 것은 `Definition` 하나뿐이다. 나머지 3필드는 `NotReplicated`.
  특히 `SourceInstance`는 **반드시** 빠져야 한다 — 가방 인스턴스가 `COND_OwnerOnly` 서브오브젝트라
  비소유자 클라이언트에서 영원히 resolve되지 않고, Iris가 상태를 계속 dirty로 잡아 재전송이 멈추지 않는다.
- 쓰기 경로를 서버 권위 하나로 줄였다. 예측 쓰기와 OnRep 쓰기가 공존하는 것 자체가 분기의 원인이었다.
- 적 캐릭터는 `EquipmentComponent`가 없고 `InitializeOnce`가 서버 전용이므로(`ULNPEnemyActorInitializerProcessor`가
  클라 월드에서 early-return) `EnemyConfig`를 복제해 같은 역할을 시킨다. 부수 효과로 클라이언트의
  적 `GetActiveWeaponDef()`가 null을 반환하던 같은 종류의 구멍도 함께 막혔다.

**여전히 유효한 제약 (이번 작업과 무관).** 프록시에서 어빌리티 스펙으로 값을 조회하는 설계는 불가능하다 —
`UAbilitySystemComponent::ActivatableAbilities`는 `COND_ReplayOrOwner`라 프록시에서 배열이 비어 있고,
`FGameplayAbilitySpec::ActiveCount`는 `NotReplicated`라 `IsActive()`가 로컬 발동 머신에서만 true다.
그래서 `ANS_LNPMeleeHitWindow`는 `ParryRadius`를 어빌리티 **CDO**에서 읽는 폴백을 유지한다.

---

## 4. 구현 중 발견 — 엔진 소스 분석으로 해결한 이슈

### 4.1 `UMassAgentComponent`의 NetID 캐싱 타이밍 갭 — 에이전트 경로 퍼펫 실패

**증상:** Enemy 퍼펫은 정상인데 플레이어 폰만 클라이언트에서 `netIDValid=0`으로 영구 대기 — 파이프라인 전 구간(서버 NetID 발급 → bubble 전송 → 클라 엔티티 스폰)이 로그상 전부 정상인데도 링크가 성립하지 않았다.

**원인 (엔진 `MassAgentComponent.cpp` 분석):** `SetEntityHandleInternal`은 핸들 설정 순간 엔티티의 `FMassNetworkIDFragment`를 읽어 복제 프로퍼티에 **1회만** 캐싱하고 갱신 경로가 없다. 그런데 NetID 발급은 별도의 Add 옵저버(`UMassNetworkIDFragmentInitializer`)가 수행하므로 실행 순서에 의존한다:

| 생성 경로 | 순서 | 결과 |
|:---|:---|:---|
| 스포너 경로 (Enemy) | 엔티티 먼저 생성(옵저버 실행됨) → 액터 나중 링크 | 링크 시점에 NetID가 이미 유효 — 정상 |
| **에이전트 경로 (플레이어 폰)** | 컴포넌트 등록이 엔티티를 생성 → **옵저버 실행 전에 캐싱** | 무효값 캐싱, 영구 대기 |

**해결:** `ULNPMassAgentComponent` 서브클래스가 `SetEntityHandleInternal` 오버라이드에서 "서버 + 핸들 유효 + NetID 무효" 조합일 때 옵저버 실행 이후 재조회로 NetID를 채운다. 엔진 퍼펫 플로우 자체는 무수정 — 캐싱 시점만 보정.

### 4.2 `UAnimNotifyState` 인스턴스 공유 — 원격 클라이언트 근접 데미지 100% 유실

**증상:** 리슨 호스트의 근접 공격은 정상인데, 원격 클라이언트의 근접 공격은 서버가 히트 판정을 전혀 수행하지 못해 데미지가 전량 유실됐다.

**원인:** `UAnimNotifyState`는 해당 몽타주를 재생하는 **모든 AnimInstance가 공유하는 단일 오브젝트**다(UE 공식 문서 명시 제약). 기존 코드는 진행 중인 스윙의 엔티티 핸들을 노티파이 멤버 변수에 저장했는데, 서버 월드의 `NotifyBegin`이 값을 세팅한 직후 같은 캐릭터의 클라이언트 월드 `NotifyBegin`이 이를 덮어써 서버 판정용 참조가 유실됐다. **PIE 특유의 문제가 아니다** — 데디케이티드 서버에서도 두 캐릭터가 스윙 구간을 겹치면 동일하게 재현된다. PIE 2인은 "서버 실행분·클라 실행분이 항상 동시에 겹치는" 조건이라 100% 재현됐을 뿐.

**해결:** `MeshComp`(캐릭터별 유일)를 키로 하는 `TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveSwing>`으로 스윙 상태를 인스턴스별 분리. 교훈: **AnimNotifyState에 가변 상태를 두려면 반드시 호출 컨텍스트(MeshComp) 기준으로 분리해야 한다.**

### 4.3 발사 피치·Aim Offset 동기화 — "이미 도착하고 있던 데이터"의 소비부만 연결

**문제:** 원격 클라이언트의 발사 피치가 서버에 없어 서버 권위 발사체(와 그로부터 방송되는 관전 Ghost)가 항상 수평으로 발사됐고, 관전자 화면의 상체 자세(Aim Offset)도 동기화되지 않았다.

**분석:** 새 RPC를 추가하기 전에 Mover 입력 파이프라인을 추적한 결과, 소유 클라이언트의 시선 회전은 **이미 매 Tick 서버에 도착하고 있었다** — `OnProduceInput`이 `FCharacterDefaultInputs::ControlRotation`(축당 16비트 압축)을 InputCmd에 실어 보내고 Network Prediction 백엔드가 복제한다. 문제는 아무도 이 값을 읽지 않았던 것.

**해결 — 데이터 채널 신설 없이 소비부 3곳 연결:**

| 구간 | 수단 |
|:---|:---|
| 소유 클라 → 서버 | 기존 그대로 (추가 작업 없음) |
| 서버 → 관전자 | 프록시가 `GetLastInputCmd()`로 InputCmd를 조회 가능하게 만든다 (수단은 아래 참조) |
| 소비 단일 진입점 | `ALNPPlayerCharacter::GetBaseAimRotation()` 오버라이드 — 로컬 제어면 Control Rotation, 아니면 InputCmd의 `ControlRotation` 반환 |

AnimInstance는 원래부터 `GetBaseAimRotation()`으로 AimPitch/AimYaw를 산출하므로(구면 세계 보정 포함) **무수정**으로 상체 자세가 동기화되고, 서버의 원격 플레이어 발사 방향도 같은 함수로 해결된다.

> **"서버 → 관전자" 구간의 변천.** 최초 구현은 `UMoverComponent::bSyncInputsForSimProxy = true`(엔진 옵션, 플레이어 폰에만 적용)로 InputCmd를 SyncState에 동봉했다. 보간 프록시는 시뮬레이션을 돌지 않아 InputCmd를 받을 길이 없기 때문이다. **ForwardPredict 전환(§4.4) 이후 이 옵션은 제거했다** — 프록시가 실제로 시뮬레이션되면서 `CachedLastUsedInputCmd`가 일반 경로(`MoverComponent.cpp`의 시뮬레이션 틱·`SetSimulationOutput`)에서 채워지므로 우회책이 불필요해졌고, 매 프레임 실리던 InputContainer 페이로드도 함께 사라졌다.

### 4.4 "게스트 화면에서만 호스트가 1초 늦게 움직인다" — NetworkPrediction 보간 서비스 분석 (2026-08-19)

`UnrealEditor.exe -game`으로 리슨 서버와 게스트를 각각 띄워 관찰하던 중 발견했다. 지연이 **단방향**이었다 — 게스트를 조작하면 서버 화면은 즉시 반응하는데, 서버를 조작하면 게스트 화면이 1초 넘게 늦었다.

**단방향인 이유는 두 방향이 서로 다른 코드를 타기 때문이다.**

| 방향 | 경로 |
|:---|:---|
| 클라 → 서버 | `UNetworkPredictionComponent::ServerReceiveClientInput` **RPC**. 서버가 받은 InputCmd로 그 폰을 직접 시뮬레이션 → 보간 없음 |
| 서버 → 클라 | `ReplicationProxy_Simulated` **프로퍼티 복제**(`COND_SimulatedOnlyNoReplay`) + **보간 서비스** |

즉 지연은 언제나 "시뮬레이티드 프록시를 바라보는 쪽"에만 생긴다. 자기 폰은 로컬 예측이라 항상 즉시 반응하므로, 호스트 1명 + 게스트 1명 구성에서는 한쪽만 느린 것처럼 보인다. **게스트가 2명이면 서로를 볼 때도 같은 지연이 생긴다.**

**설계값 100ms의 출처.** `Config/DefaultNetworkPrediction.ini`가 `SimulatedProxyNetworkLOD=Interpolated`(엔진 기본값은 `ForwardPredict`)에 `FixedTickInterpolationBufferedMS=100`이었다. 보간 모드의 프록시는 100ms 분량 버퍼를 쌓고 그 과거를 재생하므로, 같은 PC라 핑이 0이어도 100ms는 남는다.

**1초는 설계값이 아니다.** 엔진 소스(`NetworkPredictionWorldManager.cpp`) 확인 결과, 고정 틱 보간 경로의 `InterpolateRate`는 `1.f` 아니면 `0.f`뿐이고 **따라잡기 분기도, 버퍼 상한 클램프도 없다**. 반면 Independent(가변) 틱 경로에는 보정이 있다:

```cpp
if (BufferedMS > MaxBufferedMS)   // IndependentTickInterpolationMaxBufferedMS=250
{
    UE_LOGF(LogNetworkPrediction, Warning, "Independent Interpolation fell behind. BufferedMS: %d", BufferedMS);
    VariableTickState.Interpolation.fTimeMS = LatestRecvTimeMS - DesiredBufferedMS;  // 스냅 복구
}
```

`NetworkPredictionSettings.h`에 `FixedTick...MaxBufferedMS`에 해당하는 설정 자체가 없다. 프로젝트는 `PreferredTickingPolicy=Fixed`라 보정 없는 쪽을 탄다. 정확히 왜 1초 부근에서 안정화되는지는 특정하지 못했다 — 타이밍 설정(`FixedStepRealTimeMS = (1/60)×1000`, `FixedStepMS = 16`)은 정상이었다.

**해결: `SimulatedProxyNetworkLOD=ForwardPredict`.** 보간 서비스를 아예 타지 않으므로 버퍼 지연도 위 결함도 무관해진다. 고정 틱 정책에서만 선택 가능한 옵션이다 — `NetworkPredictionConfig.h`의 `IndependentNetworkLODs`는 시뮬레이티드 프록시에 `Interpolated | SimExtrapolate`만 허용하고 `ForwardPredict`를 지원하지 않는다.

**대가와 부수 효과**

- 예측이 빗나가면 보정 스냅이 보인다. 이를 완화하는 것이 이미 켜져 있던 `bEnableFixedTickSmoothing`으로, 설정 주석에 "including forward-predicted sim proxies"라고 명시된 대로 원래 ForwardPredict와 짝을 이루는 옵션이다. 보간 모드에서는 이 옵션의 본래 용도가 놀고 있었다.
- ⚠️ **적 캐릭터 액터도 포워드 예측 대상이 된다.** `SimulatedProxyNetworkLOD`는 NetworkPrediction **전역** 설정이고, `ALNPEnemyCharacter`는 `ALNPCharacterBase`를 상속해 같은 NP 백엔드 Mover를 가지며 `bReplicates = true`다. 적 움직임도 보간 지연이 사라지는 이득이 있지만, 클라이언트마다 적 액터 1기당 풀 시뮬레이션 + 롤백이 돈다. 액터는 High LOD 적만 가지므로 상한은 있으나 **적이 몰리는 구간의 클라이언트 프레임은 별도 확인이 필요하다**(§6).
- ⚠️ 스무딩 서비스가 SyncState 컬렉션의 모든 데이터 구조에 `Interpolate`를 호출한다. 오버라이드가 없으면 기본 구현이 `check(false)`다 — 이동 문서 §7.1의 부가 발견 참조.

### 4.5 "달리는데 뒤로 툭툭 되돌아간다" — 서버 입력 버퍼 기아(fault)와 롤백 (2026-08-19)

PC가 버벅이는 상태에서 `-game` 2프로세스로 플레이하니 게스트의 자기 캐릭터가 전진 중 주기적으로 뒤로 당겨졌다. 결론부터: **서버 프레임 저하 자체가 아니라 서버의 InputCmd 버퍼 기아가 원인이며, 발산 버그가 아니다.**

**경로** (`NetworkPredictionService_Input.inl`의 `TRemoteInputService`, 141-162행)

서버가 그 클라이언트의 미처리 InputCmd를 모두 소비하면 `bFault = true`로 전환하고 **해당 폰의 시뮬레이션 진행을 멈춘다.** 버퍼가 `FaultLimit`만큼 다시 찰 때까지 대기하는 동안 클라이언트는 계속 앞으로 예측한다. 서버가 재개해 권위 상태를 보내면 그 값은 클라이언트 예측보다 뒤에 있으므로 롤백 + 리플레이가 돌고, 화면에는 "뒤로 당겨짐"으로 보인다. `FaultLimit`은 fault마다 +1 되어 `MaximumRemoteInputFaultLimit=6`에서 멈춘다 — 반복될수록 입력 지연을 더 쌓아 안정을 사는 구조라 **처음 몇 번이 가장 크게 튄다.**

`np.TimeDilation.Enable`은 엔진 기본값이 `true`라 서버가 클라 틱 주기를 미세 조정해 버퍼 수위를 맞추지만, 기본 보정폭이 1%/틱이라 완만한 드리프트용이고 갑작스러운 히치는 덮지 못한다.

**테스트 환경이 이 현상을 증폭한다 — 그리고 그 설정은 유지한다.**

`FixedTickFrameRate=60`인데 녹화용 실행 인자가 `t.MaxFPS 60` + `-corelimit=4`이고 두 프로세스가 같은 PC를 나눠 쓴다. 프레임 상한과 고정 틱이 같으면 클라이언트는 잘 돼야 프레임당 InputCmd 1개를 만들어 **헤드룸이 0**이다. 실제로 백그라운드 프로세스 몇 개를 종료하자 재현이 멈췄다 — 월드 위치가 아니라 CPU 경합과 상관관계가 있다는 것이 발산 버그가 아니라는 결정적 근거다. 이 인자들은 1P·2P 화면 동시 녹화를 위한 의도적 설정이고(빼면 비포그라운드 창이 심하게 버벅인다), 별도 PC·상한 없음 환경에서는 빈도가 크게 낮아지므로 **그대로 둔다.**

**남는 실환경 리스크와 진단 기준**

- 네트워크 내성은 이미 확보돼 있다. `FixedTickInputSendCount=6`이 InputCmd를 6개씩 중복 전송하므로 연속 5패킷 손실까지 버퍼가 마르지 않는다. 실환경 fault는 그보다 나쁜 지터·손실이어야 한다.
- ⚠️ **호스트 히치는 실환경에도 남는다.** 리슨 서버 호스트가 곧 플레이어라, 호스트가 히치하면(GC 스파이크·레벨 스트리밍·Mass 스폰 버스트) 누적 시간만큼 고정 틱을 몰아 처리하며 **접속한 전원의 버퍼를 동시에 굶긴다.**
- 구분: 특정 클라만 튀면 그 클라의 환경 문제, **전원이 동시에 튀면 호스트 히치.** 프레임이 멀쩡한데 특정 월드 위치에서만 반복되면 그것은 SyncState 누락에 의한 진짜 발산이므로 별건으로 파야 한다.
- 관측: `LogNetworkPrediction` verbosity를 Log로 올리면 롤백 프레임 로그가 찍히고(`RollbackFrame %d AHEAD of PendingFrame %d` — `NetworkPredictionWorldManager.cpp:227`), Network Prediction Insights 트레이스에는 fault가 별도 이벤트로 남는다.

---

## 5. 검증

- **PIE 2·3인 멀티플레이:** 근접·원거리 양방향 전투(호스트↔클라, 클라↔클라), PvP·NPC 가드/패링/반사, 무기 교체, 루팅, HitReact/HitStop 전파, 관전 화면 발사체 표시·피격 소멸 전수 확인.
- **지연·손실 시뮬레이션:** `net.PktLag=150/250`, `net.PktLoss=5` — Lag Compensation 체감 위화감 없음, 고지연 패링 성공, 구면 이동 Up 벡터 정상.
- **정량 실측:** 클라이언트 퍼펫 Transform 오차 `maxPlayerEntityActorGap=0cm`(프레임 단위 동기화), Dead Reckoning ≈184ms 외삽 스폰 정상 동작 로그 확인.
- **Standalone `-game` 모드 검증 병행:** PIE가 놓치는 초기화 순서 크래시 커버. 2026-08-19부터 **Mass 엔티티 Low LOD 가시성도 필수 항목**이다(§3.5 템플릿 빌드 시점) — PIE는 이 함정을 구조적으로 재현하지 못한다. 게스트 화면 원거리 빛기둥·Enemy 가시성 재검증 대기 중.
- **ForwardPredict 전환 + 대시 InputCmd 이관 (2026-08-19, `-game` 리슨서버 + 게스트 2인):**
  - 게스트 화면의 호스트 지연이 체감 불가 수준으로 소멸 (전환 전 1초+).
  - 적도 부근 질주 상호 관찰 — 구면 자세·Up 벡터 위화감 없음, 스무딩 보정 스냅 미관측.
  - `LNP.Debug.ShowSpeed` 실측으로 질주·가드 배율이 원격 관찰 측에서도 정상 반영 확인, 가드 애니메이션 동기화 정상.
  - 클라이언트 대시가 서버 화면에 정상 표시. 다방향·연속 대시에서 쿨다운 정상, **이중 발동 미관측**(입력 버퍼 창 0.05초 동안 의도 비트가 유지되지만 쿨다운 Modifier가 재진입을 차단).
  - 양방향 대시 몽타주 재생 정상. `bSyncInputsForSimProxy` 제거 후에도 원거리 무기 장비 시 Aim 회전 양방향 동기화 정상.

> 위 항목은 아직 **2인 구성**에서만 확인했다. 클라이언트 2명(호스트 조작 없음) 구성은 미검증 — 대시 버그가 "호스트는 되니 절반은 맞다"로 위장됐던 전례가 있으므로 3인 검증 시 클라↔클라 대시를 별도 항목으로 확인할 것.

## 6. 향후 과제

- 수백 엔티티 규모 대역폭·CPU 프로파일링 (MassReplication 레거시 호환 경로의 스케일 한계 확인)
- **적 다수 구간의 클라이언트 CPU 확인** — ForwardPredict가 전역 설정이라 High LOD 적 액터마다 클라이언트에서 풀 시뮬레이션 + 롤백이 돈다(§4.4). 부담이 확인되면 적 폰만 `Interpolated`로 되돌리는 개별 설정 경로를 파야 한다
- 관전 Ghost catch-up 수렴 (외삽 스폰의 총구 이펙트 단절이 체감되면 도입)
- NPC 원거리 공격의 방송 채널 분리 (현재는 공격자 Actor RPC — Low LOD NPC는 Actor가 없음. SalvoID 체계는 선반영 완료)
