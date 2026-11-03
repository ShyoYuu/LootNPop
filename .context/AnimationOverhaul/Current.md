# 애니메이션 개선 현재 작업 상태

> 상태: 다음 단위 사용자 확인 대기
> 마지막 갱신: 2026-11-03(§50 ⓓ 적 최소 UAF 경로·AN-028·AsShared 대응)
> 현재: **5-0·5-1 완료, 5-2 진행 중. §50에서 `ActorPromoted` 적 UAF 경로 기능은 통과했지만, 표시 포즈 복사가 쓰는 `Remap Pose`의 엔진 use-after-free를 Go 차단 요인으로 기록했다(AN-028).** 최종 Go/No-Go는 미판정이며 Phase 6은 승인 전이다. Phase 0·1A·1C·2·3 완료, 1B 대기.

§49까지는 `ec9187f`, §50(공용 드라이버·적 시험 클래스·사망 팝 333·시험 에셋 3개·스크립트 5개·문서)은 그 다음 커밋(`git log`의 "ActorPromoted 적 최소 경로")에 있다. 커밋 뒤 unity 포함 에디터 빌드 성공(17 actions·68초). 커밋에서 제외한 것: 사용자 에셋 4개(`TestMap03.umap`·`DT_PlayerBaseStats`·`DT_LongSword_Levels`·`DA_MassSpawnConfig_TestOnePerSpecies`), 다른 세션이 고친 `Source/LootNPop/Replication/LNPMassReplication.cpp`, **임시값** `Config/DefaultGame.ini`의 `EnemyRagdollDuration=10`(시험이 끝나면 지운다). 구현 단위마다 계속할지 확인하고, PIE/GUI 수동 조작이 빠르면 즉시 요청한다.

## 다음 세션에서 바로 할 일

1. `CLAUDE.md`·`README.md`·`Decisions.md`와 `phases/Phase05_UAFSpike.md` §4·§48~§50을 읽는다.
2. **AN-028 뒤 방향을 사용자와 정한다.** 후보: ⓐ 최종 판정을 "엔진 결함으로 보류(No-Go 아님)"로 내리고 Phase 5를 닫는다(AN-006: 다음 엔진 버전에서 재검토). ⓑ 남은 미검증 항목(ⓑ CR Two Bone IK·ⓒ Manny 리타깃·물리 CR 상시 비용)을 계속 측정해 엔진 수정 뒤 바로 판정할 수 있게 한다. ⓒ 플레이어 시험 폰도 같은 Remap 위험을 안고 있으므로 시험을 계속한다면 크래시 가능성을 감수한다.
3. UAF `AsShared` 단언은 메커니즘 확정·대응 완료다(§50 "AsShared 원인과 대응"). 스폰 프레임에 첫 실행 전에 놓인 시스템의 남은 이벤트 틱이 돌며 단언한다. 시험 폰은 놓기 직전 이벤트 틱을 끈다(`LNP.UAFSpike.DisableTicksOnRelease`, 기본 1). 리슨 2인에서 실제로 스폰 프레임 해제를 일으킨 주체는 미확인이다. 대응을 켠 채 재발하면 같은 프레임의 "releases a UAF system before its first event" 경고 유무부터 본다. 미확정으로 남은 것은 게스트 사망 랙돌이 사망 자세와 무관하게 참조 포즈에서 시작하는 현상(전환 뒤 동작·착지는 자연스러움, 원인 미확인). 더 파려면 §50 표와 프로브 스크립트에서 시작한다.
4. `EnemyRagdollDuration=10` 임시값을 지울지 사용자에게 확인한다.

## 재현 도구와 시험 설정

- 적 시험: 에디터 콘솔 `LNP.Spawn.ConfigOverride /Game/Characters/UAFSpike/DA_MassSpawnConfig_UAFSpikeEnemy.DA_MassSpawnConfig_UAFSpikeEnemy`(호스트·게스트 모두, 월드 시작 전). 사본이 없으면 `py "Scripts/AnimOverhaul/SetupEnemyUAFSpike_5-2.py"`로 만들고 `BP_PlayerUAFSpike`의 `UAFLocomotion`을 `BP_EnemyUAFSpike`에 붙여넣는다(이미 있는 BP·EntityConfig는 다시 저장하지 않는다).
- ANS 감사 `LNP.UAFSpike.DebugNotifies 1`은 플레이어·적 시험 폰 모두를 기록한다. 시험 CVar `LNP.UAFSpike.DestroyBeforeFirstEvent`는 적을 첫 입력 틱에서 파괴한다(기본 0). 1은 스폰 다음 프레임, 2는 스폰 프레임이다. 2에 `LNP.UAFSpike.DisableTicksOnRelease 0`을 더하면 `AsShared` 단언이 결정적으로 재현된다(에디터가 종료된다).
- 랙돌 프로브: `ProbeEnemyRagdoll_5-2.py [all]`·`ProbeGuestSkinnedMeshes_5-2.py`·`ProbeRagdollRefPose_5-2.py`. PIE를 Pause한 뒤 상태 표시줄 Cmd 입력창에서 `py "<경로>"`로 실행하고 결과는 `Saved/AnimationOverhaul/2026-11-03_UAF52EnemyPath/`의 JSON으로 읽는다. 게스트 창은 SlateInspector `Screenshot`으로 캡처한다(데스크톱 캡처 금지).
- 루트모션 읽기 `Scripts/AnimOverhaul/InspectUAFRootMotion_5-2.py`(§42~§43)는 그대로 유효하다.

## 유지할 구현과 수용 기준

- **AN-025:** 즉시 사격·낮은 입력 지연과 현재 로컬 라이플 총열 보정을 유지한다. 육안 품질을 충족한 일반 이동 시차 잔차는 허용한다.
- **AN-026·AN-027·AN-028:** Go면 ABP 전부 폐기, 적 경로·표시 후처리 UAF 이전이 Go 조건, 루트모션 워핑은 판정 제외, Remap Pose 엔진 결함은 Go 차단 요인.
- AG 순서: MM/Fall_Loop·History·Bool 보완 → Strafe/입력 어댑터 → Rifle Idle/AO·Blend Layer → FullBodyAdditivePreAim → DefaultSlot → FullBody → UpperBody+마스크 → OffsetRoot/구면 보정 → CR/Passthrough+Rifle Aim → 물리 CR(§49) → Output. 다자식 노드를 추가하면 `[UAFSteering] ready=1`을 다시 확인한다(§46).
- 시험 폰(플레이어·적)은 원본 `ABP_SourceUAFSpike`의 일반 평가=false, 표시 메시 애니 끔, UAF가 Remap→Write Pose로 표시 메시에 쓴다. 랙돌 중에는 쓰기를 멈추고 표시 메시를 ABP 없는 엔진 평가로 돌린다(§49). 기본 플레이어/적은 변경하지 않았다.
- 사용 중인 시퀀스 삭제/배치 overwrite와 과거 손 고정 후보 복원은 금지한다. 재생성 절차는 README §4다.
- 승격 적 Actor는 강등 때 파괴되고 승격 때 새로 스폰된다(Mass 액터 풀 미사용, `bKeepLowResActors=false`). 풀 재사용을 전제로 한 설계·시험을 하지 않는다.

## 로컬 근거와 커밋 범위

| 근거 폴더(Saved/AnimationOverhaul 아래) | 내용 |
|:---|:---|
| 2026-11-02_UAF52PhysicsSpike | §49 물리 CR·포즈 복사·랙돌 근거와 스크립트 |
| 2026-11-03_UAF52EnemyPath | §50 Setup*.json·PIE1P.log·RagdollProbe/SkinnedProbe/RefPoseProbe·GuestTPose*.png·Crash_AsShared.log·Crash_RemapPoseGC.log·AsSharedRepro_B/AsSharedFix_A·C(재현·대응 로그) |

이전 단위(§36~§48)의 근거 폴더와 커밋 해시는 `history/Phase05_Log.md`와 실행서가 소유한다. Saved는 git 제외다. 전체 빌드는 에디터 정상 종료 후 `Build.bat LootNPopEditor Win64 Development D:/UnrealProjects/LootNPop/LootNPop.uproject -WaitMutex -NoHotReloadFromIDE`로 권한 확장 실행한다. GASP 에디터/소스 에셋은 읽기 전용이다.
