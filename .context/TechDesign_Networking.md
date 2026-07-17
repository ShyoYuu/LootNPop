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

**퍼펫(Puppet) 링크 — bubble 엔티티 ↔ 복제 Actor 자동 연결:** 엔진 정식 경로인 `UMassAgentComponent`의 NetID 핸드셰이크를 사용한다. 서버가 컴포넌트의 복제 프로퍼티 `NetID`에 `FMassNetworkID`를 싣고, 클라이언트 `OnRep_NetID`가 bubble이 스폰한 엔티티를 `FindEntity(NetID)`로 찾아 연결한다(액터·엔티티 도착 순서 레이스는 엔진이 orphan 대기로 처리). 퍼펫 초기화가 EntityConfig 템플릿 조성을 엔티티에 더해주므로 서버와 동일한 아키타입이 클라이언트에서 성립하고, 클라이언트 예측 판정(근접 HitStop·관전 Ghost 충돌)이 로컬 Mass 쿼리만으로 동작한다. 이 과정에서 발견한 엔진 타이밍 갭은 §4.1 참조.

**Actor 복제 설정:** Enemy `NetUpdateFrequency=30`·`NetCullDistanceSquared=30000²`, LootPod `NetUpdateFrequency=10`·`20000²`. LootPod 게이지(`CurrentGaugePercent`)는 매 프레임 변하므로 2% 이상 변화 시에만 마킹(0/1 경계값은 항상 반영).

### 3.6 플레이어 이동·시선 — Mover 2.0

- **이동:** Mover 2.0(Network Prediction 백엔드)이 클라이언트 예측·서버 검증·시뮬레이티드 프록시 보간을 내장한다. `SprintModifier`·`GuardModifier`, 그리고 구형 중력의 곡률 보정 누산값(`CurvatureDelta`)까지 SyncState에 포함해 롤백 시 자동 복구된다 — 적도 부근 구면 이동에서도 원격 캐릭터의 Up 벡터가 항상 구 중심을 향함을 검증.
- **시선(발사 피치·Aim Offset):** 신규 RPC·복제 프로퍼티 **0개**로 구현 — 이미 서버에 도착하고 있던 Mover InputCmd의 `ControlRotation`을 소비부만 연결했다 (§4.3). `GetBaseAimRotation()` 오버라이드 단일 진입점으로 서버 발사 방향과 관전자 화면 상체 자세(Aim Offset)가 함께 동기화된다.
- **무기 장착:** 클라이언트는 비주얼 즉시 로컬 적용 + `Server_EquipWeapon()` RPC, 서버가 GAS 어빌리티를 부여하고 `EquippedWeaponData` 복제 → `OnRep`에서 타 클라이언트 비주얼 갱신. GAS 권한은 항상 서버에만 있다.

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
| 서버 → 관전자 | `UMoverComponent::bSyncInputsForSimProxy = true` (엔진 옵션) — InputCmd를 SyncState에 동봉해 시뮬레이티드 프록시가 `GetLastInputCmd()`로 조회 가능 |
| 소비 단일 진입점 | `ALNPPlayerCharacter::GetBaseAimRotation()` 오버라이드 — 로컬 제어면 Control Rotation, 아니면 복제된 InputCmd의 `ControlRotation` 반환 |

AnimInstance는 원래부터 `GetBaseAimRotation()`으로 AimPitch/AimYaw를 산출하므로(구면 세계 보정 포함) **무수정**으로 상체 자세가 동기화되고, 서버의 원격 플레이어 발사 방향도 같은 함수로 해결된다. `bSyncInputsForSimProxy`는 플레이어 폰에만 적용해 대역폭 증가를 최소화했다.

---

## 5. 검증

- **PIE 2·3인 멀티플레이:** 근접·원거리 양방향 전투(호스트↔클라, 클라↔클라), PvP·NPC 가드/패링/반사, 무기 교체, 루팅, HitReact/HitStop 전파, 관전 화면 발사체 표시·피격 소멸 전수 확인.
- **지연·손실 시뮬레이션:** `net.PktLag=150/250`, `net.PktLoss=5` — Lag Compensation 체감 위화감 없음, 고지연 패링 성공, 구면 이동 Up 벡터 정상.
- **정량 실측:** 클라이언트 퍼펫 Transform 오차 `maxPlayerEntityActorGap=0cm`(프레임 단위 동기화), Dead Reckoning ≈184ms 외삽 스폰 정상 동작 로그 확인.
- **Standalone `-game` 모드 검증 병행:** PIE가 놓치는 초기화 순서 크래시 커버.

## 6. 향후 과제

- 수백 엔티티 규모 대역폭·CPU 프로파일링 (MassReplication 레거시 호환 경로의 스케일 한계 확인)
- 관전 Ghost catch-up 수렴 (외삽 스폰의 총구 이펙트 단절이 체감되면 도입)
- NPC 원거리 공격의 방송 채널 분리 (현재는 공격자 Actor RPC — Low LOD NPC는 Actor가 없음. SalvoID 체계는 선반영 완료)
