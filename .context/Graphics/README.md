# 그래픽스 작업 안내

> 상태: 활성
> 역할: 이 작업의 진입점과 문서 라우팅
> 마지막 구조 갱신: 2026-11-27(IsleSkin V007·모노리스 라우팅)

## 1. 목적

이 폴더는 LootNPop에서 그래픽스 실무 경험(GPU 최적화, 커스텀 셰이더, 커스텀 렌더 패스)을 쌓는 작업을 여러 세션에 걸쳐 관리한다. 결과물은 포트폴리오로도 쓴다. 그래서 "이 게임 구조(구면 월드·수천 개의 Mass 엔티티) 때문에 엔진 기본값이 깨졌고, 렌더러를 분석해 이렇게 고쳤고, 수치로 이만큼 나아졌다"는 흐름을 남기는 것을 중시한다.

| 트랙 | 내용 | 원본 |
|:---|:---|:---|
| 0 측정 | 패키지 빌드 측정 하네스와 마네킹 상태 기준선 | `Roadmap.md` Phase 0 |
| C 콘텐츠 | 옥탄트 1개와 적 2~3종을 에픽 무료 에셋으로 고품질 벤치마크 씬으로 채운다 | Phase 1 |
| P 군중 렌더링 | 대규모 적 전투의 GT/RT/GPU 비용 분석, 슬롯·LOD·그림자·머티리얼 절제, ISKM↔ISM↔Actor 전환 팝핑 | Phase 2 |
| R 렌더 패스 | RDG 렌더 모듈 기반, 화면 효과 레이어, 캐릭터 강조 채널, 스캔 시야 | Phase 3 |
| S 지형 흔적 | 화염방사기·런처·라이플이 구면 지형에 남기는 흔적 | Phase 4 |
| O 렌더링 최적화 | GPU 패스 분해, 해상도·업스케일 스윕, 적 Lumen 기여 절제, 패스별 GPU 예산. 기존 Phase 게이트에 붙여 병행한다(GX-010) | `Roadmap.md` §4 트랙 O |

JustDodge 작업이 넘긴 그래픽스 항목(J-012)도 이 폴더가 소유한다(`Roadmap.md` §5).

범위 밖은 다음과 같다.

- 엔진 소스 수정과 셰이딩 모델 추가(GX-008)
- 게임 전체 아트 교체. 콘텐츠는 벤치마크 씬 범위만 채운다(GX-004)
- 사운드

## 2. 세션 시작 시 필수 문서

1. `README.md`
2. `Current.md`
3. `Decisions.md`

이 세 문서를 읽은 뒤에는 아래 라우팅 표에서 지금 작업에 필요한 문서만 더 읽는다. `history/` 폴더와 현재 단계에서 먼 `phases/` 문서는 기본으로 읽지 않는다.

## 3. 문서 라우팅

| 작업 | 추가로 읽을 문서 |
|:---|:---|
| 단계 조정·Phase 전환·JustDodge 이관 항목 | `Roadmap.md` |
| 현재 Phase 구현 | `Current.md`가 가리키는 `phases/PhaseNN_*.md` |
| Phase 1의 1-A 마감·출발 기준선과 다음 1-B 준비 | [Phase 1 실행서](phases/Phase01_BenchmarkContent.md) §13, [출발 기준선 보고서](analysis/Phase01_StartBaselineReport.md), [기존 1-B0 선정](phases/Phase01_RidgeZonePalette.md) §8 |
| Phase 1 Ridge 대표 구역·Electric Dreams 팔레트·장식 금지 규칙 | [1-B0 구역·팔레트](phases/Phase01_RidgeZonePalette.md) |
| Phase 1-B 반입·재생성·좌표/재질·소수 배치/바람·샘플 비용/반복 원인/회전·변형 반입·WIP 베이크/런타임·접촉 검증·대표 구역 PCG·바위 Blocker 승격·밀도·동선 등급별 완화와 새 세션 인계 | [Current](Current.md), [환경 반입 실행서](phases/Phase01_EnvironmentImport.md) §7~33, [현재 팔레트](phases/Phase01_RidgeZonePalette.md) §10 |
| Ridge 지표 전체 Electric Dreams 교체·현재 큰 배치 보존·전체 큰 배치 초벌→닫힌 몸체와 ED 외피→파생 충돌/PIE→세부 마감(GX-034) | [Phase 1-BR](phases/Phase01_RidgeReferencePatch.md) §2.1·U5a/U5b |
| U4 V011 큰 배치 채택·외곽 장식 종료와 현재 몸체 시험 인계 | [현재 인수인계](analysis/Phase01_U4LayoutHandoff.md) · 내부 몸체 시험 입력/근거 포함 |
| U5a-1 첫 몸체 생성·V002 검토본·경사로 REVIEW·UV 경고 보정과 다음 연결 검토 | [몸체 시험 결과](analysis/Phase01_U5aBodyTrial.md) |
| U5a-2 V003 경계 보정·실제 보존/16비교 완료(이전 시험본) | [경계 보정 결과](analysis/Phase01_U5aBodyRefinement.md) |
| U5a-3 V004 동선 유지 사용자 승인·실제 보존/22비교 완료 | [연결 초안 결과](analysis/Phase01_U5aConnectionTrial.md) |
| U5a-4 V005 석재 외형·26비교·실제 보존/858정점 PASS·유지 사용자 승인 | [석재 외형 결과](analysis/Phase01_U5aStoneRefinement.md) |
| U5a-5 V006 몸체 캡/경계/재질·26비교·실제 보존/73,111정점 PASS·사용자 유지 | [몸체 보정 결과](analysis/Phase01_U5aBodyCaps.md) |
| V008 채택·다음701파일/27자세·새 세션 첫 범위/시작 프롬프트 | [현재 인수인계](analysis/Phase01_U5aBodyHandoff.md) · [채택 저작 기준](../../Scripts/Profiling/Graphics/ClosedBodyTrial_V001/Handoff_V008.md) |
| 부유섬 2개·하늘 석교 외피 시험(IsleSkin V001~V006)·산괴 동쪽 탑 모노리스 외피 V007(GX-040)·GX-038 부유 표현 범위·보라색 원인(구역 판정) | [부유섬·석교 외피 시험](analysis/Phase01_IsleSkinTrial.md) |
| U5a-6 V007 사용자 채택·실제 교차0/73,111정점/26비교 | [자기 교차 검토](analysis/Phase01_U5aSelfIntersectionReview.md) |
| U5a-7 V008 사용자 채택·73,273정점/27비교·다음701보존·GX-037 부유석 활용 | [경계 검토](analysis/Phase01_U5aBoundaryReview.md) |
| V010 PIE 제보 수집 종료·풀/밀도 복구·GX-034 전체 큰 배치 시작 당시의 이전 인계 | [이전 전체 큰 배치 인수인계](analysis/Phase01_LargeLayoutHandoff.md) |
| U4-0 대조·V037 보존 목록/복구·비교8시점·Z1~Z7 순서·U4-1~3 배치와 최종 검사 경과 | [U4 큰 배치 계획](analysis/Phase01_U4LargeLayoutPlan.md) |
| 네 개선 완료 뒤 통합 검증·재측정·Phase0 마무리 | [개선 후 재개 절차](phases/Phase00_PostOptimization.md) |
| 측정 지표·절차·노트북 측정 함정·보고 형식 | `design/Measurement.md` |
| Phase0 공통 기준선 수치·개선 전후·남은 GPU 비용 요약 | [Phase0 기준선 보고서](analysis/Phase00_BaselineReport.md) |
| 배회 프로세서 최적화·다른 세션 인계 | [배회 최적화 인계](analysis/WanderOptimizationHandoff.md) |
| 그래플 앵커 Niagara·월드 후처리 작업 대기 | [그래플 앵커 최적화 인계](analysis/GrappleAnchorOptimizationHandoff.md) |
| GT 렌더 동기화·RT/GPU 비용 분석 | [렌더 동기화 최적화 인계](analysis/RenderSyncOptimizationHandoff.md) |
| 렌더링 최적화(트랙 O)·Lumen 동적 메시 비용·해상도 비례 비용 | [선행 렌더링 부하 측정](analysis/RenderingLoadBaseline_20260925.md) |
| 해상도별 GPU 비용(고정/화소 비례 분해)·내부 해상도 운영 조건(GX-012) | [단위 O-2 로그](history/UnitO2_Log.md) |
| Nav 최근접 노드 반복 조회 | [Nav 최근접 노드 최적화 인계](analysis/NavNearestNodeOptimizationHandoff.md) |
| 절제(ablation) 측정과 분모 규약 | [Guide_NetBandwidth.md](../Guide_NetBandwidth.md) |
| ISKM·ISM 표현, 유의도 구간, 에셋 요건(Optimize for Instancing·usage 플래그·제자리 클립) | [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) §6 |
| 적 Actor 승격·강등 | [TechDesign_EnemyNPC.md](../TechDesign_EnemyNPC.md) |
| Niagara 원거리 함정(`bLocalSpace`, 구면 중력) | [TechDesign_ParrySystem.md](../TechDesign_ParrySystem.md) §8 |
| 화염방사기 분사·화상 VFX, 무기 구성 | [TechDesign_Ability.md](../TechDesign_Ability.md) §5.6 |
| 착탄·폭발·화염 판정 경로(흔적 트리거) | [TechDesign_HitDetection.md](../TechDesign_HitDetection.md) |
| LootPod Low LOD 빛기둥 | [TechDesign_LootPod.md](../TechDesign_LootPod.md) |
| 적 탐색 공간 분할(스캔 대상 수집) | [TechDesign_TargetQuery.md](../TechDesign_TargetQuery.md) |
| 옥탄트 구성·지형 형상 소유권 | [TechDesign_WorldGeneration.md](../TechDesign_WorldGeneration.md), [Guide_OctantLevelInstance.md](../Guide_OctantLevelInstance.md) |
| JustDodge 연출 의도(화면 연출·림라이트·색 규약) | [JustDodge/design/VisualDesign.md](../JustDodge/design/VisualDesign.md) |
| 리타깃·스켈레톤(파라곤 적 반입) | [AnimationOverhaul/README.md](../AnimationOverhaul/README.md) |
| 과거 경위·회귀 원인 조사 | 해당 `history/PhaseNN_Log.md` |

## 4. 모든 구현이 지켜야 하는 불변 조건

- **성능 판정은 패키지 빌드에서만 한다.** 에디터 `-game`은 `WITH_EDITOR` 코드와 디버그 드로우로 오염된다(GX-001).
- **기본 `DA_MassSpawnConfig`를 보존한다.** 측정 편성은 별도 에셋을 만들고 오버라이드로 고른다. Phase 0 하네스는 `LNP.Graphics.Benchmark.Profile`로 선택하며 일반 PIE의 편성 선택에는 `LNP.Spawn.ConfigOverride`를 쓴다.
- **구면 월드다. 월드 Z가 Up이라는 가정을 쓰지 않는다.** 높이 안개·RVT·데칼 투영처럼 엔진 기능이 Z를 가정하는지 먼저 확인하고, 적도 근처에서 검증한다.
- **렌더 확장은 엔진 수정 없이 한다.** 프로젝트·플러그인 모듈과 SceneViewExtension/RDG 범위 안에서 해결한다(GX-008).
- **연출은 비복제 코스메틱이다.** 스캔 시야·지형 흔적·화면 효과는 기존 경로(GameplayCue, 착탄 이벤트)에서 트리거만 받는다. 게임플레이 판정에 쓰지 않는다(GX-007).
- **Niagara 월드 스페이스 이미터를 주의한다.** 월드 원점에서 약 24,000cm 떨어진 구 내벽에서 한 픽셀도 그려지지 않은 전례가 있다(ParrySystem §8).
- **에픽 무료 에셋은 git에 올리지 않는다.** 재생성 절차만 문서로 남긴다(GX-004).

## 5. 문서별 소유 정보

- `README.md`: 작업 범위, 불변 조건, 문서 라우팅
- `Current.md`: 지금의 상태와 바로 다음 행동
- `Decisions.md`: 모든 세션이 알아야 하는 확정 결정(`GX-` 접두어)
- `Roadmap.md`: 트랙·Phase 순서, Phase 게이트, JustDodge 이관 항목의 원본
- `design/`: 현재 유효한 상세 설계와 측정 규약
- `analysis/`: 측정으로 확인한 사실·미확정 사항과 별도 최적화 세션의 인계 자료
- `phases/`: 해당 단계의 실행 계획과 완료 조건(착수 직전에 만든다)
- `history/`: 끝난 작업, 측정 기록, 시행착오(첫 작업 세션에서 만든다)

정보의 원본은 한 곳에만 둔다. 다른 문서에는 요약과 링크만 둔다. 구현이 끝나 프로젝트 공통 설계가 생기면(예: `TechDesign_Rendering.md`, `EngineAnalysis_RDG.md`) 공통 문서로 옮기고, 이 폴더에는 링크만 남긴다.

## 6. 포트폴리오 산출물 규약

Phase마다 다음 세 가지를 남긴다.

1. **영상** 30초~1분: 전후 비교 또는 기능 시연
2. **수치 표**: `design/Measurement.md`의 보고 형식을 따른다. 목표 예산(GX-002) 대비로 쓴다
3. **문서 요약**: 문제 → 엔진 분석 → 해결 → 측정 순서로 쓴다

## 7. 문서 갱신 규칙

### 세션 시작

1. 필수 문서 세 개를 읽는다.
2. `Current.md`에서 현재 Phase와 다음 행동을 확인한다.
3. 라우팅 표에서 필요한 설계·Phase 문서만 읽는다.
4. 코드와 에셋의 실제 상태가 문서와 맞는지 확인한다.

### 작업 중

- 새로 생긴 영구 규약은 해당 `design/` 문서에 반영한다.
- 작업 전체가 지켜야 하는 결정이면 `Decisions.md`에도 한 줄을 추가한다.
- 측정 수치와 근거는 현재 Phase 로그에 기록한다.

### 세션 종료

1. `Current.md`를 실제 상태로 갱신한다(이력은 누적하지 않는다).
2. 수행 내용과 검증 결과를 `history/PhaseNN_Log.md`에 날짜별로 추가한다(없으면 만든다).
3. 완료 조건을 충족했으면 Phase 문서의 체크리스트를 갱신한다.
4. 설계가 바뀌었으면 관련 `design/`과 `Decisions.md`를 먼저 고친다.
5. 다음 세션이 바로 실행할 구체적인 작업 한두 개를 남긴다.

### Phase 종료

1. 완료 조건, 측정 증거, 포트폴리오 산출물(§6)을 로그에 남긴다.
2. `Roadmap.md`의 상태를 갱신한다.
3. `Current.md`를 다음 Phase 기준으로 짧게 다시 쓴다.
4. 다음 Phase 실행 문서는 착수 직전에 만들거나 구체화한다.
5. 공통 설계 문서가 새로 생기면 문서 인덱스 3곳(CLAUDE.md·ProjectOverview·Notion)을 함께 고친다.

## 8. 문서 상태 표기

| 상태 | 의미 |
|:---|:---|
| 활성 | 매 세션의 현재 진입점 |
| 확정 | 바꾸기 전에 명시적인 재논의가 필요한 결정 |
| 기준 설계 | 구현이 따라야 하는 현재 설계 |
| 초안 | 구현·측정 결과에 따라 바뀔 수 있음 |
| 검증 필요 | 엔진 또는 프로젝트 실험이 남아 있음 |
| 완료 | 해당 Phase의 완료 조건과 검증을 통과 |
| 보류 | 착수 조건을 기다리는 중 |
| 폐기 | 사용하지 않으며 기각 이유만 보존 |

## 9. 컨텍스트 예산 원칙

- 필수 세 문서는 합계 약 4,000~6,000토큰 이하를 목표로 한다.
- `Current.md`는 완료 이력을 쌓지 않고 짧게 유지한다.
- `Decisions.md`는 결론 중심의 표로 유지하고 상세 근거는 링크한다.
- 먼 Phase는 `Roadmap.md`에 요약만 두고 착수 전에 상세화한다.
- 측정 원자료(CSV·트레이스)는 문서에 붙이지 않는다. `Saved/` 경로와 요약 수치만 남긴다.

## 10. 현재 진입점

현재 작업 위치와 다음 행동은 `Current.md`를 따른다. 전체 순서를 다시 조정해야 할 때만 `Roadmap.md`를 읽는다.
