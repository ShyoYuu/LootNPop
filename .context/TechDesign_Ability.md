# 어빌리티 시스템 기술 설계

## 1. 한눈에 보기

GAS 기반 전투 시스템. **ASC는 PlayerState에 귀속**되고(폰 교체에도 스킬·버프 유지 — Lyra 패턴), 발사체는 Actor가 아닌 **Mass Entity**로 스폰되어 프로세서 파이프라인이 이동·판정을 맡는다.

```
[ALNPPlayerState]
  ├── UAbilitySystemComponent     ← 어빌리티 실행·GE 적용의 주체
  ├── ULNPBaseAttributeSet        ← 스텟 7종 + 탄창 상태 + IncomingDamage(Meta)
  ├── ULNPEquipmentComponent      ← 무기 슬롯 + Active/Passive 스킬 슬롯
  └── ULNPInventoryComponent      ← 가방·활성 버프 아이템 인스턴스

[ALNPCharacterBase (Pawn)]
  └── IAbilitySystemInterface     ← PlayerState의 ASC를 위임 반환
      InitAbilityActorInfo: 서버 PossessedBy / 클라 OnRep_PlayerState
```

### Ability 클래스 계층

```
UGameplayAbility
  └── ULNPGameplayAbility              ← 공통 기반 (GetOwningCharacter / GetOwningLNPPlayerState / GetAttackSpeed)
        ├── ULNPAbility_BasicAttack    ← 무기 기본 공격 추상 기반
        │     │   ComputeDamage / ApplyCooldown·CheckCooldown(무기별) / CheckCost·ApplyCost(탄창)
        │     │   KnockbackStrength·PoiseDamage·ParryRadius 프로퍼티
        │     ├── ULNPAbility_RangedAttack        ← Mass 발사체 스폰 후 즉시 종료
        │     │     └── ULNPAbility_RangedSpreadAttack   ← 육각 링 산탄 (기본 19발)
        │     └── ULNPAbility_MeleeAttack         ← Chooser 몽타주 + 콤보 섹션 + 타겟 보정, 몽타주 종료까지 유지
        ├── ULNPAbility_Reload         ← 탄창 재장전 (폰 DefaultAbilities, §5.5)
        ├── ULNPAbility_ParrySuccess   ← GameplayEvent.Parry.Success 트리거
        └── ULNPAbility_Stagger        ← Stagger.Light / Stagger.Heavy 트리거, 경직 구간을 어빌리티 수명으로 소유
```

발동 입력 스냅샷(`FLNPFireAimTargetData`·`FLNPMeleeAssistTargetData`)은 `GAS/Abilities/LNPAttackInputTargetData.h` — §4.

---

## 2. 클래스 구조

### 2.1 AttributeSet — `ULNPBaseAttributeSet`

| Attribute | 기본값 | 비고 |
|:---|:---:|:---|
| Health / MaxHealth | 100 / 100 | MaxHealth ≥ 1. `PostAttributeChange`가 MaxHealth 감소 시 Health를 잘라준다 |
| AttackPower | 10 | 피해 공식의 기저 |
| AttackSpeed / MoveSpeed / LootSpeed | 1 / 1 / 1 | 배율 — ≥ 0.01 클램프 |
| DefensePower | **10** | `LNPDamage::ApplyDefense`. ≥ 0 클램프 |
| PoiseResistance | **150** | 경직력 감쇠(§2.5). ≥ 0 클램프. 적은 `ULNPEnemyConfig::PoiseResistance`(기본 20)가 정한다 |
| MagazineAmmo / MagazineSize | 0 / 0 | **스탯이 아니라 상태** — 장착 무기 탄창 잔량·크기(§5.5). 메타 테이블·스탯 탭·버프 비대상. `MagazineAmmo`는 `[0, MagazineSize]` 클램프 |
| IncomingDamage | 0 | **Meta** — 복제 안 함. `PostGameplayEffectExecute`에서 방어력 적용 후 Health에 반영하고 즉시 0 |

감쇠식은 `GAS/LNPDamageFormula.h`에 둘: `LNPDamage::ApplyDefense`, `LNPPoise::ApplyResistance` — 둘 다 `× 100 / (100 + X)`.

#### 기초값 초기화 — `DT_PlayerBaseStats`

생성자 기본값을 **플레이어는 데이터 테이블로 덮어쓴다.** `FGameplayAttributeData::BaseValue`가 `BlueprintReadOnly`라 `EditAnywhere`를 달아도 디테일 패널에서 편집할 수 없기 때문이다.

| 요소 | 내용 |
|:---|:---|
| `Content/Gameplay/DT_PlayerBaseStats` | Row Type은 엔진 `AttributeMetaData`. 어트리뷰트당 1행, 행 이름 `LNPBaseAttributeSet.<어트리뷰트명>` |
| `ALNPPlayerState::DefaultAttributeTable` | `EditDefaultsOnly`. `BP_LNPPlayerState`가 지정. 프리셋은 테이블을 여러 개 만들어 파생 BP마다 지정 |
| `ALNPPlayerState::PostInitializeComponents` | 서버에서 `ASC->InitStats()` 후 Health를 MaxHealth로 맞춘다 |

- ⚠️ **ASC의 `DefaultStartingData`("Attribute Test")를 쓰면 안 된다.** `OnRegister`에서 처리되는데, 어트리뷰트셋 등록(`SpawnedAttributes`)은 `InitializeComponent`라 목록이 비어 있어 **어트리뷰트셋이 하나 더 생긴다.** `InitStats`도 같은 이유로 `PostInitializeComponents`가 가장 이른 안전 시점이다.
- ⚠️ 엔진이 읽는 열은 `BaseValue`뿐이다. `MinValue`/`MaxValue`는 무시되므로 값 제한은 `PreAttributeChange`가 한다.
- ⚠️ 행 이름이 문자열 매칭이라 어트리뷰트를 리네임하면 그 행이 **조용히 무시**된다.
- ⚠️ `Health` 행은 사실상 무시된다(MaxHealth로 덮어씀). 덜 찬 체력으로 시작하는 기획이 생기면 조건부로 바꿀 것.

PIE 1인 검증(2026-08-30): 테이블을 `MaxHealth 137`/`AttackPower 23`으로 바꿔 base 반영, 무기 +10 적용(current 33), Health 137, 어트리뷰트셋 인스턴스 1개 확인. **잔여:** 2인 리슨 서버 게스트 초기 복제.

#### 스탯 파이프라인 규약 — 채널 2개만 쓴다

스텟 하나당 어트리뷰트 하나다. **배율용 보조 어트리뷰트를 두지 않는다**(구 `AttackMultiplier` 제거).

```
최종 = (기초 + 무기 스텟 + 합연산 버프) × (1 + Σ 곱연산 버프)
```

GAS 어그리게이터 식 `((Base + AddBase) × MultiplyAdditive ÷ DivideAdditive × MultiplyCompound) + AddFinal`에서 `MultiplyAdditive`는 Bias 1.0으로 합산(`FAggregatorModChannel::SumMods`)된다 → **배율끼리 더해진 뒤 한 번만 곱해진다.** 중복 획득 시 체감 효율 저하가 공짜로 따라온다.

| 개념 | 채널 |
|:---|:---|
| 합연산 버프 · 무기 스텟 | `EGameplayModOp::AddBase` |
| 곱연산 버프 | `EGameplayModOp::MultiplyAdditive` |

- ⚠️ `DivideAdditive` / `MultiplyCompound` / `AddFinal` / `Override`는 **사용 금지** — 스탯 UI의 `C = A × B` 분해(→ [TechDesign_InGameMenu.md §4](TechDesign_InGameMenu.md))가 깨진다.
- ⚠️ 곱연산 버프는 기초값 0인 스텟에서 무효다. 새 스텟의 기초값은 반드시 양수로.

#### 선언형 스탯 모디파이어 — `GAS/LNPStatModifier.h`

아이템 DataAsset이 `TArray<FLNPStatModifier>`(`{어트리뷰트, Flat/Percent, 크기}`)를 선언하면 `LNPStat::ApplyModifiers`가 공용 GE 2종에 SetByCaller로 주입해 적용한다. **스텟×연산 조합마다 GE 에셋을 만들지 않는다.**

| 요소 | 내용 |
|:---|:---|
| `ULNPGameplayEffect_StatFlat` / `_StatPercent` | Infinite GE. 메타 테이블 전 스텟에 `AddBase` / `MultiplyAdditive` 모디파이어. no-op 값 0 / 1.0. 해당 Op 항목이 없으면 GE를 만들지 않는다 |
| `LNPStat::GetStatMetaTable()` | 스텟 목록의 **단일 출처**(7종: MaxHealth·AttackPower·AttackSpeed·DefensePower·MoveSpeed·LootSpeed·PoiseResistance) — 어트리뷰트·SetByCaller 태그(`LNP.GE.Data.Stat.*`)·표시명·표기 방식 |
| `LNPStat::MakeModifierText()` | 아이템 설명문 자동 생성 ("Base Attack +10" / "Attack +40%") |
| `LNPStat::ResolveStatValue()` | ASC 없이 최종값 계산. 용처는 적 Mass 엔티티 스폰 시 MaxHealth 하나 |

⚠️ SetByCaller 태그를 지정하지 않으면 GAS는 에러 로그 후 **0**을 반환한다. Percent GE에서 0은 스텟을 0으로 만들므로, `ApplyModifiers`는 **모든 스텟 태그를 no-op 값으로 먼저 채운 뒤** 필요한 항목만 덮어쓴다.

GE 수명은 적용한 쪽이 핸들로 관리한다:

| 적용 위치 | 해제 |
|:---|:---|
| `ULNPEquipmentComponent::GrantItemImpl` (무기·스킬) | `RevokeItemImpl` |
| `ULNPInventoryComponent::AddBuffItem` (버프) | `ExpireBuffInstance` / `RemoveBuffInstance` |
| `ALNPEnemyCharacter::InitializeFromConfig` (적 무기) | 재초기화 시 `WeaponStatEffects` 해제 |

> ⚠️ 적은 `EquipmentComponent`를 거치지 않고 `WeaponData`를 직접 읽는다. 여기서 무기 스텟을 적용하지 않으면 적 피해량이 무기분만큼 사라지고, 해제하지 않으면 LOD 전환마다 스텟이 누적된다.

PIE 1인 검증(2026-08-16, `AbilitySystemInspectorToolset`): LootSpeed base 1.0 → 합연산 +1.0 = 2.0 → 곱 +50% = 3.0 → 곱 +50% 추가 = **4.0**(곱복리라면 4.5) → 30초 후 기간제 곱연산 2개만 만료되어 2.0. 무기 장착만으로 `AttackPower` 10 → 20. **잔여:** 2인 PIE 복제, `DefensePower` 기초 10 도입에 따른 적 피해 밸런스.

### 2.2 피해 파이프라인

```
판정 (Mass Processor)
  → FLNPApplyDamageGECommand (Game Thread, LNPHitDetectionShared.h)
      → 무기 ProjectileDamageEffect (Instant GE, 근접·원거리 공용 필드) — TAG_GE_Data_Damage SetByCaller(양수)
          → IncomingDamage (Meta) → PostGameplayEffectExecute → 방어력 적용 → Health 차감
```

네이티브 구현은 `ULNPGameplayEffect_Damage`(`IncomingDamage`에 `Additive`).

기본 피해량(`ComputeDamage`) = **`AttackPower` 최종값 × 피해 계수.** 무기 스텟·곱연산 버프는 이미 어그리게이터에 반영된 뒤다(§2.1).

피해 계수(`GetDamageCoefficient`) = 두 축의 곱:

| 축 | 위치 | 뜻 |
|:--|:--|:--|
| `BaseDamageCoefficient` | 어빌리티 CDO (`EditDefaultsOnly`) | 같은 무기의 강공격·특수공격에 주는 개성 |
| `AbilityCoefScale` | 무기 레벨 테이블 행 | 무기 레벨에 따른 성장 |

**무기 레벨은 GAS 어빌리티 스펙 레벨로 흐른다** — `GrantItemImpl`이 `FGameplayAbilitySpec(Class, 아이템레벨)`로 부여하므로 `GetAbilityLevel()`이 곧 무기 레벨이다. 합성으로 레벨이 바뀌면 `ULNPEquipmentComponent::RefreshWeaponSlotGrants()`가 회수 후 재부여한다.

쿨다운은 단일 `ULNPGameplayEffect_Cooldown`에 **무기별 `FireCooldown / AttackSpeed`를 per-spec Duration으로 주입**하고, **무기별로 각자 돈다**(§5.2).

### 2.3 아이템 정의 DataAsset

```
UPrimaryDataAsset
  └── ULNPItemDefinitionBase (Abstract)
        ├── DisplayName / AbilitiesToGrant / EffectsToApply / StatModifiers
        ├── ULNPWeaponData   ← 무기 (아래 표)
        ├── ULNPSkillData    ← 스킬 (Active/Passive 구분은 슬롯 위치)
        └── ULNPBuffData     ← 버프 (Duration: 양수 = 기간제, -1 = 영구)

UDataAsset
  └── ULNPWeaponVisualSet   ← 무기 표현 세트 (메시·소켓·그립 보정·AnimLayerClass·AnimSetTag·재장전/발사 무기 애니)
                              여러 ULNPWeaponData가 공유한다 (DA_Launcher → VS_Shotgun)
```

`StatModifiers`가 베이스에 있어 무기·스킬·버프가 같은 형식으로 스텟을 선언한다.

**단, 무기의 스텟 원본은 `LevelTable`의 레벨 행이다.** 읽기는 항상 `ULNPWeaponData::GetStatModifiersForLevel(Level)`을 거치고, 테이블이 없으면 베이스 `StatModifiers`로 폴백한다(레벨 1 고정, 합성 불가). 둘 다 채우면 최초 사용 시 경고 로그 — 조용한 무시는 함정이 되기 때문이다. 적 NPC는 같은 함수를 레벨 1로 호출한다.

**`ULNPWeaponData` 주요 필드:**

| 분류 | 필드 |
|:---|:---|
| 표현 | `VisualSet` |
| 조준 | `DefaultAimMode` (표현이 아니라 조작 규칙이라 VisualSet이 아닌 여기) |
| 공격 | `FireCooldown`, `MaxComboCount`, `MagazineSize`(0 = 탄약 없음), `ReloadTime`, `MeleeIdealDistance`(근접 보정, 0 = 위치 보정 없음) |
| 레벨 | `LevelTable` (행 구조 `FLNPWeaponLevelRow`, **행 이름 = 레벨 숫자**) |
| 발사체 | `ProjectileType`(Linear/Guided/Lobbed), `ProjectileSpeed`, `ProjectileGravity`, `HitRadius`, `ExplosionRadius`, `ProjectileLifetime`, `MuzzleOffset`, `ProjectileDamageEffect`(근접도 사용), `ProjectileVFXData` |

- `ProjectileGravity`는 **`Lobbed`일 때만** 적용된다. 해석 창구는 `GetEffectiveProjectileGravity()` 하나(→ [TechDesign_HitDetection.md §3.4](TechDesign_HitDetection.md)).
- 공격 몽타주는 WeaponData가 아닌 **Chooser Table**에서 고른다(`VisualSet->AnimSetTag`가 입력 조건, → [TechDesign_CombatAnimation.md §6.1](TechDesign_CombatAnimation.md)).
- 런처는 `VS_Shotgun`을 가리켜 메시·애님 레이어·몽타주를 샷건과 함께 쓰고, 발사 간격·발사체·스탯은 자기 것을 갖는다. ⚠️ **표현 세트도 그 안의 태그도 규칙의 키가 될 수 없다** — 공유하는 무기들이 함께 묶인다. 무기의 정체성은 `ULNPWeaponData` 에셋 그 자체다(§5.2).
- `ParryRadius`·`KnockbackStrength`·`PoiseDamage`(근접은 콤보별 배열)는 어빌리티 프로퍼티다 — 같은 무기라도 어빌리티마다 튜닝할 수 있다.

### 2.4 장비·인벤토리 컴포넌트

인스턴스 모델(가방·버프 = `ULNPInventoryItemInstance` + FastArray, 장착 슬롯 = `FLNPWeaponInstance`/`FLNPSkillInstance`)과 복제 구조는 → [TechDesign_Inventory.md](TechDesign_Inventory.md). 어빌리티 쪽에서 알아야 할 것만:

- **`ULNPEquipmentComponent`** (PlayerState, 쓰기는 서버 전용): 무기 슬롯 1 + Active Skill 슬롯 N(`ULNPSettings::MaxActiveSkillSlots`, 기본 4) + Passive 목록. 장착/해제는 `GrantItemImpl`/`RevokeItemImpl` 공통 로직으로 GA Grant/Clear·GE Apply/Remove. 서버 슬롯의 `GrantedAbilities[0]`이 기본 공격 GA 핸들이다.
- 기본 무기(`DefaultWeapon`)는 폰 `PossessedBy`의 `EnsureDefaultWeapon()`이 가방에 넣고 자동 장착한다 — 기본 무기도 WeaponData라 특수 처리가 없다.
- **`ULNPInventoryComponent`** (PlayerState): `AddBuffItem(Def, RemainingDuration)` → Infinite GE 적용 + 만료 타이머. `RemoveBuffInstance()` → GE 제거 + **남은 시간 반환**(드랍된 LootDice에 이어붙이는 용도).

### 2.5 경직(Poise) 파이프라인

피해와 나란히 흐르는 **두 번째 판정 축**이다. 경직도를 줄이는 수단은 매 틱 자연회복뿐이라, 그 속도를 넘는 화력을 몰아쳐야 상대가 굳는다.

```
공격 어빌리티 PoiseDamage
  → FLNPWeaponTraceFragment / FLNPProjectileSharedFragment ::PoiseDamage   (KnockbackStrength와 같은 자리)
      → 판정 Processor 서버 구역: LNPPoise::Accumulate(피격자 FLNPPoiseFragment, ...)
      → ULNPPoiseProcessor → FLNPStaggerCommand → GA_Stagger + GameplayCue
```

| 축 | 위치 | 비고 |
|:--|:--|:--|
| 경직력 | `ULNPAbility_BasicAttack::PoiseDamage`, 근접은 `ComboPoiseDamages` | **무기 레벨 스케일 없음** — 제곱으로 커지면 고레벨 무기 하나로 영구 경직락이 된다. ⚠️ 산탄은 발마다 누적되므로 발당 값 |
| 경직저항력 | `PoiseResistance` 어트리뷰트 → `FLNPPoiseFragment::Resistance` 미러 | 정식 스텟이라 합/곱 파이프라인·스탯 탭에 자동으로 얹힌다. 판정 Pass가 워커 스레드라 ASC를 볼 수 없어 미러가 필요하다 |
| 행동 차단 | `ULNPAbility_Stagger` (§3.3) | 어빌리티 수명이 곧 차단 구간 |

**누적·자연회복·임계 판정·네트워크 정책은 → [TechDesign_Poise.md](TechDesign_Poise.md)** (기획 의도는 [GameDesign_Poise.md](GameDesign_Poise.md))

---

## 3. 어빌리티 상세

### 3.1 근거리 — `ULNPAbility_MeleeAttack`

```
ActivateAbility
├─ CommitAbility (쿨다운)
├─ EvaluateMontage(TAG_Montage_Situation_Attack)  ← Chooser에서 무기별 몽타주 선택
├─ 콤보 인덱스 → 섹션명 (0이면 첫 섹션, 이후 "Section_{N+1}")
├─ ApplyMeleeAssist (발동 요청 스냅샷 기반 타겟 보정, → CombatAnimation §6.6)
└─ PlayMontageAndWait (재생 속도 = AttackSpeed)
     ├─ OnCompleted/OnBlendOut → ClearRelativeTag + EndAbility
     └─ OnInterrupted/OnCancelled → ClearRelativeTag + EndAbility(cancelled)
EndAbility → ClearMeleeAssist (워프 타겟·회전 보정 해제, 조기 종료 경로까지 한 곳에서)
```

- 어빌리티는 **몽타주가 끝날 때까지 살아있다** — 콤보 전환 시 `CancelCurrentAttackAbility()`로 취소 후 재발동(§4).
- `ClearRelativeTag`: 인터럽트 시 ANS가 남긴 `Block.AttackInput`/`State.ComboWindow`를 정리.
- 피격 판정은 몽타주의 `ANS_LNPMeleeHitWindow`. 어빌리티는 `GetAbilityDamage()`/`GetKnockbackForCombo()`/`GetPoiseDamageForCombo()`/`GetParryRadius()`로 파라미터를 공급한다.

### 3.2 원거리 — `ULNPAbility_RangedAttack`

```
ActivateAbility → Commit(쿨다운·탄약) → SpawnProjectile() → PlayMontage(Attack) + PlayWeaponFireAnimation → 즉시 EndAbility
```

`SpawnProjectile()` 핵심 단계:
1. `FLNPProjectileSharedFragment` 구성(VFX·GE·피해·반경·넉백·스플래시 넉백·경직력·중력) → `GetOrCreateConstSharedFragment`
2. 스폰 위치: `LNPFireGeometry::ResolveMuzzleLocation`(`Muzzle` 소켓 + `MuzzleOffset`)
3. 발사 방향(`GetFireDirections`, 가상) → `LNPFireGeometry::ResolveAimDirection`:
   - 플레이어: 발동 요청에 실린 조준점·시선(`FLNPFireAimTargetData`)으로 총구에서 수렴 — 예측 클라와 서버가 같은 값을 쓴다(→ [TechDesign_HitDetection.md](TechDesign_HitDetection.md) §7.7)
   - 스냅샷이 없는 사수(적 NPC): `GetBaseAimRotation()`
4. 네트워크: 예측 키/SalvoID, Ghost 등록·거부 델리게이트, 관전자 Multicast 방송(→ [TechDesign_Networking.md](TechDesign_Networking.md))
5. 방향마다 `FMassCommandBuildEntityWithSharedFragments`로 엔티티 빌드(Deferred)

**산탄 (`ULNPAbility_RangedSpreadAttack`):** `GetFireDirections` 오버라이드. 배치 공식은 순수 엔티티 적 공격과 공유하는 `LNPSpread::BuildHexRingDirections`(`HitDetection/LNPSpreadPattern.h`) — Cube 좌표 육각 링, 발수 `1 + 3N(N+1)`.

확산 형태는 무기가 아니라 **어빌리티가 소유한다** — 같은 무기의 강공격·특수공격이 산탄 폭을 달리할 수 있어야 한다.

| 프로퍼티 | 기본값 | 의미 |
|:---|:---|:---|
| `HexRingCount` | 2 | 링 수(0~5). 0=1발, 1=7발, 2=19발, 3=37발 |
| `HexStepDegrees` | 7.5 | 인접 셀 간 각도. 링 수와 곱한 값이 최대 반각 (2 × 7.5 = 15도) |

⚠️ **구면 중력에서 방향 오프셋을 월드 오일러 각(Yaw/Pitch) 덧셈으로 만들지 않는다.** `FRotator::Yaw`는 월드 Z축 둘레 회전이라 실제 각변위가 `cos(Pitch)`에 비례해 줄고, 월드 Pitch ±90°에서 가로 폭이 0으로 붕괴한다. 구면 중력에서는 서 있는 위치에 따라 "캡슐 기준 수평"이 월드 Pitch 0°도 ±90°도 되므로 **극에서는 정상, 적도에서는 찌그러지는** 위치 의존 결함이 된다. 규약:

1. **오프셋은 기준 방향 자신의 직교 기저에서 잡는다.** `RightAxis = Cross(RefUp, BaseDir)`, `UpAxis = Cross(BaseDir, RightAxis)`.
2. **`RefUp`은 월드 Z가 아니라 `GetUpDirection()`(중력 Up)이다.** 패턴 롤이 캐릭터 자세를 따라야 화면상 방향이 일정하다. `BaseDir ∥ RefUp` 특이점에서는 캐릭터 전방으로 롤을 정한다.
3. **오프셋은 `tan(각도)`로 준다** — `BaseDir`에 수직인 평면 격자를 투영하는 것과 같아 벽에 쏘면 정육각형이 된다. 90°를 넘으면 `tan` 부호가 뒤집히므로 반각을 ±80°로 클램프한다.

서버 판정과 고스트 방송이 `GetFireDirections`의 **같은 배열을 공유**하므로 이 함수만 고치면 양쪽이 따라간다.

⚠️ 펠릿마다 Mass 엔티티·트레일 VFX·넉백·경직력이 **각각** 발생한다. `KnockbackStrength`를 단발 무기와 같게 두면 근접 전탄 명중 시 발수만큼 배가된다(`GA_RangedAttack_Shotgun` 10, Rifle 50).

**트레일 진영 색 (2026-08-22):** 트레일 Niagara 시스템이 `TintColor`(LinearColor) User 파라미터를 노출하면 `ULNPProjectileVisualSubsystem`이 진영 색(`LNPSettings`의 `Player/EnemyProjectileTintColor`)을 주입한다. 파라미터가 없는 시스템에서는 조용히 무시된다. 현재 `NS_BulletGlow`(Shotgun·적 NPC Pistol 공용)만 대응.

- **패링 시 색 전환**이 공짜로 따라온다 — 프로세서가 `InstigatorTeam`을 반전시키면 `FLNPProjectileVisualFragment::AppliedTeam`과 어긋난 프레임에만 `SetTrailTeam()`으로 재주입한다(매 프레임 주입은 샷건 19발에서 헛비용).
- ⚠️ **`ScaleColor`(Particle Update)의 `Color Value To Scale`을 `User.TintColor`에 링크해야 한다.** `InitializeParticle.Color`(Spawn)에만 링크하면 스폰 시점 색으로 고정돼 패링해도 안 바뀐다.
- ⚠️ 트레일 파티클은 `Lifetime = 9999`(C++이 컴포넌트를 파괴)라 `NormalizedAge ≈ 0`이다. 나이 기반 커브는 무효 — `Particles.Age` 절대값으로 구동할 것.

**머티리얼 (`M_LNP_ProjectileGlow`):** Niagara 기본 `M_DepthFade_SpriteSimple`은 Additive라 원리적으로 불투명해질 수 없다. 진영 구분용으로 **Masked + Unlit** 머티리얼을 신설했다(UV 중심 거리 하드 컷 원반 + 진영 색 ×0.09 아웃라인). ⚠️ Unlit이라 `TintColor`가 곧 Emissive다. 세 채널이 모두 1을 넘으면 톤매핑에서 흰색으로 포화되므로 색조 채널 하나만 1을 살짝 넘긴다.

### 3.3 리액션 — `ULNPAbility_ParrySuccess` / `ULNPAbility_Stagger`

GameplayEvent 트리거로 자동 발동(생성자에서 `AbilityTriggers` 등록). Player·Enemy 어느 ASC에든 Grant 가능 — 플레이어는 폰 `DefaultAbilities`, 적은 `ULNPEnemyConfig::DefaultAbilities`로 부여한다.

`ULNPAbility_ParrySuccess`는 방어자 `ReactionMontage`를 재생하고 즉시 종료한다(이벤트 발송은 `LNPHitDetectionShared.h`).

**`ULNPAbility_Stagger`는 몽타주를 재생하지 않고, 경직 구간 동안 살아 있으면서 태그만 소유한다.**

| 항목 | 값 |
|:--|:--|
| 트리거 | `Stagger.Light`(그로기) / `Stagger.Heavy`(다운) — **경직도 상태 전이가 유일한 진입점**. 패링도 경직도(`LNPPoise::ApplyParryBreak`)를 거친다 |
| `ActivationOwnedTags` | `State.Staggered`, `Block.AttackInput`, `Block.MovementInput` |
| `ActivationBlockedTags` | `State.Staggered` — 그로기 중 재진입 방지. **다운은 이 태그에 자기가 막히므로** `FLNPStaggerCommand`가 그로기 GA를 먼저 취소한다 |
| `CancelAbilitiesWithTag` | `Ability.Reload` (§5.5) |
| 수명 | **그로기는 지속 시간이 없다** — 경직도가 T1 아래로 회복하면 `ULNPPoiseProcessor`가 취소한다. 다운만 `ULNPSettings::PoiseDownLockSeconds`(1.8s)로 `WaitDelay` |
| 몽타주 | **없음.** `GameplayCue.LNP.Character.Stagger` → Chooser(`Situation.Stagger` × `Value.Stagger.Light/Heavy/Parried`) |

몽타주와 차단을 갈라놓은 이유는 적 ASC의 `Minimal` 복제다 — 어빌리티 활성화가 시뮬레이티드 프록시에 도달하지 않아, GA가 몽타주를 들면 게스트 화면에서 적 경직이 안 보인다. 반대로 입력 차단은 권위 상태라 코스메틱 큐에 실을 수 없다.

**상태 전이 규칙·패링 연계·네트워크 정책은 → [TechDesign_Poise.md](TechDesign_Poise.md)**

---

## 4. 발동 흐름 (입력 → 어빌리티)

```
AttackAction 입력 (ULNPInputHandlerComponent) — 실패 시 0.05초 버퍼로 재시도
  → ALNPCharacterBase::TryActivateAttack()
      ├─ State.Staggered → false (콤보 창 분기보다 앞 — 콤보 창 태그가 1프레임 남아도 차단을 건너뛰지 않게)
      ├─ State.ComboWindow → 태그 소비 + 인덱스 증가 + Server_SetComboIndex + CancelCurrentAttackAbility + 재발동
      ├─ Block.AttackInput → false
      └─ 평상시 → ResetCombo(인덱스가 바뀔 때만 서버 동기화, 발동 RPC보다 앞) → TryActivateAttack_Impl()
          └─ ALNPPlayerCharacter::TryActivateAttack_Impl
               ├─ 탄창 무기 + MagazineAmmo < 1 → TryReload() 후 false (자동 재장전)
               ├─ 핸들: 서버는 WeaponSlot.GrantedAbilities[0], 클라는 복제 스펙을 클래스로 탐색
               └─ 입력 스냅샷 캡처 → ASC->TriggerAbilityFromGameplayEvent(Handle, GameplayEvent.Attack.Activate, Payload)
                    FreeAim: FLNPFireAimTargetData(조준점·시선) / 그 외: FLNPMeleeAssistTargetData(보정 대상·락온·이동 입력)
```

- 무기 교체 시 슬롯 핸들이 새 무기 GA로 바뀌므로 입력 재바인딩이 필요 없다.
- **공격 입력 중 소유 클라만 아는 값은 InputCmd가 아니라 발동 요청에 싣는다.** 엔진이 `ServerTryActivateAbilityWithEventData` 한 번에 보내고 서버 `ActivateAbility`의 `TriggerEventData`로 도착한다. InputCmd는 서버가 고정 틱에서 마지막으로 소비한 커맨드라 입력 버퍼 깊이만큼 과거 값이고, 조건이 맞는 동안 상시 대역폭을 쓴다(→ [TechDesign_Networking.md](TechDesign_Networking.md)).
- ⚠️ 스냅샷 구조체 필드는 **반드시 UPROPERTY, 커스텀 NetSerialize 금지.** Iris의 TargetDataHandle 직렬화기는 리플렉션으로 디스크립터를 만들고 NetSerialize를 무시한다 — UPROPERTY 없는 필드는 전부 기본값으로 도착한다(경고 한 줄뿐). 양자화는 필드 타입(`FVector_NetQuantize` 등)으로 표현한다.

---

## 5. 어필 포인트 (설계 판단)

### 5.1 "발사체 = Mass Entity" — GAS와 Mass의 역할 분담

어빌리티는 **스폰까지만** 책임지고 즉시 종료한다. 수백 발이 동시에 날아도 어빌리티 인스턴스·액터·컴포넌트 비용이 없고, 이동·판정·VFX·파괴는 4단 프로세서(`ULNPProjectileMovement/HitDetection/Visualization/DestructionProcessor`, 별도로 디버그 드로우)가 청크 단위로 처리한다. 무기 상수는 `ConstSharedFragment`로 공유되어 같은 무기의 발사체가 청크·메모리를 공유한다.

### 5.2 단일 Cooldown GE + per-spec Duration 주입 + EffectSource 키

GAS 표준 관행(무기마다 Cooldown GE 클래스)을 버리고 `SetDuration(FireCooldown / AttackSpeed)` 주입으로 단일 클래스가 모든 무기를 커버한다 — 무기 추가가 DataAsset 편집만으로 끝난다.

**GE 클래스를 하나로 합치면 차단 키도 하나가 된다.** 엔진 `UGameplayAbility::CheckCooldown()`은 GE CDO의 GrantedTags를 ASC 태그와 대조할 뿐이고, 쿨다운 GE 태그는 `LNP.Ability.Cooldown.Attack` 하나다. 그대로 두면 **런처를 쏜 뒤 라이플로 바꿔도 런처의 3초가 끝날 때까지 라이플이 안 나간다.** 무기별 GA 클래스를 둬도 소용없다 — `CheckCooldown()`은 ASC 태그를 본다.

그래서 Duration과 **차단 키를 분리했다**(2026-09-11):

| 축 | 담는 곳 | 값 |
|:--|:--|:--|
| 지속 시간 | 스펙의 Duration | `WeaponDef->FireCooldown / AttackSpeed` |
| 차단 키 | 스펙 컨텍스트의 SourceObject | 장착 **무기 정의(DataAsset)** |

`ApplyCooldown`이 `GetContext().AddSourceObject(WeaponDef)`로 무기를 스탬프하고, `CheckCooldown` 오버라이드가 `FGameplayEffectQuery::EffectSource`로 **그 무기의 쿨다운 GE만** 조회한다. 쿨다운 GE는 기본 스택 정책(`None`)이라 무기마다 별개로 공존하다 스스로 만료되므로 **교체 시 제거 로직이 필요 없다.** 무기를 못 찾으면 엔진 기본 동작으로 물러난다.

기획 결정은 **"무기별로 각자 돈다"** — 교체로 쿨다운을 지울 수 없고, 키가 **정의 단위**라 같은 무기 두 자루를 번갈아 장착해도 연사 제한을 우회할 수 없다.

- ⚠️ **표현 태그를 키로 쓰면 안 된다.** 런처는 `VS_Shotgun`을 공유해 `AnimSetTag`가 샷건과 같으므로, 그 태그를 키로 쓰면 **런처와 샷건이 쿨다운을 공유**한다. 이 함정이 표현을 `ULNPWeaponVisualSet`으로 떼어낸 계기다(§2.3).
- ⚠️ **`CheckCooldown()`은 CDO에서도 불린다.** 첫 활성화 전 `InternalTryActivateAbility`는 CDO로 `CanActivateAbility`를 부르고, CDO의 `CurrentActorInfo`는 null이다. 무기는 **인수 `ActorInfo`**에서 꺼내야 한다(`GetEquippedWeaponDefFor`). `GetEquippedWeaponDef()`를 쓰면 첫 발이 항상 기본 동작으로 떨어진다.
- ⚠️ **`GetCooldownTimeRemaining()`은 아직 무기 무관이다**("전 무기 중 최댓값"). 읽는 코드가 없어 오버라이드하지 않았다 — 공격 쿨다운 UI를 붙일 때 같은 필터로 함께 오버라이드할 것.

### 5.3 Meta Attribute 기반 피해 정산

피해를 Health에 직접 쓰지 않고 `IncomingDamage` Meta 어트리뷰트를 경유시켜 방어력·클램프를 `PostGameplayEffectExecute` 한 곳에 모았다. 피해 출처(근접/원거리/스플래시/엔티티 공격)가 늘어도 정산 로직은 그대로다.

### 5.4 크로스헤어 수렴 발사와 서버 폴백

3인칭 총기의 고전 문제(총구 방향 ≠ 화면 중앙)를 카메라 광선 수렴점으로 푼다. 카메라가 없는 서버는 발동 요청에 실린 조준점·시선을 쓰므로 자기 총구에서 같은 점으로 수렴한다. 조준점만 싣고 축을 서버의 과거 값에서 가져오면 빠르게 조준을 옮기는 순간 검증에 걸리므로 **같은 순간의 시선을 함께** 싣는다.

### 5.5 탄창·재장전 (2026-09-14)

기획은 [GameDesign_Ability.md](GameDesign_Ability.md) §3.1 "탄창·재장전".

#### 탄약 = 어트리뷰트 + Cost GE — 예측을 GAS에 맡긴다

라이플 연사(10발/s)는 소유 클라 예측이 필수다. 탄약을 별도 복제 변수로 두면 늦게 도착한 서버 값이 연사 중 로컬 차감을 덮어써 HUD가 튄다. **Cost GE는 GAS가 예측 키로 선반영하고 서버 확정 시 정산**하므로 이 경로에 태웠다.

| 요소 | 내용 |
|:--|:--|
| `MagazineAmmo` / `MagazineSize` | `ULNPBaseAttributeSet`. 전 클라 복제(HUD·무기 애니 큐가 읽는다) |
| `ULNPGameplayEffect_AmmoCost` | Instant, `MagazineAmmo` `AddBase −1`. 산탄도 발사 1회 = 1발 |
| `ULNPAbility_BasicAttack::CheckCost` | 탄창 무기면 `MagazineAmmo ≥ 1`. **현재값**을 본다 — 예측 중인 차감까지 반영된 값 |
| `ULNPAbility_BasicAttack::ApplyCost` | 탄창 무기면 Cost GE 적용 |
| `ActivationBlockedTags += State.Reloading` | 재장전 중 발사 차단. 막힌 입력은 공격 버퍼로 흐른다 |

`MagazineSize = 0`(기본값)이면 두 오버라이드가 통과한다 — 근접·적 NPC 무기는 동작 불변. 무기는 `CheckCooldown`과 같은 이유로 **인수 `ActorInfo`**에서 읽는다(§5.2).

#### 재장전 GA — `ULNPAbility_Reload`

- 폰 `DefaultAbilities`로 부여(무기 무관). `AssetTags = Ability.Reload`, `ActivationOwnedTags = State.Reloading`, `ActivationBlockedTags = State.Staggered, State.Reloading`. 가득 찼거나 탄창 없는 무기면 `CanActivateAbility` 실패.
- 시간 = `ULNPWeaponData::GetReloadDuration(AttackSpeed)` = `ReloadTime / AttackSpeed`.
- 캐릭터 몽타주: Chooser `LNP.Montage.Situation.Reload`, 배속 = `몽타주 길이 / 재장전 시간`. `CHT_Montage` 행: Pistol/Rifle/Shotgun → `AM_MM_*_Reload`(런처는 `VS_Shotgun` 공유). 행이 없으면 시간만 흐른다.
- 입력: `IA_Reload`(`R` / 게임패드 `Y`) → `ALNPPlayerCharacter::TryReload`. 자동 재장전은 §4.

| 취소 | 경로 |
|:--|:--|
| 경직 | `ULNPAbility_Stagger::CancelAbilitiesWithTag += Ability.Reload` |
| 무기 교체 | `ULNPEquipmentComponent::ClearWeaponSlot`이 `CancelAbilities(Ability.Reload)` |
| 대시 | GA가 `ULNPCharacterMoverComponent::OnDashExecuted` 구독 (서버·소유 클라 각자 시뮬레이션에서 발송) |

- ⚠️ **탄을 채우는 곳은 WaitDelay 콜백이 아니라 `EndAbility`(취소가 아닐 때)다.** 서버 인스턴스는 자기 타이머와 소유 클라의 종료 통지(`ServerEndAbility`) 중 **먼저 온 쪽**으로 끝난다. 클라 통지가 먼저 오면 서버 콜백은 불리지 않아 탄이 증발한다. 조기 종료 통지로 재장전을 건너뛰지 못하게 `경과 ≥ 재장전 시간 × MinCompletionRatio(0.8)` 하한을 둔다 — 서버 활성화와 클라 종료 통지는 같은 편도 지연을 겪으므로 정상 경과는 거의 1.0이다.
- ⚠️ **소유 클라도 로컬로 채운다.** 복제를 기다리면 RTT 동안 0발로 보여 자동 재장전이 다시 걸린다. 서버 복제가 같은 값으로 수렴시킨다.

#### 무기 메시 애니 — GameplayCue로 싣는다

`ULNPWeaponVisualSet::WeaponReloadAnim`(무기 메시 스켈레톤의 시퀀스)을 `GameplayCue.LNP.Weapon.Reload`(`ULNPGameplayCueNotify_Reload`)가 재생한다. 무기 메시에는 AnimBP가 없어 단일 노드 재생이다. 재생·정지는 `ALNPCharacterBase::PlayWeaponMeshAnimation` / `StopWeaponMeshAnimation`.

- **큐인 이유:** GA는 서버·소유 클라에서만 돈다. 큐는 소유 클라에서 예측 재생되고 다른 클라에는 ASC가 복제한다.
- ⚠️ **배속을 큐 파라미터로 싣지 않는다.** Mixed 복제 모드의 비소유 클라는 최소 복제 큐로 받아 파라미터가 오지 않는다. 대상 캐릭터의 복제된 무기 정의 + `AttackSpeed`로 같은 식을 직접 계산한다.
- `OnRemove`: 정지 + 애니 해제 — 취소 시 탄창이 빠진 포즈로 굳지 않게, 교체로 메시가 바뀌었어도 옛 시퀀스가 남지 않게.

**발사 파츠 모션 (`WeaponFireAnim`):** `PlayWeaponFireAnimation()`이 `Weap_*_Fire`를 1배속 재생한다(Lyra 원본은 캐릭터 발사 몽타주와 길이가 같다). 큐가 아니라 **이미 매 발 가는 두 경로**에 얹었다 — `ULNPAbility_RangedAttack::ActivateAbility`(서버·소유 클라)와 `Multicast_SpawnGhostProjectiles`(관전자, 대역폭 추가 0). 같은 방송이 **캐릭터 발사 몽타주**도 재생한다 — 그 전에는 몽타주가 어빌리티 안에서만 돌아 게스트 화면에서 다른 플레이어의 발사 모션이 안 보였다. `DA_NPC_Pistol`도 `VS_Pistol`을 써서 승격 Actor 적에도 걸린다.

#### 상체 슬롯 — `ABP_Lyra`에 새로 만들었다

Lyra 재장전 몽타주는 `UpperBody` / `UpperBodyAdditive` 슬롯을 쓰는데 **`ABP_Lyra`에는 그 슬롯이 없었다**(발사 몽타주는 `FullBodyAdditivePreAim`). 없는 슬롯의 몽타주는 에러 없이 **아무것도 안 보인다.** `DefaultSlot` 뒤에 끼운 구조:

```
DefaultSlot → Save cached pose 'PreUpperBody'
  Use 'PreUpperBody' ─────────────────────────→ Layered blend per bone (BasePose, spine_01 분기, 메시 공간 회전 블렌드)
  Use 'PreUpperBody' → Slot 'UpperBody' ───────→        (BlendPoses_0)                       → AdditiveHitReact → …
```

`UpperBodyAdditive` 트랙은 슬롯이 없어 재생되지 않는다(보정용 가산 레이어라 생략).

#### 잔량 보관 — 인스턴스에 "소모량"으로

- `LNP.Item.AmmoSpent` 태그 스택. **남은 수가 아니라 쓴 수다** — 태그 스택은 0이면 엔트리가 사라져 "기록 없음"과 "0발"을 못 가르지만, 소모량은 부재 = 0 = 가득이라 신품이 저절로 가득 찬다.
- 저장: `ClearWeaponSlot`(교체·해제·사망 드랍 공통). 복원: `ApplyMagazineAttributes`(크기를 먼저 쓴다 — 잔량 클램프 상한).
- 드랍 왕복: `ALNPLootDice::AmmoSpent`(**비복제** — 서버 픽업만 읽는다) → `AddItemInstance(Def, Level, AmmoSpent)`. 레벨과 같은 이유로 **자동 장착보다 먼저** 기록한다.

---

## 6. 미구현 항목

| 항목 | 세부 내용 |
|:---|:---|
| Pistol·Rifle 트레일 진영 색 | 두 무기는 Ribbon 기반 `NS_*BulletTrail`을 쓴다. `TintColor` User 파라미터를 같은 방식으로 추가해야 한다(§3.2) |
| Active Skill 발동 | `ULNPInputHandlerComponent::ActiveSkillActions`는 바인딩돼 있으나 슬롯 GA 발동 연결이 없다 |
| Passive Skill GameplayEvent | 피격 시 피격자 ASC에 이벤트 전송 → Passive 자동 발동 트리거 연결 |
| 리액션 GA 부여 정식화 | ParrySuccess·Stagger·Reload를 폰/적 설정의 `DefaultAbilities` 배열에 수동 지정한다. 누락하면 조용히 동작하지 않는다 |
| 공격 쿨다운 잔여 시간 | `GetCooldownTimeRemaining()` 무기별 오버라이드(§5.2) — UI를 붙일 때 |
