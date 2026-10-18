# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0 단위 0-3(스켈레톤과 소스 에셋 규칙, 사용자 결정 포함) — 착수 전
> 마지막 갱신: 2026-10-18

## 현재 결정

코드·LootNPop 에셋 변경은 아직 없다. 사용자 결정:

- GASP는 구성을 보며 직접 다시 만들고, 소스 에셋만 마이그레이트·git 제외(AN-001)
- Locomotor 사족 드론은 최후순위 별도 트랙(AN-008)
- GASP 에디터는 MCP 포트 8001로 동시에 띄워 읽기 전용 참조(AN-009)
- 옛 GASP 유래 소스 에셋은 중복이어도 5.8 최신본으로 교체 — Phase 1C(AN-012)
- 로코모션 선택 코어는 **Motion Matching 노드 방식**, SM + Chooser + Blend Stack 방식은 만들지 않음(AN-015)

결정 대기: 기준 스켈레톤(AN-011), 마커리스 소스 영상 정책(AN-010) — 0-3에서 묻는다. 최신화 방식(AN-013 제안)과 LootNPop 산출물 추적(AN-014)은 Phase 1C 단위 1C-1에서 묻는다.

⚠️ `GASP_Mannequin/Animations/Enemy/*`(LootNPop이 만든 리타깃 적 애니 14개)는 `.gitignore`에 걸려 저장소에 없다. Phase 1C 전에 로컬 파일을 지우거나 덮어쓰지 않는다(`research/UE58_AnimationFeatures.md` §6).

⚠️ GASP 에디터에서 `SandboxCharacter_Mover_ABP`·`SandboxCharacter_CMC_ABP`가 dirty다. 원인은 `read_graph_dsl`이다(`design/GASPReference.md` §1.2). GASP 에디터를 닫을 때 저장하지 않는다.

## 0-2 결과 요약 (상세 `design/GASPReference.md` §2)

- 블록 B0~B12. Phase 2 = 2-1 변수 공급층(C++) → 2-2 MM 선택 코어(PSD Chooser) → 2-3 Offset Root·Steering·Orientation Warping → 2-4 전환 세트 → 2-5 발 배치·경사 → 2-6 Leans·적 검증(`Roadmap.md` §4).
- 0-3 근거: 필요한 소스 에셋은 전부 `SK_UEFN_Mannequin` 스켈레톤이다. MM 경로 시퀀스 461개(앉기·슬라이드·트래버설 빼면 약 319개)·PSD 71·PSS 12, 경사 BS 2·Lean BS 3, CR `CR_Biped_FootPlacement`. 시퀀스 곡선 6종·블렌드 프로파일·미러 테이블이 UEFN 스켈레톤에 붙어 있다(§2.7).

## 바로 다음 행동

- **새 세션**에서 [Phase 0](phases/Phase00_ReferenceSetup.md) 단위 0-3을 시작한다. `design/GASPReference.md` §2.7을 근거로 AN-011 선택지(Lyra 유지 vs UEFN 전환)를 양쪽 리타깃 대상 개수와 함께 정리해 사용자에게 묻는다. Lyra 쪽은 무기 서브 ABP·몽타주가 실제로 쓰는 `Lyra_Mannequin/Animations` 개수를 세고, 두 스켈레톤에 `ik_foot_root`·`ik_foot_l/r`·`spine_01~05`가 있는지 확인한다.
- 같은 단위에서 마이그레이트 규칙(노티파이·모디파이어 BP 처리, 소스 에셋 확보 절차)과 AN-010을 정한다.

## 인계 근거

- MCP 읽기 방법·함정: `design/GASPReference.md` §1
- 0-1·0-2 수행 기록: `history/Phase00_Log.md`
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
- 사용자 T3D 자료(git 제외): `Saved/GASPRef/`
