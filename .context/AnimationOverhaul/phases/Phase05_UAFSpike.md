# Phase 5 — UAF 판정 스파이크

> 상태: 5-0·5-1 완료. 5-2 진행 중. §26 DefaultSlot·§27 FullBody/ANS 확인, §28 생존 Actor의 활성 차단2종+Melee 모듈 제거 보완 완료. 나머지 종료 경로/전체 게이트·최종 Go/No-Go 미판정. 새 세션 인계 정리 완료
> 마지막 갱신: 2026-10-28
> 전제: Phase 2·3 완료. AN-006에 따라 판정 전 마이그레이션하지 않는다.

## 1. 목표와 범위

무기 없는 플레이어 1명을 Mover UAF와 UAF Motion Matching으로 구동하고 `Roadmap.md` Phase 5의 일곱 항목을 판정한다. 무기 레이어·몽타주 계약은 별도 시험 입력으로 확인한다. 기존 `ABP_Main_UEFN`·무기 서브 ABP·표시 ABP·Chooser·PSD는 수정하지 않는다. 시험 구성은 `/Game/Characters/UAFSpike/`에 둔다. GASP는 읽기 전용이다(AN-009).

**각 단위 종료 후 사용자에게 계속 진행할지 확인한다.** 에디터 조작·PIE·그래프 배선이 사용자 수동 조작으로 더 효율적이면 즉시 요청한다. 자동화 도구를 새로 만드는 것으로 우회하지 않는다.

## 2. 단위와 완료 조건

| 단위 | 작업 | 검증·종료 조건 | 상태 |
|:---|:---|:---|:---|
| 5-0 | GASP 참조 유무·로컬 엔진 API 조사, 실행서 작성 | 참조 조회 결과·재사용 경로·차단 후보·미검증 항목 기록. 코드·에셋 변경 없음 | 완료(2026-10-23) |
| 5-1 | 최소 테스트 폰·UAF 그래프, 기존 PSD·Chooser·Mover 궤적 연결 | 같은 Chooser가 반환하는 PSD 목록과 실제 UAF 검색 결과 확인. 북극·적도 이동/정지/회전·공중 고정·발 배치·움직이는 base 확인. 동등 구현 경로가 없으면 해당 사유로 조기 No-Go 판정 | 완료(2026-10-27, §19). 최소 기능 조건과 시험 정리 충족. 기존 ABP와의 최종 동등성 통과를 의미하지 않음 |
| 5-2 | 몽타주·ANS·레이어·루트모션·네트워크·디버깅 판정 | 아래 표 전 항목에 실행 근거를 붙여 Go/No-Go 기록. 기본 플레이어·승격 적 경로에 회귀가 없는지 확인 | 진행 중(§26 DefaultSlot·§27 FullBody/ANS 정상·중단·§28 생존 Actor의 활성3종 모듈 제거 보완 확인. 나머지 종료 경로/전체 게이트는 남음) |

최초 로드맵의 1~2세션 예상에서 **준비 포함 3단위로 구체화**했다. 로컬 GASP UAF 참조를 찾지 못했고, Phase 2 구면 보정과 몽타주 메시 선택을 별도로 확인해야 한다. 5-1의 조기 No-Go면 5-2를 수행하지 않는다.

## 3. 5-0 결과 — 확인된 사실과 한계

### GASP 참조

- 로컬 프로젝트: `D:/UnrealProjects/GameAnimationSample`, EngineAssociation 5.8. 파일명 검색에서 UAF·AnimNext 이름 에셋을 찾지 못했고 `.uproject`에 해당 플러그인의 명시적 활성화가 없다. 이것만으로 에셋 부재나 최종 플러그인 활성 상태를 단정하지 않는다.
- 사용자가 GASP 에디터 Output Log의 Python에서 다음 읽기 전용 조회를 실행했고 결과는 `LogPython: []`였다.

```python
import unreal; ar = unreal.AssetRegistryHelpers.get_asset_registry(); print([(str(a.package_name), str(a.asset_class_path)) for a in ar.get_assets_by_path('/Game', recursive=True) if any(k in str(a.asset_class_path).lower() for k in ('uaf', 'animnext'))])
```

- 결론: **조회한 `/Game` 레지스트리에서 UAF·AnimNext 클래스 에셋을 찾지 못했다.** 플러그인 콘텐츠·Blueprint 내부 컴포넌트까지 전수 부재를 증명한 것은 아니다. 현재 참조 없이 최소 구성을 만드는 방향이며, GASP 저장·수정은 하지 않았다.
- 현재 세션에는 LootNPop MCP만 노출돼 있어 GASP 전용 연결 우회 구현 대신 사용자 수동 조회를 요청했다.
- [Epic GASP 5.8 소개](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8)는 Mover Pawn의 State Machine·Chooser·Blend Stack 개선을 설명하며 UAF 캐릭터는 명시하지 않는다. 소개문의 누락을 부재의 증거로 삼지 않는다.

### 로컬 엔진 소스 대조

기준 엔진은 `C:/Program Files/Epic Games/UE_5.8/Engine/`. 아래 경로는 그 기준 상대 경로다. API 존재는 실행 통과를 의미하지 않는다.

| 항목 | 확인된 구현 | 다음 실증·주의점 |
|:---|:---|:---|
| PSD 재사용 | `Plugins/Experimental/UAF/UAFPoseSearch/Source/UAFPoseSearch/Internal/MotionMatchingTraitData.h`: `Databases`는 기존 `UPoseSearchDatabase` 배열. `NextUpdateInterruptMode`·`BlendArguments` 입력 존재 | Phase 2 DB 목록·출발/제자리회전 인터럽트·블렌드 시간이 실제로 반영되는지 확인 |
| Chooser 재사용 | `UAF/UAFChooser/Source/UAFChooser/Private/RigUnit_EvaluateChooser.h/.cpp`(이 표에서 `UAF/`는 `Plugins/Experimental/UAF/`): `FRigVMDispatch_EvaluateChooser`는 `UChooserTable`·`ContextObject`를 받아 객체 배열 결과를 지원 | 기존 `CHT_PoseSearchDatabases_UEFN`은 `ULNPAnimInstance` 컨텍스트를 읽는다. 같은 객체/값으로 평가하고 결과 배열을 MM에 전달해야 한다. `ChooserPlayerTrait`는 `UUAFAnimChooserTable`와 단일 선택 경로이므로 기존 다중 PSD Chooser를 그 핀에 직접 꽂는 것으로 대체하지 않는다 |
| 포즈 이력·궤적 | `UAF/UAFPoseSearch/Source/UAFPoseSearch/Internal/HistoryCollectorTraitData.h`: 외부 `FTransformTrajectory` 입력·포즈 이력 참조 변수. `Plugins/Experimental/MoverAnimNext/Source/MoverAnimNext/Private/Graph/RigUnit_GenerateMoverTrajectory.cpp`: Mover 예측기 호출 | 기존 샘플 규약·접평면 값·이력 시점 일치 필요. 기본 제공 궤적이라고 구면 통과로 간주하지 않는다 |
| Offset Root | `UAF/UAFWarping/Source/UAFWarping/Private/OffsetRootBoneTrait.h/.cpp`: 모드·평면 법선·오차 제한 입력. 속도 제한에 `GetSafeNormal2D`, 회전 제한에 기존과 같은 곱셈 순서가 남아 있음 | 기본 법선은 월드 Z. `Accumulate/Interpolate`에서 컴포넌트 회전을 따라가지 않는다. 현재 `ULNPAnimInstance::NativeThreadSafeUpdateAnimation`은 태그 `OffsetRoot`의 `FAnimNode_OffsetRootBone` 상태를 직접 고치므로 UAF trait에는 적용되지 않는다. Up 정렬·base delta·Up 둘레 90° 제한을 동등하게 구현할 수 있는지 먼저 판정 |
| Control Rig | `UAF/UAFControlRig/Source`에 trait·변수 매핑 구현 | 수정한 `CR_Biped_FootPlacement_UEFN`과 지상 알파·착지 리셋·움직이는 base 처리의 핀 연결을 실증 |
| ABP 연결 | `UAF/UAFAnimGraph/Source/UAFAnimGraph/Internal/Graph/AnimGraph/AnimNode_AnimNextGraph.h`: UAF 그래프 출력을 기존 AnimGraph에서 실행하는 노드 | 공급층·후처리를 유지하는 중간 시험 경로는 있다. ABP 래퍼만 성공한 것을 Mover UAF·UAFLayering 전체 Go로 판정하지 않는다 |
| 몽타주·ANS | `UAF/UAFAnimGraph/Source/UAFAnimGraph/Private/Injection/UAFMontageComponent.cpp`: 기존 `UAnimInstance`를 몽타주 전용 초기화·틱. `UAF/UAF/Source/UAF/Private/Module/UAFNotifyDispatcherComponent.cpp`: 활성 ANS 비교·`NotifyEnd` 전달 | 몽타주 컴포넌트는 Actor의 첫 SkeletalMeshComponent를 찾고 `bEnableAnimation`을 검사한다. 숨김 원본/표시 메시 2개인 LNP에서 대상이 맞아야 한다. 몽타주 전용 초기화로 기존 C++ 변수 공급이 계속 돈다고 가정하지 않는다. 중단·블렌드아웃·인스턴스 종료 때 End 누락/중복 실증 필요 |
| 레이어 | `UAF/UAFLayering/Source/UAFLayering/Public/UAFLayeringUtils.h`: 레이어 켬/끔·가중치 API. Montage Layer Data trait 존재 | `ALI_WeaponStyles` 교체·본 마스크·AO·그립 IK 계약과 동등한지 별도 판정 |
| Mover 루트모션 | `Plugins/Experimental/MoverAnimNext/Source/MoverAnimNext/Private/Graph/RigUnit_MoverToggleRootMotion.cpp`: `FLayeredMove_RootMotionAttribute` 큐/취소 | 기존 근접 Motion Warping 어댑터 경로와 동일하다는 뜻은 아님. 중복 소비·적도 방향·취소·재시뮬레이션 실증 필요 |
| 디버깅 | `UAF/UAF/Source/UAFEditor/Private/RewindDebugger/` 및 `UAF/UAFAnimNode/Source/UAFAnimNodeEditor/Private/RewindDebugger/` 구현 존재 | 실제 선택 DB/포즈·그래프·시점 추적이 가능한지 사용자 기록 확인 |

첫 행의 전체 경로 표기와 나머지 축약은 같은 로컬 설치본을 가리킨다. 공개 API 목록도 [Epic UAF 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/PluginIndex/UAF)에서 확인했다.

## 4. 실행 판정표

| 게이트 | 통과 기준 | 현재 판정 |
|:---|:---|:---|
| PSD·Chooser | 기존 에셋을 변경하지 않고 같은 상태 입력의 PSD 배열로 UAF가 검색. Starts/Stops/Turns 등 전환과 인터럽트 확인 | 부분 확인 — 대표 전환의 전달·실제 선택(§13), Sprint Turns·GaitTransitions·Run Pivot의 전달·후보 검색(§17) 확인. §17의 목표 전환 클립은 미선택. 지속 고속/전환의 최종 품질 비교는 5-2에 남음(§19) |
| 몽타주·ANS | 슬롯 3종과 그룹 계약 유지. ANS 5종의 정상 종료·`Montage_Stop`(즉시/블렌드아웃)·교체 시 End 정확히 한 번, 태그/근접 판정 엔티티 잔류 없음 | **부분 확인·전체 게이트 미판정** — §26 DefaultSlot·§27 FullBody/ANS 정상·대표 활성 Stop/교체/이동 취소 확인. §27 시스템 제거 실패는 §28의 같은 활성3종 표본에서 End 각1·태그/실제 엔티티 정리로 보완 확인. Combo/Cancel 활성 제거·Actor/월드 종료·UpperBody/전체 슬롯·Branching은 미검증 |
| UAFLayering | 런타임 레이어 전환·본 마스크·무기별 AO/그립 IK와 슬롯 순서 동등. Phase 3 표시 포즈·공격 중 물리 억제 계약 유지 | 미검증 |
| Motion Warping·Mover | 루트모션 공격의 이동/회전/중단이 기존 어댑터와 일치, 중복 적용 없음 | 미검증 |
| 프록시·재시뮬레이션 | 리슨 2인 호스트/게스트 및 `ActorPromoted`에 컨트롤러 없는 경로·게스트 재시뮬레이션 확인 | 미검증 |
| 디버깅 | 선택 DB/클립·그래프 실행을 기록하고 필요한 시점으로 돌아가 확인 가능 | 부분 확인 — 최소 MM 구성의 UAF 시스템/그래프·클립 트랙, Active Pose·과거 구간 재조회 확인. 전체 계약은 5-2에서 판정 |
| 구면·최종 포즈 | 적도 이동/회전·정지·경사·착지·움직이는 base에서 루트 Up·발 배치 유지. 공중 `Fall_Loop` 고정(AN-007) | 부분 확인 — 구면 루트 제한(§10)·공중 고정(§12)·Strafe 비교(§14)·발 배치/경사/적도(§15)·패널 점프/착지와 큰 Yaw 롱소드(§18) 확인. 루트 수치는 CR 앞이며 기존 ABP 대비 최종 포즈 품질·무기 통합 후 동등성은 5-2에 남음(§19) |

**현재 Go/No-Go는 보류.** 소스 조사만으로 체크하지 않는다. 한 항목이라도 동등하게 구현할 수 없으면 AN-006에 따라 No-Go와 사유를 `.context/DiscardedApproaches.md`에 남긴다. 통과해도 Phase 6 착수는 사용자 확인 뒤 별도 단위다.

**5-1에서 확인한 5-0 조사 한계:** 기본 `Evaluate Chooser`의 UAF 경로는 `UAFAssetInstance` 구조체를 컨텍스트 0번에 먼저 넣고 객체를 나중에 넣는다. 기존 테이블의 0번은 `LNPAnimInstance`여서 직접 연결 시 반복 오류가 났다. 단순히 `AnimContext`를 채우면 해결되는 문제가 아니다. 기존 테이블을 보존하고 `UChooserFunctionLibrary::EvaluateChooserMulti`로 같은 객체 컨텍스트에서 평가한 배열을 전달하는 시험으로 바꿨다.

## 5. 다음 행동

현재 §28의 **생존 Actor UAF 제거 보완 단위는 완료**다. 새 세션에서는 사용자 진행 요청을 받은 뒤 Combo/Cancel 활성 제거를 먼저 확인하고, Actor/월드 종료 시 정리 소유권과 현재 호출 생략 경로를 조사한다. 구체적인 재현 메모는 §28 마지막에 둔다. 한 구현 단위마다 진행 확인을 유지하며, 이번 인계 요청을 후속 구현 승인으로 해석하지 않는다. 전체 슬롯/ANS 게이트·최종 Go/No-Go는 미판정이다. 아래는 5-1 착수 당시의 실행 기록이다.

사용자 승인으로 5-1을 시작했다. LootNPop MCP의 `ListEnabledPlugins`에서 `UAF`·`UAFAnimGraph`·`UAFMirroring`가 활성인 것을 확인했다. `UAFChooser`·`UAFPoseSearch`·`UAFControlRig`·`UAFWarping`·`MoverAnimNext` 5개를 `LootNPop.uproject`에 명시하고 JSON 파싱·diff 공백 검사를 통과했다. MCP `SetPluginEnabled`는 메모리의 프로젝트 설정만 바꾸고 파일을 저장하지 않아 파일에도 명시했다(엔진 툴 구현 확인). 런타임 로드 확인은 재시작 후 수행한다. GASP는 더 필요하지 않아 닫아도 된다고 안내했다.

사용자 재시작 후 MCP `IsEnabled`로 위 5개가 모두 `true`인 것을 확인했다. 현재 MCP에는 UAF 그래프 편집 도구가 없어 수동 조작으로 아래 시험을 진행했다.

### 5-1 중간 결과 — 기존 Idle PSD 직접 재생

- `AG_LocomotionSpike`: Motion Matching 템플릿(Blend Stack Core·Blend Smoother Core·Motion Matching·Pose History) → Animation Output. `Databases`에 기존 `PSD_Stand_Idles_UEFN`, Scene Settings의 메시에는 `SKM_UEFN_Mannequin`을 지정했다.
- 최초에는 앉았다 일어나는 동작을 반복했다. DB 의존성은 Idle Loop와 `M_Relaxed_Transition_Crouch_to_Stand`였고, 로그에는 스키마가 요구하는 본 4개 누락 경고가 반복됐다. 기본 템플릿의 `CollectedBones`는 빈 배열이다.
- Pose History `Collected Bones`에 `foot_l`·`foot_r`·`pelvis`·`spine_05`를 추가하자 사용자 관찰은 자연스러운 서 있는 Idle(호흡 정도)로 바뀌었다. 최신 로그에서 본 누락 경고가 중단됐고 컴파일 성공을 확인했다. `FoleyEvent: Walk` 미처리 경고와 미리보기 메시 쓰기 경고도 관찰했으므로 경고 전체 0이나 런타임 통과로 일반화하지 않는다.
- 구성 사본 생성·지정 저장 성공: `/Game/Characters/UAFSpike/BP_PlayerUAFSpike` ← `/Game/Pawns/BP_LNPPlayer`, `S_LocomotionSpike` ← `/UAFAnimGraph/Internal/S_SingleGraph`. 기존 플레이어·엔진 템플릿은 수정하지 않았다.
- 첫 이동 시험은 숨김 ABP가 기존 C++ 상태·궤적을 공급하고 표시 메시를 UAF가 평가하는 중간 구성으로 한다. 두 그래프가 함께 돌아 성능 비교에 쓰지 않는다. 구면 보정·전체 UAF 이전의 동등성이 확인된 것은 아니다.

### 5-1 중간 결과 — Chooser 컨텍스트와 입력 연결 코드

- 사용자 배선 후 로그에서 컨텍스트 0번 타입 불일치를 확인했다(§4 아래). 사용자가 `Evaluate Chooser.Result → Databases`를 끊고 컴파일·저장했다.
- `Source/LootNPop/Animation/UAFSpike/LNPUAFSpikeAnimInstance.h/.cpp`: 기존 공급층 파생 클래스와 Native Shared Variables 구조체 `FLNPUAFSpikeInputs`를 추가했다. 기존 Chooser를 `this` 컨텍스트로 평가하고 `Databases`·`Trajectory`·`InterruptMode`·`BlendArguments`를 UAF 프록시 변수에 전달한다. UAF `PrePhysics`는 숨김 메시 틱 뒤로 의존성을 건다. 시스템 템플릿 `Graph` 변수도 지정한다. `[UAFSpike] inputs=ready/unbound`는 최초·성공 상태 변경 때만 출력한다.
- 기본 `ULNPAnimInstance` 구현·기본 플레이어 ABP는 변경하지 않았다. 테스트용 메인 ABP 사본을 새 파생 클래스로 바꾸고 `SpikeSystem`·`SpikeGraph`를 지정할 예정이다. 시스템과 그래프 양쪽에 같은 Native Shared Variables를 가져와야 한다.
- `LootNPop.Build.cs`에 `UAF`·`UAFAnimGraph` 비공개 의존성을 추가하고 프로젝트에도 두 플러그인을 명시했다. 사용자 에디터 종료 후 권한 확장으로 기준 명령 `Build.bat LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE` 실행: **Succeeded, 종료 코드 0, 12 actions, 59.59초**. 헤더 등록·컴파일·링크 통과이며 런타임 전달 성공은 아직 아니다.

### 5-1 중간 결과 — 테스트 사본 설정

- 재실행 후 새 네이티브 클래스 CDO를 조회해 등록을 확인했다. 메인 ABP 사본 `ABP_SourceUAFSpike`를 `ULNPUAFSpikeAnimInstance`로 부모 변경·경고를 오류로 취급한 컴파일 통과. 기본값 `SpikeSystem=S_LocomotionSpike`, `SpikeGraph=AG_LocomotionSpike`, 기존 `LocomotionDatabaseChooser` 유지 확인.
- `BP_PlayerUAFSpike`: AnimSourceMesh의 AnimClass를 위 사본으로 변경. VisualMesh는 `SKM_UEFN_Mannequin`, `bEnableAnimation=false`, `bDisablePostProcessBlueprint=true`. `UAFLocomotion` 컴포넌트 추가, System Asset은 `S_LocomotionSpike`, Output Component의 Component Property는 `VisualMesh`. 컴파일·저장 및 주요 설정 재조회 통과.
- `BP_GameModeUAFSpike`: 기존 `BP_LNPGameMode` 사본에 DefaultPawnClass를 테스트 플레이어로 지정. 컴파일·저장 통과. 기본 GameMode 에셋·전역 설정은 변경하지 않았다.
- 사용자 수동 연결 완료: AG의 임시 Chooser·로컬 변수 정리, S와 AG에 같은 `LNPUAFSpikeInputs` Native Shared Variables 추가, AG의 공유 `Databases`·`Trajectory` Getter → MM 입력. 화면에서 배선 확인. `InterruptMode` → MM `NextUpdateInterruptMode`, `BlendArguments` → MM 동명 입력은 아직 연결 전이다. 필요 핀은 MM Details의 `Customization → Node Layout → Edit Layout`으로 노출할 수 있다(로컬 엔진 구현 확인).

**현재 사용자 수동 조작:** 메인 레벨의 World Settings GameMode Override 원래 값을 메모한 뒤 `BP_GameModeUAFSpike`로 임시 변경, 1인 PIE 로딩 후 3초 대기·표시 포즈 보고, PIE를 유지한다. 로그의 입력 전달 상태를 먼저 확인한다. 시험 후 원래 GameMode로 복원한다. 이동·구면·런타임 입력 전달은 아직 미검증이다. 5-1 완료가 아니므로 5-2 진행 확인 단계로 넘어가지 않는다.

### 5-1 중간 결과 — 첫 PIE 출력 경로 진단

- TestMap03에서 테스트 GameMode로도 런타임 지형·스폰 정상(사용자 보고). 표시 마네킹 T 포즈, `[UAFSpike]` 전달 로그 없음. 실제 ABP 갱신·조기 반환 원인은 아직 미확정이다.
- 실행 중 UAF 컴포넌트는 Output Component=`VisualMesh`이나 `AnimSourceMesh` 쓰기 거부가 반복됐다. 템플릿 시스템의 Initialize·Write Pose 노드는 별도 `SkeletalMesh` 변수를 사용한다. 해당 변수의 기본 locator는 `Current Actor → Get SkeletalMeshComponent`로 첫 메시를 찾는다(`AnimNextComponentLocatorFragment.cpp`: `FindComponentByClass`). 컴포넌트 출력 설정만으로 이 명시적 입력이 바뀌지 않는다.
- 사용자에게 기존 Component locator를 Property locator로 교체하고 클래스 `LNPCharacterBase`의 `VisualMesh`를 선택·컴파일·저장·PIE 재시험하도록 요청했다. 아직 수정·런타임 성공 미확인이다. 입력 전달 로그 누락도 별도로 확인해야 한다.
- GameMode Override는 시험용 임시 변경 상태다. 시험 후 원래 값으로 복원한다. 현재 git에 TestMap03 변경이 보이므로 복원·저장 여부도 확인한다. 다른 레벨 변경을 통째로 되돌리지 않는다.

### 5-1 중간 결과 — 런타임 입력·Idle 출력 확인

- ABP CDO 조회에서는 유효하던 SpikeSystem·SpikeGraph가 실제 AnimInstance에서는 None이었다. 사용자가 ABP Class Defaults의 참조를 비웠다가 재지정·컴파일·저장한 뒤 실제 인스턴스의 두 참조와 `[UAFSpike] inputs=ready databases=1 trajectorySamples=31`을 확인했다. CDO와 런타임 값 차이의 근본 원인은 미확정이다.
- S의 실제 locator는 `Current Actor → Get SkeletalMeshComponent → VisualMesh`였다. Property가 Component 뒤에 붙어 Actor 속성을 읽지 못하는 경로였다. 중간 Component fragment만 제거해 `Current Actor → VisualMesh`로 바꾸자 사용자 관찰이 Idle로 바뀌었고 05:25 PIE 최신 로그에서 쓰기 거부 경고도 중단됐다.
- 다음 사용자 수동 작업은 AG 공유 `InterruptMode`·`BlendArguments` Getter를 MM의 `NextUpdateInterruptMode`·`BlendArguments`에 연결하는 것이다. 숨은 핀은 Node Layout에서 기존 노출 핀과 같은 범주로 배치한다. 이동·실제 선택 DB·구면·최종 일곱 게이트는 여전히 미검증이다.

### 최신 체크포인트 — 실제 UAF DB/클립 추적

- 공유 인터럽트·블렌드 입력 배선 후 사용자 이동·방향 전환·점프·착지 관찰 통과. Pose History의 Phase 커브 수집을 추가해 스키마 누락 경고도 해소했다. Foley 이름 이벤트 처리기 없음 경고는 잔류한다.
- 사용자 승인으로 LootNPopEditor에 Rewind 저장 경로 오버라이드를 추가했다. 엔진 파일 수정 없이 UE 5.8 내부 헤더를 참조한다. Development 빌드·시작 로그·새 Trace 생성으로 프로젝트 Saved/RewindDebugger 저장을 검증했다. 기존 문서 쪽 폴더는 재생성되지 않았다. 저장 경로 보완 단위 완료.
- 19:19 KST Listen Server 녹화 중 `Trace.Status`에서 Object·Frame·Animation·PoseSearch·UAF 채널 활성을 확인했다. 설치 UE 5.8 `GameplayInsights/Private/GameplayProvider.cpp`의 `AppendRecordingInfo`는 ListenServer를 녹화 시간축에서 제외하고, Rewind는 이 시간축으로 대상 객체 조회 범위를 계산한다. 1인 PIE 설정도 `PIE_ListenServer`였고 로그의 실제 월드 모드도 일치했다.
- 사용자에게 PIE Net Mode를 `Play Standalone`으로 변경해 비교하도록 요청했다(별도 프로세스 Standalone Game과 구분). 19:22 KST Standalone·입력 ready 로그와 플레이어/하위 트랙 생성을 확인했고 사용자도 그래프 표시를 보고했다. 최신 Trace는 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261023_192247450.utrace`다. **빈 트랙 진단 단위 완료.** 엔진·프로젝트 코드·에셋 변경 없음.
- 사용자 승인 후 같은 녹화에서 `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_2 → AG_LocomotionSpike` 및 클립·EvaluationProgram 트랙과, `UAFLocomotion → Pose Search` 검색 트랙을 확인했다. `AnimSourceMesh → ABP_SourceUAFSpike → Pose Search`와 소유 경로가 다르다. 사용자 수동 스크럽으로 Rewind Debugger Details의 Active Pose를 읽었다(별도 창 불필요).
- 약 4초: `PSD_Stand_Run_RL_Loops_UEFN → M_Relaxed_Run_Switch_RR_RL_Rfoot`, Asset Player Time 0.595301초. 약 12초: `PSD_Stand_Idles_UEFN → M_Relaxed_Stand_Idle_Loop`, 표의 Time 약 0.43초·이동 속도 0. 다시 약 4초로 돌아가 같은 달리기 DB/클립 재조회 확인. 돌아간 프레임은 달라 Time 약 0.47초·비용 1.126으로 표시됐으며 동일 프레임 재현이라고 기록하지 않는다.
- **실제 UAF DB/클립 추적 단위 완료.** 새 녹화·코드·에셋 변경 없음. 입력 PSD 배열 시점별 대조·Starts/Stops/Turns 전환/인터럽트와 전체 디버깅 계약 통과는 아직 아니다. 다음 구면 Offset Root 보정 동등 구현 단위 진행 전 사용자 확인을 받는다. PIE는 사용자 일시정지 상태, Net Mode는 Standalone으로 유지됐다. 이후 리슨 2인 회귀는 별도다.
- 5-1 전체는 미완료다. 기존 녹화의 UAF 달리기·Idle 검색 결과는 확인했다. 입력 PSD 배열 시점별 대조·전환/인터럽트·루트 보정·발 배치·구면·공중 고정·움직이는 base 및 최종 일곱 게이트 전체는 미검증이며 5-2에 착수하지 않는다. GameMode Override는 시험용 임시 상태로 종료 시 원래 값으로 복원한다.

## 6. 구면 루트 보정 — 동등 구현 경로 조사(2026-10-23)

**조사·구현/그래프 연결 단위 완료. 기본 UAF trait 핀 설정만으로는 기존 보정과 동등하지 않다.** 아래는 최초 보정 경로의 조사·구현 기록이다. 첫 런타임의 방향 추종 문제는 이후 §8 Steering·§10 소비 후 보정으로 현재 시험 범위에서 개선을 확인했다. 전체 구면 게이트/Go는 아직 보류하며 최신 인계는 §11이다.

### 기존 계약과 차이

기준은 `ULNPAnimInstance::NativeThreadSafeUpdateAnimation`과 `design/GASPReference.md` §2.10·§2.12다. 엔진 경로는 `C:/Program Files/Epic Games/UE_5.8/Engine/` 기준이다.

| 항목 | 기존 ABP 계약 | UAF에서 필요한 처리 |
|:---|:---|:---|
| 지면 평면 | 컴포넌트 Up을 GroundNormal로 공급 | `MeshComponentTransformWorld`는 표시 메시의 월드 변환, `AnimatedGroundNormal`은 그 메시의 Up을 공급. 기본 월드 Z 유지 금지 |
| 이동/회전 모드 | 이동 중 Interpolate·정지/공중/전신 몽타주 Release, 회전은 전신 몽타주 외 Accumulate | 기존 Getter의 의미로 enum을 명시적으로 매핑. enum 숫자 캐스팅에 의존하지 않음 |
| base 운반 | Accumulate·Interpolate에서만 `BasedMovementDelta` 이동·회전 추가 | UAF 자신의 누적 상태에 같은 규칙 적용. Release에서는 엔진이 컴포넌트 델타를 더하므로 중복 적용 금지 |
| Up 정렬 | 누적 회전을 컴포넌트 Up으로 스윙 정렬, Yaw 보존 | `SimulatedRotation`을 기본 루트 평가 직전에 보정. 법선 핀만으로 회전 정렬이 되지 않음 |
| 회전 오차 | 로컬 Z의 부호 있는 Yaw를 ±90°로 제한 | 기본 `MaxRotationErrorDegrees=-1`, 보정 코드에서 컴포넌트 회전 뒤에 로컬 Z 회전을 곱함. 기본 UAF에도 잘못된 곱셈 순서가 남아 있음 |
| 이동 제한 | 반경 30cm, 이동 속도 클램프 끔 | `MaxTranslationError=30`, `bClampToTranslationVelocity=false` 유지. 기본 UAF도 `GetSafeNormal2D` 사용 |
| 복귀 시간 | 이동 반감기 Idle 0.1초·Moving 0.3초, 회전 0.1초 | UAF의 SmoothingTime은 반감기가 아님. 기존 감쇠 계수와 맞추려면 매 프레임 `(max(HalfLife, DeltaTime)+1e-8)/ln(2)`로 변환. 이동·회전 각각 적용 |

반감기 대조 근거: 기존 `AnimationWarping/Source/Runtime/Private/BoneControllers/AnimNode_OffsetRootBone.cpp`의 `DamperImplicit`는 `ln(2)*dt/(max(HalfLife,dt)+epsilon)`을 `InvExpApprox`에 넣는다. UAF trait는 이동에 `FMath::ExponentialSmoothingApprox`, 회전에 `SpringMath::ExponentialSmoothingApproxQuat`를 사용하며 둘 다 `dt/SmoothingTime`을 넣는다. 단위 변환은 감쇠 계수 대조이며 전체 포즈·네트워크 동등성의 증명은 아니다.

### 최소 후보 — 기본 Offset Root 위에 프로젝트 additive trait

- `UAFWarping/Source/UAFWarping/Private/OffsetRootBoneTrait.h`의 `FOffsetRootBoneTrait::FInstanceData`는 `SimulatedTranslation`·`SimulatedRotation`·현재/이전 메시 변환을 가진다. 일반 latent 핀은 이 누적 상태를 바꿀 수 없다.
- `UAFAnimGraph/Public/TraitCore/TraitBinding.h`의 `GetStackBaseTrait`·`GetTrait()->GetTraitUID()`·`GetInstanceData<T>()`로 같은 스택의 정확한 기본 trait를 확인한 뒤 상태에 접근하는 후보가 있다. 다른 기본 trait이면 상태를 임의 캐스팅하지 않고 설정 오류로 처리한다.
- 프로젝트 additive trait의 `IEvaluate::PostEvaluate`에서 보정 task를 먼저 추가한 뒤 상위 구현으로 전달하면 기본 Offset Root task가 뒤에 추가된다. `FEvaluationProgram::Execute`는 추가 순서대로 실행한다. 따라서 직전 프레임 누적 상태에 base 운반·Up 정렬·Yaw 제한을 적용하고 기본 root motion 소비·평면 투영·감쇠·오차 제한·포즈 쓰기를 이어 가는 구조를 시험할 수 있다.
- 이 경로는 **UE 5.8 UAFWarping 내부 헤더 의존성**을 추가한다. 기본 task 전체를 프로젝트로 복사하는 대안은 중복 유지보수 범위가 커 첫 후보로 선택하지 않는다. 기존 ABP 후처리 래퍼도 시험 경로는 있지만 UAF 자체 보정 동등성 근거로 삼지 않는다.
- `UAFAnimNode`의 공개 `FUAFOffsetRootBoneNode/AnimOp`도 조사했다. 현재 trait 그래프와 다른 실행 체계이고 같은 상태 보정·월드 축 문제도 남아 있어, 공개 헤더가 있다는 이유로 기존 AG에 바로 대체할 수 있다고 판단하지 않았다.

### 구현 단위의 범위와 검증

1. `Source/LootNPop/Animation/UAFSpike/` 안에 얇은 보정 trait·평가 task를 두고 필요한 모듈/내부 include 의존성만 추가한다. 기본 ABP·플레이어 경로·엔진 파일은 수정하지 않는다.
2. 기존 공급층 갱신 후 복사된 `BasedMovementDelta`와 모드·반감기·표시 메시 변환을 UAF 입력으로 전달한다. 이동 base 누적은 `UpdateLocomotionValues`가 현재 프레임 값으로 옮긴 뒤 Pending을 비우므로, 파생 `NativeUpdateAnimation`에서 Super 이후의 값을 읽는 현재 공급 시점을 활용한다. UAF 평가 전 틱 의존성을 유지한다.
3. 첫 평가·텔레포트·무효/비활성 프레임에 오래된 상태를 보정하거나 base를 두 번 소비하지 않는지 확인한다. 보정 task는 기본 task와 같은 상태 수명·VM 실행 조건 안에서 동작해야 한다. 기본 trait가 초기화로 값을 덮는 첫 프레임도 고려한다.
4. 사용자가 테스트 AG에 기본 Offset Root와 보정 trait를 같은 스택으로 추가하고 입력을 연결한다. UAF 자신의 상태를 보정하며 숨김 ABP의 가상 루트를 다시 적용하지 않는다. 향후 Aim·FutureFacing 등의 루트 기준도 UAF 상태로 맞춰야 하며 기존 숨김 ABP 상태를 전체 이전의 단일 기준으로 간주하지 않는다.
5. C++ 컴파일·등록·테스트 AG 컴파일 확인 후 북극/적도 이동·정지·큰 회전, 공중 전환, 이동/회전 base 탑승을 수동 검증한다. Up 기울기·Yaw 상한·위치 반경·base 중복 여부를 확인한다. 이 구현·실증 전 동등성 통과로 기록하지 않는다.

### 코드·빌드 중간 결과

- `LNPSphericalOffsetRootTrait.h/.cpp`: 같은 스택의 기본 trait UID 확인 후, 기본 평가 task 앞에 프로젝트 보정 task를 넣는다. 엔진 task 구현은 복사하지 않는다. `LNPUAFSpikeRootInputs.h`의 프레임별 입력 사본으로 base 운반·Up 정렬·Yaw ±90° 제한·30cm 위치 제한·감쇠 단위 변환을 적용한다.
- `LNPUAFSpikeAnimInstance`는 기존 Super 갱신 뒤 표시 메시 변환·base 변위·모드·반감기·지상/텔레포트·일련번호를 공유 입력 `OffsetRoot`에 전달한다. 첫 평가/텔레포트는 기본 task 초기화를 이용하고, 같은 입력 일련번호의 base 운반은 중복 적용하지 않는다. 누락 입력은 설정 오류를 한 번 기록하고 기본 루트 평가를 비활성화한다.
- `LootNPop.Build.cs`에 UAFWarping 비공개 의존성과 UE 5.8 `Private` include 경로를 추가했다. 엔진 파일·기본 ABP·플레이어 에셋은 수정하지 않았다.
- 사용자 에디터 종료 후 기준 전체 빌드 성공(13 actions, 190.73초). 최종 검토에서 루트모션 속성 누락 프레임에도 표시 메시 변환이 기본 task 초기화 전에 전달되도록 순서를 조정했고 증분 빌드 성공(4 actions, 8.41초). 새 경고 없음, diff 공백 검사 통과.
- 재실행 후 MCP에서 `LNPSphericalOffsetRootSharedData`·`LNPUAFSpikeRootInputs`가 ScriptStruct로 등록된 것을 확인했다. AG Variables에 공유 `OffsetRoot`가 자동으로 나타났으며 재가져오기는 필요 없었다. 기존 S·AG의 시작 시 자동 컴파일 성공도 확인했지만 새 보정 그래프 컴파일과 구분한다.
- **정확한 수동 추가 경로:** 테스트 AG 빈 공간 우클릭 → `Trait Stack` 추가 → 노드 선택 후 Details `Add Trait`에서 `Offset Root Bone`을 먼저, `LNP Spherical Offset Root`를 다음에 추가한다. 개별 Offset Root는 우클릭 노드 템플릿에 없었다. `AnimNextAnimGraphEditorModule.cpp::CollectContextMenuActions`는 빈 Trait Stack RigUnit 메뉴를 등록하고, `TraitStackEditor.cpp::CreateTraitStackHeaderWidget`는 빈 스택에는 Base·그다음에는 Additive trait를 고르는 Add Trait를 제공한다. 기존 Pose History는 Motion Matching 스택 내부 trait다.
- 사용자 보고·화면으로 새 Trait Stack에 `Offset Root Bone`과 `LNP Spherical Offset Root` 배치·기본 이동/회전 속도 클램프 꺼짐을 확인했다. 사용자는 MM 출력 → 새 노드 Input → Animation Output 배선 완료를 보고했지만 공유 `OffsetRoot`의 Get 선택은 아무 노드도 만들지 않았다. 새 배선의 컴파일·실행은 미검증이다.
- Getter 실패 원인: `RigVMRegistry.cpp::RefreshEngineTypes_NoLock`는 미등록 C++ 구조체를 건너뛰고, `RigVMSchema.cpp::SupportsExternalVariable_NoLock`는 타입 인덱스가 없으면 오류 없이 false를 반환한다. UObject의 ScriptStruct 등록·Variables 표시와 별도다. `LootNPop.cpp`의 모듈 시작에 `RegisterStructTypes`·`FindOrAddType`로 RootInputs를 등록하고 RigVM 직접 의존성을 추가했다. 엔진 수정 없음. 사용자 저장·에디터 종료 후 Development 빌드 성공(5 actions, 48.13초, 새 경고 없음). 재실행 뒤 Getter 생성·RootInputs 전체 핀 연결·AG/S Compile·Save를 요청했으며 답변 대기다. PIE 미시작.
- **2026-10-24 최종 확인:** 사용자가 연결·컴파일·저장 완료를 보고했다. AG 화면에서 Get OffsetRoot.Value → 보정 trait의 Root Inputs 전체 핀과 MM → 보정 스택 → Output, 컴파일 성공 아이콘·All Saved·속도 클램프 꺼짐을 확인했다. 14:46:28 KST AG 최종 컴파일·14:46:29 저장, 14:46:50 S 컴파일 성공 로그를 확인했다. S는 변경이 없어 파일 시각이 유지됐다. 편집 중 GUID/editor data 경고·bValid 하위 핀 탐색 오류는 있었으며, 최종 화면의 전체 구조체 연결·성공 컴파일과 구분한다. 구현/연결 단위 완료, PIE 꺼짐. 다음 런타임 구면·base 검증 단위는 사용자 진행 승인 후 시작한다.
- **첫 런타임 시험에서 문제 재현:** 사용자 시작 지면 이동·정지·급반전·점프/착지 후, 룩 급회전에 표시 방향이 충분히 따라가지 않아 전방 달리기 포즈가 옆/대각선을 향하는 문제를 보고했다. PIE 종료 후에도 20261024_145134378 녹화가 로컬에 남았다. 기존 ABP의 회전 Accumulate 계약은 Steering×2로 보충한 루트모션과 짝인데 최소 UAF에는 그 Steering 체인이 빠져 있다. 설치 UAFWarping에도 Steering trait가 있고 RootBoneTransform·TargetOrientation을 필요로 한다. UAF 자신의 루트 상태·현재 애니 평가와 연결해야 하며 숨김 ABP 루트를 그대로 공급하거나 회전 모드만 바꿔 동등하다고 처리하지 않는다. 현재 Offset Root CVar 0/1 수동 비교 답변 대기, 코드 수정 전이다. 구면/base 게이트는 미통과다.
- **CVar 비교 결과:** 사용자는 Offset Root 1에서 방향 어긋남 재현·0에서 미재현을 확인했다. 로그에서 14:57:15 KST 0, 14:57:46 1 복원·MCP 최종 값 1을 확인했다. 현재 PIE는 꺼짐. Offset Root 경로와 문제의 연관성을 확인했으며 Steering 누락의 단독 원인을 증명한 것은 아니다. 다음 Steering 보완·급전환 재검증 단위는 사용자 승인 후 착수한다. 적도·base로 시험 범위를 넓히기 전에 이 방향 문제를 해결한다.

## 7. 새 세션 — Steering 보완 단위의 조사 시작점

**이 절은 이전 인계의 조사 시작점이다.** 2026-10-24~25에 아래 조사 후 §8~§10을 구현·검증했다. 현재 새 세션의 시작점은 §11이며 아래 미구현/대기 문구는 당시 상태다.

1. 현재 재현 기준은 이동 중 룩 급회전 뒤 전방 달리기 포즈가 옆/대각선으로 달리는 것이다. Offset Root `1`에서 쉽게 재현·`0`에서 미재현, 마지막 값 `1`, PIE 종료 상태다. 이 비교는 Offset Root 적용 경로와의 연관성을 보여 주며 Steering 누락이 유일한 원인이라는 증명은 아니다.
2. 기존 계약은 `design/GASPReference.md` §2.10의 블렌드 스택 Steering×2·OrientationWarping·ResetRoot·CopyBone 후 OffsetRoot다. 현재 UAF는 최소 MM/Pose History → Offset Root+프로젝트 구면 보정 → Output으로 Steering·OrientationWarping·ResetRoot/CopyBone·발 배치가 빠져 있다. 이번 문제는 우선 Steering/회전 추종 범위에서 조사하고 나머지 누락을 검증 완료로 간주하지 않는다.
3. 설치 엔진 `Plugins/Experimental/UAF/UAFWarping/Source/UAFWarping/Private/SteeringTrait.h/.cpp`: additive Steering은 RootBoneTransform·TargetOrientation·Alpha, 미래 루트모션 추출용 IAttributeProvider·ITimeline을 이용해 회전 루트모션을 Override한다. ProceduralTargetTime·AnimatedTargetTime 등은 shared data 상수이고 latent 목록은 Alpha·TargetOrientation·RootBoneTransform뿐이다. 기존 ABP의 곡선 기반 목표 시간(0.1~1.5초·0.1~0.3초), 이동 Steering의 AnimatedTargetTime=10, TurnInPlace 별도 Steering과 같다고 가정하지 않는다. `UAFPoseSearch/Source/UAFPoseSearch/Private/MotionMatchingTrait.h`와 `UAFAnimGraph/Source/UAFAnimGraph/Internal/Traits/BlendStackTrait.h`에서 UAF의 애니별 보정 위치·인터페이스 전달을 확인한다.
4. UAF 자신의 이전 누적 루트와 현재 UAF 선택 애니·곡선을 기준으로 연결한다. 숨김 ABP는 Chooser 상태와 궤적 공급자로 유지되지만 그 ABP의 가상 루트·선택 DB 태그는 UAF 결과와 달라질 수 있다. `GetSteeringTargetFacing`의 컴포넌트 Up 정렬 규칙을 유지한다. 실제 애니 평가 위치·task 순서와 root motion 속성의 유효성을 확인하고 필요한 최소 변경을 정한다.
5. 구현·빌드·AG/S 컴파일 후 동일 급전환을 수동 비교하고, 해결되면 북극/적도·공중·base 시험을 재개한다. 실제 UAF root의 기울기·Yaw/위치 제한·base 중복을 확인하며 숨김 ABP의 `[AnimB3]`만으로 통과시키지 않는다. 단위마다 진행 확인, GUI/PIE가 빠르면 즉시 수동 요청한다.

인계용 로컬 자료: 재현 녹화 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261024_145134378.utrace`(171,067,599바이트), UAF 검색 비교 `RewindDebugger-LootNPop-Editor-20261023_192247450.utrace`, 구면 스택 전 AG/S 백업 `Saved/AnimationOverhaul/2026-10-23_PreSphericalOffsetRoot_dadd230e/`. Saved는 git 제외라 이 커밋에는 포함되지 않는다. Rewind는 프로젝트 Saved 경로만 사용한다. 1인 기록은 Net Mode=`Play Standalone`, 별도 프로세스 Standalone Game이 아니다.

`TestMap03`은 기존 사용자 변경과 시험용 GameMode Override가 섞여 있어 이번 커밋에서 제외하고 로컬에 유지한다. 새 체크아웃에서는 `BP_GameModeUAFSpike`를 임시 지정해야 한다. 5-1 종료 시 원래 Override를 확인해 복원하되 맵 전체를 되돌리지 않는다. DT_PlayerBaseStats·DT_LongSword_Levels·DA_MassSpawnConfig_TestOnePerSpecies도 이번 작업 밖의 로컬 변경으로 남긴다.

검증: 최신 C++ 빌드 성공(5 actions, 48.13초), AG/S 컴파일·배선 확인과 사용자 CVar 비교까지다. 새 Steering 코드·빌드·급전환 해결은 아직 없다. 구면·적·2인 최종 회귀·5-1 완료·5-2·Go 판정도 남아 있다.

## 8. Steering 보완·현재 시작 지면 급전환 재검증(2026-10-24, 단위 완료)

사용자 진행 요청·저장 후 에디터 종료 답변을 받고 스파이크 C++만 변경했다. 기존 그래프 배선은 유지한다.

- `LNPUAFSpikeRootInputs`에 궤적 사본과 이동 Steering 활성 플래그를 추가한다. 숨김 ABP는 상태·궤적만 공급하며 그 ABP의 누적 루트·선택 DB 태그를 가져오지 않는다.
- 기존 구면 보정 trait가 `IHierarchy`의 forwarding을 통해 `ITimeline`·`IBlendStack`·MM 스택에 접근한다. 실제 `FMotionMatchingTrait` 검색 결과의 DB `TurnInPlace` 태그와 선택 클립의 `SteeringTargetTime` 곡선을 읽는다. 활성 그래프의 FactoryObject가 `UAnimSequence`이며 실제 MM 선택과 일치할 때 같은 활성 timeline의 시간·재생속도·루프 설정으로 미래 루트모션을 **PostEvaluate에서 즉시 샘플링**하고 회전 값 2개만 task에 복사한다. 비시퀀스/불일치/시간 없음은 ready=0이다. UAFPoseSearch 모듈의 Private·Internal 헤더 의존성이 있다. 최초 `IAttributeProvider` 콜백 지연 호출은 아래 크래시 후 제거했다.
- task 순서는 **UAF 입력 포즈 평가 → 기존 base/Up/Yaw 보정 → Steering 회전 루트모션 Override → 기본 Offset Root 소비·포즈 쓰기**다. RootMotion 이동량은 바꾸지 않고 UAF 자신의 보정된 누적 루트 회전과 Up 정렬한 궤적 facing을 비교한다. 두 Steering 채널은 각자 스프링 속도를 가진다. 이동은 애니 루트 속도 10 초과·AnimatedTargetTime=10·절차 시간 0.1~0.3, 제자리 회전은 실제 DB 태그·AnimatedTargetTime=0.2·절차 시간 0.2다. 목표 샘플 시간은 곡선 0~1을 0.1~1.5초로 매핑한다.
- 첫 평가·텔레포트·무효 입력·루트모션 없음·비활성·CVar 끔에서 스프링 잔여 속도를 버린다. 전신 몽타주에 해당하는 회전 Release에서는 적용하지 않는다. `LNP.UAFSpike.Steering`은 기본 1이며 Offset Root를 켠 채 Steering만 비교한다. `LNP.Anim.DebugLocomotion 2`는 `[UAFSteering]`을 약 1초마다 출력한다. ready는 현재 활성 시퀀스/MM 선택 일치·시간·궤적 유효성, applied는 Override 호출 여부다. rootYaw/rootTilt는 **이번 루트모션 소비 전** UAF 상태이며 최종 포즈 수치로 해석하지 않는다. 이전 코드의 callback 바인딩 여부만으로는 캡처 객체 수명이 보장되지 않았다.
- **한계:** 블렌드된 출력에 활성 클립 예측으로 한 번 적용하므로 기존 샘플별 Steering×2와 다르며 기존 0.1초 알파 블렌드도 재현하지 않았다. 이번 급전환 개선 후보이며 B2 전체·미러·전환 블렌드 동등성이나 최종 Go의 근거가 아니다. OrientationWarping·ResetRoot/CopyBone·발 배치도 여전히 별도다.
- 빌드: 첫 시도는 UAFPoseSearch 내부 헤더 경로 누락으로 컴파일 실패했다. Internal 경로 추가 후 기준 권한 확장 Development 빌드 **Succeeded, 14 actions, 54.53초**, 새 경고 없음. diff 공백 검사 통과. 엔진 파일·기본 ABP·맵은 수정하지 않았다.
- AG/S 변경 전 백업은 `Saved/AnimationOverhaul/2026-10-24_PreSteering_020043e9/`이며 두 파일 SHA-256 일치를 확인했다. 재실행 후 사용자 컴파일·저장 완료 보고와 15:51:57 KST S·15:52:01 AG 성공 컴파일 로그를 확인했다. 파일 시각은 이전 그대로라 내용 변경에 따른 저장이 발생했다고 주장하지 않는다. 아래 첫 크래시를 수정한 뒤 최종 재시험을 완료했다.

### 첫 런타임 크래시·수정

- 사용자 Steering 0에서 기존 옆/대각선 달리기가 쉽게 재현, 1에서는 크래시한다고 보고했다. 0 구간 로그는 ready=1·applied=0·rootYaw ±90°·rootTilt 0° 표본이 있다. 16:02:15·16:03:41 KST 로그에서 `SequencePlayer.cpp:78 AnimSequence->IsValidLowLevel()` assertion → 네이티브 루트모션 delegate → 프로젝트 task 호출을 확인했다. 1의 개선을 관찰할 수 없었으므로 통과 처리하지 않는다.
- 콜백 캡처 객체가 무효인 점은 확인했지만 무효화의 정확한 원인(GC 등)은 확정하지 않았다. assertion을 끄거나 무시하지 않고 **네이티브 콜백 지연 호출·task 내 delegate 자체를 제거**했다. 현재 구현은 위의 활성 시퀀스 직접 샘플·회전 값 전달이다. 엔진 파일 변경 없음. 첫/두 번째 크래시 로그는 `Saved/AnimationOverhaul/2026-10-24_SteeringCallbackCrash/`에 복사 보존했다. 원래 Saved/Crashes의 dump는 유지한다.
- 에디터 프로세스 종료 확인 후 기준 권한 확장 Development 빌드 성공(5 actions, 21.48초, 새 경고 없음). 공유 입력·그래프 배선은 추가 변경하지 않았다. 수정 후 크래시 소실·Steering 적용은 아직 미검증이다.

### 수정 후 최종 재시험 결과

- 사용자는 재실행 뒤 8방향 달리기·정지·방향 전환 등을 시험했고 어색한 부분이 없었다고 보고했다. 16:10:35~16:10:48 KST의 Offset Root=1·Steering=1·디버그=2 명령, 16:11:02 inputs=ready를 확인했다.
- 16:11:03~16:12:08 KST 약 65초의 `[UAFSteering]` 66개 모두 ready=1, 56개 applied=1. 실제 F/B/LL/RL 이동·Starts·Sprint 클립을 읽었고 이동 중 회전 루트모션 Override 실행을 확인했다. 이번 실행 assertion/fatal·구면 입력/스택 설정 오류는 0건이다. 기존 FoleyEvent 미처리 경고는 남았다.
- 약 1초 간격의 **소비 전 상태 표본**에서 rootTilt 모두 0°·|rootYaw| 최대 58.1°·|deltaYaw| 최대 6.33°다. 전 프레임 최대값·최종 포즈 오차 상한·정밀 구면/base 동등성으로 일반화하지 않는다. tip=1 표본은 0개라 제자리 회전 채널의 실제 적용은 별도 확인이 필요하다.
- 결과 확인 시 MCP PIE=false, Steering=1·Offset Root Enable=1/Enabled=true. 성공 로그는 `Saved/AnimationOverhaul/2026-10-24_SteeringVerified/LootNPop.log`에 복사했다. 이번 결과 정리에서 C++·에셋 변경이나 재빌드 없음.

**이번 단위 완료:** 현재 시작 지면의 이동 중 방향 추종 문제 개선·콜백 제거 후 해당 assertion 미재발·실제 Steering 적용 확인. **다음 제안 단위는 북극·적도 및 움직이는 base의 구면 루트 검증이며 승인 전이다.** 위치/최종 UAF 루트 수치 경로를 정하고 직접 플레이가 빠르면 수동 요청한다. 제자리 회전 채널·샘플별 보정 동등성·공중 고정·발 배치 등 미검증 범위는 유지한다. 5-1 미완료·5-2 미착수·Go/No-Go 보류다.

## 9. 북극·적도/base 구면 루트 검증(2026-10-24~25, 관찰/측정 단위 완료·수치 미통과)

사용자가 진행을 승인했고 시험 지점으로 직접 이동·패널 탑승할 수 있다고 답했다. 자동 이동/테스트 장치를 만들지 않는다. 편집 월드에는 PlayerStart 4개를 조회했고 런타임 패널/마커 조회는 빈 배열이었다. 런타임 생성 패널의 부재로 해석하지 않는다.

### 진단 보완

- 기존 `[UAFSteering]`은 소비 전 상태의 약 1초 간격 표본이다. 최종 Up·30cm 제한·base 중복 검증에 그대로 사용하지 않는다.
- 기존 prepare task → 기본 Offset Root task → 새 `FLNPInspectSphericalOffsetRootTask` 순서로, 기본 task가 쓴 누적 상태와 같은 VM keyframe의 실제 root를 읽는다. 출력 root는 그 프레임의 메시 월드 변환으로 옮긴다. 포즈/상태를 수정하지 않으며 숨김 ABP 포즈를 읽지 않는다.
- `LNP.Anim.DebugLocomotion 2`일 때만 매 평가 프레임을 모아 약 1초마다 `[UAFRoot]` 출력. `maxStateCm/Yaw/Tilt`는 기본 Offset Root의 소비/감쇠/클램프 후 누적 상태, `maxPoseCm/Yaw/Tilt`는 실제 출력 루트다. cm는 같은 프레임의 메시 원점과 거리, Yaw는 메시 로컬의 절댓값, Tilt는 메시 Up과 루트 Up의 각도다. 현재 그래프에서는 Offset Root 뒤 다른 포즈 노드가 없어 이 지점이 AG 최종 루트지만 이후 후처리를 넣으면 재확인한다.
- `frames/moving/ground/based/resets`는 해당 구간의 평가 횟수와 조건별 횟수다. moving은 이동 Steering 활성 플래그(공중도 포함)이므로 정지 탑승은 `ground=frames`, `moving=0`, `based=frames`, `resets=0`인 안정 구간으로 좁혀 본다. 구간이 섞이면 마지막 상태만으로 정지 탑승이라 분류하지 않는다.
- lat·upZ·pos·base는 구간 마지막 프레임의 위도(원점 기준)·메시 Up.Z·좌표·base 컴포넌트 경로다. 북극은 +Z 축 부근, 적도는 Z≈0인지 실제 좌표/위도로 확인한다. `maxBaseCm/Deg`는 공급된 프레임별 base 운반 델타의 구간 최대값이며 이 값만으로 중복 소비가 없음을 증명하지 않는다. 공유 RootInputs에 base 경로와 동적 여부만 추가했다.
- 성공 조건은 시험 위치·실제 base 탑승/이동 조건 확인, 최종 루트 Up 정렬과 기존 30cm 상태 제한 유지, 정지 패널 탑승에서 앞선 ±30cm 이중 운반 패턴이 없고 반복 운반 뒤 계속 밀림/급회전/기울어짐이 없는 것이다. 출력 루트와 누적 상태의 차이가 있으면 원본 입력 root 포즈와 분리해 조사하며 state 거리만으로 표시 포즈까지 통과 처리하지 않는다. 이동/회전 base 델타를 각각 실제로 관찰한 범위만 판정한다.

### 빌드·현재 수동 요청

사용자 저장·에디터 종료 답변 후 권한 확장 기준 Development 빌드를 수행했다. 첫 빌드는 동명 `FReferencePose` 충돌로 실패했고 `UE::UAF::FReferencePose` 명시 후 **Succeeded, 4 actions, 11.68초**, 새 경고 없음. diff 공백 검사 통과. 스파이크 파일 4개만 추가 변경했고 엔진·기본 ABP·맵은 바꾸지 않았다.

에디터 재실행 → AG/S 각각 Compile·Save → 1인 Standalone PIE·Offset Root=1·Steering=1·디버그=2 → 북극(+Z 축 부근) 수동 시험을 요청했다. 새 배선은 없다. 아래 결과를 확인했고 현재 적도 시험 답변 대기다.

### 북극 결과와 현재 요청(2026-10-25)

- 사용자 북극 근처 이동 관찰에서 어색한 부분 없음 보고. 12:01:19 KST AG·12:01:22 S 성공 컴파일과 12:01:47~12:02:00 세 CVar 명령을 확인했다.
- 구간 끝 위도 80.59~89.93°인 `[UAFRoot]` 135개 구간에 8,086 평가 프레임(ground 7,952·based 0·reset 0)이 포함됐다. 각 구간의 매 프레임 최대값을 집계해 누적 상태와 실제 출력 모두 거리 최대 30.000cm·Yaw 최대 92.40°·Tilt 최대 0.055°를 확인했다. lat은 구간 끝 좌표이므로 각 프레임의 위도 범위를 측정했다고 주장하지 않는다. 공중 포함 구간을 순수 지상 최대값으로 해석하지 않는다.
- 92.40°는 12:04:48 KST 지상 구간(60프레임·moving 24), Tilt 0.055°는 12:04:22 지상 구간(60프레임·moving 60)에서 나왔다. 현재 보정은 **소비 전** ±90° 클램프이고 기본 task가 그 뒤 애니/Steering 루트모션을 더하므로 평가 후 엄밀한 ±90° 상한은 보장되지 않는다. 관찰 이상 없음과 별개로 이 상한을 통과 처리하지 않고 적도 결과와 함께 검토한다. 이번 결과 정리에서는 코드를 바꾸지 않았다.
- 완전 정지 지상 구간은 9개. 마지막 12:06:17 거리 최대 0.055cm → 12:06:18~19 0.000cm로 수렴했다. 12:06:06의 `TurnInPlace` DB·045_L에서 ready=1·applied=1·tip=1을 확인해 제자리 회전 채널의 실제 실행 근거가 생겼다. 전체 실행 assertion/fatal 0건. Foley 미처리 경고 등 잔류 경고를 전체 0으로 표현하지 않는다.
- 로그는 `Saved/AnimationOverhaul/2026-10-25_NorthRoot/LootNPop.log`에 복사했다. MCP에서 PIE 활성·Steering=1·Offset Root Enable=1/Enabled=true 확인. 일시정지 여부는 이 API만으로 확인하지 않았다.

### 적도 결과와 현재 요청(2026-10-25)

- 사용자 적도 부근에서도 비슷하게 플레이했고 어색한 부분이 없었다고 보고했다. 북극 결과 확인 전 지나간 적도 표본과 분리하기 위해 12:06:19 KST 이후·구간 끝 |lat|≤5°인 구간으로 집계했다.
- 12:08:50~12:11:18 KST 접근/시험 구간은 끝 위도 −1.97~4.97°·138구간·8,143평가 프레임, 모두 지상·base/reset 0. 누적 상태/실제 출력 모두 거리 최대 30.000cm·Yaw 최대 90.00°·Tilt 최대 1.613°다. 완전 정지 지상 구간 39개, 마지막 12:11:11~18 거리 최대 0.000cm·Tilt 0.000°로 수렴했다. 구간 끝 위도 기준이라 모든 프레임의 위도를 측정한 것은 아니다.
- Tilt 1.613° 피크는 12:10:02 KST(60프레임 모두 이동·지상·base 없음), 1.454°는 12:09:11에 기록됐다. 가까운 시점의 Steering 표본은 Sprint Loop지만 **최대 Tilt가 난 동일 프레임의 클립/원인은 아직 특정하지 않았다.** 현재 최대값 로그만으로 원본 애니 회전·Steering 스프링·기타 시점 차이를 단일 원인으로 확정하지 않는다. 사용자 관찰 이상 없음과 분리해 최종 Up 완전 정렬은 보류한다.
- 전체 실행 assertion/fatal 0건, MCP PIE 활성·Steering=1 확인. 로그는 `Saved/AnimationOverhaul/2026-10-25_EquatorRoot/LootNPop.log`에 복사했다. 이번 결과 확인에서 코드/에셋·설정 변경·빌드 없음.

**현재 수동 요청:** 같은 PIE·세 CVar 유지, 이동패널 위 이동/룩 입력 없이 왕복 1회 이상(최소 30초), 이후 걷기→정지·룩 방향 전환 약 10초. 왕복 완료 여부와 앞서감/밀림/기울어짐/급회전을 보고하고 PIE 일시정지. base 수치 확인 후 두 지역의 Yaw/Tilt 피크도 검토한다. 이 단위 미완료·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 패널 결과·이번 단위 판정(2026-10-25)

- 사용자는 요청한 패널 시험을 완료했고 어색한 부분이 없었다고 보고했다. 실제 base는 `BP_PyramidExteriorPanel_C_3.PanelMesh`, 동적 base 탑승 177구간·10,327 based 프레임을 확인했다. 컴포넌트 조회에서도 해당 PanelMesh를 확인했다. 로그는 `Saved/AnimationOverhaul/2026-10-25_PanelRoot/LootNPop.log`에 보존했다.
- `based=ground=frames`, moving=0, reset=0인 이동 기준 정지 탑승은 107구간·6,332프레임. 누적 상태/실제 출력 모두 거리 최대 0.901cm, 마지막 구간은 0.000cm였다. 관찰한 범위에서 이전 ±30cm 이중 운반 패턴은 재현되지 않았다. moving 플래그만으로 룩 입력까지 없었다고 증명하지는 않으며 요청한 무입력 탑승은 사용자 시험 보고와 함께 해석한다.
- 정지 구간 Tilt 최대 **5.196°**, 이동 포함 based 구간 Tilt 최대 **10.593°**, 전체 based 거리 최대 30cm·Yaw 최대 90°다. 정지 구간의 5.196°는 12:17:02 KST, 이동 구간 10.593°는 12:16:53에 기록됐다. 사용자 시각 관찰 이상 없음과 별개로 최종 Up 정렬은 미통과다.
- base 프레임 델타 최대 38.071cm·0.0028°다. 병진 운반은 확인했으나 큰 각도로 회전하는 base의 동등성까지 통과 처리하지 않는다. 전체 실행 assertion/fatal 0건, MCP PIE 활성 확인. 일시정지 자체는 이 API로 확인하지 않았다.
- 소스 재대조: 프로젝트는 기본 task **전**에 base 운반·Up 정렬·±90° 제한을 적용한다. 기본 task가 그 뒤 `ConsumedRootMotionDelta * SimulatedTransform`을 수행하며 회전 Accumulate에서 root rotation을 소비한다. 기본 회전 클램프는 구면 곱셈 문제 때문에 −1로 꺼 두었다. 따라서 현재 순서만으로 최종 Up/±90°가 보장되지 않는 구조를 확인했다. 피크가 난 동일 프레임의 세부 루트모션/스프링 원인은 아직 계측하지 않았다.

**관찰·측정 단위는 완료, 구면 수치는 미통과.** 북극·적도·패널에 대해 사용자 관찰과 실제 UAF 출력을 확보했고, 패널 병진의 이중 운반 패턴은 미재현이다. 최종 Up/Yaw 보완과 큰 각도 회전 base 등은 남긴다. 5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### LogAnimation 경고 확인

사용자가 다수 경고의 영향을 질문했다. 확인 시점 LogAnimation Warning 1,997건은 모두 UAF 시스템의 미구현 Foley 이벤트다(Run 1,846·Walk 83·Jump 36·RunStrafe 32), LogAnimation Error 0건. 설치 소스 `UAF/Private/Script/UAFRigVMComponent.cpp:79~83`은 이벤트 엔트리가 없으면 경고를 기록하고 그 호출을 반환한다. 해당 Foley 연출이 실행되지 않지만 현재 이동/포즈 검증을 중단할 사유는 아니다. 로그 비용·누락 연출은 최종 구성 전에 정리해야 하며 루트 기울기와의 인과는 확인되지 않았다. 이번 확인에서 이벤트·소스 에셋·로그 카테고리를 수정하지 않았다.

## 10. 소비 후 구면 제약 보완(2026-10-25, 단위 완료)

1. 기본 Offset Root 소비 후에도 Up 정렬·컴포넌트 Up 둘레 ±90° 제약이 유지되도록 최소 프로젝트 보정 경로를 정한다. 원본 애니의 의도된 포즈 회전과 누적 root 제약을 구분한다.
2. **같은 UAF 누적 상태와 실제 출력 root를 일관되게** 보정한다. 숨김 ABP 루트 대입·표시 포즈만 덮기·회전 Release로 우회하지 않는다. 기존 base 운반·위치/모드·CVar/알파 계약과 게임플레이 분리는 유지한다. 엔진 파일은 수정하지 않는다.
3. 빌드/컴파일 후 동일 북극·적도·패널 수동 회귀와 최종 수치로 검증한다. 직접 조작이 빠르면 즉시 요청한다. 큰 각도 회전 base는 관찰한 델타 범위와 분리한다.

사용자 진행 승인·PIE 종료/저장/에디터 종료 답변 후 아래를 구현했다. 빌드·수치 회귀 시험과 아래 최종 수동 재시험까지 마쳐 이 단위는 완료했다.

### 구현

- `LNPSphericalOffsetRootTrait.h/.cpp`에 `FLNPFinalizeSphericalOffsetRootTask`를 추가했다. 실행 순서는 준비(base 운반/기존 회전 제약·Steering) → 기본 Offset Root(루트모션 소비/감쇠/위치 제한/포즈 쓰기) → **최종 회전 보정** → 기존 진단이다.
- 준비/최종 보정에 같은 구면 회전 함수를 사용한다. 컴포넌트 Up으로 스윙 정렬 후 그 컴포넌트의 로컬 Z Yaw를 ±90°로 제한한다. 월드 Z를 회전축으로 쓰지 않는다.
- 최종 보정은 누적 상태의 이전 회전 S와 보정 회전 C, 컴포넌트 회전 M으로부터 `M⁻¹·C·S⁻¹·M`을 구해 출력 root의 로컬 회전에 곱한다. 상태만 고치거나 출력만 덮지 않는다. 원본 포즈가 상태에 대해 가진 상대 회전은 보존하며, root 위치/스케일·누적 위치·base 델타·루트모션 속성은 변경하지 않는다. 원본 입력 root 자체에 의도된 기울기/Yaw가 있다면 그 부분까지 무조건 평평하게 만들지 않는다. 최종 누적 상태와 실제 포즈의 진단을 계속 분리한다.
- 디버그 꺼짐에서도 보정한다. Offset Root 비활성에서는 task를 넣지 않으며, 무효 입력·초기화/기본 task 미실행·알파 0·비양수 dt·루트모션 속성 없음에서는 기존 출력을 유지한다. 숨김 ABP 포즈/루트·기본 플레이어·엔진 파일·base 운반 로직은 변경하지 않았다.

### 빌드·회귀 시험

- 기준 권한 확장 Development 빌드 **Succeeded, 5 actions, 37.65초**, 새 컴파일 경고 없음. diff 공백 검사 통과.
- `LootNPop.Animation.UAFSpike.SphericalRootFinalization` 자동화 시험을 같은 C++ 파일의 개발 시험 조건부 구간에 추가했다. +Z/+X/−Z/기울어진 컴포넌트 방향과 Yaw 양/음 경계에서 관찰한 10.593° Tilt·±92.4° Yaw를 수치로 재현한다. 보정 후 상태 Up·±90° 경계, 입력 root의 비영 상대 회전 및 위치/스케일 보존, 이미 제약 안에 있는 출력의 불변성을 검증한다.
- `/Engine/Maps/Entry`·NullRHI headless UnrealEditor-Cmd에서 실제 시험 실행 **1/1 성공·실패 0·종료 코드 0**, 시험 시간 약 0.02초. 보고서 `Saved/Automation/2026-10-25_UAFRootFinalization/index.json`, 로그 `Saved/Logs/UAFRootFinalizationTest_2026-10-25.log`. 회전 계산 시험이며 그래프/네트워크/PIE 실행 통과는 아니다.

### 최종 실행 재검증

- 사용자 재실행·컴파일/저장 및 요청한 북극·적도·패널 시험을 완료했고 어색한 부분이 없었다고 보고했다. 12:43:40 KST S·12:43:44 AG 성공 컴파일, 12:44:07~17 세 CVar 실행을 확인했다. 새 배선 없음.
- 12:44:40~12:50:59 KST 기록된 378구간·22,583평가 프레임 전체에서 누적 상태와 실제 출력 모두 거리 최대 30cm·Yaw 최대 90°·Tilt 최대 **0.000°(로그 해상도)**였다. 구간 내부는 매 평가 프레임을 집계한다. 로그가 없는 프레임이나 임의의 원본 애니/조건까지 일반화하지 않는다.

| 구간(구간 끝 위도/조건 기준) | 구간 수 / 평가 프레임 | 위도 범위 | 거리 최대(상태/출력) | Yaw 최대(상태/출력) | Tilt 최대(상태/출력) |
|:---|:---|:---|:---|:---|:---|
| 북극, lat≥80°·based=0 | 65 / 3,882 | 81.20~89.88° | 30cm | 87.70° | 0.000° |
| 적도, \|lat\|≤5°·based=0 | 73 / 4,372 | −2.89~4.79° | 30cm | 90.00° | 0.000° |
| 동적 base 포함 | 118 / 7,036 | 4.19~7.98° | 30cm | 87.38° | 0.000° |
| 정지 탑승(ground=based=frames, moving=reset=0) | 70 / 4,192 | 5.30~6.02° | 0.345cm | 87.14° | 0.000° |

- 동적 base 프레임은 실제 7,009개(구간 경계의 비탑승 프레임 포함 평가 합계 7,036과 구분). 패널 정지 탑승의 마지막 거리 최대는 0.000cm. 이전의 거리 이중 운반과 새로운 회전 보정 부작용은 이 관찰 범위에서 재현되지 않았다. base 델타 최대 75.336cm·0.001°로 큰 각도의 회전 base는 여전히 미검증이다.
- 제자리 회전 Steering 적용 표본 12개, assertion/fatal·구면 입력/스택 설정 오류 0건. LogAnimation Error 0건, 기존 Foley 미처리 Warning은 1,361건 남았다. 이 단위에서 경고를 억제하거나 Foley를 구현하지 않았다.
- 로그는 `Saved/AnimationOverhaul/2026-10-25_FinalRootVerified/LootNPop.log`에 복사 보존했다. MCP PIE 활성·Steering=1·Offset Root Enable=1/Enabled=true 확인. 일시정지는 별도 확인하지 않았다. 결과 정리에서 코드/에셋 변경·재빌드 없음.

**이번 구현·재검증 단위 완료.** 이전 북극 Yaw 92.4°·적도 Tilt 1.613°·패널 Tilt 10.593° 문제는 현재 구성/시험 범위의 최종 상태와 출력에서 개선을 확인했다. 다음 제안은 공중 Fall_Loop 고정(AN-007)의 UAF 경로 조사·구현·수동 검증이며 사용자 승인 전 착수하지 않는다. 샘플별 보정 동등성·큰 회전 base·OrientationWarping/ResetRoot/CopyBone·발 배치·입력/전환·적/네트워크는 남았다. 5-1 미완료·5-2 미착수·Go/No-Go 보류다.

## 11. 새 세션 인계(2026-10-25)

> 아래는 공중 고정 착수 전의 기준 인계다. 현재 진행은 §12를 따른다.

사용자가 다음 작업을 새 세션으로 넘기고 문서 정리·커밋을 요청했다. 이번 세션에서는 다음 공중 고정 단위에 착수하지 않는다.

### 시작할 단위

**공중 Fall_Loop 고정(AN-007)의 UAF 경로 조사·구현·수동 검증.** 새 세션에서 진행 확인 후 다음 순서로 좁힌다.

1. `Decisions.md` AN-007과 [TechDesign_CombatAnimation.md](../../TechDesign_CombatAnimation.md) §5.5, 기존 `ABP_Main_UEFN`의 공중 포즈 계약을 읽는다. 현재 테스트 AG는 공중에도 MM 출력을 쓰므로 기존 공중 고정과 동등하지 않다. GASP 점프/낙하 체계로 교체하지 않는다.
2. 기존 UEFN 리타깃 Fall_Loop 소스의 실제 참조/존재와 UAF에서 고정 포즈를 선택할 최소 경로를 확인한다. `Retargeted/Lyra/`는 git 제외이며 새 환경은 README §4·Phase02의 재생성 절차가 필요하다. 소스 에셋을 수정/저장하지 않는다.
3. `/Game/Characters/UAFSpike/`와 스파이크 C++ 범위에서 연결하고 공중 진입/유지·착지 복귀를 확인한다. 그래프 조작·점프/착지가 더 빠르면 바로 수동 요청한다. 이 단위가 끝나면 다시 다음 단위 진행 여부를 묻는다.

### 재사용할 현재 구성·검증

- 에셋은 `ABP_SourceUAFSpike`·`BP_PlayerUAFSpike`·`BP_GameModeUAFSpike`·`S_LocomotionSpike`·`AG_LocomotionSpike`. S locator=`Current Actor → VisualMesh`, AG=MM/Pose History → Offset Root Bone+LNP Spherical Offset Root → Output. 공유 RootInputs 전체 핀 배선은 이미 있고 이번 커밋의 새 에셋 변경은 없다.
- `LNPSphericalOffsetRootTrait`는 준비(base/Up/Yaw·Steering) → 기본 Offset Root → 최종 회전 보정 → 선택적 진단을 실행한다. Steering은 활성 UAF 시퀀스/시간/DB 태그 기준이며 네이티브 루트모션 콜백을 지연 호출하지 않는다. 엔진 UAFWarping·UAFPoseSearch 내부 헤더 의존성이 있어 엔진 갱신 때 재대조해야 한다.
- 최신 코드 빌드 5 actions·37.65초 성공, 실제 C++ 수치 회귀 1/1 성공. AG/S 컴파일과 사용자 북극·적도·패널 재시험 관찰 이상 없음. §10의 22,583평가 프레임에서 최종 Tilt 0.000°·Yaw 최대 90°·거리 최대 30cm, 패널 정지 탑승 최대 0.345cm. 새 코드 변경이 없으면 기준 빌드를 반복하지 않는다.
- 직접 플레이/패널 탑승은 사용자가 할 수 있다. 진단은 `a.UAF.OffsetRootBone.Enable 1`, `LNP.UAFSpike.Steering 1`, `LNP.Anim.DebugLocomotion 2`. `[UAFSteering]`은 소비 전 표본, `[UAFRoot]`는 최종 상태/출력의 구간 내 매 평가 최대값이다. 숨김 ABP `[AnimB3]`와 소유권이 다르다. Rewind 녹화가 필요하면 1인 PIE Net Mode=`Play Standalone`, 프로젝트 Saved 경로만 사용한다.
- 인계 확인 시 PIE는 활성이다. 일시정지 여부는 별도 확인하지 않았고 에디터/PIE를 변경하지 않았다. 다음 편집 전 현재 상태를 다시 확인한다. TestMap03은 테스트 GameMode Override가 로컬에 남아 있으며 다른 체크아웃에서는 수동 재지정해야 한다. 원래 Override의 구체 값은 기록이 없어 5-1 종료 복원 전에 사용자 기준값을 확인하고 맵 전체를 되돌리지 않는다.

### 한계·로컬 자료·커밋 범위

- 5-1 전체 미완료·5-2 미착수·Go/No-Go 보류. 공중 고정·발 배치·OrientationWarping/ResetRoot/CopyBone·입력 PSD 배열 대조·전환/인터럽트 전체·샘플별 보정 동등성·큰 각도 회전 base·적/2인 등은 미검증이다. 숨김 ABP와 UAF가 함께 돌아 성능 비교도 하지 않는다.
- FoleyEvent 미구현 경고는 해당 호출을 건너뛴다. 최종 시험에서 LogAnimation Warning 1,361·Error 0건이었고 경고 억제/이벤트 구현은 하지 않았다. 현재 로코모션 검증은 가능하나 누락 연출과 로그 정리는 남아 있다.
- 최종 로그 `Saved/AnimationOverhaul/2026-10-25_FinalRootVerified/LootNPop.log`, 수치 시험 보고서 `Saved/Automation/2026-10-25_UAFRootFinalization/index.json`, 별도 시험 로그 `Saved/Logs/UAFRootFinalizationTest_2026-10-25.log`. 중간 크래시·북극·적도·패널 로그와 이전 AG/S 백업도 Saved에 보존했다. 모두 git 제외이며 새 체크아웃에는 포함되지 않는다.
- 이번 커밋은 스파이크 C++/Build.cs 5개와 Phase 5 문서 5개다. 기존 사용자 변경인 TestMap03·DT_PlayerBaseStats·DT_LongSword_Levels·DA_MassSpawnConfig_TestOnePerSpecies는 커밋에서 제외하고 로컬에 둔다. GASP 소스/엔진/기본 ABP/맵 수정은 없다.

## 12. 공중 Fall_Loop 고정 경로(2026-10-25, 단위 완료)

사용자 Phase 5 진행 요청으로 착수했고, 아래 마지막 완료 판정까지 구현·검증을 마쳤다. 중간의 대기 문구는 진행 이력이다. **공중 고정 단위만 완료이며 5-1 전체·Go/No-Go는 보류, 5-2 미착수다.**

### 확인한 계약과 구현 경로

- 기존 `ABP_Main_UEFN`의 Sequence Player를 MCP로 조회했다. 실제 참조는 `/Game/Characters/Retargeted/Lyra/MM_Unarmed_Jump_Fall_Loop_UEFN`, 재생속도 1·시작 0·루프 켬이다. Bool은 지상 MM/공중 고정 포즈를 양방향 0.25초로 전환한다. 해당 클립의 에셋 레지스트리 존재도 확인했다. AN-007대로 공중 전체를 고정하며 GASP 점프 체계로 교체하지 않는다.
- AG 목표 구성: `MM → True / Fall_Loop Sequence Player → False`의 `Blend By Bool` → 기존 `Offset Root Bone + LNP Spherical Offset Root` → Output. Condition은 이미 공급하는 공유 `OffsetRoot.bOnGround`, Sequence Player는 `ForceLoop`·속도 1·시작 0, Bool의 Blend Times는 `[0.25, 0.25]`, Always Update True Child는 끔이다. 새 공유 변수·기본 ABP 변경은 필요 없다.
- Pose History는 MM 스택에서 Bool 스택의 Blend Smoother 다음으로 재배치한다. 본 4개(`foot_l, foot_r, pelvis, spine_05`)·`Phase` 커브 및 기존 다른 설정/입력은 유지한다. 지상·공중의 최종 혼합 포즈를 수집하고 MM이 공중에서 비활성이어도 히스토리 수명을 유지하려는 것이다. 엔진 `HistoryCollectorTrait.cpp`는 Update에서 scoped `IPoseHistory`를 자식에 공급하고, PostEvaluate에서 하위 평가 뒤 수집한다.
- 엔진 기본 `Blend By Bool`은 자식이 둘이라 `IHierarchy`의 기본 인터페이스 forwarding이 중단된다. `LNPSphericalOffsetRootTrait.cpp`의 Steering 조회를 보완했다. 기존 직접 MM 경로를 먼저 찾고, 실패하면 `IDiscreteBlend`의 목적 자식에서 MM 스택을 찾은 뒤 그 스택의 활성 클립/시간을 함께 읽는다. 공중 Sequence Player를 이전 지상 MM 결과와 혼합해 Steering하지 않는다. 구면 보정/task·공유 입력/소스 에셋은 변경하지 않았다. 공개 인터페이스만 추가 사용하며 새 엔진 내부 헤더 의존성은 없다.

### 현재 수동 작업과 검증

- MCP에서 PIE 활성 확인 → 사용자에게 수동 종료 요청 → 종료 답변과 `IsPIERunning=false` 확인. AG가 dirty여서 해당 AG만 저장 성공 후 `Saved/AnimationOverhaul/2026-10-25_131403_PreAirLoop/AG_LocomotionSpike.uasset`에 파일 백업했다. 다른 사용자 에셋을 저장/되돌리지 않았다.
- UAF 전용 MCP 그래프 편집 도구가 없어 수동 배선을 바로 요청했다. 사용자는 1~5단계까지 한 것 같다고 보고했고, 6단계의 trait 이동 의미를 질문했다. “이동”은 노드 위치 이동이 아니라 **Bool 안에 Pose History를 새로 추가·동일 설정 복사 후 MM의 기존 trait 제거**라고 정정해 안내했다. 현재 재배치·Compile/Save 완료 답변 대기다. 실제 배선 전체·공중 루프 실행은 아직 검증하지 않았다.
- C++ 수정은 한 cpp 파일, diff 공백 검사는 통과했다. **컴파일·실행 전**이며 배선 완료 뒤 Live Coding과 그래프 컴파일 결과를 확인한다. 새 반영으로 지상 Steering 조회가 유지되는지 로그로 확인하고 공중 진입/장기 유지·착지 복귀를 북극/적도에서 수동 시험한다. 필요 시 Rewind의 실제 UAF Sequence Player와 지상 MM 복귀를 확인한다. 기존 구면 수치 시험은 회전 계산의 기준이지 이번 그래프 경로 통과 근거가 아니다.

### 배선 완료·Live Coding 확인(13:26 KST)

- 사용자가 Pose History 재배치·Compile/Save 완료를 보고했다. 로그에서 13:26:05~06 AG 성공 컴파일과 13:26:07 저장을 확인했다. 배선·설정 전체를 MCP로 직접 덤프한 것은 아니다.
- PIE 비활성을 확인한 뒤 `LiveCodingToolset.CompileLiveCoding` 실행 결과 **Success**. 13:26:47 AG 및 S 재컴파일도 성공했다. 새 C++ 타입·공유 변수 변경이 없어 Live Coding으로 반영했으며 전체 Development 빌드를 반복하지 않았다. 기존 구면 계산 시험을 이번 조회 경로 검증으로 간주하지 않는다.
- 사용자에게 1인 PIE Standalone과 기존 세 CVar(Offset Root=1, Steering=1, DebugLocomotion=2)로 북극/적도 지상 이동·급반전·정지 룩 회전, 제자리/이동 점프, 활공 또는 그래플 5초 이상 체공, 착지 후 이동을 요청했다. 마지막 지상 5초 정지 후 일시정지·이상/사용 기믹 보고를 기다린다. 아직 실제 공중 루프·지상 Steering 복귀·구면 수치를 통과 처리하지 않는다.

### 첫 PIE 회귀 보고·확인된 입력 문제

- 사용자는 북극/적도에 도착하기 전 정면 달리기 모션이 이상하고, 점프 착지 후 잠시 옆 걷기를 재생한 뒤 정면으로 돌아온다고 보고했다. 지역별 공중 시험은 미완료이며 이번 단위를 통과 처리하지 않는다. MCP에서는 PIE 종료를 확인했다.
- 13:28 KST 실행에서 `Phase` 커브를 입력 IPoseHistory에서 찾지 못하는 경고 **794건**을 확인했다. 기존 Foley 경고와 다른 새 회귀다. 같은 실행의 UAFSteering 34개 표본에 실제 지상 DB/클립·시간과 ready=1이 기록돼 새 목적 자식 경유 조회는 지상에서 작동했다. 이것만으로 모든 전환·공중 분기·Steering 동등성을 증명하지 않는다.
- 엔진 `HistoryCollectorTrait.cpp`는 Generate Trajectory가 꺼져 있으면 해당 trait의 `Trajectory` 입력을 그대로 질의용 히스토리에 공급한다. 재배치한 trait는 기존 MM 스택의 핀 연결을 자동으로 물려받지 않는다. 새 Bool Pose History의 Phase 배열·공유 Trajectory 연결·Generate Trajectory 끔·속도 배율 1·본 4개와 Bool Condition을 수동 확인/보완하도록 요청했다. **Phase 누락은 로그로 확인했으며, 궤적 미연결 여부와 착지 회귀의 전체 원인은 아직 확인 전**이다.
- 이 확인에서는 C++를 추가 수정/컴파일하지 않았다. 로그는 `Saved/AnimationOverhaul/2026-10-25_AirLoopRegression/LootNPop.log`에 보존했다. 설정 확인 뒤 현재 위치에서 짧은 지상 달리기·점프/착지 재시험으로 좁히며 지역 순회를 반복하지 않는다.

### 설정 보완·짧은 재시험 결과(13:37 KST)

- 사용자가 Collected Curves의 Phase 누락을 확인해 입력했다. 공유 Trajectory는 새 Bool Pose History에 연결했고 Generate Trajectory는 꺼짐·속도 배율 1·본 4개 정상이라고 보고했다. **기존 MM 스택의 Trajectory 핀은 Pose History 소유이므로 해당 trait 제거 후 사라지는 것이 정상**이다. MM 연결을 유지하라는 안내를 정정했다. 보완 이전 궤적 연결 누락까지 확정한 것은 아니다. 13:36:51~52 AG 컴파일/저장 성공 확인.
- 현재 위치에서 짧은 정면 달리기·이동 점프/착지를 재시험했고 사용자는 **두 증상 모두 사라짐**을 보고했다. 기록은 남극 인근(구간 끝 위도 −87.21~−67.46°)이며 북극/적도 시험으로 분류하지 않는다.
- 13:37:21~41의 21구간·1,247평가 프레임(ground 1,015·공중 232)에서 상태/출력 모두 거리 최대 30cm·Yaw 최대 21.28°·Tilt 최대 0.000°(로그 해상도). 새 실행의 Phase 경고·LogPoseSearch Warning/Error·LogAnimation Error·assertion/fatal 0건, 기존 Foley 경고는 남는다. 1초 표본의 Steering applied=1 12개·공중 ready=0/applied=0 4개와 착지 후 실제 Lands→Run DB/클립 복귀를 확인했다. 모든 공중 프레임의 클립 고정이나 5초 이상 체공을 증명한 것은 아니다.
- 로그는 `Saved/AnimationOverhaul/2026-10-25_AirLoopShortVerified/LootNPop.log`에 보존했다. 확인 시 PIE는 종료됐으며 세 CVar는 Offset Root=1/Steering=1/Debug=2였다. 코드 추가 변경·재컴파일 없음.
- 같은 구현 단위의 남은 시험으로 스폰 근처 긴 체공 5초 이상·적도 점프/긴 체공/착지·이동, 가능하면 Rewind 녹화를 요청했다. 실제 UAF Fall_Loop 재생과 지상 복귀를 별도로 추적한다. 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 긴 체공·적도 녹화 시험의 로그 확인(13:39~13:42 KST)

- 사용자가 요청한 시험·녹화 완료와 관찰 이상 없음을 보고했다. 새 Trace는 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261025_133940695.utrace`(521,755,801바이트), 로그는 `Saved/AnimationOverhaul/2026-10-25_AirLoopRecorded/LootNPop.log`에 보존했다. 기본 플레이어·엔진·소스 에셋 변경이나 재컴파일는 없다.
- 13:39 이후~13:42:55 로그의 205구간·10,163평가 프레임(ground 6,997·air 3,166)에서 상태/출력 모두 거리 최대 30cm·Yaw 최대 90°·Tilt 최대 0.000°(로그 해상도). 구간 끝 위도는 −87.21~51.05°다. 구간 안에서는 매 평가 프레임을 집계하지만 위도는 마지막 값이므로 전 프레임의 위치 범위와 동일시하지 않는다.

| 구간 끝 조건 | 구간 수 / 평가 프레임 | 지상 / 공중 프레임 | 최종 상태·출력 수치 |
|:---|:---|:---|:---|
| 남극 인근, lat≤−80° | 17 / 895 | 856 / 39 | 거리≤30cm·Yaw≤10.15°·Tilt 0.000° |
| 적도, \|lat\|≤5° | 45 / 2,202 | 1,812 / 390 | 거리≤30cm·Yaw≤90°·Tilt 0.000° |
| 순수 공중, ground=0 | 45 / 2,298 | 0 / 2,298 | 거리≤15.899cm·Yaw≤90°·Tilt 0.000°, 끝 위도 5.11~51.05° |

- 이동 상태 로그의 진입/복귀 쌍에서 연속 공중 5.28초와 **32.53초**를 확인했다. 그래플 발동은 11회다. 32.53초 구간은 13:42:13.634~46.159 KST에 여러 그래플 발동을 포함한다. 이 긴 체공이 위도 ±5°에 계속 머물렀다거나 활공도 별도로 검증했다고 표현하지 않는다.
- Steering 적용 표본 109개·공중 MM 조회 비활성 표본 62개, 입력 ready·궤적 31샘플 확인. Phase 누락·PoseSearch Warning/Error·LogAnimation Error·assertion/fatal 0건, 기존 Foley Warning 443건 잔류. 녹화 종료 시 MassTrace의 미종료 PrePhysics 구간 경고 1건과 그 요약 Error 카테고리 출력(`1 warnings; 0 errors`)이 있어 전체 로그 Error 0건이라고 주장하지 않는다. 플랫폼 ETW 권한 메시지·프로파일 타이머 메타데이터 경고도 녹화 분석에 기록됐다. Trace 파일 생성·분석은 확인했으나 모든 추적 채널의 완전성을 보증하지 않는다.
- 현재 Rewind에서 UAFLocomotion→S→AG 쪽 공중 Sequence Player의 Fall_Loop 두 시점과 착지 후 지상 MM 복귀를 조회하도록 수동 요청했다. 숨김 AnimSourceMesh의 Pose Search 조회와 구분한다. 확인 시 PIE 활성이고 일시정지 상태 자체는 API로 별도 확인하지 않았다. **실제 클립 추적 결과 대기, 공중 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.**

### Rewind 확인 경로 정정 — SequenceInfo

- 사용자는 숨김 ABP_SourceUAFSpike에서 Fall_Loop를 찾았지만 AG_LocomotionSpike 쪽 트랙에서는 찾지 못했다고 보고했다. 숨김 ABP의 재생은 UAF 실제 공중 출력의 근거로 쓰지 않는다. AG 하위 트랙의 이름 부재만으로 미실행 또는 추적 한계 중 어느 쪽이라고도 확정하지 않는다.
- 설치 엔진 `UAFAnimGraphEditor/Private/RewindDebugger/SequenceInfoTrack.cpp`에서 **UUAFComponent 대상 SequenceInfo 트랙**과 그 Details를 확인했다. `RefreshSequenceInfoFromEvaluationProgram`은 기록된 `FSerializableEvaluationProgram.Tasks`의 `FAnimNextAnimSequenceKeyframeTask`를 읽어 실제 AnimSequence·SampleTime을 표시한다. 따라서 공중 고정 시퀀스는 `UAFLocomotion → SequenceInfo → Sequence Trace Info`에서 확인한다. 이름별 AG 하위 트랙을 찾도록 한 안내를 정정했다.
- 기존 녹화의 긴 체공 두 시점과 착지 후를 이 목록에서 조회하도록 요청했다. 새 코드/계측·PIE/녹화 없이 결과를 기다린다. 목록이 비거나 없으면 UI/기록 상태를 먼저 확인하며 공중 단위는 계속 미완료로 둔다.

### 현재 UI의 대체 조회 — EvaluationProgram Tasks

- 사용자가 SequenceInfo 트랙이 없다고 보고했고, computer-use로 실제 Rewind 창을 읽어 현재 트랙 목록에도 없는 것을 확인했다. 설치 소스에 creator 등록과 표시 조건 구현이 있다는 사실만으로 현재 UI에서 조회 가능하다고 안내한 점을 정정한다. 표시 조건의 실제 실패 원인까지 진단하지 않았고 별도 디버거 수정은 하지 않는다.
- 이미 있는 `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_7 → AG_LocomotionSpike → EvaluationProgram`을 선택하고 Details의 Tasks를 펼쳤다. 약 **156.1초**에서 7개 평가 task 중 `[1] Anim Next Anim Sequence Keyframe Task`의 `AnimSequence=MM_Unarmed_Jump_Fall_Loop_UEFN`, `SampleTime=0.243282`, `Looping=true`를 화면 및 Slate 텍스트로 직접 확인했다. 숨김 ABP의 기록이 아니라 UAF가 기록한 평가 프로그램이다.
- 같은 프레임에는 History Collector 전/후 작업과 준비/기본 Offset Root/최종 회전 보정/진단 작업이 있다. 공중 중간의 실제 시퀀스 평가 근거이며, 이 한 시점만으로 전체 긴 체공·전환을 검증했다고 확대하지 않는다.
- GUI에서 task 펼침은 성공했으나 시간 헤드 이동 시도가 반영되지 않아 자동 조작을 반복하지 않고, 사용자에게 약 159초 공중과 약 190초 착지 후의 클립/SampleTime 조회를 요청했다. 기존 녹화·PIE 상태와 에셋은 유지했다. observer는 해제했다. **나머지 두 시점 확인 대기이며 단위 미완료**다.

### 최종 완료 판정

사용자가 같은 녹화에서 다음 값을 확인했다. 첫 행은 에이전트가 computer-use와 Slate 텍스트로 직접 읽은 값이며, 다음 두 행은 사용자 수동 조회다.

| 녹화 타임라인 | 실제 UAF 평가 클립 | SampleTime | 근거 |
|:---|:---|:---|:---|
| 약 156.1초, 공중 | `MM_Unarmed_Jump_Fall_Loop_UEFN` | 0.243282 | EvaluationProgram Tasks[1] 직접 조회, Looping 켬 |
| 약 159초, 같은 체공 | `MM_Unarmed_Jump_Fall_Loop_UEFN` | 0.102736 | 사용자 조회 |
| 약 190초, 착지 후 | `M_Relaxed_Run_Stop_F_Lfoot` | 1.812737 | 사용자 조회 |

- [x] 기존 ABP와 같은 공중 전체 조건·Fall_Loop 소스·루프·양방향 0.25초 경로 구성.
- [x] 혼합 포즈의 Pose History·Phase·Mover Trajectory 유지, 지상 Steering 조회 보완.
- [x] Live Coding 성공·AG/S 컴파일/저장, 최초 달리기/착지 회귀의 설정 보완 및 짧은 재시험 확인.
- [x] 긴 체공·적도 사용자 관찰 이상 없음, 기록된 10,163평가 프레임의 최종 루트 수치 확인.
- [x] 실제 UAF 평가 프로그램에서 공중 두 시점의 고정 루프와 착지 후 지상 클립 복귀 확인.

**공중 Fall_Loop 고정 구현·검증 단위 완료.** 매 프레임 클립 동일성·활공 단독·북극 별도 재시험·적/네트워크 전체까지 확대하지 않는다. 남극 인근·적도 접근/횡단과 여러 위도의 실제 그래플/공중 표본 및 사용자 관찰 범위의 판정이다. 기존 경고/추적 한계는 위 기록대로 유지한다.

다음 제안은 입력 PSD 배열의 같은 시점 대조·전환/인터럽트 검증 단위다. 승인 후 기존 녹화에서 공급/수신 공유 Databases·InterruptMode와 실제 MM 검색을 대조하고 부족한 전환 표본만 추가한다. 이번 단위에서 다음 작업에 착수하지 않는다. 5-1 전체 미완료·5-2 미착수·Go/No-Go 보류.

## 13. 입력 PSD 배열·대표 전환/인터럽트 검증(2026-10-25, 단위 완료)

사용자의 “계속 진행하자” 승인으로 착수했다. 공중 고정 완료 상태를 유지하고 기존 녹화 `20261025_133940695.utrace`부터 읽는다. 기본 ABP·Chooser·PSD·엔진은 수정하지 않는다.

### 확인 경로와 성공 기준

1. 같은 녹화 시점의 공급 Chooser 결과와 `AG_LocomotionSpike → Native Properties → LNPUAFSpike Inputs`의 Databases 배열을 대조한다. Interrupt Mode·Blend Arguments의 BlendTime도 공급 C++ 술어/로그와 대조한다. 안정 상태 한 표본만으로 전환 프레임 전체를 통과 처리하지 않는다.
2. 출발의 Starts+Loops 다중 결과, 정지/방향 전환·제자리 회전 요청·착지 첫 프레임과 후속 프레임을 확인한다. 실제 UAF 선택 DB/클립·평가 작업과 구분해 기록한다. 부족한 전환 종류만 수동 표본을 추가한다.
3. 입력 목록과 선택 클립의 무조건 동일성을 요구하지 않는다. DoNotInterrupt에서는 현재 검색 목록에 없는 이전 DB의 진행 포즈를 유지할 수 있다. 입력 전달 일치와 인터럽트 시 전환 동작을 각각 검증한다. 특정 프레임의 결과를 전체 전환/일곱 게이트 통과로 확대하지 않는다.

### 코드·기존 로그 대조

- 스파이크 공급층은 기존 Chooser를 같은 AnimInstance 컨텍스트로 평가하고 기존 `GetMMInterruptMode`·`GetMMBlendTime` 값을 공유 변수에 보낸다. 기본 ABP의 `[AnimB1]`은 DB 배열이 바뀔 때 목록·인터럽트·블렌드 시간을 기록한다. UAF의 `[UAFSteering]`은 선택 결과의 약 1초 표본이므로 전환 첫 프레임과 동일시하지 않는다.
- 기존 C++ 인터럽트 조건: 이동 모드 변화·Moving 중 방향 변화·Idle↔Moving 경계·정지 제자리 회전 요청이면서 아직 회전 DB가 아닐 때 `InterruptOnDatabaseChange`, 그 외 DoNotInterrupt. 지상 0.5초·착지 직후 0.2초·공중 상승 0.15초/그 외 0.5초다. UAF MotionMatchingTrait는 입력 Databases와 NextUpdateInterruptMode를 실제 검색에 사용한다. 이번 확인에서 코드 변경·빌드 없음.

### 190초 안정 상태 첫 표본

- AG의 기록된 Native Properties를 직접 펼쳐 Databases=`[PSD_Stand_Idles_UEFN]`(원소 1개), Interrupt Mode=`Do Not Interrupt`, BlendTime=`0.5`를 읽었다. 같은 시점 공급 Chooser의 Rewind 디버그 대상은 `ABP_SourceUAFSpike_C_0 in BP_PlayerUAFSpike_C_0`이며 입력 OnGround/Idle/Run/F와 Idle 결과 행을 확인했다. Chooser는 읽기용 디버깅으로 열었다가 닫았고 저장/파일 변경은 없다.
- 같은 190초의 실제 UAF Run Stop 평가는 앞 단위에서 확인했다. 현재 Idle 입력과 이 결과의 이름이 다르다는 이유로 오류로 판정하지 않는다. 이 한 표본에서 진행 포즈 유지와 입력 배열을 구분했다.
- 사용자가 약 1.5초(첫 출발)로 직접 스크럽하도록 요청했다. 공유 입력·실제 검색을 해당 프레임에서 이어서 읽을 예정이다. 현재 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 출발 다중 배열·인터럽트 경계 확인

- 사용자 첫 출발 스크럽 후 약 1.5초 AG 수신값은 `[PSD_Stand_Run_F_Starts_UEFN, PSD_Stand_Run_F_Loops_UEFN]`(순서 유지), DoNotInterrupt, BlendTime 0.5였다. 같은 시점 공급 Chooser 디버그 대상은 기존 숨김 ABP이며 Starts/Loops 두 결과 행과 OnGround/Moving/Run/F를 확인했다. 이 표본은 첫 이동 경계가 지난 뒤라 DoNotInterrupt라는 이유로 전달 실패라고 판단하지 않는다.
- Rewind 툴바 버튼을 hover해 `Step one frame back`임을 확인한 뒤 한 프레임 뒤로 이동했다. 같은 배열·BlendTime 0.5를 유지하면서 AG 수신 Interrupt Mode가 **Interrupt on Database Change**로 바뀌었다. 여기에서 UAF 쪽 Pose Search를 선택해 Active Pose=`PSD_Stand_Run_F_Starts_UEFN / M_Relaxed_Run_Start_F_Rfoot`, Asset Player Time=0.266667초, Interrupt Mode=`Interrupt on Database Change`, Elapsed Pose Search Time=0.0을 읽었다.
- 한 프레임 더 뒤의 AG 수신값은 `[PSD_Stand_Idles_UEFN]`·DoNotInterrupt·BlendTime 0.5였다. 따라서 기록된 인접 프레임의 입력 흐름은 Idle → 시작 인터럽트와 Starts+Loops → 인터럽트 해제다. 기존 공급 `[AnimB0]/[AnimB1]`도 Idle→Moving 첫 프레임에 같은 배열·인터럽트·0.50을 기록한다. CPU 콜백 단위 동시성이나 모든 출발 사례까지 증명하지 않는다.
- Pose Search Details는 해당 프레임에 새 검색 이벤트가 없으면 이전 표시를 유지할 수 있으므로, 이전 프레임의 입력값은 AG Native Properties로 다시 읽었다. 후보 목록을 Active Pose로 오인하지 않고 Active Pose 첫 행과 아래 인터럽트/시간 필드를 구분했다.
- 다음은 약 16초의 첫 착지 구간에서 0.2초 블렌드·착지 DB와 후속 지상 입력으로 복귀하는지 확인한다. 사용자 수동 스크럽 대기. 이 확인에서 코드/에셋 저장·빌드·새 녹화 없음.

### 첫 착지 경계·후속 지상 설정 확인

- 사용자가 첫 착지 구간으로 스크럽한 뒤 AG Native Properties에서 `[PSD_Stand_Run_Lands_Heavy_UEFN]`·`Interrupt on Database Change`·BlendTime **0.2**를 읽었다. UAF Pose Search Details의 Active Pose는 같은 DB의 `M_Relaxed_Jump_F_Land_Run_Heavy_Rfoot`, Interrupt Mode도 `Interrupt on Database Change`, Elapsed Pose Search Time은 0.0초였다. Asset Player Time 표시값은 1.0초다.
- 툴바의 다음 프레임으로 이동한 뒤 AG 수신값은 `[PSD_Stand_Sprint_Loops_UEFN]`·DoNotInterrupt·BlendTime **0.5**였다. 앞 프레임으로 돌아가면 착지 DB·인터럽트·0.2가 다시 표시됐다. 착지 설정이 일반 지상 프레임에 남지 않는 표본이다. 착지 이전 공중 프레임까지 이번 인접 조회로 확정하지는 않았다.
- 기존 공급 로그도 13:39:56.624 KST(프레임 615)에 같은 착지 DB·인터럽트·0.20, 다음 13:39:56.649(616)에 Sprint Loops·DoNotInterrupt·0.50을 기록한다. 착지 프레임의 공급 Chooser 디버그 창은 Gait Sprint를 표시해 AG의 착지 입력/로그와 시점 차이가 있었다. 이 창의 표시만으로 동일 평가의 공급 배열 대조가 끝났다고 주장하지 않으며, 원인도 확정하지 않는다. 입력/실제 UAF 검색·공급 로그의 일치와 구분한다.
- 코드·에셋 저장·빌드·새 녹화 없이 확인했다. 다음으로 기존 녹화 약 188.6초의 제자리 회전 구간을 사용자에게 수동 스크럽 요청했다. 정지·방향 전환 검증은 남았고 현재 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류다.

### 제자리 회전 요청·실제 검색 확인

- 사용자 스크럽 후 약 188.6초 AG 수신 배열은 `[PSD_Stand_TurnInPlace_UEFN]`, InterruptOnDatabaseChange, BlendTime 0.5였다. 공급 Chooser 디버그 대상은 `ABP_SourceUAFSpike_C_0 in BP_PlayerUAFSpike_C_0`, 상태 OnGround/Idle/Run/F에서 TurnInPlace 결과 행의 선택 화살표를 확인했다. 이 표본에서 공급 결과와 수신 배열이 일치한다. 테이블은 저장하지 않고 닫았다.
- 같은 구간 UAF Pose Search의 Active Pose는 `PSD_Stand_TurnInPlace_UEFN / M_Relaxed_Stand_Turn_045_R`, InterruptOnDatabaseChange, Elapsed Pose Search Time 0.0초, Asset Player Time 0.153641초였다. 실제 회전 검색/재생의 근거이며 공급층의 회전 선택과 혼동하지 않는다.
- 툴바로 이전 프레임의 `[PSD_Stand_Idles_UEFN]`·DoNotInterrupt·0.5를 확인하고 앞으로 돌아가면 회전 DB·인터럽트가 다시 표시됐다. 이어 두 프레임 앞으로 이동한 표본은 회전 DB·DoNotInterrupt·0.5였다. 회전 요청 인터럽트가 들어오고 이후 해제되는 것을 확인했으며 정확히 한 프레임짜리 펄스라고 일반화하지 않는다.
- 공급 로그는 13:42:49.300 KST에 정지 중 회전 요청(facing −53°), TurnInPlace·인터럽트·0.50, 49.572에 Idles·DoNotInterrupt, 49.617에 회전 클립 선택 표본을 기록했다. 입력 해제와 진행 회전 포즈 유지의 구분은 앞의 기준을 따른다. Idle 복귀 시점의 AG 직접 조회는 아직 하지 않았다.
- 다음으로 약 189.4초 마지막 달리기→정지 구간을 사용자에게 수동 스크럽 요청했다. 방향 전환도 남았으며 현재 단위는 진행 중이다. 코드/에셋 변경·빌드·새 녹화 없음.

### 마지막 달리기 정지 경계 확인

- 사용자 스크럽 후 약 189.4초 AG 수신값은 `[PSD_Stand_Run_Stops_UEFN]`·DoNotInterrupt·BlendTime 0.5였다. 실제 UAF Pose Search Active Pose는 같은 DB의 `M_Relaxed_Run_Stop_F_Lfoot`, DoNotInterrupt, Elapsed Pose Search Time 0.0초, Asset Player Time 1.15367초였다.
- 한 프레임 뒤로 이동한 AG 입력은 같은 Run Stops 단일 배열·**InterruptOnDatabaseChange**·0.5였다. 그 시점 공급 Chooser의 디버그 대상/상태는 숨김 ABP·OnGround/Idle/Run/F이며 Run Stops 결과 행의 선택 화살표를 확인했다. 공급과 수신 배열이 일치하고 정지 경계 인터럽트가 들어왔다. Chooser는 저장하지 않고 닫았다.
- 공급 로그의 13:42:50.071~072 KST는 Moving→Idle와 Run Stops·인터럽트·0.50, 50.121은 Idles·DoNotInterrupt·0.50을 기록한다. AG의 190초 Idles·DoNotInterrupt·0.5는 앞서 직접 확인했다. 따라서 정지 첫 프레임/직후 Run Stops와 이후 Idle 복귀 표본을 확보했으나, 그 사이 모든 프레임의 입력 전환을 읽었다고 주장하지 않는다.
- 일부 트랙 선택/스텝 이후 Details가 비거나 이전 트랙 표시를 유지해 해당 조회를 추가 근거로 쓰지 않았다. 자동 선택을 반복하지 않고 다음 약 35.35초 방향 전환으로 수동 스크럽 및 AG 선택을 요청했다. 현재까지 코드/에셋 저장·빌드·새 녹화 없음. 방향 전환 검증이 남아 단위는 계속 진행 중이다.

### 방향 전환 표본 조회(수신 배열 확인 대기)

- 사용자 스크럽 후 약 35.35초 UAF Pose Search Active Pose는 `PSD_Stand_Run_RL_Spins_UEFN / M_Relaxed_Run_Spin_B_RL_Rfoot`, InterruptOnDatabaseChange, Elapsed Pose Search Time 0.0초, Asset Player Time 0.966667초였다. 후보표에는 RL Loops와 RL Spins 양쪽이 있으나, 후보표로 공유 입력 배열의 순서와 전체성을 대신하지 않는다.
- 공급 Chooser 창은 기존 숨김 ABP 디버그 대상·OnGround/Moving/Run/R와 RL Loops 행의 선택 화살표를 표시했다. RL Spins 행은 화면 아래에 있어 직접 확인하지 않았다. 공급 로그의 13:40:16.030~031 KST(프레임 574)는 F→R·Spin 요청과 `[PSD_Stand_Run_RL_Loops_UEFN, PSD_Stand_Run_RL_Spins_UEFN]`·인터럽트·0.50, 16.131(578)은 RL Loops 단독·DoNotInterrupt·0.50을 기록한다.
- 현재 AG 트랙이 선택 표시되어도 Details에는 Pose Search가 남아 있어 공유 입력 배열을 아직 확정하지 않았다. 테이블 창은 저장하지 않고 닫았다. 사용자에게 시간을 유지한 채 다른 트랙→AG 재선택으로 Native Properties가 표시되는지 확인 요청했다. 코드/에셋 변경·새 계측·빌드·녹화 없이 기존 기록으로 검증 중이다. 현재 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 방향 전환 수신 배열·경계 확인과 단위 종료

- 사용자 재선택 후 Native Properties 화면으로 전환했지만 처음에는 원소 0개였다. 인접 프레임 조회 중 원소 1개의 `LNPUAFSpike Inputs`가 표시돼 구조체와 DB 배열을 직접 펼쳤다. 처음의 빈 표시를 실제 DB 입력 없음이나 실행 실패로 간주하지 않는다. 로컬 `FAnimNextModuleTrack::UpdateInternal`은 현재 Game 프레임 범위의 VariablesTimeline 이벤트를 읽기 전에 표시 배열을 비우므로, 빈 기록과 실제 실행 입력의 의미를 구분해야 한다. 빈 표시의 정확한 발생 원인은 확정하지 않았다.
- AG 수신 배열은 순서대로 `[PSD_Stand_Run_RL_Loops_UEFN, PSD_Stand_Run_RL_Spins_UEFN]`, DoNotInterrupt, BlendTime 0.5였다. 한 프레임 이전은 **같은 배열·InterruptOnDatabaseChange·0.5**, 다시 한 프레임 이전은 `[PSD_Stand_Run_F_Loops_UEFN]`·DoNotInterrupt·0.5였다. 공급 로그의 F→R 방향 경계·두 DB 순서와 일치한다. Chooser 화면은 RL Loops 행까지만 직접 확인했으며 두 번째 행은 로그로 대조했다.
- 따라서 방향 전환의 이전 루프 → 두 DB/인터럽트 → 같은 두 DB/인터럽트 해제와 실제 UAF RL Spin 검색을 확인했다. 숨김 ABP와 UAF의 선택 발·클립 시간이 서로 같아야 한다고 요구하지 않는다. 서로 다른 포즈 이력을 사용하며 입력 목록/정책 전달과 각 경로의 실제 검색을 검증한 것이다.

| 대표 표본 | 입력·정책 확인 | 실제 UAF 확인 |
|:---|:---|:---|
| Idle | Idles 단일 배열·DoNotInterrupt·0.5, Chooser 대조 | 이전 Run Stop 진행 포즈 유지와 구분 |
| 출발 | Idles → Starts+Loops/인터럽트 → 같은 배열/해제, 0.5 | Run Start Rfoot |
| 착지 | Heavy Land/인터럽트/0.2 → Sprint Loops/해제/0.5, 공급 로그 대조 | Heavy Run Land Rfoot |
| 제자리 회전 | Idles/해제 → TurnInPlace/인터럽트 → 같은 DB/해제, 0.5, Chooser 대조 | Stand Turn 045 R |
| 정지 | Run Stops/인터럽트 → 같은 DB/해제 → 이후 Idles 표본, 0.5, Chooser 대조 | Run Stop F Lfoot |
| 이동 방향 전환 | Run F Loops/해제 → RL Loops+Spins/인터럽트 → 같은 배열/해제, 0.5, 공급 로그 대조 | Run Spin B RL Rfoot |

**이번 입력 배열·대표 전환/인터럽트 검증 단위는 완료했다.** 기존 녹화와 코드/로그만 읽었고 추가 C++/에셋 수정·저장·빌드·계측·녹화는 없다. 문서만 갱신했다. 같은 시점의 Chooser 창 전체 결과까지 모두 직접 읽은 것은 아니며, 착지 표시 시점 차이·빈 Native Properties 프레임은 한계로 유지한다. Pivot·Sprint Turns·GaitTransitions·모든 방향/보행 조합·모든 프레임의 전달 동등성을 통과 처리하지 않는다. PSD·Chooser 게이트는 부분 확인, 5-1 미완료·5-2 미착수·Go/No-Go 보류다.

다음 제안 단위는 **지상 Orientation Warping의 기존 ABP 대비 동등 경로 조사·최소 구현·검증**이다. Steering/Offset Root와 별도 항목으로 다루고, 공개 UAF 경로가 없으면 구현 가능성과 제한을 먼저 판정한다. 발 배치 Control Rig·ResetRoot/CopyBone·큰 회전 base와 적/네트워크 검증은 별도로 남긴다. 사용자 진행 확인 전 다음 구현에는 착수하지 않는다.

## 14. 기본 UAF Strafe Warping 비교(2026-10-25~26, 비교 단위 완료)

사용자가 Phase 5 재개를 요청했고, 조사에서 확인한 차이를 설명한 뒤 **기본 UAF Strafe Warping을 최소 연결해 시각 비교하는 경로**를 선택했다. 기존 ABP의 수학을 별도 이식하는 단위가 아니다. 기본 노드가 존재한다는 이유로 B2 전체나 최종 게이트를 통과 처리하지 않는다.

### 실제 구성·엔진 대조

- LootNPop MCP의 `IsPIERunning=false`. GASP 에디터는 사용하지 않았다. `ABP_SourceUAFSpike`의 실제 중첩 MM 그래프에서 `AnimGraphNode_OrientationWarping_1.Node`를 읽었다. Graph 모드, RootBoneTransform 공간, Z 축, 척추 5개, IK root/feet, 분배 0.5, 보간 8, 각도 임계값 135, 최소 속도 10, 카운터 보간 45, 최대 보정 180, 보상 임계값 45, 미래 예측 **TargetTime=0.8초**다. §2.10 기존 표에는 마지막 값이 빠져 있었다.
- 기준 소스: 엔진 `Animation/AnimationWarping/Source/Runtime/Private/BoneControllers/AnimNode_OrientationWarping.cpp`, `Experimental/UAF/UAFWarping/Source/UAFWarping/Private/StrafeWarpingTrait.h/.cpp`. 같은 플러그인의 `AnimNode/UAFStrafeWarpingNode`도 있으나 현재 AG는 trait 평가 경로이므로 비교 후보는 `FStrafeWarpingTrait`다.

| 계약 | 기존 OrientationWarping | 기본 UAF Strafe Warping |
|:---|:---|:---|
| 방향·Up | 월드 이동 방향을 가상 루트 로컬로 변환, 로컬 Z 평면 투영 | 월드 목표 쿼터니언을 입력 루트 로컬로 변환, 로컬 Z 평면 투영. 구면 사용 가능 후보이며 적도 실행 검증 전 |
| 미래 루트모션 | 실제 TargetTime 0.8초, 미래 오차가 작으면 사용 | 해당 예측 코드가 주석 처리돼 있어 없음 |
| 발 처리 | `ik_foot_root` 회전과 `ik_foot_l/r` 회전 상쇄 | 실제 `foot_l/r`로 목표를 만들고 두 본 IK를 풀거나 IK 목표만 설정. 같은 IK root 필드 없음 |
| 루트 | 루트를 돌리고 뒤의 ResetRoot를 거침 | 기본 PreserveOriginalRootRotation=true는 루트 자식을 보정. false는 직접 루트 회전 |
| 보정 단위 | MM 블렌드 샘플마다 커브·상태·워핑 | 이번 최소 후보는 블렌드된 포즈 1회, 평가 포즈의 Enable_Warping 사용 |

기존 발 배치 CR은 `UseIKBoneTargets=false`라 FK 발끝을 쓴다. 따라서 비교 후보는 `FootMode=DoBasicIK`로 실제 다리를 보정한다. IK 목표만 설정하는 모드와 같다고 처리하지 않으며, 아직 CR을 연결하거나 검증하지 않았다. RootMotion 방향·각도 보간의 공통 부분도 완전 동등하다고 일반화하지 않는다.

### 입력·순서 어댑터 구현

- `LNPUAFSpikeRootInputs`에 `LastNonZeroVelocity`를 추가하고 숨김 ABP의 기존 공급값을 전달한다. 숨김 ABP의 가상 루트는 전달하지 않는다.
- `LNP Spherical Offset Root`는 평가 범위 인터페이스로 자신의 루트를 자식에게 노출한다. 새 `LNP Strafe Warping Inputs`가 그 루트의 기존 준비/Steering task를 먼저 한 번 예약한다. 어댑터가 없으면 기존 루트 준비 경로를 유지한다.
- 의도한 평가 순서: 자식 포즈 → 기존 구면 준비/Steering → Strafe 입력 준비 → **엔진 기본 Strafe Warping task** → 진단 → 기본 Offset Root → 최종 구면 보정 → 기존 루트 진단. 엔진 보정 수학은 복제·변경하지 않았다. 이 순서는 실행 프로그램에서도 확인해야 한다.
- 알파는 실제 평가 포즈의 `Enable_Warping`, 방향은 현 Up 평면에 투영한 `LastNonZeroVelocity`, 기준은 준비가 끝난 UAF 누적 루트다. 공중·텔레포트·초기 프레임·유효하지 않은 입력·CVar 끔에서는 알파와 보간 잔여값을 초기화한다. 루트 CVar 끔에서는 Strafe도 비활성이다.
- 비교 CVar `LNP.UAFSpike.StrafeWarping 0/1`, 기본 1. `LNP.Anim.DebugLocomotion 2`일 때 `[UAFStrafe]`가 실제 기본 task의 알파·각도·커브를 약 1초 간격으로 기록한다. 아직 런타임 성공 근거가 아니다.

### 구현·복구·수동 배선 경위

- C++ 4파일 수정, `git diff --check` 통과. 17:24 KST 요청한 Live Coding의 UBT 컴파일은 9 actions·293.99초·Succeeded, 패치 링크와 AG/S 재컴파일도 성공했다. 그러나 MCP 응답은 300초 시간 초과, 17:29:50 KST 에디터가 크래시했다. 사용자 콜스택은 `FLiveCodingModule::AttemptSyncLivePatching → ShowNotification → FSlateNotificationManager → SWindow → mimalloc`이며 새 Strafe 평가 task는 아직 배선·실행되지 않았다. Live Coding 재인스턴싱/알림 경로에서 발생했다는 범위까지만 확인했고 메모리 손상 근본 원인은 미확정이다.
- 크래시 로그·UBT 로그·사용자 콜스택은 `Saved/AnimationOverhaul/2026-10-25_StrafeLiveCodingCrash/`에 보존했다. 에디터 종료 상태에서 권한 확장으로 `Build.bat LootNPopEditor Win64 Development ... -WaitMutex -NoHotReloadFromIDE` 전체 빌드를 시작했다(12 actions). 정상 DLL로 재실행하고 AG/S·기존 구면 회귀를 확인한 뒤 그래프를 편집한다. 단순 재시도 Live Coding이나 엔진 수정은 하지 않았다.
- **정상 전체 빌드 성공: 12 actions·88.71초·Succeeded, 컴파일 진단 오류 없음.** 사용자에게 에디터 재실행과 AG/S 정상 열기만 요청했다. 재실행·MCP 복구·기존 구면 수치 회귀는 대기 중이다. 원본 AG와 편집 전 백업 SHA256이 일치해 디스크 에셋 변경 없음을 확인했다.
- **재실행 확인:** 사용자가 AG/S 정상 열기를 확인했다. MCP 읽기 호출 재접속·PIE 종료 상태를 확인했고 재실행 로그에서 두 에셋의 컴파일 성공을 읽었다. 기존 `LootNPop.Animation.UAFSpike.SphericalRootFinalization` 수치 회귀는 1/1 성공, 오류·경고 0, 0.016초다. 이 시험은 기존 구면 제약 수학의 회귀이며 아직 배선하지 않은 Strafe·평가 순서·시각 품질을 검증하지 않는다. 다음은 아래 수동 배선이다.
- 에셋 배선·저장·PIE·새 녹화는 하지 않았다. 편집 전 디스크 AG 백업: `Saved/AnimationOverhaul/2026-10-25_PreStrafeWarping/AG_LocomotionSpike.uasset`.
- 컴파일 성공 뒤 수동 배선 후보: 기존 Bool/Pose History → **Passthrough(기본) + Strafe Warping + LNP Strafe Warping Inputs(마지막)** → 기존 Offset Root/LNP Spherical → Output. 기존 Bool 공중 연결·공유 입력은 유지한다. Strafe의 Alpha·TargetOrientation·RootBoneTransform 추가 배선은 필요 없다(어댑터가 실제 task 실행 직전에 전달).
- **생성 메뉴 정정:** Passthrough는 노드 생성 메뉴 항목이 아니라 스택의 기본 trait다. 실제 에디터 우클릭 검색으로 `Animation → Trait Stack` 항목을 확인했다. 빈 Trait Stack 생성 → 선택 → Details의 `Add Trait`에서 Passthrough 추가 → 같은 스택에 Strafe Warping → LNP Strafe Warping Inputs 순으로 추가한다. 엔진 `TraitStackEditor.cpp`도 빈 스택의 Add Trait는 Base, 이후는 Additive로 필터링한다. 확인 중에는 메뉴만 열고 닫았으며 에셋은 수정하지 않았다.
- **스택 생성 확인:** 사용자 생성 후 Slate 화면/Details에서 Passthrough → Strafe Warping → LNP Strafe Warping Inputs 순서를 확인했다. 새 스택은 아직 미연결이다. Details에서 IK Foot Bones 배열이 편집 가능하고 Spine/Foot 배열은 각각 0개, 보간 10·각도 임계값 90 기본값임을 읽었다. Strafe 카테고리만 펼쳤고 값은 바꾸지 않았다. 다음은 보간 8·임계값 135·척추 5개·다리 2개를 수동 설정하고 기존 Bool과 루트 사이를 배선한 뒤 컴파일·저장하는 단계다. 생성 과정의 AG 컴파일은 성공했고 엔진 메타데이터 `EditCondition ... 0.0f`의 `f` 파싱 오류가 별도로 기록됐다. 값/포즈 평가 실패로 분류하지 않으며 기본값을 쓰는 해당 두 설정의 엔진 코드는 변경하지 않았다.
- **설정·배선 완료 보고 후 확인:** 사용자가 설정·배선·컴파일·저장 완료를 보고했다. 화면에서 Bool → 새 스택 → 기존 Offset Root의 포즈 연결, 보간 8·임계값 135·Preserve Original Root Rotation 켬·Do Basic IK·척추 배열 5개를 확인했다. 다리 필드 전체와 척추 각 원소는 아직 별도 재조회하지 않았다. AG 17:58:37 KST 컴파일 성공, 17:58:38 저장 로그와 All Saved 확인. PIE는 종료 상태다. CVar 재조회는 Steering=1, StrafeWarping=1, OffsetRootBone.Enable=1, DebugLocomotion=0이다. 사용자에게 DebugLocomotion 2 설정과 1인 PIE 북극 지상 약 10초의 첫 실행을 요청한다. 아직 평가 task 순서·실제 보정 각도·시각 품질 검증 전이다.
- 기본 Strafe 설정 후보: RotationAxis Z, DistributedBoneOrientationAlpha 0.5, RotationInterpSpeed 8, LocomotionAngleDeltaThreshold 135, SpineBones spine_01~05, PreserveOriginalRootRotation=true, FootMode DoBasicIK, FootData 두 다리(thigh_l/calf_l/foot_l/ik_foot_l, thigh_r/calf_r/foot_r/ik_foot_r, JointSideVector 로컬 Z). 나머지 위 표와 같은 기본값 유지. 편집 UI에서 항목 접근과 값 재조회를 확인한다.
- 수동 비교 예정: 북극 지상에서 대각 이동·급전환·정지/출발을 0/1로 비교, 적도에서 같은 동작 및 점프/착지. 먼저 짧은 크래시/배선 확인 후 기록 시험으로 나눈다. 발 꼬임·상체 비틀림·루트 기울기·발 미끄러짐을 보고, 로그/평가 task 순서를 대조한다. 아직 다음 구현 단위나 발 배치·ResetRoot/CopyBone·큰 회전 base·적/네트워크에 착수하지 않았다.

### 첫 실행 확인

- 사용자가 첫 실행 정상(T 포즈·다리 꼬임·크래시 관찰 없음)을 보고했고 MCP에서 PIE 활성 확인. `[UAFSpike] inputs=ready databases=1 trajectorySamples=31`, `[UAFSteering] ready=1 applied=1`, `[UAFStrafe]` 알파/각도 적용을 읽었다.
- 고정 스냅샷 `Saved/AnimationOverhaul/2026-10-25_StrafeFirstRun/LootNPop.log`와 `Summary.json`을 보관했다. Strafe 표본 230개 중 알파 비영 155개, 기록 각도 −57.687~67.899°다. 이는 약 1초 간격의 표본이며 매 프레임 각도의 최소·최대가 아니다.
- `[UAFRoot]` 230구간의 매 프레임 최대값을 합산하면 누적 상태/출력 모두 거리≤30.000cm·Yaw≤90.00°·Tilt=0.000°다. 구간 끝 위도는 −87.21~89.79°로 북극만의 시험으로 한정하지 않는다. 모든 프레임의 위치나 지상 여부를 같은 범위로 일반화하지 않는다. 구성 오류·LogAnimation Error·ensure/fatal은 스냅샷에서 0건이며, 기존 Foley 경고와 에디터 메타데이터 오류까지 0이라는 뜻은 아니다.
- 첫 실행은 실제 기본 Strafe task의 동작과 관찰/루트 수치 확인이다. 켬/끔 품질 비교·적도에서 의도한 동작·공중 보정 해제·Rewind EvaluationProgram의 task 순서·본 원소 전체 재조회는 아직 남았다. 다음 요청은 현재 북극 부근에서 Rewind 녹화 후 StrafeWarping 0/1 각각 같은 이동·급전환/정지, 켬 상태 점프 1회, 이후 PIE 일시정지다. 코드·에셋·빌드는 추가 변경하지 않았다.

### 0/1 비교·재시작 후 복원 확인(2026-10-26)

- 사용자는 10월 25일 두 모드 모두 녹화했고, **둘 다 자연스럽지만 1이 약간 더 나은 느낌이며 큰 체감 차이는 없다**고 보고했다. 단위 전체 완료나 기존 ABP와의 동등성으로 확대하지 않는다.
- PC 재시작 뒤 기존 변경 C++ 4파일·저장 AG·전체 빌드 DLL이 남아 있고, 이번 에디터 기동 로그에서 AG/S 컴파일 성공을 확인했다. MCP 재접속과 `IsPIERunning=false`를 확인했다. 이전 세션의 PIE 인스턴스를 복원한 상태가 아니라 저장된 녹화를 조회 중이다. 구현 변경이 없어 빌드·자동화 회귀를 반복하지 않았다.
- 현재 연 녹화는 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261025_184053031.utrace`(233,145,933바이트)다. 로드 완료 로그로 파일을 확인했다. 10월 25일 전체 공급 로그는 `Saved/Logs/LootNPop-backup-2026.10.25-09.43.54.log`에 남아 있어 `Saved/AnimationOverhaul/2026-10-26_StrafeComparisonResume/LootNPop_2026-10-25.log`로 보존하고 `Summary.json`을 만들었다.
- 명령 시각: Strafe 0은 18:40:49.394 KST, 녹화 시작 18:40:53.070, Strafe 1은 18:41:30.167, 녹화 종료 18:42:14.479다. 녹화상 약 37.1초부터 켬 구간이다.

| 로그 구간 | 끔 | 켬 |
|:---|:---|:---|
| Strafe 표본 / 알파 비영 | 37 / 0 | 43 / 38 |
| 표본 각도 | 전부 0° | −65.764~69.866° |
| Root 구간 / 평가 프레임 | 37 / 1,794 | 43 / 2,062 |
| 집계 지상 프레임 | 1,794 | 2,062 |
| 누적/출력 거리·Yaw·Tilt 최대 | 30cm·90°·0.000° | 30cm·90°·0.000° |
| 구간 끝 위도 | 87.78~89.77° | 88.49~89.86° |

각도는 약 1초 간격의 표본이고 Root 값은 각 진단 구간의 매 프레임 최대값이다. 경계의 첫 Root 구간은 이전 프레임을 일부 포함할 수 있다. 양 구간은 서로 다른 수동 입력이며 동일 프레임 A/B나 성능 비교가 아니다. 이번 비교에서 공중 진입 로그가 없고 집계 프레임도 모두 지상이라 **점프/공중 게이트의 근거로 쓰지 않는다**.

### 실제 평가 프로그램 순서 확인(2026-10-26)

사용자가 약 50초의 지상 시점으로 수동 이동하고 `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_1 → AG_LocomotionSpike → EvaluationProgram`을 선택했다. Rewind Debugger Details의 Tasks 11개를 직접 읽었다.

| 인덱스 | 실제 task |
|:---|:---|
| 0~3 | History Collector Pre Evaluate → Anim Sequence Keyframe → Evaluation Notifies → History Collector |
| 4 | LNPPrepare Spherical Offset Root Task(기존 Steering 포함) |
| 5 | LNPPrepare Strafe Warping Task |
| 6 | Anim Next Strafe Warping Task(엔진 기본) |
| 7 | LNPInspect Strafe Warping Task |
| 8 | Anim Next Offset Root Bone Task |
| 9 | LNPFinalize Spherical Offset Root Task |
| 10 | LNPInspect Spherical Offset Root Task |

같은 기록의 Keyframe task는 `M_Relaxed_Sprint_Loop_F`, SampleTime=1.6425였다. 준비/Steering → 기본 Strafe → 기본 Offset Root → 최종 구면 보정 순서를 확인했다. 이 한 시점의 기록을 모든 프레임·전환 동등성으로 확대하지 않는다. 조회 중 코드·에셋 변경·저장·빌드·새 녹화는 없으며 Slate observer를 해제했다.

**순서 확인 직후 남았던 작업(아래 최종 결과로 해소):** 적도에서 Strafe 0/1 이동 비교, 켬 상태 점프/체공/착지로 공중 알파 해제 및 복귀 확인. 필요하면 새 녹화는 이 미검증 구간에 한정한다. 본 배열 전체 재조회도 남았지만 실제 기본 다리 평가의 첫 실행은 정상이다. 워핑 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류. 다음 구현 단위에는 아직 착수하지 않는다.

### 적도·공중 검증과 비교 단위 종료(2026-10-26)

사용자가 적도 0/1 이동과 점프·착지를 완료했고 **둘 다 정상, 차이는 작음**이라고 보고했다. 최종 녹화는 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261026_172123536.utrace`(189,181,212바이트)다. 앞선 `171910512`·`172025681` 기록을 최종 시험과 섞지 않는다. 공급 로그와 집계 JSON은 `Saved/AnimationOverhaul/2026-10-26_StrafeEquator/`에 고정 보관했다.

- 명령/기록 시각(KST): Strafe 0 17:21:20.149 → 녹화 17:21:23.564 → Strafe 1 17:21:48.299 → 녹화 종료 17:22:30.193. 기록상 약 24.7초부터 켬 구간이다.

| 로그 구간 | 끔 | 켬(점프 포함) |
|:---|:---|:---|
| Strafe 표본 / 알파 비영 | 24 / 0 | 42 / 25 |
| 각도 표본 | 전부 0° | −76.412~56.725° |
| Root 구간 / 평가 프레임 | 24 / 1,203 | 42 / 2,120 |
| 지상 / 공중 프레임 | 1,203 / 0 | 1,844 / 276 |
| 누적/출력 거리 최대 | 30.000cm | 30.000cm |
| 누적/출력 Yaw 최대 | 77.88° | 90.00° |
| 누적/출력 Tilt 최대 | 0.000° | 0.000° |
| 구간 끝 위도 | −0.66~1.84° | −1.56~2.35° |

Root 값은 진단 구간별 매 프레임 최대값의 합집계이고 각도는 약 1초 간격 표본이다. 구간 끝 위도와 프레임별 위도를 구분하며, 켬 구간의 최대값은 지상·공중 혼합이다. 경계 첫 진단 창은 직전 프레임을 일부 포함할 수 있다. 정확히 같은 입력/포즈의 A/B 또는 성능 시험으로 해석하지 않는다.

- **공중 해제:** 17:22:16.242~27.306 사이 공중 진입/착지 7쌍(각 약 0.70~0.76초)을 확인했다. 공중에 찍힌 Strafe 표본 6개는 모두 alpha=0·angle=0이었다. 특히 17:22:26.559 진입 뒤 26.628 표본은 **curve=0.631인데 alpha=0·angle=0**이어서 커브가 남은 전환 중에도 지상 조건이 보정을 차단함을 확인했다.
- **착지 복귀:** 17:22:27.306 착지 뒤 27.633 표본에서 alpha=1·curve=1·angle=−6.786°로 재활성화됐다. 이후 Idle 커브 0에서 alpha/각도 0으로 돌아왔다. 이전 Phase §12의 긴 체공 시험은 별도이며, 이번 기록을 긴 체공 전체 검증으로 확대하지 않는다.
- 새 기록에서 스택/공유 입력 구성 오류·LogAnimation Error·ensure/fatal은 확인되지 않았다. 기존 Foley 경고·앞선 에디터 EditCondition 파싱 오류가 사라졌다는 뜻은 아니다. 코드/에셋/빌드 추가 변경 없이 고정 로그를 분석했고 PIE 활성 상태에서 녹화 종료를 확인했다. Slate observer는 해제했다.

**기본 Strafe Warping 비교 구현 단위 완료.** 승인받은 기본 노드 후보를 연결해 실제 task 순서·북극/적도 0/1·공중 해제/착지 복귀·최종 구면 루트 제한과 사용자 관찰을 확인했다. 기본값 1을 유지하는 시험 후보이며, 기존 ABP의 TargetTime 0.8·샘플별 보정·발/루트 처리와 완전 동등하다고 판정하지 않는다. 본 원소 전체의 독립 재조회도 하지 않았다. 스파이크 전체·최종 Go의 근거로 확대하지 않는다.

다음 제안 단위는 **발 배치 Control Rig의 UAF 연결 경로 조사·최소 구현·검증**이다. ResetRoot/CopyBone의 남은 계약·큰 회전 base·추가 전환 표본·적/네트워크·무기/몽타주 게이트도 별도로 남는다. **사용자 진행 확인 전 다음 구현에 착수하지 않는다.** 5-1 미완료·5-2 미착수·Go/No-Go 보류.

## 15. 발 배치 Control Rig UAF 연결(2026-10-26, 최소 연결·검증 단위 완료)

사용자가 새 세션에서 Phase 5 계속 진행을 요청해 이 단위를 시작했다. 기존 CR/ABP·Chooser·PSD·GASP 소스를 수정하지 않는다. 종료 후 다음 구현 단위는 다시 진행 확인을 받는다.

### 엔진 대조·최소 구현

- 기준 계약은 `design/GASPReference.md` §2.12와 Phase02 단위 2-5다. `/Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN`의 공개 변수 이름·타입을 MCP로 확인했다. `UseIKBoneTargets`의 에셋 기본값은 true이므로 UAF 노드에서 반드시 false로 지정한다.
- 로컬 UE 5.8 `UAFControlRig/Source/UAFControlRig/Internal/ControlRigTrait.h`, `Private/ControlRigTrait.cpp`는 Control Rig 입력 포즈·커브 전달, 공개 변수 핀, 바인딩된 SkeletalMeshComponent의 월드 변환/OwnerComponent 등록을 지원한다. UAF 시스템은 이미 `VisualMesh`에 바인딩돼 있고 시험 메시도 UEFN이다. 실제 실행 바인딩·트레이스는 아직 미검증이다.
- `UAFAnimGraph/Source/UAFAnimGraph/Public/Traits/PassthroughBlendTrait.h`·`Private/Traits/PassthroughBlendTrait.cpp`는 단일 입력 Control Rig의 평가 결과와 원래 포즈를 혼합한다. Bool 알파 0에서 CR 평가 task를 추가하지 않고, 중간 알파에서는 포즈 복사/CR/블렌드 task를 추가한다. 따라서 새 분기나 엔진 보정 수학을 만들지 않고 **Control Rig(Base) + Passthrough Blend(Additive)** 스택을 기존 최종 구면 보정 뒤에 둔다. Bool 알파 블렌드 인 0.3초·아웃 0.2초를 사용한다.
- `LNPUAFSpikeAnimInstance.h/.cpp`에 `FootPlacement` 공유 입력을 추가했다. 기존 `GroundNormal`, `ShouldUseFootPlacementRig`, `ShouldResetFootPlacement`, `IsOnStaticGround`, `IsFootPlacementDebugEnabled`를 재사용한다. 최초 활성/착지/CVar 재활성화 때 `bForceReset`을 켠다. 기존 `OffsetRoot` 입력의 base 이동·텔레포트 플래그를 CR에 연결하므로 중복 공급하지 않는다. `LootNPop.cpp`에 새 구조체의 RigVM 등록을 추가했다. 새 trait·CVar·엔진 변경은 없다.

### 편집·빌드 근거

- MCP에서 PIE 활성 확인 → 사용자 수동 종료 → `IsPIERunning=false` 확인. AG/S만 저장 성공 후 `Saved/AnimationOverhaul/2026-10-26_PreFootPlacement/`에 파일 백업했다(AG 310,698·S 78,051바이트). 기존 사용자 변경 4개는 보존한다.
- 사용자 에디터 종료 후 정상 전체 빌드를 권한 확장으로 실행했다. `Build.bat LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`: **Succeeded, 종료 코드 0, 8 actions, 43.72초**. C++ 등록·컴파일·링크 근거이며 새 AG 배선·런타임 성공 근거는 아니다. Live Coding은 사용하지 않았다.
- 다음은 사용자 에디터 재실행 후 아래 배선·AG/S 컴파일·저장, 북극/적도·경사/턱·점프/착지·움직이는 base 수동 검증이다. 구조체 Getter가 재시작 후 실제 생성되는지 확인한다. 기존 구면 진단은 새 CR **앞**의 값이므로 발 배치 뒤 최종 포즈까지 검증했다고 해석하지 않는다.

### 배선 계약

기존 최종 Offset Root/LNP Spherical 스택 → 새 Control Rig/Passthrough Blend 스택 → Output. CR은 기존 구면 보정본을 읽기 전용으로 참조한다. 원본 CR 변경이 필요해지면 UAFSpike 사본을 먼저 만든다.

| 새 스택 입력 | 값/공유 입력 |
|:---|:---|
| Control Rig 참조 | `CR_Biped_FootPlacement_UEFN` |
| Ground Normal | `FootPlacement.GroundNormal` |
| GroundMovementWorldDelta | `OffsetRoot.BasedMovementDelta` |
| DoRaycast | true |
| UseIKBoneTargets | false |
| Enable Foot Pinning | `FootPlacement.bAllowFootPinning` |
| Enable Slope Warping | `FootPlacement.bAllowSlopeWarping` |
| WorldZDamperEnabled | `FootPlacement.bAllowFootPinning`(기존 `IsOnStaticGround` 계약) |
| Has Teleported | `OffsetRoot.bReset` |
| ForceReset | `FootPlacement.bForceReset` |
| DebugDraw | `FootPlacement.bDebugDraw` |
| Passthrough Blend Alpha Input Type | Bool |
| Passthrough Blend bEnabled | `FootPlacement.bEnabled` |
| Bool Blend In/Out | 0.3 / 0.2초 |

### 재실행·수동 설정 중간 확인

- 사용자 재실행 후 MCP 읽기 연결·PIE 종료 상태와 새 입력 구조체의 ScriptStruct 등록을 확인했다. 기동 로그에서 기존 AG/S 컴파일 성공을 확인했다.
- 사용자가 새 스택에 Control Rig → Passthrough Blend를 생성하고 CR 참조를 지정했다. Slate로 참조 `CR_Biped_FootPlacement_UEFN`을 읽었고, 화면에서 Bool Value 알파·Blend In 0.3/Out 0.2를 확인했다. 입력 포즈/커브 전달은 켜짐, 전역 공간 전달은 꺼짐이다.
- **UI 안내 정정:** 새 `Control Rig`(v2)의 Input 범주를 실제로 확인하기 전에 노출하도록 안내했다. 사용자 질문 뒤 소스를 좁혀 대조하니 `UAFControlRigEditor/Private/AnimNextControlRigEditorModule.cpp`는 `ControlRigTraitSharedData`(Legacy)에만 커스텀 UI를 등록하고 v2 등록은 없다. v2에 변수 핀 생성 구현은 있으나 현재 Details에 노출 UI가 없는 원인이다. Legacy는 `Hidden` 메타데이터 때문에 Add Trait 메뉴에서도 제외된다(`TraitStackEditor.cpp::FStructFilter`). 엔진/프로젝트 C++ 우회 코드를 바로 추가하지 않고 새 미연결 스택의 복사 텍스트로 Legacy 구성 가능 여부를 확인 중이다.
- 사용자 노드 복사 텍스트로 미연결 대상 `AnimNextTraitStack_2`를 확인했다. Details 복사 JSON과 노드 복사(`Begin Object`)를 구분했다. 엔진의 `UAnimNextController::SwapTraitByName`을 쓰는 일회성 Python `Saved/AnimationOverhaul/2026-10-26_PreFootPlacement/UseLegacyFootRig.py`로 Legacy 전환·동일 CR 지정 후 AG 컴파일 성공과 실제 Input/Use Pin UI 표시를 확인했다. Python 모드에서 `py` 접두사를 넣은 첫 명령은 실행 전 SyntaxError였고, `exec(compile(open(...).read(), ..., 'exec'))`로 실행했다. AG 전환 전 백업은 `AG_BeforeLegacy.uasset`다.
- **Legacy 채택 보류(사용자 우려).** 두 타입은 같은 `TControlRigTraitBase` 실행 구현을 공유하고 참조 형식만 다르지만, 이전 타입을 최종 채택하는 것으로 진행하지 않는다. 신규 v2를 유지하는 경로를 좁혀 조사했다. `URigVMController::ImportNodesFromText`는 unit node를 붙여넣을 때 `RepopulatePinsOnNode`를 호출하며, CR v2의 `GetProgrammaticPins`는 공개 변수 이름 배열로 핀을 생성한다. Python에는 Repopulate 자체가 노출되지 않아 기존 Export/Import API를 이용한다. 엔진/추가 C++/MCP 도구 변경은 없다.
- **v2 핀 생성 성공:** 사용자 `ExposeV2FootRigPins.py` 실행 후 `[UAFFeetSetup] V2 stack=AnimNextTraitStack_3 generatedPins=10 DoRaycast=True UseIKBoneTargets=False LegacyRemoved=True`와 AG 컴파일 성공을 확인했다. 화면에서 새 Control Rig(v2)·CR 참조·10개 변수·Bool/0.3·0.2를 확인했다. Legacy 스택은 제거됐다. 엔진 Export/Import가 재생성한 변수 핀의 범주는 `SetPinCategory`로 지정했다. 이 절차는 미연결 스택만 변경하고 Undo bracket 실패 시 취소한다. 실행 전 백업은 `AG_BeforeV2Pins.uasset`다.
- 복사 후 기존 포즈 `Input`이 노드 레이아웃에서 빠진 것을 화면에서 발견했다. 사용자 `clear_pin_category` → `set_pin_category(..., 'Control Rig')` → `set_pin_index_in_category(..., 0)` 실행 후 Input 표시와 AG 컴파일 성공을 확인했다. 재현 스크립트에도 이 보완을 추가했다. Slate Type은 focus 실패로 실행되지 않았으며 사용자 수동 명령으로 처리했다. v2의 Input 목록 UI 자체는 여전히 없는 상태이며 생성한 핀을 그래프에서 직접 연결한다.
- **공유 입력 배선 확인:** 사용자가 §15 표의 Getter/포즈 배선과 AG/S Compile·Save 완료를 보고했다. 로그에서 AG(18:58:41 KST)·S(18:58:48 KST) 컴파일 성공, 화면의 All Saved를 확인했다. 19:37:49 KST 사용자 읽기 전용 Python의 실제 링크 11개를 대조해 입력 9개와 포즈 2개가 표대로인 것을 확인했다. `VariableNode_5`는 FootPlacement Getter, `VariableNode_4`는 OffsetRoot Getter다. 특히 `Has Teleported ← VariableNode_4.Value.bReset`이며 Enabled 오연결은 없었다. `AnimNextTraitStack.Result → AnimNextTraitStack_3.ControlRigTraitSharedData_v2.Input → RigUnit_AnimNextGraphRoot.Result`도 확인했다. 첫 안내의 경로 직접 조회 함수는 C++ 전용이어서 노출된 `get_source_pin().get_pin_path()`/`get_target_pin().get_pin_path()`로 정정했다.
- **북극 첫 PIE 관찰 통과:** 사용자가 걷기/달리기/제자리 회전/점프·착지에서 이상 없다고 보고했고 PIE 활성도 확인했다. 최신 세 번째 PIE(20:13:43 KST 시작)의 고정 로그 `NorthSmoke.log`에서 입력 ready 1회·unbound 0회, LogAnimation Error 0·검색한 CR/RigVM/Assertion/Ensure 오류 0, 기존 Foley 경고179회를 확인했다(고정 로그 끝20:15:39). 첫/두 번째 PIE도 ready였으나 최신 세션과 합쳐 집계하지 않았다. 이때 DebugLocomotion=0이라 북극 위도나 루트 수치를 확인한 근거는 아니다. 실제 CR task/입출력 포즈를 독립 조회한 근거도 아직 없다.
- **현재 수동 검증:** PIE에서 `LNP.Anim.DebugLocomotion 2`와 Rewind 녹화를 켜고, 경사/턱 한 곳의 FootPlacement 0→1 비교·재활성 튐, 적도 이동/큰 회전/점프·착지/경사·턱, 움직이는 발판의 정지/이동/점프·착지를 확인하도록 요청했다. 종료 시 녹화만 종료하고 PIE를 유지한다. 현재 설정은 FootPlacement=1·FootPlacementOnMovingBase=0·Steering=1·StrafeWarping=1이다. 실행 전 로그는 `BeforeFirstPIE.log`에 보존했다. 실제 CR 평가 task와 공중 알파 해제/착지 리셋·큰 회전 base는 확인 전이다.

### 최종 검증 결과·단위 종료

**발 배치 최소 연결·검증 단위 완료.** 사용자는 북극 주변 경사에서 발 배치0/1 모두 튐이 없고 **1의 발 접지가 훨씬 자연스럽다**고 평가했다. 이어 적도에서 여러 방향 걷기·달리기·점프·정지를 반복해 어색함이 없다고 보고했다. 움직이는 발판도 시각적으로 어색함이 없었다. 동일 입력/프레임 A/B나 기존 ABP와 본별 수치 대조로 확대하지 않는다.

| 근거 | 확인 결과 |
|:---|:---|
| 경사·적도 녹화 | `RewindDebugger-LootNPop-Editor-20261026_201850667.utrace`, 363,654,323바이트. Trace 연결20:18:50.706~종료20:21:01.871 KST(약131.165초). FootPlacement0은20:18:53.856, 1은20:19:18.644부터 |
| 경사0 진단 | 24개 Root 창·1,178프레임, 지상1,100. 창 끝 위도67.03~71.29° |
| 켬 진단 | 102개 Root 창·4,706프레임, 지상4,487. 이동 중 적도 도달 표본41개·1,872프레임(공중143), 창 끝 위도−1.72~2.83° |
| 기존 루트 계약 | 위 끔/켬 창에서 누적/출력 거리≤30cm·Yaw≤90°·Tilt=0.000°. **CR 앞의 진단**이며 새 발 배치 뒤 최종 본 포즈 측정은 아님 |
| 상태·오류 | 경사/적도 기록에 공중 진입·착지 각7회. LogAnimation Error0·검색한 CR/RigVM/Assertion/Ensure 오류0, 기존 Foley 경고480회 |
| 발판 첫 녹화 | `...20261026_202321824.utrace`, 28,886,782바이트, 약10.223초. Root 창10개·472프레임 전부 지상/접지 base 있음, Moving209프레임. 실제 base는 `BP_PyramidExteriorPanel_C_0.PanelMesh`. CR 앞 최대 거리30cm·Yaw77.66°·Tilt0.000° |
| 발판 두 번째 녹화 | `...20261026_202342105.utrace`, 11,019,442바이트, 약3.934초. Root 창3개·138프레임 전부 지상/base 있음·Moving0. CR 앞 거리0cm·Yaw16.94°·Tilt0.000° |

Root 창은 약1초간의 매 프레임 최대/계수이며 끝 시점 위도/명령으로 묶었다. 명령·위도 경계의 첫 창은 이전 구간을 일부 포함할 수 있다. 경사/적도 켬 구간의 based135프레임은 정지한 SpringLauncher MeshComponent 접촉이며 실제 이동 발판 검증과 구분한다. 발판 최대 base 회전은 두 녹화 각각0.0012°·0.0026°에 불과해 큰 회전 base 게이트는 남는다. 첫 창의 base 이동량에는 녹화 시작 hitch 구간이 포함될 수 있다. 성능 비교가 아니다.

**Rewind 실제 과거 시점 조회**

- 사용자가 첫 녹화를 열고35초 EvaluationProgram을 선택했다. 직접 읽은 작업10개에는 공중 분기의 Sequence Keyframe(SampleTime1.204161), 기존 준비/Strafe/Offset Root/최종 구면 보정과 진단이 있었고 **Control Rig task는 없었다**. 이 안정 공중 시점에서 발 배치 평가가 해제됨을 확인했다. 매 공중 프레임이나0.2초 블렌드아웃의 정확한 길이를 실측한 것은 아니다.
- 같은 녹화40초에는 작업12개 중 마지막 `[11] Anim Next Control Rig Task`가 `LNPFinalize Spherical Offset Root Task`와 진단 뒤에 있었다. 지상 복귀 후 실제 CR 실행과 순서를 확인했다. 별도 Pose History/원본 숨김 ABP를 읽은 결과가 아니다.
- 첫 발판 녹화5초의 AG Native Properties를 직접 읽었다. `OffsetRoot.UpdateSerial=30841`, `OnGround=true`, `OnMovingBase=true`, `MovementBase=...BP_PyramidExteriorPanel_C_0.PanelMesh`. `FootPlacement.Enabled=true`, `ForceReset=false`, `AllowFootPinning=false`, `AllowSlopeWarping=true`, `GroundNormal=(0.625998,-0.743638,0.234797)`이었다. 검증한 두 링크에 따라 핀 고정·WorldZDamper가 모두 꺼지고 경사 보정은 유지된다. CR 내부의 순간 컨트롤 상태를 전부 읽었다는 뜻은 아니다.
- 사용자가 녹화 실패를 의심했으나 세 파일의 존재/크기·정상 시작/종료 로그와 위 평가 작업/Native Properties가 실제로 확인됐다. 이후 새 PIE를 시작해 Rewind UI가 빈 상태로 바뀐 것과 저장 파일 부재를 구분한다. 재녹화하지 않고 기존 파일로 검증했다.

고정 로그·집계·조회 근거는 `Saved/AnimationOverhaul/2026-10-26_UAFFootPlacement/`의 `SlopeEquator.log`, `SlopeEquatorSummary.json`, `Panel.log`, `PanelSummary.json`, `Rewind35AirTasks.json`, `Rewind40GroundTasks.json`, `RewindPanel5Inputs.json`이다. Saved는 git 제외다. 편집/Legacy 전환/신규 v2 생성 백업과 일회성 스크립트는 `2026-10-26_PreFootPlacement/`에 있다.

**한계와 다음 승인 경계.** 착지·CVar 재활성 리셋은 코드/배선과 튐 없는 관찰·공중→지상 task 복귀로 확인했으며 `ForceReset=true`인 정확한 한 프레임은 직접 조회하지 않았다. 짧은 발판 녹화에는 공중 프레임이 없어 발판 점프/착지의 직접 기록 근거가 없다. 큰 회전 base·본별 출력 수치·전체 무기/몽타주·적/네트워크·완전 동등성 판정도 남는다. 기본 ABP/CR·Chooser·PSD·GASP 소스는 변경하지 않았다. 최종 후보는 신규 v2이며 Legacy는 남기지 않았다. **5-1 미완료·5-2 미착수·Go/No-Go 보류.** 다음 제안은 ResetRoot/CopyBone의 남은 계약 대조이며 사용자 진행 확인 전 착수하지 않는다. 종료 조회 뒤 Slate observer는 해제했다.

## 16. ResetRoot/CopyBone 잔여 계약 대조(2026-10-27, 단위 완료)

사용자의 새 Phase 5 진행 요청으로 이 대조 단위에 착수했다. 이전 인계의 조사 승인 대기는 해소됐으며 이후 단위는 다시 진행 확인을 받는다. 시작 시 PIE는 종료 상태였고 작업 트리에는 기존 사용자 에셋4개만 변경돼 있었다.

### 실제 계약과 엔진 경로

- 기본 `/Game/Characters/ABP_Main_UEFN`과 숨김 `/Game/Characters/UAFSpike/ABP_SourceUAFSpike`의 실제 MM 중첩 그래프를 읽었다. 두 CopyBone은 `foot_l/r → ik_foot_l/r`, 컴포넌트 공간이며 이동·회전·스케일 복사가 모두 false다. 기본 ABP의 세 플래그 핀도 false·연결 없음이다. `AnimNode_CopyBone.cpp`는 이 경우 즉시 반환한다. 따라서 UAF에 대응 CopyBone을 새로 넣을 동작 계약은 없다. 기존 발 배치 CR은 `UseIKBoneTargets=false`로 FK 목표를 쓰며 이 판정을 보강한다(§15).
- 기존 ResetRoot는 알파1이고 기본 ABP의 Alpha 핀도1·연결 없음이다. `Engine/Source/Runtime/AnimGraphRuntime/Private/BoneControllers/AnimNode_ResetRoot.cpp`는 루트를 기준 포즈로 되돌리면서 루트 직계 자식의 컴포넌트 공간 변환을 보존한다. 누적 Offset Root 상태나 루트모션 속성을 초기화하는 노드가 아니다.
- `CHT_PoseSearchDatabases_UEFN`의 직접 PSD 의존52개에서 GASP 애니메이션 의존을 모아 중복 제거한 시퀀스319개를 실제 조회했다. 전부 `bEnableRootMotion=true`, `RootMotionRootLock=RefPose`, `bForceRootLock=true`다. 별도 공중 `/Game/Characters/Retargeted/Lyra/MM_Unarmed_Jump_Fall_Loop_UEFN`도 같은 값이다. 폴더 전체의 사용하지 않는 PSD까지 포함한 최초 탐색과 최종 Chooser 범위를 구분한다. 이 수치는 모든 무기/몽타주·향후 반입 시퀀스를 포함하지 않는다.
- UAF `UAFAnimGraph/Private/EvaluationVM/Tasks/PushAnimSequenceKeyframe.cpp`와 `DecompressionTools.cpp`에서도 위 잠금 설정을 사용한다. `FRootMotionReset::ResetRootBoneForRootMotion`의 RefPose 경로는 기준 루트 변환을 대입한다. 루트모션 추출로 잠그는 경로 외에도 현재 시퀀스들은 ForceRootLock이 모두 켜져 있다.
- 프로젝트 Steering은 루트모션 속성만 Override한다. 엔진 `UAFWarping/Private/StrafeWarpingTrait.cpp`의 `PreserveOriginalRootRotation=true` 경로는 루트를 돌리지 않고 직계 자식 회전을 보정한다. 따라서 **이 설정을 유지하는 현재 구성에서는 Offset Root 전에 루트를 다시 RefPose로 돌리기 위한 별도 ResetRoot가 불필요하다**는 정적 판정이다. Offset Root 뒤에 추가하면 의도한 누적 오프셋을 지우므로 그 위치에 넣지 않는다.

### 수동 설정 확인·완료 판정과 검증 한계

- 사용자가 `AG_LocomotionSpike`의 Strafe Warping 스택에서 `Preserve Original Root Rotation`이 **체크돼 있음**을 직접 확인했다. §14의 true 설정 기록과 일치한다. 값 변경·PIE·새 녹화 없이 이번 최신 설정 확인을 마쳤다.
- 별도 ResetRoot 생략 판정은 위 시퀀스 설정·현재 처리 순서·Preserve=true를 전제로 한다. 향후 RootLock 설정이 다른 클립이나 루트를 변경하는 보정을 넣으면 다시 대조한다.
- **기존 ABP와 모든 본 포즈가 같다는 판정은 아니다.** 기존 OrientationWarping→ResetRoot는 회전된 자식의 컴포넌트 공간 변환을 보존하지만 기본 UAF Preserve 경로는 자식 회전만 바꾼다. 자식 이동·발/척추 처리·샘플별 보정 차이는 §14의 동등성 한계로 유지한다. 이 차이는 ResetRoot 하나를 붙이는 것으로 해소되지 않는다. CR 뒤 본별 포즈·모든 전환·큰 회전 base·적/네트워크·몽타주 게이트도 남는다.
- 정적 대조 동안 C++/에셋 변경·저장·빌드·PIE·새 녹화를 하지 않았다. 불필요한 ResetRoot/CopyBone 노드·추가 런타임 trait·진단 장치도 만들지 않았다. 조회용 Slate observer는 해제했다. **이번 대조 단위는 완료이며 5-1 미완료·5-2 미착수·Go/No-Go 보류다.**
- 로컬 근거: `Saved/AnimationOverhaul/2026-10-27_ResetRootCopyBone/ContractInspection.json`(319개별 경로·설정/스파이크 ABP 노드), `MainNodePins.json`(기본 ABP 실제 핀/연결). Saved는 git 제외다.

다음 제안 단위는 **추가 전환 표본(Pivot·Sprint Turns·GaitTransitions) 검증**이다. 기존 녹화에서 공급/수신 입력과 실제 선택을 먼저 대조하고 부족한 동작만 수동 플레이로 요청한다. 사용자 진행 확인 전 다음 단위에 착수하지 않는다.

## 17. 추가 전환 표본 검증(2026-10-27, 단위 완료)

사용자의 “계속 진행하자”로 착수했다. 성공 기준은 세 종류의 추가 표본에서 공급 로그·AG 수신 Databases/InterruptMode/BlendTime·실제 UAF 검색 결과를 구분해 대조하는 것이다. 특정 전환 DB를 반드시 선택하게 만들기 위해 조건·비용·인터럽트를 바꾸지 않는다. 실제 선택이 없으면 그 사실과 표본 한계를 기록한다. 새 C++·에셋 변경·계측/테스트 장치 구현 전에 기존 기록을 우선 사용한다.

### 기존 기록 탐색·수동 요청

- 시작 시 PIE=false·`LNP.Anim.DebugLocomotion=0`을 MCP로 확인했다. 기존 고정 로그4개를 탐색해 아래 세 표본을 찾았다. 공급 `[AnimB1]`은 배열 변경 시 로그이고 `[AnimB6]`는 숨김 ABP 선택이다. 약1초 간격 `[UAFSteering]`에는 대상 DB의 선택 표본이 없어 실제 UAF 선택 여부는 Rewind로 직접 읽는다. 로그 미검출을 미선택 확정으로 해석하지 않는다.

| 종류 | 저장 녹화·대략 시간 | 공급 배열/정책 |
|:---|:---|:---|
| Sprint Turns | `20261025_133940695.utrace` 약125.51초 | Sprint Starts + Sprint Turns, InterruptOnDatabaseChange, 0.5초 |
| GaitTransitions | 같은 녹화 약71.93초 | Run F GaitTransitions + Run F Loops, DoNotInterrupt, 0.5초 |
| Run Pivot | `20261026_171514977.utrace` 약79.38초 | Run RL Starts + RL Loops + RL Pivots, InterruptOnDatabaseChange, 0.5초 |

- 시간은 로그의 Trace 연결 시각과 입력 로그 시각의 차이로 찾은 탐색 기준이며 실제 Rewind 프레임과의 정확한 일치를 아직 확인하지 않았다. 후보 파일 존재·크기를 확인했다. Sprint Turns는 착지 바로 뒤 속도7의 표본이며 지속 고속 회전 시험으로 확대하지 않는다. Pivot도 속도26·방향L→R·예측 회전−81° 표본이다. 두 표본의 기본 ABP는 이후 Starts를 선택했지만 이를 UAF 결과로 대체하지 않는다.
- GaitTransitions는 Sprint→Run 한 프레임 조건이고 다음 공급 프레임에 Run F Loops 단독으로 돌아간다. 기존 C++는 보행 변화만으로 인터럽트하지 않는다. Sprint Turns는 Pivot 술어가 true일 때 Sprint Loops 대신 나오는 기존 Chooser 계약을 유지한다.
- **현재 수동 요청:** 사용자가 첫 녹화 약125.5초의 `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_* → AG_LocomotionSpike`를 선택해 Native Properties를 표시한다. 다음으로 인접 프레임 입력과 UAF Pose Search를 읽는다. 새 PIE·녹화·코드/에셋 변경·빌드는 아직 없다.
- 후보와 공급 근거는 `Saved/AnimationOverhaul/2026-10-27_AdditionalTransitions/Targets.json`에 보관했다. **이번 단위 진행 중·5-1 미완료·5-2 미착수·Go/No-Go 보류다.**

### Sprint Turns 입력·실제 검색 확인

- 사용자가 첫 녹화 약125.5초의 AG Native Properties를 표시했다. 최초 표본은 Jump 단일 배열·DoNotInterrupt·0.5로 아직 공중이었다. 프레임 이동 버튼 호출 뒤 표시 변화가 확인되지 않아 반복 자동화하지 않고 사용자에게 약125.55~125.65초 수동 스크럽을 요청했다. 정확한 프레임 시각은 직접 읽지 않았다.
- 사용자 이동 후 UAF `UAFAnimGraph_7 → AG_LocomotionSpike`의 수신 배열을 순서대로 `[PSD_Stand_Sprint_Starts_UEFN, PSD_Stand_Sprint_Turns_UEFN]`, InterruptOnDatabaseChange, BlendTime0.5로 직접 읽었다. 공급 로그의 배열 순서·정책과 일치한다.
- 같은 시점의 UAF Pose Search Active Pose는 `PSD_Stand_Sprint_Starts_UEFN / M_Relaxed_Sprint_Start_F_Lfoot`, 표시 시간0.07초·비용0.168, InterruptOnDatabaseChange였다. 실제 후보표에는 `PSD_Stand_Sprint_Turns_UEFN / M_Relaxed_Sprint_Turn_090_L_Rfoot`도 있으며 표시 비용2.293·시간0.33초였다. 이 표본은 Turn 입력 전달·실제 후보 평가를 확인하지만 Turn 클립 재생을 확인한 것은 아니다. 숨김 ABP의 Start Rfoot 선택과 UAF의 Lfoot 선택이 같아야 한다고 요구하지 않는다.
- 착지 직후 저속 표본·입력 해제 인접 프레임의 AG 직접 조회 미실시·현재 최신 Strafe/CR 전의 과거 구성이라는 한계를 유지한다. 이번 결과를 최신 전체 포즈 회귀나 지속 고속 Sprint Turn 통과로 확대하지 않는다. 로컬 근거는 `SprintTurnsInspection.json`이다.
- **현재 수동 요청:** 같은 녹화 약71.9~72.1초의 AG Native Properties에서 GaitTransitions+Run F Loops 두 DB가 보이는 프레임을 찾는다. GaitTransitions와 Pivot 검증이 남아 단위는 미완료다. 에셋 변경·저장·빌드·새 PIE/녹화는 없다.

### GaitTransitions 수신 확인·선택 조회 대기

- 사용자 수동 이동 후 같은 녹화의 AG Native Properties에서 `[PSD_Stand_Run_F_GaitTransitions_UEFN, PSD_Stand_Run_F_Loops_UEFN]` 순서·DoNotInterrupt·BlendTime0.5를 직접 읽었다. 공급 로그와 배열/정책이 일치한다. 정확한 프레임 시각이나 인접 프레임 입력 해제는 이번 조회에서 읽지 않았다.
- UAF Pose Search를 자동 클릭했으나 Details가 AG 입력에 머물러 있어 실제 선택 근거로 사용하지 않았다. **현재 요청:** 시간을 유지하고 사용자가 UAF Pose Search 트랙을 직접 선택해 Active Pose·후보표를 표시한다. 로컬 입력 근거는 `GaitTransitionsInspection.json`이다.
- 엔진 MM trait는 검색을 건너뛴 프레임에도 요청 DB와 진행 DB를 trace에 남긴다. 따라서 트랙/후보 DB 이름만으로 그 프레임의 새 검색 실행이나 전환 클립 재생을 판정하지 않는다. 실제 Active/Continuing Pose와 검색 상태를 구분해 읽는다. GaitTransitions 선택 조회·Pivot이 남아 단위는 미완료다.

### GaitTransitions 실제 검색·진행 포즈 유지 확인

- 사용자가 시간을 유지한 채 UAF Pose Search를 직접 선택했다. Active Pose와 Continuing Pose는 모두 `PSD_Stand_Sprint_Loops_UEFN / M_Relaxed_Sprint_Loop_F`, 표시 비용2.276이었다. MM 상태는 DoNotInterrupt·Elapsed Pose Search Time0.0초·Asset Player Time1.338866초였다.
- 후보표의 `PSD_Stand_Run_F_GaitTransitions_UEFN / M_Relaxed_Transition_Sprint_to_Run_Lfoot`는 표시 비용3.537·시간0.5초였고 그 외 전환 후보들도 표시됐다. 입력에 없는 이전 Sprint DB의 진행 포즈를 유지했으며 그 비용이 표시된 전환 후보보다 낮았다. 전환 입력 전달·후보 평가를 확인한 것으로, 전환 클립을 실제 재생했다고 판정하지 않는다. 모든 프레임의 검색 동작이나 모든 보행 조합으로 확대하지 않는다.
- 로컬 근거는 `GaitTransitionsInspection.json`과 `GaitTransitionsSearch.json`이다. 이번에는 인접 프레임의 입력 해제를 AG에서 재조회하지 않았다. 공급 로그의 한 프레임 해제와 구분한다.
- **현재 수동 요청:** `20261026_171514977.utrace`의 약79.35~79.55초에서 AG Native Properties의 Run RL Starts+Loops+Pivots 세 DB 표본을 찾는다. Pivot 입력/실제 선택이 남아 단위는 미완료다. 코드/에셋 변경·저장·빌드·새 PIE/녹화는 없다.

### Run Pivot 입력·후보 검색 확인

- 사용자가 두 번째 녹화로 이동한 뒤 AG 수신 배열을 `[PSD_Stand_Run_RL_Starts_UEFN, PSD_Stand_Run_RL_Loops_UEFN, PSD_Stand_Run_RL_Pivots_UEFN]` 순서·InterruptOnDatabaseChange·BlendTime0.5로 직접 읽었다. 공급 로그와 일치한다. 이번 에디터 로그에서도 두 녹화의 로드 완료 경로를 확인했다.
- 같은 표본의 UAF Pose Search Active Pose는 `PSD_Stand_Run_RL_Starts_UEFN / M_Relaxed_Run_Start_RL_Lfoot`, 표시 비용0.631·Asset Player Time0.0초·Elapsed Pose Search Time0.0초·InterruptOnDatabaseChange였다.
- 최초 후보 검색어를 `Pivots`로 잘못 안내해 빈 결과가 나왔다. 로컬 `PoseSearchDebuggerDatabaseView.cpp`는 DB명이 아닌 `AssetName.Contains(Token)`으로 필터링한다. PSD의 실제 시퀀스 이름도 `...Run_Pivot_LL_RL...`(단수)임을 의존 목록으로 확인했다. 탐색 중 뒤 프레임의 진행 포즈 표시가 나왔으므로 그 값을 원래 표본의 후보 비용으로 사용하지 않았다.
- 사용자가 세 DB 프레임으로 복귀하고 검색어를 `Pivot`으로 바꾼 뒤, Active Pose/비용/인터럽트/재생시간이 위 원래 값으로 돌아온 것을 읽었다. 후보표의 `PSD_Stand_Run_RL_Pivots_UEFN / M_Relaxed_Run_Pivot_LL_RL_Lfoot`는 표시 비용2.648·시간0.87초였다(Rfoot 후보2.662도 표시). Pivot 후보 평가까지 확인했으며 Pivot 클립 재생은 확인하지 않았다.
- 로컬 근거는 `RunPivotInspection.json`, `RunPivotSearch.json`, `RunPivotCandidates.json`이다. 자동 입력은 포커스 실패로 false였고 수동 조작으로 진행했다. 엔진·C++·에셋은 수정하지 않았다.

### 이번 단위 완료 판정

| 표본 | AG 수신 배열·인터럽트·블렌드 | 실제 UAF 결과 | 표시된 해당 전환 후보 |
|:---|:---|:---|:---|
| Sprint Turns | Sprint Starts+Turns · InterruptOnDatabaseChange · 0.5 | Start F Lfoot · 비용0.168 | Turn090 L Rfoot · 비용2.293 |
| GaitTransitions | Run F GaitTransitions+Loops · DoNotInterrupt · 0.5 | Sprint Loop F 유지 · 비용2.276 | Sprint→Run Lfoot · 비용3.537 |
| Run Pivot | RL Starts+Loops+Pivots · InterruptOnDatabaseChange · 0.5 | RL Start Lfoot · 비용0.631 | Pivot LL→RL Lfoot · 비용2.648 |

**세 추가 표본의 입력 전달·정책·후보 검색 대조 단위 완료.** 기존 녹화·공급 로그·현재 C++와 실제 UAF 입력/검색을 대조했으며 새 코드/에셋·저장·빌드·PIE·녹화·계측 장치를 만들지 않았다. 종료 시 PIE=false·조회 Slate observer 해제를 확인했다. Rewind에는 마지막 Pivot 필터와 과거 시점이 남아 있다.

**한계:** 해당 세 표본에서 목표 전환 클립은 선택되지 않았다. Sprint Turns/Pivot은 저속 경계 표본이고 두 기록은 최신 발 배치 연결 전 구성이다. 현재 포즈 품질·지속 고속 동작·모든 보행/방향·모든 프레임의 전달 동등성·인접 프레임 해제까지 통과한 것은 아니다. 표시 비용은 각 시점의 후보 비교이며 DB 간 일반 비용 척도나 성능 수치로 사용하지 않는다. 특정 클립을 강제하기 위해 조건/비용/인터럽트를 바꾸지 않았다. **5-1 미완료·5-2 미착수·Go/No-Go 보류다.**

다음 제안은 **큰 회전 base 및 발판 점프/착지 검증**이다. 기존 발판으로 수동 시험 가능한 범위를 먼저 확인하고 큰 회전이 불가능하면 필요한 최소 시험 구성을 설명한다. 사용자 진행 확인 전 다음 단위에 착수하지 않는다.

## 18. 큰 회전 base 및 발판 점프/착지 검증(2026-10-27, 시험 범위 내 단위 완료)

사용자의 “계속 진행하자”로 착수했다. 우선 기존 발판에서 공중 전환/동일 발판 착지와 CR 해제/복귀를 직접 기록한다. 회전은 기존 발판이 실제로 제공하는 범위를 확인하고, 부족한 축/크기의 시험을 위해 새 장치를 만들기 전에 필요한 최소 구성을 설명한다. 기본 ABP·CR·Mover·동적 지형 시스템을 바꾸는 단위가 아니다.

### 기존 패널 계약·측정 범위

- 시작 PIE=false, FootPlacement1·FootPlacementOnMovingBase0·Steering1·StrafeWarping1·DebugLocomotion0을 확인했다. 마커는 현재 에디터 월드 조회에서 찾지 못했으며 이를 런타임 패널 부재로 해석하지 않는다.
- `ALNPMovingPanel`은 마커에서 받은 위치 곡선을 등속 왕복한다. 열린 경로의 자세는 양 끝의 중력 수평 자세를 거리 비율로 Slerp하며, 닫힌 경로는 원점 자세 고정이다. 별도의 제자리 Yaw 회전 설정은 없다. 경로는 활성화 후 변경을 지원하지 않으므로 실행 중 Path를 덮어쓰지 않는다.
- `Art/Octants/Pyramid_00/Placement.json`과 조립용 transform 계산상 세 Lift의 끝점 Up 차이는0°, FacadeFerry는약29.336°, SkyFerry는약14.829°다. 이는 소스 배치에서 계산한 예상값이며 현재 월드의 실제 마커/런타임 패널 자세를 측정한 근거가 아니다. 큰 회전을 전부 새 장치로만 시험해야 한다고 결론 내리지 않는다.
- **진단 범위 보완:** `[UAFRoot] maxBaseDeg`는 `BasedMovementDelta`의 프레임 회전량이다. 공급은 Mover의 `OnBasedMovementApplied`가 보낸 TransformDelta를 누적한 값이며, 엔진 `BasedMovementUtils.cpp`는 base 이동으로 Actor에 적용한 델타를 이벤트로 보낸다. 패널 Actor 자체의 전체 회전량이나 경로 양 끝 회전각과 동일하지 않다. 앞선 §15의 작은 값만으로 실제 패널 회전이 작았다고 확대하지 않는다. 기존 큰 회전 검증 미완료 판정은 유지한다.

### 현재 수동 요청·검증 기준

1. 1인 PIE에서 `LNP.Anim.DebugLocomotion 2`, `LNP.DynamicTerrain.PlaceRider`로 가까운 패널에 탑승한다. 기존 FootPlacement/MovingBase/Steering/Strafe 값은 유지한다.
2. Rewind 녹화를 시작해 입력 없이 약10초 탑승, 중앙 점프→같은 발판 착지3회, 짧게 걷고 멈춘 뒤 점프/착지1회를 수행한다. 발/골반 튐·착지 급회전·발판 앞섬/밀림을 보고한다. 어려우면 자동 시험 장치를 만들지 않고 즉시 어려운 지점을 확인한다.
3. 녹화만 종료하고 PIE는 유지한다. 로그의 지상/공중·base 전환·실제 발판 신원과 구면 제한을 확인하고, 녹화에서 공중 CR 해제/착지 복귀를 읽는다. 이 진단은 CR 앞 수치라는 §15 한계를 유지한다.

**현재 수동 시험 결과 대기.** 새 코드/에셋 변경·빌드·시험 장치를 만들지 않았다. 소스 조사 근거는 `Saved/AnimationOverhaul/2026-10-27_BaseJumpRotation/SourceInspection.json`이다. 이번 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 사용자 관찰·새 기록의 로그 확인

- 사용자는 요청한 발판 시험을 완료하고 이상 없음을 보고했다. 새 파일은 `RewindDebugger-LootNPop-Editor-20261027_153613941.utrace`(148,240,307바이트), Trace 연결15:36:13.996~채널 복원/기록 종료15:37:22.448 KST, 약68.452초다. 고정 로그는 `PanelJump.log`, 집계는 `PanelJumpSummary.json`에 보관했다.
- 녹화 구간에 지상→공중/공중→지상17쌍이 있다. 끝 시각이 녹화 안인 Root 창68개는2,661프레임, 지상/동적 base2,147·공중514프레임, Moving857프레임이다. base 문자열은 `BP_PyramidExteriorPanel_C_0.PanelMesh`와 공중의 None이다. 첫 창이 녹화 전 프레임을 일부 포함할 수 있고 마지막 창 뒤 일부 프레임은 합계에 들어가지 않으므로 녹화 전체 프레임 수로 해석하지 않는다.
- CR 앞 누적/출력 최대 거리30cm·Yaw90°·Tilt0.000°. 전달 based 델타 최대 회전0.0023°다. 구간 내 LogAnimation Error0·assertion/ensure/fatal0, 기존 Foley 경고58회다. 기록 종료 직후15:37:22.465에 LogTrace error64가 있지만 파일 존재/크기·분석 종료와 구분하며 과거 task 재조회로 실제 기록을 확인할 예정이다.
- PIE가 계속 실행 중일 때 실제 패널 Actor 변환을3회 읽었다. 샘플 사이 쿼터니언 각도 차이 최대10.896426°로 패널 자체의 여러 도 회전이 있음을 확인했다. 이는 녹화 후 라이브 조회의 하한이며 경로 전체 각도·녹화 프레임의 측정·큰 Yaw 회전을 통과한 근거가 아니다. `LivePanelRotation.json`에 실제 변환과 계산을 보관했다.
- **현재 수동 요청:** PIE를 일시정지하고 새 녹화 약26.3초의 UAF EvaluationProgram Tasks를 표시한다. 첫 점프 진입은 녹화 약25.946초, 착지는26.702초로 공중26.3·복귀27.3초를 좁혀 읽는다. 수치는 공급 로그 기준의 탐색 시각이며 실제 프레임 확인 전이다. CR task의 해제/복귀와 큰 회전의 나머지 범위는 아직 미판정이다.

### Rewind 공중/재착지 조회·기존 발판 소단위 종료

- 사용자 수동 스크럽으로 약26.3초의 실제 UAF EvaluationProgram 작업10개를 읽었다. `MM_Unarmed_Jump_Fall_Loop_UEFN`의 Sequence Keyframe(SampleTime0.308793), History·기존 준비/Strafe/Offset Root/최종 구면 보정·진단이며 **Control Rig task는 없다**. 안정 공중에서 발 배치 평가가 해제됨을 확인했다.
- 같은 녹화 약27.3초 재착지 뒤에는 작업12개였다. 마지막 `[11] Anim Next Control Rig Task`가 최종 구면 보정과 진단 뒤에 있다. 실제 CR 복귀를 확인했으며 정확한 블렌드 길이·ForceReset=true 한 프레임·17개 점프 전체의 task 배열까지 읽은 것은 아니다.
- 첫 Root 창을 제외하고 frames=ground=based·moving0인 안정 탑승 창30개/1,167프레임의 CR 앞 최대 거리는0.143cm·Yaw83.46°·Tilt0°였다. 이 부분집합은 로그 기반 정지 상태이며 사용자 입력 원본의 전 프레임 대조는 아니다. 전체 혼합 창의30cm 최대와 구분한다.
- 새 파일에서 공중/지상 과거 task를 실제로 읽었으므로 기록 종료 직후 LogTrace error64를 녹화 전체 실패로 해석하지 않는다. 해당 오류와 기존 Foley 경고는 억제하거나 수정하지 않았다.
- 근거는 `RewindAirTasks.json`, `RewindGroundTasks.json`, `PanelJump.log`, `PanelJumpSummary.json`, `LivePanelRotation.json`이다. 종료 시 PIE=true·DebugLocomotion2를 재확인했고 사용자는 일시정지 요청 후 Tasks를 표시했다. 일시정지 상태 자체는 별도 API로 확인하지 않았다. Slate observer는 해제했고 에셋 저장·코드/에셋 변경·빌드는 없다.

**기존 발판의 점프/동일 발판 착지·CR 해제/복귀·구면 루트 제한과 실제 패널 회전 범위 확인 소단위 완료.** 사용자 관찰은 이상 없음이며 실제 패널 회전은 라이브 표본 사이10.896426° 하한을 확인했다. 이 회전은 경로를 따른 기울기 변화이며 제자리에서90° 이상 도는 Yaw 회전 시험은 아니다. CR 뒤 전체 본 포즈·큰 Yaw·적/네트워크·최종 게이트는 남는다. **§18 전체·5-1 미완료·5-2 미착수·Go/No-Go 보류다.**

다음 구현 제안은 **큰 Yaw 회전용 최소 시험 발판 구성·검증**이다. 시험용 BP 하나에 메시와 기본 RotatingMovement를 사용해 현재 중력 Up 둘레로90° 이상 회전하는 후보를 검토한다. 기존 동적 패널 경로·Mover·기본 맵/에셋을 바꾸지 않는 방향이며 구체 API/배치/복구 절차는 승인 후 조사한다. 사용자 진행 확인 전 다음 조사·구현에 착수하지 않는다.

### 큰 Yaw 발판 구현·초기 탑승 시험 대기

사용자의 추가 진행 확인으로 이 구현 단위에 착수했다. 성공 기준은 실제 동적 base 위에서90° 이상 Yaw 회전 노출·구면 Up 유지·루트 제한·발/골반/몸통의 튐 여부를 확인하는 것이다. 기존 패널 경로·Mover·ABP/CR은 변경하지 않는다.

- `/Game/Characters/UAFSpike/BP_UAFYawTestPlatform`을 StaticMeshActor 부모로 만들었다. 루트 StaticMeshComponent0은 엔진 Cube·scale(16,16,0.3)·Movable·LNPDynamicTerrain 충돌(QueryAndPhysics/WorldDynamic)·물리/중력 시뮬레이션 끔이다. SCS `YawRotation`은 기본 RotatingMovementComponent이며 로컬 Yaw30°/초·피벗0·Root 자동 선택이다. 엔진 구현은 `OldRotation * DeltaRotation`으로 회전하고 UpdatedComponent/Owner의 틱 의존성을 설정한다.
- BP 컴파일 호출 정상 종료 후 메시/충돌/회전 설정을 재조회했고 해당 BP만 저장했다(34,175바이트). C++/엔진/도구 변경이나 전체 빌드는 없다.
- 에디터 SceneTools의 스폰은 PIE 중 거부되고 에디터 월드를 사용한다. PIE 전용 Python 스폰 스크립트를 준비했으나 첫 실행은 `MathLibrary.make_transform` 미노출, 생성자로 고친 두 번째 실행은 `GameplayStatics.begin_deferred_actor_spawn_from_class` 미노출로 **Actor 생성 전에** 실패했다. `BlueprintInternalUseOnly` 함수의 Python 제외도 로컬 엔진에서 확인했다. 우회 API나 추가 C++를 만들지 않고 이 경로를 중단했으며 생성했던 `Scripts/AnimOverhaul/SpawnUAFYawTestPlatform.py`는 제거했다.
- 대신 실제 플레이어 위치/자세·캡슐 반높이96cm를 읽고 PIE 종료 후 Actor 하나를 에디터에 임시 배치했다. Actor label=`UAFYawTestPlatform_Temporary`, 위치=(-1748.459333,1602.789693,-29846.663008), 회전=(-3.625604,-5.385914,-2.740603), scale(16,16,0.3). 윗면은 기존 발보다50cm 높고 로컬 Z는 당시 중력 Up이다. 맵은 저장하지 않았다.
- 기존 `LNP.Debug.TeleportToOctant`의 좌표 변환·반지름30000 설정으로 윗면 좌표를 계산했다. **현재 수동 요청:** 새 1인 PIE에서 `LNP.Debug.TeleportToOctant 4 36002.142 -161.222 74.236` 실행, 착지 후 입력 없이5초 탑승·회전/포즈 정상 여부 보고. 아직 새 녹화는 요청하지 않았다. 명령은 폰을 새로 생성해 윗면150cm 위에 놓는다.
- 정확한 정리 대상 Actor ref와 배치 계산은 `Saved/AnimationOverhaul/2026-10-27_YawPlatform/EditorPlacement.json`, BP 설정은 `Setup.json`, 실패까지 로그는 `BeforeEditorPlacement.log`다. 시험 후 PIE 종료·해당 에디터 Actor만 제거하고 맵 전체를 저장/되돌리지 않는다. **초기 런타임 미검증·이번 구현 단위와§18/5-1 미완료·5-2 미착수·Go/No-Go 보류다.**


### 큰 Yaw 관찰·로그 확인 및 에디터 재시작(같은 날, 정리 진행 중)

- 사용자는 초기 회전/탑승 정상을 보고했다. 런타임 RotatingMovement의 로컬 Yaw30°/초·실제 루트 UpdatedComponent를 재조회했다. 라이브 Actor 두 자세의 쿼터니언 차이는91.27487998455808°·Up 차이0°다. 이는 실제 큰 회전 노출의 근거이며 녹화와 동기화된 프레임 적분은 아니다. 근거는 `LiveYawRotation.json`이다.
- 사용자가 총기는 카메라 방향을 유지하려고 제자리 회전을 반복하고 현재 롱소드는 발판과 함께 회전한다고 설명했다. 수동 회전 입력 없는 탑승 기준으로 **현재 롱소드 상태**의15초 정지→10초 짧은 걷기/멈춤→5초 정지를 녹화했고 이상 없음을 보고했다. 총기 동작을 결함으로 판정하거나 총기/카메라 유지 분기까지 검증했다고 해석하지 않는다.
- 새 녹화 `20261027_163122363.utrace`는109,352,237바이트, 연결16:31:22.503~종료16:32:03.972 KST의41.469초다. 끝 시각이 녹화 안인 Root 창41개/1,846프레임은 모두 지상/base, Moving500프레임이며 실제 base는 시험 발판의 StaticMeshComponent0 하나다. 첫 창은 녹화 전 프레임을 일부 포함할 수 있고 마지막 창 뒤 일부 구간은 합계에서 빠지므로 정확한 전체 녹화 프레임 수로 쓰지 않는다.
- CR 앞 최대 거리30cm·Yaw88°·Tilt0°. 전달 based 델타 최대7.806cm·8.5424°는 발판 총 회전량과 다르다. 첫 창 제외 frames=ground=based/moving0인25창1,143프레임의 최대 거리21.766cm는 정지 직후 감쇠를 포함한다. 마지막 정지6창(UTC07:31:58.015~07:32:03.083)은 최대 거리0cm·Yaw0.16°·Tilt0°다. 전체 본 배열이나 CR 뒤 수치로 확대하지 않는다.
- 녹화 중 LogAnimation Error0·assertion/ensure/fatal0, 기존 Foley 경고78회다. 종료6ms 뒤 LogTrace error64가 있으며 아직 해당 파일 확인과 구분한다. 고정 로그/집계는 `YawRecord.log`/`YawRecordSummary.json`이다.
- 사용자가 한도 초기화 중 에디터를 재시작하고 AG_LocomotionSpike와 임시 Actor의 Unsaved 항목을 저장했다고 보고했다. 재연결 후 PIE=false·TestMap03·동일한 정확한 임시 Actor 참조를 확인했다. 원래 사용자 에셋4개 해시는 `PreservedAssetHashes.json`과 모두 같다. 발판은 `Content/__ExternalActors__/Maps/TestMap03/E/N2/QRLU7Z5MMC4DQV6760VJ46.uasset`으로 저장됐으며 맵 전체를 되돌리지 않는다.
- AG는 HEAD391,920바이트→재저장415,245바이트로 달라졌다. 바이트/문자열 차이만으로 의미 변경을 단정하지 않는다. 현재 AG·HEAD 원본·외부 Actor를 `AfterRestart/`에 백업했고 읽기 전용 그래프 Export로 기존 배선/기본값을 대조한다. 자동 입력 포커스 실패로 사용자에게 Cmd 명령 실행을 즉시 요청했다. 에셋 변경/컴파일/저장을 하지 않는 조회다.
- 재시작 후 Rewind 화면 약10초의 UAF EvaluationProgram에서12개 task·최종 구면 보정/진단 뒤 마지막 Control Rig를 읽었다. 열린 파일 신원은 사용자 확인 대기다. 저장된 AG 검사와 임시 Actor 정리가 남아 **이번 단위/§18·5-1 미완료·5-2 미착수·Go/No-Go 보류**를 유지한다.


### 저장된 AG 계약·Rewind 파일 확인 및 임시 Actor 제거(같은 날)

- 사용자가 열린 파일이 `20261027_163122363.utrace` 약10초임을 확인했다. 실제 UAF EvaluationProgram은12개 task이며 `[9] LNPFinalize Spherical Offset Root Task → [10] LNPInspect Spherical Offset Root Task → [11] Anim Next Control Rig Task`다. 사용자 파일/시점 확인과 직접 읽은 배열은 `RewindYawTasks.json`에 보관했다. 기록 종료 직후 error64를 전체 녹화 실패로 해석하지 않는다.
- 사용자 Cmd 실행으로 현재 AG를 읽기 전용 Export했다. 발 배치 연결11개(입력9/포즈2), 공개핀10개, ControlRig v2·Legacy 없음, Bool 알파·진입0.3/해제0.2초, DoRaycast=true·UseIKBoneTargets=false, 기존 CR 참조, PreserveOriginalRootRotation=true가 기존 문서 계약과 일치한다. 근거는 `AfterRestart/AG_CurrentExport.txt`, `AGContractInspection.json`, `AGExposedPinsInspection.json`이다. 완전한 HEAD 의미 차이 판정이나 재시작 후 새 PIE 시험을 한 것은 아니다. 검증한 구성을 유지하며 AG를 원복/재저장하지 않았다.
- PIE=false에서 기록된 정확한 임시 Actor만 SceneTools로 제거했다(true). 재조회 결과 Actor 목록은 비었다. 외부 Actor 파일은 백업 해시와 같지만 에디터의 파일 잠금으로 직접 삭제되지 않았다. 반복 시도나 맵 전체 저장 대신, 해당 빈 패키지 하나만 EditorLoadingAndSavingUtils.save_packages에 전달하도록 사용자에게 요청했다. 로컬 FileHelpers는 빈 패키지를 디스크 삭제 대상으로 처리한다. **삭제 반영 확인 전이며 이번 단위는 아직 정리 중이다.**
- 재시작 후 현재 CVar는 FootPlacement1/MovingBase0/Steering1/Strafe1/DebugLocomotion0이다. 에이전트가 값을 바꾸지 않았다. 원래 사용자 변경4개 해시는 여전히 동일하며 다음 단위는 아직 시작하지 않는다.


### 정리 완료·이번 단위 최종 판정(같은 날)

- 사용자가 해당 빈 외부 패키지 하나에 save_packages를 실행했다. 이어 정확한 외부 Actor 파일 부재, SceneTools 조회 결과 빈 배열, PIE=false를 확인했다. 기존 사용자 변경4개는 `PreservedAssetHashes.json`과 여전히 모두 일치한다. 맵 전체 저장/원복, AG 원복/재저장, 새 C++·빌드·커밋/푸시는 하지 않았다. 재사용할 시험 BP는 남겼다. 정리 근거는 `Cleanup.json`이다.
- **§18 완료:** 기존 이동 패널의 점프/동일 발판 착지·공중 CR 해제/복귀와, 별도 로컬 Up Yaw 발판의91.274880° 라이브 회전·Up 유지·롱소드 정지/짧은 걷기 관찰·CR 앞 루트 제한·녹화 약10초 지상 CR 실행을 확인했다. 에디터 재시작 후 저장된 AG의 문서화된 계약도 보존됐다.
- **범위 한계:** 큰 Yaw 시험은 배치 위치인 남극 부근(위도 약−85.46°)·현재 롱소드 기준이다. 적도 큰 Yaw, 총기 카메라 유지 분기, 전환 클립 실제 재생/지속 고속, CR 뒤 전체 본 포즈 동등성·ForceReset 정확한 한 프레임·적/네트워크·5-2 게이트까지 통과한 것은 아니다. 기존 패널 점프와 큰 Yaw의 모든 조합을 시험한 것도 아니다. 이번 단위 성공을 전체 마이그레이션 승인으로 확대하지 않는다.
- **5-1 미완료·5-2 미착수·Go/No-Go 보류.** 다음 제안은 **5-1 종료 판정·남은 비교 범위 정리**다. 최초 조건에 완료 근거를 대응하고 실제로 필요한 부족 표본/비교만 정한다. 다음 단위 진행 확인 전 조사/구현하지 않는다.

## 19. 5-1 종료 판정·남은 비교 범위 정리(2026-10-27, 단위 완료)

사용자의 Phase 5 재개 요청으로 최초 조건(§2)과 기존 완료 근거를 대조했다. **5-1의 최소 기능 조건은 충족했다.** 기본 UAF Strafe와 기존 OrientationWarping의 차이가 남으므로 최종 동등성이나 Go를 선언하지 않는다. 5-2의 일곱 게이트는 계속 실행 근거로 판정한다.

### 최초 종료 조건과 근거

| 5-1 조건 | 기존 확인 근거 | 종료 판정의 범위 |
|:---|:---|:---|
| 최소 테스트 폰·UAF 출력·Mover 궤적 | §5의 시험 폰/시스템/그래프, `Current Actor → VisualMesh`, 숨김 메시 뒤 PrePhysics, 공유 궤적 전달. §13·§17의 실제 UAF 입력/검색 | 중간 공급 구성으로 기능 확인. 숨김 ABP 제거·성능 비교는 하지 않음 |
| 같은 Chooser의 PSD 배열·실제 검색 | §13의 Idle·출발·정지·착지·제자리 회전·방향 전환, §17의 세 추가 전환 입력/정책/후보 | 대표 입력의 전달·검색 확인. 목표 전환 클립을 반드시 골라야 한다는 조건은 없음 |
| 북극·적도 이동/정지/회전 | §10의 관찰·루트 제한, §14의 북극/적도 Strafe 0/1, §15의 발 배치 연결 후 북극·적도 관찰 | 단계별 완료 근거를 사용. 모든 조합을 최신 구성으로 다시 녹화하지 않음 |
| 공중 Fall_Loop 고정 | §12의 두 공중 시점/착지 실제 클립, §14의 공중 Strafe 해제/복귀, §15·§18의 공중 CR 해제/착지 복귀 | 공중 계약의 구현·대표 실행 확인. 모든 공중 프레임의 클립을 전수 조회한 것은 아님 |
| 발 배치·경사·재착지 | §15의 기존 CR/v2 배선·북극 경사/적도 관찰·실제 task, §18의 발판 점프/착지 및 재시작 후 AG 계약 | 연결·활성 조건·대표 관찰 확인. ForceReset=true 한 프레임·CR 뒤 본별 수치는 미측정 |
| 움직이는 base | §10의 병진·중복 운반 수정, §18의 실제 패널 회전/점프와 별도 로컬 Up Yaw91.274880°·Up 유지·롱소드 관찰 | 기존 패널과 큰 Yaw 각각의 범위에서 확인. 적도 큰 Yaw·총기 카메라 유지까지 교차 검증한 것은 아님 |
| 조기 No-Go 차단 여부 | §10·§14~§16에서 구면 루트·기본 Strafe·발 배치·루트 잠금의 실행/구성 경로 확인 | 현재 근거에 동등 구현이 불가능하다는 확정 사유는 없음. 알려진 처리 차이의 최종 수용 판정은 남음 |

### 남은 비교의 소속과 최소 범위

- **5-2에서 반드시 판정:** §4의 일곱 게이트. 몽타주/ANS·UAFLayering·Motion Warping/Mover·적/네트워크는 아직 미검증이다. 이미 확인한 PSD 검색과 Rewind 기록은 재사용하며 새 계약에 필요한 부분만 추가 확인한다.
- **최종 포즈·PSD 게이트의 남은 비교:** 최신 발 배치까지 포함한 UAF와 기존 ABP의 대표 이동 품질을 비교한다. 특히 지속 달리기/질주 뒤 방향·보행 전환을 포함해 §17의 저속 표본 한계를 보완한다. §14·§16의 미래 예측/샘플별 보정·발/자식 본 처리 차이가 실제 품질에 미치는 영향과 무기 통합 후 결과를 판정한다. 구체 시험 구성은 해당 5-2 단위에서 정하며 지금 새 폰·진단 장치를 만들지 않는다.
- **별도 필수 조건으로 추가하지 않음:** 모든 본/동일 프레임의 수치 일치, 모든 전환 클립의 선택, 모든 위도×장비×발판×점프 조합, ForceReset의 정확한 한 프레임 전수 조회. 기존 조건은 기능·품질 동등성이며 알고리즘의 수치 동일성이 아니다. 실제 이상이나 근거 충돌이 발견되면 관련 표본만 좁혀 보완한다.
- 특정 전환 후보가 미선택된 사실은 유지한다. 이 자체를 실패로 보거나 선택시키려고 Chooser/PSD/비용을 바꾸지 않는다. 대표 품질 비교에서 기존 경로 대비 문제가 확인되면 재현 구간을 좁혀 판정하며, 동등하게 해결할 수 없으면 AN-006의 No-Go를 적용한다.

### 실제 상태 대조·종료 정리

- 시작 작업 트리는 기존 사용자 에셋4개만 수정 상태였다. 해당4개 SHA256은 §18 `Cleanup.json`과 모두 같았다. 현재 AG도 `AfterRestart/AG_LocomotionSpike.uasset` 검사 백업과 SHA256이 같아 기존 계약 검사 근거를 재사용한다. 소스는 `LNPUAFSpikeAnimInstance`의 Chooser 다중 평가·궤적/인터럽트/발 배치 공급과 대조했다. 새 PIE·Rewind·빌드를 실행하지 않았다.
- MCP로 **PIE=false**와 TestMap03 WorldSettings의 시험용 GameMode를 확인했다. 사용자가 원래 값은 **`/Game/Gameplay/BP_LNPGameMode`**라고 알려줬다. 맵의 디스크 사본을 `Saved/AnimationOverhaul/2026-10-27_UAF51Closure/TestMap03_BeforeGameModeRestore.umap`에 보관한 뒤 `DefaultGameMode` 하나를 원래 생성 클래스로 복원·재조회했다. `save_assets`에는 `/Game/Maps/TestMap03`만 전달해 저장 성공을 확인했다. 다른 에셋 저장·맵 전체 원복은 하지 않았다.
- 저장 후에도 원래 GameMode·PIE=false·맵 dirty=false를 확인했다. 다른 사용자 데이터3개와 AG의 해시는 그대로다. 복원 전/후 해시·백업과 에디터 조회 근거는 같은 폴더의 `ClosureAudit.json`·`EditorVerification.json`이다. 이번 단위는 문서/설정 정리로 C++ 테스트·빌드·플레이를 반복하지 않았다.
- **5-1 완료·5-2 미착수·Go/No-Go 보류.** 이번 종료 판정/정리 단위는 완료다. 다음 제안은 **5-2 몽타주·ANS 최소 연결 경로 조사**로, 이중 메시의 대상 선택과 기존 슬롯/ANS 종료 계약을 먼저 확인한다. 사용자 진행 확인 전 해당 조사·구현에 착수하지 않는다. 5-2 시험 때만 GameMode Override를 다시 `BP_GameModeUAFSpike`로 임시 지정하며 복원 기준은 위 원래 값이다.

## 20. 5-2 몽타주·ANS 최소 연결 경로 조사(2026-10-27, 조사 단위 완료)

사용자의 5-2 시작 요청으로 몽타주 대상·틱·슬롯/ANS 경로를 로컬 UE 5.8 소스와 현재 시험 폰 설정에 대조했다. 이번 단위는 연결 후보와 검증 순서를 확정하는 조사이며 C++/에셋을 변경하거나 PIE를 시작하지 않았다. **기본 Montage Trait를 현재 그래프에 추가하는 것만으로는 기존 계약을 유지할 수 없다.** 다음 후보의 실행 가능성·몽타주/ANS 통과는 아직 미판정이다.

### 확인한 기본 경로와 현재 구성의 충돌

엔진 경로는 `C:/Program Files/Epic Games/UE_5.8/Engine/` 기준이다.

| 확인 대상 | 소스/현재 설정 | 결론 |
|:---|:---|:---|
| 몽타주 소유 메시 | `Plugins/Experimental/UAF/UAFAnimGraph/Source/UAFAnimGraph/Private/Injection/UAFMontageComponent.cpp`의 `OnBindToInstance`: UAF Owner의 첫 `USkeletalMeshComponent` 선택, `bEnableAnimation=true`면 경고 후 중단 | 시스템의 포즈 출력 locator를 VisualMesh로 지정해도 이 대상은 바뀌지 않음. 현재 기본 연결을 그대로 사용할 수 없음 |
| 몽타주 클래스/설정 | 같은 모듈 `Internal/Injection/UAFMontageComponent.h`: 노출 설정은 틱 이전 이벤트와 Legacy Montage AnimInstance 클래스. 구현에서 메시 지정 입력을 사용하지 않음 | Legacy 클래스 지정만으로 일반 ABP 갱신과 몽타주 전용 틱의 충돌을 해결하지 못함 |
| 현재 시험 폰 | MCP CDO: AnimSourceMesh는 `ABP_SourceUAFSpike`·일반 평가 켬, VisualMesh는 일반 평가 끔, WeaponMesh는 AnimClass 없음·일반 평가 켬. BP 부모는 `/Script/LootNPop.LNPPlayerCharacter` | 숨김 ABP를 단순히 끄면 `NativeUpdateAnimation`이 공급하던 Chooser·궤적·구면/발 배치 입력도 멈춤 |
| 첫 메시의 의미 | `Actor.cpp::FindComponentByClass`는 OwnedComponents의 첫 일치 항목 반환. CDO의 메시 열거는 WeaponMesh→AnimSourceMesh→VisualMesh였음 | 첫 메시를 AnimSourceMesh라고 가정하지 않음. CDO 열거를 실제 PIE 순서의 증거로 확대하지 않고, 컴포넌트 순서 조작으로 해결하지 않음 |
| 전용 초기화/틱 | `AnimInstance.cpp::InitializeMontageOnly`는 skeleton/required bones 준비, `TickMontageOnly`는 몽타주 갱신·이벤트 처리. 일반 NativeInitialize/NativeUpdate 호출을 대신하지 않음. `SkeletalMeshComponent.cpp::TickAnimation`은 일반 평가 끔에서 반환 | 런타임 슬롯 그룹 등록·B0 공급을 별도로 유지해야 함. 일반 틱과 전용 틱을 함께 돌려 중복 전진시키지 않음 |
| ANS 메시 | `UAF/Source/UAF/Private/Module/AnimNextActorComponentReferenceComponent.cpp`도 Owner의 첫 해당 컴포넌트를 선택. `UAFNotifyDispatcherComponent.cpp`는 그 참조로 NotifyBegin/Tick/End 전달 | 포즈 출력·몽타주 소유·ANS 전달 대상은 각각 확인해야 함. 현재 포즈 locator만 맞춰서는 부족함 |
| 기존 게임플레이 대상 | `ALNPCharacterBase::GetAnimInstance`는 AnimSourceMesh. `ANS_LNPCancelMontageOnMovement`는 전달받은 MeshComp의 AnimInstance에서 Stop. MeleeHitWindow는 MeshComp를 스윙 상태 키로 사용하고 실제 칼날은 WeaponMesh에서 읽음 | 몽타주 소유와 ANS 메시를 같은 AnimSourceMesh로 유지하는 후보가 기존 계약 변경을 줄임. 숨김 ABP와 UAF 양쪽에서 ANS를 중복 발화시키면 안 됨 |

`FMontageTrait`는 루트 모듈의 `FUAFMontageComponent`를 사용하며 슬롯별 포즈/노티파이를 공급한다. `UAFLayering`의 `MontageLayerDataTrait`도 같은 컴포넌트를 사용하므로 레이어 노드로 바꾸는 것만으로 위 문제를 우회하지 못한다.

### 다음 최소 구현 후보 — 시험 폰 안에서 명시적 대상·공급 분리

1. **원본 메시를 명시적으로 선택하는 시험 전용 C++ 폰.** `ALNPPlayerCharacter` 파생으로 두고 `FindComponentByClass(TSubclassOf<UActorComponent>)`의 정확한 SkeletalMeshComponent 질의에만 AnimSourceMesh를 반환하며 나머지는 Super에 위임하는 후보다. 해당 함수가 엔진의 virtual임을 확인했다. BP_PlayerUAFSpike의 현재 부모도 네이티브 LNPPlayerCharacter이므로 시험 사본만 새 파생 클래스로 변경할 수 있는 구성이다. 기본 플레이어/적 클래스·엔진은 수정하지 않는다. 이 선택이 UAF 몽타주·노티파이 참조와 GAS ActorInfo에 실제로 일관되게 반영되는지는 런타임 검증 대상이다.
2. **원본 메시를 몽타주 전용으로 전환하고 입력 공급을 명시적으로 구동.** 동일한 스파이크 AnimInstance를 AnimSourceMesh에 유지하되 일반 포즈 평가를 끄고, 시험 전용 게임 스레드 구동에서 기존 B0/공유 입력 공급을 프레임당 한 번 수행하는 후보다. 몽타주 재생 시간은 엔진 전용 틱만 진행시킨다. 초기 인스턴스 생성·Native 초기화/슬롯 그룹·UAF Graph 참조 공급의 순환 대기와 틱 선후 관계를 먼저 해결한다. 단순히 `bEnableAnimation`만 끄거나 `NativeUpdateAnimation`만 호출하면 완료라는 뜻은 아니다.
3. **공급 분리 자체의 회귀를 먼저 검증.** 숨김 그래프 평가가 없어지면 기존 `OffsetRootTransform`을 읽는 FutureFacing/Aim 기준도 사라진다. UAF의 완료된 루트 상태를 안전하게 공급하는 경로 또는 별도 동등 대책을 포함해야 하며 기본 컴포넌트 facing으로 조용히 대체하지 않는다. 이전 입력/구면/공중 계약을 유지하는 것이 이 다음 단위의 성공 기준이다. 워커 상태를 게임 스레드에서 동기화 없이 직접 읽지 않는다.
4. 공급 분리 검증 뒤 **DefaultSlot 하나의 UAF Montage Trait**부터 연결한다. 기존 순서에 맞춰 Strafe 뒤·Offset Root/발 배치 앞을 후보로 삼고 먼저 in-place 몽타주를 확인한다. 실제 배선·첫 표본은 그 구현 단위에서 선택한다. ABP 래퍼·원본 포즈 복사를 성공시켜 native UAF 몽타주 통과로 기록하지 않는다.

위 항목은 **구현 후보**다. 이번에 엔진 포크·내부 private 필드의 강제 변경·몽타주 상태 복사 브리지를 만들지 않았다. 타깃 선택/공급 분리가 동등하게 성립하지 않으면 원인을 기록하고 경로를 다시 판정한다. 조사만으로 No-Go나 Go를 선언하지 않는다.

### 몽타주·ANS의 실행 성공 기준과 단위 분리

| 후속 단위 | 최소 성공 기준 |
|:---|:---|
| 원본 메시 선택·입력 공급 분리 | 실제 UAF/GAS/ANS 대상이 AnimSourceMesh로 일치. 슬롯 그룹 초기화·B0/공유 입력 연속 갱신·프레임당 단일 공급·단일 몽타주 전진. 대표 북극/적도 이동·회전·공중/착지에서 이전 계약 회귀 없음 |
| DefaultSlot 최소 연결 | 실제 UAF 평가 task에서 몽타주 포즈 확인, 정상 재생/종료·즉시 Stop·블렌드아웃 Stop·다른 몽타주 교체 확인. 같은 슬롯을 중복 평가하지 않음 |
| 슬롯·ANS 계약 확대 | 기본 DefaultSlot/FullBody/UpperBody와 기존 그룹/가산 슬롯 순서 유지. ANS5종의 Begin/End 정확히 한 번·Tick 대상 일치·태그/근접 엔티티 잔류 없음. 정상 종료뿐 아니라 이동 취소·몽타주 교체·인스턴스/시스템 종료를 확인 |

- 실제 슬롯 그룹 표는 `LNPAnimInstance.cpp`의 런타임 등록이 소유한다. DefaultSlot·FullBody·FullBodyAdditivePreAim·UpperBody·UpperBodyAdditive는 DefaultGroup, AdditiveHitReact는 AdditiveGroup이다. §4의 핵심3슬롯만 확인하고 다른 기존 계약을 삭제하지 않는다. UpperBody의 본 마스크/AO/그립 IK·물리 히트리액트와 근접 루트모션·네트워크는 해당5-2 게이트에 남긴다.
- UAF dispatcher는 실행 종료 시 현재/이전 ANS 목록을 비교해 End를 큐에 넣는 코드를 가진다. 이것은 Stop/교체/시스템 파괴에서 End가 정확히 한 번 전달됐다는 실행 근거가 아니다. 같은 파일/헤더에서 별도 종료 훅을 찾지 못했으므로 파괴 경로는 특히 미판정으로 둔다. Native branching point와 queued notify도 실제 몽타주 설정을 구분해 확인한다.
- PIE/GUI 확인은 위 연결이 준비된 뒤 사용자 수동 플레이를 바로 요청한다. 아직 연결되지 않은 그래프에 시험을 요청하거나 새 자동화 도구를 만들지 않는다. 각 후속 구현 단위마다 종료 후 진행 확인을 받는다.

### 이번 단위 상태

MCP에서 PIE=false를 확인했고 조회만 수행했다. TestMap03은 §19의 BP_LNPGameMode 복원 상태를 유지하며 시험 GameMode를 재지정하지 않았다. 코드/플러그인/에셋 변경·컴파일·저장·새 녹화·빌드·커밋/푸시는 없다. 조회 설정과 근거 목록은 `Saved/AnimationOverhaul/2026-10-27_UAF52MontageInvestigation/Inspection.json`에 보관한다. **이번 조사 단위 완료·5-2 진행 중·몽타주/ANS 실행 미검증·Go/No-Go 보류**다. 다음 제안은 **시험용 원본 메시 선택·입력 공급 분리 최소 구현**이며 사용자 진행 확인 전 착수하지 않는다.

## 21. 시험용 원본 메시 선택·입력 공급 분리(2026-10-27~28, 기능 스모크 완료·호출 순서 오류 미해결)

사용자의 추가 진행 요청으로 §20의 다음 구현에 착수했다. 성공 기준은 시험 폰의 명시적 원본 선택·일반 포즈 평가 없는 단일 입력 공급·UAF 루트 기준 유지와 대표 구면 이동 회귀 확인이다. 몽타주 포즈/ANS 계약은 이후 native Montage Trait 연결에서 실행 판정한다.

### 소스 구현·빌드

- `Animation/UAFSpike/LNPUAFSpikePlayerCharacter.h/.cpp`: 시험 전용 LNPPlayerCharacter 파생 클래스. 정확한 SkeletalMeshComponent 질의에는 AnimSourceMesh를 반환하고 다른 질의는 Super에 위임한다. 원본의 일반 평가 끔/항상 틱 설정을 기본으로 두며 잘못된 BP 설정은 오류로 알린다. 게임 월드의 PostInitializeComponents에서 스파이크 인스턴스를 준비하고 폰의 PrePhysics 틱으로 입력을 구동한다. 기본 캐릭터/플레이어/적 클래스는 수정하지 않았다.
- `LNPUAFSpikeAnimInstance`: 몽타주 전용 초기화와 프로젝트 Native 초기화를 명시적으로 수행해 슬롯 그룹·캐시를 유지한다. 일반 NativeUpdate 경로와 명시적 드라이버 경로를 분리하고 후자는 GFrameCounter로 프레임당 한 번만 갱신한다. UAF PrePhysics는 폰의 공급 틱 뒤에 의존성을 건다. 기존 ABP 사본의 Chooser/System/Graph 기본값은 그대로 사용한다.
- `LNPUAFRootFeedback.h`와 기존 구면 trait/task: UAF 루트 보정 뒤 누적 월드 루트·입력 serial·유효 여부를 모듈 컴포넌트에 기록한다. 게임 스레드는 UAF의 ReadComponent API와 컴포넌트 잠금을 통해 값 사본만 읽어 기존 `OffsetRootTransform/bHasOffsetRoot`에 반영한 뒤 B0를 갱신한다. 무효/비활성 루트도 통지해 이전 값을 무조건 유지하지 않는다. 워커 trait 상태 포인터를 게임 스레드로 넘기지 않는다. 이 루트는 CR 앞 값이다.
- UAF 모듈 인스턴스에서 피드백 컴포넌트를 만드는 데 `UAF/Internal` 헤더 경로가 추가됐다. 기존 UAFWarping/UAFPoseSearch 내부 의존성과 함께 엔진 갱신 시 재검토 대상이다. 엔진 소스·공유 입력 구조체 레이아웃·AG 배선은 수정하지 않았다.
- 종료 확인 전 첫 빌드 호출은 권한 확장 환경의 에디터 프로세스 검사에서 차단돼 Build.bat이 시작되지 않았다. 제한 환경 Get-Process 결과로 종료됐다고 안내했던 내용을 정정했고 사용자 종료 확인을 받은 뒤 실행했다.
- 첫 실제 전체 빌드는9 actions·66.19초, 루트 trait 파일의 `Module/AnimNextModuleInstance.h` C1083으로 실패했다. 헤더가 UAF/Internal에 있음을 확인해 include 경로를 추가했다. 같은 구성 재빌드는 **14 actions·98.10초·Succeeded·종료 코드0**이다. 새 네이티브 타입이 있어 Live Coding을 쓰지 않았다.
- 근거/백업: `Saved/AnimationOverhaul/2026-10-27_UAF52SourceDriver/`의 BeforeBuild·Build_FirstFailureSummary·Build_Succeeded.log·BuildSummary와 시험 BP/AG 백업. 기본 에셋/사용자 데이터는 저장하지 않았다.

### 검증 범위와 후속 경계

재실행 뒤 아래와 같이 시험 BP 설정·수치 회귀·남극 부근/적도 기능 스모크를 수행했다. 현재 그래프에는 native Montage Trait가 없다. 실제 GAS ActorInfo에 저장된 메시·UAF 몽타주 소유 메시·ANS dispatcher 대상과 단일 몽타주 전진은 연결 후 검증해야 한다. 이 단계의 virtual 메시 조회 결과만으로 해당 계약을 통과시키지 않는다. 기존 5-1의 북극 표본도 새 공급층의 북극 재시험 근거로 재사용하지 않는다.

### 재실행·시험 BP 설정·수치 회귀 확인

- 사용자가 재실행 후 AG_LocomotionSpike·S_LocomotionSpike 정상 열기를 확인했다. 새 MCP 읽기 호출로 PIE=false와 기존 부모를 확인했고 시험 BP의 재부모화 직전 디스크 사본을 `BP_PlayerUAFSpike_BeforeReparent.uasset`에 추가 보관했다.
- BP_PlayerUAFSpike의 부모를 `/Script/LootNPop.LNPUAFSpikePlayerCharacter`로 변경·재조회했다. 원본의 AnimClass는 기존 ABP_SourceUAFSpike, 일반 평가=false·VisibilityBasedAnimTickOption=AlwaysTickPoseAndRefreshBones였다. 새 부모 기본값이 반영돼 별도 속성 덮어쓰기는 하지 않았다. warnings_as_errors=true 컴파일 호출이 오류 없이 종료됐고 해당 BP만 저장 성공했다. 클래스 객체에 인스턴스 속성을 조회한 첫 호출의 Warning은 CDO 조회로 정정했으며 컴파일 오류로 해석하지 않는다.
- 숨김 ABP 사본의 SpikeSystem·SpikeGraph·LocomotionDatabaseChooser 참조가 기존 S/AG/Chooser인지 재조회했다. AG를 수정/저장하지 않았다.
- 기존 `LootNPop.Animation.UAFSpike.SphericalRootFinalization`은 **1/1 성공·0.017769903초·오류/경고0**이다. 이 결과는 구면 수학의 회귀이며 새 공급층의 런타임 동작을 판정한 것은 아니다.
- TestMap03 WorldSettings의 원래 BP_LNPGameMode를 확인한 뒤 시험용 BP_GameModeUAFSpike로 **메모리에서만** 변경·재조회했다. 맵은 저장하지 않았다. 원래 복원값은 `/Game/Gameplay/BP_LNPGameMode.BP_LNPGameMode_C`다.
- 사용자에게 1인 PIE·`LNP.Anim.DebugLocomotion 2`, 정지→걷기/달리기→큰 룩 회전→점프/착지의 첫 스모크를 요청했다. 공격/새 녹화 없이 관찰과 라이브 로그/속성을 확인했다. 아래에 2026-10-28 결과를 기록한다.
- 설정/수치 근거는 같은 로컬 폴더의 `BlueprintSetup.json`·`RootRegression.json`이다.

### 명시적 공급·루트 피드백과 구면 기능 스모크(2026-10-28)

- 사용자는 첫 이동/회전/점프·착지, AG/S 탭이 닫혀 있는 상태의 반복 점프, 적도 이동/정지/큰 룩 회전/점프·착지 모두 **이상 없음**으로 보고했다. 후속 설명에 따르면 탭 닫기 요청 당시 이미 닫혀 있어 별도 조작은 없었다. 새 녹화·공격·소스 변경·빌드는 없었다.
- 런타임 AnimSourceMesh의 일반 평가=false·기존 ABP_SourceUAFSpike, VisualMesh의 일반 평가=false를 읽었다. 사용자 읽기 전용 Python의 `get_component_by_class(SkeletalMeshComponent)`도 실제 AnimSourceMesh를 반환했다. 소스 인스턴스의 `bExplicitDriver=true`, `bHasOffsetRoot=true`와 갱신되는 입력/피드백을 확인했다. 적도 최신 조회는 input41537/feedback41536·OnGround다. 단일 공급 상한은 코드의 GFrameCounter 가드, 갱신 연속성은 스냅샷/로그가 근거다.
- 두 Python 표본의 입력은14166→26285, 피드백은14165→26284였다. 에디터 프레임 증가12129와 입력 증가12119의 차이10은 원인이 확정되지 않았다. 에디터 프레임에는 비게임/일시정지 프레임도 포함될 수 있어 정확한 매 게임 프레임 공급률이나 전수 추적으로 확대하지 않는다. 두 표본에서 피드백은 입력보다1 작았다.

| 표본 | 진단 분모·동작 | CR 앞 구면 루트 결과 |
|:---|:---|:---|
| 첫 스모크 | 창139개/7,653프레임, 공중646·이동1,970, 점프/착지15쌍. 창 끝 위도−87.21~−79.83° | 상태/포즈 거리≤30cm·Yaw≤25.92°·Tilt0° |
| 두 Python 표본 사이 반복 점프 | 창220개/12,124프레임·점프/착지15쌍. 창 끝 위도−84.71~−81.56°. 요청 전 대기 포함 | 거리≤30cm·Yaw≤90°·Tilt0° |
| 적도 도착 이후 | 창 끝 위도 절댓값≤5°로 선택한171창/8,615프레임, 지상8,096·이동2,130, 위도−3.53~1.49°. 적도 도착 이후 점프/착지13쌍 | 거리≤30cm·Yaw≤90°·Tilt0° |

창 합계는 정확한 전체 PIE 프레임 수나 사용자 조작20초의 분모가 아니다. 적도 선택은 창 끝 위도 기준이며 창 안의 모든 프레임 위치를 보장하지 않는다. 루트는 CR 앞 진단이고 CR 뒤 본별 포즈 동등성·최종 품질 통과를 뜻하지 않는다. 첫 표본은 남극 부근이며 북극 표본으로 표기하지 않는다.

### 발견한 호출 순서 오류와 이번 판정

- 첫 스모크의 UTC01:49:04.662에 `FMotionMatchingTrait::PostEvaluate, PostEvaluate called without calling PreUpdate on this frame!`가 **1회** 있었다. 직전 지상→공중 전환은01:49:04.431이다. 오류에 Actor/인스턴스 식별자가 없어 PIE와 열린 그래프 미리보기 중 어느 쪽인지 확정하지 못했다. 사용자는 시각적 이상을 보고하지 않았다.
- 두 Python 표본 사이 점프/착지15쌍과 이후 적도 표본에서는 추가 PoseSearch 오류0이다. 사용자는 닫기 요청 당시 AG/S 탭이 이미 닫혀 있었고 추가 조작을 하지 않았다고 정정했다. **탭 닫기 전/후 비교가 아니며, 미리보기 원인·탭 닫기 효과·수정 완료로 판정하지 않는다.** 최초 오류 시각의 탭 상태를 별도로 특정한 기록도 없다. 엔진 MotionMatchingTrait의 검사값은 PreUpdate/PostEvaluate 사이 bool 상태이며 원인을 특정한 프레임 추적 근거는 아직 없다. 기존 Bool/Fall_Loop AG 배선은 수정 전 백업과 SHA256이 같다.
- 첫 스모크·후속 표본의 LogAnimation Error·드라이버 오류·assertion/ensure/fatal은0이다. 기존 Foley 이벤트 미처리 경고는 남으며 첫 스모크91회·두 번째 Python 이후 이동/대기 포함 구간342회다. 오류를 억제하지 않았다.
- **공급 분리 구현과 사용자 기능 스모크는 완료했으나, 이번 단위 전체의 오류 해소 판정은 보류**다. 5-2·최종 일곱 게이트·Go/No-Go는 계속 미판정이다. 다음 제안은 native Montage 연결 전에 **Motion Matching 호출 순서 오류의 원인 범위를 좁히는 조사 단위**다. 사용자 진행 확인 전 조사/구현을 확장하지 않는다.

근거: `Saved/AnimationOverhaul/2026-10-28_UAF52DriverSmoke/`의 FirstSmoke·AfterPreviewClosedJump·AfterEquator 로그/요약, RootWindows·RuntimeInspection JSON. PIE=true·DebugLocomotion2·AG/S 미리보기 탭 닫힘 상태를 유지했다. TestMap03의 에디터 메모리는 BP_GameModeUAFSpike이며 원래 복원값은 BP_LNPGameMode다. 이번 로그 확인에서는 맵을 저장/원복하지 않았다. 현재 디스크 맵 해시는 §19/빌드 전 기준과 다르므로 동일 보존이라고 주장하지 않는다. 변경 경위는 미확정이며 현재 사본을 TestMap03_CurrentDisk.umap에 보관했다. 다른 사용자 데이터3개와 AG 해시는 기존 기준과 같다. `PreservedAssets.json`·`PreservationNotes.json`이 근거다. PIE 중 map dirty 조회는 도구의 Asset does not exist 오류로 확인하지 못했으며 맵 부재로 해석하지 않는다.

## 22. Motion Matching 호출 순서 오류의 원인 범위 조사(2026-10-28, 조사 완료·원인 미확정)

사용자가 조사를 승인하고 AG/S 탭 닫기 요청 당시 이미 닫혀 있어 아무 조작도 하지 않았다고 설명했다. §21의 반복 점프는 탭 닫기 전/후 실험이 아니므로 관련 해석을 정정했다. 이번 단위는 소스 대조와 좁힌 수동 재현 시험이며 코드/에셋을 변경하지 않았다.

### 확인된 경로와 유력 후보

경로는 설치 UE 5.8 `Engine/` 기준이다.

| 확인한 사실 | 소스/현재 설정 | 의미 |
|:---|:---|:---|
| 지상 MM 갱신 조건 | `UAFAnimGraph/Private/Traits/BlendByBool.cpp:70`은 TrueChild 유효·AlwaysUpdate 또는 `FAnimWeight::IsRelevant`일 때만 갱신 큐에 추가 | 현재 AlwaysUpdateTrueChild=false이므로 지상 가중치≤0.00001이면 MM 갱신 생략 가능 |
| 자식 제거 조건 | `BlendSmoother.cpp:164`는 가중치≤0일 때만 blend 종료·자식 제거 | **0보다 크고0.00001 이하**인 자식은 유효한 채 남을 수 있음 |
| 포즈 평가 방문 조건 | `BlendByBool.cpp:97`은 양쪽 자식 핸들을 반환. `TraitInterfaces/IEvaluate.cpp:181~187`은 유효 핸들을 가중치 검사 없이 방문 | 위 구간에서 PreUpdate를 건너뛴 MM에 PostEvaluate가 도달할 수 있는 코드 경로가 존재 |
| 오류 검사 방식 | `UAFPoseSearch/Private/MotionMatchingTrait.cpp`의 PreUpdate는 bool을 false, PostEvaluate는 true로 변경하며 이미 true이면 이번 오류를 출력. 초기값도 true | 검사값은 프레임 번호가 아님. 갱신 누락·첫 평가 전 갱신 누락·중복 평가를 로그 한 줄만으로 구분할 수 없음 |
| 현재 공중 분기 | MM→Bool True, Fall_Loop→False, 조건=bOnGround. 양쪽 블렌드0.25초·HermiteCubic. MM 출력 링크는1개 | 최초 오류가 이륙 후0.231초에 발생한 사실과 블렌드 종료 부근 가설이 양립함. 실제 오류 프레임의 가중치를 증명하지는 않음 |

전체 엔진 경로 접두사는 `Plugins/Experimental/UAF/UAFAnimGraph/Source/` 및 `UAFPoseSearch/Source/`다. 임계값은 `Source/Runtime/Core/Public/Math/VectorRegister.h:133`, 관련성 검사는 `Source/Runtime/Engine/Public/Animation/AnimTypes.h:666`이다. `FAlphaBlend`의 HermiteCubic은 SmoothStep을 사용하며0에 도달하기 전 임계값 아래의 양수 구간이 존재한다. 해당 구간을 실제 최초 오류가 통과했는지는 미측정이다.

AG SHA256은 수정 전 백업과 같다. 설정은 동일 해시의 기존 Export를 재사용했다. ObjectTools의 RigVMPin.DefaultValue 직접 읽기는 지원되지 않아 실패했고 반복하지 않았다. 시험 폰의 명시적 공급 코드는 EvaluateGraph/UpdateGraph를 직접 호출하지 않는다. 기존 구면/Strafe trait도 각 경로에서 상위 PostEvaluate를 한 번 전달한다. 이것만으로 모든 중복 평가·다른 인스턴스·스케줄 문제를 배제하지는 않는다.

### 좁힌 재현 시험

- 시작 시 MCP에서 PIE=false를 확인했다. 사용자에게 새1인 PIE·DebugLocomotion2·AG/S 탭 닫힘 상태에서 약45초 방향을 바꾸며 착지 직후 재점프를 반복하도록 요청했다. 최초 오류 직전 착지→이륙 간격0.295초를 겨냥한 시험이다. 사용자는 방향을 바꾸며 다수 점프를 완료했다고 답했다. 이 답변은 조작 완료 보고이며 시각 이상 없음이라는 명시 답변으로 확대하지 않는다.
- 새 드라이버 초기화 UTC02:09:35.173부터 캡처 끝02:11:02.006까지 **점프/착지59쌍**, PoseSearch 오류0·LogAnimation Error0·드라이버 오류0·assertion/ensure/fatal0이다. 구간에는 수동 조작 전후 대기도 포함된다. 새 녹화·공격·빌드 없이 PIE=true를 유지했다. 재현되지 않았으므로 최초 오류 해결 판정은 하지 않는다.
- 근거는 `Saved/AnimationOverhaul/2026-10-28_UAF52OrderInvestigation/`의 SourceAnalysis.json·MotionMatchingOutgoingLinks.txt·RapidJumpBaseline.log/요약이다. 과거 AfterPreviewClosedJump 파일명은 그대로 두되 탭 조작이 없었다는 정정을 추가했다.

### 판정·다음 한 단위

**조사 단위 완료, 최초 오류의 원인 확정/수정은 미완료**다. 유력 후보는 Bool 공중 블렌드 끝의 갱신/평가 임계값 차이이며 엔진 소스에서 그 경로를 확인했다. 실제 발생 인스턴스·가중치·입력 serial이 없어 이번 오류의 직접 원인으로 단정하지 않는다. 공급층 변경 때문이라는 판정이나 UAF No-Go도 내리지 않는다.

다음 제안은 **기존 스파이크 trait에 조건부 진단 로그만 추가하는 구현 단위**다. 바깥 루트의 PreEvaluate에서 유효 MM 자식의 검사 bool이 이미 true인 경우에만 모듈/Owner 경로·입력 serial·Bool 목적 자식·MM 가중치를 남겨 다음 오류와 대응한다. 초기/반복 평가 가능성도 구분할 수 있게 기록한다. 기존 타입을 재사용하고 엔진/에셋·공중 블렌드 정책·AlwaysUpdate 설정을 바꾸지 않는다. 레이아웃 변경이 필요 없는 범위로 제한하며 이전 Live Coding 크래시 이력이 있어 빌드 방법은 구현 시 명확히 정한다. 사용자 진행 확인 전 구현하지 않는다. 진단 후에도 불규칙 재현이면 같은 수동 시험을 무작정 반복하지 않고 얻은 기록에 따라 다음 경로를 선택한다.

## 23. 호출 순서 오류 조건부 진단 구현(2026-10-28, 구현/검증 완료·동일 오류 재현)

사용자 진행 승인으로 기존 `LNPSphericalOffsetRootTrait.cpp`의 PreEvaluate에 진단을 추가했다. 성공 기준은 전체 빌드와 대표 수동 스모크에서 진단 경로를 확인하고, 재발하면 엔진 오류와 같은 인스턴스/serial/가중치를 대응하는 것이다. 오류가 재발하지 않으면 원인 확정·수정 완료로 처리하지 않는다.

- `WITH_EDITOR`·기존 `LNP.Anim.DebugLocomotion>=2`에서만 Bool 자식을 읽는다. 유효한 MM 자식의 `bIsPostEvaluateBeingCalled`가 PreEvaluate 진입에 이미 true일 때만 `[UAFMMOrder]` Warning을 남긴다. MM 검사값·블렌드/검색 상태는 변경하지 않는다.
- 출력은 host의 전체 UObject 경로·module/MM/root 인스턴스 주소·현재 입력 serial·마지막 루트 적용 serial·지상 여부·목적/현재 자식 index·실제 가중치·엔진 관련성 판정·선택 DB/애니다. MM 자식의 PostEvaluate보다 먼저 기록해 엔진 오류와 대응할 수 있게 한다. 경로는 PIE/미리보기 구분, 포인터는 같은 실행의 인스턴스 대응, serial은 반복 입력 평가/공급 정체 가설을 판단하는 근거다. 이전 적용 serial만으로 엔진 프레임이나 갱신 횟수를 직접 측정한 것은 아니며 DB 없음도 초기화의 확정 증거는 아니다.
- 새 CVar·타입·구조체 필드/레이아웃·에셋·엔진 변경은 없다. 기본 상위 PreEvaluate 전달과 기존 루트 task 순서는 유지한다. `Animation/AnimTypes.h`를 명시적으로 포함해 실제 FAnimWeight 판정을 출력한다. 정상 상태에서는 로그를 출력하지 않는다.
- 변경 직전 cpp 사본과 사용자 에셋/시험 BP/AG 해시를 `Saved/AnimationOverhaul/2026-10-28_UAF52OrderDiagnostic/`에 보관했다. 변경 범위 검토는 include1줄과 진단 블록36줄이며 헤더는 이번 단위에서 변경하지 않았다. 진단 구현을 그대로 복제한 테스트를 추가하지 않는다. 필요한 검증은 실제 편집기 빌드와 런타임 상관관계다.
- 이전 Live Coding 크래시 이력에 따라 이번 cpp 변경도 에디터 종료 후 `LootNPopEditor Win64 Development` 전체 빌드로 확인했다. 사용자의 종료 회신 뒤 권한 확장 환경의 프로세스 검사에서도 에디터 부재를 확인하고 기준 명령을 실행했다. **4 actions·20.50초·Succeeded·종료 코드0**이다. 진단 cpp 컴파일과 lib/dll 링크를 확인했다. Live Coding은 사용하지 않았다. 근거는 같은 폴더의 Build.log·BuildSummary.json이다.
- 사용자에게 에디터 재실행·TestMap03 준비 확인을 요청했다. AG/S 탭과 PIE는 아직 열지 않도록 안내했다. 시험 GameMode 확인 후 DebugLocomotion2·빠른 재점프 표본을 수동 요청한다. **컴파일만 완료, 런타임 진단 경로는 미검증이며 이번 구현 단위는 미완료**다. 기존 구면 수학/포즈를 바꾸지 않은 진단 추가이므로 과거 수치 회귀를 반복하지 않는다. 실패/재발 기록에 따라 필요한 후속만 진행한다.

### 재실행 설정 확인·수동 시험 요청

- 사용자 재실행 확인 뒤 MCP 읽기 호출에서 PIE=false·GetOpenAssets=[]를 확인했다. TestMap03은 이미 BP_GameModeUAFSpike였다. 이번 재연결에서 설정을 변경/저장하지 않았다. 재시작 후에도 시험 값이라는 사실과 §21의 디스크 해시 차이를 유지하며, 이전의 메모리만 임시 지정했다는 기록을 현재 디스크 복원 상태로 확대하지 않는다. 원래 복원값은 사용자 확인한 BP_LNPGameMode다.
- DebugLocomotion=0을 확인해 사용자에게 **PIE 시작 전에 2로 설정**한 뒤 새1인 PIE·약45초 방향 전환/착지 직후 재점프·이상 여부 보고·PIE 유지를 요청했다. 열린 AG/S 탭·공격·새 녹화는 필요 없다. 초기 MM 자식의 첫 평가 구간도 기록하기 위해 디버그를 먼저 켠다.
- 근거는 OrderDiagnostic/RestartSetup.json이다. 현재 사용자 수동 결과 대기·진단 이벤트 실행 미검증이며 최초 오류 해소/단위 완료를 선언하지 않는다.

### 진단 발화·엔진 오류 대응 확인과 이번 단위 종료

- 사용자는 이번 방향 전환/빠른 재점프 시험도 **이상 없음**으로 보고했다. 드라이버 초기화 UTC02:20:10.701부터 캡처 끝02:23:09.429까지 로그상 **점프/착지133쌍**이다. 이 구간에는 조작 전후 대기가 포함되며 요청한45초의 정확한 분모로 해석하지 않는다.
- **[UAFMMOrder]1회와 기존 PoseSearch 호출 순서 오류1회**가 UTC02:20:42.056·동일 로그 프레임 접두사[967]에서 연속으로 출력됐다. 진단 host는 실제 PIE의 `BP_PlayerUAFSpike_C_0.UAFLocomotion`이다. 이번 재현을 AG/S 미리보기 오류로 해석하지 않는다.
- 당시 input1691·previousApplied1690, ground0·목적 자식1(Fall_Loop)·현재 자식0(MM), **가중치0.00000220537186·relevant0**이었다. 유효 MM의 검사 bool이 이미 true인 상태를 자식 평가 전에 기록했고 바로 뒤 엔진 PostEvaluate 오류가 발화했다. 직전 착지02:20:41.720→이륙41.889(지상0.169초), 오류는 이륙0.167초 뒤다. 기본0.25초 전환이 중간에 반전될 수 있으므로 고정0.25초 뒤에만 나타나는 문제로 한정하지 않는다.
- §22의 엔진 코드와 이번 값을 대응하면 **양수이지만 관련성 임계값0.00001 이하인 지상 MM 자식이 제거되지 않은 채, 갱신 큐에서는 제외되고 평가에는 방문하는 경로**가 이번 재현에서 확인된다. 입력 번호/직전 루트 적용 번호만으로 모든 중복 평가 가능성을 전수 배제한 것은 아니지만 이 조건이 같은 오류를 발생시키는 근거를 확보했다. 최초 무진단 이벤트 자체의 인스턴스 식별은 여전히 없으며, 이번에 동일 오류가 실제 PIE에서 해당 경로로 재현됐다는 판정이다.
- LogAnimation Error·드라이버 오류·assertion/ensure/fatal은0이다. Root177창/9,607진단 프레임(지상4,081·이동7,602), 창 끝 위도−89.48~3.27°, CR 앞 상태/포즈 거리≤30cm·Yaw≤90°·Tilt0°다. 이는 진단 추가 뒤 기능 스모크 근거이며 CR 뒤 포즈 동등성이나 몽타주/ANS 통과를 뜻하지 않는다.
- 진단 추가 전 기준으로 사용자 에셋4개·시험 BP·AG의 디스크 해시는 모두 그대로다. 새 녹화·추가 소스/에셋 변경·저장·커밋/푸시는 없다. PIE=true·DebugLocomotion2를 유지했다. 원시 근거는 OrderDiagnostic/DiagnosticSmoke.log·DiagnosticSmokeSummary.json·RootWindows·MatchedIncident.log·AfterSmokeHashes.json이다.

**조건부 진단 구현·전체 빌드·런타임 발화 검증 단위는 완료**다. 오류를 수정하거나 억제하지 않았고5-2/Go-NoGo는 보류다. 다음 제안은 **평가에 남아 있는 자식이 갱신에서만 빠지지 않도록 프로젝트 안에서 최소 보정하는 구현/재검증 단위**다. 기존 자식 수명·공중 Fall_Loop과0.25초 블렌드 정책을 유지하며 엔진 포크나 MM 전체 공중 상시 갱신을 기본 해법으로 택하지 않는다. 구체 구현은 해당 단위에서 확인하고 사용자 진행 승인 전 수정하지 않는다. 다음 수동 시험에서는 같은 빠른 재점프로 진단/엔진 오류와 기능 회귀를 함께 확인한다.

## 24. Bool 블렌드의 유효 자식 갱신 누락 보정(2026-10-28, 구현/검증 완료)

사용자 승인으로 §23에서 재현한 경계 조건의 보정을 구현한다. 성공 기준은 전체 빌드/AG 연결과, 같은 빠른 재점프에서 **보충 갱신의 실제 실행·기존 순서 오류0·사용자 기능 회귀 없음**을 함께 확인하는 것이다. 보정 분기가 실행되지 않은 무오류 표본만으로 수정 검증을 완료하지 않는다.

- 신규 `LNPUAFBoolBlendUpdateTrait.h/.cpp`의 빈 공유 타입과 additive 트레이트는 Bool 스택에 추가할 `LNP Bool Blend Update`다. 자식 생성/제거·블렌드 가중치/시간·조건을 바꾸지 않으며 별도 인스턴스 상태/CVar는 없다.
- 설치 엔진 `IUpdate.cpp:234`와 실제 순회는 IUpdateTraversal을 각 트레이트에 따로 호출하고 상위로 전달하지 않는다고 명시한다. 보정은 기본 Bool을 대체/재호출하지 않고 **기본 큐에서 제외된 유효 자식만** 추가한다. True 가중치·False의1-True·관련성/AlwaysUpdate 조건과 PreviousChildIndex/bWasRelevant를 그대로 읽어 중복 등록을 피하고 BlendingOut/NewlyRelevant를 유지한다. 자식이 제거되면 보정에서도 갱신하지 않는다. 가중치0에서도 DeltaTime0 등으로 유효 핸들이 남는다면 같은 원칙을 적용한다.
- `UAFAnimGraph/Internal` include 경로를 Build.cs에 추가했다. Bool 공유/인스턴스 타입에 대한 UE5.8 내부 의존성이므로 엔진 갱신 시 재검토한다. 기존 DebugLocomotion2에서 실제 보충 때만 `[UAFBoolUpdate]`로 host·자식/목적·가중치·NewlyRelevant를 기록한다. §23 진단/엔진 오류는 유지한다. 엔진·기본 캐릭터/ABP·공유 입력·공중 상시 MM 유지 정책은 변경하지 않는다.
- 시작 시 PIE=false를 확인했다. AG/Build.cs 사본과 사용자 에셋4개·시험 BP/AG의 해시는 `Saved/AnimationOverhaul/2026-10-28_UAF52BoolUpdateFix/`에 보관했다. 소스 준비 뒤 해당 에셋 해시는 모두 그대로이며 AG는 아직 수정하지 않았다.
- UAF 트레이트 추가용 MCP 도구가 없고 ControlRig 도구는 ControlRigBlueprint 전용이다. 빌드 후 **기존 Blend By Bool 노드 선택→Details Add Trait→LNP Bool Blend Update 추가→Compile/Save**를 수동 요청한다. 기존 스택/연결은 유지한다. GUI 한 번이 더 효율적이므로 도구/스크립트를 만들지 않는다.
- 새 타입 때문에 에디터 종료 후 기준 Development 전체 빌드를 수행한다. 현재 작업 저장·에디터 종료 회신을 요청한 상태이며 **종료 확인/빌드 대기·이번 단위 미완료**다. 종료 확인 전 빌드하지 않는다. 재실행 후 수동 연결·설정 확인·빠른 재점프까지 같은 단위에서 이어간다.

### 전체 빌드 완료·AG 추가 요청

- 사용자의 종료 회신 직후 프로세스 검사에서 LootNPop 에디터가 아직 남아 첫 호출은 Build.bat 시작 전에 차단됐다. 실제 프로젝트 명령행을 확인한 뒤 잠시 기다렸고 해당 프로세스가 정상 종료된 것을 확인했다. 강제 종료는 하지 않았다. 종료 전 빌드 시도나 컴파일 실패로 해석하지 않는다.
- 실제 전체 빌드 직전 LootNPop 프로세스 부재를 다시 확인했다. 새 파일/UHT 처리와 보정 cpp 컴파일·lib/dll 링크를 포함해 **15 actions·58.89초·Succeeded·종료 코드0**이다. Live Coding은 사용하지 않았다. BoolUpdateFix/ShutdownGuard.json·Build.log·BuildSummary.json이 근거다.
- 사용자에게 에디터 재실행 후 기존 Bool 노드의 Details/Add Trait에서 **LNP Bool Blend Update를 한 번 추가→Compile/Save→AG 탭 닫기**를 요청했다. 기존 트레이트/연결은 유지하고 PIE는 아직 시작하지 않도록 안내했다. 현재 수동 완료 회신 대기이며 AG 연결·보충 갱신 실행·회귀 검증은 남아 있다. **이번 단위 미완료·5-2/Go-NoGo 보류**다.

### AG 수동 추가 보고·실행 시험 요청

- 사용자가 추가 완료를 보고했다. 재연결 읽기에서 PIE=false·GetOpenAssets=[]·시험 GameMode·DebugLocomotion0을 확인했다. AG 디스크 SHA256은 `5EFF88E13FEA53D4639517802ACB04D3E8EADA7A166789301CFFD793FCD9EBDC`로 변경됐고 저장 후 사본을 보관했다. 사용자 에셋4개·시험 BP는 보정 전 해시와 같다.
- ObjectTools의 TraitRootPinNames 직접 읽기는 지원되지 않아 실패했다. 반복/추가 도구를 만들지 않고 사용자 추가/Compile/Save 보고와 실제 `[UAFBoolUpdate]` 발화를 함께 연결 근거로 삼는다. 아직 발화 전이므로 트레이트 연결을 독립 검증 완료로 표기하지 않는다.
- 사용자에게 PIE 시작 전 DebugLocomotion2를 켜고 새1인 PIE·약60초 방향 전환/빠른 재점프·중간 정지/큰 룩 회전·이상 여부 보고·PIE 유지를 요청했다. 새 녹화/공격은 필요 없다. 현재 수동 결과 대기·이번 단위 미완료다. 설정/해시 근거는 BoolUpdateFix의 SetupReads.json·AfterAGSetupHashes.json이다.

### 보충 갱신의 실제 실행·회귀 검증과 종료 판정

- 사용자는 방향 전환/빠른 재점프/정지/큰 룩 회전에서 **이상 없음**으로 보고했다. PIE 시작 전 Debug2 입력을 로그로 확인했다. 드라이버 초기화 UTC02:44:51.677부터 캡처 끝02:46:23.704까지 **점프/착지67쌍**이다. 조작 전후 대기까지 포함한 캡처이며 요청60초의 정확한 분모는 아니다.
- 실제 PIE `BP_PlayerUAFSpike_C_0.UAFLocomotion`에서 **[UAFBoolUpdate]2회**를 확인했다. 두 표본 모두 child0(MM)·destination1(Fall_Loop)·NewlyRelevant0이며 가중치는 각각 **0.00000011920929·0.000000655651093**이다. 둘 다 기본 관련성 임계값0.00001 이하인 양수라 기존 Bool에서 빠지는 자식을 보정이 갱신한 표본이다. 사용자 추가 보고뿐 아니라 실제 실행으로 AG 연결을 확인했다.
- 이 캡처의 **[UAFMMOrder] 경고0·PoseSearch Error0**이다. PreUpdate/PostEvaluate 순서 오류를 억제하지 않았고 보정이 실제 발화한 상태에서 재현되지 않았다. LogAnimation Error·드라이버 오류·assertion/ensure/fatal도0이다. §23은 같은 임계값 아래 양수 MM 자식에서 진단/오류1회를 재현했으므로 이번 결과는 보정 분기가 한 번도 실행되지 않은 무오류 표본과 구분된다.
- Root91창/4,925진단 프레임(지상2,168·이동3,665), 창 끝 위도−89.49~−36.77°에서 CR 앞 상태/포즈 거리≤30cm·Yaw≤90°·Tilt0°를 유지했다. 창 합계는 정확한 전체 PIE 프레임 수가 아니며 CR 뒤 포즈/본 동등성·모든 위도 조합 검증으로 확대하지 않는다.
- 연결 후 기준으로 사용자 에셋4개·시험 BP·AG의 해시는 모두 그대로다. 보정 전 대비 AG만 사용자 추가/저장으로 변경됐고 저장 후 사본을 유지한다. 새 녹화·추가 코드/에셋 수정·맵 저장/원복·커밋/푸시는 없다. PIE=true·Debug2·AG/S 탭 닫힘을 유지했다. 근거는 BoolUpdateFix/FixSmoke.log·FixSmokeSummary.json·FixSmokeRootWindows.json·AfterSmokeHashes.json이다.

**이번 Bool 보정 구현/검증 단위는 관측한 스파이크 조건에서 완료**다. §21 공급 분리 기능 스모크에서 남았던 호출 순서 오류는 §23 재현/원인 경로 확인과 이번 실제 보정 실행/무오류 표본으로 후속 검증을 마쳤다. 모든 그래프·DeltaTime0·NewlyRelevant=true·AlwaysUpdate=true·False 자식 경계의 런타임 전수 검증을 주장하지 않는다. 해당 분기들의 중복 방지/상태 전달은 기본 조건의 여집합을 사용한 코드 대조가 근거다. 이전 무진단 이벤트의 인스턴스를 소급 특정하지도 않는다.

5-2 전체와 최종 Go/No-Go는 계속 미판정이다. 실제 몽타주/GAS/ANS 대상·단일 몽타주 전진은 연결 후 확인한다. 다음 제안은 §20의 **native DefaultSlot 최소 연결**이며 사용자 진행 확인 전 조사/구현을 확장하지 않는다. 기존 보정/진단은 유지해 후속 통합에서도 같은 오류가 다시 생기는지 관찰한다.

## 25. 새 세션 인계·단일 커밋 정리(2026-10-28)

사용자 요청으로 다음 몽타주 단위를 시작하지 않고 문서를 정리한다. Current는 이번에 유지할 구현·검증 요약·바로 다음 작업·승인 경계·에디터/맵 상태·로컬 근거 중심으로 갱신하고, Roadmap의 오래된 Phase5 상태와 누적 중간 상태를 최종 결과로 요약했다. README/Decisions와 프로젝트 공통 설계는 새 확정 결정이나 정식 마이그레이션이 없어 변경하지 않는다.

- 다음 시작점은 §20의 native DefaultSlot 하나 최소 연결이다. 연결 뒤 실제 몽타주/GAS/ANS 메시 선택·단일 몽타주 전진·정상 종료/Stop/교체를 확인한다. 이번 인계/커밋 승인을 그 구현 승인으로 해석하지 않는다. 사용자 새 진행 요청 후 착수한다.
- 인계 시 읽기 조회는 **PIE=false·GetOpenAssets=[]·DebugLocomotion2·BP_GameModeUAFSpike**다. 재시작/새 PIE 뒤 현재 참조를 새로 확인한다. 맵을 자동 복원/저장하지 않았으며 원래 GameMode는 사용자 확인한 BP_LNPGameMode다. TestMap03 전체를 원복하지 않는다.
- 이번 커밋 범위는 문서4개·시험 BP/AG2개·스파이크 소스/Build.cs10개로16파일이다. 기존 사용자 변경4개(TestMap03·플레이어 기본 스탯·롱소드 레벨·스폰 설정)는 제외/보존한다. 인덱스와 에셋 해시를 확인하고 최근 커밋의 feat 제목·한국어 항목별 본문 양식을 따른다. Saved 근거는 git 제외이므로 커밋하지 않는다.
- 기존 전체 빌드/수치 회귀/수동 스모크를 재사용하고 인계만을 위해 새 빌드·PIE·녹화를 반복하지 않는다. Build.cs의 이번 작업 중 발생한 BOM 제거만 기존 인코딩으로 복원해 불필요한 차이를 없앴으며 기능 변경은 없다. 다음 구현·새 채팅 생성·푸시는 수행하지 않는다.

## 26. native DefaultSlot 최소 연결·검증(2026-10-28, 단위 완료)

사용자 재개 요청으로 §20의 다음 단위에 착수했다. 성공 기준은 실제 UAF 몽타주 포즈·단일 전진·정상 종료·즉시/블렌드아웃 Stop·교체 확인이다. 슬롯/ANS 전체나 최종 게이트 통과를 의미하지 않는다.

- 시작 읽기 조회에서 PIE=false·열린 에셋 없음·TestMap03의 BP_GameModeUAFSpike·AG dirty=false를 확인했다. 현재 시험 GameMode 유지 여부를 사용자에게 확인 요청했다. 회신 전 PIE를 시작하거나 맵을 저장/복원하지 않았다.
- AG 해시는 §24의 `5EFF88E13FEA53D4639517802ACB04D3E8EADA7A166789301CFFD793FCD9EBDC`와 같다. 수정 전 AG 사본과 사용자 에셋4개·시험 BP/AG 해시는 `Saved/AnimationOverhaul/2026-10-28_UAF52DefaultSlot/`의 `AG_BeforeDefaultSlot.uasset`·`BeforeHashes.json`에 보관했다.
- 설치 엔진의 `Internal/Injection/MontageTrait.h`에서 native Montage Trait의 Source·SlotName·두 Bool 입력을 확인했다. 기존 Export의 Strafe Result → Offset Root Input 위치 사이에 노드 하나를 넣고 Slot Name=DefaultSlot, Always Update Source=false, Disable Synchronization=false로 Compile·해당 AG만 Save·탭 닫기를 요청했다. Bool 보정 등 기존 스택을 유지하고 PIE는 시작하지 않도록 안내했다. 전용 UAF 편집 MCP가 없어 새 도구를 만들지 않고 수동 연결을 요청했다.
- 첫 표본은 `/Game/Characters/Retargeted/Lyra/AM_SW_Damage_Fast_UEFN`이다. SlotAnimTracks에서 DefaultSlot·길이 약0.567초·시퀀스 `A_SW_Damage_Fast_UEFN`을 확인했고 시퀀스는 bEnableRootMotion=false·AAT_None이다. 약0.7초의 `AM_SW_Blocking_Hit_UEFN`도 같은 슬롯의 후속 교체 후보로 확인했다. Dash는 FullBody이므로 이 단위의 첫 표본으로 쓰지 않는다. Notifies 직접 읽기는 지원되지 않아 실패했으며 노티파이 부재로 해석하지 않는다.
- **현재 AG 수동 연결·GameMode 회신 대기, 이번 단위 미완료**다. C++/에셋 수정·PIE·빌드·새 녹화·맵 저장·커밋/푸시는 수행하지 않았다. 연결 후 실제 런타임 대상을 새로 조회하고 필요한 수동 재생 시험을 요청한다.

### 수동 연결 완료 보고·저장/컴파일 확인·첫 재생 요청

- 사용자는 AG 연결 완료와 시험 GameMode 유지를 회신했다. 새 읽기에서 PIE=false·열린 에셋 없음·AG dirty=false·BP_GameModeUAFSpike를 확인했다. 로그 UTC04:59:02의 컴파일 성공·04:59:03의 해당 AG 저장·04:59:13의 탭 닫힘을 확인했다.
- AG 새 SHA256은 `68CABF1278BF2519B8D33CF4E41BA07118CF99F6717FA29DC973D00B13D8B4BB`이며 `AG_AfterDefaultSlot.uasset`에 보관했다. `AfterAGSetupHashes.json`에서 시험 BP·사용자 에셋4개는 수정 전과 같다. 그래프 Nodes/Links 직접 조회는 지원되지 않아 실패했다. 반복하거나 새 편집 도구를 만들지 않았고, 사용자 배선 보고·컴파일 성공을 실제 Montage task 실행 확인으로 확대하지 않는다.
- GUI 작업 구간에는 Strafe EditCondition의 `f` 파싱 오류와 bInitialized/AlphaBlend 텍스트 가져오기 경고·FootPlacement editor data 경고도 있다. 컴파일 성공과 별도로 보존하며 런타임 무오류나 기존 설정의 완전 동일성을 주장하지 않는다. 원문은 `AGSetup.log`다.
- 다음 수동 시험은 새1인 PIE Net Mode=Play Standalone, DebugLocomotion2, Rewind 녹화 후 첫 표본을0.1배속(약5.67초)으로 재생하는 것이다. Output Log Python에서 새 game world/player/AnimSourceMesh/AnimInstance를 구해 경로와 Montage_Play 반환을 출력한다. 복귀한 로코모션·눈에 보이는 피격 자세/오류 여부를 보고하고 녹화 정지 뒤 PIE를 유지하도록 요청한다. 첫 재생 후 실제 trace/evaluation task와 전진/종료를 좁혀 확인하며 Stop/교체는 같은 단위의 후속 시험이다.
- 현재 실제 재생 전·이번 단위 미완료다. 추가 C++ 수정/빌드·새 녹화·맵 저장/복원·커밋/푸시는 없다. 시험 종료 시 원래 GameMode를 해당 속성만 복원하되 맵 전체를 원복하지 않는다.

### 첫 재생 결과·Montage 원본 갱신 재확인

- 사용자는 피격 자세가 슬로우모션처럼 재생된 뒤 Idle로 돌아왔다고 보고했다. 요청한0.1배속과 일치하는 관찰이며 실시간 재생률/단일 전진을 수치 검증한 것은 아니다. UTC05:08:25.765의 Python 출력에서 실제 `BP_PlayerUAFSpike_C_0.AnimSourceMesh.ABP_SourceUAFSpike_C_0`의 Montage_Play 반환0.5666666626930237을 확인했다.
- 같은 재생 구간 UTC05:08:26.010~05:08:31.177에서 **UAFMMOrder236회·FMotionMatchingTrait 오류236회·FHistoryCollectorTrait 오류236회**가 있었다. 진단 host는 실제 PIE UAFLocomotion이며 내부 Bool의 MM 자식은 ground1·weight1·relevant1이다. §24의 임계값 이하 Bool 자식 표본과는 조건이 다르며 기존 보정을 삭제/되돌리지 않는다. 이번 연결을 무오류 통과로 처리하지 않는다.
- 설치 엔진 `MontageTrait.cpp`의 QueueChildrenForTraversal은 AlwaysUpdateSource=false이고 원본 가중치가 관련성 임계값 이하면 원본 갱신을 제외한다. GetChildren은 유효 원본을 가중치 검사 없이 평가에 넘긴다. 새 Montage가 MM/Pose History를 포함한 상위 경로에 있으므로 이 생략 경로는 이번 지속 오류와 부합하는 원인 후보다. 실제 Montage SourceWeight는 측정하지 않아 특정 값까지 입증한 것으로 기록하지 않는다.
- 최소 후속 조치로 **PIE 종료 후 새 Montage Trait의 Always Update Source만 true로 변경 → Compile/해당 AG만 Save/탭 닫기**를 즉시 수동 요청했다. 같은 구현 단위의 재검증이며 새 C++/엔진 수정·도구 구현은 없다. 이 설정은 몽타주가 원본을 덮는 동안에도 로코모션을 갱신하는 엔진 옵션이다. 성능은 측정하지 않았다. 완료 회신 전 새 PIE/재생을 요청하지 않는다.
- 녹화 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261028_140819272.utrace` 약22MB가 생성됐다. 실제 EvaluationProgram task/몽타주 시간축은 아직 확인하지 않았다. 녹화 정지 부근 LogTrace error64와 trace 분석 경고도 있어 녹화 무결성을 아직 판정하지 않는다. 사용자 그래프 조작과 겹치지 않도록 UI 조회를 확장하지 않았다.
- 근거는 DefaultSlot 폴더의 `FirstPlay.log`·`FirstPlaySummary.json`이다. 읽기 시 PIE=true·Standalone 창을 확인했으며 이후 사용자 수동 변경을 기다린다. **이번 단위 미완료·Stop/교체/대상 전체 계약·최종 Go/No-Go 미판정**이다.

### Always Update Source 변경 완료 보고·재검증 요청

- 사용자는 true 변경 완료를 보고했다. 새 조회에서 PIE=false·열린 에셋 없음·AG dirty=false·시험 GameMode·DebugLocomotion2를 확인했다. UTC05:11:31.938의 컴파일 성공·05:11:32.947의 저장·05:11:39의 탭 닫힘 로그가 있다.
- AG SHA256은 `3EBCB7119262252281E5929D5207434A0ADA26ADC8EDD8A973CB45477FF3E77A`다. `AG_AfterAlwaysUpdateSource.uasset`·`AfterAlwaysUpdateSetupHashes.json`·`AlwaysUpdateSetup.log`를 보관했다. 시험 BP와 사용자 에셋4개는 변경 전 해시를 유지했다. GUI의 정확한 설정값은 사용자 보고를 근거로 하며 실제 오류 해소는 아직 확인하지 않았다.
- 같은1인 Standalone PIE/Rewind·같은 몽타주0.1배속 재생을 요청한다. 새 월드/폰/원본 인스턴스를 다시 구하고 Idle 복귀 뒤 Montage_IsPlaying/GetCurrentActiveMontage를 출력해 잔류 여부를 확인한다. 녹화를 끈 뒤 PIE는 유지한다. 실제 UAF task·단일 전진·Stop/교체 검증은 후속으로 남는다.
- 현재 사용자 재생 결과 대기·이번 단위 미완료다. C++/엔진 변경·빌드·맵 저장·커밋/푸시는 없다.

### true 설정 후 재생 재검증·정상 종료 확인·Rewind 시점 선택 대기

- 사용자는 재실행 완료를 보고했다. 새 드라이버 시작 UTC05:16:14.726 이후의 캡처에서 **MM 오류0·Pose History 오류0·UAFMMOrder0·LogAnimation Error0·드라이버 오류0**을 확인했다. 첫 false 설정 재생의 각236회와 구분한다. 실제 원본 인스턴스 경로의 Montage_Play 반환은 UTC05:16:35.428에0.5666666626930237이고, UTC05:16:45.502의 종료 조회는 **playing=False·active=None**이다. 이 사후 조회만으로 정확한 종료 프레임/콜백·단일 전진을 입증하지 않는다.
- 새 trace `RewindDebugger-LootNPop-Editor-20261028_141624426.utrace`는46,209,880바이트다. 정지 부근 LogTrace error64는 이번에도 있어 별도로 보존한다. Slate에서 새 runtime 창의 Standalone 표기와 Rewind의 원본 ABP Montages 트랙·UAFLocomotion/S_LocomotionSpike/UAFAnimGraph_0/AG_LocomotionSpike·EvaluationProgram 트랙을 확인했다. 데이터가 표시되는 사실은 확인했으나 실제 피격 task/시간축과 trace 전체 무결성은 아직 미판정이다.
- 일시정지/피격 시점 선택은 사용자 수동 GUI가 빠르므로 즉시 요청했다. 사용자가 PIE 일시정지·피격 중간 시점 스크럽·EvaluationProgram 선택·Rewind Debugger Details 열기를 완료하면 새 refs로 읽는다. 툴의 Pause/트랙 Click 성공 반환만으로 실제 일시정지/시점 선택 완료라고 기록하지 않는다. 추가 녹화/도구 구현 없이 기존 녹화를 사용한다. 사용한 observer_2/3은 해제했다.
- 근거는 `AlwaysUpdateRetry.log`·`AlwaysUpdateRetrySummary.json`이다. **첫 재생의 갱신 순서 오류는 변경 후 표본에서 재발하지 않았고 정상 종료 상태를 확인했으나, 이번 DefaultSlot 단위는 task·단일 전진·Stop/교체 확인 전으로 미완료**다. 실제 GAS ActorInfo/ANS 저장 대상과 슬롯/ANS 전체 게이트를 통과시킨 것도 아니다. 코드/에셋 추가 수정·빌드·맵 저장/복원·커밋/푸시는 없다.

### Rewind의 ABP 몽타주 트랙 소유권·실제 UAF 피격 task 확인

- 사용자는 피격 시점을 열고 몽타주 트랙이 UAF가 아닌 ABP 하위인 점을 질문했다. 설치 엔진 `AnimInstance.cpp:583`은 AnimInstance를 소유자로 TRACE_ANIM_MONTAGE를 기록한다. `UAFMontageComponent.cpp`는 그 인스턴스를 몽타주 전용으로 틱하고 위치/재생률/블렌드를 복사하며, `MontageTrait.cpp:408~418`은 시퀀스/AnimPosition으로 UAF keyframe task를 추가한다. 따라서 현재 구성에서 ABP 하위 몽타주 시간 트랙과 UAF 포즈 평가는 양립한다. ABP 트랙만으로 native 포즈 평가를 통과시키지는 않는다.
- 실제 새 Slate 읽기에서 PIE Resume 버튼을 확인해 일시정지 상태를 확인했다. 사용자가 연 UAFAnimGraph_0/EvaluationProgram의16개 task 중 **Index8=Anim Next Anim Sequence Keyframe Task·A_SW_Damage_Fast_UEFN·SampleTime0.282371·Looping=false**를 펼쳐 읽었다. Index9/10의 블렌드·Index12 OffsetRoot·Index13 최종 구면 보정·Index15 ControlRig가 뒤따른다. 이 프레임의 피격 포즈가 native UAF 평가 프로그램에 들어 있음을 확인했다. 최종 본별 동등성이나 모든 프레임의 중복 평가 전수 검증은 아니다.
- 원본은 `HitEvaluationProgram.txt`다. 화면의 Rewind 커서는 약13초 부근이지만 정확한 시간값을 별도로 읽지 못해0.282371과 시간축의 정확한 비율을 아직 계산하지 않는다. 단일 전진 확인을 위해 기존 녹화에서 약1초 뒤로 스크럽하고 EvaluationProgram을 다시 선택하도록 수동 요청했다. 추가 녹화/자동화 도구는 만들지 않았다. observer_4/5는 해제했다.
- 정상 종료/순서 오류 재검증과 실제 UAF 피격 task 확인은 충족했으나 **단일 전진·즉시/블렌드아웃 Stop·교체는 미검증**이다. 전체 DefaultSlot 단위와5-2/Go-NoGo는 미완료다.

### 두 시점의 재생 전진 비교·Stop/교체 수동 시험 준비

- 사용자의 약1초 뒤 스크럽 후 같은 UAF task의 SampleTime은 **0.396758**이다. 앞선0.282371과의 차는 **0.114387초**다. 동일 화면 눈금/스케일에서 커서는 약12.9초→14.1초 부근으로 이동했고, 약1.1~1.2초 녹화 구간에0.114387초 샘플 전진은 요청0.1배속과 부합한다. 대표 표본에서 두 배 전진 징후는 없다고 판정한다. 정확한 커서 수치/게임 DeltaTime·모든 프레임의 틱 횟수는 읽지 못했으므로 정확한 비율이나 전수 단일 틱 검증으로 확대하지 않는다. 새 녹화/계측을 필수 조건으로 추가하지 않는다.
- 새 snapshot은 `HitSecondTimeSnapshot.txt`다. PIE Resume 버튼이 보여 일시정지 유지 상태이며 task는16개다. 사용한 observer_6은 해제했다. 교체 후보 A_SW_Blocking_Hit_UEFN은 bEnableRootMotion=false·AAT_None도 확인했다.
- 다음 수동 시험은 같은 PIE의 Rewind 프리뷰를 끝내고 Resume한 뒤, 공통 AM_SW_Damage_Fast_UEFN0.05배속 재생을 각 경우에 다시 실행하는 것이다. 재생 중 위치/playing을 찍고 Stop0/Stop0.5/다른 DefaultSlot 몽타주를 각각 수행한다. Stop0.5 뒤1초 이상·교체 몽타주0.1배속 재생 뒤9초 이상 대기한 종료 상태도 출력한다. 실제 동작 전 playing=true·위치가 있어야 중단 표본이며, 이미 종료한 재생에 Stop한 결과는 성공 근거로 쓰지 않는다. 코드/새 자동화 도구 대신 사용자 콘솔 조작을 즉시 요청한다.
- **즉시 Stop·블렌드아웃 Stop·교체는 실행 대기·이번 단위 미완료**다. 전체 슬롯/ANS 종료 계약·GAS ActorInfo·무기·루트모션·적/네트워크·최종 게이트도 남아 있다.

### Stop/교체 첫 시도 검토·종료 후 호출로 유효 중단 표본 부족

- 사용자는 이상 없음으로 보고했다. 로그의 원본 첫 재생은 UTC05:52:28.190이고 Stop0Before는05:52:47.026으로 약18.836초 뒤다.0.05배속 전체 재생 예상 약11.33초보다 늦었으며 **Stop0Before=False/0.0·Stop05Before=False/0.0·ReplaceBefore=False/0.0**이다. 각 경우 실제 재생 중이던 원본을 끊은 표본이 아니므로 Stop/교체 성공으로 판정하지 않는다. 사용자 관찰을 부정하는 것이 아니라 중단 대상이 없었던 조건을 구분한다.
- Stop0After/Stop05End는False/None, ReplacePlay는0.699999988079071, ReplaceAfter=False/True, ReplaceEnd=False/False/None이다. 새 몽타주 단독 재생/종료는 확인되지만 실제 재생 교체의 증거는 아니다. 해당 실행 구간 MM/Pose History/진단 오류는0이다.
- 근거는 `StopReplaceSmoke.log`·행동 구간 `StopReplaceActions.log`·`StopReplaceAttemptSummary.json`이다. 같은 PIE=true·시험 GameMode를 유지한다. 단위 완료/맵 복원·다음 구현 착수·커밋/푸시는 하지 않았다.
- 콘솔 입력 시간을 충분히 확보하도록 **매 시험 직전에 Montage_Play(원본,0.001,MontageLength,0.2)**를 새로 실행하는 수동 재시험으로 바꾼다. 원본은0.2초 지점부터 약366.7초 동안 남아 있어 수십 초 뒤 콘솔을 입력해도 실제 중단 대상을 확보한다. 각 동작 전에 playing/위치를 출력하고 true일 때만 유효 표본으로 인정한다. 위치를0.2초에서 시작하는 것은 이번 중단 시험의 입력이며 앞서 확인한 정상 시작/종료·0.1배속 비교를 대체하지 않는다. 새 코드/자동화 도구는 만들지 않는다.
- **이번 단위는 미완료**다. 즉시/블렌드아웃 Stop와 실제 교체 표본을 확보한 뒤 종료 판정을 한다.

### 유효한 Stop/교체 표본·단위 완료·시험 정리

0.2초 지점부터0.001배속으로 각 경우에 새로 재생한 재시험에서 다음을 확인했다. 느린 재생은 콘솔 입력 시간을 확보하는 시험 입력이며 실제 플레이 속도/블렌드 정책 변경이 아니다.

| 경우 | 중단/교체 직전 | 결과 |
|:---|:---|:---|
| 즉시 Stop | playing=True·위치0.2037631273 | Stop0 뒤 playing=False·active=None |
| 블렌드아웃 Stop | playing=True·위치0.2031794488 | Stop0.5 뒤 약6초 후 playing=False·active=None |
| 다른 DefaultSlot 몽타주 교체 | 기존 playing=True·위치0.2039573938 | 새 재생 반환0.6999999881·직후 기존False/새True·약8.94초 뒤 둘 다False/active=None |

- 해당 유효 중단 구간의 **MM 오류0·Pose History 오류0·UAFMMOrder0·LogAnimation Error0**이다. 사용자는 다른 이상을 보고하지 않았고, **즉시 Stop은 의도대로지만 플레이 감각으로는 어색하다**고 관찰했다. 0초 블렌드의 강제 중단 기능 확인과 플레이 품질 수용을 구분한다. 이 결과로 실사용 취소 블렌드를0초로 정하거나 기존 정책을 변경하지 않았다. Stop0.5의 정확한 종료 프레임·곡선이나 ANS End 콜백을 수치 검증한 것은 아니다.
- 이로써 **native DefaultSlot 최소 연결 단위는 관측한 스파이크 범위에서 완료**다. 실제 UAF keyframe task·정상 재생/종료·대표 구간 전진·유효한 즉시/블렌드아웃 Stop·교체와 사후 잔류 없음을 확인했다. 대표 전진 근거는 위 두 시점/눈금 비교이며 모든 프레임 틱 횟수의 전수 측정으로 확대하지 않는다. 첫 무효 Stop/교체 표본은 삭제하지 않고 유효 재시험과 분리 보존한다.
- 실제 원본 AnimInstance의 재생 경로와 UAF 포즈 평가를 확인했지만 **GAS ActorInfo/ANS 전달 대상·ANS5종 Begin/End/잔류·다른 슬롯/그룹·가산 슬롯·무기 레이어·루트모션·적/네트워크·최종 포즈/성능은 미검증**이다.5-2와 최종 일곱 게이트·Go/NoGo는 계속 미판정이다. 다음 제안은 §20의 슬롯·ANS 계약 확장으로 사용자 진행 확인 전 착수하지 않는다.
- 종료 정리로 PIE를 정상 종료하고false를 확인했다. TestMap03 WorldSettings의 **DefaultGameMode 속성 하나만** 원래 `/Game/Gameplay/BP_LNPGameMode.BP_LNPGameMode_C`로 메모리 복원하고 재조회했다. 열린 에셋은 없고 맵 dirty=true다. 맵 저장·전체 원복은 하지 않았으며 디스크 맵/다른 사용자 에셋3개·시험 BP의 해시는 단위 시작과 같다. 현재 디스크 맵은 이전 시험 설정을 유지하므로 재시작/재로드 후 값은 새로 확인한다.
- 현재 AG 해시는 true 변경 후의 `3EBCB7119262252281E5929D5207434A0ADA26ADC8EDD8A973CB45477FF3E77A`를 유지한다. 이번 단위는 AG의 native 트레이트/옵션과 문서4개 변경이며 C++ 변경·추가 빌드·새 도구·커밋/푸시는 없다. 기존 사용자 변경4개를 포함하거나 원복하지 않았다.
- 근거는 `StopReplaceValidSmoke.log`·`StopReplaceValidSummary.json`·`AfterValidSmokeHashes.json`·`ClosureAudit.json`이며 최초 캡처/trace/UAF task 근거는 같은 DefaultSlot 폴더와 앞선 녹화를 재사용한다. README/Decisions·프로젝트 공통 설계는 새 확정 결정/정식 마이그레이션이 없어 변경하지 않았다.

## 27. FullBody 슬롯 추가·ANS5종 계약 확인(2026-10-28, 시스템 종료 계약 미통과)

사용자 진행 승인으로 슬롯·ANS 확장에 착수했다. 이번 구현 단위는 **FullBody 슬롯 추가와 기존 근접 몽타주의 ANS5종/실제 GAS 대상 확인**으로 좁힌다. UpperBody의 본 마스크/AO/IK 결합은 레이어 게이트와 함께 후속 단위에서 다룬다. 전체 슬롯/ANS 게이트를 이번 준비만으로 통과시키지 않는다.

### 계획과 성공 기준

1. 기존 슬롯/근접 몽타주·실제 콜백 경로 확인 → 기존 슬롯 순서·원본을 보존하고 필요한 구성만 추가.
2. 스파이크 폰 한정 진단·FullBody 연결 → 기준 전체 빌드와 AG Compile/Save 확인.
3. 실제 수동 실행 → FullBody 포즈/DefaultSlot 배타 그룹, ANS5종 Begin/End 페어·기능 Tick 대상·GAS ActorInfo 메시/인스턴스 일치, 정상 종료/유효 중단/교체/이동 취소/시스템 종료에서 태그·근접 엔티티 정리 확인. 결과마다 관측 범위와 미검증 항목을 구분한다.

### 확인한 기존 구성·범위

- 기존 ABP의 슬롯 순서는 FullBodyAdditivePreAim→DefaultSlot→FullBody→마스크를 씌운 UpperBody→AdditiveHitReact다. 단순 전신 UpperBody 슬롯 추가로 본 마스크를 대체하지 않는다. 기존 런타임 그룹 등록은 그대로 사용한다.
- 기존 `Saved/AnimRetarget/LyraToUEFN_RetargetList_compare.json`에서 AM_SW_Attack_01_UEFN의 DefaultSlot·ANS5종/두 공격 구간을 확인했다. 첫 구간은 이동/공격 차단 시작 약0초, MeleeHitWindow0.6611~0.9943초, ComboWindow1.0024~1.3405초, CancelOnMovement1.3368~2.4667초다. 이는 기존 리타깃 대조 자료이며 새 런타임 발화 근거가 아니다. 현재 Notifies 직접 조회는 지원되지 않으므로 필요하면 기존 AnimationLibrary 읽기 방법으로 좁혀 확인한다.
- UAF dispatcher는 실제 NotifyBegin/Tick/End를 선택된 SkeletalMeshComponent로 전달하지만 ANS5종에 호출 횟수 로그가 없었다. 태그가0이라는 사실만으로 중복 Begin/End를 배제하지 않는다. MeleeHitWindow는 실제 무기 정의/WeaponMesh가 있어야 엔티티를 만들므로 콜백 로그와 BuildQueued를 함께 확인한다.
- 근접 엔티티 검증은 정상 속도 표본을 기본으로 한다. MeleeData.TimeToLive는 ANS 원래 TotalDuration+0.2초이며 극단적으로 느린 재생에 맞춰 늘어나지 않는다. §26의 입력 시간 확보용 느린 재생을 근접 엔티티 수명 동등성 근거로 쓰지 않는다.

### FullBody 수동 연결 보고·보존

- 시작 읽기 PIE=false였다. 사용자에게 기존 DefaultSlot Result→새 Montage Source→Offset Root Input, Slot Name=FullBody·Always Update Source=true·Disable Synchronization=false로 추가/Compile/해당 AG만 Save/탭 닫기를 즉시 수동 요청했다. 새 편집 도구를 만들지 않았다.
- 사용자는 완료를 회신했다. 새 조회에서 PIE=false·AG dirty=false·열린 에셋 없음과 컴파일/저장/탭 닫힘 로그를 확인했다. AG SHA256은 **DADEE0844CEE352A47965875247B2D1CA4A697E4B343A5D85B0E35EBC351C00D**다. 정확한 설정/배선은 사용자 보고를 근거로 하며 FullBody 실제 포즈는 아직 미검증이다.
- `Saved/AnimationOverhaul/2026-10-28_UAF52SlotANS/`에 이전 정상 DefaultSlot 사본·추가 후 AG·StartHashes/AfterFullBodyHashes를 보관했다. 시험 BP와 기존 사용자 에셋4개의 디스크 해시는 그대로다. 맵 저장·GameMode 변경·PIE는 아직 하지 않았다.

### 진단 코드 준비·빌드 요청

- `LNPUAFNotifyAudit.h/.cpp`를 추가했다. **LNP.UAFSpike.DebugNotifies 기본0**,1은 Begin/End와 엔티티,2는 실제 기능을 가진 Melee/Cancel의 Tick도 기록한다. 대상은 ALNPUAFSpikePlayerCharacter뿐이다. notify 객체/이벤트·Queued/Branching 모드·실제 전달 Mesh/AnimInstance·GAS ActorInfo Mesh/AnimInstance·태그 수·프레임/월드 시각을 기록한다.
- ANS5종의 기존 콜백에 진단 호출만 넣고, 원래 Tick만 있던 CancelOnMovement에는 동일 Super 호출+로그의 Begin/End override를 추가했다. 기존 차단/콤보/취소·Melee 엔티티 생성/파괴·TTL 정책은 변경하지 않았다. 태그 ANS의 로그는 기존 변경 뒤이며, Melee Begin/End는 진입 시점이다. 모든 로그를 일괄 사후 상태로 해석하지 않는다.
- Melee의 BuildQueued/DestroyQueued를 기록하고, 파괴 요청 뒤0.1초의 게임 스레드 ticker에서 동일 엔티티 핸들의 유효/활성을 읽는다. 직접 정리하지 않으며 월드가 종료 중이거나 없어 확인할 수 없으면 AfterDestroyUnavailable로 남긴다. 무효 핸들에서 IsEntityActive를 호출하지 않는다. 이 조회는 실제 End 호출/정리를 대신하거나 End 누락을 숨기지 않는다.
- 소스는 기존6개/신규2개이며 C++ API와 diff 공백 검사를 대조했다. **컴파일 전**이다. 사용자에게 필요한 본인 작업만 저장 후 LootNPop 에디터 종료를 요청했다. 종료 회신과 실제 프로세스 부재 확인 전 빌드하지 않는다. 기존 Live Coding 크래시 이력 때문에 Live Coding 대신 권한 확장 기준 Development 전체 빌드를 사용한다.
- 현재 **에디터 종료/전체 빌드·실행 확인 대기, 이번 단위 미완료**다. 콜백/그룹/ANS End·GAS 대상·엔티티 정리를 아직 통과시키지 않으며5-2/Go-NoGo도 미판정이다. 커밋/푸시는 없다.

### 에디터 종료 확인·전체 빌드 성공·첫 수동 실행 요청

- 사용자는 저장 안 함으로 에디터를 종료했다고 회신했다. 첫 CIM 조회는 접근 불가였으므로 빈 결과를 종료 근거로 쓰지 않았다. 실제 빌드 명령의 Get-Process 조회에서 UnrealEditor 프로세스가 없음을 확인하고 ShutdownGuard.json을 갱신했다. 강제 종료/에디터가 열린 상태의 빌드·Live Coding은 없다.
- 기준 명령을 처음부터 권한 확장으로 실행했다: LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE. **16 actions·106.09초·Succeeded·종료 코드0**이다. UHT 처리·진단/ANS6개 cpp와 관련 모듈 컴파일·lib/dll 링크·metadata를 통과했다. 셸 측 전체 경과106.499초는 별도 측정이며106.09초는 UBT 보고값이다.
- Build.log·BuildSummary.json·ShutdownGuard.json·AfterBuildHashes.json을 보관했다. FullBody AG/시험 BP/사용자 에셋4개의 디스크 해시는 빌드 전과 같다. 맵 메모리 복원을 저장하지 않고 종료했으므로 재실행 뒤 GameMode는 새로 확인해야 한다.
- 사용자에게 에디터 재실행/TestMap03·시험 BP_GameModeUAFSpike 확인(필요시 임시 지정, 맵 미저장), PIE 전 Cmd 콘솔에서 DebugNotifies2/DebugLocomotion2, 새1인 Play Standalone PIE를 요청했다. 롱소드 장착/적 없는 곳/이동 없이 정상 공격3회(각 Idle 복귀 후)와 대시3회·이상 여부 보고/PIE 유지를 요청했다. 첫 표본에 새 녹화/Python은 필요 없다.
- 현재 **첫 실행 회신 대기·이번 단위 미완료**다. 빌드 성공을 실제 콜백/대상/정리·FullBody/ANS 계약 통과로 처리하지 않는다. 이후 새 runtime refs로 로그/태그/엔티티를 확인하고 중단/교체/이동 취소/종료를 검증한다. 추가 소스 수정·맵 저장·커밋/푸시는 없다.

### 정상 공격3회·ANS 페어/실제 GAS 대상/정리 확인

- 사용자는 정상 공격/대시에서 이상 없음으로 보고했다. 재시작 후 새 조회에서 PIE=true·BP_PlayerUAFSpike_C_0·시험 GameMode·DebugNotifies2를 확인했다. CVar2 설정은 PIE 전 로그에도 있다. 신규 runtime 참조를 사용했으며 이전 Python/widget 참조는 재사용하지 않았다.
- 정상 공격3회의 ANS5종은 **각 Begin3/End3·총15쌍**, 최대 동시 균형1·최종0·음수 없음이다. 기능 Tick은 Melee49회·Cancel158회다. 전체 콜백237개에서 실제 전달 Mesh/AnimInstance와 GAS ActorInfo의 Mesh/AnimInstance가 모두 AnimSourceMesh/ABP_SourceUAFSpike로 일치했다. Mode0(Queued) 표본이며 Branching Point 전수 검증이 아니다.
- 엔티티122:1/122:3/122:5 모두 BuildQueued→DestroyQueued→AfterDestroy(valid0/active0)를 확인했다. 마지막 태그 샘플은 move/attack/combo0이고 실제 ASC 활성 태그도 AimMode.None/VisualSet.LongSword뿐이었다. MM/Pose History/순서 진단/LogAnimation 오류0이다. 에디터 재접속의 MCP Unknown session 로그는 애니메이션 오류와 분리한다.
- FullBody 대시3회는 사용자 관찰 이상 없음이며 별도 Rewind task/클립 전수 조회는 하지 않았다. 정상 공격의 실제 GAS 경유 재생과 ANS 원본 대상 일치를 확인했으나 다른 중단/시스템 종료까지 확대하지 않는다. 근거는 FirstNormalSmoke.log·FirstNormalSummary.json·FirstNormalRuntimeReads.json이다.

### 이동 취소·대시 교체 표본·다음 활성 창 준비

- 사용자는 공격 직후 이동 유지와 공격 중 대시 교체에서 이상 없음으로 보고했다. 실제 캡처는 대기/추가 시도를 포함해 이동/공격 차단 ANS 시작12회이며 요청3+3의 정확한 분모가 아니다. 각 ANS Begin/End는 이동/공격 차단12쌍씩·Melee/Combo/Cancel9쌍씩으로 **총51쌍**이다. 모든 이벤트 균형 최대1·최종0·음수0, 기능 Tick의 활성 구간 밖 호출0이다.
- MovementStop 호출6회와 선딜 차단 ANS의 약0.256~0.322초 조기 End 표본을 확인했다. 일부 Cancel 창도 정상 약1.13초보다 짧은0.128/0.207초에 종료됐다. 사용자 대시 조작과 대응하는 중단 표본이며 새 몽타주를 직접 기록한 전수 트랙 증거는 아니다. Melee9쌍은 약0.34~0.38초라 활성 Melee의 강제 중단 근거로 쓰지 않는다.
- 전체321콜백의 GAS/ANS 대상 불일치0이다. 엔티티9개의 Build/Destroy/AfterDestroy가 각각9회이고 모두valid0/active0이다. 조회된 차단/콤보 태그 잔류와 MM/Pose History/순서 진단 오류0이다. 원문/요약/사후 태그 조회는 MovementDashSmoke.log·MovementDashSummary.json·MovementDashRuntimeReads.json이다.
- 다음 유효 강제 중단 표본은 **일시정지한 PIE에서 원본 몽타주0.72초/1배속 재생 후 Skip1회**로 준비하도록 즉시 수동 요청했다. Melee 창이 짧고 TTL은 애니 재생률에 맞춰 늘지 않으므로, 월드 시간을 멈춘 상태에서 활성 콜백/엔티티를 확보한 뒤 Stop/Resume하는 방식이다. 준비 단계 자체를 실제 Begin/생성 확인으로 처리하지 않고 회신 후 새 로그로 확인한다. 새 자동화 도구/진단 코드 추가는 없다.
- 현재 준비 회신 대기이며 **이번 단위 미완료**다. 강제 Stop0/블렌드아웃·시스템/인스턴스 종료·다른 슬롯/레이어·최종 게이트는 남아 있다. 같은 PIE를 유지했고 맵 저장/복원·추가 빌드·커밋/푸시는 없다.

### 활성 Melee 즉시 Stop·Combo 블렌드아웃 Stop·Cancel 교체 확인

- Pause/0.72초 재생/Skip1회 뒤 이동 차단·공격 차단·Melee가 같은 프레임에 Begin했고 Melee Tick/엔티티122:25 BuildQueued를 확인했다. ASC에서도 두 차단 태그가 활성 상태였다. Stop 직전 playing=True·위치0.7366669178·월드 시각1147.659564로, 일시정지 중 실시간 대기에도 시뮬레이션 시각은 유지됐다.
- Stop0/Resume 뒤 월드 시각1147.676231(차 약0.016667초)에서 세 ANS가 각각 End1회, 엔티티 DestroyQueued와0.1초 뒤valid0/active0을 확인했다. 원래 Melee 종료 경계나 TTL까지 기다린 표본이 아니다. Stop 직후False/None·사후 차단 태그 잔류 없음이다. 원문은 PausedMeleePrepare.log·PausedMeleeStop0.log다.
- Combo 준비1.10초/Skip1회 뒤 이동 차단·Combo Begin을 확인했다. Stop0.5 직전True·위치1.1166669130이며 Resume 뒤 월드 시각1151.445586→1151.462252에서 두 End가 각각1회였다. 사후 태그 잔류 없음이다. 정확한 시각별 포즈 블렌드 곡선을 측정한 것은 아니다.
- Cancel 준비1.60초/Skip1회 뒤 Cancel Begin을 확인했다. 교체 직전True·위치1.6166666746, FullBody Dash 새 재생 반환0.9333000183·직후 기존False/새True, Resume 뒤 월드 시각1154.840198→1154.856864에서 Cancel End1회다. 이 교체는 물리 대시 입력이 없는0.1배속 포즈 시험이다. 원문은 PausedComboCancel.log, 세 경우 요약은 PausedStopsSummary.json이다.
- 이로써 ANS5종의 정상 경로와 대표 활성 중단 경로를 확인했다. **모든 ANS×모든 Stop/교체 변형의 전수 조합을 검증한 것은 아니다.** 실제 객체/이벤트와 callback 대상은 로그로 확인하며 이번 추가 표본에도 순서 오류/LogAnimation 오류는 없었다. Branching Point는 여전히 미검증이다.

### 활성 ANS 상태의 UAF 시스템 종료 준비

- 마지막 종료 경로는 현재 PIE에서만 UAFLocomotion 컴포넌트를 제거해 Actor/ASC가 남은 상태의 End/태그 정리를 확인하는 시험으로 한다. 종료하면서 Actor/ASC도 함께 없어지는 PIE Stop만으로 잔류 여부를 통과시키지 않는다.
- 설치 엔진 UUAFComponent::EndPlay는 Deactivate, OnUnregister는 SystemReference.Reset을 호출한다. UActorComponent의 BlueprintCallable K2_DestroyComponent는 ScriptName=DestroyComponent이며 소유 Actor를 인자로 전달하는 경로를 확인했다. 기본 dispatcher의 실제 종료 End 보장은 아직 실행 미검증이다.
- 사용자에게 Pause 상태의 그룹 이름 조회/원본0.72초 재생→Skip1회→UAFLocomotion 검색/중단 직전 출력/DestroyComponent(소유 Actor)→컴포넌트 목록 출력→Resume1~2초/Pause를 요청했다. 자산이 아닌 PIE 컴포넌트만 제거하며 포즈가 멈추는 것은 예상되는 시험 결과다. 실행 보고/실제 Begin·제거·End·태그 정리를 확인하기 전 통과시키지 않는다.
- 현재 **종료 시험 회신 대기·이번 단위 미완료**다. 컴포넌트/시스템 종료 이후 잔류·인스턴스 종료·전체 슬롯/레이어·최종 Go/NoGo는 미판정이다. 추가 코드/빌드·맵 저장·커밋/푸시는 없다.

### 시스템 제거 시 End 누락·차단 태그 잔류 확인 — 종료 계약 미통과

- 사용자 실행 결과 그룹 조회는 원본 근접/FullBody Dash 모두 **DefaultGroup**이었다. Pause/0.72초 재생/Skip1 뒤 이동 차단·공격 차단·Melee의 Begin3·Melee Tick·엔티티122:27 BuildQueued를 확인했다. 실제 대상과 GAS 참조는 기존 원본 메시/인스턴스로 일치했다.
- 제거 직전 원본은 **playing=True·위치0.7366667986**이었다. DestroyComponent(소유 Actor) 뒤 UAFLocomotion 이름 목록은 **[]**이고 Actor/ASC는 남아 있었다. Resume 뒤 공급층 AnimB3 로그도4회 있어, 단순히 일시정지한 채 다음 갱신을 기다린 상태와 구분한다.
- 이 캡처는 **Begin3·End0·DestroyQueued0·AfterDestroy0**이다. 실제 ASC 조회에도 **LNP.Block.MovementInput·LNP.Block.AttackInput이 잔류**했다. 로그 오류/크래시가 없어도 게임 상태 정리 계약은 실패다. 엔티티는 End에서 파괴 요청이 없었지만 TTL 이후 실존을 직접 조회하지 않았으므로, 살아 있는 엔티티 누수나 정상 정리를 어느 쪽으로도 단정하지 않는다.
- 원문은 SystemShutdown.log, 요약은 SystemShutdownSummary.json, 제거 뒤 태그 조회는 SystemShutdownTags.json이다. PIE 종료 뒤까지의 원문도 SystemShutdownAfterPIEStop.log에 보관했으며 해당 캡처에서도 End/DestroyQueued가 나타나지 않았다. 기존 정상/대표 Stop/교체 결과와 실패 표본을 섞어 전체 무오류 통과로 기록하지 않는다.
- 확인한 엔진 경로에서 UUAFComponent EndPlay는 Deactivate, OnUnregister는 SystemReference.Reset을 호출하고, 기본 notify dispatcher의 End 큐 생성은 OnEndExecution의 활성 목록 비교에 있다. 이번 종료 경로에서 마지막 갱신 없이 활성 상태가 버려진다는 설명과 관측이 부합한다. 모든 UAF 그래프/종료 방식의 보편 결함이나 동등 보완 불가능이 증명된 것은 아니다.

### 이번 구현/검증 단위의 판정과 정리

- FullBody 구성/진단 구현·전체 빌드와 정상/이동 취소/대시 교체·대표 활성 Stop0/Stop0.5/교체 시험을 수행했다. **종료 계약은 미통과**이며 §27/몽타주·ANS 게이트를 완료로 체크하지 않는다. 다음은 **UAF 시스템 종료 시 활성 ANS를 안전하게 끝내는 경로 조사·동등 보완** 제안이며 사용자 진행 확인 전 새 코드/에셋 수정을 하지 않는다.
- 다음 보완은 실제 End를 한 번 전달해 원래 ANS가 소유한 태그·스윙 엔티티를 정리하고, 실행 중인 워커/게임 스레드 이벤트의 동기화와 중복 End를 피할 수 있는지를 확인하는 것이다. 태그만 지우거나 TTL 만료를 End 보장으로 취급하지 않는다. 프로젝트에서 동등하게 보완할 수 없음이 확인되면 AN-006에 따라 No-Go를 판정한다. **현재 최종 Go/NoGo는 계속 미판정**이다.
- 실패 근거 확보 뒤 PIE를 정상 종료했다. 새 조회 PIE=false·열린 에셋 없음, TestMap03 DefaultGameMode는 원래 `/Game/Gameplay/BP_LNPGameMode.BP_LNPGameMode_C`로 속성 하나만 메모리 복원했다. 맵 dirty=true이며 저장하지 않았다. 원본 사용자 맵/다른 에셋 변경을 통째로 원복하지 않았다. UAF 제거는 종료된 PIE 인스턴스에만 적용돼 BP/AG는 그대로다.
- 종료 시 AG DADEE084…·시험 BP·사용자 에셋4개는 빌드 직후와 같은 디스크 해시다. DebugNotifies2/DebugLocomotion2를 유지하며 다음 PIE는 새 runtime/Python 참조를 구한다. 정리 근거는 ClosureAudit.json·ClosureEditorReads.json이다.
- 이번 단위 변경은 FullBody 시험 AG·진단 소스8개(기존6/신규2)·문서4개이며 빌드 이후 추가 코드/빌드·맵 저장·커밋/푸시는 없다. 정상 결과·실패/한계·바로 다음 행동을 Current/Roadmap에 반영했다. README/Decisions·정식 전투 애니 설계는 정식 마이그레이션/새 확정 결정이 없어 바꾸지 않았다.

## 28. UAF 모듈 해제 시 활성 ANS 종료 보완(2026-10-28, 생존 Actor의 활성3종 제거 보완 확인)

사용자가 §27 다음 보완 단위를 승인했다. 앞서 실패한 **Actor/월드가 살아 있는 UAFLocomotion 컴포넌트 제거**를 먼저 수정하고, 정상 종료 뒤 중복 End가 없는지 함께 검증한다. 최종 슬롯/ANS 게이트나 전체 수명주기 동등성을 선승인하지 않는다.

### 조사와 구현

- 설치 엔진의 기본 FUAFNotifyDispatcherComponent는 OnEndExecution에서 활성 목록을 비교해 게임 스레드 End/Begin/Tick 이벤트를 만들지만, 소멸자에는 남은 상태 종료가 없다. 해당 가상 함수의 DLL export도 없어 상속 후 Super 호출로 작은 보완을 붙이는 경로는 사용하지 않았다. 엔진/기본 dispatcher 코드는 수정하지 않는다.
- FLNPUAFNotifyCleanup 모듈 컴포넌트를 추가했다. 기존 스파이크 루트 trait의 PostEvaluate에서 기본 dispatcher가 존재할 때만 그 뒤에 등록한다. 모듈의 컴포넌트 순회 순서에 따라 기본 OnEndExecution 뒤에 현재 활성 참조와 메시를 복사한다. GT에서 워커의 mutable 상태를 직접 읽지 않고 소유한 사본만 사용한다.
- FAnimNextModuleInstance::ResetBindingsAndInstanceData는 GT 확인·tick 등록 해제 뒤 ReleaseComponents를 수행한다. 새 컴포넌트 소멸자는 남은 사본을 먼저 비우고, 실제 NotifyEnd를 원래 메시/이벤트 참조로 전달한다. 태그만 강제 제거하거나 Melee TTL을 늘리는 우회는 하지 않는다. 기존 ANS가 자기 태그와 엔티티를 정리하게 한다.
- 현재 a.AnimNext.GameThreadEventBatchMode=1을 새로 조회했다. 기본 큐/틱 완료 경로의 게임 스레드 이벤트 처리를 확인했지만 다른 배치 모드나 모든 중도 해제 조합의 런타임 전수 증명은 아니다. 정상 End 이후 스냅샷이 비어 추가 End가 없는지 실제 표본이 필요하다.
- 메시/월드가 사라졌거나 월드가 종료 중이면 경고 후 호출을 생략한다. 따라서 **Actor 파괴/월드 종료까지 같은 계약을 해결했다고 주장하지 않는다.** 현재 실패 재현과 같은 생존 Actor의 모듈 제거가 우선 검증 대상이다.

### 계획과 검증 상태

1. 신규 타입 전체 빌드 → 에디터 종료 회신과 실제 프로세스 부재 확인 후 기준 Development 빌드. Live Coding은 사용하지 않는다.
2. 정상 공격/기존 중단 회귀 → 실제 ANS 페어·대상·태그/엔티티 정리 및 중복 End 확인.
3. Pause/0.72초 재생/Skip1로 활성 차단2종+Melee 확보 → 같은 UAF 컴포넌트 제거/Resume 뒤 End 각1·태그 제거·실제 엔티티 파괴 요청과 사후 무효·관련 순서 오류0 확인.

- 소스는 기존 루트 trait1개/신규 Cleanup2개이며 diff 공백 검사를 통과했다. **컴파일/실행 전으로 종료 계약은 계속 미통과**다. PIE=false·열린 에셋 없음이며 에디터 종료를 수동 요청했다. 사용자 변경4개/시험 BP/AG는 이번 보완 시작 해시를 유지했다. 맵 저장·에셋 추가 편집·커밋/푸시는 없다.
- 로컬 근거는 Saved/AnimationOverhaul/2026-10-28_UAF52NotifyCleanup/의 BeforeHashes.json·LNPSphericalOffsetRootTrait_Before.cpp부터 누적한다. 이전 실패/정상 표본은 SlotANS 폴더에 그대로 보존한다.

### 전체 빌드 성공·수동 재검증 요청

- 사용자가 에디터 종료를 회신했고 권한 확장 빌드 명령의 Get-Process 가드에서 UnrealEditor0개를 확인했다. 기준 LootNPopEditor Win64 Development·WaitMutex·NoHotReloadFromIDE로 **7 actions·47.13초·Succeeded·종료0**을 확인했다. UHT 생성3개·신규 Cleanup/기존 루트 trait/생성 모듈 컴파일과 lib/dll 링크를 통과했다. 셸 경과47.5247674초는 별도 수치다. Live Coding은 없다.
- ShutdownGuard.json·Build.log·BuildSummary.json·AfterEditorCloseHashes.json·AfterBuildHashes.json을 보관했다. AG/시험 BP/사용자 변경4개는 보완 시작 해시와 같다. 맵/에셋 저장과 커밋/푸시는 없다.
- 에디터 재실행·TestMap03의 시험 GameMode 확인(임시 변경 시 맵 미저장), DebugNotifies2/DebugLocomotion2·새1인 Standalone PIE·롱소드 정상 공격3회를 수동 요청했다. 첫 단계는 콜백 회귀 확인이며 실행 회신 대기다. 그 뒤 정상 종료 후 모듈 제거의 중복 End와 활성 ANS 제거를 확인한다. **빌드 성공만으로 종료 계약 통과를 판정하지 않는다.**

### 보완 후 정상 공격 회귀 확인·비활성 모듈 해제 시험 대기

- 사용자는 정상 공격3회에서 이상 없다고 보고했다. 새 읽기에서 PIE=true·시험 BP_PlayerUAFSpike_C_0·시험 GameMode·진단2/2를 확인했다. NormalRegression.log와 Summary.json·RuntimeReads.json에 보관했다.
- ANS5종 각각 Begin3/End3·총15쌍이고 모든 균형은 최대1/최소0/최종0이다. 기능 Tick은 Melee46·Cancel148이며 활성 구간 밖 호출0이다. 콜백224개 모두 GAS/ANS 메시·인스턴스가 원본 AnimSourceMesh/ABP_SourceUAFSpike로 일치했다. Queued(mode0) 표본이며 Branching Point 전수 검증은 아니다.
- 엔티티122:1/122:3/122:5는 모두 BuildQueued→DestroyQueued→AfterDestroy(valid0/active0)다. 현재 ASC 태그는 AimMode.None/VisualSet.LongSword뿐이다. MM/Pose History/순서 진단/LogAnimation 오류0·종료 보완 경고0이다. 정상 재생에서 보완 추가로 인한 관측 회귀는 없다.
- Idle/Pause 후 새 Python 참조로 active montage=None을 확인하고 UAF 컴포넌트를 제거, Resume1~2초/Pause하도록 수동 요청했다. **모듈 해제에서 정상 End를 중복 호출하지 않는지와 활성 ANS 종료 보완은 아직 미검증**이다. 제거는 PIE 인스턴스에만 적용하며 맵/에셋 저장·추가 코드/빌드·커밋/푸시는 없다.

### 정상 종료 후 UAF 제거·프로젝트 ANS 중복 End 없음

- 사용자 완료 회신 뒤 로그의 IdleReleaseBefore=None·IdleReleaseAfter=[]를 확인했다. Resume 뒤 공급 틱7회와 실제 ModuleRelease 로그가 있어 제거/지연 해제가 진행된 표본이다. 프로젝트 ANS5종의 추가 Begin/End/Tick0·엔티티 추가 요청0·현재 ASC 차단/콤보 태그0·관련 애니메이션/Python 오류0이다. 정상 공격 종료 뒤 그 ANS의 중복 End는 관측되지 않았다.
- 단, ModuleRelease의 native 활성 참조 수는 **remaining=1**이다. 프로젝트 ANS5종 로그와 구별하며 대상은 이번 로그에서 특정하지 못했다. Idle/active montage=None은 모든 그래프 ANS가 비어 있음을 뜻하지 않는다. 소멸자가 호출됐다는 근거와 전체 활성 상태가 비었다는 주장은 구분한다.
- IdleRelease.log·Summary.json·Tags.json을 보관했다. 새 PIE로 재시작/롱소드 장착/Idle/Pause 후 새 참조로 근접 몽타주0.72초 재생·Skip1을 요청했다. 실제 Begin/엔티티를 읽은 뒤 같은 컴포넌트 제거를 이어간다. **활성 제거의 End/정리는 아직 미검증**이며 전체 종료 계약·최종 GoNoGo는 미통과/미판정이다.

### 활성 Melee 준비 확인·실제 제거 회신 대기

- 사용자 완료 회신 뒤 새 PIE의 0.72초 재생 반환4.5666666031과 Skip1의 월드 시각9.158740에서 이동 차단/공격 차단/Melee Begin3·Melee Tick·엔티티122:1 BuildQueued를 확인했다. 실제 GAS/ANS 대상은 원본 메시/인스턴스로 일치하고 ASC의 이동/공격 차단 태그도 현재 활성이다. ActiveMeleePrepare.log·Tags.json이 근거다.
- 같은 Pause 상태에서 playing/위치/월드 시각을 출력하고 UAF 컴포넌트 제거→Resume1~2초→Pause하도록 요청했다. End/DestroyQueued/AfterDestroy·태그 제거는 아직 실행 회신 전이다. 새 자동화 도구/코드/빌드·맵 저장·커밋/푸시는 없다.

### 활성 제거 재시험 통과·이번 보완 단위 완료

- 제거 직전 playing=True·몽타주 위치0.7366669178·월드 시각9.1587399095를 확인했다. 제거 뒤 UAF 목록은[]이며 Actor/ASC는 살아 있었다. Resume 뒤 공급 틱2회와 ModuleRelease가 기록됐다.
- 보완은 native 활성 참조4개를 보관했고, 프로젝트의 이동 차단/공격 차단/Melee 각각 **Begin1·End1**이다. End 시각9.175407로 준비/제거 직전과 약0.016667초 차이여서 다음 시뮬레이션 프레임에 실제 종료가 전달됐다. 실제 ANS/GAS 메시·인스턴스도 원본으로 일치했다. 프로젝트3개 외 참조1개는 Idle 표본과 마찬가지로 대상 미특정이며, 전체4개에 대한 콜백 횟수를 주장하지 않는다.
- 엔티티122:1은 같은 End 프레임에 DestroyQueued가 발생했고0.1초 읽기에서 valid0/active0이다. 월드 시간상 TTL 만료보다 먼저 End/파괴 요청이 생겼으므로 TTL 정리를 대신 성공으로 계산한 표본이 아니다. 현재 ASC에는 AimMode.None/VisualSet.LongSword만 있고 이동/공격/콤보 태그는 없다. 관련 애니메이션/Python 오류0·종료 보완 경고0이다.
- PIE 종료 뒤까지의 추가 캡처에서도 Begin3·End3·BuildQueued1·DestroyQueued1·AfterDestroy1을 유지해 지연된 중복 End가 나타나지 않았다. 원문/요약은 ActiveSystemRelease.log·Summary.json·Tags.json 및 AfterPIEStop.log·Summary.json이다. 기존 §27 실패 원본은 그대로 남긴다.
- 이로써 **같은 실패 재현 조건의 생존 Actor UAF 컴포넌트 제거 보완 단위는 완료**다. 정상 공격3회 회귀·정상 종료 후 프로젝트 ANS 중복 End 없음과 활성 차단2종+Melee의 실제 End/정리를 확인했다. **Combo/Cancel의 활성 모듈 제거·Actor 파괴/월드 종료·다른 배치 모드/Branching Point는 미검증**이다. 특히 현재 코드는 월드 종료 중이면 호출을 생략하므로 전체 수명주기 동등성을 통과시키지 않는다. 전체 슬롯/ANS 게이트·최종 Go/NoGo도 미판정이다.
- 다음 제안은 나머지 ANS와 Actor/월드 종료 경로 검증이며 사용자 진행 확인 전 착수하지 않는다. 보완이 불가능한 경로가 확인되면 AN-006에 따라 판정한다. 다음 단위 범위가 정해지기 전에 무기/레이어 마이그레이션으로 넘어가지 않는다.

### 종료 정리와 보존

- PIE를 정상 종료하고 false·열린 에셋 없음을 확인했다. GameMode 속성만 원래 BP_LNPGameMode로 메모리 복원/재조회했다. 맵 dirty=true·미저장이며 디스크 맵을 덮어쓰거나 전체 원복하지 않았다. UAF 제거는 종료된 PIE 인스턴스에만 적용됐다.
- 사용자 에셋4개·시험 BP·AG의 디스크 해시는 이번 보완 시작과 모두 같다. AG는 FullBody 추가 후 DADEE084… 버전을 유지한다. ClosureHashes.json·ClosureEditorReads.json이 근거다. DebugNotifies2/DebugLocomotion2는 유지하고 다음 PIE는 새 참조를 사용한다.
- 이번 단위 변경은 기존 루트 trait1개/신규 Cleanup2개와 문서4개다. §27의 진단8개/시험 AG 변경은 기존 미커밋 작업으로 보존한다. 엔진/소스 에셋 변경·Live Coding·새 자동화 도구·맵 저장·커밋/푸시는 없다. README/Decisions/정식 전투 설계는 최종 게이트/정식 마이그레이션 전이라 갱신하지 않았다.

### 새 세션의 첫 검증 메모(제안·아직 미실행)

- 사용자 진행 요청을 받은 뒤 첫 단위를 시작한다. 기존 DefaultSlot·정상 공격3회·활성 Melee 제거를 인계 확인만을 위해 반복하지 않는다. 먼저 Combo/Cancel의 활성 모듈 제거를 확인하고 Actor/월드 종료의 정리 소유권을 조사한다. 한 구현 단위가 끝나면 다시 계속할지 확인한다.
- 시험은 맵을 저장하지 않은 BP_GameModeUAFSpike·진단2/2·롱소드·적 없는 위치를 유지한다. 컴포넌트 제거 뒤에는 같은 인스턴스를 재사용할 수 없으므로 **각 제거 시험은 새 PIE**로 시작한다. Idle/Pause 뒤 아래 공통 준비로 참조를 새로 만든다. 이어 후보 시작 지점으로 재생하고 Skip1만 실행한 뒤, 실제 Begin/태그 로그를 확인한다. 시점 값은 과거 Stop 표본의 유효 창을 재사용하는 입력이며 새로운 모듈 제거 결과가 아니다.

```python
import unreal
ans_w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
ans_p = unreal.GameplayStatics.get_player_pawn(ans_w, 0)
ans_s = next(c for c in ans_p.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == 'AnimSourceMesh')
ans_a = ans_s.get_anim_instance()
ans_m = unreal.load_asset('/Game/Characters/Retargeted/Lyra/AM_SW_Attack_01_UEFN')
```

| 후보 | Pause 중 재생 입력(실행 후 Skip1) | 먼저 확인할 활성 상태 |
|:---|:---|:---|
| Combo | `ans_a.montage_play(ans_m, 1.0, unreal.MontagePlayReturnType.MONTAGE_LENGTH, 1.10)` | 이동 차단/Combo Begin과 현재 태그. 과거 Stop0.5 표본에서 이 창을 확인했음 |
| Cancel | `ans_a.montage_play(ans_m, 1.0, unreal.MontagePlayReturnType.MONTAGE_LENGTH, 1.60)` | Cancel Begin/Tick. 과거 FullBody 교체 표본에서 이 창을 확인했음 |

- 실제 활성 상태가 확인됐을 때만 Pause 중 다음 제거를 수동 요청한다. `playing=True`와 위치가 전제다. Resume1~2초/Pause 후 실제 End 각1·정리·중복/관련 오류를 읽는다. 이미 종료한 몽타주에 제거한 표본으로 활성 종료를 통과시키지 않는다.

```python
ans_uaf = next(c for c in ans_p.get_components_by_class(unreal.ActorComponent) if c.get_name() == 'UAFLocomotion')
print('[UAFNextReleaseBefore]', ans_uaf.get_path_name(), ans_a.montage_is_playing(ans_m), ans_a.montage_get_position(ans_m), unreal.GameplayStatics.get_time_seconds(ans_w))
ans_uaf.destroy_component(ans_p)
print('[UAFNextReleaseAfter]', [c.get_name() for c in ans_p.get_components_by_class(unreal.ActorComponent) if c.get_name() == 'UAFLocomotion'])
```

- Actor/월드 종료는 위 생존 Actor 시험과 구별한다. 먼저 ANS의 ASC 접근·Melee 메시별 상태와 MassSubsystem, 엔진의 UUAFComponent 종료/모듈 해제 순서를 다시 확인해 콜백이 가능한 시점을 판단한다. 현재 `Mesh/World` 소실 또는 `bIsTearingDown`에서 End를 생략하는 동작과 AfterDestroyUnavailable은 성공 근거가 아니다. Actor가 사라져 태그를 조회할 수 없다는 사실만으로 End 정확히1회를 통과시키지 않는다. 필요 보완과 최소 수동 재현은 이 조사 뒤 결정한다.
- native 사본의 프로젝트 ANS 외 참조1개는 미특정이며 Idle의1개와 활성의 추가1개가 같은 대상이라는 증거도 없다. 현재 로그의 수가 같다는 사실만으로 동일성을 가정하지 않는다. Branching Point/다른 이벤트 배치 모드는 미검증으로 유지하며 관련 실패가 확인되면 별도 근거를 남긴다.
