# LootNPop 개발 계획 (Development Plan)

---

## Phase 1: Foundation (핵심 시스템 & 구형 월드)

- [x] **Mover 플러그인 기반 캐릭터 이동**
    - `ULNPCharacterMoverComponent` + `ALNPCharacterBase` 구현. Enhanced Input 매핑 포함.
- [x] **구형 중력 시스템** (`ULNPPawnGravityComponent`)
    - `RadialOutward` 모드로 Dyson Sphere 내벽 중력 구현. 곡률 보정(`UpdateControllerOrientation`) 및 Pitch 클램프 완료.
- [x] **질주 시스템** (`LNPSprintModifier`)
    - Mover의 Stance 패턴 적용. 수정자(Modifier) 기반 설계로 네트워크 롤백 안정성 확보.
- [x] **게임 초기화 시퀀스** (`ALNPGameMode`, `ALNPGameState`)
    - 4단계 서버 주도 초기화: `WorldGeneration → SurfaceBaking → EntitySpawning → Complete`
    - 서버: `GameMode::BeginPlay()` 기점으로 단계 진행 및 `ServerPhase` 복제.
    - 클라이언트: `OnRep_OctantGenSeed` → 월드 생성, `OnRep_ServerPhase` → TryBeginClientBaking() (투게이트 패턴).
    - 플레이어 스폰 게이팅: `bServerInitComplete = true`까지 `PendingPlayers` 큐 보관.
- [x] **Octant 기반 월드 분할 스폰** (`ULNPOctantSpawnSubsystem`)
    - 8개 Level Instance를 결정론적 시드(`FRandomStream(OctantGenSeed)`)로 배치.
    - 서버 GameMode → 클라이언트 OnRep 경유로 동일 seed 보장.
- [x] **표면 캐시 시스템** (`ULNPSurfaceCacheSubsystem`)
    - 게임 시작 전 구형 내벽 표면 좌표를 등장방형(Equirectangular) 그리드로 사전 계산.
    - Mass 워커 스레드에서 O(1) 안전 조회. NavMesh 및 인라인 라인 트레이스를 대체.
- [x] **로딩 스크린 흐름** (`ALNPPlayerController`)
    - 클라이언트 로컬 베이킹 완료 시 로딩 스크린 해제 후 서버에 Ready 신호 전송.

---

## Phase 2: Environment (환경 및 상세화)

- [x] **PCG 기반 구체 지형 생성** (`UPCGSphereWorldSettings`)
    - Spherified Octant 투영 + 3D Perlin Noise 변조 + 경사면 정렬 구현.
    - 결과물은 HISM으로 Bake하여 런타임 계산 제거.
- [x] **인터랙터블 레지스트리 기반 상호작용 인프라** (`ULNPInteractableRegistrySubsystem`, `ULNPInteractionComponent`)
    - 살아 있는 인터랙터블(Pod·Dice) 레지스트리를 컴포넌트가 순회. `UMassAgentComponent` + Niagara VFX 연동.
    - SmartObject 공간 쿼리는 Mass Representation의 액터 풀링(재사용·텔레포트)과 충돌해 폐기 (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 03). `ALNPLootPod`의 `SmartObjectComponent`는 미사용 잔존.
- [x] **LootPod 랜덤 스폰 로직**
    - Mass Spawner와 연동한 LootPod 위치 결정 및 동적 스폰.

---

## Phase 3: Gameplay (전투 및 상호작용)

- [x] **MassEntity 기반 루팅 시스템** (`ULNPLootingProcessor`, `ULNPIdleToLootingProcessor`)
    - 상태 전환(Idle ↔ Looting → Popped), 게이지 누적, 거리 체크 완료.
    - 루팅 존(500cm) 내 **모든 플레이어의 루팅 속도 합산** — 협동 시 가속. 상호작용 반경(150cm)과 분리.
    - 전원 이탈 시 게이지 감쇠(존 활성 유지, 복귀만으로 재개), 0 도달 시에만 완전 취소.
    - 게이지 완료 → `Popped` → `ALNPLootDice::SpawnPodRewards` 보상 스폰 + 엔티티 파괴.
    - 설계 명세: [TechDesign_LootPod.md](TechDesign_LootPod.md)
- [x] **LootDice 보상 픽업** (`ALNPLootDice`, `ULNPLootDiceRewardTable`)
    - Mass가 아닌 **순수 Actor** — 서버 권위 Chaos 물리 + Iris `ReplicatedMovement`(각속도 포함). Pod Pop 임펄스·고속 회전.
    - 가중 추첨 리워드 테이블(`DA_LootDiceRewardTable`), 획득 시 Server RPC → 인벤토리 편입, 60초 타이머 소멸.
    - PIE 1인 검증 완료. **잔여:** 2인 복제(정지 윗면 일치·슬립 트래픽), 구형 월드 중력, 드랍/양도 검증.
    - 설계 명세: [TechDesign_LootDice.md](TechDesign_LootDice.md)
- [x] **인벤토리 아이템 인스턴스 모델** (`ULNPInventoryItemInstance`, `ULNPInventoryComponent`)
    - 공유 DataAsset 포인터 → **UObject 인스턴스 + `FGuid ItemId` 정체성**으로 전환. 장착본/보관본 오검출 버그 해소.
    - FastArray 델타 복제 + 등록 서브오브젝트(`COND_OwnerOnly`), Iris 2인 PIE 검증 완료.
    - 스탯은 `FLNPGameplayTagStackContainer`(Lyra 포팅). 버프는 서버 권위 카운트다운 + 클라 로컬 표시, 드랍/재획득 잔여시간 라운드트립.
    - **잔여:** 인스턴스별 **랜덤** 스탯 롤링, 스태킹/수량, 정렬·필터. (무기 레벨은 아래 항목에서 해소)
    - 설계 명세: [TechDesign_Inventory.md](TechDesign_Inventory.md)
- [x] **무기 레벨·합성** (`FLNPWeaponLevelRow`, `ULNPWeaponData::LevelTable`, `ULNPInventoryComponent::TryMergeItem`)
    - 레벨별 스텟·어빌리티 계수를 **무기별 DataTable에 절대값으로** 입력한다(공식 아님, 행 이름 = 레벨).
      **테이블의 마지막 연속 행이 곧 그 무기의 최대 레벨.** 현재 테스트 데이터는 10레벨까지(지수 ×2 씨앗).
    - 같은 종류·같은 레벨 n개(`LNPSettings.WeaponMergeMaterialCount`, 기본 3) → 다음 레벨 1개.
      **대상 자신이 결과물**이 되므로 "비장착 n개 → 1개"와 "장착본 +1레벨(n-1 소모)"이 한 경로로 성립한다.
    - 무기 레벨이 GAS 어빌리티 스펙 레벨로 흘러 `ComputeDamage`가 `AttackPower × 계수`를 낸다.
      기초 스텟·계수가 둘 다 오르므로 **피해는 제곱으로 증가** — `AbilityCoefScale`은 완만하게 둘 것.
    - LootDice 페이로드에 `ItemLevel`(COND_InitialOnly) — **다른 플레이어가 주워도 레벨 보존**.
    - UI: 셀 배지 3분할(장착 좌상단 / 버프 잔여 우상단 / 레벨 우하단), 디테일 패널 Merge 버튼(`Merge (2/3)` · `Max Lv.`).
    - 2P Standalone 검증 완료(2026-08-20): 게스트 합성·장착본 합성·드랍 후 타 플레이어 획득 시 레벨 보존, 최대 레벨 차단, 레벨별 피해 차이.
    - 설계 명세: [TechDesign_Inventory.md](TechDesign_Inventory.md) §7, [GameDesign_Ability.md](GameDesign_Ability.md) §3.1
- [x] **CommonUI 인게임 메뉴** (`ULNPMenuRootWidget` 외 UI/Menu 13종)
    - 3탭(캐릭터 스탯 / 인벤토리 / 환경설정). `UCommonActivatableWidgetStack`(열기·닫기) + `UCommonActivatableWidgetSwitcher`(탭 전환) 조합.
    - 게임패드 우선 조작 — L1/R1 탭 이동, ✕ 선택, ○ Back. 루트 하나만 Back 핸들러로 두고 활성 탭에 위임(`디테일 → Grid → 닫기`).
    - 스탯 탭: `ULNPStatsViewModel`이 GAS 어그리게이터 식대로 `C (A × B)` 분해 → `URichTextBlock` 인라인 마크업(MVVM 필드 1개).
    - 인벤토리 탭: `CommonTileView` Grid + 디테일 패널, Equip/Drop. 메뉴 중 폰 입력 매핑 컨텍스트 제거, 스탠드얼론에서만 일시정지.
    - 하단 힌트 바(`ULNPMenuHintBarWidget`): 탭이 자기 힌트를 선언하고 루트가 Back·탭 이동을 얹는다.
      입력 타입에 따라 키 심볼이 자동 전환(`LNPInputGlyph`, 텍스트 심볼). 상호작용 프롬프트도 같은 해석기를 쓴다.
      ⚠️ `UCommonBoundActionBar`는 ✕·방향 이동을 표현할 수 없어 폐기(사유는 TechDesign §3.3).
    - 다국어 기반: `Config/Localization/Game.ini` 타깃(en 원본, en/ko 생성).
    - 구 `ULNPInventoryWidget`·`ULNPInventoryEntryWidget` 및 관련 WBP 폐기.
    - **잔여:** 힌트 바·프롬프트의 게임패드 실기기 확인, 환경설정 탭 내용, 2인 PIE 일시정지 미적용 확인.
    - 설계 명세: [TechDesign_InGameMenu.md](TechDesign_InGameMenu.md)
- [x] **합/곱 이원 버프 시스템** (`GAS/LNPStatModifier.*`, `ULNPGameplayEffect_StatFlat/_StatPercent`)
    - `최종 = (기초 + 무기 스텟 + 합연산 버프) × (1 + Σ 곱연산 버프)` — GAS의 `AddBase` / `MultiplyAdditive`
      두 채널만 써서 계산 코드 없이 성립시킨다. 곱연산 중복 시 배율이 합산되어 체감 효율이 체감.
    - 아이템 DataAsset이 `{어트리뷰트, Flat/Percent, 크기}`를 선언하고 공용 GE 2종이 SetByCaller로 수용 —
      스텟×연산 조합마다 GE 에셋을 만들지 않는다. 아이템 설명문도 이 선언에서 생성.
    - 무기 스텟을 어트리뷰트 파이프라인으로 이관(`WeaponData.Damage` 제거), 전용 배율 어트리뷰트 `AttackMultiplier` 제거,
      `DefensePower` 기초 0 → 10(곱연산 버프가 무효화되지 않도록), 영구 버프(`Duration = -1`).
    - **잔여:** 방어력 기초값 밸런스 회귀.
      (무기 스텟 이관은 2026-08-20 무기 레벨 테이블로 완료 — 무기 3종의 스텟 원본이 `LevelTable`로 옮겨졌다.)
    - 설계 명세: [TechDesign_Ability.md](TechDesign_Ability.md) §2.1, [GameDesign_Ability.md](GameDesign_Ability.md) §3.3
- [x] **GAS 기반 전투 시스템**
    - ASC/AttributeSet (`ALNPPlayerState`), `ULNPEquipmentComponent`, `ULNPInventoryComponent`.
    - 어빌리티 계층: `ULNPGameplayAbility` → `ULNPAbility_BasicAttack` → `ULNPAbility_RangedAttack` / `ULNPAbility_MeleeAttack`.
    - 발사체 시스템: `ULNPProjectileMovementProcessor`(PrePhysics) + `ULNPProjectileHitDetectionProcessor`(StartPhysics) + Visualization + Destruction 4개 프로세서.
    - 선분-캡슐 원거리 HitDetection, `InstigatorTeam` 팀 구분 피격 처리.
    - 공격 입력 바인딩 (`ULNPInputHandlerComponent`), 0.05초 입력 버퍼링.
- [x] **전투 애니메이션 시스템** (`ABP_Player`, `ULNPAnimInstance`)
    - `ALI_WeaponStyles` 인터페이스 정의 및 무기별 서브 AnimBP (`ABP_Unarmed`, `ABP_Sword`, `ABP_Pistol`) 제작.
    - UpperBody / FullBody 슬롯 분리 블랜딩 파이프라인 + Inertialization 적용.
    - GAS `State.Block.MovementInput` 태그 기반 이동 입력 차단 (`ULNPInputHandlerComponent` 연동).
    - 설계 명세: [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md)
- [x] **총기류 Aim 모드**
    - 총기 장비 시 UpperBody 레이어에 Aiming 포즈 블랜딩.
    - ADS 카메라 전환(`CR_ADS` 리그 프리셋 + `CDE_ThirdPerson` 디렉터 분기), 조준 감도 완화,
      ADS 중 대시·질주 차단 및 이동 속도 저하(`FLNPADSModifier`). 상세: [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §2.6
- [x] **Enemy NPC HP Bar** (월드 스페이스)
    - High LOD Actor 상태에서만 표시. `HP > 0 && HP < MaxHP` 조건 충족 시 가시화.
    - `UWidgetComponent` (World Space, Transparent 블렌드) + `ULNPHpBarWidget` (BindWidget 기반).
    - GAS Health 속성 변경 델리게이트로 실시간 갱신. 스폰 시 `SyncFromEntity`에서 초기값 주입.
    - Blueprint 서브클래스(`WBP_LNPHpBar`)에 `UProgressBar` 이름 `HpBar`로 배치 필요.
- [x] **플레이어 HUD** (MVVM 기반)
    - `ULNPHudViewModel`(FieldNotify): ASC 델리게이트로 `HealthPercent`, `bIsFreeAiming` 자동 갱신.
    - `ULNPHudWidget`: ViewModel 생성·주입(`UMVVMView::SetViewModel`)·해제 담당.
    - `ALNPPlayerController`: `BeginPlay`에서 위젯 생성, `OnPossess`/`OnUnPossess`에서 ViewModel 초기화·해제.
    - 조준점: `TAG_AimMode_FreeAim` 활성 시에만 표시 (bIsFreeAiming → Blueprint Function Binding).
    - 설계 명세: [TechDesign_HUD.md](TechDesign_HUD.md)
- [x] **근접 HitDetection** (AnimNotify 기반)
    - `UANS_LNPMeleeHitWindow`: 무기 본 위치를 매 프레임 `FLNPWeaponTraceFragment`에 기록.
    - `ULNPWeaponTraceHitDetectionProcessor`: Swept Quad(삼각형 2개) vs. 캡슐 축 선분 최단 거리 판정. `SwordRadius + CapsuleRadius` 임계값.
    - `ULNPWeaponTraceLifetimeProcessor`: `TimeToLive` 만료 시 엔티티 자동 파괴 (NotifyEnd 미호출 안전장치).
    - 중복 피격 방지 (`AlreadyHit[8]` 배열). 에디터 전용 디버그 드로우 프로세서 포함.
    - 피격 시 `FLNPPlayerLootingTag` 제거 → LootPod 루팅 취소 연동은 미구현.
- [x] **콤보 시스템**
    - 몽타주 섹션 분기 기반 다단 콤보 구현.
- [x] **Chooser 기반 몽타주 선택 시스템**
    - Chooser 테이블을 활용하여 상황에 맞는 공격 몽타주 자동 선택.
- [x] **산탄 공격**
    - 다수의 발사체를 분산 발사하는 산탄 공격 구현.
- [x] **히트리액션**
    - 피격 시 캐릭터 히트리액션 애니메이션 구현.
- [ ] **피격 반응 시스템** (넉백·드랍)
    - HitStop 구현 완료.
    - 미구현: 넉백 Launch (구형 곡률 기반 궤적), 아이템 드랍.
- [x] **경직(Poise) 시스템** (구현·에디터 작업 완료, PIE 1인 + Standalone 2인 검증 — 2026-08-29)
    - **자연회복만이 경직도를 줄인다.** 리셋·차감·면역이 없고, 자연회복 속도를 상회하는 화력을 몰아쳐야만
      게이지가 유지·상승한다. 유일한 예외가 다운(게이지 0 + 면역)이며 그것이 스턴락을 끊는 단 하나의 탈출구다.
    - **T1 이상 = 그로기**(공격·이동 불가, 누적 계속), **T2 도달 = 다운**(고정 시간 + 리셋 + 면역).
      그로기는 고정 시간이 아니라 게이지 값에 종속된 상태다 — 종료는 프로세서가 이탈 에지에서 GA를 취소한다.
      T1~T2 간격이 곧 딜 구간이라 **임계값은 폰별**이다(적은 넓게, 플레이어는 좁게) — 전역 상수로 두면
      저항이 높은 쪽 밴드가 오히려 길어져 의도와 정반대가 된다.
    - "T1 바로 위 걸치기" 무한 그로기는 **유지 시간 비례 유입 보너스**(`PoiseGroggyBonusPerSecond`)로 막는다 —
      타임아웃과 달리 다운이 항상 타격 위에서 일어나 인과가 보인다.
    - `FLNPPoiseFragment`(서버 전용, 비복제) + `ULNPPoiseProcessor`(감쇠·임계) + `FLNPStaggerCommand`(발동).
      Actor 승격 여부와 무관하게 같은 눈금으로 쌓이고, Low LOD 적은 `bIsGroggy`·`ImmunityTimeRemaining`으로 이동만 멈춘다.
    - **입력 차단은 GA가, 몽타주는 GameplayCue가 소유한다** — 적 ASC가 `Minimal` 복제라 어빌리티 활성화가
      시뮬 프록시에 안 가기 때문. 부수 효과로 기존 패링 스태거의 미복제 결함도 해소.
    - 일반 히트리액트 몽타주에는 입력 차단 ANS를 붙이지 않기로 결정 (사유: TechDesign_CombatAnimation §6.4).
    - 가드 브레이크: 막아낸 공격도 `PoiseGuardMultiplier`만큼 누적, 돌파 시 `Client_ForceReleaseGuard`로 가드 해제.
    - **근접 패링도 경직 시스템으로 통합** (`LNPPoise::ApplyParryBreak`) — 공격자 T1의 `PoiseParryBreakRatio`배를
      한 번에 쏟아부어 그로기에 빠뜨린다(저항 미적용). 전용 스태거 GA·`Parry.Stagger` 이벤트·`Parry.Parried`
      공격자 몽타주 직접 재생은 제거 — 고정 시간 GA와 게이지 그로기를 병행하면 GA가 먼저 끝나며 그로기가 조용히 깨진다.
      패링 연출은 `Value.Stagger.Parried` 밸류 태그로 살렸다(행동은 일반 그로기와 동일, 몽타주만 분기).
    - 랙돌·사망 대상 제외. 검증용 시각화: `LNP.Debug.DrawPoise 1` (`ULNPPoiseDebugDrawProcessor`, 에디터·서버 전용,
      표시 거리는 `LNP.Debug.DrawPoiseDistance`).
    - **Standalone `-game` 2인 검증 완료:** 발신 13 / 호스트 13 / 게스트 13, 밸류 태그 3종(Light·Heavy·Parried)이
      양쪽 로그에서 완전 일치. 호스트·게스트 어느 쪽이 패링해도 동일하게 재현된다.
    - **에디터 작업 (전부 완료):**
        1. `DA_PlayerEntityConfig`의 `MassAssortedFragmentsTrait`에 `FLNPPoiseFragment` 추가.
           ⚠️ 없으면 플레이어는 경직도가 아예 안 쌓인다 (`PushPoiseResistanceToEntity`가 경고 로그를 남긴다).
        2. **GA_Stagger를 무기에서 폰으로 이관.** 원래 `DA_LongSword`·`DA_NPC_LongSword`의 어빌리티 목록에서만
           참조돼 **롱소드를 들지 않은 폰은 경직 어빌리티가 없어 입력 차단이 안 걸렸다**
           (몽타주 큐는 떠서 "굳는 시늉만 하고 계속 움직이는" 증상). `BP_LNPPlayer` /
           `ULNPEnemyConfig`의 `DefaultAbilities`로 옮겨 무기와 무관하게 만들었다.
        3. Chooser 행 3종 — Light `AM_SW_Damage_Fast` / Parried `AM_SW_Damage_Backward` / Heavy `AM_MM_HitReact_Front_Hvy_01`.
           `GCN_LNP_Character_Stagger` 노티파이 에셋.
        4. 어빌리티별 `PoiseDamage`·`ComboPoiseDamages` 값, `DA_Enemy_*`의 `PoiseResistance`·`Poise*Threshold`.
    - **검증용 임시값은 전부 원복 완료 (2026-08-29).** 검증 중에는 양쪽 다 경직 발동 전에 먼저 죽어
      관찰이 불가능했던 탓에 공격력을 눌러 두었으나, 검증이 끝나 원래 값으로 되돌렸다 —
      `DT_*_Levels` 4종(+30 / +10 / +10 / +20), `DA_NPC_Pistol`(+10), `DefaultGame.ini`의 임계값 오버라이드 제거,
      경직력도 초기값(근접 [30, 45] · 피스톨 12 · 라이플 10 · 샷건 3)으로 복귀.
    - **임계값 초안을 C++ 기본값에 반영** — `ULNPSettings` 플레이어 60 / 95(밴드 35), `ULNPEnemyConfig` 적 60 / 200(밴드 140).
      ini 오버라이드 없이 이 값이 시작점이 되도록 했다(구 기본값 100 / 200은 폰별 임계값 도입 전 값이라 의도와 반대였다).
    - ⚠️ **수치 밸런스는 플레이로 조정하기 전이다.** 원복된 공격력 기준 근접 3~4타에 서로 죽어서
      플레이어 경직(T1까지 약 4타)이 거의 발동하지 않는다 — 경직 임계값이 아니라 공격력·HP 쪽 문제다.
    - 부수 발견: **`DA_NPC_LongSword`은 참조자가 0인 미사용 에셋이다.** 근접 적은 `DA_LongSword`를 쓴다 — 정리 대상.
    - 부수 수정: **`FLNPEnemyFragment::Defense`가 시드되지 않아 항상 0**이었다 — 같은 공격이 Low LOD 적에게만
      15% 더 아프게 들어가고 있었다(High LOD는 ASC의 DefensePower 15를 쓴다). `BuildTemplate`에서 MaxHealth와
      같은 방식(`LNPStat::ResolveStatValue`)으로 시드하도록 수정.
    - 설계 명세: [TechDesign_Poise.md](TechDesign_Poise.md), [GameDesign_Poise.md](GameDesign_Poise.md)
- [x] **Guard / Parry 시스템** (핵심 기능 완료, GameplayCue 에셋 연결 잔여)
    - Guard: `FLNPParryStateFragment` Fragment 각도 판정 → `FLNPGuardBlockCommand` → 데미지 차단 + GameplayCue. 동작 확인.
    - Parry(근접): Guard 입력 직후 0.15초 창 → `FLNPMeleeParryCommand` → 방어자 GA_ParrySuccess + 공격자 GA_Stagger.
    - Parry(투사체): Processor에서 Fragment Velocity/InstigatorTeam/Instigator 반전 → `FLNPProjectileParryCommand` (방어자 GA_ParrySuccess만). 동작 확인.
    - 판정 구조: `FLNPParryStateFragment` Mirror Fragment 기반으로 Processor(Worker Thread)에서 직접 판정. `FLNPApplyDamageGECommand`는 GE 적용 + HitReact + HitStop 처리.
    - 판정 반경 분리: `HitRadius`(피격)와 `ParryRadius`(패링)를 독립 필드로 분리. 2단계 판정 — ParryRadius 먼저 체크, 미발동 시 HitRadius 체크. 동작 확인.
    - Guard 이동 제한: `FLNPGuardModifier` (GuardWalkSpeed 200 cm/s) — Sprint와 동일한 Mover Modifier 패턴.
    - Guard 자세 애니메이션: `ABP_Sub_LongSword`에서 `bIsGuarding` Bool Blend 노드로 Guard 자세 블렌딩. 동작 확인.
    - 에디터 잔여: GameplayCue 에셋 연결 (`GameplayCue.LNP.Guard.Block`, `GameplayCue.LNP.Parry.Success`), Guided/Lobbed 투사체 반사 타입.
    - 설계 명세: [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md)

---

## Phase 4: AI & Scale (대규모 AI)

- [x] **MassEntity 기반 적 NPC 스폰** (`ULNPMassSpawnSubsystem`)
    - 데이터 에셋(`DA_MassSpawnConfig`) 기반 배치. 스폰 완료 시 `OnSpawningComplete` 델리게이트 발행.
- [x] **적 이동 프로세서** (`ULNPEnemyMovementProcessor`)
    - 구형 표면을 따른 이동. `ULNPSurfaceCacheSubsystem`에서 표면 노멀 조회 (스레드 안전).
- [x] **슬롯 기반 타겟팅 서브시스템** (`ULNPTargetingSubsystem`)
    - Melee/Ranged 슬롯 풀 관리. `DistanceToTargetSq` 기반 우선순위 경쟁.
- [x] **StateTree 기반 적 AI** (`ULNPEnemyStateTreeProcessors`)
    - Leash / Chase / Attack 상태 전환. Mass-StateTree 통합.
- [ ] **Enemy Low LOD(순수 엔티티) 전투** — 트랙 A 완료 (2026-09-05), 트랙 B·C 미착수
    - `ULNPEnemyConfig::CombatMode`(`ActorPromoted` / `PureEntity`)로 승격을 **옵션화**. 기본값은 종전 거동.
      승격 차단은 EntityConfig의 `LODRepresentation`이 맡고, 코드는 전투 시 LOD를 끌어올리지 않는 것까지만 한다 —
      LOD를 눌러 막으면 유의도·틱 레이트까지 함께 눌린다. 어긋남은 `ULNPEnemyTrait::ValidateTemplate`이 양방향 경고.
    - `ULNPEntityAttackProcessor`가 공격 위상(Windup/Active/Recovery)을 직접 구동한다. **판단은 StateTree Task가,
      진행은 프로세서가** — 신호 구동인 Task Tick에 위상을 두면 신호가 끊긴 프레임에 스윙이 중간에 멈춘다.
      경직·다운 시 공격 중단도 여기가 유일한 경로다(Actor가 없으면 `FLNPStaggerCommand`가 도달하지 못한다).
    - 근접은 **절차적 가상 칼날** — 4점을 계산해 기존 Swept Volume 판정을 그대로 태운다. 판정·패링·가드 코드는
      한 줄도 바뀌지 않았다. 칼날은 별도 엔티티라 2패스(계산→반영), 마커는 Tag가 아니라 Fragment.
    - 원거리는 산탄까지 지원하며 배치 공식을 `LNPSpread::BuildHexRingDirections`로 어빌리티와 공용화.
    - 부수 수정: **적 판정 캡슐 96cm 이중 보정 제거.** 좌표 규약 통일 이전의 잔재가 남아 있었는데, 전투 중 적이
      예외 없이 승격되던 동안에는 그 분기가 실전에서 거의 안 돌아 드러나지 않았다. 먼 거리 Low LOD 적 저격에도
      같은 오차가 있었다. 판별 원본도 `EnemyTypeTag` 문자열 비교 → `ELNPEnemyAttackType` 필드로 교체.
    - **잔여:** 트랙 B(행동 상태 1바이트 복제)·트랙 C(ISM↔ISKM). 게스트에 엔티티 발사체가 안 보이는 것은
      Stage 4(관전 가시성) 미착수라 정상이다. **검증용 임시값 2건 원복 필요**(무기 레벨1 공격력 0, NPC 체력 5배).
    - 설계 명세: [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md)
- [ ] **시야각·상태 기반 타겟팅 가중치 보강**
    - 현재 거리 기반만 구현. 시야각(Angle) 및 공격 상태 가중치 추가.
- [ ] **난이도 스케일링**
    - 활성 LootPod 수 추적 → 슬롯 한도 또는 적 능력치 단계적 조정.
- [x] **LOD 기반 Actor ↔ Entity 전환**
    - `ULNPEnemyLODOverrideProcessor`: Confirmed/Combat 상태 시 거리 무관 High LOD 강제.
    - `ULNPEnemyActorInitializerProcessor` / `ULNPEnemyActorSyncProcessor`: Config 기반 초기화, HP/타겟 역동기화.
- [x] **NPC 이동 결함 정비** (2026-08-26 ~ 08-27)
    - **좌표 규약 통일**: 엔티티 Transform 기준점을 **캡슐 중심**으로 못박음.
      LOD에 따라 발밑↔중심으로 96cm 튀던 것을 해소. LowLOD ISM 오프셋도 함께 조정.
    - **배회 교착 해소**: 구면에서 목적지 거리를 **접평면 성분으로만** 재도록 변경.
      반경 방향 차이를 거리에 섞으면 방향 벡터가 0이 되어 엔티티가 완전히 굳었다.
    - **호스트 LowLOD 미표시 해소**: 리슨 서버는 넷 모드 플래그가 `Client|Server`라
      `UMassCrowdServerRepresentationLODProcessor`(Server 전용, 거리 테이블 하드코딩)가
      시각화 LOD를 덮어썼다. `Config/DefaultMass.ini`로 해당 프로세서 2종을 파이프라인에서 제외.
    - **이동 속도 정상화**: Mover의 `ComputeVelocity`가 이동 의도 벡터를 **정규화하지 않아**
      크기<1을 지속 입력하면 속도가 곱셈으로 붕괴(180cm/s 기대 → 27cm/s). 방향(단위 벡터)과
      속도(`SetAIDesiredSpeed` → MaxSpeed)를 분리. Entity/Actor가 같은 속도를 쓰게 되어 전환도 매끄러워짐.
    - **배회 교착 구조 제거** (08-27): 도착 임계값을 `FLNPEnemyMovementConfig::ArrivalTolerance`
      하나로 일원화(30 / 100 / 50 3중 불일치 해소)하고 **배회 타임아웃**을 넣었다. StateTree Tick은
      신호 구동이라 스스로 깨어날 수 없으므로, 시간 측정·깨우기는 매 프레임 도는 MovementProcessor가
      맡고 목표 재추첨 판단은 IdleTask가 단독으로 한다.
    - **지면 관통·Low LOD 피격 판정 종료 (08-27)**: 둘 다 좌표 규약 불일치의 파생이었고
      별도 조치 없이 해소된 것이 실측으로 확인됐다. 조사용 계측 코드는 전량 제거.
    - **잔여**: 게스트 LowLOD 이동 끊김 — 클라이언트 보간 부재. 미착수.
      LOD 전환 시 튐은 잔존하나 사용자 판정으로 **수용(보류)**.

---

## Phase 5: Loop (멀티플레이어 완성)

- [ ] **Iris 기반 네트워킹 구현**
    - 설계 명세: [TechDesign_Networking.md](TechDesign_Networking.md)
    - [x] Phase 1: Iris 활성화, GAS 복제 모드, ALNPEnemyCharacter bReplicates, GameplayCue 에셋 연결 (PIE 2P 검증 완료)
    - [x] Phase 2: HitStop/HitReact GameplayCue 전파 (코드 완료, GameplayCueNotify VFX·그래프 연결은 에디터 잔여)
    - [x] Phase 3: 근접 HitDetection 클라이언트 예측 + Lag Compensation + Guard/Parry 서버 복제 (PIE 2인 근접 PvP 왕복 검증 완료 — `UAnimNotifyState` 싱글턴 공유로 인한 근접 판정 실패 버그 발견·수정)
    - [x] Phase 4: 원거리 Projectile 클라이언트 예측 (LocalPredicted + Ghost Projectile, 거부 롤백)
    - [x] Phase 4.5: Ghost Projectile 정합성 개선 — 키 전역 고유화(SalvoID), per-entry TTL, 패링 반사 소멸+재스폰, 관전 Ghost Dead Reckoning·로컬 충돌, Rewind 발사 시점 캐싱 (PIE 3인 검증 완료)
    - [x] Phase 5: 무기 교체 서버 권한화 (Server_EquipWeapon RPC)
    - [x] Phase 6: Enemy MassReplication (FMassNetworkID, BubbleHandler) + ALNPEnemyCharacter bReplicates (이중 복제)
    - [x] Phase 6.5: Player MassReplication — 존재만 복제하는 최소 스키마 bubble + 엔진 퍼펫 링크로 클라 플레이어 엔티티 성립. 에이전트 경로 NetID 캐싱 타이밍 갭은 ULNPMassAgentComponent로 보정 (PIE 2인 검증: players=2, gap=0cm)
    - [x] Phase 7: LootPod MassReplication + ALNPLootPod bReplicates (이중 복제) — 부수 발견: 원격 클라 루팅 입력 미전달 공백을 Server_StartLooting RPC로 해소 (PIE 검증 완료)
    - [x] 발사 피치·Aim Offset 동기화 — Mover InputCmd의 ControlRotation 재사용 + bSyncInputsForSimProxy, GetBaseAimRotation 오버라이드 (PIE 검증 완료)
    - [x] Phase 8: Mass 복제 **단일 스트림 통합** — Enemy·Player·LootPod이 `ALNPMassClientBubbleInfo` / `ULNPMassReplicator` 하나를 공유. 엔진의 파괴 처리 경로가 타입 무구분이라 버블이 2개 이상이면 타 타입 엔트리를 자기 핸들로 제거하려다 크래시(2P 루팅 완료 시 실측). ⚠️ `DA_PlayerEntityConfig`는 CoreRedirects 경유 — 에디터 재저장 후 리다이렉트 제거 필요
    - [x] 자세 인코딩 — 엔진 기본 핸들러의 월드 Yaw 복원이 구 내벽에서 엔티티를 눕히는 문제를, **접평면 로컬 Yaw** 인코딩으로 해소(추가 대역폭 0). 극점 약 3.5m 링 특이점은 수용
    - [x] 가시 거리 짝 맞춤 — 트레잇 `ReplicationCullDistance` ↔ `MassCrowdVisualizationTrait.VisibleLODDistance` 일치 강제 (엔진 기본 5,000cm 방치로 "서버엔 보이는데 클라엔 안 보임" 발생, 2026-08-05 수정)
- [ ] **승리 조건 및 세션 관리**
    - 메달(가칭) 4개 수집 시 승리. 게임 시작/종료/결과 처리 흐름.

---

## Phase 6: Polish (최종 폴리싱)

- [ ] **HUD 추가 요소** (미니맵, 점수 등) — 인벤토리는 인게임 메뉴로 완료
- [ ] **VFX(Niagara) 및 사운드 통합**
- [ ] **게임플레이 밸런싱**
