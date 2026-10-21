# PIE Mover·Mass 경고 분석 — 2026-10-21

## 대응 현황

- **1번 설정 수정 완료:** `BP_LNPEnemy` 내부 `MassAgentComponent.MassAgentCapsuleCollisionSyncTrait_0.syncDirection`을 `BothWays`에서 `ActorToMass`로 변경했다. `bSyncTransform=true`, 대상 `AllNetModes`는 유지했다.
- Unreal MCP로 설정 변경, 경고를 오류로 취급하는 Blueprint 컴파일, 에셋 저장을 수행했다. 컴파일 후 값 재조회와 저장 성공·미저장 변경 없음, 디스크 파일 변경을 확인했다.
- 수정 전 에셋 백업: `Saved/Backups/PIEWarningFix-Step1/BP_LNPEnemy-before-capsule-sync-20261021-163726-118.uasset`.
- **플레이 검증:** 사용자가 2인 PIE 게스트로 남반구 ISM↔Actor 경계를 반복 왕복해, §10 수정 전의 툭툭 이동 현상이 완전히 사라졌다고 확인했다. 전환 누적 이동 재현 케이스는 통과다. 최종 Puppet의 역방향 태그 제거, 공중/넉백 중 승격·다른 위도는 별도 미검증이다. 일반 Async 위치 경고나 CMC Error의 해소를 의미하지 않는다.
- **2번 PIE 검증 통과:** 플레이어 `DA_PlayerEntityConfig`와 적 BP 내부 AgentConfig에서 CMC 전용 Movement Sync·Orientation Sync를 함께 제거했다. 새 2인 PIE에서 CMC 추출 Error·Mass Error·전체 Error·템플릿 의존성 실패 메시지가 모두 0건이다. 사용자가 게스트의 장거리 이동·이동 장치·NPC 전환과 배회 모션의 자연스러운 연결을 확인했다. 자세한 변경과 근거는 §11을 참조한다.
- **3번은 재발 관찰:** 위 새 세션에 원점 시작 상태·Up=0 경고가 모두 0건이다. 이전 간헐적 초기화 문제의 근본 해결을 입증한 것은 아니므로 해당 이동 경로에 추가 수정은 하지 않았다.
- **4번 PIE 로그 검증 통과:** 초기화 전 예측 호출을 미루고 메시 자세의 정지 궤적을 공급했다(§12). 사용자의 NPC를 만날 때까지의 짧은 PIE에서 궤적 경고가 23→0건이고 CMC·Mass·템플릿 오류와 Up=0도 모두 0건이다. 이번 보고에는 첫 모션의 시각적 평가가 따로 포함되지는 않았다.
- **5번 PIE 로그 검증 통과:** 플레이어·적 Blueprint의 `bWarnOnExternalMovement=false` 적용 후 새 2인 PIE에서 위치 검사 경고·궤적 경고·Up=0·CMC/Mass/전체 Error·템플릿 의존성 오류가 모두 0건이다. 외부 이동 수용은 계속 false다. 남은 Warning은 별개 Iris·GameplayCamera 관련 3줄이다. 실제 외부 위치 쓰기 진단도 이 옵션의 영향을 받는 한계와 복구 방법은 §13에 기록한다. 아래 본문과 최초 집계는 수정 전 조사 결과다.
- **소유권 재검토:** §9에서 Entity Transform 직접 쓰기와 실제 적 5종 설정을 대조했다. 현재 구성의 지속 이동에는 `ActorToMass`가 맞다. 이후 전환 누적 이동이 수동 재현되어 §10의 전용 ActorManagement·Mover 자세 전달 수정을 적용했고, 동일 경계 왕복 재검증에서 해당 증상 소멸을 확인했다.

## 조사 범위와 결론

`Saved/Logs/LootNPop*.log` 12개, 현재 프로젝트 C++ 코드, 로컬 UE 5.8 엔진 소스, 실행 중인 에디터의 Blueprint CDO·EntityConfig를 읽었다. 코드·에셋·설정을 수정하거나 새 PIE를 실행하지 않았다. 아래 숫자는 조사 시점 스냅샷이며, 열려 있는 `LootNPop.log`에는 이후 에디터 작업 로그가 더 붙을 수 있다. 시간은 로그 원문 UTC이며 한국 시각은 +9시간이다.

- **대량 반복의 대부분:** Async 이동 모드의 시뮬레이션 상태와 실제 캡슐 갱신 시점이 달라 `CheckForExternalChanges`가 경고한다. 이 메시지 자체는 네트워크 위치 보정이나 크래시를 뜻하지 않는다.
- **Mass 메시지의 정체:** Warning이 아니라 `LogMass: Error: Trying to extract CharacterMovementComponent ... failed`다. Mover Pawn에 CMC 전용 Agent Movement Sync Trait이 연결돼 있다.
- **실제 수정 가치가 높은 추가 발견:** `BP_LNPEnemy` 내부 AgentConfig의 캡슐 Sync가 `BothWays`다. 별도 Enemy EntityConfig의 `ActorToMass`만 확인해서는 이 역방향 동기화를 배제할 수 없다.
- **별도 초기화 문제:** 일부 적의 클라이언트 프록시는 잠시 원점 SyncState로 시뮬레이션하며 0 벡터 Up 경고를 낸다. 수만 건의 일반 위치 경고와 구분해야 한다.
- **권장 순서:** 적 내부 Config의 쓰기 권한 정리 → CMC Trait 정리 → 프록시 초기 상태 점검 → 애니메이션 예측 준비 조건 → 남은 정상 Async 위치 경고 억제.

## 1. 집계

| 파일 | 위치 불일치 | 초기화 전 궤적 | Up=0 | Mass CMC Error |
|---|---:|---:|---:|---:|
| LootNPop-backup-2026.10.20-05.12.40.log | 20,242 | 18 | 0 | 18 |
| LootNPop-backup-2026.10.20-05.56.13.log | 3,666 | 7 | 0 | 7 |
| LootNPop-backup-2026.10.20-07.09.21.log | 21,116 | 13 | 4 | 13 |
| LootNPop-backup-2026.10.20-09.34.57.log | 34,458 | 16 | 0 | 16 |
| LootNPop-backup-2026.10.20-10.36.51.log | 5,915 | 16 | 0 | 16 |
| LootNPop-backup-2026.10.20-11.00.54.log | 24,442 | 20 | 0 | 20 |
| LootNPop.log | 61,170 | 62 | 44 | 59 |
| **합계** | **171,009** | **152** | **48** | **149** |

나머지 5개 로그에는 위 메시지가 없다. 시작·에디터 작업 위주의 파일이므로 이것을 증상 해결의 증거로 보지는 않았다. 한 로그에 PIE가 여러 번 들어 있으므로 표는 플레이 1회당 수치가 아니다.

현재 로그의 위치 불일치 메시지만 약 21.0 MB이며, 집계 시점 파일 약 22.2 MB의 약 95%다. 정확한 프레임 시간 비용은 측정하지 않았다. 문자열 포맷·로그 출력·Output Log 처리 비용과 다른 진단을 가리는 문제가 확실히 존재한다.

현재 로그의 위치 경고는 플레이어 Authority 17,676건, 적 Authority 16,462건, 적 SimulatedProxy 14,868건, 플레이어 AutonomousProxy 10,331건, 플레이어 SimulatedProxy 1,833건이다. 호스트나 로컬 플레이어에만 한정된 문제가 아니다.

원시 집계: `Saved/Diagnostics/PIEWarningCounts-20261021.json`.

## 2. Mover: Simulation start location ... disagrees ...

### 발생 조건

엔진 `Mover/Private/KinematicActorSimulation.cpp:259`의 `CheckForExternalChanges`는 매 시뮬레이션 스텝에서 다음 두 위치를 비교한다.

1. 이번 스텝 `FMoverDefaultSyncState`의 위치.
2. 실제 `UpdatedComponent`의 위치.

거의 동일하지 않으면 경고한다. 이동 기반이 존재하면 이 검사를 건너뛰며, Async 이동 모드나 정상 서브스텝인지 구분하는 조건은 없다. 위치 비교는 서버·클라이언트 상태를 비교하는 것이 아니다.

현재 두 Pawn의 CDO에서 확인한 값:

- Backend: `UMoverNetworkPredictionLiaisonComponent`.
- `bWarnOnExternalMovement=true`.
- `bAcceptExternalMovement=false`.
- 적 시작 모드: `AsyncFalling`; 플레이어도 프로젝트의 Async 이동 모드를 사용한다.

`AsyncWalkingMode.h:19`와 `AsyncFallingMode.h:17`은 SceneComponent를 직접 수정하지 않고 시뮬레이션한다고 명시한다. 시뮬레이션은 매 스텝 새 SyncState를 쓰지만, 실제 캡슐은 `MoverComponent.cpp:357`의 `FinalizeFrame` 등 상태 적용 시점에 따라온다. 따라서 한 렌더 프레임에 고정 틱이 여러 번 돌거나 재시뮬레이션할 때 두 값은 정상적으로 달라질 수 있다.

예를 들어 현재 로그 3869~3872행에서는 같은 화면 프레임 `[210]`에서 실제 위치가 고정된 동안 시뮬레이션 위치만 연속으로 진행한다. 이는 위 구조와 일치한다. **Async 이동 모드를 사용한다는 것과 백엔드가 물리 워커 스레드에서 실행된다는 것은 별개**다. 실제 CDO 백엔드는 NetworkPrediction이며, ChaosMover라고 단정하면 안 된다.

### 위험도

- 경고 분기 자체는 상태를 바꾸지 않는다. 현재처럼 `bAcceptExternalMovement=false`라면 위치 차이로 SyncState를 덮어쓰지 않는다.
- 따라서 이 메시지가 6만 번 있다는 것만으로 6만 번의 리컨사일·네트워크 불일치·이동 실패가 있었다고 해석하면 안 된다.
- 반대로 모든 경고가 무해하다고 결론낼 수도 없다. 실제 외부 Transform 변경과 원점 초기 상태도 같은 메시지를 사용한다. 특히 적 내부 `BothWays` 설정은 별도 원인 후보다(§4).
- 기존 `.context/TechDesign_CharacterMovement.md` §7.7~7.9에 설명된 서브스텝 캐시·점프/넉백 위치·중력 보정은 현재 코드에도 들어 있다. 이 경고의 존재만으로 이미 수정한 점프 버그가 재발했다고 판단할 근거는 없다.

### 수정 방향

적의 외부 쓰기 경로와 초기화 문제를 먼저 정리한 뒤, 정상 Async 모드에서 `bWarnOnExternalMovement=false`로 설정하는 것은 합리적이다. 이 옵션은 경고 출력만 바꾸며 이동 알고리즘을 바꾸지 않는다. 다만 실제 외부 이동 진단도 함께 잃으므로, 문제 재현 때 켜거나 추후 진단을 Finalize된 상태와 비교하는 방식으로 한정할 수 있다.

**`bAcceptExternalMovement=true`로 경고를 해결하려 해서는 안 된다.** 이것은 단순한 로그 옵션이 아니다. 검사 함수가 실제 캡슐 위치를 시작 SyncState에 넣으므로, 정상적으로 앞서 간 서브스텝 상태를 낡은 화면 프레임 위치로 되돌릴 수 있다. `LogMover` 카테고리 전체의 Verbosity를 Error로 내리는 것도 Up=0·초기화 문제를 가리므로 권장하지 않는다.

## 3. Mass: Trying to extract CharacterMovementComponent ... failed

### 확정 원인

발생 지점은 엔진 `MassActors/Private/MassAgentTraits.cpp:37`의 `FMassAgentTraitsHelper::AsComponent<T>`다. `UMassAgentMovementSyncTrait::BuildTemplate`이 등록한 Object Fragment Initializer가 `UCharacterMovementComponent`를 찾는다.

프로젝트 `ALNPCharacterBase`는 `ACharacter`가 아닌 `APawn`을 상속하고 `ULNPCharacterMoverComponent`를 생성한다. 둘은 이름만 비슷할 뿐 서로 대체 가능한 컴포넌트가 아니다. 재시도나 BeginPlay 지연으로 CMC가 생기지 않는다.

에디터에서 읽은 실제 연결:

| 위치 | Movement Sync | Orientation Sync | Capsule Sync |
|---|---|---|---|
| `/Game/Pawns/DA_PlayerEntityConfig` | ActorToMass | ActorToMass | ActorToMass, bSyncTransform=true |
| `BP_LNPPlayer` 내부 AgentConfig | 위 에셋을 Parent로 사용 | 동일 | 동일 |
| `BP_LNPEnemy` 내부 AgentConfig | BothWays | BothWays | BothWays, bSyncTransform=true |
| `/Game/Enemy/DA_EnemyEntityConfig_ActorPromoted_Melee01` | 없음 | 없음 | ActorToMass, bSyncTransform=true |

현재 로그의 59건은 플레이어 7건, 적 52건이다. 엔티티/Actor 연결·초기화 때 실행되는 Initializer의 실패이며, 프레임마다 같은 실패를 출력하는 프로세서 구조는 아니다. 적 생성·Puppet 연결이 많으면 반복해서 보인다.

### 실제 영향

CMC Wrapper가 채워지지 않아 해당 Movement·Orientation Translator가 일을 하지 못한다. 엔진 Translator는 포인터가 유효할 때만 실행하므로 이 실패 자체에서 바로 null 역참조로 죽는 구조는 아니다.

플레이가 되는 이유도 설명된다. 캡슐 Translator는 별도 경로이고, 프로젝트 적은 `ULNPEnemyActorSyncProcessor` 및 `ALNPEnemyCharacter::SyncToEntity`로 프로젝트 전용 `FLNPEnemyVelocityFragment` 등을 동기화한다. 하지만 CMC Trait이 제공한다고 가정한 `FMassVelocityFragment` 동기화는 실패하므로, 이를 읽는 엔진 기능을 추가하면 무음 오류가 될 수 있다. 프로젝트 소스의 활성 C++ 코드에는 이 프래그먼트의 직접 사용이 발견되지 않았지만, 전체 엔진·모든 에셋 소비자가 없다는 뜻은 아니다.

**위험도: 즉시 크래시보다는 중간 수준의 구성 오류.** 실패한 기능을 켜 둔 상태이고 로그의 Error 표기도 실제 구성 불일치를 나타낸다.

### 수정 방향

`DA_PlayerEntityConfig`와 `BP_LNPEnemy` 내부 AgentConfig에서 CMC 전용 Movement Sync·Orientation Sync를 함께 제거하는 방향이 가장 단순하다. Orientation Sync는 CMC Wrapper를 요구하므로 Movement Sync만 지우면 Trait 의존성을 깨뜨릴 수 있다.

캡슐 Sync는 중심 기준 위치와 회전을 함께 복사하므로 Mover Pawn의 Transform 동기화에 사용할 수 있다. 속도 소비자가 실제 필요하면 Mover SyncState를 읽는 단방향 동기화를 그 소비자에 맞춰 추가한다. 프래그먼트 존재에 의존하는 엔진 Trait·쿼리는 제거 전 확인한다.

CMC를 더미로 추가하거나 Actor를 ACharacter로 바꾸는 것은 해결 범위가 과하다. 이동 소유권을 중복시키고 잘못된 데이터 경로를 계속 유지하게 된다. Feet Location Sync로 대체하는 것도 이 프로젝트의 캡슐 중심·구면 중력 규약과 맞지 않는다.

## 4. 추가 발견: BP_LNPEnemy 내부 Capsule Sync의 BothWays

이 항목은 Mass CMC Error와 별도다. 캡슐 컴포넌트는 실제 존재하므로 역방향 Translator가 유효하게 동작할 수 있다.

엔진 경로:

1. `MassAgentTraits.cpp:78` — BothWays이면 `UMassTransformToActorCapsuleTranslator`도 추가.
2. `MassEntityTemplateRegistry.h:251` — `AddTranslator`가 Translator의 RequiredTags를 템플릿에 추가.
3. `MassAgentSubsystem.cpp:390` — Puppet 초기화 시 Actor 내부 템플릿의 Composition을 기존 엔티티에 추가.
4. `MassCapsuleComponentTranslators.cpp:80` — 역방향 Translator가 `CapsuleComp->SetWorldTransform(..., bSweep=false)` 실행.

따라서 별도 스폰 EntityConfig가 ActorToMass여도, Pawn 내부 BothWays가 추가한 역방향 태그가 사라지는 것은 아니다. 이는 문서가 정한 **Actor 표현 중 Transform은 Mover 단독 소유** 규칙과 맞지 않는다.

**위험도: 이번 조사에서 우선 확인할 설정 문제.** 실제로 매번 가시적인 위치 되돌림이 발생한다고 로그만으로 확정할 수는 없다. Translator 실행 순서와 그 시점 Transform 값에 따라 동일 위치를 다시 쓸 수도 있다. 그러나 Mover 밖의 유효한 캡슐 쓰기 경로가 설정으로 존재한다는 것은 확인됐다. 일반 위치 경고 전부를 오탐으로 처리하기 전에 제거해야 한다.

권장 수정은 `BP_LNPEnemy.MassAgentComponent` 내부 Capsule Sync도 `ActorToMass`로 맞추는 것이다. 별도 EntityConfig만 고치면 불충분하다. 수정 후 PIE를 새로 시작해 템플릿 캐시를 새로 만들고, High LOD 승격·강등·풀 재사용·게스트 표현을 검증한다. 역방향 태그 `FMassCapsuleTransformCopyToActorTag`가 최종 Puppet 엔티티에 남지 않는지도 확인한다.

## 5. Mover: Cannot predict trajectory ... not initialized

현재 로그 62건 중 59건은 `Initialized? 0 BegunPlay? 0 BeingDestroyed? 0`인 초기화 구간이다. 나머지 3건은 C_12에서 `Initialized? 1 BegunPlay? 1`인데도 발생했다(40163·40183·40198행). 이 3건은 §6의 원점 상태·Up=0 구간과 겹친다. 따라서 메시지의 “not initialized”라는 문구를 그대로 초기화 플래그의 상태라고 해석하면 안 된다.

`ULNPAnimInstance::UpdateLocomotionValues` (`LNPAnimInstance.cpp:435`)가 `UMoverTrajectoryPredictor`를 호출하고, 엔진이 Simulation·RollbackBlackboard·유효 이동 모드·기본 SyncState 중 필요한 조건을 갖추지 못하면 같은 경고와 기본 샘플 반환 경로에 도달한다 (`MoverComponent.cpp:2344`, `:2400`, `:2498`). BeginPlay 이후 3건에서 이 중 정확히 어느 조건이 실패했는지는 추가 계측이 필요하다.

애니메이션 업데이트는 BeginPlay 이전에도 일어날 수 있다. 현재 코드는 Mover 포인터가 있다는 조건과 예측할 준비가 됐다는 조건을 구분하지 않는다. 로그에 찍는 초기화 플래그만으로 함수의 모든 실패 조건을 대표할 수도 없다.

**위험도: 대부분 낮음, 원점 상태와 겹친 3건은 §6과 함께 조사.** 보통 첫 애니메이션 평가의 궤적 품질 문제다. 기본 샘플 때문에 첫 프레임 Motion Matching이 좋지 않은 포즈를 고르거나 순간적인 속도 판정이 흔들릴 수 있지만, 이 메시지가 실제 Pawn 이동 시뮬레이션을 중단시키는 것은 아니다. 장시간 반복되거나 BegunPlay 이후에도 계속되면 별도 장애로 봐야 한다.

수정은 궤적 생성 호출만 Mover 초기화·BeginPlay 및 사용 중인 Simulation의 예측 준비 조건 뒤로 미루는 방향이다. 준비 전에는 현재 메시 Transform의 정지 궤적을 공급하거나 유효한 이전 궤적을 유지하고, 첫 유효 프레임에서 히스토리를 초기화한다. 재사용 시 이전 생명의 궤적을 섞지 않아야 한다. AnimInstance 전체 업데이트를 막으면 무기·몽타주 등 다른 초기화까지 영향을 받을 수 있다.

## 6. Mover: Ignoring the provided UpDirection ... zero vector

현재 로그 44건은 적 세 프록시에 집중된다.

| 적 | 횟수 | 첫~마지막 로그 UTC | 관측 폭 |
|---|---:|---|---:|
| BP_LNPEnemy_C_12 | 16 | 06:18:13.662~13.790 | 128 ms |
| BP_LNPEnemy_C_16 | 18 | 06:19:52.887~53.000 | 113 ms |
| BP_LNPEnemy_C_17 | 10 | 06:20:06.105~06.109 | 4 ms |

이는 실제 오류 지속 시간 전체가 아니라, 첫·마지막 해당 로그 사이의 폭이다. 현재 로그에는 시뮬레이션 시작 위치 `V(0)`과 실제 컴포넌트 위치가 다른 경고도 27건 있다. 예를 들어 40146~40150행에서 C_12의 SimulatedProxy 시작 상태는 원점인데 실제 캡슐은 `(22241,-14241,5135)`다. 두 위치 차이는 약 269 m다.

확인한 코드 경로:

1. `LNPCharacterMoverComponent.cpp:451` — `OnPreSimulate`가 시작 SyncState 위치로 `ApplyGravityAt` 호출.
2. `LNPPawnGravityComponent.cpp:97` — RadialOutward에서 `(Location-GravityOrigin).GetSafeNormal()` 계산.
3. 위치가 중력 중심이면 결과가 0이지만 함수는 true 반환.
4. `UpdatePawnOrientation`이 0 중력과 0 Up을 전달.
5. 엔진 `MoverComponent.cpp:2276`은 Up Override를 거절하고 비활성화한다.

**위험도: 중간, 초기화 조건에 따라 높아질 수 있음.** 정상 반지름 방향을 정할 수 없는 상태에서 시뮬레이션을 진행한다. 경고만 조용히 한다고 위치·중력 상태가 올바르게 되는 것은 아니다. 관측상 짧은 초기화 구간이지만, 초반 스냅·잘못된 바닥 판단·예측 불일치 가능성을 배제할 수 없다. 원점 상태가 왜 NP 히스토리에 들어갔는지까지는 로그만으로 확정하지 못했다.

우선 스폰·복제 초기 상태·Puppet 연결·풀 재활성화 때 유효 Transform을 확보하기 전에 Mover가 진행되는지 확인한다. 근본 수정은 올바른 시작 SyncState와 진행 시점을 보장하는 것이다. 방어적으로 중심에서는 방향 계산 실패를 반환해 0 중력·0 Up을 함께 덮어쓰지 않도록 할 수 있지만, 이것만으로 잘못된 시작 위치를 고친 것은 아니다. 시뮬레이션 중 현재 Actor 위치로 무조건 대체하면 서브스텝·리시뮬레이션의 결정성을 다시 깨뜨릴 수 있다.

## 7. 함께 보인 경고

- **NetworkPrediction:** 전체 18건, 현재 5건의 `RollbackFrame ... EQUAL PendingFrame`이 있다. 엔진 `NetworkPredictionWorldManager.cpp:215`는 아직 실행하지 않은 PendingFrame에 보정이 도착한 경우 해당 분기에서 보정을 적용하지 않고 기다린다. 현재 로그에는 별도 severity를 쓰는 `SystemFault`도 268줄 있어 Warning 검색만으로는 누락된다. 입력 스트림 기아·복구·overflow 기록이며 각각을 268개의 독립 장애나 268프레임 손실로 해석하면 안 된다. 단일 프로세스 다중 PIE의 프레임 지연이나 네트워크 지연 모두 후보이며, 현재 로그만으로 네트워크 구현 결함 또는 Mover 로그 부하와의 인과를 확정할 수 없다.
- **PoseSearch:** 과거 두 로그에 애니메이션 미연결 12,425건, SchemaCardinality 불일치 3건, 인덱스 준비/실패로 검색 생략 75건이 있다. 현재 로그에는 없다. `Database None`의 None은 엔진이 애니메이션 항목의 `GetName()`을 출력한 결과여서 실제 데이터베이스 이름을 알려 주지 못하는 메시지다 (`PoseSearchDerivedData.cpp:1618`). Schema를 바꾸는 동안 낡은 인덱스를 읽거나 미연결 항목을 가진 상태가 원인 후보이며, 검색 생략은 Motion Matching 품질에 직접 영향을 줄 수 있다. 재발하면 해당 PSD의 항목·Schema를 확인하고 인덱스를 재생성한다. 전체 DDC 삭제부터 할 근거는 없다. 최신 로그에 없다는 것만으로 모든 미사용 PSD까지 정리됐다고 확정하지 않는다.

## 8. 수정 단위와 검증 기준

| 순서 | 수정/확인 대상 | 성공 기준 |
|---|---|---|
| 1 | BP_LNPEnemy 내부 Capsule Sync를 ActorToMass로 정렬 | 최종 Puppet에 역방향 캡슐 태그가 없고 승격·강등·풀 재사용·게스트에서 위치 되돌림 없음 |
| 2 | 플레이어 에셋·적 내부 Config의 CMC Movement/Orientation Trait 정리 | CMC 추출 Error 0, 템플릿 의존성 실패 0, 회전·위치·필요 속도 데이터 유지 |
| 3 | 적 프록시의 초기 원점 SyncState 경로 | 신규 적·복제 합류·풀 재활성화에서 잘못된 원점 시뮬레이션과 Up=0 경고 0 |
| 4 | AnimInstance의 궤적 예측 준비 조건 | 초기화 전 예측 경고 0, 첫 포즈·무기 애니메이션 정상 |
| 5 | 정상 Async 위치 검사 경고 억제 | 대량 경고 제거, 다른 LogMover Warning은 관측 가능, 실제 이동·보정 양상 악화 없음 |

검증은 자동화 장치 신규 구현보다 사용자의 수동 PIE가 적절하다. 최소 Standalone과 2인 Listen Server에서 스폰·달리기·대시 점프·피격·사망/리스폰·적 LOD 왕복을 확인한다. 고정 틱이 프레임당 여러 번 돌도록 `t.MaxFPS 40` 조건도 포함한다. 재현이 필요한 프록시 초기화는 중간 합류/새 적 스폰을 포함한다.

`np.PrintReconciles 1`, `Mover.LocalPlayer.ShowCorrections 1`은 실제 보정 여부를 별도로 보는 데 사용한다. 로그 부하가 성능에 주는 영향은 동일 조건에서 경고 출력 전후 프레임 시간으로 비교해야 하며, 이번 조사에서는 성능 수치를 측정하거나 재현 플레이를 수행하지 않았다.

## 9. ActorToMass 선택 재검토 — Entity 직접 이동과의 양립

### 판단

**Entity Transform 직접 쓰기는 실제로 존재한다. 현재 C++ 경로와 적 5종 설정을 대조한 결과, 지속적인 Actor 이동에는 `ActorToMass`를 유지하는 것이 맞다.** 핵심은 Entity의 쓰기 가능 여부가 아니라, 같은 개체가 Actor로 움직이는 동안 Entity가 별도 이동 원본이 되는지다.

`BothWays`는 두 상태 중 최신 값을 골라 주는 충돌 해결 정책이 아니다. 두 Translator를 서로 다른 그룹에서 실행해 Actor→Entity와 Entity→캡슐 복사를 모두 수행한다. Mover의 시뮬레이션 SyncState를 Entity 위치로 갱신하는 기능도 아니다. 같은 Transform을 쓰는 원본을 단계별로 정하고, 소유권 전환은 별도로 처리해야 한다.

### 직접 쓰기 경로별 확인

| 경로 | Entity Transform 쓰기 조건 | Actor와의 관계 |
|---|---|---|
| 지상 이동·회전 | `LNPEnemyProcessors.cpp:868`의 Actor 유무 분기에서 Actor가 없는 else만 위치·회전을 적분(`:890`, `:930`) | Actor가 있으면 `SetAIMoveInput`·`SetAIOrientationIntent`·`SetAIDesiredSpeed`만 전달하고 Mover가 이동 |
| 지상 동적 발판 운반 | 같은 파일 `:696`, Actor가 없을 때만 `ApplyDynamicSupportDelta` | Actor 이동을 Entity에서 따로 계산하지 않음 |
| 공중 넉백·사망 팝 | Actor 없는 경로만 공중 적분. Actor 시체는 `:746`에서 제외 | Actor 피격은 `LNPHitDetectionShared.h:705`의 `ApplyKnockback` → Mover 효과. Actor 시체는 랙돌 경로 |
| 구간 배회 | `LNPEnemyWanderProcessor.cpp:127`의 `bSafe`에 `Actors[I].Get()==nullptr`, `:134`에 PureEntity 조건 | Actor가 있는 동안 배회 위치 쓰기 실행 안 함 |
| 비행·벽보행 | 도메인 태그에 맞는 프로세서가 Entity 위치를 직접 적분 | 프로세서 자체의 Actor 배제 조건에 기대는 구조는 아니다. 실제 설정이 PureEntity·Actor 미생성 매핑인지 확인해야 함(아래 표) |
| 클라이언트 보간·구간 배회 재생 | `LNPMassReplication.cpp:318`에서 현재 표현이 High/LowResSpawnedActor면 continue | Actor 표현 중에는 Mass 보간이 Transform을 쓰지 않음. 인스턴스 표현이면 Entity 복제 보간이 위치 원본 |
| 서버 신규 스폰 | `LNPMassSpawnSubsystem.cpp:771`의 `SetupSpawnedEntities` | 새 엔티티의 초기 위치·회전 구성. 활성 Actor를 수시로 재배치하는 경로가 아님 |
| 클라이언트 버블 최초 추가 | `LNPMassReplication.cpp:179`에서 초기 Transform 반영 | 최초 엔티티 생성의 위치 시드. 이후 변경 수신은 보간 목표를 갱신하고 Actor 표현 중 적용은 건너뜀 |
| 발사체·근접 판정 Transform | `ANS_LNPMeleeHitWindow`, `LNPEntityAttackProcessor`, `LNPProjectileProcessors` 등 | 적 본체가 아니라 별도로 생성한 투사체·공격 판정 엔티티의 Transform |

위 표는 프로젝트 C++의 직접 쓰기와 호출 조건을 검토한 결과다. 모든 Blueprint 그래프나 실행 중 최종 아키타입을 검증했다는 뜻은 아니다. ActorToMass도 같은 순간의 Mover 상태·캡슐·Entity 위치를 원자적으로 일치시키지는 않으므로, 페이즈 사이의 갱신 지연과 지속적인 소유권 충돌은 구분해야 한다.

### 실제 에셋 설정 확인

에디터에서 각 `DA_EnemyEntityConfig_*`의 LNPEnemyTrait·MassCrowdVisualizationTrait와 연결된 EnemyConfig를 읽었다.

| 적 설정 | CombatMode / 도메인 | High·Medium·Low·Off 표현 | Actor 템플릿 |
|---|---|---|---|
| ActorPromoted_Melee01 | ActorPromoted / GroundSupport | Actor·Actor·StaticMesh·None | BP_LNPEnemy |
| PureEntity_Melee01 | PureEntity / GroundSupport | SkinnedMesh·SkinnedMesh·StaticMesh·None | 없음 |
| PureEntity_Ranged01 | PureEntity / GroundSupport | SkinnedMesh·SkinnedMesh·StaticMesh·None | 없음 |
| PureEntity_Flyer01 | PureEntity / FreeFlight | StaticMesh·StaticMesh·StaticMesh·None | 없음 |
| PureEntity_Crawler01 | PureEntity / SurfaceCrawl | StaticMesh·StaticMesh·StaticMesh·None | 없음 |

따라서 현재 비행·벽보행 Entity 직접 이동은 BP_LNPEnemy의 캡슐 Sync와 충돌하지 않는다. 향후 두 도메인을 ActorPromoted로 바꾸면 이 결론은 성립하지 않는다. `LNPEnemyMassTypes.cpp:181` 이후 검사는 경고만 하므로 잘못된 조합을 강제로 막지는 않는다.

### 소유권 전환은 별도 경로로 유지된다

`ActorToMass`로 바꿔도 Representation의 Actor 스폰·재배치 기능은 제거되지 않는다.

- Entity→Actor 승격: 엔진 RepresentationProcessor가 Entity Transform으로 Actor를 스폰/재배치한다. 프로젝트 `ULNPEnemyActorInitializerProcessor`가 Health·속도를 전달하고, 조건을 만족하면 Mover 텔레포트 효과로 자세를 맞춘다(`LNPEnemyProcessors.cpp:1238`, `:1242`, `:1261`).
- Actor 활성 상태: 입력→Mover→캡슐→Mass 순서로 위치를 전달한다.
- Actor 강등/풀 반환: 실제 사용하는 MassCrowd ActorManagement가 Puppet을 pause하고 AgentSubsystem에서 추가 Composition을 제거하는 수명주기를 거친다. 이후 Actor 없는 이동 경로가 Entity를 갱신한다.

활성 Actor를 게임 로직으로 강제 이동시켜야 한다면 Mover 텔레포트 효과로 시뮬레이션 상태를 바꾸고 ActorToMass로 따라오게 해야 한다. Entity 위치만 먼저 바꾸는 새 코드를 넣으면, 변경이 Actor에 전달되기보다 다음 ActorToMass에서 지워질 수 있다. 이것은 `BothWays`를 다시 켤 이유가 아니라 명시적 재배치 경로가 필요한 조건이다.

### 추가로 발견한 승격 좌표 규약의 불일치

실제 5개 에셋의 ActorManagement 클래스는 기본 `UMassCrowdRepresentationActorManagement`다. 이 클래스의 `GetOrSpawnActor(:73)`·`TeleportActor(:90)`는 Entity 위치를 발밑으로 간주하고 **월드 Z에 캡슐 반높이를 더한다**(`:83`, `:97`). TeleportActor의 추가 sweep도 월드 Z 기준이다. 프로젝트 Entity 위치는 캡슐 중심이고 중력은 구면이므로 이 기본 구현을 그대로 중심 기준이라고 볼 수 없다. 기존 문서/주석에서 엔진 TeleportActor가 중심 기준이라고 설명한 부분은 부정확했다.

현재 프로젝트의 승격 보정은 `Config && !IsFlying && Velocity.IsNearlyZero && Surface.IsValid`일 때, 유효 스냅샷·같은 세대·투영 성공까지 만족해야 Mover 텔레포트를 큐잉한다. 조건을 벗어나는 공중/속도가 있는 승격이나 지원면 없는 재활성화까지 모두 보장하지는 않는다. 기본 raw Actor teleport만으로 Mover SyncState가 함께 갱신되는 것도 아니다.

이는 양방향 Translator가 올바르게 보장해 주던 기능을 제거했다고 확정한 사례는 아니다. 그 Translator도 캡슐만 쓰며 Mover SyncState를 갱신하지 않는다. 다만 기존 역방향 쓰기를 없앴을 때 전환 프레임의 덮어쓰기 순서가 달라질 수 있으므로, 이번 설정 수정의 회귀 검증 대상으로 포함해야 한다.

**최종 권고:** 지속 이동의 설정은 ActorToMass 유지. 1번 대응의 플레이 검증에는 정적 접지 승격 외에 공중/넉백 중 승격·낮은 LOD에서 복귀·구면 적도 부근 풀 재사용을 포함한다. 후속 수정 시 ActorManagement의 중심·중력 좌표 규약과 Mover 상태 전달을 한 경로로 일관되게 맞추는 것이 필요하다. 이번 재검토에서는 추가 게임 코드·에셋 수정이나 새 PIE 실행을 하지 않았다.

## 10. 남반구 ISM→Actor 전환 시 적도 방향 누적 이동 재현과 수정

사용자의 2인 PIE에서 게스트가 전환 경계를 왕복할 때, 정지/되돌림 없이 ISM→Actor마다 남반구 적이 적도 방향으로 캡슐 반높이 정도 이동하는 현상이 재현됐다. 최신 `LootNPop.log`에는 적 Authority의 `실제 캡슐 - 시작 SyncState = (0,0,+96cm)`인 위치 경고가 26건 있었다. 예: 13941행, UTC 08:24:13.458. 이는 기본 MassCrowd의 월드 +Z 반높이 보정이 실제 개입한다는 근거다. 이 숫자는 조사 당시 최신 로그 기준이며 앞 절의 이전 로그 집계와 구분한다.

남반구의 월드 +Z 변위에는 적도 방향의 접선 성분이 있다. 접지 보정이 뒤따르면 지면을 따라 옆으로 이동한 것으로 보일 수 있으므로, 캡슐 반높이만큼 반복된다는 관측과 원인 구조가 맞는다.

작성한 수정:

- `ULNPEnemyRepresentationActorManagement`: MassCrowd를 상속해 메시 활성화·Puppet pause를 유지하되, 스폰과 서버 재배치는 기본 MassRepresentation의 중심 좌표 전달을 사용한다. 월드 Z 반높이 덧셈과 Z 기준 sweep을 제거한다.
- 게스트 프록시에는 Representation의 TeleportActor를 적용하지 않는다. 위치는 Mover 복제가 소유하므로 ISM 보간 Transform으로 이미 복제된 Actor를 재배치하지 않는다.
- 서버 ActorInitializer는 모든 활성화에서 시작 중심·자세를 Mover 텔레포트 효과로 전달한다. 기존 같은 층 접지 투영은 성공했을 때만 목표 위치를 보정하고, 공중·유효 지원면 없음·투영 실패도 원래 Entity 자세를 전달한다.
- `MassCrowd` 모듈 의존성을 추가했다. 적용 대상은 Actor 승격용 EntityConfig의 ActorManagement 클래스다.

검증 및 적용:

- Live Coding 최종 결과 `Success`, UBT `Result: Succeeded` 확인. 새 네이티브 클래스가 에디터의 ActorManagement 하위 클래스 목록에 등록된 것도 확인했다.
- `/Game/Enemy/DA_EnemyEntityConfig_ActorPromoted_Melee01`의 MassCrowdVisualizationTrait에 전용 ActorManagement를 연결하고 저장했다. 재조회에서 이 클래스 필드만 변경됐으며, LOD 표현 매핑과 나머지 Params는 그대로임을 비교했다. 에셋의 미저장 변경은 없다.
- 수정 전 EntityConfig 백업은 `Saved/Backups/PIEWarningFix-Promotion/`에 보관했다.
- 초기 컴파일에서 누락된 Mass 핸들 정의 헤더와 UHT 줄 번호 불일치 오류를 수정한 뒤 최종 성공을 확인했다. UE 5.8의 직접 정의 헤더는 `Mass/EntityHandle.h`다.
- **플레이 재검증 통과(2026-10-21, 사용자 수동 확인):** 같은 남반구 2인 PIE 게스트에서 ISM↔Actor 경계를 반복 왕복했고, 수정 전의 툭툭 이동 현상이 완전히 없어졌다고 확인했다. 이 재현 케이스의 누적 이동은 해결 확인으로 기록한다. 공중/넉백 중 승격·다른 위도와 최종 Puppet 태그는 이번 보고의 검증 범위에 포함하지 않는다. 새 PIE 자동 실행이나 추가 코드·에셋 변경은 하지 않았다.

## 11. CMC 전용 Movement·Orientation Sync Trait 제거

### 의존성 확인

- `UMassAgentMovementSyncTrait`는 CMC Wrapper와 `FMassVelocityFragment`를 추가하고 CMC 추출 Initializer를 등록한다. `UMassAgentOrientationSyncTrait`도 같은 Wrapper를 요구하므로 두 Trait을 함께 제거한다.
- 프로젝트 `Source`·`Config` 검색에서 `FMassVelocityFragment`·CMC Wrapper·해당 Translator 태그를 사용하는 활성 코드가 발견되지 않았다. `LNPEnemyMassTypes.cpp`의 관련 줄은 주석이다. 적은 `FLNPEnemyVelocityFragment`와 자체 Actor 동기화를 사용한다.
- 실제 플레이어 Assorted Fragments는 Actor·ParryState·PositionHistory·Poise와 Player 태그다. CMC Wrapper나 Mass 기본 Velocity는 포함하지 않는다.
- 연결된 MassCrowdMember·MassReplication·Visualization Trait의 엔진 구현과 프로젝트 StateTree Task 외부 데이터 요구를 확인했다. 이 구성에는 CMC Wrapper·Mass 기본 Velocity의 직접 의존성이 없다. 실제 PIE 템플릿 생성 성공은 별도 검증한다.
- 캡슐 Sync의 `bSyncTransform=true`는 위치와 회전을 함께 복사한다. 별도의 Mover 속도 Translator를 새로 만들 필요가 확인되지 않아 추가하지 않았다.

### 적용 결과

| 대상 | 수정 전 | 수정 후 |
|---|---|---|
| `/Game/Pawns/DA_PlayerEntityConfig` | AssortedFragments·OrientationSync·CapsuleCollisionSync·MovementSync·Replication | AssortedFragments·CapsuleCollisionSync·Replication |
| `BP_LNPEnemy` 기본 MassAgentComponent 내부 Config | CapsuleCollisionSync·MovementSync·OrientationSync | CapsuleCollisionSync |

- 두 Config의 Parent는 `None`을 유지했다. 남은 Trait의 순서와 참조를 확인했다. 두 캡슐 Trait 모두 `ActorToMass`, `bSyncTransform=true`, `AllNetModes`다.
- 적 Blueprint를 경고도 오류로 취급하여 컴파일했고 성공했다. 컴파일로 CDO 내부 서브오브젝트 경로가 다시 생성되고 Config GUID도 갱신되어, 원래 경로 문자열의 완전 일치 대신 남은 Trait과 설정을 재조회해 검증했다.
- 두 에셋 저장 성공과 `is_dirty=false`를 확인했다. 이번 단계의 C++ 변경·Live Coding·새 PIE 실행은 없다.
- 수정 직전 저장본 백업: `Saved/Backups/PIEWarningFix-Step2/DA_PlayerEntityConfig-20261021-202426-510.uasset`, `BP_LNPEnemy-20261021-202426-510.uasset`.
- **PIE 검증 통과(2026-10-21):** 사용자가 1P·2P를 켜고 주로 게스트에서 테스트했다. NPC의 ISM↔Actor 전환은 같은 위치에서 끊김 없이 자연스러웠고, 배회 중 전환해도 모션이 자연스럽게 이어졌다. 게스트의 달리기·스프린트·그래플 훅·런처·패러글라이딩으로 월드를 이동하며 어색한 부분을 발견하지 못했다고 확인했다.
- 수정 전 로그 기준점(`Saved/Diagnostics/PIEWarningFix-Step2-Checkpoint.json`, 11,954행)의 마지막 행이 현재 파일과 일치하는지 확인한 뒤 이후 구간만 집계했다. 테스트는 UTC 11:26:58.362~11:34:56.255(한국 20:26:58~20:34:56), 새 21,610행이다. CMC 추출 Error 0건, Mass Error 0건, 전체 Error 0건, 누락 Fragment·Trait/템플릿 의존성 실패 메시지 0건이다. 이전 CMC Error 24건은 앞 구간에 남아 있으므로 전체 파일 누적 건수로 회귀를 판정하지 않는다.
- 같은 구간에 일반 위치 검사 경고는 19,246건 남았고, 초기화 전 궤적 경고는 23건(플레이어 5·적 18)이다. 모두 `Initialized=0, BegunPlay=0, BeingDestroyed=0`이다. 원점 시작 위치·Up=0 경고는 각각 0건이다. 집계는 `Saved/Diagnostics/PIEWarningFix-Step2-Results.json`에 보관한다.

## 12. 초기화 전 궤적 예측 호출 지연

§11의 새 세션에서 남은 궤적 경고 23건은 전부 Mover 초기화·BeginPlay 이전이다. `ULNPAnimInstance::UpdateLocomotionValues`에서 Predictor 포인터가 있다는 이유만으로 엔진 예측을 호출하던 경로를 수정한다.

- 예측 호출 조건에 Mover 초기화·BeginPlay 완료, 파괴 중 아님, 현재 이동 모드·기본 SyncState 존재를 추가한다. 엔진의 비공개 Simulation을 노출하거나 새로 생성하지 않는다.
- 준비 전에는 엔진 `InitTrajectorySamples`로 현재 메시 위치·회전의 정지 궤적을 만든다. 기존과 같은 과거 15·현재 1·미래 15개 샘플을 공급하며 매번 새 궤적을 만들어 이전 사용의 궤적을 섞지 않는다. 준비 후에는 기존 Predictor 경로로 돌아간다.
- AnimInstance 전체 갱신이나 무기·몽타주 로직을 중단하지 않는다. 이동 시뮬레이션·네트워크 설정·일반 위치 경고 옵션도 변경하지 않는다.
- 이 조건은 관측된 초기 호출을 막는 수정이다. BeginPlay 이후의 비공개 Simulation·RollbackBlackboard 이상까지 검증하는 완전한 준비 상태 API는 아니다. 해당 초기화가 정상이라는 엔진 수명주기를 전제로 하며, 과거 `Initialized=1, BegunPlay=1` 경고는 별도 재발 관찰 대상이다.
- 검증: 코드 차이·공백 검사 통과. Unreal MCP Live Coding `Result: Success`, UBT `Result: Succeeded`(29.17초)를 확인했다.
- **PIE 로그 검증 통과(2026-10-21):** 사용자가 NPC를 만날 때까지만 짧게 플레이했다. 기준점 33,584행의 마지막 행 일치를 확인한 뒤 새 5,482행을 집계했다. PIE 시작 준비 UTC 11:45:30경부터 종료 11:46:41.971(한국 20:45:30~20:46:41) 구간을 포함한다. 궤적 경고 0건, CMC·Mass·전체 Error·템플릿 의존성 실패 메시지 각각 0건, Up=0·원점 시작 경고 각각 0건이다. 일반 위치 경고 4,408건은 남았다. 결과는 `Saved/Diagnostics/PIEWarningFix-Step4-Results.json`에 보관한다. 첫 모션이 시각적으로 자연스러웠는지에 대한 별도 보고는 없으므로 로그 검증과 구분한다.

## 13. 일반 외부 위치 검사 경고 출력 조정

§11·12의 두 차례 새 PIE에서 CMC·Mass 오류와 Up=0·원점 시작 경고가 재발하지 않았고, 사용자는 §11에서 장거리 이동·이동 장치·NPC 전환이 자연스럽다고 확인했다. 일반 위치 검사 경고는 계속 대량 반복된다. §2의 고정 시뮬레이션 틱·FinalizeFrame 갱신 시점 차이에 따른 출력 부담을 줄이기 위해 다음 설정을 적용했다. 모든 개별 경고가 무해함을 입증한 것으로 해석하지 않는다.

- 대상: `/Game/Pawns/BP_LNPPlayer`·`/Game/Enemy/BP_LNPEnemy` 기본 MoverComponent.
- `bWarnOnExternalMovement`: true→false. `bAcceptExternalMovement`: false 유지. 엔진 `UKinematicActorSimulation::CheckForExternalChanges`는 두 옵션이 모두 false이면 검사를 건너뛴다. 기존에도 수용 옵션이 false여서 SyncState 덮어쓰기는 하지 않았으므로, 이번 변경은 이동 수용 방식이나 네트워크 시뮬레이션을 바꾸지 않는다.
- `LogMover` 카테고리 전체를 제한하지 않았다. Up=0·예측 실패 등 다른 Mover 경고는 계속 관측할 수 있다.
- **진단 한계:** 같은 검사로 잡히던 실제 외부 위치 쓰기와 원점 시작 위치 차이 메시지도 출력되지 않는다. 향후 위치 이상이나 초기화 문제를 조사할 때는 해당 Blueprint의 Mover 고급 설정 `Warn On External Movement`를 다시 켜고 새 PIE로 재현한다. 원점 문제가 근본 해결됐다고 선언하거나, 경고 0건을 위치 정확성의 증거로 삼지 않는다.
- 두 Blueprint를 경고도 오류로 취급하여 컴파일했고 성공했다. 설정을 다시 읽어 두 값이 모두 false인 것과 적 AgentConfig에 캡슐 Trait만 남은 것을 확인했다. 저장 성공·`is_dirty=false`를 확인했다. C++ 재컴파일은 필요하지 않았다.
- 변경 전 백업: `Saved/Backups/PIEWarningFix-Step5/BP_LNPPlayer-20261021-204814-390.uasset`, `BP_LNPEnemy-20261021-204814-390.uasset`.
- **PIE 로그 최종 검증 통과(2026-10-21):** 사용자가 새 2인 PIE를 짧게 테스트했다. 기준점 39,149행의 마지막 행 일치를 확인한 뒤 새 1,049행을 집계했다. UTC 11:49:35.544~11:51:03.353(한국 20:49:35~20:51:03), NPC SteeringTask 실행을 포함한 구간에서 위치 검사 경고·초기화 전 궤적 경고·Up=0·CMC 추출 Error·Mass Error·전체 Error·템플릿 의존성 실패 메시지가 각각 0건이다. 위치 검사 0건은 출력 억제 결과이며, 실제 위치 오차가 없음을 독립적으로 측정한 결과는 아니다. 사용자는 테스트 완료를 보고했고, 이번 짧은 실행의 별도 시각 평가 문구는 없었다.
- 남은 Warning 3줄은 Iris `ClientSetViewTarget` 복제 준비 경고 1줄과 GameplayCamera 평가 컨텍스트 재생성 관련 2줄(메시지 본문·호출 위치)이다. 서로 다른 Warning 원인으로는 2종이다. 이번 Mover/Mass 수정 범위 밖이므로 별도 원인 확인 전 수정하지 않는다. 이전 실행의 multicast attachment 경고는 이번 구간에는 없다.
- 기준점·결과는 `Saved/Diagnostics/PIEWarningFix-Step5-Checkpoint.json`·`PIEWarningFix-Step5-Results.json`에 보관한다. 이번 대량 로그 대응은 이 재현 범위에서 검증 완료이며, §6의 과거 간헐적 프록시 초기화 문제는 추가 코드 수정 없이 재발 관찰 대상으로 남긴다.

## 참고 근거

- 프로젝트: `Source/LootNPop/Character/LNPCharacterBase.h:46`, `LNPCharacterBase.cpp:45`.
- 애니메이션: `Source/LootNPop/Animation/LNPAnimInstance.cpp:411`, `:435`.
- 중력: `Source/LootNPop/Gravity/LNPPawnGravityComponent.cpp:87`, `:97`, `:145`.
- 서브스텝: `Source/LootNPop/Movement/LNPCharacterMoverComponent.cpp:451`.
- 기존 우회: `Source/LootNPop/Movement/LNPInstantMovementEffects.cpp:11`.
- 기존 설계/분석: `.context/TechDesign_CharacterMovement.md` §7.7~7.9, `.context/TechDesign_EnemyNPC.md` §5.1.
- 엔진 경로의 기준: `C:/Program Files/Epic Games/UE_5.8/Engine/Plugins/`.
- [공식 UMoverComponent API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/Mover/UMoverComponent): 외부 이동 경고와 수용 옵션의 의미. 본 프로젝트의 실제 발생 조건 판단은 설치된 엔진 소스에 근거했다.
- [공식 Agent Movement Sync API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/MassActors/UMassAgentMovementSyncTrait): Trait 클래스와 소속 모듈. CMC 의존성은 로컬 `MassAgentTraits.cpp`에서 확인했다.
