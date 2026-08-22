# 어빌리티 시스템 기술 설계

## 1. 한눈에 보기

GAS 기반 전투 시스템. **ASC는 PlayerState에 귀속**되고(폰 교체에도 스킬·버프 유지 — Lyra 패턴), 발사체는 Actor가 아닌 **Mass Entity**로 스폰되어 프로세서 파이프라인이 이동·판정을 전담한다.

```
[ALNPPlayerState]
  ├── UAbilitySystemComponent     ← 어빌리티 실행·GE 적용의 주체
  ├── ULNPBaseAttributeSet        ← HP, 공격력, 공격 속도, 방어력, 이동속도
  ├── ULNPEquipmentComponent      ← 무기 슬롯 + Active/Passive 스킬 슬롯 관리
  └── ULNPInventoryComponent     ← 아이템 보관함, 버프 아이템 목록

[ALNPCharacterBase (Pawn)]
  └── IAbilitySystemInterface     ← PlayerState의 ASC를 위임 반환
      InitAbilityActorInfo: 서버 PossessedBy / 클라 OnRep_PlayerState
```

### Ability 클래스 계층

```
UGameplayAbility
  └── ULNPGameplayAbility              ← 공통 기반 (GetOwningCharacter / GetOwningLNPPlayerState)
        ├── ULNPAbility_BasicAttack    ← 무기 기본 공격 추상 기반
        │     │   GetEquippedWeaponDef / ComputeDamage / ApplyCooldown(무기별 duration 주입)
        │     │   KnockbackStrength·ParryRadius 프로퍼티
        │     ├── ULNPAbility_RangedAttack        ← Mass 발사체 스폰 후 즉시 종료
        │     │     └── ULNPAbility_RangedSpreadAttack   ← 육각 링 산탄 (중앙 1 + 링 2 = 19발)
        │     └── ULNPAbility_MeleeAttack         ← Chooser 몽타주 + 콤보 섹션, 몽타주 종료까지 유지
        ├── ULNPAbility_ParrySuccess   ← TAG_GameplayEvent_Parry_Success 트리거
        └── ULNPAbility_Stagger        ← TAG_GameplayEvent_Parry_Stagger 트리거
```

---

## 2. 클래스 구조

### 2.1 AttributeSet — `ULNPBaseAttributeSet`

| Attribute | 기본값 | 비고 |
|:---|:---:|:---|
| Health / MaxHealth | 100 / 100 | Health는 [0, MaxHealth] 클램프. `PostAttributeChange`가 MaxHealth 감소 시 Health를 잘라준다 |
| AttackPower | 10 | 피해 공식의 기저 |
| AttackSpeed / MoveSpeed / LootSpeed | 1 / 1 / 1 | 배율 — ≥ 0.01 클램프 |
| DefensePower | **10** | `LNPDamage::ApplyDefense` 공식에 사용. ≥ 0 클램프 |
| IncomingDamage | 0 | **Meta 어트리뷰트** — 복제 안 함. GE가 전달한 원시 피해량을 `PostGameplayEffectExecute`에서 방어력 적용 후 Health에 반영하고 즉시 0으로 초기화 |

방어력 공식 (`LNPDamageFormula.h`): `FinalDamage = RawDamage * (100 / (100 + Defense))`

#### 스탯 파이프라인 규약 — 채널 2개만 쓴다

스텟 하나당 어트리뷰트 하나다. **배율용 보조 어트리뷰트를 두지 않는다**(구 `AttackMultiplier` 제거).

```
최종 = (기초 + 무기 스텟 + 합연산 버프) × (1 + Σ 곱연산 버프)
```

이 공식은 GAS 어그리게이터에 그대로 들어 있다 —
`((Base + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal`,
그리고 `FAggregatorModChannel::SumMods`(`GameplayEffectAggregator.cpp:216`)가
`Sum = Bias + Σ(Mag − Bias)`이며 `MultiplyAdditive`의 Bias가 1.0이다.
→ **배율끼리 합해진 뒤 한 번만 곱해진다.** 중복 획득 시 체감 효율 저하가 공짜로 따라온다.

| 개념 | 채널 |
|:---|:---|
| 합연산 버프 · 무기 스텟 | `EGameplayModOp::AddBase` |
| 곱연산 버프 | `EGameplayModOp::MultiplyAdditive` |

⚠️ `DivideAdditive` / `MultiplyCompound` / `AddFinal` / `Override`는 **사용 금지** —
쓰는 순간 스탯 UI의 `C = A × B` 분해(→ [TechDesign_InGameMenu.md §4](TechDesign_InGameMenu.md))가 깨진다.
⚠️ 곱연산 버프는 기초값이 0인 스텟에서 무효다(0 × 1.4 = 0). 새 스텟의 기초값은 반드시 양수로.

#### 선언형 스탯 모디파이어 — `GAS/LNPStatModifier.h`

아이템 DataAsset이 `TArray<FLNPStatModifier>`(`{어트리뷰트, Flat/Percent, 크기}`)를 선언하면
`LNPStat::ApplyModifiers`가 공용 GE 2종에 SetByCaller로 값을 주입해 적용한다.
**스텟×연산 조합마다 GE 에셋을 만들지 않는다.**

| 요소 | 내용 |
|:---|:---|
| `ULNPGameplayEffect_StatFlat` | Infinite GE. 전체 스텟에 `AddBase` 모디파이어, no-op 값 **0** |
| `ULNPGameplayEffect_StatPercent` | Infinite GE. 전체 스텟에 `MultiplyAdditive` 모디파이어, no-op 값 **1.0** |
| `LNPStat::GetStatMetaTable()` | 스텟 목록의 **단일 출처** — 어트리뷰트·SetByCaller 태그(`LNP.GE.Data.Stat.*`)·표시명·표기 방식 |
| `LNPStat::MakeModifierText()` | 아이템 설명문 자동 생성 ("Base Attack +10" / "Attack +40%") |

⚠️ SetByCaller 태그를 지정하지 않으면 GAS는 에러 로그 후 **0**을 반환한다. Percent GE에서 0은 스텟을
0으로 만들어버리므로, `ApplyModifiers`가 **항상 모든 스텟 태그를 no-op 값으로 먼저 채운 뒤** 필요한 항목만 덮어쓴다.

GE 수명은 GE 자체가 아니라 적용한 쪽이 핸들로 관리한다:

| 적용 위치 | 해제 |
|:---|:---|
| `ULNPEquipmentComponent::GrantItemImpl` (무기·스킬) | `RevokeItemImpl` |
| `ULNPInventoryComponent::AddBuffItem` (버프) | `ExpireBuffInstance` |
| `ALNPEnemyCharacter::InitializeFromConfig` (적 무기) | 재초기화 시 `WeaponStatEffects` 해제 |

> ⚠️ 적은 `EquipmentComponent`를 거치지 않고 `WeaponData`를 직접 읽는다. 무기 스텟을 여기에 적용하지
> 않으면 적 피해량이 무기분만큼 사라지고, 해제하지 않으면 LOD 전환마다 스텟이 누적된다.

#### PIE 1인 검증 완료 (2026-08-16)

`GASToolsets.AbilitySystemInspectorToolset.GetAttributeValues/GetActiveEffects`로 실측:

| 상태 | LootSpeed (base 1.0) | 판정 |
|:---|:---|:---|
| 기본 | 1.0 | — |
| 합연산 +1.0 | **2.0** | AddBase ✅ |
| + 곱연산 +50% | **3.0** | MultiplyAdditive ✅ |
| + 곱연산 +50% 하나 더 | **4.0** | 배율 합산 ✅ (곱복리라면 4.5) |
| 30초 후 | **2.0** | 곱연산 GE 2개만 만료, 영구(-1) 합연산은 잔존 ✅ |

- 무기 장착만으로 `AttackPower` base 10 → current 20 (`DA_Pistol`의 `{AttackPower, Flat, 10}`) ✅
- 스탯 탭 실측: `Attack 28.0 (20.0 × 140%)`, `Max HP 150 (100 × 150%)`, `Loot Speed 200% (200% × 100%)` ✅
- GE CDO 생성 시점에 네이티브 태그·어트리뷰트가 모두 해석됨 (SetByCaller 누락 경고 없음) ✅

**잔여:** 2인 PIE(클라이언트 복제), `DefensePower` 기초 10 도입에 따른 적 피해 밸런스 회귀.

### 2.2 피해 파이프라인

```
판정 (Mass Processor)
  → FLNPApplyDamageGECommand (Game Thread)
      → ULNPGameplayEffect_Damage (Instant GE) — TAG_GE_Data_Damage SetByCaller로 피해량 전달
          → IncomingDamage (Meta) → PostGameplayEffectExecute → 방어력 적용 → Health 차감
```

기본 피해량 계산 (`ComputeDamage`): **`AttackPower` 최종값 × 어빌리티 피해 계수.**
무기 스텟은 장착 GE로 `AddBase`에 합산되고 곱연산 버프도 어그리게이터가 이미 곱한 뒤다 (→ §2.1).

피해 계수(`ULNPAbility_BasicAttack::GetDamageCoefficient`, 2026-08-20) = 두 축의 곱이다:

| 축 | 위치 | 뜻 |
|:--|:--|:--|
| `BaseDamageCoefficient` | 어빌리티 CDO (`EditDefaultsOnly`) | 같은 무기의 강공격·특수공격에 주는 개성 |
| `AbilityCoefScale` | 무기 레벨 테이블 행 | 무기 레벨에 따른 성장 |

**무기 레벨은 GAS 어빌리티 스펙 레벨로 흐른다** — `GrantItemImpl`이 `FGameplayAbilitySpec(Class, 아이템레벨)`로
부여하므로 어빌리티가 `GetAbilityLevel()`로 자기 무기의 레벨을 읽어 레벨 행을 찾는다. 별도 배관이 없다.
레벨이 바뀌면(합성) `ULNPEquipmentComponent::RefreshWeaponSlotGrants()`가 회수 후 새 레벨로 재부여한다.

쿨다운은 단일 `ULNPGameplayEffect_Cooldown` 클래스에 **어빌리티가 무기별 `FireCooldown`을 per-spec Duration으로 주입** — 무기마다 GE 클래스를 만들지 않는다.

### 2.3 아이템 정의 DataAsset

```
UPrimaryDataAsset
  └── ULNPItemDefinitionBase (Abstract)
        ├── DisplayName / AbilitiesToGrant / EffectsToApply / StatModifiers
        ├── ULNPWeaponData   ← 무기 (아래 표)
        ├── ULNPSkillData    ← 스킬 (Active/Passive 구분은 슬롯 위치)
        └── ULNPBuffData     ← 버프 (Duration: 양수 = 기간제, -1 = 영구)
```

`StatModifiers`가 베이스에 있으므로 무기·스킬·버프가 같은 형식으로 스텟을 선언한다.

**단, 무기의 스텟 원본은 `LevelTable`의 레벨 행이다** (2026-08-20). 레벨마다 값이 달라야 하므로
정의 하나에 붙은 고정 배열로는 담을 수 없다. 읽기는 항상 `ULNPWeaponData::GetStatModifiersForLevel(Level)`을
거치며, 테이블이 없는 무기는 베이스 `StatModifiers`로 폴백한다(레벨 1 고정, 합성 불가).
둘 다 채워 두면 최초 사용 시 경고 로그가 뜬다 — 조용한 무시는 함정이 되기 때문이다.
적 NPC는 `EquipmentComponent`를 거치지 않으므로 `ALNPEnemyCharacter::InitializeFromConfig`가
같은 함수를 레벨 1로 호출한다.

**`ULNPWeaponData` 주요 필드:**

| 분류 | 필드 |
|:---|:---|
| 태그·애니메이션 | `WeaponTag`, `DefaultAimMode`, `AnimLayerClass` |
| 메시 | `WeaponMesh`, `AttachSocketName`, `WeaponMeshRelativeLocation/Rotation` (그립 피벗 보정) |
| 공격 | `FireCooldown`, `MaxComboCount` |
| 레벨 | `LevelTable` (행 구조 `FLNPWeaponLevelRow`, **행 이름 = 레벨 숫자**) |
| 발사체 | `ProjectileType`(Linear/Guided/Lobbed), `ProjectileSpeed`, `HitRadius`, `ExplosionRadius`, `ProjectileLifetime`, `MuzzleOffset`, `ProjectileDamageEffect`, `ProjectileVFXData` |

> 공격 몽타주는 WeaponData가 아닌 **Chooser Table**에서 선택된다 (WeaponTag가 Chooser 입력 조건). → [TechDesign_CombatAnimation.md §6.1](TechDesign_CombatAnimation.md)
> `ParryRadius`·`KnockbackStrength`(콤보별 배열 포함)는 어빌리티 프로퍼티다 — 같은 무기라도 어빌리티에 따라 다르게 튜닝 가능.

### 2.4 아이템 인스턴스 / 장비·인벤토리 컴포넌트

```cpp
FLNPWeaponInstance / FLNPSkillInstance { Definition; GrantedAbilities[]; AppliedEffects[]; }
FLNPBuffInstance   { Definition; AppliedEffects[]; RemainingDuration; }  // 0 = 무기한
```

**`ULNPEquipmentComponent`** (PlayerState 부착): 무기 슬롯 1개 + Active Skill 슬롯 N개(`ULNPSettings::MaxActiveSkillSlots`, 기본 4) + Passive 목록. 장착/해제 시 `GrantItemImpl`/`RevokeItemImpl` 공통 로직으로 GA Grant/Clear·GE Apply/Remove. `DefaultWeapon`은 BeginPlay 자동 장착 — "맨손"도 WeaponData라 특수 처리가 없다.

**`ULNPInventoryComponent`** (PlayerState 부착): 보관함 + 버프 기간 추적. `AddBuffItem(Def, RemainingDuration)` → GE Infinite 적용 + Tick 차감 → 만료 시 자동 해제. `RemoveBuffItem()` → GE 제거 + **남은 시간 반환** (드랍된 월드 아이템에 이어붙이는 용도).

---

## 3. 어빌리티 상세

### 3.1 근거리 — `ULNPAbility_MeleeAttack`

```
ActivateAbility
├─ CommitAbility (쿨다운·코스트)
├─ EvaluateMontage(TAG_Montage_Situation_Attack)  ← Chooser에서 무기별 몽타주 선택
├─ 콤보 인덱스 → 섹션명 "Section_{N+1}" 결정
└─ PlayMontageAndWait 태스크
     ├─ OnCompleted/OnBlendOut → 태그 정리(ClearRelativeTag) + EndAbility
     └─ OnInterrupted/OnCancelled → 태그 정리 + EndAbility(cancelled)
```

- 어빌리티는 **몽타주가 끝날 때까지 살아있다** — 콤보 전환 시 `TryActivateAttack()`이 `CancelCurrentAttackAbility()`로 취소 후 재발동.
- `ClearRelativeTag`: 인터럽트 시 ANS가 남긴 `TAG_Block_AttackInput`/`TAG_State_ComboWindow`를 안전 정리.
- 피격 판정은 몽타주의 `ANS_LNPMeleeHitWindow` 담당. 어빌리티는 `GetAbilityDamage()`/`GetKnockbackForCombo(ComboIdx)`/`GetParryRadius()`로 판정 파라미터를 공급.

### 3.2 원거리 — `ULNPAbility_RangedAttack`

```
ActivateAbility → Commit → SpawnProjectile() → PlayMontage(Attack) → 즉시 EndAbility
```

`SpawnProjectile()` 핵심 단계:
1. `FLNPProjectileSharedFragment` 구성 (VFX·GE·반경·넉백 등 무기 상수) → `GetOrCreateConstSharedFragment`
2. 스폰 위치: `Muzzle` 소켓 + `MuzzleOffset`
3. 발사 방향 (`GetFireDirections`, 가상 함수):
   - 로컬 컨트롤: 카메라 크로스헤어 LineTrace 수렴점 (150cm 미만 근접 목표는 전방 폴백)
   - 서버의 원격 플레이어: 복제된 `GetBaseAimRotation()` (클라 카메라가 서버에 없음)
4. 네트워크: 예측 키/SalvoID 발급, Ghost 등록·거부 델리게이트, 관전자 Multicast 방송 (→ [TechDesign_Networking.md](TechDesign_Networking.md))
5. 방향 배열 순회하며 `FMassCommandBuildEntityWithSharedFragments`로 엔티티 빌드 (Deferred)

**산탄 (`ULNPAbility_RangedSpreadAttack`):** `GetFireDirections` 오버라이드. Cube 좌표계 육각 링 순회(`1 + 3N(N+1)`)로 중앙 1발 + N링을 균일 각도 간격으로 배치.

확산 형태는 무기 DataAsset이 아니라 **어빌리티가 소유한다** — 같은 무기가 산탄 폭이 다른 강공격·특수공격을
가질 수 있어야 하고 그 축은 어빌리티별로 갈리기 때문이다. `EditDefaultsOnly` 2종:

| 프로퍼티 | 기본값 | 의미 |
|:---|:---|:---|
| `HexRingCount` | 2 | 링 수. 발사 수 = `1 + 3N(N+1)` → 0=1발, 1=7발, 2=19발, 3=37발 |
| `HexStepDegrees` | 7.5 | 인접 셀 간 각도. 링 수와 곱한 값이 확산 최대 반각 (2링 × 7.5도 = 15도) |

⚠️ 펠릿마다 Mass 엔티티·트레일 VFX·명중 시 넉백이 **각각** 발생한다. `KnockbackStrength`를 단발 무기와
같은 값으로 두면 근접 전탄 명중 시 발수만큼 배가된다 (`GA_RangedAttack_Shotgun`은 10으로, Rifle 50 대비 낮춤).

**트레일 진영 색 (2026-08-22):** 트레일 Niagara 시스템이 `TintColor`(LinearColor) User 파라미터를 노출하면
`ULNPProjectileVisualSubsystem`이 진영 색(`LNPSettings`의 `Player/EnemyProjectileTintColor`)을 주입한다.
현재 `NS_BulletGlow`(Shotgun·적 NPC Pistol 공용)만 대응. 파라미터가 없는 시스템에서는 `SetVariableLinearColor`가
조용히 무시되므로 미대응 트레일도 그대로 동작한다.

- **패링 시 색 전환**이 공짜로 따라온다 — Processor가 `Proj.InstigatorTeam`을 반전시키므로,
  `FLNPProjectileVisualFragment::AppliedTeam`과 어긋나는 프레임에만 `SetTrailTeam()`으로 재주입한다.
  매 프레임 Niagara 파라미터를 건드리면 샷건 19발에서 헛비용이 된다.
- ⚠️ **`ScaleColor`(Particle **Update**)의 `Color Value To Scale`을 `User.TintColor`에 링크해야 한다.**
  `InitializeParticle.Color`(Particle Spawn)에만 링크하면 스폰 시점 색으로 고정돼 패링해도 색이 안 바뀐다.
- ⚠️ 트레일 파티클은 `Lifetime = 9999`(C++이 컴포넌트를 파괴하는 구조)라 `NormalizedAge ≈ 0`이다.
  나이 기반 커브(알파 페이드·크기 축소)는 전부 무효 — 필요하면 `Particles.Age` 절대값으로 구동할 것.

**머티리얼 (`M_LNP_ProjectileGlow`):** Niagara 기본 `M_DepthFade_SpriteSimple`은 **BLEND_Additive**라
배경에 색을 더하기만 해서 원리적으로 불투명해질 수 없다(아무리 밝혀도 연기처럼 비침). 진영 구분을 위해
**Masked + Unlit** 머티리얼을 신설했다 — UV 중심 거리로 하드 컷한 원반 + 진영 색의 짙은 버전(×0.09) 아웃라인.
⚠️ Unlit이라 `TintColor`가 곧 Emissive다. 세 채널이 모두 1을 넘으면 톤매핑에서 흰색으로 포화돼 진영 구분이
사라지므로, 색조 채널 하나만 1을 살짝 넘기고 나머지는 1 이하로 둔다.

### 3.3 패링 리액션 — `ULNPAbility_ParrySuccess` / `ULNPAbility_Stagger`

GameplayEvent 트리거로 자동 발동(생성자에서 `AbilityTriggers` 등록). 각각 방어자 ReactionMontage / 공격자 StaggerMontage를 재생하고 즉시 종료. Player·Enemy 어느 쪽 ASC에든 Grant 가능.

---

## 4. 발동 흐름 (입력 → 어빌리티)

```
AttackAction 입력 (ULNPInputHandlerComponent)
  → ALNPCharacterBase::TryActivateAttack()
      ├─ ComboWindow 태그 있음 → 콤보 전환 (태그 소비 + 인덱스 증가 + 서버 동기화 + 재발동)
      ├─ Block.AttackInput 있음 → false (호출자가 0.05초 버퍼로 재시도)
      └─ 평상시 → ResetCombo 후 ASC->TryActivateAbility(WeaponSlot.GrantedAbilities[0])
```

무기 교체 시 `GrantedAbilities[0]`이 새 무기의 GA 핸들로 교체되므로 입력 재바인딩이 필요 없다.

---

## 5. 어필 포인트 (설계 판단)

### 5.1 "발사체 = Mass Entity" — GAS와 Mass의 역할 분담

어빌리티는 **스폰까지만** 책임지고 즉시 종료한다. 수백 발 동시 비행 시에도 어빌리티 인스턴스·액터·컴포넌트 비용이 없고, 이동·판정·VFX·파괴는 4단 프로세서 파이프라인이 청크 단위로 병렬 처리한다. 무기 상수는 `ConstSharedFragment`로 공유되어 같은 무기의 발사체가 청크·메모리를 공유한다.

### 5.2 단일 Cooldown GE + per-spec Duration 주입

GAS의 표준 관행(무기마다 Cooldown GE 클래스)을 버리고 `SetDuration(WeaponDef->FireCooldown)` 주입으로 단일 클래스가 모든 무기를 커버 — 무기 추가가 DataAsset 편집만으로 끝난다.

### 5.3 Meta Attribute 기반 피해 정산

피해를 Health에 직접 쓰지 않고 `IncomingDamage` Meta 어트리뷰트를 경유시켜, 방어력·클램프 처리를 `PostGameplayEffectExecute` 한 곳에 집중. 피해 출처(근접/원거리/스플래시)가 늘어도 정산 로직은 불변.

### 5.4 크로스헤어 수렴 발사와 서버 폴백

3인칭 총기의 고전 문제(총구 방향 ≠ 화면 중앙)를 카메라 광선 수렴점으로 해결하되, 카메라가 존재하지 않는 서버의 원격 플레이어는 Mover InputCmd로 복제된 시선 회전을 사용 — 패럴랙스 오차는 코스메틱 범위로 한정된다.

---

## 6. 미구현 항목

| 항목 | 선행 조건 | 세부 내용 |
|:---|:---|:---|
| 발사체 전용 트레일 VFX (Pistol·Rifle) | 없음 | 두 무기는 Ribbon 렌더러 기반 `NS_*BulletTrail`을 쓴다. 진영 색을 받으려면 `TintColor` User 파라미터를 같은 방식으로 추가해야 함 (§3.2 트레일 진영 색) |
| Active Skill 입력 바인딩 | 없음 | `ActiveSkillActions` 배열은 InputHandler에 존재 — 슬롯 GA 발동 연결 필요 |
| Passive Skill GameplayEvent | 없음 | 피격 시 피격자 ASC에 이벤트 전송 → Passive 자동 발동 트리거 연결 |
| ParrySuccess/Stagger 자동 Grant | 없음 | 현재 `DefaultAbilities` 배열로 수동 지정 — 초기화 흐름 정식화 필요 |
| LootPod → 인벤토리 연동 | LootPod 보상 시스템 | 루팅 성공 아이템을 `InventoryComponent`에 추가하는 흐름 (→ [TechDesign_LootPod.md](TechDesign_LootPod.md)) |
