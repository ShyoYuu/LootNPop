# 애니메이션 개선 확정 결정

> 상태: 확정 결정 원장
> 마지막 갱신: 2026-10-18
> 규칙: 결론만 기록하고 상세 근거와 구현은 링크된 문서가 소유한다. ID 접두어 `AN-`은 다른 작업 폴더(`D-`·`L-`)와 구분하기 위한 것이다.

## 결정 목록

| ID | 결정 | 상태 | 상세 근거 |
|:---|:---|:---|:---|
| AN-001 | GASP 시스템은 에셋을 통째로 마이그레이트하지 않고 **구성을 보며 핵심 블록부터 LootNPop에서 직접 다시 만든다.** 애니메이션 시퀀스·스켈레탈 메시 같은 소스 에셋만 마이그레이트하고 git에서 제외한다(기존 `Content/Characters/GASP_Mannequin/*` 패턴). ABP·Chooser·PSD·PSS·Control Rig·PhysicsControl 프로필 같은 구성 에셋은 직접 만들고 추적한다. | 확정(2026-10-16, 사용자 결정) | `Roadmap.md` §1 |
| AN-002 | GASP의 이동 모드 BP(`BP_MovementMode_*`)와 BPI 인터페이스(`BPI_SandboxCharacter_*`)는 가져오지 않는다. 이동은 기존 C++ Mover 모드가 소유하고, 애니 변수는 `ULNPAnimInstance`(C++)가 접평면 투영 값으로 공급한다. | 확정 | [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §7.4 |
| AN-003 | 상태별 애니메이션 선택 로직은 ABP 그래프가 아니라 **Chooser 테이블과 C++**에 둔다. PSD·Chooser는 UAF(`UAFChooser`·`UAFPoseSearch`)에서도 재사용되므로 이후 UAF 이전 비용을 그래프 배선 수준으로 줄인다. | 확정 | `research/UE58_AnimationFeatures.md` §1 |
| AN-004 | 히트리액트는 지금처럼 **코스메틱**이다(GameplayCue 수신 시 머신별 로컬 재생, 비복제, 데디 서버 생략). 물리 히트리액트는 `VisualMesh`의 상체 바디만 구동하고 캡슐·Mover·판정은 건드리지 않는다. 행동을 끊는 경직(GA_Stagger)은 몽타주 경로를 유지한다. | 확정 | [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §6.4·§6.5 |
| AN-005 | GASP식 파워드 래그돌 넘어짐·기상(캡슐이 물리를 따라가는 연출)은 이 작업 범위 밖이다. 서버 권위 위치·리컨실리에이션 문제를 낳으므로 필요해지면 별도 결정으로 다룬다. | 확정 | `Roadmap.md` §5 |
| AN-006 | UAF는 **판정 게이트**다. Phase 5 스파이크를 통과하기 전에는 마이그레이션에 착수하지 않는다. 불합격이면 사유를 [DiscardedApproaches.md](../DiscardedApproaches.md)에 남기고 다음 엔진 버전에서 다시 본다. | 확정 | `Roadmap.md` Phase 5 |
| AN-007 | 공중 `Fall_Loop` 고정([TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §5.5)은 유지한다. 수직 채널을 가진 공중 스키마를 따로 만들기 전에는 GASP 점프·낙하 처리로 바꾸지 않는다. | 확정 | 같은 문서 §5.5 |
| AN-008 | Locomotor 사족 드론은 **최후순위 별도 트랙**이다. 이동 외 전투 모션이 GASP에 없어 전투 참여 방안이 서기 전에는 이식하지 않는다. 검증·이식 모두 본 트랙과 독립적으로 진행한다. | 확정(2026-10-16, 사용자 결정) | `Roadmap.md` 트랙 L |
| AN-009 | GASP 에디터는 MCP `unreal-mcp-gasp`(포트 8001)로 LootNPop 에디터와 동시에 띄워 **읽기 전용**으로 참조한다. GASP 쪽에서는 저장·수정 툴을 쓰지 않는다. | 확정(2026-10-16, 사용자 결정) | `README.md` §4 |
| AN-010 | 마커리스 모캡에 쓰는 다른 게임 영상은 프로토타입·레퍼런스로 한정하고, 최종 출시 에셋은 직접 촬영한 영상으로 같은 파이프라인을 돌린다(타사 모션 작업물 권리 문제). | 확정(2026-10-18, 사용자 결정) | `research/UE58_AnimationFeatures.md` §4 |
| AN-011 | 로코모션 기준 스켈레톤은 **GASP UEFN 스켈레톤**이다. `AnimSourceMesh`를 `SKM_UEFN_Mannequin` + UEFN 스켈레톤 메인 ABP로 바꾸고, 로코모션은 GASP 5.8 클립을 그대로 쓴다. 공격·리로드·무기별 레이어·몽타주처럼 Lyra 애니가 필요한 곳은 **Lyra 시퀀스를 UEFN으로 오프라인 리타깃해** 계속 쓴다(현재 실사용 91개 + 이후 필요분, `PSD_*_Lyra` 로코모션 30개는 GASP로 대체). `VisualMesh`는 캐릭터별 메시를 유지하고 UEFN → 그 메시 리타기터로 포즈를 받는다(플레이어 MoverSample 메시는 UEFN→MS, UEFN 메시 적은 같은 스켈레톤이라 포즈 복사). 전환은 1C 뒤 Phase 2 단위 2-0a·2-0b가 한다. | 확정(2026-10-18, 사용자 결정) | `design/GASPReference.md` §2.7 |
| AN-012 | 옛 GASP 버전에서 가져온 소스 에셋(`Content/Characters/GASP_Mannequin/`, MotionMatchingData 제외)은 **중복이어도 GASP 5.8 최신본으로 교체**한다. 최신 GASP에만 있는 에셋은 일괄 반입하지 않고 필요할 때 가져온다(AN-001). | 확정(2026-10-18, 사용자 결정) | `phases/Phase01C_GASPAssetRefresh.md` |
| AN-013 | 최신화는 LootNPop 폴더를 GASP 원본 패키지 경로(`/Game/Characters/UEFN_Mannequin/`)로 옮긴 뒤 같은 경로의 파일을 덮어쓰는 방식으로 한다. 이후 GASP 업데이트도 같은 덮어쓰기로 처리한다. | 확정(2026-10-18, 0-3에서 AN-016 원본 경로 규칙과 함께 사용자 선택) | 같은 문서 "핵심 방식" |
| AN-014 | GASP 폴더 안에 섞인 LootNPop 산출물(리타깃 적 애니 `Animations/Enemy/*` 등)은 GASP 경로 밖으로 옮기되 **git에는 추적하지 않는다** — 라이선스 원본의 리타깃 산출물이라 AN-016 `Content/Characters/Retargeted/` 규칙을 따른다. 출처 미판별 4개는 1C-1에서 판별한다. | 확정(2026-10-18, AN-016 선택으로 해소) | `phases/Phase01C_GASPAssetRefresh.md` 단위 1C-1 |
| AN-015 | 로코모션 선택 코어는 GASP의 **Motion Matching 노드 방식**(Chooser가 상태별 PSD 묶음을 고르고 MM 노드가 검색)으로 재구성한다. GASP Mover 기본값인 State Machine + Chooser + Blend Stack 방식은 만들지 않는다(검증 전 Experimental, 유지보수 부담, ABP 분기 누적 — AN-003). | 확정(2026-10-18, 사용자 결정) | `design/GASPReference.md` §2.5 |
| AN-016 | **소스 에셋 폴더·git 규칙(AN-001 세부).** ① GASP 소스 에셋은 GASP 패키지 경로를 그대로 쓴다(`Content/Characters/UEFN_Mannequin/`, 시퀀스가 참조하는 `Content/Misc/SandboxAnimCurveCompressionSettings`·`Content/Blueprints/AnimModifiers/`). `.gitignore`는 이 경로 단위로 제외한다(1C-2에서 `GASP_Mannequin/*` 패턴을 교체). ② LootNPop 구성 에셋(PSD·PSS·Chooser·ABP 등)은 GASP 경로 폴더 안에 두지 않는다 — `GASP_Mannequin/MotionMatchingData/`는 1C-2에서 LootNPop 소유 폴더로 옮긴다. GASP 경로 안의 GASP 구성 에셋(PSD·Chooser·ABP)은 반입하지 않는다. ③ 라이선스 원본(Lyra·GASP 등)을 리타깃한 산출물은 원본 라이선스를 따라 git에서 제외하고, LootNPop 소유 제외 폴더 `Content/Characters/Retargeted/`에 둔다. 재생성 절차(리타기터·대상 목록)는 만든 단위가 문서로 남긴다. | 확정(2026-10-18, 사용자 결정) | `README.md` §4, `phases/Phase01C_GASPAssetRefresh.md` |
| AN-017 | **시퀀스가 끌고 오는 노티파이·모디파이어.** 엔진 의존만 있는 독립 에셋(커브 압축 설정 `SandboxAnimCurveCompressionSettings`, `AM_*` 애니 모디파이어)은 GASP 원본 경로 그대로 반입하고 git에서 제외한다. GASP 게임 로직·오디오 체인을 끌고 오는 노티파이(`BP_AnimNotify_FoleyEvent*` → `/Game/Audio/Foley/*`, `BP_NotifyState_EarlyTransition` → `BPI_SandboxCharacter_ABP`·CMC ABP)와 시퀀스가 하드 참조하는 GASP PSD는 반입하지 않는다. 대신 `Config/DefaultEngine.ini` `[CoreRedirects]`에 패키지마다 **`PackageRedirects=(OldName="<패키지>",Removed=true)`만** 넣어 로드 경고를 없앤다(C++ 없음, 1C-1 실험 F — 노티파이는 클래스 없는 빈 이벤트로 남는다. ini 적용은 1C-3). 소스 파일은 수정하지 않아 다시 덮어쓸 수 있게 유지한다. 발소리가 필요해지면 그때 no-op(또는 재구현) 노티파이 C++ 클래스 + `ClassRedirects ... OverrideClassName`(실험 E)으로 바꾼다. | 확정(2026-10-18, 사용자 결정. 같은 날 1C-2 착수 전 "C++ 자리표시자" → "Removed만"으로 수정) | `phases/Phase01C_GASPAssetRefresh.md` 단위 1C-1·1C-3 |

## 변경 규칙

- 확정 결정을 바꿀 때 기존 행을 조용히 덮어쓰지 않는다. 상태를 `대체됨`으로 바꾸고 새 ID를 추가한다.
- 변경 이유와 영향을 관련 설계 문서와 현재 Phase 로그에 기록한다.
- 단순 구현 선택이나 임시 디버그 값은 이 원장에 넣지 않는다.
