# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 최신 방향(2026-11-02): **§37 라이플 기본 자세/AO 최소 연결 단위는 대표 검증과 종료 정리까지 완료**했다. 원본 Lyra 장착과 손의 애니메이션 회전을 전달하는 공통 LyraFrame 리타깃을 사용한다. 사용자는 최종 근적도 첫/재장착·조준/이동/급회전·점프/착지·장전/대시 취소·복귀에서 육안상 어색함이 없다고 확인했다. 한 프레임의 실제 Idle/AO/가산/본별 합성·장전/구면 보정/CR task도 확인했다.
> 현재 작업: **5-0·5-1 완료, 5-2 진행 중. 다음 세션은 사용자 승인된 정밀 총열 보정이다.** 현재 총열/탄 초기 방향 차이는142.2m에서1.550~1.559°·10.64m에서3.319~3.347°이며, 탄/전달된 실제 조준점 오차는≤0.000001°다. 이 잔차는 다음 단위로 분리했다. 모든 슬롯/위도·네트워크·Phase 5 최종 게이트/Go-No-Go는 미판정이다.
> 마지막 갱신: 2026-11-02

> 종료 상태: PIE=false·운영 GameMode 메모리 복원·맵 미저장·시험14개 clean·열린 에셋0·DebugLocomotion/DebugNotifies/DebugFireAim=0. 사용자4개/공유3개 해시 보존. §37 변경의 커밋은 사용자 승인(2026-11-02) 범위이며 사용자4개는 제외한다. 푸시/새 채팅 생성은 하지 않는다.

Phase 0·1A·1C·2·3은 완료, 1B는 대기다. **최종 일곱 게이트·Go/No-Go는 미판정**이며 UAF 마이그레이션 승인으로 해석하지 않는다.

## 새 세션의 시작점과 승인 경계

1. `CLAUDE.md`·`README.md`·`Current.md`·`Decisions.md`와 `phases/Phase05_UAFSpike.md` §2·§4·§35·§37을 먼저 읽는다. §36 마지막 완료/인계는 기존 표시·IK 계약의 근거다. 슬롯 구성은 §26~§27, 종료 보완/대표 검증은 §28~§32를 필요할 때 읽는다. 최종 게이트는 `Roadmap.md` Phase 5가 소유한다.
2. §26의 **native DefaultSlot 최소 연결 단위는 완료**다. 실제 UAF 피격 task·정상 종료·대표 구간 전진·재생 중 Stop0/Stop0.5/교체·종료 잔류 없음·해당 구간 순서 오류0을 확인했다. 첫 무효 중단 표본은 재시험으로 보완했다. 0초 Stop은 의도대로지만 플레이 감각은 어색하다는 사용자 관찰을 유지하고 실사용 블렌드 정책은 바꾸지 않았다.
3. §31~§32 대표 종료 검증·§34 UpperBody·§36 원본/표시 ABP·IK 차단·§37 라이플 자세/AO 최소 연결은 각각 기록한 대표 범위에서 완료다. 다음 세션에는 별도로 승인한 정밀 총열 보정만 이어간다. 월드 독립 태그/사후 실제 무효·native 참조1개 미특정·예약 GT 이벤트/콜백 내부 파괴/Branching/다른 배치 모드·전체 게이트의 한계는 유지한다.
4. **한 번에 한 구현 단위만 진행하고 완료마다 계속할지 확인한다.** PIE/GUI 수동 조작이 빠르면 즉시 요청한다. 전용 UAF 그래프 편집 MCP가 없으므로 간단한 트레이트 추가는 수동 GUI가 효율적이다. 인계만을 위해 기존 빌드/녹화를 반복하지 않는다.

## 새 세션에서 바로 할 일

1. 아래 정밀 총열 보정 인계 범위로 시작한다. §37 완료 근거는 재사용하며 인계만을 위한 재녹화/재빌드는 하지 않는다. 현재 에디터는 운영 GameMode이므로 시험 PIE를 시작하려면 TestMap03의 메모리상 GameMode를 BP_GameModeUAFSpike로 다시 지정한다. 맵 전체 저장은 하지 않는다. 재시작/재로드 뒤에는 실제 값을 먼저 읽는다.
2. AG의 Idle/AO는 Retargeted/UAFLyraFrame/의 _LyraFrame이고 시험 BP는 CHT_MontageLyraFrame·DA_RifleLyraContract를 쓴다. UEFN 마스크(root0/spine_02=0.5/spine_03 아래1)·관련성 트레이트·Alpha1/Aim입력을 유지한다. 원본 장착은 weapon_r·위치0·Yaw−90°·Scale1이다. 과거 손 고정 후보/영속 그립 전용 읽기로 되돌리지 않는다.
3. 현재 생성24개(시퀀스23/AO1)의 원본 대비 방향 최대0.104332°·기존 대비 오른손/손가락 그립 변화 최대0.009846cm·손 위치 변화0.040612cm다. 생성/연결은 README §4 순서를 따르며 기존 시퀀스를 삭제/overwrite하지 않는다. 미채택 진단 사본은 연결에서 제외한 채 보존한다.
4. 발사 진단 LNP.Projectile.DebugFireAim은 비Shipping·기본0이며 다음 세션에서 재사용할 수 있다. 마지막 전체 빌드는5 actions·31.28초·Succeeded/종료0이고 이후 C++ 변경은 없다. 실제10발은 발사점/Muzzle 거리0cm·속도5000cm/s·중력0·표적 방향 오차≤0.000001°다. 초기 방향 계측이며 충돌/후속 렌더 반동/네트워크 검증은 아니다.
5. 발사 후보 FullBodyAdditivePreAim은 현재 지원3슬롯 밖이므로 실제 탄 발사 성공을 발사 애니 슬롯 소비 통과로 해석하지 않는다. 전체 무기 전환·Manny 견착·독립 커브/모든 슬롯·Mover 루트모션·적/네트워크·일곱 최종 게이트/Go-No-Go와 Phase 6은 남아 있다.

## 별도 세션으로 넘기는 정밀 총열 보정

- 사용자 승인 방향(2026-11-02): 발사체에 쓰는 실제 조준점을 총열·팔 자세도 매 프레임 향하게 한다. 이번 §37 기본 연결/회귀/종료 단위를 마친 다음 세션에서 구현한다. 새 채팅 생성 자체는 요청받지 않았다.
- `LNPInputHandlerComponent.cpp`의 TickComponent는 로컬 원거리 조준 모드에서 이미 매 틱 ComputeCrosshairAimPoint를 호출한다. 카메라 원점/ControlRotation 광선의 물리 트레이스와 플레이어/적 수학 질의를 합친 캐시를 GetCrosshairAimPoint로 공개하고, CaptureFireAimInput과 ADS 가이드가 공유한다. 자세 보정도 같은 캐시를 소비하고 별도 트레이스나 표적 선택기를 만들지 않는다.
- `LNPFireGeometry::ResolveAimDirection`의 실제 표적 수렴/근접 기각·fallback 계약을 먼저 읽는다. 총구 원점이 자세 보정으로 움직이는 피드백과 입력/루트/평가 순서, 구면 접평면 및 표시 왼손 IK와의 관계를 조사한다. 기존 발사 기하를 총열 방향으로 바꾸는 접근이나 무기 메시의 고정 회전값 추가를 먼저 채택하지 않는다.
- LyraFrame 리타깃·weapon_r 부착/원본 상대 운동·양손 접촉을 유지하고, 장전/대시 동안 보정 가중치를 내려 원본 동작을 보존하는 경로를 검토한다. 기존 몽타주/커브·IK 피드백이 보정 차단을 소유할 수 있는지 먼저 확인한다. 새 범용 시스템/모든 무기 마이그레이션은 범위가 아니다.
- 검증 기준은 실제 발사 시점 총열↔실제 초기 방향 계측, 근/원거리·상하좌우·구면 대표 자세와 장전/대시 차단·복귀다. 현재 기준은 LyraFrameActualFire_2026-11-02.json의 원거리 약1.56°/근거리 약3.34°이며 그립/게임플레이를 함께 보존한다. 구현 단위별 진행 확인과 수동 조작 우선 원칙을 유지한다.

## 재사용하는 §36 검증 결과

| 범위 | 결과 | 한계 |
|:---|:---|:---|
| 빌드/자동화 | Development13 actions·99.12초, 최종4 actions·7.22초 성공/종료0. HandIKFeedback1개 Success·테스트 경고0/오류0 | 첫 float 비교 실패는1e-6 허용 오차만 보완했고 리포트 보존 |
| 정상 표시/재장전 | 사용자 정상. 피드백71표본 모두valid1·실제 원본/표시 왼손 알파 Idle1→재장전0 | 간헐 표본·두 상태 읽기이며 독립 커브/모든 프레임 검증 아님 |
| 대시 중단/복귀 | 사용자 정상. 피드백17표본 모두valid1·FullBody 요구량1·후속 종료 알파1 | 모든 슬롯 중첩/배치 동등성은 미판정 |
| 물리 | Idle mode100/spine_04 반응1회·복귀/출력 위치 동일. 롱소드 공격 중 suppressed1·실제 적용0·종료 PhysicsAlpha0/지연 재생 없음 | 표시 경로 직접 호출. HP/GameplayCue 전달·모든 블렌드/네트워크 경계 검증 아님 |
| 근적도 새 폰 | PodTerraceSouth slot5/Bastion 바닥·새 폰C_1의 실제 원본 출력/표시 인스턴스 확인. 28표본valid1·CR 앞67창 위도−6.99°~−3.40°·거리≤30cm/Yaw≤90°/Tilt0°·사용자 정상 | 정확히 위도0/모든 위도·CR 뒤 본별 최종 포즈 검증 아님 |
| 종료/보존 | PIE=false·운영 GameMode 메모리 복원·맵 미저장·시험3개clean/열린 에셋0. 사용자4개/연결3개/빌드 소스7개 해시 보존 | 디스크 맵의 시험 GameMode는 재시작 때 다시 적용될 수 있음 |

§36 당시 Foley·에디터/접속 경고와 무기 이상 회전 미확정 상태는 역사적 근거다. 무기 규약 문제의 후속 해결/한계는 §37을 따른다. 오른손 알파도 내려갔지만 UEFN CopyPose에는 오른손 IK 노드가 없어 견착 소비 통과가 아니다. 독립 커브·전체 슬롯/그룹/배치·무기 자세/AO/전환·Manny 리타깃/견착·Mover 루트모션·적/네트워크·최종 PSD/포즈 품질·성능은 남아 있다. 과거 §19~§34와 실패 이력은 실행서/로그를 필요한 경계만 읽는다.

## 유지할 구현과 에셋

- §36 `LNPUAFMontageIKTrait.h/.cpp`는 native PreUpdate 뒤 local×graph global 가중치를 관찰한다. 모듈 Begin에서 작업 사본 초기화·End에서 잠금으로 완료 사본 공개, 세 슬롯 누락/중복이면 invalid다. 일반 원본의 기존 프록시 계산은 유지하고 표시/링크 인스턴스는 원본의 virtual 계산에 위임한다. 시험 원본만 완료 사본+컴포넌트 커브를 읽으며 몽타주 틱/자식 큐/ANS 정책은 바꾸지 않는다.
- §34 `LNPUAFUpperBodyMontageTrait.h/.cpp`는 native UpperBody Montage 위 additive 평가 어댑터다. 기존 그룹/재생/ANS/자식 갱신을 바꾸지 않고 평가된 원본 keyframe 복사→native 몽타주→spine_01 합성을 예약한다. 별도 마스크 에셋/범용 캐시는 없다. 속성 마스크는 compact 순서로 변환해 native 본/커브 합성과 별도로 처리한다. 일반 keyframe 전용이며 추상 런타임/NamedSet은 지원하지 않는다. transient5본/두 순서 task 테스트와 정상/이동/대시 대표 실행·한 프레임 평가 구조를 확인했다. 원본 커브0/IK알파1과 표시 인스턴스None은 표시 IK 소비 검증이 아니다.
- 시험 BP `/Game/Characters/UAFSpike/BP_PlayerUAFSpike`의 부모는 `/Script/LootNPop.LNPUAFSpikePlayerCharacter`다. AnimSourceMesh는 기존 `ABP_SourceUAFSpike`·일반 평가=false·AlwaysTickPoseAndRefreshBones다. 정확한 SkeletalMeshComponent 질의만 원본 메시를 반환하고 다른 질의는 Super에 위임한다. 기본 플레이어/적 클래스는 바꾸지 않았다.
- 시험 폰이 몽타주 전용 초기화와 프로젝트 Native 초기화/슬롯 그룹을 준비하고 PrePhysics 틱에서 기존 B0/공유 입력을 공급한다. GFrameCounter 가드는 프레임당 최대 한 번 공급한다. UAF PrePhysics는 폰 공급 틱 뒤에 의존한다. 두 serial 표본만으로 매 게임 프레임 공급률을 전수 측정했다고 주장하지 않는다.
- 루트 평가 task의 완료 상태를 모듈 피드백에 기록한다. 게임 스레드는 UAF ReadComponent와 잠금으로 사본만 읽어 기존 OffsetRootTransform/bHasOffsetRoot를 갱신한다. FutureFacing/Aim은 이 CR 앞 UAF 루트를 기준으로 삼는다. 워커 trait 상태 포인터를 게임 스레드에서 직접 읽지 않는다.
- AG 흐름: **MM/Fall_Loop Bool + Pose History + LNP Bool Blend Update → Passthrough/Strafe/입력 어댑터 → Rifle Idle+AO/Apply Additive·Blend Layer+관련성 트레이트 → Montage(DefaultSlot) → Montage(FullBody) → Montage(UpperBody)+LNP UpperBody Montage Mask → Offset Root/최종 구면 보정 → Control Rig(v2)/Passthrough Blend → Output**. 세 Montage는 Always Update Source=true·Disable Synchronization=false다. 대표 대시/재장전·이전 ANS 종료 보완은 확인했지만 전체 무기/종료 계약은 미판정이다. §36 사용자 GUI 보고의 locator/Output Component는 AnimSourceMesh이고 세 Montage에 LNP Montage IK Feedback을 추가했다. 기존 구면 CR·0.3/0.2초 Bool 알파·착지 리셋/발판 정책은 유지한다.
- Bool 보정은 기본과 같은 가중치/AlwaysUpdate/관련성 및 BlendingOut/NewlyRelevant 조건을 읽어 기본이 제외한 유효 자식만 추가한다. 기본 큐와 중복 등록하지 않고 자식 수명/가중치/블렌드를 바꾸지 않는다. DebugLocomotion2에서 `[UAFBoolUpdate]`가 실제 보충을 기록한다. `[UAFMMOrder]` 진단과 엔진 오류 검사도 억제하지 않았다.
- UAF/Internal·UAFAnimGraph/Internal/Private·UAFWarping/Private·UAFPoseSearch 내부 API 의존성이 있다. UE 버전 갱신 시 재검토한다. 새 타입/trait 레이아웃 변경은 에디터 종료 후 전체 빌드한다. 과거 Live Coding은 완료 알림 Slate/mimalloc 경로에서 크래시했으며 원인은 미확정이다(§14).
- 현재 §37 AG 해시는 `20662985653A76E91AAE018BE08970FAC429FD303A6A4ACF96DCC03D1C328FD6`, BP는 `34354E9C64F6A152F9DC541234BE62CDAE2F048390B40F6B9CA45C68593D63A0`다. §36/§34/FullBody 이전 사본은 각 Saved 근거 폴더에 보존하며 현재 AG를 과거 백업으로 덮어쓰지 않는다. 시험 Yaw 발판 BP는 유지하며 재배치 없이 이전 Teleport 명령을 실행하지 않는다.
- `LNPUAFNotifyAudit.h/.cpp`와 ANS5종 호출부/Cancel Begin·End override는 전체 빌드를 통과했다. 기본값0은 꺼짐,1은 Begin/End/엔티티,2는 기존 기능 Tick도 기록한다. 실제 태그/몽타주/엔티티 정책을 바꾸지 않는다. Mass 사후 읽기는0.1초 간격이며 Pause에서는 월드 시각이 진행할 때까지 기다린다. 월드 종료 시 미확인으로 기록하며 정리를 대신하지 않는다.
- `FLNPUAFNotifyCleanup`은 기본 dispatcher 뒤의 활성 사본을 잠금으로 보호한다. 시험 폰 Destroyed/EndPlay에서 연결 해제 전에 사본 종료 소유권을 인수하고 잠금 밖에서 컴포넌트 등록 해제/실제 End를 전달한다. 무효 UAF 참조는 읽지 않으며 이후 소멸자의 중복 End를 막는다. 소멸자 fallback의 메시/월드 소실·tearing down 가드는 유지한다. Audit은 Pause 중 월드 시각이 진행할 때까지 사후 관측을 기다린다. 예약 GT 이벤트·콜백 내부 파괴/다른 배치 모드·월드 사후 실제 무효까지 해결했다고 보지 않는다.

## 인계 시 에디터와 맵 상태

- 종료 및 인계 직전 새 읽기는 PIE=false·TestMap03·운영 BP_LNPGameMode·열린 에셋0이다. GameMode 속성만 메모리 복원했고 맵은 저장하지 않았다. 디스크 사용자 맵에는 이전부터 시험 GameMode가 있으므로 재시작/맵 재로드 때 실제 값을 다시 읽는다.
- DebugLocomotion/DebugNotifies/DebugFireAim을0으로 정리하고 재읽었다. Rewind 녹화는 종료됐고 읽기용 Slate observer도 해제했다. 다음 PIE에서는 새 폰/인스턴스 참조를 찾는다. 이전 p/ans_* 참조·재배치 전 Yaw 발판 Teleport는 재사용하지 않는다.
- 시험 BP/S/AG·마스크·LyraWeaponContract10개 총14개는 모두clean이다. 실제 BP의 새 Chooser/라이플 데이터와 AG 새 Idle/AO 의존성을 재확인했다. §37 소스6개/연결14개/사용자4개·공유3개 해시와 에디터 읽기를 UAF52RifleOverlay의 Closure*2026-11-02 근거에 남겼다. 인계만을 위한 새 빌드/PIE/녹화는 반복하지 않는다.
- 직접 미지원인 RigVMPin.DefaultValue/TraitRootPinNames/AnimScriptInstance를 반복 조회하지 않는다. 간단한 그래프/locator/PIE는 수동 요청하고 실제 실행 로그를 확인한다.

## 로컬 근거와 변경 범위

- `Saved/AnimationOverhaul/2026-10-29_UAF52DisplayPathInvestigation/`: 기존 ABP/엔진 경로 조사·설정·소스/에셋 해시.
- `Saved/AnimationOverhaul/2026-10-29_UAF52DisplayBridge/`: Before_* 소스4개/시험 에셋3개, Build/BuildRetry·첫/재시험 리포트·BuiltSourceHashes, FirstDisplay*/TwoStateIK*/DashSmoke*, PhysicalIdle*/PhysicalAttack*, EquatorAfterRespawn*, ClosureEditorReads/Closure*Hashes/PIEStop. 인계 비교는 Handoff* 근거에 추가한다.
- `Saved/AnimationOverhaul/2026-10-29_UAF52RifleOverlay/`: §37 생성/장착/프레임/몽타주 보고서·실제 발사10발·ClosureRegression/Counts·ClosureEvaluationProgram JSON/원문5개·ClosureEditorReads/Source/ConnectedAsset/UserAndSharedHashes_2026-11-02. 최종 녹화는 Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261102_002711425.utrace다. 상세 실패/수정 이력은 history/Phase05_Log.md가 소유한다.
- Saved는 git 제외이므로 같은 로컬에서만 재사용한다. 다른 체크아웃의 제외 소스/리타깃 에셋은 README §4의 재생성 절차를 따른다. 이전 §31~§34 종료/상체 합성 근거는 실행서/로그의 해당 절이 소유한다.

§35~§36은 `6523d60`에 커밋돼 있다. §37의 입력 소스2개·관련성 트레이트2개·Build.cs·발사 진단1개와 소유한 시험 에셋/재생성 스크립트·문서가 이번 사용자 승인 커밋 범위다. 과거 손 부착 C++ override는 제거된 상태로 유지한다. 미채택 진단 사본은 현재 연결에서 제외한 채 이력으로 보존한다. 푸시/새 채팅 생성은 요청받지 않았다. 변경 전 사본과 해시는 UAF52RifleOverlay/Before*에 보관했다.

기존 사용자 변경4개는 해시를 보존하고 커밋에서 제외한다: `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. 깨끗한 트리를 만들기 위한 원복/맵 전체 저장은 하지 않는다.

새 반사 타입/가상 함수·trait 레이아웃 변경은 에디터 정상 종료 확인 뒤 Development 전체 빌드한다. Build.bat/cook/package는 처음부터 권한 확장 실행하며 Live Coding/전체 빌드를 동시에 쓰지 않는다. GASP 에디터/소스 에셋은 읽기 전용이다(AN-009·AN-018).
