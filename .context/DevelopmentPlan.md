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
- [x] **Octant 이음매·프랍 접지 정비** (2026-09-10)
    - 지각을 **두께 없는 단면**으로 확정(100cm 쉘 제거)하고 안쪽 3면은 **구면 투영 *전에*** 삭제 —
      순서를 뒤집으면 면이 경계 호로 붕괴해 이음매에 두께 0인 판이 남는다(실측 삼각형의 32%).
    - PCG 프랍 접지를 복셀 점군 양자화 → **지각 콜리전 라인트레이스**로 교체. SurfaceCache와 같은
      `bTraceComplex=false`를 쓰므로 프랍과 NPC 높이가 같은 기준을 공유한다.
    - 설계 명세: [TechDesign_WorldGeneration.md](TechDesign_WorldGeneration.md)

---

## Phase 3: Gameplay (전투 및 상호작용)

- [x] **MassEntity 기반 루팅 시스템** (`ULNPLootingProcessor`, `ULNPIdleToLootingProcessor`)
    - 상태 전환(Idle ↔ Looting → Popped), 게이지 누적, 거리 체크 완료.
    - 루팅 존(500cm) 내 **모든 플레이어의 루팅 속도 합산** — 협동 시 가속. 상호작용 반경(250cm + 정면 60°)과 분리.
    - 전원 이탈 시 게이지 감쇠(존 활성 유지, 복귀만으로 재개), 0 도달 시에만 완전 취소.
    - 게이지 완료 → `Popped` → `ALNPLootDice::SpawnPodRewards` 보상 스폰 + 엔티티 파괴.
    - 설계 명세: [TechDesign_LootPod.md](TechDesign_LootPod.md)
- [x] **LootDice 보상 픽업** (`ALNPLootDice`, `ULNPLootDiceRewardTable`)
    - Mass가 아닌 **순수 Actor** — 서버 권위 Chaos 물리 + Iris `ReplicatedMovement`(각속도 포함). Pod Pop 임펄스·고속 회전.
    - 가중 추첨 리워드 테이블(`DA_LootDiceRewardTable`), 획득 시 Server RPC → 인벤토리 편입, 60초 타이머 소멸.
    - 2인 PIE 검증 완료(2026-07-17): 루팅 → Pop → 획득 → 인벤토리 반영. **잔여:** 정지 윗면 일치, 슬립 트래픽 실측, 동시 획득 선착순.
    - 설계 명세: [TechDesign_LootDice.md](TechDesign_LootDice.md)
- [x] **인벤토리 아이템 인스턴스 모델** (`ULNPInventoryItemInstance`, `ULNPInventoryComponent`)
    - 공유 DataAsset 포인터 → **UObject 인스턴스 + `FGuid ItemId` 정체성**으로 전환. 장착본/보관본 오검출 버그 해소.
    - FastArray 델타 복제 + 등록 서브오브젝트(`COND_OwnerOnly`), Iris 2인 PIE 검증 완료.
    - 스탯은 `FLNPGameplayTagStackContainer`(Lyra 포팅). 버프는 서버 권위 카운트다운 + 클라 로컬 표시, 드랍/재획득 잔여시간 라운드트립.
    - **잔여:** 인스턴스별 **랜덤** 스탯 롤링, 스태킹/수량, 정렬·필터. (무기 레벨은 아래 항목에서 해소)
    - 설계 명세: [TechDesign_Inventory.md](TechDesign_Inventory.md)
- [x] **무기 레벨·합성** (`FLNPWeaponLevelRow`, `ULNPWeaponData::LevelTable`, `ULNPInventoryComponent::TryMergeItem`)
    - 레벨별 스텟·계수를 **무기별 DataTable에 절대값으로** 입력한다(공식 아님, 행 이름 = 레벨).
      **마지막 연속 행이 곧 그 무기의 최대 레벨.**
    - 같은 종류·같은 레벨 n개(`LNPSettings.WeaponMergeMaterialCount`, 기본 3) → 다음 레벨 1개.
      **대상 자신이 결과물**이라 "비장착 n개 → 1개"와 "장착본 +1레벨"이 한 경로로 성립한다.
    - ⚠️ 무기 레벨이 어빌리티 스펙 레벨로 흘러 기초 스텟·계수가 둘 다 오르므로 **피해는 제곱으로 증가** —
      `AbilityCoefScale`은 완만하게 둘 것. LootDice 페이로드의 `ItemLevel`로 남이 주워도 레벨이 보존된다.
    - 2P Standalone 검증 완료(2026-08-20): 게스트·장착본 합성, 드랍 후 타 플레이어 획득 시 레벨 보존, 최대 레벨 차단.
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
- [x] **전투 애니메이션 시스템** (`ABP_Lyra`, `ULNPAnimInstance`)
    - Motion Matching 로코모션 + `ALI_WeaponStyles` Linked Anim Layer로 서브 AnimBP 5종
      (`ABP_Sub_Unarmed`/`Pistol`/`Rifle`/`Shotgun`/`LongSword`)을 무기에 따라 런타임 교체.
    - 몽타주 슬롯 2종 — `DefaultSlot`(전신: 검술 공격·피격·대시·사망), `UpperBody`(재장전) + Inertialization.
    - GAS `State.Block.MovementInput` 태그 기반 이동 입력 차단 (`ULNPInputHandlerComponent` 연동).
    - 설계 명세: [TechDesign_CombatAnimation.md](TechDesign_CombatAnimation.md)
- [x] **총기류 Aim 모드**
    - 총기 서브 AnimBP의 Aim Offset(`AimPitch`/`AimYaw`)으로 상하 조준. 적 NPC도 같은 경로를 공유한다.
    - ADS 카메라 전환(`CR_ADS` 리그 프리셋 + `CDE_ThirdPerson` 디렉터 분기), 조준 감도 완화,
      ADS 중 대시·질주 차단 및 이동 속도 저하(`FLNPADSModifier`). 상세: [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §2.6
- [x] **Enemy NPC HP Bar · 락온 마커** (스크린 스페이스 커스텀 Slate, 2026-09-09 전환)
    - 순수 엔티티에는 `UWidgetComponent`를 달 수 없어 **월드 스페이스 위젯 경로를 통째로 대체**했다.
      승격 Actor의 옛 `UWidgetComponent` + `ULNPHpBarWidget` + 빌보드 회전은 삭제 — 적 종류로 표현이 갈리지 않는다.
    - `LNPUI` 모듈의 `SLNPScreenMarkers` 한 클래스가 스타일(`bDrawFill`)로 락온 마커/HP 바를 겸한다.
      `ULNPHudWidget::NativeTick`이 투영·히스테리시스·페이드를, `ULNPEnemyMarkerProcessor`가 후보 수집을 맡는다.
    - 선결 조건이던 **HP 비율 복제**(`FLNPReplicatedAgent::HealthPct` 1바이트)를 함께 해소.
    - 락온 레티클 텍스처 `T_LockOnReticle` 신규. 옛 `WBP_LNPHpBar`·`WBP_LockOnMarker`는 삭제.
    - 남은 것: 월드 지오메트리 뎁스 가림(보류). 설계 명세: [TechDesign_HUD.md](TechDesign_HUD.md) §11
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
- [x] **콤보 시스템**
    - 몽타주 섹션 분기 기반 다단 콤보 구현.
- [x] **Chooser 기반 몽타주 선택 시스템**
    - Chooser 테이블을 활용하여 상황에 맞는 공격 몽타주 자동 선택.
- [x] **산탄 공격**
    - 다수의 발사체를 분산 발사하는 산탄 공격 구현. 배치 공식(`LNPSpread::BuildHexRingDirections`)은 적 엔티티와 공용.
- [x] **탄창·재장전** (2026-09-14)
    - 탄약을 **어트리뷰트 + Cost GE**로 둔다(`MagazineAmmo`/`MagazineSize`) — 별도 복제 변수면 늦게 온 서버 값이
      연사 중 로컬 차감을 덮어써 HUD가 튄다. GAS 예측 키가 선반영·정산을 대신한다.
    - `ULNPAbility_Reload`는 폰 부여(무기 무관). 취소는 경직·무기 교체·대시 3경로.
      ⚠️ 탄을 채우는 곳은 WaitDelay 콜백이 아니라 `EndAbility`다 — 서버는 클라 종료 통지에 먼저 끝날 수 있다.
    - 무기 메시 애니는 `ULNPWeaponVisualSet::WeaponReloadAnim`을 GameplayCue가 단일 노드로 재생.
    - 설계 명세: [TechDesign_Ability.md](TechDesign_Ability.md) §5.5
- [x] **폭발 무기 — 유탄 발사기** (2026-09-09 ~ 09-10)
    - 포물선 발사체 + ADS 궤도 가이드(`ULNPTrajectoryGuideComponent`). 가이드는 실탄과 **같은 함수**로
      궤적을 만든다 — 따로 적분하면 가리키는 곳과 터지는 곳이 갈린다.
    - **지면 착탄·수명 만료도 폭발로 취급**해 종말 판정을 판정 프로세서 하나로 모았다. 그 전에는 발밑·벽에
      쏘면 임팩트 VFX만 뜨고 범위 피해도 넉백도 없었다. 스플래시는 거리 감쇠 + 순수 엔티티 포함.
    - 설계 명세: [TechDesign_HitDetection.md](TechDesign_HitDetection.md) §6
- [x] **적 탐색 질의 시스템** (`ULNPTargetQuerySubsystem`, `ULNPTargetQueryProcessor`, 2026-09-06 ~ 09-09)
    - 물리 경유 탐색 3곳(조준점·근접 보정·락온)이 **순수 엔티티를 통째로 빠뜨리던** 것을 Mass 상시 질의로 교체.
      요청/응답이 아니라 매 프레임 갱신되는 질의라 소비자는 결과만 읽는다.
    - 락온은 대상을 엔티티 핸들로, 마커는 스크린 스페이스로(위 HP Bar 항목과 같은 경로).
    - 공간 분할(등장방형 축소 행 격자)은 이 시스템의 **선행 조건이 아니다** — 질의 셋은 3,000기에서도
      선형 스캔으로 충분하다. 첫 소비자는 적 겹침 분리력이다.
    - 설계 명세: [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md)
- [x] **히트리액션**
    - 피격 시 캐릭터 히트리액션 애니메이션 구현.
- [x] **피격 반응 시스템** (HitStop·넉백·사망 드랍)
    - `ALNPCharacterBase::ApplyKnockback` → Mover `FApplyVelocityEffect`(Instant Effect). 권위에서만 트리거하고
      결과가 SyncState로 복제된다. 피격 넉백과 근접 패링 성공 시 공격자 넉백이 같은 경로를 쓴다.
    - 사망 시 가방(장착 무기 포함)+활성 버프 전량을 LootDice로 Pop (`DropAllItemsOnDeath`, 2026-08-21).
- [x] **경직(Poise) 시스템** (구현·에디터 작업 완료, PIE 1인 + Standalone 2인 검증 — 2026-08-29)
    - `FLNPPoiseFragment`(서버 전용, 비복제) + `ULNPPoiseProcessor`(감쇠·임계) + `FLNPStaggerCommand`(발동).
      T1 = 그로기(게이지 종속 상태), T2 = 다운(고정 시간 + 리셋 + 면역). **임계값은 폰별**이다.
    - **입력 차단은 GA가, 몽타주는 GameplayCue가 소유한다** — 적 ASC가 `Minimal` 복제라 어빌리티 활성화가
      시뮬 프록시에 안 간다. 부수 효과로 기존 패링 스태거의 미복제 결함도 해소.
    - 근접 패링을 경직 파이프라인으로 통합(`LNPPoise::ApplyParryBreak`). 전용 스태거 GA·`Parry.Stagger` 이벤트 제거 —
      고정 시간 GA와 게이지 그로기를 병행하면 GA가 먼저 끝나며 그로기가 조용히 깨진다.
    - 가드 브레이크: 막아낸 공격도 `PoiseGuardMultiplier`만큼 누적, 돌파 시 `Client_ForceReleaseGuard`.
    - **Standalone `-game` 2인 검증 완료:** 발신/호스트/게스트 13건 일치, 밸류 태그 3종(Light·Heavy·Parried) 동일.
      검증용 임시값·ini 임계값 오버라이드는 전부 원복(2026-08-29). 시각화는 `LNP.Debug.DrawPoise 1`.
    - **에디터 작업 전부 완료** — `DA_PlayerEntityConfig` Fragment 추가, GA_Stagger를 무기에서 폰으로 이관,
      Chooser 행 3종 + `GCN_LNP_Character_Stagger`, 어빌리티·적 DA의 경직 수치.
      ⚠️ GA_Stagger가 무기에 달려 있으면 그 무기를 안 든 폰은 "굳는 시늉만 하고 계속 움직인다".
    - ⚠️ **수치 밸런스는 플레이로 조정하기 전이다.** 근접 3~4타에 서로 죽어 플레이어 경직(T1까지 약 4타)이
      거의 발동하지 않는다 — 경직 임계값이 아니라 공격력·HP 쪽 문제다.
    - 부수 발견: `DA_NPC_LongSword`는 참조자 0인 미사용 에셋(정리 대상). 부수 수정: `FLNPEnemyFragment::Defense`
      미시드로 같은 공격이 Low LOD 적에게만 15% 더 아팠다.
    - 설계 명세: [TechDesign_Poise.md](TechDesign_Poise.md), [GameDesign_Poise.md](GameDesign_Poise.md)
- [x] **Guard / Parry 시스템** (핵심 기능 완료, GameplayCue 에셋 연결 잔여)
    - Guard: `FLNPParryStateFragment` Fragment 각도 판정 → `FLNPGuardBlockCommand` → 데미지 차단 + GameplayCue. 동작 확인.
    - Parry(근접): Guard 입력 직후 0.15초 창 → `FLNPMeleeParryCommand` → 방어자 GA_ParrySuccess + 공격자 GA_Stagger.
    - Parry(투사체): Processor에서 Fragment Velocity/InstigatorTeam/Instigator 반전 → `FLNPProjectileParryCommand` (방어자 GA_ParrySuccess만). 동작 확인.
    - 판정 구조: `FLNPParryStateFragment` Mirror Fragment 기반으로 Processor(Worker Thread)에서 직접 판정. `FLNPApplyDamageGECommand`는 GE 적용 + HitReact + HitStop 처리.
    - 판정 반경 분리: `HitRadius`(피격)와 `ParryRadius`(패링)를 독립 필드로 분리. 2단계 판정 — ParryRadius 먼저 체크, 미발동 시 HitRadius 체크. 동작 확인.
    - Guard 이동 제한: `FLNPGuardModifier` (GuardWalkSpeed 200 cm/s) — Sprint와 동일한 Mover Modifier 패턴.
    - Guard 자세 애니메이션: `ABP_Sub_LongSword`에서 `bIsGuarding` Bool Blend 노드로 Guard 자세 블렌딩. 동작 확인.
    - GameplayCue 에셋 연결 완료(`GCN_LNP_Guard_Block`·`GCN_LNP_Parry_Success`, VFX 지정됨).
    - **잔여: Guided 반사** — 유도 투사체 타입 자체가 미구현이라 현재 반사는 전부 Linear다. 방어자 HitStop도 미구현.
    - 설계 명세: [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md)

---

## Phase 4: AI & Scale (대규모 AI)

- [x] **MassEntity 기반 적 NPC 스폰** (`ULNPMassSpawnSubsystem`)
    - 데이터 에셋(`DA_MassSpawnConfig`) 기반 배치. 스폰 완료 시 `OnSpawningComplete` 델리게이트 발행.
- [x] **적 이동 프로세서** (`ULNPEnemyMovementProcessor`)
    - 구형 표면을 따른 이동. `ULNPSurfaceCacheSubsystem`에서 표면 노멀 조회 (스레드 안전).
- [x] **슬롯 기반 타겟팅 서브시스템** (`ULNPTargetingSubsystem`)
    - `ELNPTargetSlotPool` 3분할(Melee 10 / Ranged 20 / **Promoted** 2) — 승격 개체에 독립 예산을 준다.
      풀 판별 원본은 `ULNPEnemyConfig::GetSlotPool()`, 점수는 거리 하나(`1,000,000 / (거리+1)`).
- [x] **StateTree 기반 적 AI** (`ULNPEnemyStateTreeProcessors`)
    - `Combat`(└ Attack / Chase) / `Alert` / `Idle` 3계층. 전이는 전부 조건 불일치 → Transition to Root.
- [x] **Enemy Low LOD(순수 엔티티) 전투** — 트랙 A·B·C 완료 (2026-09-05 ~ 09-13)
    - `ULNPEnemyConfig::CombatMode`(`ActorPromoted` / `PureEntity`)로 승격을 **옵션화**. 기본값은 종전 거동.
      승격 차단은 EntityConfig의 `LODRepresentation`이 맡고, 코드는 전투 시 LOD를 끌어올리지 않는 것까지만 한다 —
      LOD를 눌러 막으면 유의도·틱 레이트까지 함께 눌린다. 어긋남은 `ULNPEnemyTrait::ValidateTemplate`이 양방향 경고.
    - `ULNPEntityAttackProcessor`가 공격 위상(Windup/Active/Recovery)을 직접 구동한다. **판단은 StateTree Task가,
      진행은 프로세서가** — 신호 구동인 Task Tick에 위상을 두면 신호가 끊긴 프레임에 스윙이 중간에 멈춘다.
      경직·다운 시 공격 중단도 여기가 유일한 경로다(Actor가 없으면 `FLNPStaggerCommand`가 도달하지 못한다).
    - 근접은 **절차적 가상 칼날** — 4점을 계산해 기존 Swept Volume 판정을 그대로 태운다. 판정·패링·가드 코드는
      한 줄도 바뀌지 않았다. 칼날은 별도 엔티티라 2패스(계산→반영), 마커는 Tag가 아니라 Fragment.
    - 원거리는 산탄까지 지원하며 배치 공식을 `LNPSpread::BuildHexRingDirections`로 어빌리티와 공용화.
    - 부수 수정: **적 판정 캡슐 96cm 이중 보정 제거** — 전투 중 적이 예외 없이 승격되던 동안에는 그 분기가
      거의 안 돌아 드러나지 않았다. 판별 원본도 `EnemyTypeTag` 문자열 비교 → `ELNPEnemyAttackType` 필드로 교체.
    - **트랙 B** — 행동 상태를 1바이트(`ELNPEnemyAction` 6값, 3비트)로 복제. 일회성 전이만 갱신 주기 게이트를
      우회한다. 발사체 관전 가시성도 함께 개통.
    - **트랙 C** — ISM↔ISKM 인스턴싱 애니메이션(Idle/Move → 공격·경직·사망·패링 모션 → 무기 스킨드 메시).
      ⚠️ 렌더 요건은 Nanite가 아니고, 애니메이션 인덱스는 추가 순서에 의존한다(→ 설계 명세 §6).
    - **검증용 임시값 2건은 원복 완료**(2026-09-16) — `DT_*_Levels` 4종, `DA_NPC_Pistol`.
    - 설계 명세: [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md)
- [ ] **루팅 방해 어그로** — 루팅 중인 플레이어를 최우선 타겟으로 끌어당긴다.
    점수 가산항으로 붙일지 별도 어그로 규칙으로 둘지 미정. ⚠️ 가산치 튜닝이 곧 난이도다.
    (거리 단항 점수는 **의도된 설계**다 — 추격 자격이 이진값이라 순위를 더 가를 이유가 없다.
    → [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §4.2)
- [x] **적 인지·추격 사다리 재설계** (2026-09-01)
    - 상태를 `None → Alert → Confirmed` 사다리로 정리하고 **경계 인내**(대치를 시간으로 끝낸다)와
      **피격 반응**(맞은 쪽을 돌아본다)을 넣었다. 추격 자격이 플레이어의 Pod 거리만 읽으므로
      `Alert`에서 무엇을 하든 상태 판정에 되먹임이 없다.
    - 설계 명세: [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §5
- [x] **적 겹침 분리력 + 공간 격자** (`ULNPEnemySpatialGridProcessor`, `ULNPEnemySeparationProcessor`, 2026-09-07)
    - 엔진 `MassAvoidance`·내비 장애물 그리드는 **Z-Up 평면 전용**이라 구형 월드에서 못 쓴다
      (→ [DiscardedApproaches.md](DiscardedApproaches.md) Case 05). 같은 계산을 접평면에서 자체 수행.
    - 분리력은 Fragment에 남기고 소비는 `ULNPEnemyMovementProcessor`가 한다 — **Transform의 주인은 하나로 유지**
      (표면 스냅·경사 체크가 그쪽에 있다). 예측 회피(CPA)는 넣지 않았다.
- [x] **순수 엔티티 피격 반응** (2026-09-07)
    - 적이 *피격자*인 방향을 개통 — 연출·넉백·플린치·공격 잠금·사망 팝. Actor가 없으면
      `FLNPStaggerCommand`가 도달하지 못하므로 `ULNPEntityAttackProcessor`가 유일한 중단 경로다.
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
    - **게스트 이동 끊김 해소 (08-28)**: `FLNPReplicatedMovementFragment` + `ULNPMassSmoothingProcessor`(클라 전용)가
      수신 사이 프레임을 Lerp/Slerp로 메운다. 보간 구간은 직전 두 수신 간격을 0.05~0.5초로 clamp —
      장기 정지 후 재개 시 간격이 수십 초로 잡혀 기어갔다. AI 이동 속도는 InputCmd로 전달해
      클라 재시뮬레이션이 CDO MaxSpeed(800)로 폴백하던 것을 막았다.
    - **잔여**: LOD 전환 시 튐은 잔존하나 사용자 판정으로 **수용(보류)**.

---

## Phase 5: Loop (멀티플레이어 완성)

- [ ] **Iris 기반 네트워킹 구현**
    - 설계 명세: [TechDesign_Networking.md](TechDesign_Networking.md)
    - [x] Phase 1: Iris 활성화, GAS 복제 모드, ALNPEnemyCharacter bReplicates, GameplayCue 에셋 연결 (PIE 2P 검증 완료)
    - [x] Phase 2: HitStop/HitReact GameplayCue 전파. 에디터 잔여는 `GCN_LNP_Melee_Impact` 하나 — VFX·Sound·CameraShake 전부 미지정
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
    - [x] Mass 복제 대역폭 3단 정비 (2026-09-03 ~ 09-04) — ① 컬 거리 무력화 해소(대기 1.1MB/s → 82KB/s),
      ② 갱신 주기 게이트(엔티티당 2,762 → 825 B/s), ③ 페이로드 양자화(갱신 1회당 62 → 12.6 B).
      전투 평균 26.3 KB/s·피크 50.6 KB/s(상한의 34%, 포화 샘플 0건). ⚠️ int16 양자화가 **월드 반지름을 하드 캡**한다
      → [Guide_NetBandwidth.md](Guide_NetBandwidth.md)
    - [x] 가시 거리 짝 맞춤 — 트레잇 `ReplicationCullDistance` ↔ `MassCrowdVisualizationTrait.VisibleLODDistance` 일치 강제 (엔진 기본 5,000cm 방치로 "서버엔 보이는데 클라엔 안 보임" 발생, 2026-08-05 수정)
- [ ] **승리 조건 및 세션 관리**
    - 메달(가칭) 4개 수집 시 승리. 게임 시작/종료/결과 처리 흐름.

---

## Phase 6: Polish (최종 폴리싱)

- [ ] **HUD 추가 요소** (미니맵, 점수 등) — 인벤토리는 인게임 메뉴로 완료
- [ ] **VFX(Niagara) 및 사운드 통합**
- [ ] **게임플레이 밸런싱**
