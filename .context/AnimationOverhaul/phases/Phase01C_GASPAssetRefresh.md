# Phase 1C — GASP 소스 에셋 최신화

> 상태: 초안(착수 직전에 단위별 세부를 다시 확인)
> 선행: Phase 0 완료(MCP 읽기 방법, 폴더·git 규칙 AN-016, 노티파이·모디파이어 처리 AN-017, 스켈레톤 기준 AN-011)
> 후행: Phase 2(2-0a Lyra 세트 UEFN 리타깃부터)는 이 Phase가 끝난 뒤 최신 시퀀스·스켈레톤으로 시작한다
> 순서: 단위 1C-1 → 1C-2 → 1C-3 → 1C-4. 단위마다 커밋하고 사용자 확인을 받는다.

## 배경

`Content/Characters/GASP_Mannequin/`(MotionMatchingData 제외)은 예전 GASP 버전에서 가져온 소스 에셋이다. 사용자 결정으로 **중복 에셋이라도 GASP 5.8 최신본으로 바꾼다**(AN-012).

2026-10-18 조사(`research/UE58_AnimationFeatures.md` §6):

| 구분 | 수 | 처리 |
|:---|---:|:---|
| 최신 GASP `Characters/UEFN_Mannequin/`과 상대 경로가 같은 에셋 | 411 | 최신본으로 교체 |
| 최신 GASP에 없는 에셋 — `Animations/Enemy/*` 14개(LootNPop이 만든 리타깃 적 애니, 5.7MB), 그리고 출처를 판별해야 하는 4개(`Animations/ABP_UEFN`, `Poses/M_Pose_01`, `Run/M_Relaxed_Run_Loop_F_Troy`, `Meshes/SM_UEFN_Mannequin`: 옛 GASP에만 있던 것인지 LootNPop 산출물인지) | 18 | 보존(AN-014). 4개는 1C-1에서 출처 판별 |
| 최신 GASP에만 있는 에셋 | 약 1,950 | 일괄 반입하지 않는다. Phase 2 블록이 요구할 때만 가져온다(AN-001) |
| `MotionMatchingData/`(LootNPop 구성 에셋, git 추적) | 7 | 교체 대상 아님. 참조 유지 확인과 PSD 재빌드 |

**누가 이 에셋을 쓰는가.** 플레이어는 Lyra 스켈레톤(`ABP_Lyra`, `PSD_*_Lyra`)이다. UEFN 스켈레톤·메시는 **적**이 쓴다: `BP_LNPEnemy`, `DA_EnemyEntityConfig_PureEntity_Melee01/Ranged01`, `ASTP_Enemy_UEFN_Sword_Body/Shotgun_Body`, 무기 `SKM_LongSword_UEFN`·`SKM_Shotgun_UEFN`, 리타기터 `RTG_GASP_to_MS`·`RTG_Lyra_to_GASP`·`RTG_Lyra_to_GASP_noRootMotion`. 그래서 이 Phase의 회귀 위험은 대부분 적(특히 ISKM 순수 엔티티)에 있다.

**AN-011 이후.** Phase 2 단위 2-0b부터 플레이어·`ActorPromoted` 적의 `AnimSourceMesh`도 `SKM_UEFN_Mannequin`이 되고, Lyra 무기·몽타주 애니는 이 Phase가 확정한 UEFN 스켈레톤으로 리타깃된다(2-0a). 그래서 1C-1의 스켈레톤 차이 확인은 적뿐 아니라 이후 모든 캐릭터의 기준을 정하는 일이다. 1C가 끝난 뒤 스켈레톤을 다시 바꾸면 리타깃 사본을 다시 만들어야 한다.

## 핵심 방식 — 경로를 GASP 원본에 맞춘 뒤 파일로 덮어쓴다 (AN-013, 확정)

언리얼 에셋은 패키지 경로로 서로를 참조한다. GASP 최신본은 `/Game/Characters/UEFN_Mannequin/...`을 참조하고, LootNPop의 기존 본은 `/Game/Characters/GASP_Mannequin/...`에 있다. 그대로 복사하면 스켈레톤이 두 벌이 되고, 411쌍을 하나씩 Replace References로 통합해야 한다(MCP에 해당 도구가 없어 수작업이 된다).

대신:

1. LootNPop 에디터에서 폴더 `/Game/Characters/GASP_Mannequin/` → `/Game/Characters/UEFN_Mannequin/`으로 **이동**한다(MCP `AssetTools.move`). 참조자(적 BP·DA·ASTP·리타기터·PSD)가 새 경로로 갱신된다.
2. 경로가 GASP와 같아졌으므로, 에디터를 끈 상태에서 411개 파일을 GASP 최신본으로 **덮어쓴다**. 내부 참조가 그대로 맞는다.
3. 앞으로 GASP를 다시 업데이트할 때도 같은 덮어쓰기로 끝난다.

대가: 추적되는 참조 에셋이 한 번 다시 저장되고, `.gitignore` 패턴과 문서 경로를 고쳐야 한다. 제외 대상인 GASP 폴더 이름이 GASP 원본과 같아진다.

## 반입 목록과 경로 규칙 (AN-016·AN-017)

- 반입은 GASP 패키지 경로 그대로 한다. 대상은 `Characters/UEFN_Mannequin/`의 필요분, 시퀀스가 참조하는 `Misc/SandboxAnimCurveCompressionSettings`와 `Blueprints/AnimModifiers/AM_*`(엔진 의존만)이다. GASP 경로 안의 GASP 구성 에셋(PSD·PSS·Chooser·ABP)은 반입하지 않는다.
- Foley 노티파이 10종(`BP_AnimNotify_FoleyEvent_*` → `/Game/Audio/Foley/*`)과 `BP_NotifyState_EarlyTransition`(→ `BPI_SandboxCharacter_ABP`·CMC ABP)은 반입하지 않는다. `Config/DefaultEngine.ini` `[CoreRedirects]`의 `ClassRedirects`로 LootNPop no-op 노티파이 C++ 클래스(`ULNPAnimNotify_GASPPlaceholder` 류, 노티파이·노티파이 스테이트 각 1개)에 연결한다. BP 생성 클래스 → 네이티브 클래스 리다이렉트가 시퀀스 노티파이 인스턴스에서 경고 없이 풀리는지 1C-1에서 시퀀스 1개로 먼저 실험한다. 실패하면 경고를 받아들이고 사유를 로그에 남긴다.
- 소스 파일은 수정하지 않는다(노티파이 제거·재저장 금지). 다음 GASP 업데이트도 같은 덮어쓰기로 끝나야 한다.
- 이 절의 **경로별 반입 목록**이 `README.md` §4 "새로 클론한 환경에서 소스 에셋 확보"의 원본이다. 1C-1에서 표로 채운다.
- LootNPop 구성 에셋은 GASP 경로 폴더 안에 두지 않는다. `GASP_Mannequin/MotionMatchingData/` 7개는 1C-2에서 LootNPop 소유 폴더로 옮긴다.
- LootNPop이 만든 리타깃 산출물은 git에서 제외하고 `Content/Characters/Retargeted/`에 둔다. 재생성 절차(리타기터·대상 목록)를 만든 단위가 남긴다.

## 단위 1C-1 — 대조표와 백업

1. 411개를 **바이트 단위로 비교**(해시)해 실제로 바뀐 파일만 교체 목록에 남긴다.
2. 교체 목록과 Phase 2가 쓸 MM 경로 시퀀스의 **최신본 의존성**을 GASP MCP `get_dependencies`로 따라간다. 0-3 조사로 알려진 외부 의존은 커브 압축 설정, `AM_BakePhaseCurveFromFootstepNotifies`, Foley 10종, EarlyTransition이다(`design/GASPReference.md` §2.7). 위 "반입 목록과 경로 규칙"에 따라 반입 목록과 리다이렉트 대상 클래스 목록을 확정하고, 리다이렉트 실험을 한다. 놓치면 교체 뒤 참조가 깨진다.
3. 스켈레톤·메시 변화를 확인한다(본 목록·가상 본·소켓·커브·바인드 포즈). 바인드 포즈가 바뀌면 ISKM이 영향을 받는다.
4. 교체 전 원본 전체를 `Saved/AssetRefresh/<날짜>/`에 백업하고 해시 목록을 남긴다.
5. 출처 미판별 4개를 참조자와 생성 이력으로 판별한다. 아무도 참조하지 않는 옛 GASP 에셋은 삭제 후보로 올린다(삭제는 사용자 확인 후). `ABP_UEFN`은 `CHT_PSD_UEFN`만 참조하는 옛 UEFN 모션매칭 실험으로 보인다(0-3).
6. LootNPop 산출물(Enemy 14개 + 판별 결과)의 이동 대상을 확정한다. AN-014가 확정됐으므로 `Content/Characters/Retargeted/`로 옮기고 git에서는 계속 제외한다. 14개의 재생성 절차(`RTG_Lyra_to_GASP_noRootMotion`, 원본 Lyra 시퀀스 목록)를 남긴다.

완료 조건: 교체·보존·신규 대조표, 경로별 반입 목록·리다이렉트 대상 목록, 리다이렉트 실험 결과, 스켈레톤·메시 차이 보고, 백업, 출처 판별, LootNPop 산출물 이동 목록·재생성 절차.

## 단위 1C-2 — 경로 정렬 (에디터 작업, 커밋 단위)

1. AN-014에 따라 LootNPop 산출물을 먼저 옮긴다(`AssetTools.move`).
2. `GASP_Mannequin` → `UEFN_Mannequin` 폴더 이동, 리다이렉터 정리.
3. `MotionMatchingData/` 7개를 LootNPop 소유 폴더로 옮긴다(AN-016). `.gitignore`: `Content/Characters/GASP_Mannequin/*`·예외 줄을 지우고 GASP 원본 경로 단위 패턴(`Content/Characters/UEFN_Mannequin/`, `Content/Misc/SandboxAnimCurveCompressionSettings.uasset`, `Content/Blueprints/AnimModifiers/`)과 `Content/Characters/Retargeted/`를 넣는다. `Content/Blueprints/` 전체는 제외하지 않는다(LootNPop이 나중에 쓸 수 있다).
4. 참조 무결성 확인: 이동 전후 참조자 목록 비교, 로드 경고 0.
5. 추적 파일 변경을 커밋한다(경로만 바뀌고 동작은 같아야 한다). PIE에서 적 표시·애니가 그대로인지 확인한다.

완료 조건: 새 경로에서 기존과 같은 동작, 리다이렉터 없음, 커밋.

## 단위 1C-3 — 최신본 덮어쓰기

1. no-op 노티파이 C++ 클래스와 `[CoreRedirects]`를 넣고 `Build.bat`로 빌드한다(1C-1 실험 결과대로).
2. LootNPop 에디터를 끈다. 교체 목록의 파일과 1C-1 반입 목록(커브 압축 설정·`AM_*`)을 GASP에서 같은 경로로 복사한다. GASP 파일은 읽기만 한다(AN-009).
3. 에디터를 다시 켜고 로드 경고·깨진 참조를 확인한다. Foley·EarlyTransition 누락 클래스 경고가 없어야 한다.
4. MCP로 교체 에셋의 클래스·의존성을 다시 조회해 대조표와 맞는지 확인한다.

완료 조건: 깨진 참조 0, 로드 경고 0(또는 원인과 대응 기록).

## 단위 1C-4 — 회귀 검증

1. **ISKM 순수 엔티티 적:** `ASTP_Enemy_UEFN_*` 재베이크가 필요한지, ISKM 거리에서 몸통·무기 메시가 보이고 애니가 도는지 확인한다(바인드 포즈 함정, [TechDesign_EnemyNPC_LowLOD.md](../../TechDesign_EnemyNPC_LowLOD.md) §6.7).
2. **리타깃 적 애니(LootNPop 산출물):** 새 스켈레톤에서 그대로 맞는지, 다시 리타깃해야 하는지 확인한다.
3. **`ActorPromoted` 적:** `BP_LNPEnemy` 승격 후 로코모션·공격·랙돌.
4. **PSD:** `MotionMatchingData/PSD_*_UEFN` 재빌드, 경고 확인.
5. 적도 근처 PIE, 리슨 2인 PIE.
6. 문서: 바뀐 경로를 관련 TechDesign 문서에 반영한다.

완료 조건: 위 항목 통과, `Current.md`·`Roadmap.md` 갱신.

## 완료 조건 체크리스트

- [ ] 1C-1: 대조표·반입/리다이렉트 목록·리다이렉트 실험·스켈레톤 차이·백업·산출물 이동 목록
- [ ] 1C-2: 경로 정렬, `.gitignore`, 커밋
- [ ] 1C-3: 최신본 덮어쓰기, 깨진 참조 0
- [ ] 1C-4: ISKM·리타깃 애니·승격 적·PSD·2인 PIE 회귀 통과
