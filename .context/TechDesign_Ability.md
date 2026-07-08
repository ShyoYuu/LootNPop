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
| Health / MaxHealth | 100 / 100 | Health는 [0, MaxHealth] 클램프 |
| AttackPower | 10 | 피해 공식의 기저 |
| AttackSpeed / MoveSpeed | 1 / 1 | ≥ 0.01 클램프 |
| DefensePower | 0 | `LNPDamage::ApplyDefense` 공식에 사용 |
| AttackMultiplier | 1 | 피해 배율 (≥ 0.01) |
| IncomingDamage | 0 | **Meta 어트리뷰트** — 복제 안 함. GE가 전달한 원시 피해량을 `PostGameplayEffectExecute`에서 방어력 적용 후 Health에 반영하고 즉시 0으로 초기화 |

방어력 공식 (`LNPDamageFormula.h`): `FinalDamage = RawDamage * (100 / (100 + Defense))`

### 2.2 피해 파이프라인

```
판정 (Mass Processor)
  → FLNPApplyDamageGECommand (Game Thread)
      → ULNPGameplayEffect_Damage (Instant GE) — TAG_GE_Data_Damage SetByCaller로 피해량 전달
          → IncomingDamage (Meta) → PostGameplayEffectExecute → 방어력 적용 → Health 차감
```

기본 피해량 계산 (`ComputeDamage`): `(AttackPower + WeaponData.Damage) × AttackMultiplier`

쿨다운은 단일 `ULNPGameplayEffect_Cooldown` 클래스에 **어빌리티가 무기별 `FireCooldown`을 per-spec Duration으로 주입** — 무기마다 GE 클래스를 만들지 않는다.

### 2.3 아이템 정의 DataAsset

```
UPrimaryDataAsset
  └── ULNPItemDefinitionBase (Abstract)
        ├── DisplayName / AbilitiesToGrant / EffectsToApply
        ├── ULNPWeaponData   ← 무기 (아래 표)
        ├── ULNPSkillData    ← 스킬 (Active/Passive 구분은 슬롯 위치)
        └── ULNPBuffData     ← 버프 (MaxDuration, 0 = 무기한)
```

**`ULNPWeaponData` 주요 필드:**

| 분류 | 필드 |
|:---|:---|
| 태그·애니메이션 | `WeaponTag`, `DefaultAimMode`, `AnimLayerClass` |
| 메시 | `WeaponMesh`, `AttachSocketName`, `WeaponMeshRelativeLocation/Rotation` (그립 피벗 보정) |
| 공격 | `FireCooldown`, `MaxComboCount`, `Damage` |
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

**산탄 (`ULNPAbility_RangedSpreadAttack`):** `GetFireDirections` 오버라이드. Cube 좌표계 육각 링 순회(`1 + 3N(N+1)`)로 중앙 1발 + 2링 = 19발을 균일 각도 간격으로 배치.

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
| 발사체 Niagara VFX 에셋 | 없음 | 파이프라인(`ULNPProjectileVisualSubsystem` trail 풀·임팩트 큐)은 완성. `ULNPVFXData` 에셋 제작·할당만 잔여 |
| Active Skill 입력 바인딩 | 없음 | `ActiveSkillActions` 배열은 InputHandler에 존재 — 슬롯 GA 발동 연결 필요 |
| Passive Skill GameplayEvent | 없음 | 피격 시 피격자 ASC에 이벤트 전송 → Passive 자동 발동 트리거 연결 |
| ParrySuccess/Stagger 자동 Grant | 없음 | 현재 `DefaultAbilities` 배열로 수동 지정 — 초기화 흐름 정식화 필요 |
| LootPod → 인벤토리 연동 | LootPod 보상 시스템 | 루팅 성공 아이템을 `InventoryComponent`에 추가하는 흐름 (→ [TechDesign_LootPod.md](TechDesign_LootPod.md)) |
