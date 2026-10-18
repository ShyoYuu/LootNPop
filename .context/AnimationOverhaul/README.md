# 애니메이션 대규모 개선 작업 안내

> 상태: 활성
> 역할: 이 작업의 진입점과 문서 라우팅
> 마지막 구조 갱신: 2026-10-16

## 1. 목적

UE 5.8 Game Animation Sample(이하 GASP)의 애니메이션 구성을 LootNPop에 들여와 플레이어·적의 움직임 품질을 올린다. 여러 세션에 걸쳐 진행하며, **구현 단위 하나 = 세션 하나**를 기본으로 한다.

핵심 목표:

- 옛 GASP 버전에서 가져온 소스 에셋을 GASP 5.8 최신본으로 교체한다(AN-012).
- GASP Mover ABP 구성을 따라 플레이어와 `ActorPromoted` 적의 로코모션을 재구성한다.
- GASP 5.8의 PhysicsControl(물리 애니메이션)로 가벼운 히트리액트 몽타주를 대체한다.
- UE 5.8 마커리스 모션 캡처로 영상에서 근접 무기 모션을 뽑아 공격 세트를 늘린다.
- 위 결과를 UAF로 옮길 수 있는지 판정하고, 통과하면 마이그레이션한다.
- (최후순위 별도 트랙) GASP Locomotor 사족 드론을 신규 NPC로 쓸 수 있는지 검증한다.

범위 밖: 파워드 래그돌 넉다운·기상처럼 캡슐을 물리에 끌려가게 하는 연출(AN-005), `PureEntity`(ISKM) 적의 애니메이션 체계, 데디케이티드 서버 전용 처리. 상세는 `Roadmap.md` §5.

## 2. 세션 시작 시 필수 문서

1. `README.md`
2. `Current.md`
3. `Decisions.md`

이 세 문서를 읽은 뒤 현재 단위에 필요한 문서만 아래 라우팅 표에 따라 추가로 읽는다. 현재 단위와 먼 `phases/` 문서와 `history/`는 기본 컨텍스트에 올리지 않는다.

## 3. 문서 라우팅

| 작업 | 추가로 읽을 문서 |
|:---|:---|
| 전체 순서 조정·Phase 전환·트랙 확인 | `Roadmap.md` |
| 현재 단위 구현 | `Current.md`가 가리키는 `phases/PhaseNN_*.md` |
| UE 5.8 기능 사실(UAF·PhysicsControl·Locomotor·마커리스·워핑 노드 Up 축) | `research/UE58_AnimationFeatures.md` |
| GASP 블록별 구성 조사 결과 | `design/GASPReference.md` (Phase 0에서 생성) |
| GASP 소스 에셋 최신화(경로 정렬·덮어쓰기·적 회귀) | `phases/Phase01C_GASPAssetRefresh.md`, `research/UE58_AnimationFeatures.md` §6 |
| 물리 히트리액트(Phase 1A 결과·RBWC 설정·Phase 3 인계) | `research/PhysicsHitReactSpike.md` |
| 현재 메인 ABP·무기 레이어·몽타주·ANS·근접 보정 | [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) |
| 구형 중력·Mover·랙돌(§9) | [TechDesign_CharacterMovement.md](../TechDesign_CharacterMovement.md) |
| 경직(그로기·다운)과 히트리액트의 경계 | [TechDesign_Poise.md](../TechDesign_Poise.md) |
| 적 Actor 승격·표현 풀 재사용 | [TechDesign_EnemyNPC.md](../TechDesign_EnemyNPC.md) |
| ISKM·PureEntity와의 경계 | [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) |
| 복제·게스트 재시뮬레이션 | [TechDesign_Networking.md](../TechDesign_Networking.md) |
| 과거 경위·회귀 원인 조사 | 관련 `history/PhaseNN_Log.md` |

## 4. GASP 참조 환경

| 항목 | 값 |
|:---|:---|
| GASP 로컬 프로젝트 | `D:/UnrealProjects/GameAnimationSample` (EngineAssociation 5.8) |
| GASP 에디터 MCP | `unreal-mcp-gasp` → `http://127.0.0.1:8001/mcp` (GASP 쪽 `Unreal MCP`·`AllToolsets` 활성화, 포트 8001) |
| LootNPop 에디터 MCP | `unreal-mcp` → `http://127.0.0.1:8000/mcp` |
| 동시 실행 | 두 에디터를 함께 띄워 GASP에서 읽고 LootNPop에서 만든다 |

- GASP 에디터는 **읽기 전용 참조**다. 그쪽에서는 저장·수정 툴을 쓰지 않는다(AN-009).
- MCP 서버 목록은 세션 시작 때 로드된다. `unreal-mcp-gasp`가 안 보이면 GASP 에디터가 떠 있는지, 포트가 8001인지 확인하고 세션을 다시 연다.
- 큰 ABP를 통째로 덤프하지 않는다. 블록 단위(블렌드 스택, Offset Root Bone, Foot Placement…)로 그래프를 좁혀 읽는다.
- MCP로 읽을 수 있는 범위와 방법은 Phase 0 단위 0-1이 확정해 `design/GASPReference.md` §1에 남긴다.

**새로 클론한 환경에서 소스 에셋 확보(AN-016).** 저장소에는 구성 에셋만 있고, 그것이 참조하는 GASP 소스 에셋과 리타깃 산출물은 없다. ① Fab에서 "Game Animation Sample Project"를 **UE 5.8 버전**으로 받아 `D:/UnrealProjects/GameAnimationSample`에 프로젝트를 만든다. ② LootNPop 에디터를 끈 상태에서 반입 목록의 경로를 GASP `Content/`에서 LootNPop `Content/`로 **같은 상대 경로 그대로** 복사한다. 반입 목록은 `phases/Phase01C_GASPAssetRefresh.md`가 소유한다(`Characters/UEFN_Mannequin/`의 필요분, `Misc/SandboxAnimCurveCompressionSettings`, `Blueprints/AnimModifiers/`). GASP 경로 안의 PSD·Chooser·ABP는 복사하지 않는다. ③ `Content/Characters/Retargeted/`(Lyra → UEFN 리타깃 산출물, 적 리타깃 애니 포함)는 각 산출 단위가 남긴 재생성 절차로 다시 만든다. ④ 에디터를 켜고 깨진 참조와 로드 경고를 확인한다. Foley·EarlyTransition 노티파이 누락은 `CoreRedirects`로 처리되므로 경고가 나오면 안 된다(AN-017). Phase 1C-2 전까지는 옛 경로 `Content/Characters/GASP_Mannequin/`을 쓰므로, 그때까지는 기존 작업 환경의 폴더를 그대로 복사해 온다. Lyra·MoverSample·Close Combat: Swordsman·무기 같은 다른 제외 에셋은 이 절차의 범위 밖이다.

## 5. 아키텍처 한 장 요약

```
[ALNPCharacterBase]
  AnimSourceMesh ── ABP_Lyra (ULNPAnimInstance)      ← Phase 2가 GASP 구성으로 재구성
  │  (숨김)          ├ 로코모션 (모션매칭 → GASP식 MM 노드·워핑·발 배치)
  │                  ├ 공중 Fall_Loop 고정 (AN-007)
  │                  ├ Linked Anim Layer (무기별 서브 ABP)
  │                  └ Slot DefaultSlot / FullBody / UpperBody (몽타주·ANS)
  │   현재: SKM_Manny(Lyra) → 2-0b부터: SKM_UEFN_Mannequin + UEFN 메인 ABP (AN-011)
  │
  VisualMesh ───── ABP_Retarget로 AnimSourceMesh 포즈를 받아 그린다  ← 랙돌·물리 히트리액트의 대상 메시
                     ├ 플레이어 SKM_Manny_Simple(MoverSample), 적 SKM_UEFN_Mannequin
                     └ (Phase 3) PhysicsControl 상체 임펄스 — 코스메틱, 비복제
```

`ActorPromoted` 적은 같은 `ABP_Lyra`를 공유하므로 Phase 2 결과를 자동으로 받는다. 무기·몽타주용 Lyra 애니는 UEFN으로 리타깃한 사본(`Content/Characters/Retargeted/`, git 제외)을 쓴다(AN-011·AN-016).

## 6. 핵심 용어

| 용어 | 의미 |
|:---|:---|
| GASP | Epic의 Game Animation Sample Project. 이 작업의 참조 원본 |
| 소스 에셋 | 애니메이션 시퀀스·스켈레탈 메시처럼 직접 만들지 않고 가져오는 원재료. 마이그레이트하고 git에서 제외한다(AN-001) |
| 구성 에셋 | ABP·Chooser·PSD·PSS·Control Rig·PhysicsControl 프로필처럼 LootNPop에서 직접 만드는 것. git에 추적한다 |
| 블록 | GASP Mover ABP를 쪼갠 기능 단위(블렌드 스택, Offset Root Bone, 워핑, 발 배치…) |
| 물리 히트리액트 | 피격 순간 상체 바디에 임펄스를 주고 모터로 애니메이션 포즈에 복귀시키는 코스메틱 반응 |
| 판정 게이트 | 통과 여부로 다음 Phase 착수를 가르는 스파이크(UAF·Locomotor) |

## 7. 모든 구현이 지켜야 하는 불변 조건

- **구면 월드.** 위치마다 중력 Up이 다르다. 새로 넣는 애니 노드·Control Rig·물리 설정은 월드 Z를 Up으로 가정하는지 먼저 확인하고, 적도 근처에서 검증한다(`research/UE58_AnimationFeatures.md` §5).
- **게임플레이와 연출 분리.** 물리 애니메이션은 캡슐·Mover·판정을 움직이지 않는다. 히트리액트는 코스메틱이고 복제하지 않는다(AN-004).
- **무기 레이어·슬롯·ANS 계약 유지.** `ALI_WeaponStyles` 레이어 교체, `DefaultSlot`·`FullBody`·`UpperBody` 슬롯, ANS 5종의 시각 계약을 깨지 않는다. ANS는 근접 판정 엔티티 수명과 입력 차단을 소유한다.
- **선택 로직은 Chooser와 C++에.** ABP 그래프에 분기 로직을 쌓지 않는다(AN-003).
- **애니 변수는 `ULNPAnimInstance`가 공급한다.** 속도·가속은 캐릭터 Up에 수직인 접평면으로 투영한 값만 넘긴다.
- **ActorPromoted 회귀 금지.** 적은 컨트롤러가 없고, 게스트는 적 Actor 이동을 재시뮬레이션한다. 플레이어 변경은 매번 적 경로도 확인한다.

## 8. 문서별 소유 정보

- `README.md`: 작업 범위, 불변 조건, 참조 환경, 문서 라우팅
- `Current.md`: 지금의 상태와 바로 다음 행동
- `Decisions.md`: 모든 세션이 알아야 하는 확정 결정(`AN-` 접두어)
- `Roadmap.md`: 트랙·Phase 순서, 세션 단위, 게이트
- `design/`: 현재 유효한 상세 설계와 GASP 참조 조사
- `research/`: 엔진 사실 조사, 스파이크 결과, 미확정 근거
- `phases/`: 해당 Phase의 단위별 실행 계획과 완료 조건
- `history/`: 끝난 작업 로그, 측정 기록, 시행착오

정보의 원본은 한 곳에만 둔다. 다른 문서에서는 요약과 링크만 둔다. 구현이 끝나 프로젝트 공통 설계가 바뀌면 `TechDesign_CombatAnimation.md` 같은 공통 문서로 옮기고, 이 폴더에는 링크만 남긴다.

## 9. 문서 갱신 규칙

### 세션 시작

1. 필수 문서 세 개를 읽는다.
2. `Current.md`의 현재 단위와 다음 행동을 확인한다.
3. 라우팅 표에서 필요한 문서만 읽는다.
4. 코드·에셋의 실제 상태가 문서와 일치하는지 확인한다.

### 세션 종료

1. `Current.md`를 실제 상태로 갱신한다(이력은 누적하지 않는다).
2. 수행 내용과 검증 결과를 `history/PhaseNN_Log.md`에 날짜별로 추가한다(없으면 만든다).
3. 완료 조건을 충족하면 Phase 문서 체크리스트를 갱신한다.
4. 설계가 바뀌었다면 `design/`과 `Decisions.md`를 먼저 고친다.
5. 다음 세션이 바로 실행할 구체적인 한두 작업을 `Current.md`에 남긴다.

### Phase 종료

1. 완료 조건과 검증 증거를 로그에 남긴다.
2. `Roadmap.md` 상태를 갱신한다.
3. 다음 Phase 실행 문서는 착수 직전에 만들거나 구체화한다.
4. 바뀐 프로젝트 공통 설계 문서를 갱신하고, 문서가 새로 생기면 인덱스 3곳(CLAUDE.md·ProjectOverview·Notion)을 함께 고친다.

## 10. 문서 상태 표기

| 상태 | 의미 |
|:---|:---|
| 활성 | 매 세션의 현재 진입점 |
| 확정 | 변경 전 명시적 재논의가 필요한 결정 |
| 제안 | 사용자 확인 전의 권고 |
| 초안 | 구현·스파이크 결과에 따라 바뀔 수 있음 |
| 검증 필요 | 엔진 또는 프로젝트 실험이 남아 있음 |
| 완료 | 완료 조건과 검증을 통과 |
| 보류 | 착수 조건을 기다리는 중 |
| 폐기 | 쓰지 않으며 기각 이유만 보존 |

## 11. 컨텍스트 예산 원칙

- 필수 세 문서는 합계 약 4,000~6,000토큰 이하를 목표로 한다.
- `Current.md`는 완료 이력을 누적하지 않고 짧게 유지한다.
- 먼 Phase는 `Roadmap.md` 요약만 유지하고 착수 직전에 상세화한다.
- GASP 그래프 조사 결과는 원본 덤프가 아니라 "LootNPop에서 무엇을 어떻게 만들지"로 요약해 `design/GASPReference.md`에 남긴다.
