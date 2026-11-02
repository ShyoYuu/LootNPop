# 애니메이션 개선 현재 작업 상태

> 상태: 새 세션 인계 대기
> 마지막 갱신: 2026-11-02
> 현재: **5-0·5-1 완료, 5-2 진행 중. §38 총열 보정·§40 발사 가산 연결 완료, §41 보조 가산 기존 생략 확인, §42 루트모션 전달 조사 완료. 전신 몽타주 중 루트 회전 Release 판정 누락은 재현했고 아직 고치지 않았다.**

사용자는 이번 세션의 문서·인수인계 정리와 단일 커밋을 요청했다. 다음 수정 착수는 이 요청에 포함하지 않는다. 새 세션에서 진행 지시를 받으면 아래 전신 몽타주 판정 보완부터 한 단위로 진행한다. 구현 단위마다 계속할지 확인하고, PIE/GUI 수동 조작이 빠르면 즉시 요청한다. 푸시와 새 채팅 생성은 요청받지 않았다.

최종 일곱 게이트·Go/No-Go는 미판정이며 Phase 6 마이그레이션은 승인 전이다. Phase 0·1A·1C·2·3 완료, 1B 대기 상태도 유지한다.

## 다음 세션에서 바로 할 일

1. `CLAUDE.md`·`README.md`·`Decisions.md`와 `phases/Phase05_UAFSpike.md` §4·§38~§42를 읽는다. 표시/IK 전제는 §36, 라이플 프레임/재생성은 §37, UpperBody 마스크는 §33~§34가 소유한다.
2. **시험 공급층의 전신 몽타주 활성 판정 보완/재검증**을 진행한다. 일반 ABP의 기존 슬롯/그룹·Release 정책을 유지하면서 UAF 시험 경로가 실제 몽타주 상태를 읽도록 최소 보완한다. 관련 파일은 `Animation/LNPAnimInstance.h/.cpp`의 `IsFullBodyMontageActive`·`GetOffsetRootTranslationMode`·`GetOffsetRootRotationMode`, `Animation/UAFSpike/LNPUAFSpikeAnimInstance.h/.cpp`의 명시적 입력 공급이다.
3. 원인은 일반 `IsSlotActive`가 `FAnimInstanceProxy::MontageEvaluationData`를 읽는 반면 UAF의 `TickMontagesOnly → TickMontageOnly`는 그 사본을 갱신하지 않고 전용 `FUAFMontageComponent::MontageEvalData`를 만든다는 차이다. 일반 Proxy를 강제로 갱신하거나 새 RootMotionAttribute/이동 어댑터를 추가하는 우회부터 구현하지 않는다.
4. 수정 전 재현은 **AM_MM_Dash_Backward_LyraFrame 재생=true·위치0.129319906초인데 FullBody 활성=false·루트 회전 Accumulate**다. 기대 결과는 전신 몽타주 중 최종 공급 루트 회전 모드 Release, 종료 후 기존 상태 복귀다. 일반 엔진 `is_slot_active()` 값 자체를 true로 만드는 것을 완료 조건으로 삼지 않는다. 당시 이동 모드는 이미 Release였으므로 이동 모드 오류까지 재현했다고 주장하지 않는다. 지상 이동 중 전신 재생·DefaultSlot/FullBody·장전/발사 단독·중단/복귀를 필요한 대표 범위로 확인한다.
5. 새 가상 함수/레이아웃이 필요하면 사용자에게 기존 작업 저장·에디터 정상 종료를 요청한 뒤 `LootNPopEditor Win64 Development` 전체 빌드한다. Live Coding은 재사용하지 않는다. 기존 게임플레이·즉시 사격/조준 스냅샷·ANS 정책을 유지하고 이 판정 보완을 실제 이동/워핑 게이트 통과로 확대하지 않는다.

## 재현 도구와 미검증 경계

- 추적된 읽기 스크립트: `Scripts/AnimOverhaul/InspectUAFRootMotion_5-2.py`. 에디터 Output Log에서 `py "D:/UnrealProjects/LootNPop/Scripts/AnimOverhaul/InspectUAFRootMotion_5-2.py"`로 실행한다. 에셋/모듈 모델과 현재 PIE 시험 폰을 읽고 `Saved/AnimationOverhaul/2026-11-02_UAF52RootMotion/Inspection.json`만 쓴다. 실행마다 덮어쓰므로 Idle/재생 결과를 각각 보존한다.
- 재현이 필요하면 시험 GameMode를 메모리 지정하고 새 1인 PIE에서 라이플 대시 도중 Pause 후 읽는다. 필요시 `slomo 0.1`을 쓰고 검사 뒤 반드시 `slomo 1`을 복원한다. 실제 선택된 몽타주를 결과에서 확인하며 요청한 방향 이름으로 대체하지 않는다. 새 폰/인스턴스 참조를 찾고 과거 PIE 참조를 재사용하지 않는다.
- 현재 Mover의 PrimaryVisualComponent는 AnimSourceMesh와 같고 백엔드는 NetworkPrediction이다. 기존 `FLayeredMove_AnimRootMotion`은 몽타주를 시뮬레이션 시간으로 직접 추출하고 기존 MotionWarpingMoverAdapter를 거쳐 월드 이동을 만든다. UAF 최종 포즈의 RootMotionDelta 소비 경로가 아니다. RootMotionAttribute feature=false·모듈 ToggleRootMotion 없음도 확인했다.
- 현재 검술 AM_SW_Attack_01_UEFN의 추출 이동량은0이다. 전방/좌우 대시에는 이동량이 있고 뒤 대시는0이다. 이는 에셋 추출값이며 실제 물리 이동량이 아니다. 제자리 검술에 ForceMode=1을 걸어 워프 통과를 주장하지 않는다. 실제 이동/방향/중단·물리 대시와 혼합 중복·적도/재시뮬레이션과 전체 네트워크 게이트는 남아 있다.

## 유지할 구현과 수용 기준

- **AN-025:** 즉시 사격·낮은 입력 지연과 현재 로컬 라이플 총열 보정을 유지한다. 육안 품질을 충족한 일반 이동 시차 잔차는 허용한다. 발사 보류·입력 스냅샷 시점 변경·Simulated Proxy 정밀 정렬 추가 작업은 하지 않는다. 추가 정밀 계측은 이상이 의심될 때 한다. 이를 전체 네트워크 검증 생략으로 확대하지 않는다.
- 사격과 정규 UAF 평가는 기존 조준점 캐시를 각 시점에 읽으며 발동 사본을 고정 공유하지 않는다. 정밀 보정은 다중 프레임 시간 보간 없이 같은 평가에서 적용한다. 빠른 룩/거리 급변의 엄밀한 입력→표시 지연 상한은 미계측이다.
- `UAFSpikeInputs`의 PrePhysics 공급은 Actor/입력/ControlRotation 뒤·UAF 앞이다. 프레임당 최대 한 번 공급하며 표시 ABP는 UAF 출력 뒤에 평가한다. 워커 상태 포인터 대신 잠금으로 보호한 완료 사본을 GT가 읽는다.
- 최종 LNP Rifle Aim은 spine_03 회전만 바꾸고 LyraFrame·weapon_r 부착·자식 로컬/양손 그립·하체/커브/속성을 유지한다. 기존 발사 기하/전방50cm·수직 이탈150cm 게이트/fallback·월드 클리핑을 공유한다. 장전/대시 차단은 같은 실행의 세 슬롯 가중치와 DisableLHandIK 커브가 소유한다.
- AG: MM/Fall_Loop·History·Bool 보완 → Strafe/입력 어댑터 → Rifle Idle/AO·Blend Layer → **FullBodyAdditivePreAim → DefaultSlot → FullBody → UpperBody+마스크** → OffsetRoot/구면 보정 → CR/Passthrough+Rifle Aim → Output. 네 Montage는 Always Update Source=true·Disable Synchronization=false다. IK Feedback은 DefaultSlot/FullBody/UpperBody 세 개에만 유지한다.
- 시험 BP는 ABP_SourceUAFSpike의 일반 평가=false·AlwaysTickPoseAndRefreshBones다. UAF 출력은 AnimSourceMesh, 같은 UEFN VisualMesh는 기존 CopyPose/왼손 IK/물리 ABP를 사용한다. 기본 플레이어/적은 변경하지 않았다. §31~§36 ANS 종료/표시/IK 보완을 되돌리지 않는다.
- LyraFrame Idle/AO·CHT_MontageLyraFrame·DA_RifleLyraContract를 유지한다. 장착은 weapon_r·위치0·Yaw−90°·Scale1이다. `UpperBodyAdditive`는 기존 ABP/설계의 의도적 생략이며 UAF에도 추가하지 않는다. 사용 중인 시퀀스 삭제/배치 overwrite와 과거 손 고정 후보 복원은 금지한다. 재생성 절차는 README §4다.

## 재사용할 검증과 현재 에디터 상태

- §38 마지막 빌드:4 actions·10.24초·성공/종료0. TestsTiming의 `RifleAim`·`HandIKFeedback`2개 Success·테스트 경고/오류0·0.033초다. 합성 보정216회/완전 보정72조건과 손/자식/하체/커브 보존을 확인했다. 현재 C++13개 해시는 그 빌드와 같으므로 인계만을 위해 재빌드/자동화를 반복하지 않았다.
- §38 근적도 조준/이동 사격·장전/대시 복귀, §40 정지/이동 사격·장전/대시 뒤 사격은 사용자 정상이다. §40 한 프레임28 task의21=MM_Rifle_Fire_LyraFrame(0.039249초),22=Apply Additive,23~27=루트/CR/Rifle Aim을 확인했다. Rewind Tasks는 시스템 트랙 하위 **EvaluationProgram**을 선택해야 보인다. HandIK100표본 모두valid1이다.
- §40 애니 오류/ensure/MM 순서 오류0이지만 녹화 종료 Trace 오류64가1개 있다. 선택한 프레임은 실제 조회됐고 기존 Foley/CPU-profiler 경고도 보존한다. §42 읽기 스크립트의 첫 GameplayTag 생성자 오류는 import_text 방식으로 고쳤으며 수정본은 재실행에 성공했다.
- 인계 직전 새 읽기: **PIE=false·TestMap03·운영 BP_LNPGameMode·열린 에셋0·DebugLocomotion/DebugNotifies/DebugFireAim=0·RifleAim=1**. §42의 slomo1 복원/월드 Cleanup은 명령 로그와 사용자 회신으로 확인했다. TimeDilation은 범용 속성 읽기에서 노출되지 않았다.
- §42 종료 때 AG/S/BP clean, §40 종료 때 시험14개clean을 확인했다. 인계 시 연결14개/보호7개/빌드 C++13개 해시를 재대조한다. GameMode는 메모리에서만 복원했고 맵은 저장하지 않았다. 디스크 사용자 맵의 시험 GameMode는 재시작/재로드 뒤 다시 적용될 수 있으므로 다음 시험 전 실제 값을 읽는다.
- 현재 AG 해시 `4C7DC2701B525FF2E87BF8FC5E34D644A6D4E56AAF3D24180771BF98D5F996B6`, BP 해시 `34354E9C64F6A152F9DC541234BE62CDAE2F048390B40F6B9CA45C68593D63A0`다. 과거 AG 백업으로 덮어쓰지 않는다.

## 로컬 근거와 커밋 범위

| 근거 폴더(Saved/AnimationOverhaul 아래) | 내용 |
|:---|:---|
| 2026-10-29_UAF52DisplayBridge | §36 표시/IK·물리·근적도와 종료 근거 |
| 2026-10-29_UAF52RifleOverlay | §37 LyraFrame 생성/연결·31 task·근적도 종료 근거 |
| 2026-11-02_UAF52RifleAim | §38 Before*·빌드/TestsTiming·First/MoveStop/Timing 계측·근적도/종료 비교 |
| 2026-11-02_UAF52RifleFire | §40 GUI·FirstPIE/Summary·EvaluationProof/TasksRaw·Closure* |
| 2026-11-02_UAF52ReloadAdditiveReview | §41 기존 생략/그래프 조회·Preserved* |
| 2026-11-02_UAF52RootMotion | §42 Idle/DashInspection·InspectionLogTail·Closure*·Handoff* |

Saved는 git 제외다. 같은 작업 폴더의 새 세션에서는 재사용하고, 다른 환경에서는 추적된 스크립트와 실행서 요약으로 재현한다. §40 녹화는 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261102_125719631.utrace`다. 이전 실패/수정 이력은 `history/Phase05_Log.md`와 실행서가 소유한다.

§35~§36은 `6523d60`, §37은 `e5b1cb5`에 있다. 이번 단일 커밋 범위는 **§38 C++13개·현재 AG·인계용 읽기 스크립트·§38~§42 문서**다. §42에서 찾은 Release 판정 수정은 포함하지 않는다. 사용자 변경4개는 해시를 보존하고 커밋에서 제외한다: `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. 트리를 깨끗하게 만들기 위한 원복/맵 전체 저장은 하지 않는다.

전체 빌드는 에디터 정상 종료 후 `Build.bat LootNPopEditor Win64 Development D:/UnrealProjects/LootNPop/LootNPop.uproject -WaitMutex -NoHotReloadFromIDE`로 처음부터 권한 확장 실행한다. 과거 Live Coding 크래시 원인은 미확정이다. GASP 에디터/소스 에셋은 읽기 전용이며 내부 UAF API는 엔진 갱신 때 재검토한다.