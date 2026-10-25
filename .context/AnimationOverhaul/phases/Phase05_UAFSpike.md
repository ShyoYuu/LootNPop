# Phase 5 — UAF 판정 스파이크

> 상태: 5-0 완료, 5-1 진행 중(공중 고정 완료 — §12, 입력 배열·대표 전환 검증 단위 완료 — §13)
> 마지막 갱신: 2026-10-25
> 전제: Phase 2·3 완료. AN-006에 따라 판정 전 마이그레이션하지 않는다.

## 1. 목표와 범위

무기 없는 플레이어 1명을 Mover UAF와 UAF Motion Matching으로 구동하고 `Roadmap.md` Phase 5의 일곱 항목을 판정한다. 무기 레이어·몽타주 계약은 별도 시험 입력으로 확인한다. 기존 `ABP_Main_UEFN`·무기 서브 ABP·표시 ABP·Chooser·PSD는 수정하지 않는다. 시험 구성은 `/Game/Characters/UAFSpike/`에 둔다. GASP는 읽기 전용이다(AN-009).

**각 단위 종료 후 사용자에게 계속 진행할지 확인한다.** 에디터 조작·PIE·그래프 배선이 사용자 수동 조작으로 더 효율적이면 즉시 요청한다. 자동화 도구를 새로 만드는 것으로 우회하지 않는다.

## 2. 단위와 완료 조건

| 단위 | 작업 | 검증·종료 조건 | 상태 |
|:---|:---|:---|:---|
| 5-0 | GASP 참조 유무·로컬 엔진 API 조사, 실행서 작성 | 참조 조회 결과·재사용 경로·차단 후보·미검증 항목 기록. 코드·에셋 변경 없음 | 완료(2026-10-23) |
| 5-1 | 최소 테스트 폰·UAF 그래프, 기존 PSD·Chooser·Mover 궤적 연결 | 같은 Chooser가 반환하는 PSD 목록과 실제 UAF 검색 결과 확인. 북극·적도 이동/정지/회전·공중 고정·발 배치·움직이는 base 확인. 동등 구현 경로가 없으면 해당 사유로 조기 No-Go 판정 | 진행 중(최종 구면 루트·패널 병진·공중 고정·대표 입력/전환 확인, 워핑·발 배치 등 미완료) |
| 5-2 | 몽타주·ANS·레이어·루트모션·네트워크·디버깅 판정 | 아래 표 전 항목에 실행 근거를 붙여 Go/No-Go 기록. 기본 플레이어·승격 적 경로에 회귀가 없는지 확인 | 미착수 |

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
| PSD·Chooser | 기존 에셋을 변경하지 않고 같은 상태 입력의 PSD 배열로 UAF가 검색. Starts/Stops/Turns 등 전환과 인터럽트 확인 | 부분 확인 — Idle·출발/정지·착지·제자리 회전·방향 전환의 입력 배열/인터럽트/블렌드와 실제 UAF 선택 확인(§13). Pivot·Sprint Turns·보행 전환 등 전 종류 실증은 남음 |
| 몽타주·ANS | 슬롯 3종과 그룹 계약 유지. ANS 5종의 정상 종료·`Montage_Stop`(즉시/블렌드아웃)·교체 시 End 정확히 한 번, 태그/근접 판정 엔티티 잔류 없음 | 미검증 |
| UAFLayering | 런타임 레이어 전환·본 마스크·무기별 AO/그립 IK와 슬롯 순서 동등. Phase 3 표시 포즈·공격 중 물리 억제 계약 유지 | 미검증 |
| Motion Warping·Mover | 루트모션 공격의 이동/회전/중단이 기존 어댑터와 일치, 중복 적용 없음 | 미검증 |
| 프록시·재시뮬레이션 | 리슨 2인 호스트/게스트 및 `ActorPromoted`에 컨트롤러 없는 경로·게스트 재시뮬레이션 확인 | 미검증 |
| 디버깅 | 선택 DB/클립·그래프 실행을 기록하고 필요한 시점으로 돌아가 확인 가능 | 부분 확인 — 최소 MM 구성의 UAF 시스템/그래프·클립 트랙, Active Pose·과거 구간 재조회 확인. 전체 계약은 5-2에서 판정 |
| 구면·최종 포즈 | 적도 이동/회전·정지·경사·착지·움직이는 base에서 루트 Up·발 배치 유지. 공중 `Fall_Loop` 고정(AN-007) | 부분 확인 — 북극·적도·패널 병진의 최종 Tilt/Yaw·거리 제한과 공중 고정 확인. 큰 각도 회전 base·발 배치 등은 미검증 |

**현재 Go/No-Go는 보류.** 소스 조사만으로 체크하지 않는다. 한 항목이라도 동등하게 구현할 수 없으면 AN-006에 따라 No-Go와 사유를 `.context/DiscardedApproaches.md`에 남긴다. 통과해도 Phase 6 착수는 사용자 확인 뒤 별도 단위다.

**5-1에서 확인한 5-0 조사 한계:** 기본 `Evaluate Chooser`의 UAF 경로는 `UAFAssetInstance` 구조체를 컨텍스트 0번에 먼저 넣고 객체를 나중에 넣는다. 기존 테이블의 0번은 `LNPAnimInstance`여서 직접 연결 시 반복 오류가 났다. 단순히 `AnimContext`를 채우면 해결되는 문제가 아니다. 기존 테이블을 보존하고 `UChooserFunctionLibrary::EvaluateChooserMulti`로 같은 객체 컨텍스트에서 평가한 배열을 전달하는 시험으로 바꿨다.

## 5. 다음 행동

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
