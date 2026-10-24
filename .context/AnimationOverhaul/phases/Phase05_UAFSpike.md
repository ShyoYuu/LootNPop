# Phase 5 — UAF 판정 스파이크

> 상태: 5-0 완료, 5-1 진행 중(구면 보정 구현·연결 후 급전환 방향 추종 실패 확인, Steering 보완 단위 승인 대기)
> 마지막 갱신: 2026-10-24
> 전제: Phase 2·3 완료. AN-006에 따라 판정 전 마이그레이션하지 않는다.

## 1. 목표와 범위

무기 없는 플레이어 1명을 Mover UAF와 UAF Motion Matching으로 구동하고 `Roadmap.md` Phase 5의 일곱 항목을 판정한다. 무기 레이어·몽타주 계약은 별도 시험 입력으로 확인한다. 기존 `ABP_Main_UEFN`·무기 서브 ABP·표시 ABP·Chooser·PSD는 수정하지 않는다. 시험 구성은 `/Game/Characters/UAFSpike/`에 둔다. GASP는 읽기 전용이다(AN-009).

**각 단위 종료 후 사용자에게 계속 진행할지 확인한다.** 에디터 조작·PIE·그래프 배선이 사용자 수동 조작으로 더 효율적이면 즉시 요청한다. 자동화 도구를 새로 만드는 것으로 우회하지 않는다.

## 2. 단위와 완료 조건

| 단위 | 작업 | 검증·종료 조건 | 상태 |
|:---|:---|:---|:---|
| 5-0 | GASP 참조 유무·로컬 엔진 API 조사, 실행서 작성 | 참조 조회 결과·재사용 경로·차단 후보·미검증 항목 기록. 코드·에셋 변경 없음 | 완료(2026-10-23) |
| 5-1 | 최소 테스트 폰·UAF 그래프, 기존 PSD·Chooser·Mover 궤적 연결 | 같은 Chooser가 반환하는 PSD 목록과 실제 UAF 검색 결과 확인. 북극·적도 이동/정지/회전·공중 고정·발 배치·움직이는 base 확인. 동등 구현 경로가 없으면 해당 사유로 조기 No-Go 판정 | 진행 중(기본 이동·UAF 달리기/Idle 검색 확인, 구면 보정 구현·연결 완료 후 런타임 검증 중) |
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
| PSD·Chooser | 기존 에셋을 변경하지 않고 같은 상태 입력의 PSD 배열로 UAF가 검색. Starts/Stops/Turns 등 전환과 인터럽트 확인 | 부분 확인 — UAF 달리기·Idle 실제 DB/클립 확인. 입력 배열 시점별 대조·전환/인터럽트 미검증 |
| 몽타주·ANS | 슬롯 3종과 그룹 계약 유지. ANS 5종의 정상 종료·`Montage_Stop`(즉시/블렌드아웃)·교체 시 End 정확히 한 번, 태그/근접 판정 엔티티 잔류 없음 | 미검증 |
| UAFLayering | 런타임 레이어 전환·본 마스크·무기별 AO/그립 IK와 슬롯 순서 동등. Phase 3 표시 포즈·공격 중 물리 억제 계약 유지 | 미검증 |
| Motion Warping·Mover | 루트모션 공격의 이동/회전/중단이 기존 어댑터와 일치, 중복 적용 없음 | 미검증 |
| 프록시·재시뮬레이션 | 리슨 2인 호스트/게스트 및 `ActorPromoted`에 컨트롤러 없는 경로·게스트 재시뮬레이션 확인 | 미검증 |
| 디버깅 | 선택 DB/클립·그래프 실행을 기록하고 필요한 시점으로 돌아가 확인 가능 | 부분 확인 — 최소 MM 구성의 UAF 시스템/그래프·클립 트랙, Active Pose·과거 구간 재조회 확인. 전체 계약은 5-2에서 판정 |
| 구면·최종 포즈 | 적도 이동/회전·정지·경사·착지·움직이는 base에서 루트 Up·발 배치 유지. 공중 `Fall_Loop` 고정(AN-007) | 미통과 — Offset Root 적용 시 급전환 방향 추종 실패. 적도·발 배치·공중 고정·base 미검증 |

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

**조사·구현/그래프 연결 단위 완료. 기본 UAF trait 핀 설정만으로는 기존 보정과 동등하지 않다.** 사용자 승인 후 아래 보정 trait를 구현했고 C++ 컴파일·링크, 에디터 등록·그래프 연결/컴파일을 확인했다. 첫 런타임에서 급전환 방향 추종 문제가 재현돼 §7의 Steering 보완 단위 승인 대기로 인계한다. 현재 No-Go로 확정하지도, 구면 게이트를 통과 처리하지도 않는다.

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

**아직 구현 승인 전이다.** 사용자가 세션 인계·커밋을 요청했으며 Steering 보완 코드는 쓰지 않았다. 새 세션에서는 진행 확인부터 받고 아래를 이어간다.

1. 현재 재현 기준은 이동 중 룩 급회전 뒤 전방 달리기 포즈가 옆/대각선으로 달리는 것이다. Offset Root `1`에서 쉽게 재현·`0`에서 미재현, 마지막 값 `1`, PIE 종료 상태다. 이 비교는 Offset Root 적용 경로와의 연관성을 보여 주며 Steering 누락이 유일한 원인이라는 증명은 아니다.
2. 기존 계약은 `design/GASPReference.md` §2.10의 블렌드 스택 Steering×2·OrientationWarping·ResetRoot·CopyBone 후 OffsetRoot다. 현재 UAF는 최소 MM/Pose History → Offset Root+프로젝트 구면 보정 → Output으로 Steering·OrientationWarping·ResetRoot/CopyBone·발 배치가 빠져 있다. 이번 문제는 우선 Steering/회전 추종 범위에서 조사하고 나머지 누락을 검증 완료로 간주하지 않는다.
3. 설치 엔진 `Plugins/Experimental/UAF/UAFWarping/Source/UAFWarping/Private/SteeringTrait.h/.cpp`: additive Steering은 RootBoneTransform·TargetOrientation·Alpha, 미래 루트모션 추출용 IAttributeProvider·ITimeline을 이용해 회전 루트모션을 Override한다. ProceduralTargetTime·AnimatedTargetTime 등은 shared data 상수이고 latent 목록은 Alpha·TargetOrientation·RootBoneTransform뿐이다. 기존 ABP의 곡선 기반 목표 시간(0.1~1.5초·0.1~0.3초), 이동 Steering의 AnimatedTargetTime=10, TurnInPlace 별도 Steering과 같다고 가정하지 않는다. `UAFPoseSearch/Source/UAFPoseSearch/Private/MotionMatchingTrait.h`와 `UAFAnimGraph/Source/UAFAnimGraph/Internal/Traits/BlendStackTrait.h`에서 UAF의 애니별 보정 위치·인터페이스 전달을 확인한다.
4. UAF 자신의 이전 누적 루트와 현재 UAF 선택 애니·곡선을 기준으로 연결한다. 숨김 ABP는 Chooser 상태와 궤적 공급자로 유지되지만 그 ABP의 가상 루트·선택 DB 태그는 UAF 결과와 달라질 수 있다. `GetSteeringTargetFacing`의 컴포넌트 Up 정렬 규칙을 유지한다. 실제 애니 평가 위치·task 순서와 root motion 속성의 유효성을 확인하고 필요한 최소 변경을 정한다.
5. 구현·빌드·AG/S 컴파일 후 동일 급전환을 수동 비교하고, 해결되면 북극/적도·공중·base 시험을 재개한다. 실제 UAF root의 기울기·Yaw/위치 제한·base 중복을 확인하며 숨김 ABP의 `[AnimB3]`만으로 통과시키지 않는다. 단위마다 진행 확인, GUI/PIE가 빠르면 즉시 수동 요청한다.

인계용 로컬 자료: 재현 녹화 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261024_145134378.utrace`(171,067,599바이트), UAF 검색 비교 `RewindDebugger-LootNPop-Editor-20261023_192247450.utrace`, 구면 스택 전 AG/S 백업 `Saved/AnimationOverhaul/2026-10-23_PreSphericalOffsetRoot_dadd230e/`. Saved는 git 제외라 이 커밋에는 포함되지 않는다. Rewind는 프로젝트 Saved 경로만 사용한다. 1인 기록은 Net Mode=`Play Standalone`, 별도 프로세스 Standalone Game이 아니다.

`TestMap03`은 기존 사용자 변경과 시험용 GameMode Override가 섞여 있어 이번 커밋에서 제외하고 로컬에 유지한다. 새 체크아웃에서는 `BP_GameModeUAFSpike`를 임시 지정해야 한다. 5-1 종료 시 원래 Override를 확인해 복원하되 맵 전체를 되돌리지 않는다. DT_PlayerBaseStats·DT_LongSword_Levels·DA_MassSpawnConfig_TestOnePerSpecies도 이번 작업 밖의 로컬 변경으로 남긴다.

검증: 최신 C++ 빌드 성공(5 actions, 48.13초), AG/S 컴파일·배선 확인과 사용자 CVar 비교까지다. 새 Steering 코드·빌드·급전환 해결은 아직 없다. 구면·적·2인 최종 회귀·5-1 완료·5-2·Go 판정도 남아 있다.
