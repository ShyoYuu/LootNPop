# Phase 5 수행 기록

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
