# Phase 5 수행 기록

## 발 배치 Control Rig UAF 연결 착수(2026-10-26)

- 사용자 Phase 5 재개 요청으로 §14 다음 발 배치 연결 단위를 시작했다. 기존 구면 보정 CR의 공개 변수와 엔진 Control Rig/Passthrough Blend의 포즈·커브·변수 전달, Bool 알파 0 평가 생략 경로를 좁혀 확인했다. 기존 CR은 수정 없이 참조하며 새 분기·trait를 만들지 않는다.
- PIE 수동 종료와 MCP false 확인, AG/S만 저장 성공 후 `Saved/AnimationOverhaul/2026-10-26_PreFootPlacement/`에 백업했다. 기존 사용자 맵/데이터 4개는 보존했다.
- 스파이크 공급층에 접지 법선·활성·착지/재활성 리셋·핀 고정·경사·디버그 입력을 추가하고 RigVM 타입도 등록했다(C++ 3파일). 기존 base 이동·텔레포트 입력은 재사용한다. 상세 연결표는 Phase05 §15가 소유한다.
- 사용자 에디터 종료 후 권한 확장 전체 Development 빌드 **Succeeded, 8 actions, 43.72초, 종료 코드 0**. Live Coding은 사용하지 않았다. 현재 사용자 에디터 재실행 대기이며 수동 배선·AG/S 컴파일·실행/회귀는 남았다. 5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### v2 UI 누락 대응·핀 생성·배선 확인(2026-10-26)

- 새 Control Rig(v2)의 변수 노출 UI가 없는데 Input 범주가 있다고 선행 안내한 것을 정정했다. 로컬 엔진은 Legacy 타입에만 커스텀 UI를 등록한다. Legacy 전환으로 Input/Use Pin 표시와 AG 컴파일 성공을 확인했지만 사용자 우려로 최종 채택하지 않았다.
- 기존 엔진 Export/Import가 programmatic pin을 재생성하는 경로로 v2 `AnimNextTraitStack_3`에 변수10개를 생성했다. DoRaycast=true/UseIKBoneTargets=false·CR 참조·Bool 알파0.3/0.2·AG 컴파일 성공을 확인했고 임시 Legacy 스택은 제거했다. 포즈 Input 레이아웃은 별도 명령으로 복구했다. 엔진·추가 C++·MCP 도구 변경은 없다. 일회성 스크립트와 각 단계 백업은 `Saved/AnimationOverhaul/2026-10-26_PreFootPlacement/`에 있다.
- 사용자 공유 Getter/포즈 배선 후 AG/S Compile·Save 성공을 확인했다. 19:37:49 KST 실제 링크11개(입력9·포즈2)를 읽어 표대로임을 확인했다. 겹친 선 때문에 의심했던 Has Teleported도 OffsetRoot.bReset에서 정상 공급됐다. 첫 경로 조회 함수 안내는 Python 미노출 함수라 정정했다. 상세 경로·근거·명령 한계는 Phase05 §15가 소유한다.
- 현재 북극 약20초의 첫 1인 PIE 수동 검증 요청 상태다. 입력 전달·실제 CR task·발/골반 포즈·적도/경사/움직이는 base 검증 전이며 단위 미완료·Go/No-Go 보류다.

### 북극 첫 관찰·입력 전달 확인(2026-10-26)

- 사용자 북극 이동/제자리 회전/점프·착지에서 이상 없음 보고, PIE 활성 확인. 최신 세 번째 PIE의 입력 ready1·unbound0, LogAnimation Error0·검색한 CR/RigVM/Assertion/Ensure 오류0을 확인했다. 기존 Foley 경고179회는 별도다. 고정 로그 `NorthSmoke.log`, 범위·진단 한계는 Phase05 §15가 소유한다.
- DebugLocomotion=0이어서 수치 검증이나 실제 CR task 증거로 쓰지 않는다. 다음으로 진단2+Rewind 녹화, 경사/턱의 발 배치0→1·적도 이동/회전/점프·움직이는 발판 검증을 요청했다. 녹화 종료 뒤 PIE 유지 요청 상태다. 이번 단위 미완료·다음 단위 착수 전 진행 확인을 유지한다.

## 5-0 — 참조 확인·소스 대조·실행서(2026-10-23)

- 사용자 요청으로 Phase 5를 선택했다. 각 구현 단위 종료 후 계속 진행할지 확인하고, 수동 조작이 더 효율적이면 즉시 요청한다.
- GASP 파일명·`.uproject` 확인 뒤, 현재 세션에 GASP 전용 MCP가 없어 사용자에게 에디터 Python 레지스트리 조회를 요청했다. `/Game`에서 클래스 경로에 UAF/AnimNext가 포함되는 에셋 결과는 사용자 보고 `LogPython: []`. 조회 범위의 한계와 명령은 실행서 §3.
- 로컬 UE 5.8 소스에서 기존 Chooser의 객체 컨텍스트·배열 평가, PSD 배열 입력, 외부 궤적·포즈 이력, 몽타주용 기존 AnimInstance와 ANS 디스패처, UAFLayering·Mover 루트모션·Rewind 구현을 확인했다. API 존재와 실행 통과를 구분했다.
- 구면 Offset Root 우회는 현재 ABP 노드 내부 상태에 의존해 UAF trait에 그대로 적용되지 않는다. 몽타주 컴포넌트의 첫 SkeletalMeshComponent 선택과 몽타주 전용 초기화도 LNP 구조에서 확인해야 한다.
- `phases/Phase05_UAFSpike.md`를 작성했다. 5-0 완료, 5-1 최소 구면 로코모션, 5-2 계약·네트워크 판정으로 나눴다. 최종 게이트 일곱 항목은 모두 미검증이며 Go/No-Go 보류다.
- 코드·플러그인 설정·에셋 변경, GASP 저장·수정, 빌드·PIE는 하지 않았다. 시작 시 사용자 수정 에셋 `DT_PlayerBaseStats`, `DT_LongSword_Levels`, `DA_MassSpawnConfig_TestOnePerSpecies` 3개는 건드리지 않았다.
- **여기서 멈추고 사용자 진행 확인을 받는다.** 다음 단위는 5-1이며 확인 전 플러그인 활성화나 테스트 에셋 생성에 착수하지 않는다.

## 5-1 — 최소 테스트 구성(2026-10-23, 진행 중)

- 사용자에게 5-1 진행 승인을 받았다. GASP 참조 조회는 끝나 에디터를 닫아도 된다고 안내했다.
- LootNPop MCP의 활성 목록에는 `UAF`·`UAFAnimGraph`·`UAFMirroring`가 있었다. 필요한 `UAFChooser`·`UAFPoseSearch`·`UAFControlRig`·`UAFWarping`·`MoverAnimNext` 5개는 없었다.
- MCP `SetPluginEnabled` 5건 뒤 프로젝트 파일이 바뀌지 않아 엔진 툴 구현을 확인했다. `IProjectManager::SetPluginEnabled`만 호출하고 `SaveCurrentProjectToDisk`를 호출하지 않는다. `LootNPop.uproject`에 해당 5개를 직접 명시했으며 JSON 파싱·`git diff --check` 통과. GASP 프로젝트 파일은 바뀌지 않았다.
- 사용자에게 미저장 작업 저장 후 LootNPop 재시작을 요청했다. 재시작 후 실제 플러그인 로드를 확인한다. 기존 애니메이션 에셋·C++는 아직 변경하지 않았다. 5-1 완료나 게이트 통과로 기록하지 않는다.
- 사용자 재시작 후 5개 모두 MCP `IsEnabled=true`를 확인했다. 현재 MCP에 UAF 그래프 편집 도구가 없어 `/Game/Characters/UAFSpike/AG_LocomotionSpike`를 `UAF Animation Graph` 타입으로 생성해 열어 달라고 요청했다. 다음은 실제 편집 화면 확인이다.
- 사용자 수동 그래프 구성·미리보기: MM 템플릿 → 출력, 기존 Idle PSD·UEFN 메시 지정. 앉았다 일어나는 반복과 스키마 필요 본 누락 경고를 확인했다. Pose History 수집 본 4개(`foot_l`·`foot_r`·`pelvis`·`spine_05`)를 넣자 자연스러운 Idle로 안정됐고 최신 로그에서도 본 누락 경고가 멈췄다. 컴파일 성공, Foley 이름 이벤트 미처리 경고는 잔류. 전체 PSD·Chooser·구면 게이트 통과는 아직 아니다.
- MCP로 기존 플레이어 사본 `BP_PlayerUAFSpike`와 엔진 단일 그래프 시스템 템플릿 사본 `S_LocomotionSpike`를 새 폴더에 생성·지정 저장했다. 원본은 수정하지 않았다. 첫 이동 시험은 숨김 ABP 공급층 + UAF 표시 메시의 중간 구성으로 하며 성능 판정에서 제외한다. 사용자에게 AG의 기존 Chooser 연결·컨텍스트/궤적 변수 배선을 요청했다.
- 배선 후 `ContextData entry 0 expects ... LNPAnimInstance ... UAFAssetInstance` 오류를 확인했다. 원인은 UAF 기본 디스패치의 구조체 우선 컨텍스트 순서다. 5-0에서 객체·배열 API 존재만 확인하고 순서 차이를 놓쳤다고 사용자에게 알렸다. 사용자는 Chooser 결과 연결을 해제·컴파일·저장했다.
- 테스트 전용 `ULNPUAFSpikeAnimInstance`·`FLNPUAFSpikeInputs`를 추가했다. 숨김 ABP의 공급층 갱신 뒤 기존 Chooser를 같은 객체 컨텍스트로 평가해 PSD 배열·궤적·인터럽트·블렌드를 UAF에 전달한다. 시스템 `Graph` 지정과 숨김 메시 → UAF 틱 의존성도 포함한다. 기본 클래스는 수정하지 않았다.
- 사용자 에디터 종료 후 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 권한 확장 전체 빌드 성공(종료 코드 0, 12 actions, 59.59초). 빌드 통과와 런타임 성공을 구분한다. 사용자에게 에디터 재실행을 요청했다. 다음은 테스트 메인 ABP 사본·클래스 적용, 시스템·그래프의 동일 Native Shared Variables 연결이다.
- 재실행 후 `ABP_SourceUAFSpike` 부모 변경·설정, 테스트 플레이어의 숨김 ABP·UEFN 표시 메시·UAF 컴포넌트 출력 대상, 테스트 GameMode를 구성했다. 사본 컴파일·지정 저장과 주요 값 재조회 통과. 원본 ABP·플레이어·GameMode는 변경하지 않았다.
- 사용자가 S와 AG에 같은 Native Shared Variables를 추가하고 공유 DB·궤적 배선을 완료했다. 인터럽트·블렌드 핀은 아직 연결 전. 먼저 전달 경로를 확인하려고 임시 GameMode Override(원래 값 메모·시험 뒤 복원)로 1인 PIE·3초 대기·포즈 보고를 요청했다. 현재 PIE 실행 결과 대기이며 단위 완료가 아니다.
- 첫 PIE: TestMap03 런타임 지형·스폰 정상, 표시 UEFN T 포즈(사용자 보고). `[UAFSpike]` 로그는 없고 `AnimSourceMesh`의 `bEnableAnimation is true` 쓰기 거부 경고가 반복됐다. 라이브 컴포넌트·ABP 설정 재조회에서 테스트 클래스·입력 에셋·Output Component=`VisualMesh`는 유지됐다.
- 시스템 화면에서 Initialize·Write Pose가 `SkeletalMesh` 변수의 명시적 입력을 쓰는 것을 확인했다. 기본 locator `Current Actor → Get SkeletalMeshComponent`는 클래스의 첫 컴포넌트를 선택하므로 Output Component 설정과 별개다. 사용자에게 기존 locator를 Property로 교체하고 `LNPCharacterBase.VisualMesh` 선택·컴파일·저장·재PIE를 요청했다. 수정·재시험과 입력 로그 누락 원인 확인은 진행 중이며 5-1 완료가 아니다.
- 사용자 Property locator 변경 후 재PIE에서도 T 포즈·AnimSourceMesh 쓰기 거부가 유지됐다. 읽기 전용 Python으로 실제 AnimSourceMesh의 인스턴스가 `ABP_SourceUAFSpike_C`이고 VisualMesh 인스턴스는 None임을 확인했다. 첫 경로 조회 실패는 실행된 명령에서 `:PersistentLevel`이 빠졌기 때문이며, 게임 월드의 PlayerPawn 조회로 대체했다.
- 실제 AnimInstance를 MCP로 조회하니 `SpikeSystem=None`, `SpikeGraph=None`였다. 소유 캐릭터·Mover·Chooser와 생성된 궤적은 유효해 기존 공급층은 실행 중이다. ABP의 생성 클래스 CDO에서는 두 참조가 지정돼 있어 기본값 조회만으로 런타임 반영을 입증하지 못했다. 입력 코드의 두 참조 검사에서 반환되는 상태다. PIE 인스턴스의 두 필드를 MCP로 지정하는 시도는 기본값 전용 속성 쓰기 거부로 실패했고 변경되지 않았다.
- 사용자에게 PIE 중지 후 ABP Class Defaults의 두 참조를 한 번 비웠다가 재지정·컴파일·저장·재PIE하도록 요청했다. 아직 재시험 전이며, CDO와 런타임 값 차이의 근본 원인은 미확정이다.
- 재지정 후 실제 인스턴스 두 참조가 유효해졌고 `05:19:49 [UAFSpike] inputs=ready databases=1 trajectorySamples=31`을 확인했다. 입력 전달 성공이나 사용자 관찰은 여전히 T 포즈이며 AnimSourceMesh 쓰기 거부가 반복된다. S의 SkeletalMesh 변수 locator 메뉴를 다시 펼쳐 저장된 경로를 확인하도록 요청했다. 실제 UAF 검색·포즈 출력은 아직 미검증이다.
- 메뉴 화면에서 실제 locator가 `Current Actor → Get SkeletalMeshComponent → VisualMesh`임을 확인했다. 중간 Component fragment만 제거하도록 요청했고, 사용자가 `Current Actor → VisualMesh`로 수정·컴파일·저장·재PIE한 뒤 Idle 포즈가 나왔다고 보고했다. 05:25:28 입력 ready(PSD 1개·궤적 31개)·런타임 참조 유효, 이후 최신 로그에서 메시 쓰기 거부 중단 확인. 전달·Idle 출력 경로를 확인했으며 이동·실제 DB/클립 추적·구면은 아직 아니다.
- 같은 5-1 범위에서 AG의 MM 입력 `NextUpdateInterruptMode`·`BlendArguments` 노출과 공유 Getter 연결을 사용자에게 요청했다. 완료 후 이동 전환을 수동 검증한다. 5-2에는 착수하지 않았다.
- 기존 범주 위치 안내가 불명확해 실제 메뉴 화면을 확인한 뒤 `Add Category`로 새 그룹을 만들어 두 핀을 배치하도록 정정했다. 사용자가 그룹 이름은 기본 `Category`로 유지하고 연결 완료. 06:21 저장 로그·컴파일 성공 아이콘·네 공유 입력 배선을 화면에서 확인했다. 범주 이름은 실행에 영향이 없다.
- 사용자에게 최소 그래프의 1인 이동 전환(전진·정지, 질주·큰 방향 전환, 제자리 카메라 회전 후 이동, 점프·착지) 수동 시험을 요청했다. 포즈 변화와 캡슐 이탈·메시 튐을 구분해 보고받으며, 현재 결과 대기다.
- 사용자 보고: 이동·방향 전환·점프·착지 등에서 어색한 부분을 느끼지 못했다. 기본 전환의 사용자 관찰은 통과로 기록한다. 06:49:58 입력 ready를 확인했다. 다만 해당 시험에서 `PSS_Relaxed_Loops_UEFN`의 `Phase` 커브가 입력 Pose History에 없다는 경고가 1,022회 발생해, 사용자에게 Collected Curves에 `Phase` 추가·저장·재시험을 요청했다. 루트 보정·발 배치·실제 선택 DB/클립 추적·구면 통과로 확대하지 않는다.
- 사용자가 `Phase` 수집·저장 후 재시험에서도 어색한 부분이 없었다고 보고했다. 07:38:11 입력 ready, 이후 Phase 누락 경고 중단을 확인했다. FoleyEvent Walk/Run 처리기 없음 경고는 계속돼 경고 전체 0으로 기록하지 않는다.
- 실제 UAF 선택 DB/클립을 확인하려고 사용자에게 Tools → Debug → Rewind Debugger에서 짧은 이동 기록 후 PIE 일시정지·창 표시를 요청했다. 숨김 ABP의 검색 결과와 구분해야 하며 아직 기록·추적 결과 대기다. 루트·발 배치 구성과 구면 게이트는 미검증이다.
- Rewind 화면은 Local Editor 녹화·데이터 수신 중이었으나 대상 트랙이 빈 상태였다. 녹화 종료 후에도 빈 트랙이 유지돼 기록 중 Trace.Status 조회를 요청했다. 사용자가 그 전에 Trace가 OneDrive로 업로드된다고 알려 저장 위치 해결을 우선했다. 현재 녹화 종료를 화면에서 확인했고 OneDrive 밖 경로를 확인하기 전 재녹화하지 않는다.
- 설치 소스 확인: RewindDebuggerModule.cpp는 TraceSessionsManager에 하위 폴더 RewindDebugger를 넘긴다. TraceSessionsManager.cpp의 기본 경로는 FPlatformProcess::UserDir() + 하위 폴더이고, WindowsPlatformProcess.cpp의 UserDir은 FOLDERID_Documents다. 따라서 문서 폴더가 OneDrive로 연결된 현재 환경에서 업로드된다. Rewind 사용자 설정에 저장 경로 필드는 없고 SetSavePathOverride/SetSaveRecordingsToDisk 호출도 연결돼 있지 않다(ChaosVD는 별도 연결). 기존 업로드 Trace의 이동·삭제는 하지 않았다.
- 사용자는 기존 Trace를 프로젝트 Saved/RewindDebugger로 직접 옮겼다. 엔진 소스 수정 없이 경로 변경이 가능한지 질문했고, Rewind 외 기능에 영향이 있어도 되며 Windows 문서에 저장할 내용이 없다고 명시했다. Windows 문서 경로는 실제 C:/Users/arkep/OneDrive/문서로 확인됐다. 이 경로 조회는 FPaths::CustomUserDirArgument를 쓰지 않으므로 -UserDir만으로 Rewind를 변경할 수 없다.
- 프로젝트 LootNPopEditor에서 Rewind 모듈의 세션 관리자에 SetSavePathOverride를 적용하는 구현 경로를 확인했다. IRewindDebuggerModule 공개 인터페이스에는 관리자 접근자가 없고, RewindDebuggerModule.h 내부 헤더의 인라인 GetTraceSessionsManager 접근자가 필요하다. 엔진 파일 수정·엔진 재빌드 없이 프로젝트 코드로 연결할 수 있으나 내부 헤더 의존성이 생긴다. 현재 조사만 수행했고 경로 변경 코드는 아직 작성하지 않았다. 새 녹화 보류 유지.
- 사용자 에디터 종료·수정 요청 후 `LootNPopEditorModule.cpp`에 엔진 초기화 완료 콜백으로 SavePathOverride를 Saved/RewindDebugger 절대 경로로 지정했다. 경로 지정이 폴더 생성보다 먼저여서 생성 실패에도 Windows 문서로 대체되지 않는다. 시작 로그 `LogLNPRewind: Recording directory:`를 남긴다. Build.cs에는 필요한 에디터 의존성과 UE 5.8 Rewind 내부 헤더 경로를, .uproject에는 GameplayInsights 활성 의존성을 추가했다. 엔진 소스 파일 변경 없음.
- 첫 Development 빌드 컴파일·링크 성공 후 플러그인 선언·구형 OnPostEngineInit 경고를 수정했다. 최종 빌드: Succeeded, 종료 코드 0, 4 actions, 8.61초, 새 경고 없음. 프로젝트 JSON 파싱·git diff --check 통과, Saved/RewindDebugger가 git 제외임을 확인했다. 사용자가 옮긴 Trace 3개는 건드리지 않았다.
- 사용자에게 에디터 재실행 후 로딩 완료만 알려 달라고 요청했다. 시작 로그 경로 확인 → 새 Trace 실제 저장 위치 검증이 남았으며, 그 전 PIE·녹화는 보류한다. 경로 보완 검증 후 Phase 5 추적 시험을 계속할지 사용자 확인을 받는다.
- 재실행 시작 로그에서 `LogLNPRewind: Recording directory: D:/UnrealProjects/LootNPop/Saved/RewindDebugger` 확인 후 사용자 짧은 녹화를 요청했다. 새 `RewindDebugger-LootNPop-Editor-20261023_185015117.utrace` 96,962,306바이트가 프로젝트 폴더에 생성됐고, 기존 Trace 3개 크기는 유지됐다. Windows 문서 쪽 RewindDebugger 폴더가 재생성되지 않은 것도 확인했다. 녹화 연결·종료 로그 확인. **저장 경로 보완 단위 완료**, 빈 트랙 진단부터 5-1을 계속할지 사용자 확인 단계다. 실제 검색 결과·최종 게이트 통과와 구분한다.
- 사용자가 문서 갱신 여부와 새 세션 인계를 문의했다. Current.md의 오래된 첫 PIE 대기 문구를 최신 상태로 정리하고 새 세션 첫 행동을 명시했다. Trace.Status는 아직 수행하지 않았으며, 저장 경로 보완 단위 완료 이후 실제 5-1 후속 작업은 재개하지 않았다. 새 세션에서 계속 진행 승인 후 빈 트랙 진단부터 이어간다.

### Rewind 빈 트랙 진단 단위 완료(2026-10-23)

- 사용자 승인으로 빈 트랙 진단부터 재개했다. 에디터 조회에서 PIE 종료·플레이어 이름 선택·`Start a recording to debug` 표시를 확인했다. 새 도구 구현 없이 사용자 수동 PIE·녹화·`Trace.Status` 실행을 요청했다.
- 19:19 KST 녹화의 `Trace.Status`: Object·Frame·Animation·PoseSearch·UAF 채널 모두 활성. 로그와 PIE 설정에서 플레이어 1명이지만 Net Mode는 Listen Server임을 확인했다. 따라서 단순 채널 누락이나 저장 위치 문제로 판단하지 않았다.
- 설치 UE 5.8 `Engine/Plugins/Animation/GameplayInsights/Source/GameplayInsights/Private/GameplayProvider.cpp:782`의 `AppendRecordingInfo`를 확인했다. 녹화 시간축 허용 조건은 Standalone·Client·Server 타깃 DedicatedServer이며 ListenServer는 제외된다. `RewindDebugger.cpp`는 이 시간축으로 스크럽 시점·대상 객체 조회 범위를 계산하므로 리슨 호스트만 있는 녹화에서 시간축·대상 트랙이 비는 경로다.
- 사용자에게 현재 PIE 종료 → Net Mode=`Play Standalone` → 짧은 이동 녹화 → 녹화 종료·PIE 일시정지를 요청했다. 19:22:37 KST 로그에서 실제 Standalone, 19:22:38 입력 ready(PSD 1개·궤적 31개), 19:22:47 녹화 연결·19:23:00 종료를 확인했다. 사용자도 Standalone 변경 후 그래프들이 생겼다고 보고했다.
- Slate 조회에서 실제 플레이어 트랙과 `AnimSourceMesh → ABP_SourceUAFSpike → Pose Search`, DB·Anim Graph·Blend Weights 하위 트랙 생성을 확인했다. **빈 트랙 원인 확인·Standalone 비교 검증 완료.** 최신 파일 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261023_192247450.utrace`는 35,178,644바이트다. 엔진·프로젝트 코드·에셋 변경이나 빌드는 하지 않았다.
- 현재 보인 Pose Search는 숨김 ABP 소유다. 실제 UAF 검색 DB/클립·시점 추적이나 최종 디버깅 게이트 통과로 일반화하지 않는다. 다음 단위는 기존 Standalone 녹화에서 UAF 소유 트랙을 구분해 실제 검색 결과를 확인하는 것이며, 진행 전 사용자 확인을 받는다. 5-1 진행 중·5-2 미착수. PIE는 사용자 일시정지 상태, PIE Net Mode는 Standalone으로 변경됐다. TestMap03 GameMode Override는 5-1 시험용 임시 상태를 유지하며 5-1 종료 시 원래 값으로 복원한다.

### 실제 UAF DB/클립 추적 단위 완료(2026-10-23)

- 사용자 계속 진행 승인 후 기존 Standalone 녹화를 그대로 조사했다. PIE 실행·일시정지 상태이며 재녹화하지 않았다. Slate로 숨김 ABP 트리를 접어 `UAFLocomotion` 소유의 `S_LocomotionSpike → UAFAnimGraph_2 → AG_LocomotionSpike`, 재생 클립·EvaluationProgram·Pose Search 트랙을 확인했다. `AnimSourceMesh` 아래 숨김 ABP의 검색 트랙과 구분했다.
- UAF Pose Search에는 Idle·Run F/B/LL/RL·Jump·Land·Turn·Starts/Stops 혼합 DB 묶음의 검색 기록이 있었다. 트랙 존재를 각 전환·인터럽트 통과로 확대하지 않았다.
- 사용자에게 UAF Pose Search 선택 후 약 4초 이동 구간 스크럽을 요청했다. 사용자는 기존 Details 위치에 Rewind Debugger Details·Active Pose가 나타났다고 보고했다. 이 패널이 정상적인 결과 화면이며 별도 디버거 창은 필요 없다고 안내했다.
- 첫 이동 표본: UAF Active Pose의 Database=`PSD_Stand_Run_RL_Loops_UEFN`, Asset=`M_Relaxed_Run_Switch_RR_RL_Rfoot`, 표 Time 약 0.60초, Motion Matching State의 Asset Player Time=0.595301초. Continuing Pose도 같은 DB·클립이었다.
- 사용자 약 12초 정지 구간 스크럽 후 Active Pose가 Database=`PSD_Stand_Idles_UEFN`, Asset=`M_Relaxed_Stand_Idle_Loop`, 표 Time 약 0.43초로 바뀌었으며 Sim/Anim Linear Velocity는 0이었다.
- 사용자 약 4초 이동 구간 재방문 후 같은 Run RL DB·클립을 다시 읽었다. 재방문 표본의 Time은 약 0.47초·비용 1.126으로 첫 표본과 다른 프레임이다. 같은 프레임의 비용/포즈 시간 일치를 검증했다고 주장하지 않는다. 과거 구간으로 돌아가 해당 시점의 UAF 결과를 읽을 수 있음을 확인했다.
- **이번 단위 완료:** 실제 UAF DB·클립·시간 추적과 이동/Idle 구분·과거 구간 재조회 확인. 입력 PSD 배열 시점별 대조·Starts/Stops/Turns 및 인터럽트는 미검증이다. 최소 그래프의 디버깅 근거로 기록하며 최종 일곱 게이트 전체 통과나 5-1 완료로 일반화하지 않는다.
- 코드·에셋 변경·빌드·새 녹화 없음. Current·Phase05 실행서·본 로그만 갱신했다. PIE 일시정지·Standalone·테스트 GameMode 유지. 다음은 구면 Offset Root 보정의 동등 구현 가능 여부를 확인하는 단위이며 사용자 승인 전 착수하지 않는다. 5-2 미착수.

### 구면 루트 보정 동등 구현 경로 조사 완료(2026-10-23)

- 사용자 진행 승인 후 기존 `ULNPAnimInstance::NativeThreadSafeUpdateAnimation`과 설치 UE 5.8의 UAFWarping OffsetRootBone trait/task, UAFAnimNode의 OffsetRootBone node/AnimOp, trait binding·평가 순서를 읽기 전용으로 대조했다. MCP에서 PIE가 유지 중인 것을 확인했다. 코드·에셋·PIE 상태 변경 없이 조사했다.
- 기본 UAF에도 월드 Z 기본 법선·이동 속도 제한의 `GetSafeNormal2D`·Accumulate/Interpolate에서 컴포넌트 회전을 따라가지 않는 누적 상태·로컬 회전 제한의 잘못된 곱셈 순서가 남아 있다. 법선/모드 핀만 연결해서 기존 구면 보정과 같아지지 않는다.
- 기존 계약은 Accumulate/Interpolate에서만 base 변위 추가 → 누적 루트 Up 스윙 정렬 → 로컬 Z Yaw ±90° 제한이다. Release에서는 엔진이 컴포넌트 변위를 반영하므로 base를 다시 넣지 않는다. 기존 30cm 위치 제한·속도 클램프 끔도 유지해야 한다.
- 새 차이: 기존 HalfLife와 UAF SmoothingTime의 의미가 다르다. 기존 DamperImplicit 및 FMath/SpringMath 소스 대조로 `(max(HalfLife, DeltaTime)+1e-8)/ln(2)` 변환이 같은 감쇠 계수를 만든다는 식을 확인했다. 전체 실행 동등성은 미검증이다.
- 최소 후보는 기본 Offset Root 위의 프로젝트 additive trait다. `GetStackBaseTrait`·TraitUID 확인·타입별 인스턴스 데이터 접근과, 기본 Offset Root task 앞에 프로젝트 보정 task를 추가하는 소스 경로를 확인했다. 실행 프로그램은 추가 순서대로 실행한다. 엔진 수정 없이 같은 UAF 누적 상태를 보정할 수 있는 후보지만 **UE 5.8 UAFWarping 내부 헤더 의존성**이 필요하고 컴파일·런타임 성공은 아직 아니다.
- 기본 trait/task 복사는 중복 유지보수가 커 첫 후보에서 제외했다. 기존 ABP 래퍼는 UAF 자체 보정 증거로 삼지 않는다. 공개 UAFAnimNode node/AnimOp는 현재 trait 그래프와 실행 체계가 달라 공개 헤더 존재만으로 즉시 대체 가능하다고 판정하지 않았다.
- Phase05 실행서 §6에 기존 계약·후보 구조·초기화/텔레포트·base 중복 방지·입력 시점·수동 검증 조건을 기록했다. **이번 경로 조사 단위 완료.** 다음은 프로젝트 보정 trait 구현·컴파일·테스트 그래프 연결 단위이며 진행 전 사용자 확인을 받는다. 구면 게이트/5-1 전체는 미완료·Go/No-Go 보류·5-2 미착수.

### 구면 보정 trait 구현·그래프 연결 단위(2026-10-23, 진행 중)

- 사용자 계속 진행 승인 후 새 C++ trait·공유 입력 추가를 위해 PIE 종료·저장·에디터 종료를 요청했고 종료 답변을 받았다.
- `LNPUAFSpikeRootInputs.h`, `LNPSphericalOffsetRootTrait.h/.cpp` 추가. 기본 Offset Root를 UID로 확인한 뒤 같은 인스턴스의 누적 상태를 기본 task 직전에 보정한다. 기존 모드별 base 규칙·Up 스윙 정렬·±90° Yaw 제한·30cm 위치 제한·반감기 변환을 적용한다. 기본 구현 복사 없음.
- 기존 스파이크 공급층에 공유 `OffsetRoot` 묶음을 추가했다. VisualMesh 월드 변환·현재 base 변위·모드·반감기·지상/텔레포트·갱신 일련번호를 전달한다. 첫 평가·텔레포트 초기화와 같은 입력의 base 중복 운반 방지를 포함한다. 유효하지 않은 입력/다른 기본 trait 설정은 한 번 오류로 기록한다.
- UAFWarping 비공개 모듈 및 UE 5.8 내부 헤더 경로 의존성 추가. 엔진 파일·기본 ABP·기본 플레이어는 수정하지 않았다.
- 권한 확장 전체 빌드 `LootNPopEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE`: UHT 7개 파일 생성, 컴파일·링크 성공, 13 actions·190.73초. 최종 검토에서 루트모션 속성이 없는 프레임의 기본 상태 초기화에도 올바른 표시 메시 변환이 들어가도록 순서를 조정했다. 최종 증분 빌드도 성공, 4 actions·8.41초, 새 경고 없음. diff 공백 검사 통과.
- 사용자에게 에디터 재실행·로딩 완료를 요청했다. 아직 PIE 시작을 요청하지 않았다. 다음은 trait 등록 확인·테스트 AG에 기본 Offset Root+프로젝트 trait 추가·공유 OffsetRoot 한 핀 연결·S/AG 컴파일이다. 구면 실증·5-1 완료·최종 Go 판정과 구분한다.
- 그래프 변경 전 `AG_LocomotionSpike`·`S_LocomotionSpike`를 `Saved/AnimationOverhaul/2026-10-23_PreSphericalOffsetRoot_dadd230e/`에 백업하고 원본/사본 SHA-256 일치를 확인했다. 에디터 프로세스는 아직 없으며 재실행 완료 답변 대기다. 구현·연결 단위는 중간 상태로 유지한다.
- 사용자 에디터 재실행 후 PIE 꺼짐을 확인했다. MCP에서 새 보정 SharedData와 RootInputs의 `/Script/LootNPop` 객체를 조회해 ScriptStruct 등록을 확인했다. AG Variables에 새 OffsetRoot가 자동으로 표시됐고 기존 S·AG의 시작 시 자동 컴파일 성공을 확인했다. 아직 새 보정 스택이 연결된 컴파일은 아니다.
- 처음에는 사용자에게 Offset Root 노드 추가를 안내했지만 사용자는 우클릭 목록에서 찾을 수 없다고 보고했다. Pose History가 기존 Motion Matching 스택 내부 trait라 별도 노드가 아니라고 정정했다. 설치 소스의 CollectContextMenuActions·CreateTraitStackHeaderWidget에서 추가 경로를 확인한 뒤, **빈 공간 우클릭 → Trait Stack → Details Add Trait의 Offset Root Bone → 다시 Add Trait의 LNP Spherical Offset Root** 순서를 요청했다. 아직 기존 선을 바꾸지 않으며 두 trait 배치 확인 대기다. 새 자동화 도구·엔진 변경 없이 수동 조작으로 진행한다.
- 사용자 두 trait 추가 보고 뒤 화면에서 기본 Offset Root Bone과 LNP Spherical Offset Root가 같은 새 스택에 표시되고 이동/회전 속도 클램프가 꺼진 것을 확인했다. 아직 새 스택은 연결되지 않았고 기존 MM→Output 직결이다. 사용자에게 MM→새 스택 Input→Animation Output, 공유 OffsetRoot Getter→RootInputs 구조체 전체 핀 연결 후 AG·S 각각 Compile·Save를 요청했다. 현재 완료 답변 대기이며 PIE는 시작하지 않았다. 이 구현/연결 단위는 아직 완료 처리하지 않는다.
- 사용자는 MM→새 스택→Output 포즈 배선 완료를 보고했지만 OffsetRoot 드래그→Get 선택은 무반응이었다. 설치 소스에서 RigVM의 C++ 구조체 자동 등록 제외와 미등록 외부 변수 Getter 생성의 무오류 거절 경로를 확인했다. Unreal ScriptStruct 등록만 확인하고 RigVM 타입 등록을 빠뜨린 구현 누락이었다.
- `LootNPop.cpp`에 작은 게임 모듈 시작 함수를 두어 RootInputs를 `FRigVMRegistry::RegisterStructTypes`·`FindOrAddType`로 명시 등록하고 Build.cs에 RigVM 직접 의존성을 추가했다. 엔진 파일 수정 없음. 사용자에게 기존 포즈 배선 저장·에디터 종료를 요청해 종료 답변을 받은 뒤 권한 확장 기준 빌드 성공(5 actions, 48.13초, 새 경고 없음).
- 재실행 후 Get OffsetRoot 생성 → 보정 trait의 RootInputs 전체 핀 연결 → AG·S 각각 Compile·Save를 요청했으며 답변 대기다. 아직 GUI 해결·새 그래프 컴파일·구면 실행 성공을 주장하지 않는다. 같은 구현/연결 단위의 수정이며 다음 구현 단위로 넘어가지 않았다. PIE 미시작.

### 구면 보정 trait 구현·그래프 연결 단위 완료(2026-10-24)

- 사용자가 재실행 후 연결·컴파일·저장 완료를 보고했다. AG를 다시 열어 Get OffsetRoot.Value가 LNP Spherical Offset Root의 Root Inputs 구조체 전체 핀에 연결되고, MM.Result → 스택.Input → Animation Output.Result로 포즈가 흐르는 것을 화면으로 확인했다. 기본 Offset Root의 이동/회전 속도 클램프는 꺼짐, Compile 성공 아이콘·All Saved 표시 확인.
- 14:46:28 KST 최종 AG 컴파일 성공(24.895ms·24.334ms), 14:46:29 AG 저장, 14:46:50 S 컴파일 성공(9.965ms) 로그 확인. S 파일 수정 시각은 이전과 같아 재저장에 따른 내용 변경이 없었다. 사용자 저장 보고와 구분해 기록한다.
- 편집 중 OffsetRoot의 cached GUID/editor data 경고와 `RootInputs.bValid` 하위 핀 탐색 오류 5회가 기록됐다. 이후 최종 전체 구조체 연결과 AG/S 성공 컴파일을 확인했으며 런타임 입력 유효성은 아직 시험하지 않았다. 이 기록을 최종 컴파일 실패나 런타임 성공으로 해석하지 않는다.
- **구현/그래프 연결 단위 완료.** C++ 재변경·새 빌드·PIE 실행 없음. 다음은 북극/적도 이동·정지·큰 회전·공중·이동/회전 base 런타임 검증으로 진행 전 사용자 확인을 받는다. 구면 게이트·5-1 전체는 미완료, Go/No-Go 보류·5-2 미착수. TestMap03 임시 GameMode Override 유지.

### 구면 루트 런타임 검증 단위 시작(2026-10-24, 진행 중)

- 사용자 진행 승인 후 수동 1인 Standalone PIE·Rewind 녹화에서 현재 시작 지면의 달리기→정지 2회·180° 급반전 2회·점프/착지를 요청했다. 기울어짐·급회전·정지 후 계속 밀림 여부를 보고하고 녹화 종료·PIE 일시정지 상태로 두도록 요청했다. 적도와 base는 후속 절차로 남겼다.
- 14:51:15 KST `[UAFSpike] BP_PlayerUAFSpike_C_0 inputs=ready databases=1 trajectorySamples=31` 확인. 현재까지 확인한 구간에는 보정 trait의 입력 누락·스택 설정 오류가 없다. 이 로그는 변수를 전달했다는 근거이며 보정 task 실행·수치 동등성 통과 근거는 아니다.
- 아직 사용자 시험 결과·새 녹화 파일은 확인하지 않았다. 시작 위치의 극/적도 구분과 움직이는 base도 미확인이다. 코드·에셋 변경·빌드 없음. 실제 UAF 누적 상태와 숨김 ABP의 `[AnimB3]`를 혼동하지 않는다.
- 사용자 결과: 시작 지면에서 달리기→정지·180° 급반전 각 2회 이상, 점프/착지 수행. 급전환 때 룩 회전량에 표시 방향이 못 따라가 전방 달리기 포즈가 옆/대각선으로 달리는 현상을 쉽게 재현했다. 정지·점프 각각의 독립 통과 보고는 없으므로 통과로 기록하지 않는다. PIE는 사용자가 실수로 종료했고 MCP 꺼짐 확인.
- 새 Trace `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261024_145134378.utrace`(171,067,599바이트·14:52:36 KST)를 확인했다. PIE 종료 시 분석 종료·Trace 연결 오류 64가 기록됐지만 파일은 남았다. 현재 세션에서 그 파일의 프레임별 UAF 결과를 재조회한 것은 아니다.
- 원인 후보: `GetOffsetRootRotationMode`는 전신 몽타주 외 Accumulate라 컴포넌트 Yaw를 즉시 따라가지 않고 애니 루트모션을 누적한다. 기존 ABP B2 Steering×2가 궤적 목표와 가상 루트의 차이를 루트모션에 보충하는 반면 현재 최소 UAF에는 Steering이 없다. 엔진 UAF SteeringTrait의 같은 역할·RootBoneTransform/TargetOrientation 입력과 RootMotion Override 경로를 확인했다. 이 구조적 누락이 유력하지만 단일 원인 확정·실행 검증은 아니다.
- 새 도구/코드를 만들기 전에 사용자에게 새 Standalone PIE에서 `a.UAF.OffsetRootBone.Enable 0` → 같은 급회전 → `1` 복원 → 같은 급회전 비교를 요청했다. 새 녹화는 불필요, 마지막 1·PIE 일시정지 요청. 답변 대기이며 런타임 단위 미완료·적도/base 검증 보류다. 새 Steering 구현 단위는 결과 정리 후 사용자 진행 확인을 받는다.
- 사용자 CVar 비교 결과: **1에서 쉽게 재현, 0에서는 재현되지 않음.** 로그의 14:57:15 KST `a.UAF.OffsetRootBone.Enable 0`, 14:57:46 `1` 실행과 MCP 조회의 최종 값 1을 확인했다. MCP에서 현재 PIE는 꺼짐도 확인했다. 재현 범위를 Offset Root 적용 경로로 좁혔으며, 전체 런타임 단위를 통과 처리하지 않는다.
- 기존 ABP의 Accumulate+Steering 조합과 최소 UAF의 Steering 누락을 근거로 다음 단위를 Steering 보완·급전환 재검증으로 제안한다. UAF 자신의 누적 루트와 현재 애니 평가·궤적 목표를 사용해야 하며 숨김 ABP의 가상 루트 대입은 제외한다. CVar 비교만으로 Steering이 유일한 원인이라고 확정하지 않는다. 코드·에셋 변경·빌드 없음. 다음 구현 단위 사용자 승인 대기, 적도/base 시험 보류.

### 세션 인계·커밋 정리(2026-10-24)

- 사용자가 새 세션으로 넘기기 위한 문서 갱신·커밋을 요청했다. Steering 보완 진행을 승인한 답변은 아직 없어 구현에 착수하지 않았다. Current의 오래된 대기/중복 이력을 현재 구성·검증 한계로 정리하고, Phase05 §7에 소스 조사 시작점·입력/실행 순서 확인 항목·재현과 재검증 범위를 남겼다. Roadmap·기능 조사 문서도 최신 결과로 맞췄다.
- 커밋 범위는 Phase 5 문서 6개, UAF 테스트 에셋 5개, 스파이크 C++·모듈/의존성·프로젝트 플러그인 활성화, Rewind 로컬 저장 보완이다. 기존 사용자 변경인 DT_PlayerBaseStats·DT_LongSword_Levels·DA_MassSpawnConfig_TestOnePerSpecies와 사용자 변경/임시 Override가 혼재한 TestMap03은 커밋에서 제외해 그대로 남긴다. 새 체크아웃에서 시험할 때 테스트 GameMode Override를 다시 지정해야 한다.
- Saved의 녹화·백업은 git 제외이며 이동·삭제하지 않았다. TestMap03 원래 Override 구체 값은 현재 기록에 없어 5-1 종료 복원 전에 사용자 메모·기준값을 확인한다. 기본 플레이어/ABP나 맵 전체를 되돌리지 않는다.
- 최신 검증 근거는 기존 Development 빌드 성공(5 actions, 48.13초), AG/S 성공 컴파일·배선 화면, 사용자 Offset Root CVar 비교다. 이번 정리에서는 새 C++·에셋 변경이나 재빌드를 하지 않고 프로젝트 JSON 파싱·diff 공백·커밋 대상 검사를 수행한다. 방향 문제 미해결·5-1 미완료·5-2 미착수·Go/No-Go 보류 상태를 그대로 인계한다.

### Steering 보완·급전환 재검증 단위 시작(2026-10-24, 진행 중)

- 사용자 새 세션 요청으로 Steering 보완 단위 승인을 받고 시작했다. 실제 선택 DB/곡선·미래 루트모션·task 순서를 설치 UE 5.8 소스에서 확인했다. MCP IsPIERunning=false, 관련 등록 Agent Skill 없음 확인. 사용자에게 구조체 변경을 위한 저장·에디터 종료를 요청했고 종료 답변을 받았다.
- `LNPUAFSpikeRootInputs`에 궤적·이동 Steering 활성 사본을 추가하고 기존 `LNPSphericalOffsetRootTrait`/task에서 UAF 자신의 누적 루트·활성 클립/시간/곡선·실제 MM 검색 DB 태그를 연결했다. UAFPoseSearch Private·Internal 헤더 의존성을 추가했다. 숨김 ABP 루트나 DB 태그는 사용하지 않는다. 그래프 배선·기본 ABP·기본 플레이어·엔진·맵 변경 없음. 자세한 구현은 Phase05 §8.
- 두 채널의 회전 스케일·스프링을 Offset Root 소비 직전에 루트모션에 적용한다. 기존 base/Up/Yaw 보정 뒤 수행하며 이동량은 유지한다. 첫 평가·텔레포트·비활성·입력/루트모션 없음은 스프링 잔여를 버린다. 비교 CVar `LNP.UAFSpike.Steering` 기본 1, `LNP.Anim.DebugLocomotion 2`의 `[UAFSteering]`은 입력 준비/적용 여부·실제 DB/클립·곡선·소비 전 루트 Yaw/기울기·deltaYaw를 출력한다.
- **동등성 한계:** 현재 경로는 블렌드 출력에 활성 클립 예측을 적용한다. 기존 샘플별 Steering·0.1초 알파 블렌드와 같다고 주장하지 않는다. 급전환 개선은 아직 미확인이고 OrientationWarping·ResetRoot/CopyBone·발 배치·공중 고정·적도/base 검증도 남았다.
- 첫 기준 권한 확장 Development 빌드는 `MotionMatchingTraitData.h` include 경로 누락으로 실패했다. UAFPoseSearch Internal 경로를 보완한 뒤 빌드 성공(14 actions, 54.53초, 새 경고 없음). diff 공백 검사 통과. 기존 사용자 로컬 에셋 4개는 그대로 유지했다.
- 재실행 전 AG/S를 `Saved/AnimationOverhaul/2026-10-24_PreSteering_020043e9/`에 복사하고 원본/사본 SHA-256 일치를 확인했다. 사용자에게 에디터 재실행·AG/S 각각 Compile·Save를 요청했고 PIE는 아직 시작하지 않도록 안내했다. **현재 완료 답변 대기.** 이어서 Offset Root=1 고정·Steering 0/1 수동 급전환을 비교한다. 이번 단위 미완료, 다음 단위 진행 확인 전에는 적도/base로 넓히지 않는다. 5-1 미완료·5-2 미착수·Go/No-Go 보류.
- 사용자 재실행·컴파일/저장 완료 답변 후 15:51:57 KST S(11.599ms)·15:52:01 AG(36.476ms) 성공 로그를 확인했다. 에셋 파일 시각은 이전 그대로여서 내용 변경 저장과 구분한다. MCP에서 PIE 꺼짐, `LNP.UAFSpike.Steering=1`, Offset Root Enable=1·Enabled=true, 디버그=0 확인. Menu section 경고는 있었으며 AG/S 컴파일 실패는 아니다.
- 사용자에게 1인 Standalone PIE, Offset Root=1 유지·디버그=2에서 Steering 0/1 각각 이동 중 룩 급회전·180° 반전 2회 이상 및 1에서 이동→정지를 요청했다. 새 녹화는 불필요하며 마지막 Steering=1·PIE 일시정지 상태와 방향 추종 차이·새 기울어짐/떨림 보고를 요청했다. 현재 시험 결과 대기, 이번 단위 미완료다.
- 사용자 0에서 기존 옆/대각선 달리기 재현·1에서 크래시 보고. 첫 로그의 16:01:25 KST Steering=0 이후 ready=1·applied=0·rootYaw ±90°·rootTilt 0° 표본을 확인했다. 16:02:14 Steering=1 뒤 16:02:15 assertion, 재실행 로그에서도 16:03:41 같은 assertion·같은 task 호출을 확인했다. 입력 ready와 콜백 bound는 콜백 내부 캡처 객체 유효성 보장이 아니었다. 1의 방향 개선은 판정할 수 없다.
- 같은 구현 단위의 결함 수정으로 네이티브 루트모션 콜백을 task에 저장·지연 호출하던 경로를 제거했다. 현재 활성 그래프 FactoryObject의 `UAnimSequence`와 실제 MM 선택 일치·현재 timeline을 확인해 PostEvaluate에서 미래 루트 회전을 직접 샘플링하고 숫자 값만 task에 넘긴다. 시간·재생속도·루프 규칙은 유지한다. 비시퀀스/불일치면 ready=0이다. 콜백 캡처 애니가 무효인 점 외에 정확한 무효화 원인은 확정하지 않았으며 엔진 assertion/소스를 수정하지 않았다.
- 에디터 프로세스가 없는 것을 확인한 뒤 권한 확장 Development 빌드 성공(5 actions, 21.48초, 새 경고 없음). 두 크래시 로그를 `Saved/AnimationOverhaul/2026-10-24_SteeringCallbackCrash/`에 복사 보존했고 원래 dump도 유지했다. 공유 입력·그래프 배선 재변경 없음.
- 사용자에게 재실행·1인 Standalone PIE, Offset Root=1·Steering=1·디버그=2로 약 30초 달리기·룩 급회전/180° 반전·이동→정지·정지 방향 전환 및 크래시/개선/새 기울어짐·떨림 보고를 요청했다. 정상 실행이면 PIE 일시정지, 0의 재현은 반복 불필요. 현재 수정 후 시험 대기다. 이번 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류.

### Steering 보완·현재 시작 지면 급전환 재검증 단위 완료(2026-10-24)

- 사용자 재시험 결과: 8방향 달리기·정지·방향 전환 등을 시험했고 어색한 부분이 없었다고 보고했다. 이번 보고를 현재 시작 지면의 이동 방향 추종 개선 근거로 채택한다. 극/적도 위치·움직이는 base까지 시험했다고 확대하지 않는다.
- 현재 실행 로그의 16:10:35~16:10:48 KST Offset Root=1·Steering=1·디버그=2 명령과 16:11:02 inputs=ready 확인. 16:11:03~16:12:08 KST 약 65초의 UAFSteering 66개 모두 ready=1·56개 applied=1. F/B/LL/RL·Starts·Sprint 클립에서 실제 루트모션 보정 실행을 확인했다. 이번 실행 assertion/fatal 및 구면 입력/스택 설정 오류 0건, Foley 미처리 경고 잔류.
- 소비 전 상태의 약 1초 간격 표본은 rootTilt 모두 0°·|rootYaw| 최대 58.1°·|deltaYaw| 최대 6.33°다. 전 프레임/최종 출력 포즈·구면/base 통과 수치가 아니다. tip=1 표본은 0개라 제자리 회전 Steering 채널의 실제 적용과 샘플별 보정 동등성은 미검증이다.
- MCP에서 현재 PIE 꺼짐·Steering=1·Offset Root Enable=1/Enabled=true 확인. 성공 로그를 `Saved/AnimationOverhaul/2026-10-24_SteeringVerified/LootNPop.log`에 복사 보존했다. C++·에셋 재변경·재빌드 없이 Current·실행서·로그와 참조 요약을 갱신했다.
- **이번 구현/재검증 단위 완료.** 다음은 북극·적도 및 움직이는 base의 구면 루트 검증 단위를 제안하고 사용자 승인 전 착수하지 않는다. 제자리 회전 채널·공중 고정·발 배치·입력 배열 대조·전환/인터럽트·적/2인 회귀는 남았다. 5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 북극·적도/base 구면 루트 검증 단위 시작(2026-10-24, 진행 중)

- 사용자 진행 승인 후 직접 북극·적도 이동 및 이전 패널 탑승 가능 답변을 받았다. 사용자 수동 조작을 사용하며 새 이동 자동화나 시험 장치를 만들지 않는다. MCP PIE=false 확인, 편집 월드 PlayerStart 4개·패널/마커는 빈 배열 조회. 런타임 패널 부재로 단정하지 않았다.
- 기존 소비 전 1초 표본으로 최종 루트를 판정할 수 없어 스파이크에 `FLNPInspectSphericalOffsetRootTask`를 추가했다. 기본 Offset Root 뒤의 같은 VM 포즈/누적 상태를 읽고 디버그=2일 때 각 1초 구간의 매 평가 프레임 최대값을 `[UAFRoot]`에 기록한다. 위치/위도·동적 base·운반 델타·조건별 프레임 수도 함께 기록한다. 숨김 ABP 포즈/루트는 사용하지 않으며 실제 포즈는 수정하지 않는다. 상세·판정 범위는 Phase05 §9.
- 공유 입력에 base 경로·동적 여부를 추가했다. 사용자 저장·종료 후 기준 권한 확장 Development 빌드: 첫 시도는 동명 FReferencePose 타입 충돌 실패, UAF 네임스페이스 명시 후 성공(4 actions, 11.68초, 새 경고 없음). diff 공백 검사 통과. 기존 스파이크 파일 4개 외 엔진·기본 ABP·맵·기존 사용자 에셋 변경 없음.
- 사용자에게 재실행·AG/S Compile/Save 후 1인 Standalone PIE, Offset Root=1·Steering=1·디버그=2로 북극 약 20초 이동→정지·급반전·정지 룩 회전 및 이상 보고·PIE 일시정지를 요청했다. 현재 답변 대기, 새 진단 컴파일/실행·북극/적도/base 결과는 미확인이다. 이 단위 미완료·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 북극 수동 시험 결과 확인(2026-10-25, 같은 구면 검증 단위 진행 중)

- 사용자 북극 근처에서 돌아다녔고 어색한 부분이 없었다고 보고했다. 12:01:19 KST AG·12:01:22 S 컴파일 성공, 세 CVar 실행과 실제 UAFRoot 출력 확인. MCP PIE 활성·Steering=1·Offset Root=1 확인. 일시정지는 별도 확인하지 않았다.
- 구간 끝 위도 80.59~89.93°인 135개 구간(8,086 평가 프레임, ground 7,952·based/reset 0)에서 누적 상태/실제 출력 모두 거리 최대 30cm·Yaw 최대 92.4°·Tilt 최대 0.055°다. lat은 구간 마지막 값이며, 공중 포함 구간과 순수 지상 구간을 구분한다. Yaw 92.4°와 Tilt 0.055° 최대가 나온 각 구간은 지상 프레임만 포함했다.
- 기존 ±90° 클램프는 루트모션 소비 전이라 최종 Yaw 92.4°가 기록됐다. 사용자 관찰 이상 없음과 분리해 **엄밀한 최종 ±90° 상한은 미보장**으로 남긴다. 이 결과만으로 구면 전체 통과나 코드 수정 필요성/원본 ABP와의 차이를 확정하지 않는다. 적도/base 결과와 함께 검토한다.
- 완전 정지 지상 구간 9개, 마지막 거리 최대 0.055→0.000cm 수렴. 12:06:06 KST TurnInPlace DB·045_L의 ready=1/applied=1/tip=1 확인으로 실제 제자리 회전 채널 실행을 확인했다. 전체 실행 assertion/fatal 0건. 북극 로그를 `Saved/AnimationOverhaul/2026-10-25_NorthRoot/LootNPop.log`에 복사 보존했다.
- 같은 PIE에서 적도로 직접 이동하고 같은 설정으로 약 30초 8방향 이동·정지·급반전·정지 룩 회전, 마지막 5초 입력 없이 정지 후 일시정지·이상 보고를 요청했다. 현재 적도 결과 대기, 이후 base를 시험한다. C++/에셋 변경·빌드 없음. 이 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 적도 수동 시험 결과 확인(2026-10-25, 같은 단위 진행 중)

- 사용자 적도 부근에서도 비슷하게 플레이했고 어색한 부분이 없었다고 보고했다. 이전 북극 접근 중 지나간 적도 표본과 분리해 12:06:19 KST 이후·구간 끝 |lat|≤5°만 집계했다. 12:08:50~12:11:18의 138구간·8,143평가 프레임은 모두 지상, 위도 −1.97~4.97°·based/reset 0이었다.
- 누적 상태/실제 출력 모두 거리 최대 30.000cm·Yaw 최대 90.00°·Tilt 최대 1.613°. 완전 정지 지상 구간 39개, 마지막 거리/Tilt는 0으로 수렴했다. 전체 실행 assertion/fatal 0건. 로그는 `Saved/AnimationOverhaul/2026-10-25_EquatorRoot/LootNPop.log`에 보존했다. MCP PIE 활성·Steering=1 확인.
- 적도 Tilt 1.613°는 지상 이동 60프레임 구간 최대값이다. 가까운 Steering 표본은 Sprint Loop지만 동일 프레임 원인은 미특정이므로 애니 회전/스프링/갱신 시점 중 어느 하나를 원인으로 확정하지 않는다. 북극 Yaw 92.4°와 함께 엄밀한 최종 상한/Up 정렬의 보류 항목으로 남긴다. 시각 관찰 이상 없음과 구면 수치 통과를 구분한다.
- 같은 PIE·세 CVar 유지로 이동패널 위 입력 없이 왕복 1회 이상(최소 30초), 이후 걷기→정지·룩 회전 약 10초와 왕복/상대 앞서감·밀림·기울어짐/급회전 보고·PIE 일시정지를 요청했다. 현재 base 시험 답변 대기다. 코드/에셋·설정 변경·빌드 없음. 이 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 패널 결과·관찰/측정 단위 종료 및 경고 확인(2026-10-25)

- 사용자 패널 시험 완료·어색한 부분 없음 보고. 실제 `BP_PyramidExteriorPanel_C_3.PanelMesh` 동적 base 탑승 177구간·10,327 based 프레임 확인. `based=ground=frames`, moving=reset=0인 구간은 107개·6,332프레임, 누적 상태/실제 출력 거리 최대 0.901cm·마지막 0.000cm였다. 이전 ±30cm 이중 운반 패턴은 이 범위에서 미재현이다.
- 최종 Tilt는 이동 기준 정지 탑승 최대 5.196°·이동 포함 10.593°였다. based 구간의 거리 최대 30cm·Yaw 최대 90°. base 델타 최대 38.071cm·0.0028°로 병진 운반은 확인했고 큰 각도 회전 base는 미검증이다. 전체 실행 assertion/fatal 0건. MCP PIE 활성·실제 PanelMesh 컴포넌트 확인. 일시정지는 별도 검증하지 않았다. 로그를 `Saved/AnimationOverhaul/2026-10-25_PanelRoot/LootNPop.log`에 보존했다.
- 소스 재대조로 프로젝트의 Up/±90° 보정 뒤 기본 task가 애니/Steering 루트모션을 추가하고 최종 구면 제약을 다시 적용하지 않는 순서를 확인했다. 엄밀한 최종 Up/Yaw는 미통과로 남긴다. 피크 동일 프레임의 세부 루트모션 원인까지 확정한 것은 아니다. 같은 UAF 상태/출력에 소비 후 제약을 일관되게 적용하는 후속 구현 단위를 제안하고 승인 전 착수하지 않는다.
- 사용자 LogAnimation 경고 영향 질문에 전체 종류를 집계했다. 확인 시점 Warning 1,997건 전부 미구현 FoleyEvent(Run 1,846·Walk 83·Jump 36·RunStrafe 32), LogAnimation Error 0건. 엔진 소스에서 해당 이벤트 엔트리가 없으면 경고 후 호출만 반환하는 경로를 확인했다. 현 로코모션 검증은 계속 가능하나 Foley 연출/로그 정리는 남고, 루트 Tilt와의 인과는 확인되지 않았다. 카테고리 억제나 새 이벤트 구현 없음.
- **북극·적도/패널 관찰·측정 단위 완료, 구면 수치 미통과.** 사용자 세 지역 관찰 이상 없음과 수치 문제를 구분해 Current·실행서에 반영했다. C++/에셋 변경·빌드 없음. 다음 소비 후 보정 구현·재검증은 사용자 승인 대기, 5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 소비 후 구면 제약 보완 구현·빌드·수치 회귀 시험(2026-10-25, 진행 중)

- 사용자 진행 승인 후 PIE 활성 확인, 종료·저장·에디터 종료를 요청해 종료 답변을 받았다. 같은 스파이크 h/cpp 두 파일에 최종 회전 보정 task와 수치 시험을 추가했다. 엔진·기본 플레이어/ABP·맵·base 운반 로직 변경 없음.
- 준비 → 기본 Offset Root → 최종 회전 보정 → 진단 순서를 구성했다. Up 스윙 정렬·컴포넌트 로컬 Yaw ±90° 제한 함수를 준비/최종 단계가 공유한다. 누적 상태의 보정 회전 차이를 메시 로컬로 옮겨 출력 root에 적용하므로 원본 입력 root의 상대 회전을 보존한다. 위치·스케일·누적 위치·루트모션 속성은 유지한다. 디버그 꺼짐에서도 실행하며 비활성/알파 0/무효 입력/기본 평가 없음은 기존 출력을 유지한다. 상세는 Phase05 §10.
- 권한 확장 기준 Development 빌드 성공(5 actions, 37.65초, 새 컴파일 경고 없음). diff 공백 검사 통과. 수치 시험 `LootNPop.Animation.UAFSpike.SphericalRootFinalization`을 headless UnrealEditor-Cmd·Entry·NullRHI에서 실행해 1/1 성공·실패 0·종료 코드 0 확인. 약 0.02초. 보고서는 `Saved/Automation/2026-10-25_UAFRootFinalization/index.json`, 별도 로그는 `Saved/Logs/UAFRootFinalizationTest_2026-10-25.log`다.
- 시험은 네 컴포넌트 방향에서 Tilt 10.593°·양/음 Yaw 92.4° 재현, 최종 상태의 Up/±90° 경계, 원본 포즈의 비영 상대 회전·위치·스케일 보존, 정상 출력 불변성을 확인했다. 그래프 task 배선/실제 플레이나 CVar/모드별 런타임 전체를 검증한 것으로 확대하지 않는다.
- 사용자에게 재실행·AG/S Compile/Save 후 같은 설정으로 북극·적도 각 20~30초와 마지막 5초 정지, 패널 무입력 왕복(최소 30초) 및 걷기/정지·룩 회전 약 10초를 요청했다. 완료 후 PIE 일시정지·새 떨림/급회전/밀림/기울어짐·크래시 보고 대기. 새 배선은 없다. 현재 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 소비 후 구면 제약 보완 단위 완료(2026-10-25)

- 사용자가 요청한 북극·적도·패널 재시험 완료·어색한 부분 없음 보고. 12:43:40 KST S·12:43:44 AG 성공 컴파일과 12:44:07~17 세 CVar 명령 확인. 새 배선이나 추가 빌드 없음.
- 12:44:40~12:50:59 KST 기록 378구간·22,583평가 프레임의 누적 상태/실제 출력 모두 거리 최대 30cm·Yaw 최대 90°·Tilt 최대 0.000°(로그 해상도). 북극 65구간·3,882프레임, 적도 73구간·4,372프레임, 동적 base 포함 118구간·7,036평가 프레임(실제 based 7,009), 정지 탑승 70구간·4,192프레임을 구분했다. 위도는 구간 끝 값 기준이며 무로그 프레임·임의 조건까지 일반화하지 않는다.
- 패널 정지 탑승 거리 최대 0.345cm·마지막 0.000cm, Tilt 0.000°. 제자리 회전 Steering 적용 표본 12개. assertion/fatal·구면 입력/스택 오류 0건·LogAnimation Error 0건, 기존 Foley Warning 1,361건 잔류. base 델타 최대 75.336cm·0.001°로 큰 회전 base는 미검증이다.
- 성공 로그는 `Saved/AnimationOverhaul/2026-10-25_FinalRootVerified/LootNPop.log`에 보존했다. MCP PIE 활성·Steering=1·Offset Root=1 확인. 현재 결과 정리에서 코드/에셋 변경 없음. 이전의 Yaw 92.4°·패널 Tilt 10.593° 최종 제약 문제는 현재 시험 범위에서 개선됨을 기록했다.
- **이번 소비 후 보정 구현·재검증 단위 완료.** 다음은 공중 Fall_Loop 고정(AN-007) UAF 경로 조사·구현·수동 검증을 제안하고 승인 전 착수하지 않는다. 샘플별 보정·큰 회전 base·발 배치·입력 대조·전환/인터럽트·적/2인 등 미검증 범위를 유지한다. 5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 새 세션 인계·커밋 정리(2026-10-25)

- 사용자가 다음 공중 고정 작업을 새 세션으로 넘기고 문서 정리·커밋을 요청했다. 공중 고정 구현/조사에 착수하지 않고 Current를 현재 결과·다음 행동 중심으로 줄였다. Phase05 §11에 다음 단위의 문서/소스 확인 순서·기존 구성·검증·자료·주의점을 모았다. Roadmap의 오래된 Steering 승인 대기 문구와 Phase05 §7의 시작점도 최신 인계로 연결했다.
- 이번 커밋 범위는 Steering·소비 후 구면 보정·최종 루트 진단/수치 회귀를 담은 스파이크 C++/Build.cs 5개와 Phase 5 문서 5개다. TestMap03 및 기존 사용자 데이터 에셋 3개는 제외한다. Saved의 로그·시험 보고서·크래시/백업은 git 제외 상태로 보존한다. 새로운 에셋/엔진/기본 ABP 변경은 없다.
- 인계 확인 시 MCP PIE 활성, 일시정지는 별도 확인하지 않았다. 에디터/PIE나 임시 GameMode Override는 변경하지 않았다. 다른 체크아웃에서는 테스트 Override를 재지정해야 하며 원래 Override는 아직 구체 값 미확인이다.
- 최신 검증은 Development 빌드 성공(5 actions, 37.65초), C++ 수치 회귀 1/1, AG/S 컴파일, 사용자 세 지역 시험과 최종 루트 수치다. 이번 정리에서는 코드 수정·재빌드·새 PIE를 하지 않고 문서 링크·diff 공백·커밋 파일 범위를 확인한다. 5-1 미완료·5-2 미착수·Go/No-Go 보류를 유지한다.

### 공중 Fall_Loop 고정 단위 착수(2026-10-25, 진행 중)

- 사용자 Phase 5 진행 요청으로 AN-007 단위에 착수했다. 기존 ABP의 실제 UEFN Fall_Loop 참조·속도 1/시작 0/루프와 Bool 양방향 0.25초를 MCP에서 확인했다. 기본 ABP·소스 에셋은 읽기만 했다.
- 사용자 PIE 종료와 MCP 비활성을 확인했다. dirty AG 하나만 저장 성공 후 `Saved/AnimationOverhaul/2026-10-25_131403_PreAirLoop/`에 백업했다. 전용 UAF 그래프 MCP가 없어 Bool·Sequence Player 배선과 Pose History 재배치를 수동 요청했다. 사용자는 1~5단계 진행을 보고했고 trait 이동 방법을 질문했다. Bool 내부에 재추가·설정 복사 후 MM 쪽 것을 제거하는 의미로 바로잡아 안내했다. 완료·컴파일 결과는 아직 대기다.
- 기본 Bool의 복수 자식이 Steering 인터페이스 전달을 막는 점을 엔진에서 확인했다. 같은 cpp의 MM 조회에 목적 자식 경유를 추가하고, 그 MM 스택에서 활성 클립·시간을 함께 읽게 했다. 새 클래스/공유 변수·엔진 변경 없음. diff 공백 검사 통과, C++ 컴파일·실행은 아직 하지 않았다.
- 현재 구성·다음 검증은 Phase05 §12가 소유한다. 이번 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류. 단위가 끝나면 다음 단위 진행 여부를 다시 확인한다.

### 공중 고정 배선 완료 보고·컴파일 확인(2026-10-25 13:26 KST)

- 사용자 Pose History 재배치·Compile/Save 완료 보고. 로그에서 13:26:05~06 AG 성공 컴파일·13:26:07 저장을 확인했다. MCP로 배선 전체를 덤프한 것은 아니다.
- PIE 비활성 확인 후 Live Coding 실행 **Success**, 13:26:47 AG/S 재컴파일 성공. cpp 한 파일의 조회 변경을 반영했다. 전체 빌드·새 자동화 시험은 수행하지 않았다.
- 북극/적도에서 기존 세 CVar로 지상 이동/회전·점프·5초 이상 활공/그래플·착지 후 이동과 마지막 5초 정지 후 일시정지·보고를 수동 요청했다. 현재 결과 대기, 공중 고정 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 공중 고정 첫 PIE 회귀 보고(2026-10-25)

- 사용자는 지역 도착 전 정면 달리기 이상·점프 착지 후 잠시 옆 걷기 재생을 보고했다. 지역별 공중 검증은 미완료, MCP PIE 종료 확인. 이번 단위는 통과 처리하지 않는다.
- 13:28 실행의 Phase 커브 누락 경고 794건을 확인했다. 같은 실행의 UAFSteering 34개 표본에서 지상 MM DB/클립·시간 조회는 작동했다. 원인 전체를 확정하지 않는다. 로그를 `Saved/AnimationOverhaul/2026-10-25_AirLoopRegression/LootNPop.log`에 복사 보존했다.
- 새 Bool Pose History의 Collected Curves=Phase·공유 Trajectory 연결·Generate Trajectory 끔·배율 1·본/Condition을 사용자에게 수동 확인/보완 요청했다. 재배치 후 실제 입력 확인 전에 넓은 지역 시험을 요청한 점을 바로잡고 짧은 지상/점프 재시험부터 진행한다. 이 확인에서 새 C++ 변경·컴파일 없음. 5-1 미완료·5-2 미착수·Go/No-Go 보류.

### Phase 보완 후 짧은 재시험 통과(2026-10-25 13:37 KST)

- 사용자 Phase 누락 보완·새 Bool Pose History에 공유 Trajectory 연결·Generate Trajectory 끔/배율 1/본 4개 정상 보고. MM의 Trajectory 핀은 제거한 Pose History 소유여서 사라지는 것이 정상이며 MM 연결 유지 안내를 정정했다. 13:36:51~52 AG 컴파일/저장 성공.
- 짧은 달리기·이동 점프/착지 후 사용자 두 증상 모두 사라짐 보고. 13:37:21~41 21구간·1,247평가 프레임(ground 1,015·air 232)에서 상태/출력 거리 최대 30cm·Yaw 21.28°·Tilt 0.000°. 실제 구간 끝 위도 −87.21~−67.46°의 남극 인근이며 북극/적도 완료로 취급하지 않는다.
- 새 실행 Phase 경고·PoseSearch Warning/Error·LogAnimation Error·assertion/fatal 0건, 기존 Foley 경고 잔류. Steering 적용 표본 12개·공중 MM 조회 비활성 표본 4개·착지 Lands/Run 복귀 확인. 로그 `Saved/AnimationOverhaul/2026-10-25_AirLoopShortVerified/LootNPop.log` 보존. 실제 고정 Sequence Player·긴 체공 전체는 아직 미검증이다.
- 확인 시 PIE 종료·세 CVar 1/1/2. 코드 변경·재컴파일 없이 남은 긴 체공/적도 및 가능하면 Rewind 녹화를 요청했다. 공중 고정 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류.

### 긴 체공·적도 녹화 시험 관찰/로그 확인(2026-10-25)

- 사용자 시험·녹화 완료·이상 없음 보고. 최신 Trace `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261025_133940695.utrace` 생성과 녹화 종료 확인, 로그는 `Saved/AnimationOverhaul/2026-10-25_AirLoopRecorded/LootNPop.log`에 보존했다.
- 13:39 이후~13:42:55 205구간·10,163평가 프레임(ground 6,997·air 3,166), 최종 상태/출력 거리≤30cm·Yaw≤90°·Tilt 0.000°. 끝 위도 기준 남극 17구간/895프레임·적도 45구간/2,202프레임 확인. 위도와 프레임 분모 한계는 Phase05 §12에 기록했다.
- 그래플 발동 11회, 연속 공중 5.28초·32.53초 확인. 순수 공중 구간 끝 위도 5.11~51.05°로 긴 체공 전체가 적도 ±5°에 머물렀다고 해석하지 않는다. Steering 적용 표본 109개·공중 MM 조회 비활성 62개. Phase/PoseSearch 경고·애니 Error·assertion/fatal 0건, Foley Warning 443건 잔류.
- 녹화 종료 때 MassTrace 미종료 PrePhysics 경고 1건·요약 Error 카테고리(`1 warnings; 0 errors`), ETW 권한/프로파일 메타데이터 메시지를 관찰했다. 전체 로그가 무경고/무Error라는 판정은 하지 않는다.
- UAFLocomotion의 실제 Fall_Loop 두 공중 시점·착지 지상 MM 복귀를 Rewind에서 확인하도록 요청했고 결과 대기다. 코드/에셋 추가 변경·재컴파일 없음. 공중 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### Rewind 클립 확인 경로 정정(2026-10-25)

- 사용자는 숨김 ABP에서 Fall_Loop를 찾았으나 AG 하위 트랙에서는 못 찾았다고 보고했다. 이것으로 UAF 실행을 통과/실패 판정하지 않았다.
- 엔진 SequenceInfoTrack.cpp에서 UUAFComponent의 `SequenceInfo` 트랙이 실제 기록된 평가 프로그램의 SequenceKeyframe task를 읽어 AnimSequence·SampleTime을 표시하는 점을 확인했다. `UAFLocomotion → SequenceInfo → Details의 Sequence Trace Info`에서 공중 두 시점·착지 후 클립을 확인하도록 안내를 정정했다.
- 기존 녹화를 유지하고 답변 대기. 새 코드/계측·PIE/녹화 없음. 공중 고정 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 실제 UAF 공중 평가 task 직접 확인(2026-10-25)

- 사용자 SequenceInfo 트랙 없음 보고 후 computer-use와 Slate snapshot으로 현재 UI를 직접 확인했다. 해당 트랙은 없으며 기존 EvaluationProgram에서 Tasks를 읽는 경로를 사용했다. SequenceInfo 표시 조건 자체의 실패 원인은 미확정으로 남긴다.
- `UAFLocomotion → S → UAFAnimGraph_7 → AG_LocomotionSpike → EvaluationProgram`의 약 156.1초 Tasks[1]에서 `MM_Unarmed_Jump_Fall_Loop_UEFN`, SampleTime 0.243282, Looping 켬을 직접 확인했다. 같은 프로그램은 History Collector·구면 Offset Root의 준비/기본/최종 보정/진단 작업도 포함한다.
- 시간 헤드 자동 스크럽이 반영되지 않아 수동이 더 효율적이라고 판단하고 약 159초 공중과 약 190초 착지 후 클립/SampleTime만 추가 조회 요청했다. 새 코드/에셋/녹화/PIE 변경 없이 화면의 평가 task를 펼친 상태로 인계했다. observer 해제, 나머지 두 시점 대기. 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 공중 Fall_Loop 고정 단위 완료(2026-10-25)

- 사용자 조회: 159초 `MM_Unarmed_Jump_Fall_Loop_UEFN`·SampleTime 0.102736, 190초 `M_Relaxed_Run_Stop_F_Lfoot`·SampleTime 1.812737. 에이전트가 직접 읽은 약 156.1초의 같은 Fall_Loop·0.243282·Looping 켬과 합쳐 실제 UAF 공중 두 시점 유지·착지 후 지상 클립 복귀를 확인했다.
- Bool/고정 시퀀스와 Pose History 재배치·Steering 조회 보완, Live Coding/AG/S 컴파일, 최초 설정 누락 회귀 해소, 긴 체공/적도 사용자 관찰, 10,163평가 프레임의 최종 Tilt 0.000°·Yaw≤90°·거리≤30cm까지 확인해 **이번 구현·검증 단위를 완료**했다. 모든 프레임 클립 동일성·활공 단독·북극 별도·적/네트워크까지 확대하지 않는다.
- 실제 확인 경로는 SequenceInfo가 아니라 AG의 EvaluationProgram Tasks다. SequenceInfo 표시 문제·기존 Foley 및 녹화 분석 경고는 남긴다. 이 결과 확인에서 추가 코드/에셋 변경·재컴파일·녹화 없음.
- Current·Phase05 완료 체크리스트/판정표·Roadmap을 갱신했다. 다음 입력 PSD 배열 대조·전환/인터럽트 검증은 진행 여부를 확인하고 승인 전 착수하지 않는다. 5-1 전체 미완료·5-2 미착수·Go/No-Go 보류.

### 입력 PSD 배열·전환/인터럽트 검증 착수(2026-10-25)

- 사용자 진행 승인 후 기존 녹화·공급 코드/로그를 재사용해 대조를 시작했다. 190초 AG Native Properties의 `[PSD_Stand_Idles_UEFN]`·DoNotInterrupt·BlendTime 0.5와 같은 시점 공급 Chooser의 Idle 행/상태를 직접 확인했다. 실제 Run Stop 진행 포즈와 입력 배열의 이름을 별개로 판정한다.
- Chooser Rewind 트랙의 더블클릭은 테이블 편집기를 디버그 대상으로 연결하며 단일 클릭에는 Details가 없는 점을 엔진에서 확인했다. 읽기 후 Chooser 창을 닫았고 해당 에셋 파일 변경은 없다.
- 약 1.5초의 첫 출발로 사용자 수동 스크럽을 요청했다. 코드/에셋 변경·빌드·추가 녹화 없음. 이어지는 실제 표본은 Phase05 §13이 소유한다. 이번 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 출발 배열·인터럽트 경계 확인(2026-10-25)

- 사용자 첫 출발 스크럽 이후, 약 1.5초 공급 Chooser의 Starts/Loops 두 결과와 AG의 같은 순서 배열·DoNotInterrupt·0.5를 대조했다. 툴바 한 프레임 뒤로 버튼으로 첫 이동 경계를 읽어 같은 배열/0.5·InterruptOnDatabaseChange를 확인했고, 한 프레임 더 뒤는 Idles·DoNotInterrupt·0.5였다.
- UAF Pose Search Active Pose의 Run_F_Starts DB·M_Relaxed_Run_Start_F_Rfoot, Asset Player Time 0.266667·InterruptOnDatabaseChange·Elapsed Pose Search Time 0.0을 읽었다. 후보표/Active Pose·이벤트 없는 프레임의 표시 잔류를 구분한다. 세부는 Phase05 §13.
- 첫 착지 약 16초로 수동 스크럽 요청. 코드/에셋 저장·빌드·새 녹화 없이 검증 중. 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 첫 착지와 후속 지상 설정 확인(2026-10-25)

- 사용자 첫 착지 스크럽 뒤 AG의 Run_Lands_Heavy 단일 DB·InterruptOnDatabaseChange·0.2초와 실제 UAF Heavy Rfoot 착지 검색/인터럽트를 확인했다. 다음 프레임은 Sprint_Loops·DoNotInterrupt·0.5초, 한 프레임 뒤로 되돌리면 착지 설정이었다. 공급 로그의 연속 프레임 615→616도 같은 값이다.
- 착지 공급 Chooser 창의 Gait Sprint 표시는 입력/로그와 시점 차이가 있어 그 창의 동일 평가 배열 대조까지 완료로 확대하지 않는다. 코드/에셋 변경·저장·빌드·새 녹화 없음. 약 188.6초 제자리 회전 구간으로 사용자 수동 스크럽 요청, 정지·방향 전환 검증은 남았다. 상세 Phase05 §13, 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 제자리 회전 요청·인터럽트 확인(2026-10-25)

- 약 188.6초 공급 Chooser의 TurnInPlace 결과와 AG의 같은 단일 배열·InterruptOnDatabaseChange·0.5를 대조했다. 실제 UAF Active Pose는 M_Relaxed_Stand_Turn_045_R, 인터럽트·Elapsed Pose Search Time 0.0·Asset Player Time 0.153641초였다.
- 이전 프레임의 Idles·DoNotInterrupt·0.5와 회전 요청 경계, 이후 회전 DB 유지·인터럽트 해제를 직접 확인했다. 단일 프레임 펄스로 일반화하지 않는다. 약 189.4초 마지막 달리기→정지로 수동 스크럽 요청했다. 상세 Phase05 §13, 현재 단위 진행 중이며 코드/에셋 저장·빌드·새 녹화 없음.

### 정지 입력·실제 Run Stop 확인(2026-10-25)

- 약 189.4초 Run Stops·DoNotInterrupt·0.5, 한 프레임 이전의 같은 배열·InterruptOnDatabaseChange·0.5와 공급 Chooser의 같은 결과를 확인했다. 실제 UAF Active Pose는 M_Relaxed_Run_Stop_F_Lfoot, DoNotInterrupt, Asset Player Time 1.15367초였다. 190초 Idle 입력 표본 및 공급 로그의 정지→Idle 복귀와 함께 기록했다.
- Details가 비거나 이전 선택을 유지한 후속 조회는 근거로 쓰지 않았다. 약 35.35초 방향 전환으로 사용자 수동 스크럽과 AG 트랙 선택을 요청했다. 상세 Phase05 §13, 단위 진행 중이며 코드/에셋 저장·빌드·새 녹화 없음.

### 방향 전환 확인·대표 입력/전환 검증 단위 완료(2026-10-25)

- 사용자 스크럽 및 Details 전환 후 일부 프레임의 빈 Native Properties를 실제 입력 없음으로 해석하지 않고 인접 프레임을 조회했다. AG에서 F Loops/DoNotInterrupt → RL Loops+RL Spins/InterruptOnDatabaseChange → 같은 두 DB/DoNotInterrupt, BlendTime 0.5를 직접 확인했다. 두 DB 순서는 공급 로그와 일치한다. 실제 UAF RL Spins/Rfoot·인터럽트·Asset Player Time 0.966667초도 확인했다. Chooser 화면은 RL Loops 선택까지 직접 읽었고 두 번째 행은 공급 로그로 대조했다.
- Idle·출발·착지·제자리 회전·정지·이동 방향 전환의 대표 표본 검증 단위 완료. 모든 전환 DB/프레임 동등성이나 PSD·Chooser 게이트 전체 통과로 확대하지 않는다. 착지 Chooser 표시 시점 차이·빈 Native Properties 프레임·Pivot/Sprint Turns/GaitTransitions 추가 표본 등 한계를 Phase05 §13에 남겼다.
- 이번 단위는 문서만 변경했고 C++/에셋 저장·빌드·새 계측·녹화 없음. Current·Phase05 판정표·Roadmap 갱신. 다음 지상 Orientation Warping 동등 경로 단위는 사용자 진행 확인 후 착수한다. 5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 세션 종료·커밋 인계(2026-10-25)

- 사용자 요청으로 다음 구현을 시작하지 않고 인계 문서를 정리했다. Current의 세션 종료 인계에 다음 첫 행동·승인 경계·실제 Rewind 조회 경로/기록 한계·로컬 Trace/로그/백업 경로·커밋 범위를 모았다. 인계 시 PIE 실행 상태만 API로 확인했으며 에디터/PIE 종료·새 녹화는 하지 않았다.
- 공중 고정 AG와 Bool 목적 자식 MM을 따르는 Steering 조회 C++, 관련 문서 4개의 총 6파일을 커밋 대상으로 한정한다. TestMap03 및 기존 데이터 에셋 3개의 로컬 변경은 보존·제외한다. 문서/코드 diff 공백 검사와 커밋 대상 확인을 수행하고, 앞서 성공한 Live Coding·AG/S 컴파일·PIE/녹화 검증 이후 구현 변경이 없어 빌드를 반복하지 않는다.
- 다음 Orientation Warping 단위는 아직 착수 전이다. 다음 세션에서 진행 요청을 받으면 이어가며, 구현 단위마다 진행 확인·수동 조작 우선 규칙을 유지한다. 5-1 미완료·5-2 미착수·Go/No-Go 보류.

### 기본 Strafe Warping 조사·입력 어댑터(2026-10-25, 진행 중)

- 사용자 재개 요청 후 실제 테스트 ABP와 로컬 UE 5.8 소스를 대조했다. 기존 OrientationWarping의 TargetTime=0.8초 예측은 기본 UAF Strafe Warping에 없고, 발/루트 처리도 다르다. 사용자가 기본 노드를 최소 연결해 시각 비교하는 경로를 선택했다. 완전 동등 보정 이식이나 최종 Go를 승인한 것으로 해석하지 않는다.
- 엔진 보정 수학을 유지하는 입력·순서 어댑터를 C++ 4파일에 구현했다. 현재 UAF 루트 기준으로 기존 준비/Steering → 기본 Strafe → Offset Root 순서를 만들고 지상 커브/접평면 방향을 공급한다. 아직 그래프 연결·실행 순서·시각 결과는 검증 전이다. 상세는 Phase05 §14가 소유한다.
- diff 공백 검사 통과. 17:24 KST Live Coding 요청은 시작 로그만 확인됐고 컴파일 결과 대기 중이다. 사용자에게 창/대화상자 상태를 요청했다. AG 디스크 백업을 Saved에 만들었고 에셋 저장·PIE·새 녹화는 하지 않았다. 기존 로컬 변경 4개를 보존했다. 워핑 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류.

### Live Coding 성공 후 알림 경로 크래시·정상 빌드 복구 착수(2026-10-25)

- 사용자가 컴파일 지연 중 Live Coding 창이 정상 표시된다고 답했고 이후 에디터 크래시 콜스택을 제공했다. UBT는 9 actions·293.99초·Succeeded, 패치 링크·AG/S 재컴파일도 성공했다. MCP 호출은 300초 시간 초과로 성공 응답을 받지 못했다.
- 17:29:50 KST `LiveCoding::AttemptSyncLivePatching → ShowNotification → Slate 알림 SWindow → mimalloc`에서 접근 위반이다. 신규 Strafe task는 미배선·미실행이며 런타임 평가 오류로 분류하지 않는다. 메모리 손상 근본 원인은 미확정이다. 상세와 근거 파일은 Phase05 §14가 소유한다.
- 로그/콜스택을 Saved에 보존하고 에디터 종료 상태에서 규정한 권한 확장 전체 빌드를 시작했다(12 actions). 완료·재실행·그래프 배선·시각 비교는 아직 대기다. 워핑 단위를 완료 처리하지 않는다.
- 정상 전체 빌드가 12 actions·88.71초·Succeeded로 종료됐다. 사용자에게 에디터 재실행과 AG/S 열기만 요청했다. AG와 편집 전 백업 SHA256 일치. 재실행 후 정상 로드·기존 구면 회귀·수동 배선·PIE는 아직 대기다.
- 사용자 재실행·AG/S 정상 열기 확인 후 MCP 재접속과 `IsPIERunning=false`, 두 에셋의 재컴파일 성공 로그를 확인했다. 기존 구면 수치 회귀 `LootNPop.Animation.UAFSpike.SphericalRootFinalization` 1/1 성공(0.016초), 오류·경고 0. 기본 Strafe는 아직 미배선·미실행이므로 그 검증으로 확대하지 않는다. 수동 배선 단계로 이어간다.
- 사용자에게 Passthrough가 생성 메뉴에 없다는 보고를 받고 안내를 정정했다. 실제 우클릭 메뉴의 `Animation → Trait Stack`을 읽었고, 빈 스택 선택 후 Details `Add Trait → Passthrough`가 기본 trait를 넣는 경로임을 엔진 소스로 확인했다. 메뉴 조회만 했으며 그래프 수정·저장 없음. Slate observer 두 개를 해제했다.
- 사용자 스택 생성 후 세 trait 순서·미연결 상태를 화면/Details로 확인했다. IK Foot Bones 배열이 Details에서 편집 가능한 것도 읽었다. 설정값 변경·포즈 배선은 하지 않았고 수동 설정/연결을 요청하는 단계다. 생성 중 AG 컴파일 성공과 엔진 Strafe 메타데이터 EditCondition의 `f` 파싱 오류를 구분해 Phase05 §14에 남겼다. Slate observer 해제.
- 사용자 설정/배선 완료 후 화면에서 포즈 연결·보간 8·임계값 135·기본 발/루트 모드·척추 5개를 확인했다. 다리 필드 전체 재조회는 하지 않았다. AG 컴파일·저장 성공 로그, All Saved, PIE 종료 상태 확인. 런타임 CVar 재조회 후 DebugLocomotion 2와 북극 약 10초 첫 수동 PIE를 요청한다. 아직 실제 Strafe 평가·시각 비교 검증 전, 단위 미완료다. Slate observer 해제.
- 사용자 첫 실행 정상 보고. 실제 UAF Strafe 각도·알파와 Steering 입력 준비를 확인했다. 고정 로그 스냅샷 230개 Strafe 표본(알파 비영 155개)의 각도 −57.687~67.899°, 230개 Root 구간 누적/출력 최대 거리 30cm·Yaw 90°·Tilt 0.000°다. 표본/구간과 매 프레임 측정 의미를 Phase05 §14에 구분했다. 로그/JSON은 `Saved/AnimationOverhaul/2026-10-25_StrafeFirstRun/`에 보관했다. 다음은 녹화·0/1 수동 비교·점프, 일시정지 후 task 순서 조회다. 단위 미완료·Go/No-Go 보류.

### 0/1 평가 기록 재개·PC 재시작 후 복원 확인(2026-10-26)

- 10월 25일 사용자 비교 평가는 두 모드 모두 자연스럽고 1이 약간 낫지만 큰 체감 차이는 없음이다. 재시작 뒤 저장 녹화 `20261025_184053031.utrace`(233,145,933바이트), 기존 C++/AG/DLL·기동 시 AG/S 컴파일 성공과 MCP 연결을 확인했다. PIE는 종료 상태이며 새 실행·빌드는 하지 않았다.
- 이전 전체 로그를 Saved에 보존하고 0/1 구간을 명령 시각으로 분리했다. 끔 37개 표본 각도 전부 0°, 켬 43개 중 알파 비영 38개·각도 −65.764~69.866°다. Root 구간의 누적/출력 거리≤30cm·Yaw≤90°·Tilt=0.000°를 확인했다. 집계 1,794/2,062프레임 모두 지상이라 공중 검증으로 쓰지 않는다. 파일·집계 범위·한계는 Phase05 §14가 소유한다.
- 사용자 수동 시점 이동/트랙 선택 후 약 50초 EvaluationProgram의 11개 task를 읽어 기존 준비/Steering → Strafe 입력 준비 → 엔진 기본 Strafe → 진단 → Offset Root → 최종 보정 → 루트 진단 순서를 확인했다. 실제 클립 Sprint Loop F·SampleTime 1.6425. Slate observer 해제, 새 코드/에셋/빌드/녹화 없음.
- 남은 적도 0/1 비교와 켬 상태 공중/착지 검증을 이어간다. 워핑 단위·5-1 미완료, 5-2 미착수·Go/No-Go 보류, 다음 구현 단위 착수 전 사용자 확인 유지.

### 적도 0/1·공중 해제/착지 복귀 확인, Strafe 비교 단위 완료(2026-10-26)

- 사용자 적도 이동·점프/착지 평가는 둘 다 정상·차이는 작음이다. 최종 `20261026_172123536.utrace`(189,181,212바이트), 고정 공급 로그/JSON(`Saved/AnimationOverhaul/2026-10-26_StrafeEquator/`)을 확인했다. 앞선 짧은 두 녹화와 구분했다.
- 끔 24표본은 alpha/각도 전부 0, 켬 42표본 중 알파 비영 25·각도 −76.412~56.725°다. 끔 1,203프레임은 모두 지상, 켬 2,120프레임 중 공중 276이다. 누적/출력 거리≤30cm·Yaw≤90°·Tilt=0.000°, 구간 끝 위도는 모두 약 ±2.4° 이내다. 표본/진단 창·경계·동일 입력 A/B가 아닌 한계는 Phase05 §14에 남겼다.
- 공중/착지 7쌍을 확인했다. 공중 표본 6개는 alpha/각도 0이고, curve=0.631이 남은 표본도 0이었다. 착지 후 alpha=1·angle=−6.786° 복귀를 확인했다. 이번 기록은 짧은 점프이며 긴 체공으로 확대하지 않는다.
- **기본 Strafe 비교 단위 완료.** 기존 ABP 완전 동등성·최종 Go는 미판정이다. 다음 발 배치 Control Rig UAF 연결 단위는 제안만 하고 사용자 진행 확인을 받는다. 5-1 미완료·5-2 미착수·Go/No-Go 보류. 로그 분석·문서 갱신 외 추가 코드/에셋/빌드는 없고 Slate observer를 해제했다.

### 세션 종료·커밋 인계(2026-10-26)

- 사용자 요청으로 다음 발 배치 구현은 시작하지 않고 Current의 종료 인계를 현재 상태로 교체했다. 다음 첫 행동·승인 경계·기존 CR 계약·에디터/CVar 상태·로컬 녹화/로그/백업·조회 경로·Live Coding 크래시와 정상 빌드 주의·커밋 범위를 정리했다.
- 인계 시 MCP 읽기 조회에서 PIE 활성, Steering/StrafeWarping/OffsetRootBone.Enable=1·DebugLocomotion=2를 확인했다. 에디터/PIE 종료나 새 녹화·배선은 하지 않았다. 최신/북극 녹화와 로그·JSON·AG 백업의 존재·파일 크기를 확인했다. Saved는 git 제외이며 다른 체크아웃에 따라가지 않는다.
- 최근 커밋 메시지의 `feat: 한국어 요약`과 선택적 한국어 불릿 본문 형식을 확인했다. C++ 4·AG 1·문서 4의 9파일만 커밋 대상으로 제한하고 기존 맵/데이터 4개는 보존·제외한다. diff 공백 검사와 대상 검토를 수행한다. 구현 변경이 없어 기존 빌드·수치 회귀·수동/기록 검증을 재실행하지 않는다.

## 발 배치 최소 연결·검증 단위 완료(2026-10-26)

- 신규 Control Rig(v2) + Passthrough Blend에 기존 구면 보정 CR을 참조하고 지상 알파·접지 법선·착지/재활성 리셋·base 이동/텔레포트 입력을 연결했다. v2 UI 등록 누락은 기존 엔진 Export/Import API로 핀을 생성해 처리했고 임시 Legacy는 제거했다. 엔진·추가 런타임 trait·MCP 도구 변경은 없다.
- C++3파일의 전체 Editor Development 빌드(8 actions·43.72초), AG/S Compile·Save, 실제 링크11개 대조를 통과했다. 이후 C++ 변경은 없으므로 빌드를 반복하지 않았다.
- 사용자 북극 경사0/1 평가: 튐 없음, 1의 발 접지가 훨씬 자연스러움. 적도 여러 방향 걷기/달리기/점프/정지도 어색함 없음. 움직이는 발판도 시각적으로 어색함 없음 보고.
- 경사/적도 녹화 `20261026_201850667.utrace`(363,654,323바이트), 발판 `202321824`(28,886,782)·`202342105`(11,019,442)를 확인했다. 녹화 시작/종료·과거 task/입력이 정상으로 남아 있어 추가 녹화 없이 검증했다. 이후 새 PIE로 빈 Rewind UI가 된 상태를 저장 실패와 구분했다.
- 경사/적도 기록은 공중 진입/착지7쌍, CR 앞 루트 거리≤30cm·Yaw≤90°·Tilt0.000°였다. 적도 창 끝 위도−1.72~2.83°·41표본/1,872프레임(공중143). 오류 검색0과 기존 Foley 경고480을 구분했다. 진단 창의 경계/분모·CR 앞 측정 한계는 Phase05 §15가 소유한다.
- 직접 Rewind 조회: 35초 공중 작업10개에는 CR 없음, 40초 지상 작업12개 마지막에 Control Rig task가 최종 구면 보정/진단 뒤에 있음. 첫 발판 녹화5초 Native Properties는 OnGround/OnMovingBase/Enabled=true·AllowFootPinning=false·AllowSlopeWarping=true로 핀 고정/루트 감쇠 억제를 확인했다.
- 짧은 발판 녹화는 지상472/138프레임이며 Moving209/0이다. 발판 공중/착지의 직접 녹화 근거·큰 회전 base(최대 회전0.0026°)·ForceReset=true 정확한 한 프레임·CR 뒤 전체 본 수치 대조는 남긴다. 관찰/코드/배선과 직접 기록 증거를 구분한다.
- Current를 최신 인계로 교체하고 실행서§15·Roadmap을 갱신했다. 이번 최소 연결 단위는 완료지만 5-1 미완료·5-2 미착수·Go/No-Go 보류다. 다음 제안은 ResetRoot/CopyBone 잔여 계약 대조이며 사용자 진행 확인 전 착수하지 않는다.
- 기존 사용자 맵/데이터4개는 보존·이번 범위에서 제외했다. TestMap03 테스트 GameMode 임시 설정은 유지하며 원래 Override 구체 값은 5-1 종료 복원 전에 사용자 기준값을 확인한다. 커밋은 실행하지 않았다. 조회 후 Slate observer를 해제했다.

## 발 배치 단위 새 세션 인수인계·단일 커밋 정리(2026-10-27)

- 사용자 요청으로 Current의 인수인계 상태·다음 세션 시작 문구·승인 경계를 확정했다. 다음 ResetRoot/CopyBone 잔여 계약 대조는 아직 진행 확인 전이며 조사·구현하지 않았다. 5-1 미완료·5-2 미착수·Go/No-Go 보류를 유지한다.
- 최근 커밋의 `feat: 한국어 요약` 제목과 한국어 불릿 본문 형식을 따른다. 이번 C++3·AG1·문서4의 총8파일을 단일 커밋 범위로 지정했다. 기존 사용자 맵/데이터4개는 보존·제외하며 TestMap03 임시 GameMode도 그대로 둔다. 푸시는 요청되지 않았다.
- 구현 변경 없이 인수인계만 보완하므로 기존 전체 빌드·AG/S 컴파일·수동/녹화 검증을 반복하지 않는다. 마지막 에디터 상태는 10월26일 확인값으로 구분한다. 로컬 Saved 근거·백업은 git 제외라 다른 체크아웃에 따라가지 않으며 다음 세션은 Current의 경로를 사용한다.

## ResetRoot/CopyBone 잔여 계약 대조(2026-10-27, 단위 완료)

- 새 Phase 5 진행 요청으로 다음 대조 단위에 착수했다. 실제 기본/숨김 ABP의 CopyBone 복사3종=false와 ResetRoot 알파1을 조회했고 기본 ABP 핀의 상수값·미연결도 확인했다. CopyBone은 엔진에서 즉시 반환하는 무동작이다.
- 현재 Chooser 직접 PSD52개의 GASP 시퀀스319개와 별도 공중 Fall_Loop이 모두 RootMotion 켬·RefPose 잠금·ForceRootLock 켬이다. UAF 압축 해제도 같은 설정으로 루트를 잠그며 프로젝트 Steering은 루트모션 속성만 변경한다. 기본 Strafe의 Preserve=true 경로는 루트를 보존하므로 현재 구성의 루트 복원만을 위한 별도 ResetRoot는 불필요하다는 정적 판정이다.
- 사용자가 AG의 최신 `Preserve Original Root Rotation`이 체크돼 있음을 확인해 §14의 true 기록과 대조를 마쳤다. 별도 노드 추가 없이 대조 단위를 완료했다. 전체 본 포즈 동등성은 판정하지 않는다. 특히 기존 OW→ResetRoot의 자식 이동 보존과 UAF의 자식 회전 보정은 다르며, 별도 ResetRoot 추가만으로 해소할 차이가 아니다(실행서§16).
- C++/에셋 변경·저장·빌드·PIE·새 녹화 없이 읽기 조사만 수행했고 기존 사용자 변경4개를 보존했다. 시작 시 PIE=false, 조회 Slate observer 해제. 로컬 근거는 `Saved/AnimationOverhaul/2026-10-27_ResetRootCopyBone/`이다.
- 다음 제안은 추가 전환 표본(Pivot·Sprint Turns·GaitTransitions) 검증이며 사용자 진행 확인 대기다. 5-1 미완료·5-2 미착수·Go/No-Go 보류다. 이번 요청으로 새 커밋/푸시는 실행하지 않는다.

## 추가 전환 표본 검증 착수(2026-10-27, 진행 중)

- 사용자 진행 확인으로 기존 로그4개를 탐색해 Sprint Turns(10월25일 녹화 약125.51초)·GaitTransitions(같은 녹화 약71.93초)·Run RL Pivot(10월26일17:15 녹화 약79.38초)의 공급 배열 표본을 찾았다. 상세 시각·배열·한계는 실행서§17과 Saved의 `2026-10-27_AdditionalTransitions/Targets.json`에 기록했다.
- 시작 PIE=false·DebugLocomotion=0. 새 코드/에셋·빌드·PIE·녹화 없이 기존 기록으로 검증한다. Sprint Turns는 착지 직후 저속 표본이며, Pivot도 저속 방향 전환이다. 이를 지속 고속 전환 검증으로 확대하지 않는다. 숨김 ABP 로그와 실제 UAF 선택을 구분한다.
- 첫 Sprint Turns 시점의 UAF AG Native Properties 표시를 사용자에게 요청했다. 현재 직접 수신/선택 조회는 대기 중이며 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### Sprint Turns 직접 조회(같은 날)

- 사용자 수동 스크럽 후 UAF AG의 Sprint Starts+Sprint Turns 수신 순서·InterruptOnDatabaseChange·BlendTime0.5를 공급 로그와 대조했다. 실제 Active Pose는 Start F Lfoot(표시 시간0.07·비용0.168), 표시된 Turn090 L Rfoot 후보 비용은2.293이었다. Turn 후보 검색까지 확인했고 Turn 클립 재생으로 확대하지 않는다. 저속 착지 직후·과거 구성 표본 한계와 인접 입력 해제의 직접 조회 미실시를 실행서§17에 남겼다.
- GaitTransitions 입력 시점(같은 녹화 약71.9~72.1초)의 AG 표시를 요청했다. Pivot도 남아 이번 단위는 미완료다. 코드/에셋·빌드·새 PIE/녹화 없이 읽었다. 로컬 증거는 `Saved/AnimationOverhaul/2026-10-27_AdditionalTransitions/SprintTurnsInspection.json`이다.

### GaitTransitions 수신 확인(같은 날, 선택 조회 대기)

- AG 수신 배열 GaitTransitions→Run F Loops·DoNotInterrupt·BlendTime0.5가 공급 로그와 일치했다. 실제 선택은 아직 미조회다. 자동 트랙 선택 뒤 Details가 입력 화면에 머물러 사용자에게 시간 유지·UAF Pose Search 직접 선택을 요청했다. 로컬 입력 근거는 `GaitTransitionsInspection.json`이며 전체 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### GaitTransitions 실제 검색 확인(같은 날)

- 사용자가 UAF Pose Search를 직접 선택했고 Active/Continuing Pose가 모두 Sprint Loop F(비용2.276), 표시된 Sprint→Run Lfoot 후보 비용3.537, DoNotInterrupt·검색 경과0.0·재생시간1.338866초를 읽었다. 전환 입력/후보 검색과 이전 포즈 유지를 확인했으며 전환 클립 재생으로 확대하지 않는다. 근거는 `GaitTransitionsSearch.json`이다.
- 남은 Run Pivot 표본을 위해 `20261026_171514977.utrace` 약79.35~79.55초의 AG 세 DB 표시를 요청했다. 이번 단위 미완료·5-1 미완료·5-2 미착수·Go/No-Go 보류를 유지한다.

### Run Pivot 대조·추가 전환 표본 단위 완료(같은 날)

- Run RL Starts→Loops→Pivots 세 DB·InterruptOnDatabaseChange·BlendTime0.5의 AG 수신값이 공급 로그와 일치했다. UAF Active Pose는 RL Start Lfoot(비용0.631·재생시간0·검색 경과0)였다. 원래 경계 프레임으로 복귀한 뒤 Pivot LL→RL Lfoot 후보 비용2.648·시간0.87을 읽었다.
- 후보 검색은 DB명이 아닌 애니 에셋 이름을 대상으로 하므로 최초 안내 `Pivots` 대신 `Pivot`(단수)으로 정정했다. 탐색 중 뒤 시점의 진행 포즈 값은 원래 표본 근거에 합치지 않았다. 상세 UI 경위·원본 근거는 실행서§17과 Saved의 RunPivot JSON3개가 소유한다.
- 세 추가 표본의 입력/정책 전달과 실제 후보 평가 대조는 완료했다. 실제 결과는 Sprint Start·Sprint Loop 유지·Run RL Start이며 목표 전환 클립 재생/지속 고속 품질을 통과한 것은 아니다. 최신 발 배치 전 녹화·저속 경계·인접 해제 프레임 직접 조회 미실시의 한계를 유지한다.
- 코드/에셋 변경·저장·빌드·새 PIE/녹화 없이 문서와 Saved 근거만 갱신했다. 종료 PIE=false·Slate observer 해제. 다음 제안은 큰 회전 base 및 발판 점프/착지 검증이며 진행 확인 대기다. 5-1 미완료·5-2 미착수·Go/No-Go 보류다. 기존 사용자 에셋4개는 보존했고 새 커밋/푸시는 하지 않았다.

## 큰 회전 base 및 발판 점프/착지 검증 착수(2026-10-27)

- 사용자 진행 확인으로 기존 패널 C++·배치 데이터를 읽었다. 열린 경로는 중력 수평 끝점 자세 보간이며 독립 Yaw 회전 설정은 없다. 소스 배치 예상 끝점 Up 차이는 FacadeFerry29.336°·SkyFerry14.829°·Lift0°로 계산됐으나 실제 런타임 측정과 구분한다.
- 기존 `maxBaseDeg`는 Mover가 Actor에 적용한 based 이동 델타이며 패널 자체의 전체 회전량이 아니다. §15의 작은 진단값을 실제 패널 회전 부족으로 일반화하지 않도록 실행서§18에 범위를 보완했다.
- 시작 PIE=false·FootPlacement1/MovingBase0/Steering1/Strafe1/Debug0 확인. 사용자에게 1인 PIE·Debug2·PlaceRider·Rewind 녹화로 정지 탑승/점프·동일 발판 착지를 요청했다. 수동 결과 대기이며 새 코드/에셋·빌드·시험 장치는 없다. 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 발판 수동 결과·로그 확인(같은 날)

- 사용자 이상 없음 보고. 새 녹화 `20261027_153613941.utrace`148,240,307바이트·약68.452초에서 점프/착지17쌍을 확인했다. Root 창68개2,661프레임 중 지상/base2,147·공중514이며 CR 앞 거리≤30cm·Yaw≤90°·Tilt0.000°다. 창 경계/분모·기존 Foley58회·종료 직후 LogTrace error64의 범위는 실행서§18에 기록했다.
- 실제 패널 변환의 라이브3표본에서10.896426° 차이를 읽어 패널 자체 회전과 전달 델타0.0023°를 구분했다. 경로 전체나 큰 Yaw 검증은 아니며 `LivePanelRotation.json`에 보존했다.
- 새 녹화 공중26.3초의 UAF EvaluationProgram Tasks 표시를 요청했다. 이후 재착지27.3초를 읽는다. CR task 직접 확인과 회전 범위 판정이 남아 이번 단위·5-1 미완료·5-2 미착수·Go/No-Go 보류다.

### 기존 발판 검증 소단위 완료·큰 Yaw 확대 진행 확인 대기(같은 날)

- 실제 Rewind 공중26.3초는 Fall_Loop(SampleTime0.308793)·작업10개/CR 없음, 재착지27.3초는 작업12개/최종 구면 보정 뒤 마지막 CR 실행이었다. 두 직접 표본으로 해제/복귀를 확인했으며 ForceReset 정확한 한 프레임·17쌍 전부의 task 조회로 확대하지 않는다.
- 첫 창 제외 안정 탑승30창/1,167프레임의 CR 앞 최대 거리0.143cm·Yaw83.46°·Tilt0°였다. 전체 혼합 창 최대30cm와 분모를 구분한다. 새 과거 task가 실제 조회돼 종료 직후 LogTrace error64를 녹화 전체 실패로 단정하지 않는다.
- 기존 발판 점프/착지·실제 회전 범위 검증 소단위는 완료다. 기존 패널의10.896426° 라이브 차이는 경로상 기울기 변화이며90° 이상 Yaw 시험은 아니다. §18 전체·5-1 미완료·5-2 미착수·Go/No-Go 보류를 유지한다.
- 다음 제안은 시험용 BP 메시+기본 RotatingMovement로 큰 Yaw 회전 발판을 구성·검증하는 구현 단위이며 진행 확인 대기다. 승인 전 조사/구현하지 않는다. 종료 PIE=true·Debug2·Slate observer 해제, 새 코드/에셋·빌드·커밋/푸시는 없다. 기존 사용자 에셋4개를 보존했다.

### 큰 Yaw 발판 구현 착수·초기 실행 대기(같은 날)

- 사용자 승인으로 BP_UAFYawTestPlatform 하나를 생성·컴파일·설정 재조회·저장했다. StaticMeshActor/Cube1600×1600×30cm·동적 지형 충돌·Movable·로컬 Yaw30°/초의 기본 RotatingMovement 구성이며 C++/엔진/도구는 추가하지 않았다.
- PIE 전용 Python 스폰은 MakeTransform/Deferred Spawn의 Python 미노출로 두 번 모두 생성 전 실패했다. 스크립트 경로를 중단·제거하고 임시 에디터 Actor 배치로 전환했다. 맵은 저장하지 않았고 정리 대상은 `YawPlatform/EditorPlacement.json`의 정확한 ref다. 상세 실패/배치/명령은 실행서§18이 소유한다.
- 새 1인 PIE에서 기존 TeleportToOctant 명령으로 발판 위에 도착해5초 탑승하도록 요청했다. 현재 스모크 결과 대기·구현 단위/§18/5-1 미완료·5-2 미착수·Go/No-Go 보류다. 시험 뒤 PIE 종료·임시 Actor만 제거해야 한다. 신규 BP 외 기본/사용자 에셋은 저장하지 않았다.


### 큰 Yaw 롱소드 시험·재시작 후 저장 상태 확인(2026-10-27)

- 초기 회전/탑승 정상, 라이브 자세 사이91.274880°·Up 차이0°를 확인했다. 롱소드15초 정지/10초 걷기·멈춤/5초 정지 녹화를 완료했고 사용자 관찰은 이상 없음이다. 총기 카메라 방향 유지에 따른 제자리 회전은 별도 분기다.
- 새 녹화 `20261027_163122363.utrace`41.469초·109,352,237바이트. Root 창41개1,846프레임 전부 지상/base·CR 앞 거리≤30cm/Yaw≤88°/Tilt0°다. 마지막 정지6창 거리0cm/Yaw≤0.16°이며 분모·경고·한계는 실행서§18이 소유한다.
- 사용자 에디터 재시작/AG·임시 Actor 저장 보고 후 PIE=false와 동일 Actor 존재를 확인했다. 기존 사용자 에셋4개 해시는 모두 보존됐고 외부 Actor 파일 하나가 생겼다. AG·HEAD·외부 Actor 백업 후 읽기 전용 그래프 검사 명령 실행을 요청했다. Rewind 약10초의12개 task/마지막 CR는 읽혔으나 열린 파일 확인 대기다.
- 현재는 검사·정리 진행 중이며 §18/5-1 미완료·5-2 미착수·Go/No-Go 보류다. 맵 전체/AG를 자동 원복하지 않으며 다음 구현 단위는 별도 진행 확인을 받는다.


### 큰 Yaw 검증 단위 마감·임시 외부 Actor 정리 완료(2026-10-27)

- 사용자 확인으로 새 녹화 약10초의 UAF12개 task·최종 구면 보정/진단 뒤 Control Rig 실행을 확정했다. 재시작 후 AG 읽기 전용 Export에서도 발 배치 연결11개·공개핀10개·v2/Bool 알파·0.3/0.2초·레이캐스트/IK/루트 회전 보존 설정이 기존 계약과 일치했다. AG는 유지했고 전체 HEAD 의미 동등성을 주장하지 않는다.
- 정확한 임시 Actor만 native 제거(true) 후 빈 조회를 확인했다. 외부 Actor 파일 직접 삭제는 에디터 파일 잠금으로 실패해 반복하지 않았다. 사용자가 해당 빈 패키지만 save_packages로 처리한 뒤 파일 부재를 확인했다. 기존 사용자 에셋4개 해시 모두 일치·PIE=false·Slate observer 해제 상태다. 신규 시험 BP는 유지, 맵 전체 저장/원복·C++/빌드·커밋/푸시는 없다.
- §18은 기존 패널 점프/착지와 큰 Yaw 롱소드 시험 범위 내 완료다. 배치 위도·장비·라이브 회전/녹화 동기화·CR 앞 진단의 한계는 실행서 최종 판정에 남겼다. 5-1 미완료·5-2 미착수·Go/No-Go 보류를 유지하며 다음 제안은 5-1 종료 판정·남은 비교 범위 정리다. 사용자 진행 확인 대기다.


### 새 세션 인수인계·단일 커밋 정리(2026-10-27)

- 사용자 요청으로 다음 구현을 시작하지 않고 인수인계를 작성했다. Current는 완료 이력의 상세 나열을 줄이고 다음 시작점·승인 경계·남은 판정·현재 에디터/CVar·검증 한계·로컬 근거·보존 대상 중심으로 정리했다. 상세 결과의 원본은 실행서§16~§18과 이력에 유지한다.
- 최근 프로젝트 커밋의 `feat: … 검증 및 인계` 제목과 한국어 항목별 본문 형식을 따른다. 커밋 범위는 문서4개·사용자 재저장 후 계약 확인한 AG·신규 Yaw 시험 BP의6파일이다. 기존 사용자 에셋4개·git 제외 Saved 녹화/근거는 포함하지 않는다. 푸시·새 세션 생성·다음 단위 구현은 하지 않는다.
- 다음 세션 제안은 5-1 종료 판정·남은 비교 범위 정리이며 사용자 진행 요청 후 착수한다. 5-1 미완료·5-2 미착수·Go/No-Go 보류를 유지한다. 기존 빌드/녹화는 반복하지 않고 문서 차이 검사와 커밋 범위·보존 해시를 확인한다.

### 5-1 종료 판정·남은 비교 범위 정리 완료(2026-10-27)

- 사용자 Phase 5 재개 요청으로 실행서§2의 최초 조건을 §10·§12~§18의 완료 근거에 대응했다. 최소 폰/입력·PSD 검색·북극/적도 이동·공중 고정·발 배치·moving base의 최소 기능 조건을 충족해 5-1 완료로 판정했다. 대응표와 한계는 실행서§19가 소유한다.
- 본별/동일 프레임 수치 일치·특정 전환 클립 강제 선택·모든 위도/장비/발판 교차 시험을 새 조건으로 추가하지 않는다. 기존 ABP 대비 최신 UAF 최종 포즈 품질·지속 고속/전환과 알려진 워핑 차이의 영향은 5-2의 PSD/최종 포즈 게이트에 남겼다. 최종 일곱 게이트·Go/No-Go는 미판정이다.
- 현재 AG는 직전 검사 백업과 SHA256이 같고, 시작 시 사용자 에셋4개도 직전 정리 해시와 일치했다. C++의 다중 Chooser·공유 입력 공급과 대조했고 새 PIE/녹화·빌드·진단 장치를 만들지 않았다.
- MCP에서 PIE=false·TestMap03의 시험용 GameMode를 확인했다. 사용자가 원래 Override는 `/Game/Gameplay/BP_LNPGameMode`라고 알려줬다. 디스크 맵을 `Saved/AnimationOverhaul/2026-10-27_UAF51Closure/TestMap03_BeforeGameModeRestore.umap`에 백업한 뒤 WorldSettings.DefaultGameMode만 복원·재조회했고 TestMap03 패키지만 저장 성공했다. 맵 전체 원복·다른 에셋 저장은 하지 않았다. 복원 저장한 맵의 해시는 이전과 달라지며 기존 사용자 변경이 섞인 상태를 유지한다.
- **이번 단위·5-1 완료, 5-2 미착수·Go/No-Go 보류.** 다음 제안은 5-2 몽타주·ANS 최소 연결 경로 조사다. 사용자 진행 확인 전 해당 조사/구현을 시작하지 않는다. 이번 변경은 문서4개와 GameMode 복원 맵이며 커밋·푸시는 하지 않았다.

### 5-2 몽타주·ANS 최소 연결 경로 조사 완료(2026-10-27)

- 사용자 5-2 시작 요청으로 기본 몽타주/노티파이의 메시 선택·초기화·틱과 기존 계약을 대조했다. FUAFMontageComponent는 포즈 출력 locator와 별개로 Owner의 첫 SkeletalMeshComponent를 선택하고 일반 평가 끔을 요구한다. UAFLayering 몽타주 데이터도 같은 컴포넌트를 사용한다. 일반 ABP 공급을 유지한 현재 구성에 Montage Trait만 추가하는 경로는 맞지 않는다.
- MCP에서 PIE=false, 시험 BP의 네이티브 LNPPlayerCharacter 부모와 메시3개의 AnimClass/일반 평가 설정을 확인했다. CDO 메시 열거는 WeaponMesh→AnimSourceMesh→VisualMesh였으며 이를 실제 PIE 첫 메시로 확대하지 않는다. 컴포넌트 순서에 기대지 않고 명시적 선택이 필요하다.
- ANS dispatcher도 별도 첫 메시 참조를 사용한다. 이동 취소 ANS의 MeshComp.GetAnimInstance와 MeleeHitWindow의 메시 키·무기 본 조회를 대조해 몽타주/GAS/ANS의 원본 메시 일치와 중복 발화 방지를 성공 기준으로 정했다. End를 큐에 넣는 소스의 존재는 Stop/교체/시스템 종료에서 정확히 한 번 실행됐다는 근거가 아니다.
- 엔진을 수정하지 않는 후보로 시험용 C++ 폰의 virtual 메시 선택과 원본 몽타주 전용 평가·명시적 B0 공급을 정리했다. Native 슬롯 초기화·Graph 참조 생성 순서·프레임당 단일 공급/몽타주 전진·숨김 OffsetRoot를 대신할 안전한 UAF 루트 기준 공급까지 다음 구현에 필요하다. 실행 가능성을 보장하거나 전체 마이그레이션을 승인하지 않는다.
- 상세 대응표·후속 구현/실행 성공 기준은 실행서§20, 조회 근거는 `Saved/AnimationOverhaul/2026-10-27_UAF52MontageInvestigation/Inspection.json`이다. 이번 조사 단위 완료·5-2 진행 중·몽타주/ANS 실행 미검증·Go/No-Go 보류. 문서4개만 갱신하고 코드/에셋·저장·PIE/녹화·빌드·커밋/푸시는 없다. 다음 시험용 원본 메시 선택·입력 공급 분리 최소 구현은 사용자 진행 확인 대기다.

### 원본 메시 선택·입력 공급 분리 소스/전체 빌드 완료(2026-10-27, 검증 진행 중)

- 사용자 추가 진행 요청으로 시험 전용 LNPUAFSpikePlayerCharacter와 명시적 AnimInstance 공급 경로를 구현했다. 정확한 SkeletalMeshComponent 질의만 원본 메시로 고정하고 일반 평가를 끈 원본의 Native 초기화/슬롯 그룹과 프레임당 단일 B0 공급을 유지하는 구성이다. UAF PrePhysics는 폰의 공급 틱 뒤에 실행한다. 기본 캐릭터·플레이어·적/기존 ABP·엔진은 수정하지 않았다.
- 루트 평가 task가 완료한 누적 월드 루트를 모듈 피드백 컴포넌트에 기록하고 게임 스레드가 UAF ReadComponent 및 컴포넌트 잠금으로 사본을 읽는다. 기존 OffsetRootTransform/bHasOffsetRoot를 갱신한 뒤 B0를 실행해 숨김 그래프 제거 후에도 FutureFacing/Aim의 루트 기준을 공급하는 구현이다. CR 뒤 전체 포즈가 아니라 CR 앞 UAF 누적 상태다. 런타임 성공은 아직 미검증이다.
- 제한 환경 Get-Process의 빈 결과로 종료를 안내했으나 권한 확장 실행 전 검사에서는 에디터가 살아 있어 Build.bat 시작 전에 중단됐다. 안내를 정정하고 사용자 에디터 종료 확인을 받은 뒤 빌드했다. 첫 실제 전체 빌드는9 actions·66.19초·C1083(UAF/Internal include 경로 누락)으로 실패했고 경로 추가 뒤 **14 actions·98.10초·Succeeded·종료 코드0**이다. Live Coding을 쓰지 않았다.
- `Saved/AnimationOverhaul/2026-10-27_UAF52SourceDriver/`에 시험 BP/AG 백업·시작 에셋 해시·첫 실패 요약·성공 빌드 로그/요약을 보관했다. 첫 실패 요약은 도구 출력 정리이며 원본 UBT 로그 사본이라고 주장하지 않는다.
- 사용자에게 에디터 재실행·AG/S 정상 열기만 요청했다. 시험 BP 부모/설정 변경·PIE·수치 회귀는 아직 수행하지 않아 **이번 단위 미완료·5-2 진행 중·Go/No-Go 보류**다. native Montage Trait/ANS 실행 판정은 후속 단위이며 이번 단계에서 공격 포즈 통과를 주장하지 않는다. 상세 후속 절차는 실행서§21이 소유한다.

### 재실행·시험 BP 설정/수치 회귀 완료, 첫 스모크 대기(같은 날)

- 사용자 AG/S 정상 열기 확인 뒤 PIE=false에서 시험 BP를 LNPUAFSpikePlayerCharacter로 재부모화했다. 원본 일반 평가=false·기존 AnimClass·항상 틱을 재조회했고 warnings_as_errors=true 컴파일 호출 정상 종료·해당 BP만 저장 성공을 확인했다. 기존 S/AG/Chooser 기본 참조도 그대로다.
- 구면 루트 수치 회귀는1/1 성공·0.017769903초·오류/경고0이다. 맵 원래 GameMode를 확인한 뒤 BP_GameModeUAFSpike를 메모리에서만 임시 지정했다. Content의 사용자 에셋4개 해시는 §19 종료 후와 같고 AG도 수정 전 백업과 같다. 에디터의 Saved/Autosaves 맵 사본은 Content 맵 저장과 구분한다.
- 1인 PIE·DebugLocomotion2·약15초 정지/이동/큰 룩 회전/점프·착지, 공격/녹화 없이 PIE 유지와 관찰 보고를 요청했다. 아직 사용자 결과 대기이며 새 런타임 통과를 주장하지 않는다. 근거는 §21과 SourceDriver 폴더의 BlueprintSetup·RootRegression·AfterSetupHashes JSON이다.

### 공급 분리 기능 스모크·적도 확인, 호출 순서 오류 후속 조사 대기(2026-10-28)

- 사용자는 첫 스모크·AG/S 미리보기 탭이 닫힌 상태의 반복 점프·적도 이동/정지/큰 룩 회전/점프·착지 모두 이상 없음으로 보고했다. 후속 설명으로 닫기 요청 당시 이미 닫혀 있었고 별도 조작은 없었음을 정정했다. 실제 virtual 메시 조회는 AnimSourceMesh였고 일반 평가=false·명시적 공급/루트 피드백 serial 갱신·bHasOffsetRoot=true를 확인했다. 최신 적도 표본은 input41537/feedback41536이다.
- 첫 표본은 남극 부근139창/7,653진단 프레임·점프/착지15쌍이다. 두 Python 표본 사이 반복 점프15쌍, 이후 적도 도착 뒤13쌍을 확인했다. 적도171창/8,615프레임의 창 끝 위도는−3.53~1.49°이며 CR 앞 거리≤30cm·Yaw≤90°·Tilt0°다. 분모·표본 범위/원시 근거와 매 게임 프레임 공급률 미측정 한계는 실행서§21이 소유한다.
- 첫 스모크에서 Motion Matching PreUpdate/PostEvaluate 순서 오류1회가 있었다. Actor 식별자가 없어 PIE/미리보기 발생원과 원인이 미확정이다. 이후 반복 점프/적도에서 추가 오류0이지만 수정 완료로 판정하지 않는다. LogAnimation Error·드라이버 오류·assertion/ensure/fatal은0이며 기존 Foley 경고는 남는다.
- 공급 분리 구현·사용자 기능 스모크 완료, 전체 단위의 오류 해소 판정은 보류다. 실제 GAS ActorInfo/UAF 몽타주/ANS 대상·몽타주 전진은 연결 후 검증한다. 5-2·최종 게이트·Go/No-Go도 미판정이다. 다음 제안은 native Montage 연결 전 호출 순서 오류 원인 범위를 좁히는 조사이며 사용자 진행 확인 대기다.
- 새 소스/에셋 변경·빌드·녹화·커밋/푸시 없이 문서4개와 git 제외 로그/요약을 정리했다. 현재 PIE=true·Debug2·AG/S 탭 닫힘이다. 에디터 메모리 GameMode는 시험용이며 원래 복원값 BP_LNPGameMode를 유지한다. 최신 디스크 맵 해시는 §19/빌드 전 기준과 다르므로 현재 사본을 DriverSmoke에 보관했고 맵 저장/전체 원복은 하지 않았다. 다른 사용자 데이터3개와 AG는 기준 해시와 같다. 맵 차이 경위는 미확정이다.

### 호출 순서 오류 원인 범위 조사·빠른 재점프59쌍(2026-10-28)

- 사용자 승인으로 §22 조사를 진행했다. 사용자가 AG/S 닫기 요청 당시 이미 닫혀 있어 아무 조작도 하지 않았다고 설명해 이전 기록을 정정했다. 탭 닫기 전/후 비교나 탭 닫기 효과로 해석하지 않는다. 최초 오류 시점의 탭 상태를 별도로 확정하지 않았다.
- 설치 UE 소스에서 Bool TrueChild가 가중치≤0.00001일 때 갱신에서 제외되지만 BlendSmoother는 가중치≤0에서만 자식을 제거하고 평가 순회는 유효 자식을 방문함을 확인했다. 현재 지상 MM/공중 Fall_Loop·0.25초 HermiteCubic·AlwaysUpdateTrueChild=false와 결합하면 갱신 없이 MM PostEvaluate가 호출될 수 있는 경로다. 최초 오류 이륙 후0.231초와 양립하지만 실제 당시 가중치/인스턴스가 없어 원인 확정은 아니다.
- 시작 시 PIE=false를 확인해 수동 재현을 요청했고 사용자가 새1인 PIE에서 방향을 바꾸며 빠른 점프를 완료했다. 캡처 구간 점프/착지59쌍·PoseSearch 오류0·LogAnimation Error/드라이버 오류/assertion/ensure/fatal0이다. 사용자 답변은 조작 완료 보고이며 별도 시각 품질 판정으로 확대하지 않는다. 새 녹화/공격은 없다.
- **조사 단위 완료·최초 오류 미해결·5-2/Go-NoGo 보류**다. 다음 제안은 기존 trait의 PreEvaluate에서 MM 검사 bool이 이미 true인 경우에만 모듈/Owner·입력 serial·자식 가중치/목적을 남기는 조건부 진단 구현이며 사용자 진행 확인 대기다. 엔진 수정·AlwaysUpdate 우회·블렌드 정책 변경은 하지 않았다.
- 근거는 `Saved/AnimationOverhaul/2026-10-28_UAF52OrderInvestigation/`의 SourceAnalysis·OutgoingLinks·RapidJumpBaseline 로그/요약과 실행서§22다. AG는 수정 전 해시와 같다. 문서4개와 로컬 근거만 정리했고 코드/에셋·빌드·커밋/푸시는 없다. 종료 상태는 새 PIE=true·Debug2·AG/S 탭 닫힘이다.

### 조건부 진단 소스 추가·에디터 종료/빌드 대기(2026-10-28)

- 사용자 승인으로 기존 LNPSphericalOffsetRootTrait.cpp의 PreEvaluate에 WITH_EDITOR·DebugLocomotion2·MM 검사 bool이 이미 true일 때만 출력하는 진단을 추가했다. host 전체 경로·module/MM/root 주소·현재/직전 적용 serial·지상/목적 자식·가중치/관련성·DB/애니를 기록하며 기존 상태/평가 전달은 바꾸지 않는다.
- 타입/레이아웃·엔진/에셋·블렌드 정책·AlwaysUpdate는 변경하지 않았다. 변경 직전 cpp와 에셋 해시는 `Saved/AnimationOverhaul/2026-10-28_UAF52OrderDiagnostic/`에 보관했다. 정상 출력과 오류 프레임 대응은 아직 런타임 미검증이다.
- 이전 Live Coding 크래시 이력에 따라 전체 빌드를 선택하고 사용자에게 PIE/에디터 종료 확인을 요청했다. **종료 확인 전 빌드하지 않으며 현재 구현 단위 미완료·최초 오류 미해결·5-2/Go-NoGo 보류**다. 빌드/재실행 뒤 수동 스모크를 요청하는 후속 절차는 실행서§23이 소유한다.

### 조건부 진단 전체 빌드 완료·재실행 대기(2026-10-28)

- 사용자 종료 회신 뒤 권한 확장 환경의 에디터 프로세스 검사에서 부재를 확인했고 기준 Development 전체 빌드를 실행했다. 진단 cpp 컴파일·lib/dll 링크를 포함해 **4 actions·20.50초·Succeeded·종료 코드0**이다. 새 실패/수정·Live Coding은 없다. 로그와 요약은 OrderDiagnostic/Build.log·BuildSummary.json이다.
- 사용자에게 에디터 재실행·TestMap03 준비 확인을 요청했다. AG/S 탭과 PIE는 아직 열지 않도록 안내했으며 재연결 후 시험 GameMode를 확인한다. 진단 런타임 경로/오류 대응은 아직 미검증·이번 단위 미완료·최초 오류 미해결·5-2/Go-NoGo 보류다.

### 진단 빌드 재실행 설정 확인·수동 점프 대기(2026-10-28)

- 사용자 재실행 보고 뒤 PIE=false·열린 에셋 없음·DebugLocomotion0·TestMap03의 BP_GameModeUAFSpike를 새로 읽었다. 이번 재연결에서 GameMode 변경/저장은 없었다. 재시작 후 이미 시험 값이므로 이전 메모리만 임시 지정 기록을 현재 디스크 복원 상태로 확대하지 않는다.
- 초기 평가부터 기록하도록 PIE 시작 전에 DebugLocomotion2를 입력한 뒤 새1인 PIE·약45초 방향 전환/착지 직후 재점프·이상 여부 보고/PIE 유지를 수동 요청했다. 새 AG/S 탭·공격·녹화는 불필요하다. 근거는 OrderDiagnostic/RestartSetup.json이며 현재 결과 대기·이번 단위 미완료다.

### 진단 발화·동일 오류의 실제 PIE 재현 확인, 진단 구현 단위 완료(2026-10-28)

- 사용자는 이상 없음으로 보고했다. 드라이버 초기화부터 캡처까지 점프/착지133쌍을 확인했다. 조작 전후 대기까지 포함한 구간이므로 요청45초의 정확한 분모로 확대하지 않는다.
- UTC02:20:42.056·프레임 접두사[967]에서 새 진단1회 직후 기존 Motion Matching PostEvaluate 오류1회가 연속 출력됐다. host는 실제 PIE BP_PlayerUAFSpike_C_0.UAFLocomotion이다. input1691/previousApplied1690·ground0·destination1/child0·weight0.00000220537186·relevant0을 기록했다.
- §22의 유력 후보였던 양수/임계값 이하의 지상 MM 자식이 갱신에서는 제외되고 평가에는 남는 경로를 이번 재현에서 확인했다. 최초 무진단 이벤트의 인스턴스를 소급 특정한 것은 아니지만 동일 오류가 실제 PIE에서 해당 조건으로 재현됐다. 관련 로그/수치·전환 간격과 한계는 실행서§23이 소유한다.
- 전체 빌드 성공에 이어 진단의 실제 조건 분기/엔진 오류 대응을 확인해 **진단 구현/검증 단위 완료**다. 오류는 수정하지 않았고5-2/Go-NoGo는 보류다. 다음 제안은 자식 수명/공중 블렌드 정책을 유지하는 프로젝트 쪽 최소 보정 구현·재검증이며 사용자 진행 확인 대기다.
- LogAnimation Error·드라이버 오류·assertion/ensure/fatal0, CR 앞 루트 제한 유지와 사용자 에셋4개/시험 BP/AG 해시 보존을 확인했다. 새 녹화·추가 소스/에셋 변경·저장·커밋/푸시는 없다. 종료 상태 PIE=true·Debug2다. 근거는 OrderDiagnostic의 DiagnosticSmoke 로그/요약·RootWindows·MatchedIncident·AfterSmokeHashes다.

### Bool 갱신 보정 소스 준비·에디터 종료/빌드 대기(2026-10-28)

- 사용자 승인으로 §24 최소 보정을 시작했다. IUpdateTraversal이 스택의 각 트레이트를 따로 방문함을 확인해, 신규 LNP Bool Blend Update가 기본 Bool이 제외한 유효 자식만 보충하도록 구현했다. 기본과 같은 가중치/관련성/AlwaysUpdate 및 BlendingOut/NewlyRelevant 조건으로 중복 등록을 피한다. 수명/블렌드 정책·엔진을 바꾸지 않았다.
- 신규 header/cpp와 UAFAnimGraph/Internal include 경로를 준비했다. 실제 보충 구간은 기존 Debug2의 UAFBoolUpdate 로그로 확인하며 이전 진단/엔진 오류는 유지한다. AG/Build.cs 백업·에셋 해시는 BoolUpdateFix 폴더에 보관했고 아직 AG는 수정하지 않았다.
- 시작 시 PIE=false였다. 새 타입 전체 빌드를 위해 사용자 에디터 종료 확인을 요청했고 회신 전 빌드하지 않는다. 재실행 뒤 기존 Bool 노드의 Add Trait로 한 번 추가하는 수동 조작이 효율적임을 바로 안내했다. 이번 단위는 빌드/연결/실행 검증 전으로 미완료·5-2/Go-NoGo 보류다.

### Bool 갱신 보정 전체 빌드 완료·AG 수동 추가 대기(2026-10-28)

- 사용자 종료 회신 직후 아직 종료 중인 LootNPop 프로세스가 있어 첫 호출은 Build.bat 전에 차단됐다. 프로젝트 명령행 확인/짧은 대기 뒤 프로세스 부재를 확인했고 강제 종료 없이 실제 빌드를 진행했다. 새 타입 UHT와 cpp 컴파일·lib/dll 링크를 포함해 **15 actions·58.89초·Succeeded·종료 코드0**이다. Live Coding·컴파일 실패/소스 재수정은 없다.
- 재실행 후 기존 Bool 노드의 Add Trait에서 LNP Bool Blend Update를 한 번 추가/Compile/Save하고 AG 탭을 닫도록 요청했다. 기존 트레이트/연결을 유지하며 PIE는 아직 시작하지 않도록 안내했다. 현재 수동 완료 회신 대기·실행 검증 전·이번 단위 미완료·5-2/Go-NoGo 보류다. 근거는 BoolUpdateFix의 ShutdownGuard·Build 로그/요약과 실행서§24다.

### Bool 보정 AG 추가 보고·실행 시험 대기(2026-10-28)

- 사용자가 기존 Bool 스택의 보정 추가/Compile/Save 완료를 보고했다. 읽기 조회에서 PIE=false·열린 에셋 없음·시험 GameMode·Debug0을 확인했고 AG 해시가 변경됐다. 저장 후 AG 사본을 보관했으며 사용자 에셋4개·시험 BP는 보정 전 해시와 같다.
- ObjectTools의 TraitRootPinNames는 직접 읽기가 지원되지 않았다. 추가 도구/조회 반복 대신 사용자 GUI 보고와 실제 UAFBoolUpdate 발화로 연결을 확인한다. PIE 시작 전 Debug2·새1인 PIE·약60초 빠른 재점프/방향 전환/정지/큰 룩 회전·이상 여부 보고/PIE 유지를 요청했다. 현재 수동 결과 대기이며 이번 단위는 미완료다. 근거는 BoolUpdateFix의 SetupReads·AfterAGSetupHashes와 실행서§24다.

### Bool 갱신 보정 실행·회귀 검증 완료(2026-10-28)

- 사용자는 이상 없음으로 보고했다. 캡처 구간 점프/착지67쌍에서 실제 PIE MM 자식의 임계값 이하 보충 갱신2회를 확인했다. 가중치0.00000011920929/0.000000655651093·child0/destination1·NewlyRelevant0이며 native Bool이 건너뛰는 유효 자식을 보정한 표본이다.
- 새 UAFMMOrder 경고0·PoseSearch Error0·LogAnimation Error/드라이버 오류/assertion/ensure/fatal0이다. 오류를 억제하지 않았으며 보정 분기가 실제 실행된 상태에서 재현되지 않았다. CR 앞 루트 제한도 유지했다. 분모·위도·미검증 분기/전체 품질 한계는 실행서§24가 소유한다.
- 전체 빌드·사용자 AG 추가/저장·실제 보충·무오류/관찰 근거로 **이번 보정 단위는 관측 범위에서 완료**다. §21 공급 분리에서 남은 순서 오류 후속 검증도 마쳤으나 몽타주/GAS/ANS 실제 대상·몽타주 전진/최종 게이트를 통과시킨 것은 아니다. 5-2/Go-NoGo는 보류이며 다음 제안은 native DefaultSlot 최소 연결이다. 사용자 진행 확인 전 해당 조사/구현을 시작하지 않는다.
- 사용자 에셋4개/시험 BP는 보정 전 해시를 유지했고 AG는 보정 추가/저장 뒤 해시가 같다. 추가 코드/에셋 변경·맵 저장/원복·새 녹화·커밋/푸시는 없다. 종료 상태 PIE=true·Debug2·AG/S 탭 닫힘이다. 근거는 BoolUpdateFix의 FixSmoke 로그/요약·RootWindows·AfterSmokeHashes다.

### 새 세션 인계 문서 정리·단일 커밋 범위 확정(2026-10-28)

- 사용자 요청으로 native DefaultSlot 다음 단위는 시작하지 않고 Current를 다음 행동/승인 경계·유지할 공급/루트/Bool 보정·검증 한계·로컬 근거·보존 대상으로 재정리했다. Roadmap의 Phase5 상태는5-1 완료/5-2 진행 중으로 갱신하고 중간 대기/오류 미확정 이력 대신 최신 재현/보정 완료를 요약했다. 상세 이력은 실행서§19~§25에 유지한다.
- 마지막 읽기 조회는 PIE=false·열린 에셋 없음·Debug2·TestMap03 시험 GameMode다. 맵 설정 저장/원복이나 새 녹화/PIE·빌드 없이 인계한다. 원래 BP_LNPGameMode와 현재 시험 값/맵 해시 차이도 명시했다. 새 세션에서 진행 요청을 받으면 DefaultSlot 최소 연결부터 이어간다.
- 이전 프로젝트 양식에 맞춰 feat 제목과 한국어 항목별 본문으로 문서4개·시험 BP/AG2개·스파이크 소스/Build.cs10개를 한 커밋으로 정리한다. 기존 사용자 에셋4개·Saved 근거는 제외한다. 코드의 기능 변경 없이 Build.cs BOM만 기존 상태로 복원했고 차이/스테이징 범위/보존 해시를 확인한다. 푸시·새 채팅 생성은 하지 않는다.

### native DefaultSlot 최소 연결 착수·수동 GUI 대기(2026-10-28)

- 사용자 재개 요청으로 다음 구현 단위에 착수했다. PIE=false·열린 에셋 없음·시험 GameMode·AG dirty=false를 확인하고 기존 AG 해시 일치를 대조했다. 수정 전 AG 사본과 사용자 에셋4개/시험 BP/AG 해시를 UAF52DefaultSlot 폴더에 보관했다.
- native Montage Trait 하나를 Strafe 뒤·Offset Root 앞에 연결하고 DefaultSlot/두 Bool=false로 Compile·해당 AG만 Save·탭 닫기를 수동 요청했다. 시험 GameMode 유지 여부도 회신 대기다. 첫 표본은 DefaultSlot·비가산·루트모션 꺼짐의 AM_SW_Damage_Fast_UEFN이며 실제 재생은 아직 하지 않았다.
- 이번 단위는 연결/재생 검증 전으로 미완료이며5-2/Go-NoGo는 보류다. 소스/에셋 수정·PIE·빌드·새 녹화·맵 저장·커밋/푸시는 없다. 상세는 실행서§26이다.

### DefaultSlot 연결 보고·Compile/Save 확인·첫 PIE 요청(2026-10-28)

- 사용자는 수동 연결 완료와 시험 GameMode 유지를 회신했다. 컴파일/저장/탭 닫힘 로그와 PIE=false·열린 에셋 없음·AG dirty=false를 확인했다. 새 AG 사본/해시를 보관했고 시험 BP/사용자 에셋4개는 그대로다.
- 그래프 Nodes/Links 직접 읽기는 지원되지 않아 반복하지 않았다. GUI 구간의 EditCondition/텍스트 가져오기/FootPlacement 경고는 별도로 보존하며 런타임 통과로 판단하지 않는다.
- 새1인 Standalone PIE/Rewind 녹화에서 AM_SW_Damage_Fast_UEFN을0.1배속으로 첫 재생하는 수동 시험을 요청한다. 실제 출력/종료·평가 task·단일 전진·Stop/교체는 남아 있어 이번 단위 미완료·Go/NoGo 보류다. 상세는 실행서§26과 UAF52DefaultSlot 폴더다.

### DefaultSlot 첫 재생·Idle 복귀 관찰과 순서 오류 확인(2026-10-28)

- 사용자 관찰은0.1배속 피격 자세 재생 후 Idle 복귀다. 실제 원본 AnimInstance의 Montage_Play 반환을 확인했지만 같은 구간 MM/Pose History 순서 오류가 각각236회였다. 내부 Bool MM 자식은 weight1/relevant1이라 이전 임계값 이하 Bool 표본과 구분한다.
- native Montage의 원본 갱신 생략/평가 방문 경로가 원인 후보다. 실제 SourceWeight는 미측정이다. 같은 단위의 최소 보완으로 PIE 종료 후 새 Montage의 Always Update Source만 true로 변경/Compile/해당 AG만 Save/탭 닫기를 즉시 수동 요청했다. 완료 회신 대기이며 재생 검증은 아직 통과하지 않았다.
- 새 trace는 생성됐으나 task/시간축/녹화 무결성 검증은 남아 있다. 원시 로그/요약은 UAF52DefaultSlot의 FirstPlay 파일에 보존했다. C++/엔진 수정·빌드·맵 저장·커밋/푸시는 없다. 이번 단위/Go-NoGo는 미판정이며 상세는 실행서§26이다.

### Always Update Source 변경 보고·재생 재검증 대기(2026-10-28)

- true 변경 완료 보고 뒤 AG의 컴파일/저장/탭 닫힘과 PIE=false·dirty=false·시험 GameMode·Debug2를 확인했다. 새 AG 사본/해시를 보관했고 시험 BP/사용자 에셋4개는 그대로다.
- 같은 몽타주의0.1배속 재생·Idle 복귀 뒤 playing/active 출력·Rewind 녹화 정지/PIE 유지를 요청한다. 아직 오류 해소/전체 단위 완료를 판정하지 않는다. 상세는 실행서§26과 UAF52DefaultSlot의 AlwaysUpdateSetup 근거다.

### DefaultSlot true 설정 재생·오류0·종료 잔류 없음(2026-10-28)

- 사용자 재실행 후 로그에서 MM/Pose History/진단 오류0을 확인했다. 실제 원본 인스턴스의 재생 반환과 종료 뒤 playing=False/active=None을 확인했으나 단일 전진/task/Stop/교체는 남아 있다.
- 최신 녹화의 ABP Montages 및 UAF graph/EvaluationProgram 트랙이 표시됐다. 피격 시점 선택은 수동 GUI가 빨라 PIE 일시정지·중간 시점 스크럽·EvaluationProgram/Details 열기를 요청했다. 녹화 정지 부근 Trace error64는 별도로 보존하며 전체 무결성 통과를 주장하지 않는다.
- 단위 미완료·Go/NoGo 보류다. 원문/요약은 UAF52DefaultSlot의 AlwaysUpdateRetry 파일, 상세는 실행서§26이다. 추가 소스/에셋 변경·빌드·맵 저장·커밋/푸시는 없다.

### ABP 몽타주 트랙 의미와 native UAF 피격 task 확인(2026-10-28)

- 사용자 질문에 ABP 하위 트랙은 전용 AnimInstance의 시간/이벤트 소유권이며 UAF 포즈 평가와 양립한다고 로컬 엔진 소스로 확인했다. 실제 EvaluationProgram의 Index8이 A_SW_Damage_Fast_UEFN을0.282371초에서 샘플링하고 뒤에 블렌드/OffsetRoot/CR task가 있어 native UAF 포즈 경로를 확인했다.
- PIE 일시정지 상태다. 단일 전진을 확인할 추가 시점은 기존 녹화에서 약1초 뒤로 스크럽하도록 요청했다. 새 녹화 없이 사용자 준비 회신을 기다리며 Stop/교체도 남아 있다. 원문 HitEvaluationProgram.txt·실행서§26이 상세 근거다. 이번 단위/Go-NoGo는 미완료다.

### 대표 구간 전진 비교·Stop/교체 시험 대기(2026-10-28)

- 같은 UAF task의 SampleTime0.282371→0.396758(차0.114387)을 확인했다. 화면 눈금 약1.1~1.2초 이동과0.1배속이 부합해 대표 구간의 두 배 전진 징후는 없다. 정확한 커서/프레임별 틱 전수 검증은 아니다. 원문은 HitSecondTimeSnapshot.txt다.
- 같은 PIE Resume 후 즉시 Stop/0.5초 Stop/다른 DefaultSlot 몽타주 교체와 사후 잔류 상태를 수동 확인하도록 요청한다. 중단 직전 실제 playing/위치를 출력해 이미 종료된 표본과 구분한다. 교체 시퀀스도 비가산/루트모션 꺼짐을 확인했다. 이번 단위/Go-NoGo는 아직 미완료다.

### Stop/교체 첫 시도는 원본 종료 후 호출·재시험 요청(2026-10-28)

- 사용자는 이상 없음으로 보고했으나 각 Before가playing=False/위치0이었다. 첫 Stop은 재생 약18.836초 뒤로 약11.33초의 재생이 이미 끝난 상태다. 원본을 끊은 표본이 없어 세 경우를 통과 처리하지 않았다. 새 몽타주의 단독 재생/종료와 해당 구간 순서 오류0은 확인했다.
- 각 시험 직전에 원본0.2초 지점/0.001배속 재생을 새로 실행해 입력 시간을 확보하는 수동 재시험을 요청한다. 이번 단위는 미완료며 상세는 실행서§26·StopReplaceAttemptSummary.json이다. 코드/에셋 추가 변경·빌드·맵 복원·커밋/푸시는 없다.

### native DefaultSlot 최소 연결 단위 완료·정리(2026-10-28)

- 각 경우0.2초 지점/0.001배속으로 새로 재생한 재시험에서 Before는 모두True·위치 약0.204초였다. Stop0/Stop0.5 뒤False/None, 교체 직후 기존False/새True와 종료 뒤둘 다False/None을 확인했다. 유효 중단 구간 MM/Pose History/진단/LogAnimation 오류0이다. 사용자는0초 Stop의 감각은 어색하지만 의도대로라고 관찰했으며 실사용 블렌드 정책은 바꾸지 않았다.
- 정상 재생/종료·UAF 피격 task·대표 전진·실제 재생 중 Stop/교체·사후 잔류 없음으로 **이번 최소 연결 단위는 완료**다. 전체 슬롯/ANS·GAS ActorInfo·무기·루트모션·적/네트워크·최종 품질/성능과5-2/Go-NoGo는 미판정이다. 다음 슬롯·ANS 계약 확장은 사용자 진행 확인을 받는다.
- PIE를 종료하고 GameMode 속성만 원래 BP_LNPGameMode로 메모리 복원/재조회했다. 맵 dirty=true·열린 에셋 없음이며 맵을 저장하지 않았다. 사용자 에셋4개/시험 BP의 디스크 해시와 true 설정 AG 해시를 유지했다. 디스크 맵은 이전 시험 설정이므로 재시작 뒤 실제 값을 새로 읽는다.
- 문서4개(Current/Roadmap/실행서/이력)와 시험 AG만 이번 변경이다. C++ 변경/추가 빌드·도구 구현·커밋/푸시는 없다. 원시 근거는 UAF52DefaultSlot의 StopReplaceValidSmoke/ValidSummary·AfterValidSmokeHashes·ClosureAudit, 상세는 실행서§26이다.

### FullBody 수동 추가·ANS5종 스파이크 진단 준비(2026-10-28)

- 사용자 진행 승인으로 FullBody/ANS5종 계약 확인에 착수했다. UpperBody 본 마스크/AO/IK는 후속 레이어 단위로 구분했다. 기존 근접 몽타주의 ANS5종 배치와 엔진 dispatch 경로를 읽고 실제 콜백/대상/정리를 확인할 진단을 준비했다.
- FullBody 하나를 DefaultSlot 뒤/Offset Root 앞에 수동 추가·Always Update Source=true·Disable Synchronization=false로 Compile/해당 AG만 Save/탭 닫기를 요청했고 완료 보고를 받았다. 새 읽기에서 PIE=false·dirty=false·열린 에셋 없음·컴파일/저장을 확인했다. AG DADEE084… 사본과 보존 해시는 UAF52SlotANS 폴더에 있다. 시험 BP/사용자 에셋4개의 해시는 그대로다.
- 기본 꺼짐·스파이크 폰 한정 LNPUAFNotifyAudit와 ANS5종의 진단 호출/Cancel Begin·End override를 준비했다(기존6/신규2소스). 실제 GAS 참조·콜백 페어·태그 수·Melee 엔티티 지연 파괴 뒤 읽기를 기록하며 기존 게임 상태/TTL/정리 정책은 바꾸지 않는다.
- diff 공백/API 대조만 마쳤고 아직 컴파일 전이다. 전체 빌드를 위해 사용자 에디터 종료 회신을 기다린다. 종료/프로세스 부재 확인 전 빌드하지 않으며 Live Coding은 사용하지 않는다. 이번 단위 미완료·Go-NoGo 미판정·커밋/푸시 없음. 상세는 실행서§27이다.

### ANS 진단 전체 빌드 성공·첫 공격/대시 수동 시험 대기(2026-10-28)

- 사용자가 저장 안 함 종료를 회신했다. CIM 접근 불가를 종료 근거로 쓰지 않고 실제 빌드 전 Get-Process에서 에디터 부재를 확인했다. 처음부터 권한 확장 기준 Development 전체 빌드로 **16 actions·106.09초·Succeeded·종료0**을 확인했다. 강제 종료/Live Coding은 없다.
- FullBody AG·시험 BP·사용자 에셋4개는 빌드 전 해시를 유지한다. 빌드/종료 가드/보존 근거는 UAF52SlotANS에 있다. 재실행 후 시험 GameMode를 새로 확인한다.
- 새1인 Standalone PIE에서 DebugNotifies2/DebugLocomotion2·롱소드 정상 공격3회·대시3회·이상 여부 보고/PIE 유지를 요청했다. 실행/실제 GAS·ANS 대상/BeginEnd/정리는 아직 미검증으로 이번 단위·Go-NoGo 미완료다. 상세는 실행서§27이다.

### 정상 공격/이동 취소/대시 교체의 ANS·GAS·정리 확인(2026-10-28)

- 정상 공격3회에서 ANS5종 Begin/End 각3회·총15쌍,237콜백의 GAS/ANS 원본 대상 일치, 엔티티3개 정리·태그 잔류/관련 오류0을 확인했다. 사용자는 대시 포함 이상 없음으로 보고했다.
- 추가 캡처는 시작12회·ANS51쌍·321콜백 대상 일치·MovementStop6호출이며 요청 횟수의 정확한 분모는 아니다. 선딜/후딜 조기 End 표본과 엔티티9개 정리·태그/순서 오류0을 확인했다. 실제 활성 Melee의 강제 중단 표본은 아직 없다.
- PIE Pause→원본0.72초/1배속 재생→Skip1회로 활성 Melee 창을 준비하는 수동 조작을 요청했다. 회신 후 실제 Begin/엔티티를 확인한다. 새 자동화 도구/코드·추가 빌드·맵 저장·커밋/푸시는 없고 단위/Go-NoGo는 미완료다. 원문은 UAF52SlotANS의 FirstNormal/MovementDash 파일, 상세는 실행서§27이다.

### 활성 ANS 강제 Stop/교체 확인·시스템 종료 시험 요청(2026-10-28)

- Pause/Skip1로 활성 Melee/차단 ANS3종과 엔티티122:25를 확보했다. Stop0 후 다음 시뮬레이션 프레임(월드 시각 차0.016667초)에 각각 End1·DestroyQueued/AfterDestroy0을 확인해 TTL 만료 전의 실제 강제 중단을 확인했다.
- 활성 Combo/이동 차단의 Stop0.5와 활성 Cancel의 FullBody Dash 교체도 각각 다음 프레임 End1·태그 잔류 없음이었다. 전체 변형 조합 전수 검증이 아닌 대표 활성 상태의 근거다. 원문/요약은 UAF52SlotANS의 PausedMelee/PausedComboCancel/PausedStopsSummary다.
- 마지막으로 Actor/ASC가 남은 상태에서 UAF 시스템 종료를 검증하도록 PIE UAFLocomotion 컴포넌트 제거를 수동 요청했다. 실행/End/잔류 확인 전이며 단위/GoNoGo는 미완료다. 자산 변경·추가 코드/빌드·맵 저장·커밋/푸시는 없다. 상세는 실행서§27이다.

### UAF 시스템 제거의 종료 계약 실패·보완 진행 확인 대기(2026-10-28)

- 두 몽타주의 런타임 그룹은 DefaultGroup이었다. 활성 ANS3종/Melee 엔티티122:27을 확보한 뒤 실제 UAF 컴포넌트를 제거했고 Actor/ASC와 Resume 뒤 공급 틱은 유지됐다. 그러나 Begin3/End0·DestroyQueued0이며 이동/공격 차단 태그가 남았다. 엔티티의 TTL 이후 실존은 직접 측정하지 않아 누수/정리 어느 쪽으로도 단정하지 않는다.
- 정상/대표 Stop/교체와 달리 **시스템 종료 계약은 미통과**다. 실제 End/정리·스레드 동기화·중복 방지를 만족하는 동등 보완 경로를 다음 단위로 제안하며 진행 확인 전 수정하지 않는다. 보완 불가능이 확인되기 전 최종 No-Go는 미판정이다. 원문/요약/태그는 UAF52SlotANS의 SystemShutdown 파일이다.
- PIE를 종료하고 GameMode 속성만 원래 BP_LNPGameMode로 메모리 복원/재조회했다. PIE=false·열린 에셋 없음·맵 dirty=true·미저장, AG/시험 BP/사용자 에셋4개 디스크 해시를 유지했다. 진단CVar2를 유지하되 다음 PIE는 새 참조를 구한다. ClosureAudit/ClosureEditorReads가 정리 근거다.
- FullBody AG·진단 소스8개·문서4개가 이번 변경이며 커밋/푸시·추가 코드/빌드·맵 저장은 없다. 이번 검증 결과는 종료 계약 실패로 남기고 다음 보완 단위의 진행 확인을 받는다. 상세는 실행서§27이다.

### UAF 모듈 해제 ANS 종료 보완 착수·전체 빌드 준비(2026-10-28)

- 사용자 진행 승인으로 이전 실패 경로의 보완에 착수했다. 기본 dispatcher의 활성 목록 비교/GT 이벤트와 모듈 해제 순서를 설치 엔진에서 확인했다. 기본 함수의 export 제약 때문에 상속 대체 대신 기본 dispatcher 뒤에서 활성 참조를 복사하는 별도 모듈 컴포넌트로 좁혔다.
- 신규 LNPUAFNotifyCleanup2개와 기존 루트 trait 등록부1개를 준비했다. GT 모듈 해제 때 보관한 활성 상태에 실제 NotifyEnd를 전달하며 태그/TTL/엔티티를 별도로 강제 정리하지 않는다. 일반 종료 후 중복 End가 없는지는 런타임 확인 전이다. 메시/월드 소실·월드 종료는 호출 생략/경고이므로 해당 경로까지 해결한 것으로 보지 않는다.
- 새 조회 PIE=false·열린 에셋 없음·GT 이벤트 배치 모드1이다. diff 공백 검사와 보존 해시를 확인했으며 전체 빌드를 위해 에디터 종료를 요청했다. 종료 회신/실제 프로세스 부재 확인 전 빌드하지 않는다. 보완 빌드/실행 전으로 종료 계약/최종 GoNoGo는 미통과·미판정이며 상세는 실행서§28이다.

### ANS 종료 보완 전체 빌드 성공·정상 재생 회신 대기(2026-10-28)

- 사용자 종료 회신 후 실제 프로세스0개를 확인하고 처음부터 권한 확장 기준 전체 빌드를 실행했다. 신규 타입의 UHT·컴파일·링크를 포함한 **7 actions·47.13초·Succeeded·종료0**이다. Live Coding은 사용하지 않았고 에디터/기존 사용자 에셋4개·시험 BP·AG의 디스크 해시는 보존했다.
- 에디터 재실행/시험 GameMode 확인·진단2/2·새1인 Standalone PIE에서 롱소드 정상 공격3회를 요청했다. 이후 정상 End 뒤 모듈 해제의 중복 End와 앞서 실패한 활성 제거 경로를 재검증한다. 실행 회신 전이므로 종료 계약은 미통과·GoNoGo 미판정이다. 빌드/해시 근거는 UAF52NotifyCleanup, 상세는 실행서§28이다.

### 종료 보완 후 정상 공격3회 회귀 확인(2026-10-28)

- 사용자 이상 없음 보고와 새 PIE/시험 폰/GameMode·진단2/2를 확인했다. ANS5종 각 Begin3/End3·총15쌍,224콜백 대상 일치·균형0~1/최종0·활성 구간 밖 Tick0이다. 엔티티3개 실제 파괴 후 무효·차단/콤보 태그 잔류0·관련 애니메이션 오류0을 확인했다.
- 정상 종료 뒤 Idle/Pause 상태에서 UAF를 제거하고 Resume/Pause하는 수동 시험을 요청했다. 실제 모듈 해제 시 중복 End와 활성 제거 보완 효과는 확인 전이다. 근거는 UAF52NotifyCleanup/NormalRegression 파일, 상세는 실행서§28이다. 추가 코드/빌드·에셋 저장·커밋/푸시는 없다.

### 정상 종료 후 모듈 제거의 ANS 중복 검사(2026-10-28)

- Idle의 active montage=None·제거 후 UAF 목록[]·Resume 공급 틱7회와 ModuleRelease를 확인했다. 프로젝트 ANS5종 추가 콜백0·태그 잔류0·관련 오류0으로 정상 종료한 ANS의 중복 End는 관측되지 않았다.
- native 종료 사본에는 remaining=1이 기록됐다. 대상은 특정하지 못했으며 모든 ANS가 비어 있었다고 판정하지 않는다. 근거는 UAF52NotifyCleanup/IdleRelease 파일이다. 새 PIE/새 Python 참조로 활성 Melee 준비를 요청했고 실제 제거 보완 효과는 아직 확인 전이다. 상세는 실행서§28이다.

### 보완 후 활성 Melee 재현 준비·제거 요청(2026-10-28)

- 새 PIE에서 Begin3/실제 원본 대상 일치·Melee 엔티티122:1 생성 요청·ASC 이동/공격 차단을 확인했다. 이전 실패와 같은 활성 상태가 준비됐으며 ActiveMeleePrepare 로그/태그를 보존했다.
- Pause에서 실제 playing/위치를 출력하고 UAF 제거/Resume/Pause하는 수동 시험을 요청했다. 종료 보완 효과는 회신/로그 확인 전이다. 상세는 실행서§28이며 추가 코드/빌드·에셋 저장·커밋/푸시는 없다.

### 활성3종 모듈 해제 보완 확인·단위 완료(2026-10-28)

- 제거 직전 playing=True/위치0.7366669178·월드 시각9.1587399095, 제거 뒤 UAF 목록[]·Actor/ASC 유지와 Resume 공급 틱을 확인했다. ModuleRelease 다음 실제 이동 차단/공격 차단/Melee End가 각1회 발생했다. End 월드 시각9.175407로 다음 시뮬레이션 프레임이다.
- 엔티티122:1의 실제 DestroyQueued와 AfterDestroy(valid0/active0), ASC 차단/콤보 태그 없음·실제 GAS/ANS 대상 일치·관련 오류0을 확인했다. native 사본4개 중 프로젝트3개 외 참조1개는 미특정이다. PIE 종료 뒤까지 프로젝트 Begin3/End3과 엔티티 생성/파괴1회를 유지해 지연 중복 End는 없었다.
- 정상 공격3회/정상 종료 뒤 중복 검사와 같은 실패 재현의 활성3종 제거로 **이번 보완 단위는 완료**다. 나머지 Combo/Cancel 활성 해제·Actor/월드 종료·다른 배치 모드는 미검증이고 전체 종료 계약/GoNoGo는 미판정이다. 특히 월드 종료 시 호출 생략이라는 구현 한계를 유지한다. 다음 종료 경로 검증 단위는 사용자 진행 확인을 받는다.
- PIE=false·열린 에셋 없음·GameMode 속성만 원래 값으로 메모리 복원·맵 dirty/미저장을 확인했다. 사용자 변경4개/시험 BP/AG의 해시는 이번 시작과 같다. 소스3개(기존1/신규2)와 문서4개가 이번 변경이며 엔진/에셋 저장·추가 빌드·커밋/푸시는 없다. 근거는 UAF52NotifyCleanup의 ActiveSystemRelease/AfterPIEStop/Closure 파일, 상세는 실행서§28이다.

### 새 세션 인계 정리·단일 커밋 범위(2026-10-28)

- 사용자가 후속 구현 대신 새 세션 인계 문서 정리와 한 번의 커밋을 요청했다. Current의 시작 문서/다음 행동/승인 경계를 정리하고 실행서 상단·단위 상태·판정표·다음 행동의 보완 전 실패 상태를 최신 §28 결과로 맞췄다. 기존 실패 표본은 삭제하지 않는다. Roadmap은 최신 단위/참조 범위를 반영했다.
- 실행서 §28에 다음 Combo/Cancel 활성 제거의 새 PIE/새 참조·후보 시점/명령·활성 확인·End/태그 성공 기준을 남겼다. Actor/월드 종료의 정리 소유권과 현재 호출 생략, native 참조1개의 미특정/동일성 미확인, Branching/다른 배치 모드와 최종 게이트 미판정을 분명히 했다. 이번 요청으로 후속 시험/마이그레이션을 시작하지 않았다.
- 마지막 읽기에서 PIE=false·열린 에셋 없음·원래 BP_LNPGameMode·DebugNotifies2를 확인했다. 사용자 에셋4개/시험 BP/AG의 해시를 보존하며 맵은 저장하지 않는다. 새 빌드/녹화/PIE를 인계만을 위해 반복하지 않는다. 같은 로컬 프로젝트의 Saved 근거는 재사용하고 다른 체크아웃에는 따라가지 않음을 Current에 유지했다.
- 최근 `feat:` 제목·한국어 항목별 본문 형식을 확인해 **문서4개·시험 AG1개·소스11개(기존7/신규4), 총16개**를 한 커밋으로 정리한다. 사용자 맵/데이터4개와 git 제외 Saved는 제외하며 커밋 전에 스테이징 범위/공백/보존 해시를 대조한다. 푸시·새 채팅 생성은 하지 않는다. README/Decisions/공통 전투 설계는 새 확정 결정·정식 마이그레이션이 없어 변경하지 않는다.
