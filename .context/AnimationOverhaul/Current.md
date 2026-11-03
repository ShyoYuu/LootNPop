# 애니메이션 개선 현재 작업 상태

> 상태: 다음 단위 사용자 확인 대기
> 마지막 갱신: 2026-11-03(§52 Manny 리타깃·분리 경로 조사 완료)
> 현재: **5-0·5-1 완료, 5-2 진행 중. AN-028(Remap Pose 엔진 use-after-free)로 Go가 막혀 있고, 사용자 결정으로 엔진 수정 뒤 바로 판정할 수 있게 남은 항목을 계속 측정한다.** §51에서 왼손 IK를 물리 CR 안 Basic IK로 옮겨 통과했다. §52 조사에서 Manny 리타깃·원본/표시 분리도 5.8 경로가 있음을 확인했다(구현은 엔진 업데이트 뒤). 최종 Go/No-Go는 미판정이며 Phase 6은 승인 전이다. Phase 0·1A·1C·2·3 완료, 1B 대기.

§51까지의 변경은 `git log`의 "표시 왼손 IK" 커밋에 있다. 커밋에서 제외한 것: 사용자 에셋 4개(`TestMap03.umap`·`DT_PlayerBaseStats`·`DT_LongSword_Levels`·`DA_MassSpawnConfig_TestOnePerSpecies`). 구현 단위마다 계속할지 확인하고, PIE/GUI 수동 조작이 빠르면 즉시 요청한다. 시험 중 Remap Pose GC 크래시(AN-028)가 날 수 있다(콜스택 `FRemapPoseDataPool::GarbageCollect`로 구분).

## 다음 세션에서 바로 할 일

1. `CLAUDE.md`·`README.md`·`Decisions.md`와 `phases/Phase05_UAFSpike.md` §4·§48·§49·§51·§52를 읽는다.
2. 엔진 업데이트 전 남은 후보를 사용자와 정한다. ⓐ 물리 CR 상시 스텝 비용 측정. ⓑ 피격 시 팔·라이플 과출렁임 튜닝. Remap 풀을 피하는 적 표시 복사 우회(§52 "AN-028과의 관계")는 사용자 결정으로 시도하지 않고 엔진 업데이트를 기다린다(2026-11-03). Manny 리타깃·견착·분리 그래프 구현은 엔진 업데이트 뒤 §52 권장 그래프로 진행한다.
3. 롱소드 Idle은 기본 자세가 없어 왼팔이 자루로 쭉 뻗는다. IK 결함이 아니며 이 트랙 밖이다.

## 재현 도구와 시험 설정

- 적 시험: 에디터 콘솔 `LNP.Spawn.ConfigOverride /Game/Characters/UAFSpike/DA_MassSpawnConfig_UAFSpikeEnemy.DA_MassSpawnConfig_UAFSpikeEnemy`(호스트·게스트 모두, 월드 시작 전). 사본이 없으면 `py "Scripts/AnimOverhaul/SetupEnemyUAFSpike_5-2.py"`로 만들고 `BP_PlayerUAFSpike`의 `UAFLocomotion`을 `BP_EnemyUAFSpike`에 붙여넣는다(이미 있는 BP·EntityConfig는 다시 저장하지 않는다).
- ANS 감사 `LNP.UAFSpike.DebugNotifies 1`은 플레이어·적 시험 폰 모두를 기록한다. 시험 CVar `LNP.UAFSpike.DestroyBeforeFirstEvent`는 적을 첫 입력 틱에서 파괴한다(기본 0). 1은 스폰 다음 프레임, 2는 스폰 프레임이다. 2에 `LNP.UAFSpike.DisableTicksOnRelease 0`을 더하면 `AsShared` 단언이 결정적으로 재현된다(에디터가 종료된다).
- 랙돌 시작 자세: `LNP.UAFSpike.RagdollHoldPose`(기본 1, 0이면 참조 포즈 평가인 이전 동작), `LNP.UAFSpike.DebugRagdoll 1`이면 랙돌 초반·텔레포트마다 `[UAFRagdollProbe]`로 버퍼·바디 손 간격(참조 포즈 ≈104cm)을 남긴다.
- 랙돌 프로브: `ProbeEnemyRagdoll_5-2.py [all]`·`ProbeGuestSkinnedMeshes_5-2.py`·`ProbeRagdollRefPose_5-2.py`. PIE를 Pause한 뒤 상태 표시줄 Cmd 입력창에서 `py "<경로>"`로 실행하고 결과는 `Saved/AnimationOverhaul/2026-11-03_UAF52EnemyPath/`의 JSON으로 읽는다. 게스트 창은 SlateInspector `Screenshot`으로 캡처한다(데스크톱 캡처 금지).
- 루트모션 읽기 `Scripts/AnimOverhaul/InspectUAFRootMotion_5-2.py`(§42~§43)는 그대로 유효하다.

## 유지할 구현과 수용 기준

- **AN-025:** 즉시 사격·낮은 입력 지연과 현재 로컬 라이플 총열 보정을 유지한다. 육안 품질을 충족한 일반 이동 시차 잔차는 허용한다.
- **AN-026·AN-027·AN-028:** Go면 ABP 전부 폐기, 적 경로·표시 후처리 UAF 이전이 Go 조건, 루트모션 워핑은 판정 제외, Remap Pose 엔진 결함은 Go 차단 요인.
- AG 순서: MM/Fall_Loop·History·Bool 보완 → Strafe/입력 어댑터 → Rifle Idle/AO·Blend Layer → FullBodyAdditivePreAim → DefaultSlot → FullBody → UpperBody+마스크 → OffsetRoot/구면 보정 → CR/Passthrough+Rifle Aim → 물리 CR(§49) → Output. 다자식 노드를 추가하면 `[UAFSteering] ready=1`을 다시 확인한다(§46).
- 시험 폰(플레이어·적)은 원본 `ABP_SourceUAFSpike`의 일반 평가=false, 표시 메시 애니 끔, UAF가 Remap→Write Pose로 표시 메시에 쓴다. 왼손 IK는 물리 CR 맨 앞 Basic IK이며 목표는 무기 앵커 본 공간 오프셋이다(§51). 랙돌 중에는 쓰기를 멈추고 표시 메시를 엔진 평가로 돌리되, 사망 순간 포즈를 내는 그래프 없는 인스턴스(`ULNPUAFRagdollPoseAnimInstance`)를 건다(§49·§50). 기본 플레이어/적은 변경하지 않았다.
- 사용 중인 시퀀스 삭제/배치 overwrite와 과거 손 고정 후보 복원은 금지한다. 재생성 절차는 README §4다.
- 승격 적 Actor는 강등 때 파괴되고 승격 때 새로 스폰된다(Mass 액터 풀 미사용, `bKeepLowResActors=false`). 풀 재사용을 전제로 한 설계·시험을 하지 않는다.

## 로컬 근거와 커밋 범위

| 근거 폴더(Saved/AnimationOverhaul 아래) | 내용 |
|:---|:---|
| 2026-11-02_UAF52PhysicsSpike | §49 물리 CR·포즈 복사·랙돌 근거와 스크립트 |
| 2026-11-03_UAF52LeftHandIK | §51 CR·AG Before 사본·해시·Build/Build2 로그 |
| 2026-11-03_UAF52EnemyPath | §50 Setup*.json·PIE1P.log·RagdollProbe/SkinnedProbe/RefPoseProbe·GuestTPose*.png·Crash_AsShared.log·Crash_RemapPoseGC.log·AsSharedRepro_B/AsSharedFix_A·C(재현·대응 로그)·RagdollHoldPose_AB.log(랙돌 시작 자세 A/B) |

이전 단위(§36~§48)의 근거 폴더와 커밋 해시는 `history/Phase05_Log.md`와 실행서가 소유한다. Saved는 git 제외다. 전체 빌드는 에디터 정상 종료 후 `Build.bat LootNPopEditor Win64 Development D:/UnrealProjects/LootNPop/LootNPop.uproject -WaitMutex -NoHotReloadFromIDE`로 권한 확장 실행한다. GASP 에디터/소스 에셋은 읽기 전용이다.
