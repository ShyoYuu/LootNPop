# 애니메이션 개선 로드맵

> 상태: 기준 로드맵
> 읽기 조건: Phase 전환, 전체 순서 변경, 작업 범위 재산정 시
> 마지막 갱신: 2026-10-16

## 1. 진행 원칙

- **구현 단위 하나 = 세션 하나.** 각 단위는 그 자체로 검증하고 커밋할 수 있는 상태로 끝난다. 단위가 끝나면 사용자 진행 확인을 받는다.
- **실현 위험이 큰 것부터.** 노력은 크지만 위험이 낮은 ABP 재구성(Phase 2)보다, 될지 안 될지 모르는 스파이크(Phase 1)를 먼저 끝낸다.
- **GASP는 따라 만든다(AN-001).** GASP 에디터에서 블록을 읽고 LootNPop에서 직접 만든다. 소스 에셋만 마이그레이트한다.
- **미래 Phase의 상세 실행서는 착수 직전에 쓴다.** 이 문서는 요약만 소유한다.
- 모든 단위의 완료 조건에 다음을 포함한다.
  - 적도 근처(중력 Up이 월드 Z와 크게 다른 지점) PIE 확인
  - 해당 변경이 `ActorPromoted` 적에 미치는 영향 확인(적은 같은 `ABP_Lyra`를 쓴다)
  - 네트워크가 걸린 변경은 PIE 2인(리슨) 확인
  - 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인(C++ 변경 시)

## 2. 트랙과 의존성

```
[본 트랙]
Phase 0 참조 환경과 기준 결정
  ├─→ Phase 1A 물리 히트리액트 스파이크 ──────────────┐
  ├─→ Phase 1B 마커리스 모캡 스파이크 ──────────────┼─→ Phase 4 모캡 근접 세트 (상시 병행)
  └─→ Phase 1C GASP 소스 에셋 최신화 ─→ Phase 2 GASP Mover ABP 재구성 ─┤
                                                     └─→ Phase 3 물리 히트리액트 본 구현 (1A + 2 필요)
Phase 2 완료 ─→ Phase 5 UAF 판정 스파이크 ─→ (Go) Phase 6 UAF 마이그레이션

[트랙 L — 최후순위, 독립 (AN-008)]
L1 Locomotor 구면 검증 → L2 드론 전투 설계 → L3 이식
```

- Phase 1A·1B·1C는 Phase 0 뒤에 서로 독립이라 **병렬 세션**으로 돌릴 수 있다. 같은 에셋을 건드리는 단위끼리는 동시에 돌리지 않는다(1C는 `GASP_Mannequin` 폴더와 적 에셋 참조를, Phase 2는 `ABP_Lyra`를 바꾼다).
- Phase 2는 1C가 끝난 뒤 최신 시퀀스로 시작한다. 낡은 시퀀스로 블록을 짠 뒤 교체하면 PSD·Chooser를 두 번 맞추게 된다.
- Phase 3은 Phase 2가 끝나 최종 포즈 파이프라인이 정해진 뒤에 들어간다(물리 노드 위치가 거기에 달려 있다).
- Phase 4는 1B를 통과하면 Phase 2·3과 병행한다. 근접 몽타주·ANS만 다루고 ABP는 건드리지 않는다.

## 3. Phase 요약

| Phase | 목표 | 세션 단위 | 상태 | 핵심 완료 조건 |
|:---|:---|:---|:---|:---|
| 0 | 참조 환경과 기준 결정 | 0-1 MCP 읽기 범위 확인 · 0-2 GASP 블록 목록 · 0-3 스켈레톤·소스 에셋 규칙 결정 (1~2세션) | 진행 중(0-1 완료) | `design/GASPReference.md` 생성, AN-011 결정, 마이그레이트 규칙 확정 (`phases/Phase00_ReferenceSetup.md`) |
| 1A | 물리 히트리액트 스파이크 | 1세션 | 계획 | 테스트 폰의 `VisualMesh` 상체에 PhysicsControl 임펄스, 적도에서 처짐 없음, 2인 PIE 각 머신 로컬 재생, 캡슐·Mover 무영향 |
| 1B | 마커리스 모캡 스파이크 | 1~2세션 | 계획 | 영상 1클립 → MetaHuman 바디 → 리타깃 → 루트모션·검 방향 정리 → 공격 몽타주로 PIE 1타 |
| 1C | GASP 소스 에셋 최신화 | 1C-1 대조표·백업 · 1C-2 경로 정렬 · 1C-3 덮어쓰기 · 1C-4 회귀 (3~4세션) | 계획 | 옛 GASP 유래 411개를 5.8 최신본으로 교체, LootNPop 산출물 보존, 깨진 참조 0, ISKM·승격 적 회귀 없음 (`phases/Phase01C_GASPAssetRefresh.md`) |
| 2 | GASP Mover ABP 재구성 | 블록당 1세션, 5~7세션 | 계획 | 아래 §4. 블록마다 적도·NPC·2인 확인, 무기 레이어·슬롯·ANS 회귀 없음 |
| 3 | 물리 히트리액트 본 구현 | 2~3세션 | 계획 | 히트리액트 몽타주 대체, 방향·강도 반영, 플레이어·`ActorPromoted` 적, 2인 PIE |
| 4 | 모캡 근접 세트 확충 | 무기 세트당 1~2세션 | 계획 | 루트모션 공격이 근접 보정 Motion Warping 경로를 탐, ANS 5종 배치, 칼날 판정 일치 |
| 5 | UAF 판정 스파이크 | 1~2세션 | 계획 | 체크리스트(§4 Phase 5) 전 항목 판정, Go/No-Go 기록 |
| 6 | UAF 마이그레이션 | 착수 시 산정 | 보류(Phase 5 Go 시) | Phase 2 결과를 UAF로 옮기고 회귀 없음 |
| L1~L3 | Locomotor 사족 드론 | 착수 시 산정 | 보류(최후순위) | 아래 §4 트랙 L |

## 4. Phase별 범위 보충

### Phase 1A — 물리 히트리액트 스파이크

- 랙돌과 같은 `VisualMesh`가 대상이다(`EnterRagdoll`이 `VisualMesh`의 PhysicsAsset을 쓴다). `AnimSourceMesh`는 포즈 원본이라 건드리지 않는다.
- PhysicsControl **컴포넌트** 방식과 ABP의 **RigidBodyWithControl 노드** 방식 중 하나를 고르고 근거를 남긴다. `VisualMesh`가 포즈를 받는 경로와의 순서가 판단 기준이다.
- 구면 중력: 시뮬레이션되는 상체 바디의 Chaos 중력을 끄고, 필요하면 기존 `TickRagdollGravity` 패턴(중력 Up 방향 가속)을 재사용한다.
- 다리·골반은 키네마틱, 상체는 부모 공간 모터로 애니 포즈에 복귀한다.
- 확인: 사망 랙돌 진입·`ExitRagdoll`(표현 풀 재사용)과 충돌하지 않는다.

### Phase 1B — 마커리스 모캡 스파이크

- 준비: `MetaHuman Animator`·`Capture Manager`·Live Link Hub와 Fab의 마커리스 플러그인(약 5GB). Windows 전용, Experimental.
- 파이프라인: 영상 → MetaHuman 바디 애니 → IK 리타깃(AN-011 스켈레톤) → 루트모션 추출·발 접지 정리 → 검 방향 보정(무기는 추적되지 않는다) → 몽타주.
- 영상 선택 기준: 고정 카메라, 전신 노출, 컷·히트스톱·VFX 가림 없는 구간.
- 산출: 파이프라인 단계별 소요·실패 지점·수작업 비율을 `research/`에 기록해 Phase 4 산정 근거로 쓴다.

### Phase 1C — GASP 소스 에셋 최신화

- 옛 GASP에서 가져온 `Content/Characters/GASP_Mannequin/`(MotionMatchingData 제외)을 중복이어도 5.8 최신본으로 바꾼다(AN-012).
- 방식은 폴더를 GASP 원본 경로(`/Game/Characters/UEFN_Mannequin/`)로 옮긴 뒤 파일을 덮어쓰는 것이다(AN-013 제안). 411쌍의 참조 교체를 피하고, 다음 GASP 업데이트도 덮어쓰기로 끝낸다.
- 적이 이 스켈레톤·메시를 쓰므로 회귀 검증의 중심은 ISKM 순수 엔티티 적과 `BP_LNPEnemy`다.
- 상세는 `phases/Phase01C_GASPAssetRefresh.md`.

### Phase 2 — GASP Mover ABP 재구성 (블록 순서는 Phase 0-2에서 확정)

잠정 블록 목록. 블록 하나가 세션 하나다.

| 단위 | 블록 | 비고 |
|:---|:---|:---|
| 2-1 | 블렌드 스택 + Chooser 기반 PSD 선택(상태별 DB) | 현 모션매칭 노드 교체. AN-003 |
| 2-2 | Offset Root Bone + Orientation·Stride Warping | 컴포넌트 공간 Up을 쓰는지 확인 |
| 2-3 | Foot Placement(`CR_Biped_FootPlacement` 구성 참조) | 접지 트레이스 방향 |
| 2-4 | 정지·회전 전환, 경사 애니메이션 | `SlopeWarping`은 기본 `GravityDir`(월드 -Z)를 쓰지 않는다 |
| 2-5 | `ActorPromoted` 적 검증 | AI 입력(`SetAIOrientationIntent`)으로 궤적 예측, 게스트 재시뮬레이션 떨림 |

유지 조건: `ALI_WeaponStyles` 레이어, 슬롯 3종, AO 경로, 공중 `Fall_Loop` 고정(AN-007), 근접 보정 Motion Warping 어댑터.

### Phase 3 — 물리 히트리액트 본 구현

- `LNPGameplayCueNotify_HitReact` → `PlayHitReact` 경로에서 몽타주 대신 임펄스(방향·강도)를 준다. `CHT_Montage`의 HitReaction 행 정리.
- HitStop(`CustomTimeDilation`)과 물리 시뮬레이션의 상호작용을 확인한다.
- 경직 몽타주(Light/Heavy/Parried)는 그대로 둔다(AN-004).

### Phase 4 — 모캡 근접 세트 확충

- 루트모션이 실린 공격은 근접 보정이 자동으로 Motion Warping 경로를 탄다(코드 존재, [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §6.6).
- 섹션 기반 콤보·ANS 5종 배치, `sword_tip`/`sword_root` 궤적과 판정 일치 확인.

### Phase 5 — UAF 판정 스파이크

무기 없는 플레이어 1명을 `MoverAnimNext`(Mover UAF)와 UAF 모션매칭으로 구동하고 아래를 판정한다.

- [ ] Phase 2의 PSD·Chooser를 그대로 재사용
- [ ] 몽타주 재생과 노티파이·노티파이 스테이트(특히 `Montage_Stop` 시 `NotifyEnd` 보장)
- [ ] 무기별 레이어 교체에 해당하는 기능(`UAFLayering`)
- [ ] Motion Warping·루트모션의 Mover 연동
- [ ] 시뮬레이티드 프록시·게스트 재시뮬레이션
- [ ] Rewind Debugger 등 디버깅 도구
- [ ] 적도 근처 동작

하나라도 동등하지 않으면 No-Go(AN-006).

### 트랙 L — Locomotor 사족 드론 (최후순위, AN-008)

- **L1 구면 검증:** 엔진 `Locomotor`는 월드 Z를 Up으로 하드코딩한다(`research/UE58_AnimationFeatures.md` §3). 프로젝트 `Plugins/`로 포크해 Up을 파라미터화하고 적도 보행을 판정한다.
- **L2 전투 설계:** GASP에는 이동만 있다. 공격 수단(Control Rig 절차 공격, `SK_Mech` 전용 클립 제작)과 물리 피격(PhysicsControl, Physics Asset 신규 제작 필요)을 정한다. 드론은 Control Rig가 Actor를 요구하므로 `ActorPromoted` 전용이다.
- **L3 이식:** 루트 소유권(Mover 캡슐 vs Locomotor 몸통 이동), 승격 전 ISKM 구간용 베이크 클립, 사족 체형과 캡슐 판정 규약.
- 착수 조건: 본 트랙 Phase 3 이후 또는 사용자 호출.

## 5. 범위 밖과 후속 작업

- 파워드 래그돌 넉다운·기상(AN-005). [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §8의 넉다운 몽타주 확보는 그와 별개로 몽타주 교체만으로 할 수 있다.
- `PureEntity`(ISKM) 적의 애니메이션 — ISKM은 물리·절차 애니메이션을 쓸 수 없다. UAFMass 플러그인은 관찰만 한다.
- 원거리 무기 왼손 IK, 이동 중 상체 공격 같은 기존 잔여 작업([TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §8)은 Phase 2 중 자연스럽게 걸리면 같이 처리하되 이 로드맵의 완료 조건에는 넣지 않는다.
