# Phase 1 작업 로그

## 2026-11-11 — 1-A 실행서·적 0 대조군 준비

### 확인한 상태

- 사용자 작업 진행 요청으로 Phase 1에 착수했다. 착수 시 HEAD는 `f0d0a8c`이며 기존 미커밋 `Current.md`·`Roadmap.md`의 출발 기준선 결정을 보존했다.
- 라이브 MCP 조회: PIE 미실행, 실제 `OctantPoolData`는 Bastion·Hollow·Acropolis·Pyramid·Grove·Ridge 순의 6종, slot mask 모두 255. 에셋은 변경하지 않았다.
- 패키지의 명시적 cook 목록은 Meadow 포함·Grove/Ridge 누락 상태였다. 해당 목록만 현행 풀에 맞췄고 편성 수치는 유지했다.
- 기존 카메라 JSON·읽기 검증·Prepare·UpdateView가 Acropolis·slot 6에 고정돼 있다. 선택 코드와 엔진 `FRandomStream`을 재현한 시드 1 배정 예측은 `[Acropolis, Ridge, Bastion, Pyramid, Grove, Hollow, Pyramid, Ridge]`다. **실제 PIE/패키지 배정 검증은 미실행**이며, 옛 경로는 slot 조건에서 실패할 것으로 예상된다. 대상 옥탄트가 보이는 경로를 확정하기 전에는 정식 측정을 시작하지 않는다.

### 변경

- [Phase 1 실행서](../phases/Phase01_BenchmarkContent.md)를 생성했다. 대상·원본·라이선스·카메라 선결 조건과 1-A~1-D 완료 기준을 정리했다.
- `LNPGraphicsBenchmark::AreEnemiesDisabled()`와 `-LNPGraphicsNoEnemies`를 추가했다. 일반 실행에서는 적용되지 않는다. 공간 계획을 유지한 채 생성 큐의 적 요청만 제외해 Pod 배치 조건을 유지한다.
- manifest에 `no_enemies`를 기록하고 해당 회차는 `performance_eligible=false`로 제외했다. 초기 적 0 허용은 진단 인자에만 한정하며 실제 생존 수와 일치해야 한다.
- 집계는 적 0 진단에서만 빈 개체 표를 허용한다. 진단의 정식 기준선 표기·초기/마지막/프레임 생존 적 혼입을 거부하며 요약에도 `NoEnemies`를 남긴다. [측정 규약](../design/Measurement.md)에 의미를 기록했다.

### 검증 근거와 한계

검증 스크립트·합성 입력·출력: `Saved/Profiling/Graphics/Phase01/Preparation_20261111/VerifyAnalyzer.ps1`, `Checks.json`, `NoEnemies/`, `Regular/`.

| 검사 | 결과 |
|:---|:---|
| 적 0 합성 입력, 빈 개체 표·시간 평균 보존 | 통과 |
| `-AllowPreview` 없는 진단 집계 거부 | 통과 |
| 일반 실행의 빈 개체 표 거부 | 통과 |
| 적 0 진단의 정식 기준선 표기 거부 | 통과 |
| 적 0 진단의 초기 적 혼입 거부 | 통과 |
| 적 0 진단의 프레임 생존 적 혼입 거부 | 통과 |
| 기존 정식 Medium 자료의 다섯 평균·초기 수·프레임 수 동일 | 통과 |
| 모든 명시적 cook root 파일 존재 | 통과 |

기존 Medium 입력은 `Saved/Profiling/Graphics/Phase00/PostOptMediumCooled_20261109_152837/Medium_20261109_062938_6401b5/Medium_DB63813C456B99106D7D38A070D2B565/`다. 원본 결과를 덮어쓰지 않고 manifest/개체 표를 작업 폴더로 복사해 같은 프레임 원자료를 재집계했다. 이 검사는 집계 회귀 근거이며 새 C++·패키지 런타임의 검증을 대신하지 않는다.

C++ 빌드·쿡·패키지 실행·카메라 육안 확인·성능 반복은 미실행이다. 빌드/Live Coding 직전 병행 여부 확인을 사용자에게 요청했다(`Current.md` §2). 대상 옥탄트와 Fab 확보 상태도 확인 대기다. 원본 라이선스 조사·에셋 반입·PCG·리타깃은 아직 시작하지 않았다.

### 다음 행동

1. 대상 옥탄트와 시드 1 실제 배정을 확인하고 사용자가 직접 대상 A/B/C 구도를 잡는다. 해당 경로를 받아 하네스의 고정 대상 검증을 필요한 범위에서 보완한다.
2. 병행 작업이 없다는 답변 뒤 C++ 빌드 검증, 경로 확정 뒤 새 패키지·짧은 검증·Medium 3회 + 적 0 1회로 이어 간다.

## 2026-11-11 — Live Coding 성공·환경 원본 재검토

- 사용자가 병행 작업 없음과 빌드 검증 진행을 승인했다. 실행 중인 에디터의 `LiveCodingToolset.CompileLiveCoding`을 호출했고 `Result: Success`, `Live coding succeeded`를 받았다. 출력에 `UnrealEditor-LootNPop.patch_0.lib/.exp` 링크가 확인됐다. 정규 Editor 전체 빌드·Game 패키지·실제 적 0 실행 근거는 아직 없다. 기존의 빌드 승인 대기는 해소됐다.
- 사용자는 Fab의 Epic Games 콘텐츠를 먼저 조사하고, 특정 팩의 지형/공간을 참고한 새 옥탄트를 만들 수도 있다고 제안했다. 대상 Grove/Ridge는 확정되지 않았다. 기존 Phase 1 준비는 유지하며 에셋/지형 변경과 정식 측정은 시작하지 않았다.
- Fab 개별 상품과 공식 샘플 문서에서 무료·퍼블리셔·구성·지형 제작 방식을 확인했다. [후보 비교](../analysis/Phase01_ContentCandidates.md)에 링크와 확인 사실/프로젝트 적용 제안을 분리해 기록했다. Electric Dreams의 CloseRange/LargeAssembly를 먼저 실물 검토하는 안을 추천하며, 판타지 유적은 Agora+Acropolis 또는 Quixel의 Dark Ruins가 비교 대상이다.
- 새 옥탄트를 채택하면 형상·Support·시드/경로 확정 뒤 장식 전 기준선을 잡는 안과 포함 범위를 먼저 합의한다. 아직 GX-004/로드맵의 확정 범위를 바꾸지 않았다.

## 2026-11-12 — Electric Dreams 8001 실물 검토

- 사용자 요청으로 `unreal_mcp_second` 연결의 Electric Dreams를 읽기 전용으로 검토했다. 프로젝트는 `D:/UnrealProjects/ElectricDreamsEnv`, UE 5.8.4다. 처음 Startup·PIE 미실행·미저장 변경 없음 확인 뒤 작은 `ElectricDreams_PCGCloseRange`를 열었다. 검토 종료 시 이 레벨의 dirty=false 확인. 원본 에셋 편집/저장·타깃 반입·설정 변경은 하지 않았다.
- 결과는 [Electric Dreams 검토 보고서](../analysis/ElectricDreamsReview_20261112.md)에 기록했다. 협곡/숲 제작 재료는 충분하고 조사한 의존성에 샘플 커스텀 게임플레이 모듈은 없었다. 원본 PCG의 +Z 경사 기준/-Z 광선, 약 70m 조합의 구면 접합, 원본별 충돌 정책 차이는 실제 적용 시 보완해야 한다.
- 대표 메시 17개 태그, 메시 4개 재질/BodySetup, 부모 재질 3종·함수, PCG 3종(총 892노드), CloseRange 대표 조합의 컴포넌트·재질 오버라이드를 확인했다. 절벽/지면/나무 3개 썸네일을 확인했다. 전체 레벨 플레이나 카메라 전 구간 검증은 하지 않았다.
- 대표 5메시의 전이 의존성은 122패키지·원본 1,628.21MiB, 텍스처 66개 중 VT 65개. 원자료는 `Saved/Profiling/Graphics/Phase01/ElectricDreamsReview_20261112/`에 보관했다. 원본 디스크 크기를 VRAM/패키지 크기로 해석하지 않는다.
- 소스와 LootNPop의 Substrate·DX12/SM6 설정을 비교했다. 소스 VT 실효값 1, 설치 엔진 VT 기본값도 1이다. 타깃 설정에 명시 항목이 없다는 이유로 꺼져 있다고 결론내리지 않았다. 타깃 반입 셰이더·VT·회전·실제 충돌·Support·패키지 성능은 미검증이다.
- 다음 제안은 절벽 2~3종·바닥 1~2종·나무 1종·식생 2~3종으로 대표 구역을 정한 뒤 작은 반입 시험이다. 새 옥탄트 이름/공간/일정·풀·원본 라이선스 적용 확인·git 제외 범위는 아직 확정/변경하지 않았다.

## 2026-11-13 — Fab 재조사·대상 확정(Ridge + Electric Dreams)

- 사용자 요청으로 Fab에서 Epic Games·Quixel Megascans의 무료 환경 팩을 다시 조사했다. 내장 브라우저로 상품 페이지와 상품 정보 응답을 읽어 라이선스 등급·가격·지원 엔진 버전을 확인했다. 조회한 후보는 모두 Standard License, Personal·Professional 0원이었다. Goddess Temple·Medieval Village Megascans Sample을 새 후보로 추가했고, Valley는 5.7까지, Open World Demo Collection은 5.2까지만 지원한다. 결과는 [후보 비교](../analysis/Phase01_ContentCandidates.md) §5에 기록했다.
- 새 옥탄트를 만드는 안과 기존 옥탄트를 꾸미는 안을 비교해, 기존 Ridge를 Electric Dreams로 꾸미는 안을 제안했다. 사용자가 이를 확정해 GX-013으로 기록했다. 실행서·Roadmap·Current에 대상과 "1차 장식은 보행 충돌 불변" 조건을 반영했다.
- 에셋 반입·에디터 조작·빌드·측정은 하지 않았다. Goddess Temple·Dark Ruins는 상품 정보만 보고 실물은 확인하지 않았다. 시드 1에서 Ridge가 slot 1·7에 배정된다는 것은 예측일 뿐이며 실제 배정은 아직 확인하지 않았다.

### 다음 행동

1. Ridge 대표 구역(전투장 1곳과 연결 구간)과 Electric Dreams 팔레트를 고르고, 원본·파생 에셋의 git 제외 경로를 정한다.
2. 작은 반입 시험(재질·VT·바람·접지, 적도 부근 확인)과 Ridge가 보이는 카메라 A/B/C 확정 중 어느 것을 먼저 할지 사용자와 정한다.

### 추가 — 1-B0 계획 문서 작성

- 사용자 요청에 따라 실작업 없이 [1-B0 구역·팔레트 문서](../phases/Phase01_RidgeZonePalette.md)를 작성했다. 근거는 Ridge V011 README·평면도·렌더와 Electric Dreams 원자료다. 원자료에서 메시 계열별 수량과 CloseRange 사용 빈도를 집계했다.
- 내용: 구역 후보 A/B/C(권장 A 대분지 허브), Z1~Z4 구역 확정 절차, P0~P4 팔레트 절차(톤 결정, 역할별 후보 계열, 8001 시각 확인, 의존성, 지면 재질), 산출물, 체크리스트, 새 세션 프롬프트.
- 작업은 사용자가 새 세션에서 진행한다.

### 추가 — 1-B0 검토 반영·세션별 인계 보강

- 사용자 요청으로 계획을 보강했다. 지금은 구역·조건부 팔레트와 후속 검토 단서를 남기고, 돌출 허용치·밀도·투영 방식 등은 해당 작업 세션에서 분석하도록 구분했다. 선정 작업 자체는 아직 미착수다.
- 기존 지각 재질과 부분 메시 보강을 우선 검토하고, 큰 절벽 외장의 노출·접촉 정합은 작은 반입 시험에서 확인한 뒤 팔레트를 최종 확정하도록 했다. 재질 방침을 메시 선정 앞으로 옮기고 의존성 시작점에 오버라이드·지면 재질을 포함했다.
- spec 기준 대분지 중심/반경과 협곡 서쪽 입구·하류 출구를 구분했다. 구면 경사·면적 추정, 카메라 연결 구간, 보호 영역과 메시 전체 범위, Decoration 계약·SurfaceData 검증을 담당 단계의 단서로 남겼다.
- §8에 세션별 진행·사용자 확인·조건부 선택·미확인 사항·다음 행동을 기록하도록 하고, 체크리스트·시작 프롬프트·Current·상위 실행서 1-B 행을 맞췄다.
- 문서와 참조 정합성을 확인했다. 에셋 반입·에디터 조작·코드 변경·빌드·측정은 하지 않았다.

## 2026-11-13 — 1-B0 Z1·P0 선정, Z2 사용자 PIE 대기

### 사용자 확인과 범위

- 사용자는 작업에 필요한 만큼 세션을 쓰도록 요청했다. Roadmap·Current·Phase 1 실행서·1-B0·후보 비교·Electric Dreams 보고서·기존 로그의 세션 수에 따른 제한 문구를 삭제했다. 일정에 맞춘 범위 축소 제안은 채택하지 않았다. 콘텐츠 범위(GX-004)와 이번 단위의 반입/배치/베이크/빌드/측정 제외는 유지한다.
- **구역 채택:** 대분지 허브·동쪽 대절벽·협곡 하류 출구·골짜기 초입·잔도 하부를 먼저 검토한다. 대표 구역을 먼저 꾸민 뒤 Ridge 전체로 확장하는 순서를 사용자와 확인했다. 경계안 채택은 실제 PIE 구도 검증 통과와 구분한다.
- **P0 채택:** 사암을 큰 면의 주조로 쓰고, 이끼·낮은 식생은 분지 바닥·하상에 제한한 혼합안이다. 메시·재질의 사용자 조건부 선택은 아직 없다.

### Z1 근거와 한계

- spec·기존 평면도·분지/정상 렌더를 읽고 대표 구역 다각형을 만들었다. 원자료는 `Saved/Profiling/Graphics/Phase01/RidgeZonePalette_20261113/`의 `ZoneDraft.json`·`HeightfieldSamples.csv`·`ZoneDraft.png`, 계산/도식은 같은 폴더 `AnalyzeZone.py`·`DrawZone.ps1`이다.
- 1m 표본 기준 로컬 평면 약 8,630m², 구면 spec 표면 추정 약 19,933m²다. 30° 미만 약 5,578m², 30~70° 약 2,526m², 70° 이상 약 11,830m²다. 현지 Up과 법선으로 계산했으며 Support 판정이 아니다. 2m 표본 총면적 19,930m²와 비교했지만 실제 메시 정확도 근거로 사용하지 않는다.
- 암반/지하 몸체의 대체 footprint(로컬 약 1,238m²)는 별도 집계했다. 표면 총계는 대체 전 높이장의 가상 면을 포함하므로 실제 지각/암반 표면 합계가 아니다. 지하 벽/바닥·개구부·섬의 겹친 레이어는 적분하지 않았다.
- 대분지 Pod 25m뿐 아니라 인접 정상 35m·골짜기 32m도 표시했다. 수치 집계의 Pod 판정은 3D 거리 추정이고 그림의 원은 로컬 평면상의 참고다. 경로 가장자리+2m는 초안이다. 훅·입구·카메라와 메시 전체 범위 검토가 빠진 잔여 면적은 배치 가능 면적으로 보고하지 않는다.
- 대표 구역의 표본은 이음매 보존/전이 띠와 겹치지 않았다. 구역 경계와 세부 해석은 [1-B0 §8](../phases/Phase01_RidgeZonePalette.md#8-결과)에 기록했다.

### 연결·읽기 확인

- MCP 8000은 `/Game/Maps/TestMap03`과 정식 Ridge를, 8001은 `/Game/Levels/Startup`과 Electric Dreams CloseRange를 반환했다. 로그의 포트 8000/8001도 확인했다. 시작 시 각 PIE=false·현재 레벨 dirty=false였다. 원본 레벨 열기·뷰포트 이동·에셋 편집/저장은 하지 않았다.
- P1 준비: 별도 Blender background에서 `--disable-autoexec`로 원본을 읽고 저장 없이 UV만 기록했다(`InspectBlend.py`·`BlendUVRead.json`). 지각은 LightMapUV 1개, 암반 Floor/Shell은 UV 없음. UE 3메시는 UVChannels=2·기본 슬롯 WorldGridMaterial·Nanite=false였다(`RidgeMeshRead.json`). 생성 스크립트/Production은 컴포넌트 M_Ridge_Ground/Rock 오버라이드를 명시하므로 기본 슬롯을 실효 재질로 해석하지 않았다. 실제 컴포넌트 재질·UE UV 배치 적합성은 미확인이다.
- 정식 메시 세 개는 조회 뒤 dirty=false였다. spec·blend의 SHA-256은 Z1 기록값과 읽기 조사 종료 시 동일했다. 두 프로젝트 에셋과 Ridge 원본은 수정하지 않았고 계산용 파일은 Saved에만 기록했다. 에셋 반입·PCG 제작/배치·베이크·빌드·성능 측정은 하지 않았다.

### 다음 행동

1. Z2 사용자 PIE를 기다린다. 두 에디터가 열려 있다는 답변 뒤에도 IsPIERunning=false라 LootNPop에서 ▶ Play로 시작하도록 구체적으로 요청했다. 시작하면 현재 Ridge slot을 확인해 네 검토점 이동 명령을 제공한다.
2. 구역 화면·밝기·적 노출 확인 뒤 Z3·Z4 보호/접촉 위험 → P1 방침 사용자 확인 → P2·P3 조건부 팔레트 사용자 확인 → P4 의존성으로 이어 간다. 1-B0은 아직 완료하지 않았다. 후속 단계와 미확인 사항은 §8을 따른다.

### 추가 — Z2 PIE 시작·실제 slot 확인

- 사용자가 PIE 시작을 알렸다. 8000 IsPIERunning=true와 2026-11-13 11:29 KST 시작 로그에서 Ridge **slot 5**, 회전 `R(P=180.00, Y=90.00)`를 확인했다. 8개 옥탄트 로드 완료 로그도 확인했다. 근거는 같은 원자료 폴더 `Z2_PIELog.json`이다. 이번 일반 PIE의 시드를 확인하지 않았으므로 벤치마크 시드 1 배정으로 취급하지 않는다.
- 서버 콘솔 명령 `LNP.Debug.TeleportToReview <Name> 5`를 네 검토점(DeepBasin·CrownEastRim·ValleyRise·TablelandRim)에 대해 제공했다. 사용자의 구도·분지 밝기·적 식별성 검토 응답을 기다린다. 실제 화면 확인 전이므로 Z2 체크는 미완료로 유지했다. 에디터/PIE를 대신 시작·종료하거나 에셋을 편집·저장하지 않았다.
- §8과 Current의 바로 다음 행동을 PIE 시작 요청에서 사용자 화면 검토 결과 확인으로 갱신했다.

### 추가 — Z2 사용자 통과·Z3/Z4 정리·P1 추천

- 사용자가 네 검토점에 대해 **"모두 문제없어"**라고 응답했다. Z2의 대표 구역·카메라 후보·분지 밝기·적 식별성 확인을 완료로 표시했다. 고정 A/B/C 좌표·연결 경로·벤치마크 시드 1 배정·정식 측정의 통과로 확대 해석하지 않았다.
- Z3 보호 대상(기존 경로/착지·인접 Pod·TablelandRimHub·HallBasinGate·검토점·이음매)과 Z4 위험/시험 항목을 §8.5에 정리했다. 돌출·밀도·마스크 수치는 후속 반입/배치 단계로 넘겼다. 장식은 Decoration 태그 단독+LNPDecoration profile을 따른다.
- P1 비교를 위해 원본 메시 재질 3개와 Surface 텍스처 2세트·기존 부모 재질/관련 이름을 읽었다. 원자료 `P1_SourceMaterials.json`·`P1_SurfaceTextures.json`. 메시 재질은 공통 UV 기반 DynamicLayering 부모이고, 현행 Ridge 원본에는 적합한 표면 UV가 확인되지 않았다. 정식 LVI 의존성에는 M_Ridge_Ground/Rock이 포함되며, 라이브 M_Ridge_Ground는 Constant3Vector 노드 1개다.
- **추천/답변 대기:** 대표 구역에 단순 불투명 재질을 적용하고 BeachCliff Surface를 큰 암반, MossyRockyGround Surface를 분지 바닥 일부의 조건부 후보로 쓴다. 6개 텍스처는 모두 4K VT다. 두 BC 썸네일을 MCP로 확인했다. 변위/WPO는 쓰지 않는 방향이며 투영 방식·노멀·스케일·반복무늬·구면 회전·원본 사암 메시와 색 일치는 반입 시험 담당이다. 재질을 생성/수정하거나 반입하지 않았다.
- 원본 MI 3개를 처음 오브젝트 경로로 dirty 조회했을 때 true를 받았다. **아래 추가 비교에서 이 결과가 경로 형식에 따른 도구 오탐임을 확인했다. 올바른 패키지 경로 조회는 모두 false다.** load/get/list/capture만 호출했고 편집·저장·되돌리기를 하지 않았다.
- P2 준비로 9개 후보 메시 Registry 태그만 읽어 `P2_CandidateTags.json`에 보관했다. 이전 보고서와 달라진 수치는 이번 현행 태그를 기준으로 재확인하며 Nanite와 fallback을 구분한다. 메시 선택/P3 원본 맵 시각 확인/P4 의존성은 아직 완료하지 않았다.

### 추가 — P1 원본 재질 재사용 비교

- 사용자가 초기 단순 재질안 대신 **원본 재질을 재사용하는 안을 더 비교**하도록 요청했다. 방침을 채택한 것으로 처리하지 않았다.
- Surface 텍스처의 referencer를 따라 `/Game/Landscape/M_BGLandscape_Auto`와 `MI_BGLandscape_Auto_01`을 확인했다. 큰 면의 기반으로 쓸 원본 지형 그래프가 실제로 있고, BeachCliff·MossyRockyGround 외 JungleGround·MossyGrass·노이즈도 참조한다. 이전 `/Game` 이름 검색에 WorldAligned 에셋이 없었던 것은 기능 부재의 근거가 아니었다. 원본이 엔진 WorldAlignedTexture/Normal 함수를 사용함을 확인했다.
- 메시용 원본 부모는 UV0×Tiling, 지형용 부모는 월드 normal B(Z) 기반 경사 마스크·지면 UV0·절벽 WorldAligned 함수 XY 출력에 의존한다. 두 부모의 WPO 출력은 비어 있고 테셀레이션은 꺼져 있다. 일부 출력 enum은 도구에서 오류라 읽지 못했으며 그 출력의 안전성까지 완료로 보고하지 않았다.
- **현재 추천/답변 대기:** 원본 지형 그래프/MI의 표현을 재사용하는 파생본을 우선 시험하고 보강 메시에는 원본 MI를 유지한다. 구면 경사 마스크·좌표/노멀·대표 구역 마스크 보완이 필요하다. 형상 변위/WPO는 쓰지 않는 방향이다. 초기 단순 재질안은 대안으로 남겼으며 P1 완료 체크는 아직 하지 않았다. 원본 파생본 채택 시 P4는 원본 지형 부모/MI의 전체 참조를 시작점에 포함해야 한다.
- 원자료: `P1_OriginalParentInventory.json`·`P1_OriginalCoordinateRead.json`·`P1_OriginalWiring.json`·`P1_OriginalLandscapeMI.json`. 근경 품질·구면 투영·색 일치·성능은 아직 시험하지 않았다. 에셋 반입/생성·원본 편집/저장·PCG·베이크·빌드·성능 측정은 하지 않았다.
- **dirty 설명 정정:** 동일 MI를 패키지 경로→오브젝트 경로→패키지 경로로 조회하니 false→true→false였다. 설치 엔진 `Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/asset.py`의 is_dirty 구현은 패키지 경로 비교와 디스크 존재 검사에 의존한다. 오브젝트 경로를 넘긴 초기 true를 미저장 변경으로 해석한 것이 잘못이었다. 올바른 패키지 경로로 부모 2개·지형 MI·메시 MI 3개·Startup을 조회한 결과는 모두 false이며 `P1_ReuseReadEndState.json`에 기록했다. 원본 디스크 수정 시각은 모두 2026-11-12였다(`P1_OriginalDiskRead.json`). 사용자에게도 정정했다. 원본을 저장하거나 변경을 되돌린 일은 없다.

### 추가 — P1·P2/P3 사용자 채택, P4 계산·1-B0 완료

- **P1 사용자 채택:** 원본 지형 재질 파생본+보강 메시 원본 MI 유지. 구면 경사·좌표/노멀·대표 구역 마스크는 후속 시험에서 보완하고 지각 재질의 변위/WPO는 사용하지 않는다. 단순 재질안은 대안으로 남겼다.
- 8001에서 원본 CloseRange를 열었다. 첫 전경 캡처 뒤 두 근접 구도가 잎에 가려, 자동 탐색을 확대하지 않고 사용자에게 카메라 조작을 요청했다. 사용자가 바닥과 높은 절벽이 함께 보이게 구도를 잡았고, MCP로 캡처해 검토에 충분함을 확인했다. 카메라/캡처 메타데이터는 `p3_userscene.json`, 원본 PNG는 `p3_userscene.png`다. 6메시의 개별 썸네일도 MCP로 확인/보존했다. 데스크톱 전체 캡처·원본 장면/설정 편집·PCG 제작/재생성 명령은 사용하지 않았다.
- **6종 사용자 조건부 채택:** SM_Cliff_01, SM_SandstoneBoulder_01, SM_MossyForestBoulder_06, SM_DryRiverBed_02, SM_CloverVarieties_01, SM_Fern_01. 기존 CloseRange 사용례를 기준으로 Cliff와 하상 후보를 좁혔다. MossyForestBoulder_06의 실효 원본은 MI_MossyForestBoulder_09다. 경로·치수·원본 재질/충돌·Nanite/로드 LOD0·조건은 [선정 결과 §8](../phases/Phase01_RidgeZonePalette.md#8-결과)이 소유한다. 나무·선반형 바위 등은 현재 팔레트에서 보류했다.
- Cliff는 부분 보강 시험만, Fern은 Pod/통로/훅/카메라 밖에서 바람/회전을 시험하는 조건이다. Clover의 원본 부모 WPO는 미연결, Fern은 연결이다. 식생의 원본 WPO와 지각 재질의 WPO 방침을 구분했다. 돌출·밀도·스케일·투영은 확정하지 않았다.
- P4는 6메시·실효 MI 6개·지형 부모/MI 2개, 총 14개 명시 시작점의 `/Game` 전이를 중복 없이 계산했다. **104패키지·104 uasset, 원본 1,785,971,111 bytes(1,703.23MiB, 1.663GiB)**, 누락 0개. 텍스처 66개 중 VT 65개, 4K 31·2K 24·1K 4·256² 5·32² 2개다. 원본 디스크 크기이며 cook 크기/VRAM/성능 수치가 아니다. 초기 약 2GB 가이드 안이다.
- StaticMesh 패키지는 7개다. 선택 6종 외 하나는 DryRiverBed_02의 complexCollisionMesh `/Game/Meshes/_GENERATED/RockFormation/SM_DryRiverBed_02` 참조다. 장식 후보로 추가 선정한 것은 아니며 전이 목록에는 포함했다. 타깃 NoCollision 반입 때 이 참조를 보존/제거할지는 별도 판단한다. 예비 후보 의존성은 현재 합계에서 제외했다.
- 원자료는 `Saved/Profiling/Graphics/Phase01/RidgeZonePalette_20261113/`에 있다. 사용례/실효 재질/로드 LOD0, MCP PNG, 전체 의존 그래프/파일 목록/집계, 조회·계산 스크립트를 보존했다. 패키지별 부분 합은 공유 참조가 있어 합산하지 않는다. 일부 초기 Registry Triangles와 로드 LOD0이 달라 후자를 표에 기록했다.
- **읽기 전용 확인:** source CloseRange와 의존 패키지 104개 모두 dirty=false(`SourceFinalState.json`), LootNPop TestMap03와 Ridge의 LVI/3메시/2재질 모두 dirty=false(`TargetFinalState.json`). Z1 spec/blend 2개·P4 시작점 파일 14개·이전 P1 재질 파일 6개의 해시는 비교 시점과 같았다(`ReadOnlyFinalVerification.json`, 중복 파일 포함 비교 그룹). 에셋 반입/제작·배치·베이크·빌드·성능 측정은 하지 않았다. 기존 미커밋 코드 변경은 건드리지 않았다.
- 1-B0 §7은 실제 완료 항목만 체크하고 §8을 최종 결과/조건/미확인 담당/바로 다음 행동으로 정리했다. Current·상위 실행서 1-B 행/단위 체크·Roadmap을 갱신했다. **1-B0은 완료, Phase 1과 환경 구현은 미완료**다.
- **바로 다음:** 1-A에서 벤치마크 실제 시드/slot과 A/B/C·연결 경로를 확정해 콘텐츠 반입 전 출발 기준선을 확보한다. A=DeepBasin·B=TablelandRim·C=ValleyRise를 후보로, CrownEastRim은 높은 하향 비교 예비로 남겼다. 이후 반입 준비·작은 반입 시험·팔레트 최종 확정→PCG/전체 확장, 파라곤 적, 전후 측정으로 진행한다.

### 추가 — 새 세션 인수인계·단일 커밋 정리

- 사용자가 **1-A를 새 세션으로 넘기고 인수인계 문서와 현재 변경을 한 번에 커밋**하도록 요청했다. 이번에는 1-A 카메라/하네스 수정·빌드·쿡·측정을 시작하지 않았다.
- 상위 실행서 §7에 완료/미완료 근거, 실제 slot과 예측의 구분, 카메라 후보, 코드/JSON/NonUFS/기존 Viewpoints 검사/패키저/런처의 연결 지점, 빌드/쿡 직전 확인과 정식 측정 순서, 새 세션 시작 프롬프트를 추가했다. Current와 README에서 이 진입점을 연결하고 1-B0의 이전 재개 프롬프트를 완료 상태에 맞게 정리했다.
- 기존 미커밋 적 0 진단 코드·집계기·현행 cook 목록 준비와 1-B0 선정 문서를 이번 단일 커밋 범위로 확인했다. 기존 프로젝트 양식인 `feat: 한국어 요약`과 항목별 본문을 따른다. 라이선스 에셋·Saved 원자료·새 환경 구현은 커밋에 포함하지 않는다.
- 커밋 전 `Saved/Profiling/Graphics/Phase01/Preparation_20261111/VerifyAnalyzer.ps1`을 다시 실행해 합성/거부·기존 Medium 재집계·cook root 존재 검사 **8개 모두 통과**했다. 이는 집계/목록 검사이며 정규 빌드·실제 적 0 실행·새 출발 기준선의 검증은 아니다. 해당 미확인 범위를 인수인계에 유지했다.

## 2026-11-13 — 1-A 착수·시드 1 수동 PIE 준비

- 필수 문서 3개·Phase 1 실행서 §7·1-B0 선정 결과 §8과 실제 하네스를 대조했다. 첫 단위는 실제 시드/slot과 A/B/C 구도·연결 경로 확정이다. 단위별 사용자 확인을 유지하며 1-B0 재선정·콘텐츠 반입은 하지 않는다.
- LootNPop MCP에서 현재 레벨 `/Game/Maps/TestMap03`, PIE=false를 확인했다. 프로젝트 등록 스킬 목록에는 이번 조회/카메라 작업에 추가 적용할 스킬이 없었다.
- 시작 상태: `LNP.Graphics.Benchmark=0`, Profile=Low, Warmup/Duration=60, `LNP.Spawn.EnemyDensity=0`, `LNP.Spawn.ConfigOverride` 빈 문자열. `BP_LNPGameState` CDO의 시드/PCG offset은 0/0이었다.
- 복구 정보를 `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/Recovery.json`에 기록한 뒤 CDO의 `octantGenSeed`만 메모리에서 1로 설정·재조회했다(PCG offset=0 유지). 에셋은 저장하지 않았다. 원본 BP SHA256는 `1833230091A9A1EC75933E0B47C8EFAC0C02982551A94760B233EFFC275A27F1`이다. **사용자 PIE 시작 후 런타임 시드 확인과 함께 CDO를 원래 0으로 복구하고 디스크 해시를 대조해야 한다.**
- 사용자에게 에디터 콘솔에서 Medium 오버라이드·밀도 1 설정 후 1인 Standalone PIE 시작을 요청한다. 기존 하네스의 Acropolis/slot 6 검증은 남아 있으므로 이번 수동 구도 확인에서는 하네스를 켜지 않는다. 실제 배정과 런타임 GameState는 시작 후 확인한다.
- 아직 미확인: 실제 Ridge slot(1·7은 여전히 예측), A/B/C pose·FOV/aspect·연결 구간. 소스 구현·컴파일·쿡·패키지·성능 측정은 하지 않았다. 다음은 실제 배정 확인 후 A=DeepBasin 구도를 사용자가 잡고 pose를 수집하는 것이다. 구도 확정 뒤 다음 구현 단위 진행 여부를 확인한다.

### 추가 — 시드 1 실제 배정 확인·A 구도 대기

- 사용자 PIE 시작 후 PIE=true·런타임 GameState 시드 1/PCG offset 0·Complete·플레이어 1명을 읽었다. 2026-11-13 13:11:56 KST 시작 로그의 실제 배정은 `[Acropolis, Ridge, Bastion, Pyramid, Grove, Hollow, Pyramid, Ridge]`다. **Ridge slot 1·7을 실제 확인했다.** 같은 시작 로그에 Medium 오버라이드, WorldDevice seed=24300, 초기화 완료가 있다. 이 확인은 고정 벤치마크 카메라/패키지 검증과 별개다.
- CDO 시드 0 복구 성공과 PCG offset 0을 재조회했다. GameState BP 디스크 SHA256가 준비 전과 동일함도 확인했다. 에셋 저장·PIE 종료는 하지 않았다. Medium·밀도 설정은 구도 확인 중 유지한다.
- 원자료는 `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/Seed1PIECheck.json`이다. slot 1을 카메라 기준 후보로 잡고 `LNP.Debug.TeleportToReview RidgeDeepBasin 1` 실행 후 대절벽/분지 바닥/적이 보이는 A 구도 조작을 요청한다. A가 준비되면 pose·렌즈를 수집하고 B/C로 이어 간다. 전체 구도·연결 경로 단위는 아직 완료되지 않았다.

## 2026-11-14 — 1-A A 구도 후보 수집

- 사용자 “A 준비됐어” 알림 후 PIE=true와 실제 카메라/플레이어 pose를 수집했다. 현재 runtime GameState는 시드 **2982**·PCG offset 0·Complete·플레이어 1명이다. 최근 시작 로그와 `RidgeDeepBasin (slot 1)` 이동 로그를 확인했다. 이전 시드 1 회차와 구분하며 기존 실제 배정 확인 기록은 보존한다.
- 현재 CVar는 밀도 1·편성 오버라이드 빈 문자열이다. 따라서 A는 구도 후보 수집이며 Medium 편성/시드 1 기준선 확인으로 취급하지 않는다. B/C 수집 후 최종 경로는 시드 1·Medium에서 다시 확인한다.
- `PlayerCameraManager_0`의 실제 게임 카메라와 `BP_LNPPlayer_C_1`의 월드 pose를 같은 배치 호출로 읽었고, 두 차례 호출 사이 값이 동일했다. slot 1 Level Instance의 실제 Ridge 에셋·원점·Yaw 90°·단위 스케일을 확인했다. slot 역회전으로 로컬 좌표/쿼터니언을 저장하고 회전 왕복 오차 및 기존 Phase 0 A 기준 pose/쿼터니언 대조를 1e-10 허용오차로 통과했다.
- A 로컬 카메라(cm)는 `(14126.49257945583, 11981.177118997784, 25578.3334521097)`, 쿼터니언 XYZW는 `(-0.8181976050010245, -0.3955483232059188, 0.3972379035773024, -0.12765677083916582)`다. FOV≈80°·컴포넌트 aspect≈1.3331548·표기 축 제약 MaintainYFOV, aspect 제한=false·축 제약 override=false다. LocalPlayer의 실효 축 제약은 아직 확인하지 못했으므로 최종 렌즈 검증이 필요하다. 컴포넌트 aspect를 실제 1440p 출력 비율 확인으로 쓰지 않는다.
- 원자료: `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/A_DeepBasin_Raw.json`·`A_DeepBasin_Candidate.json`. 저장 폴더 날짜는 앞선 동일 작업의 복구 정보와 묶기 위해 유지하며 캡처 날짜는 2026-11-14로 기록했다. 화면 캡처·에셋 변경·CVar 변경·빌드/쿡/성능 측정은 하지 않았다.
- 다음은 `LNP.Debug.TeleportToReview RidgeTablelandRim 1` 실행 후 절벽 측면·분지를 보는 B 구도 수집이다. B/C·연결 경로 확인까지 같은 작업 단위로 진행하며, 단위 완료 후 다음 구현 진행 여부를 묻는다.

### 추가 — B TablelandRim 구도 후보 수집

- 사용자가 “절벽 아래 분지를 내려다보고 왼쪽에 대절벽 일부가 함께 보이는 구도”를 설명하고 준비 완료를 알렸다. B의 절벽 측면·분지 노출 목적에 부합하는 설명임을 확인하고 수집했다. 화면을 직접 캡처/검토한 것은 아니다.
- PIE=true와 `RidgeTablelandRim (slot 1)` 이동 로그, 실제 Ridge Level Instance의 원점/Yaw 90°/단위 스케일을 확인했다. 런타임 시드 2982·PCG offset 0·Complete·플레이어 1명이며 이동으로 재생성된 플레이어는 `BP_LNPPlayer_C_2`다.
- 게임 카메라와 플레이어 pose를 두 번 읽어 일치를 확인하고 Ridge 로컬 좌표/쿼터니언으로 저장했다. 정규화·월드 회전 왕복을 1e-10 허용오차로 통과했다. B 로컬 카메라(cm)는 `(11128.920084385212, 15446.863503344848, 20202.150679161663)`, 쿼터니언 XYZW는 `(0.9543650238795534, 0.07145389454196766, -0.01860617486256938, 0.2893709598545583)`다. FOV/aspect와 관련 플래그는 A와 동일하며 렌즈 실효 축 제약 검증은 남아 있다.
- 원자료: `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/B_TablelandRim_Raw.json`·`B_TablelandRim_Candidate.json`. 출처 시드와 사용자 구도 설명을 명시했다. 에셋/렌더 설정 변경·소스 구현·빌드·측정은 하지 않았다.
- 다음은 `LNP.Debug.TeleportToReview RidgeValleyRise 1`에서 허브 전경을 보는 C 구도 수집이다. 이후 시드 1·Medium에서 최종 경로/연결 구간을 확인한다. 카메라 작업 단위는 아직 진행 중이다.

### 추가 — C ValleyRise 수집·A/B/C 후보 통합, 다음 구현 진행 확인 대기

- “대분지 허브”가 A와 같은 분지 바닥·Pod 주변을 뜻함을 설명했다. C는 ValleyRise의 발앞 지형/분지 진입길과 그 너머 분지를 담는 취지이며 A와 대상이 겹쳐도 된다고 안내했다. 사용자가 “C 준비됐어”라고 알린 뒤 수집했으며 실제 화면을 직접 검토한 것은 아니다.
- PIE=true·`RidgeValleyRise (slot 1)` 이동 로그와 Ridge Level Instance의 실제 에셋/원점/Yaw 90°/단위 스케일을 확인했다. 시드 2982·PCG offset 0·Complete·플레이어 1명이고 현재 플레이어는 `BP_LNPPlayer_C_3`다.
- C 게임 카메라/플레이어 pose의 두 차례 읽기 값이 일치했다. 로컬 회전 쿼터니언 정규화·월드 왕복을 1e-10 허용오차로 통과했다. C 로컬 카메라(cm)는 `(13900.145215414032, 15391.090894585082, 24604.96972345391)`, 쿼터니언 XYZW는 `(0.9113247022208472, 0.14771302004129033, -0.3148181370014409, 0.22035809821131586)`다. A/B/C의 FOV/aspect/관련 플래그 값은 모두 동일하다.
- 같은 원자료 폴더에 `C_ValleyRise_Raw.json`·`C_ValleyRise_Candidate.json`과 `RidgeViewpoints_Draft.json`을 저장했다. 초안의 목표는 시드 1·PCG offset 0·Ridge slot 1이며 각 점의 출처 시드 2982와 원본 pose를 보존했다. Saved 초안만 작성했고 정식 JSON/C++/스테이징/검사는 변경하지 않았다.
- **후보 수집은 완료**, 정식 경로 확정은 미완료다. A→B→C→A 이동 중 무관통·환경/적 노출, LocalPlayer 실효 렌즈 축 제약, 시드 1·Medium의 최종 재현은 남아 있다. 성능 측정·빌드·쿡은 미실행이다.
- 다음 구현 단위는 §7.2의 Ridge JSON·C++ 검증/slot 회전·NonUFS·패키저·런처·Viewpoints 검사를 함께 맞추는 작업이다. 사용자 요청대로 진행 여부를 확인한 뒤 착수한다. 이후 빌드/Live Coding 직전에 병행 작업 여부를 확인하고 고정 경로의 사용자 육안 검증을 받는다.

### 추가 — Ridge 하네스 구현·Live Coding·자동화 검증 완료

- 사용자 “계속 진행해줘” 승인으로 경로 하네스 구현 단위에 착수했다. `Scripts/Profiling/Graphics/RidgeViewpoints.json`을 만들고 A/B/C의 로컬 카메라/플레이어 pose와 출처 시드 2982를 보존했다. 목표 시드 1·PCG offset 0·Ridge slot 1이며 기존 직선 위치 보간/회전 Slerp/루프 순서는 유지했다. Acropolis JSON은 과거 기록으로 보존했다.
- C++의 ReadViewpoints 대상/slot 검증·Prepare 실제 배정 검사/기본 JSON·UpdateView slot 회전, Build.cs NonUFS, 패키저 존재 확인, 런처 파일 조회/해시를 모두 Ridge로 맞췄다. 기존 편성·풀·렌더 설정·지형/장식 에셋은 변경하지 않았다.
- Viewpoints 자동화 검사의 세 점 월드 위치/회전 기대값을 각 원본 캡처로 갱신했다. slot 6/Acropolis 에셋 거부, 캡처 aspect 보존, LocalPlayer 기본 MaintainYFOV 확인을 추가했다. 기존 잘못된 좌표계/쿼터니언 거부·구면 roll·루프/보간 검사는 유지했다.
- 엔진 BaseEngine.ini의 LocalPlayer 기본 축 제약은 MaintainYFOV다. CameraComponent.cpp는 override=true에만 카메라 제약을 전달하고 CameraStackTypes.cpp는 값이 없으면 LocalPlayer 제약을 쓴다. 프로젝트 소스/Config/현재 Saved Config에는 관련 재정의를 찾지 못했다. MCP 기본 LocalPlayer 속성 조회는 노출되지 않아 실패했으며, C++의 실제 GetDefault<ULocalPlayer>() 검사를 추가해 실행·통과했다. 실효 출력 구도는 후속 육안 검증 대상이다.
- 정적 정합 **16개**와 PowerShell 두 파일의 AST 구문 검사, `git diff --check`가 통과했다. 정적 검사는 원본 캡처 대비 월드 위치 0.001cm·회전 1e-10 허용오차, 렌즈/출처, 대상·파일 참조 일치를 확인한다. 원자료는 `HarnessStaticValidation.json`이다.
- Current §2의 실행 직전 병행 확인 규칙에 따라 사용자에게 물었고 **“병행 작업 없음 — 컴파일·검사 진행”** 답변을 받았다. PIE는 이미 종료된 상태였다(종료 호출은 실행 중 세션 없음으로 반환, 뒤 재조회도 false). 이후 MCP CompileLiveCoding이 **Result: Success / Live coding succeeded**를 반환했다.
- 테스트를 강제 재발견하고 갱신된 **LootNPop.Graphics.Benchmark.Viewpoints 1/1 통과**, 오류/경고 0, 0.0158315초를 확인했다. 근거는 `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/HarnessLiveCoding.json`·`HarnessAutomation.json`이다. 이 결과는 정규 전체 Editor 빌드·Game 패키지 검증과 구분한다.
- **이번 구현 단위는 완료.** 다음은 사용자 진행 확인 뒤 시드 1·Medium의 고정 경로를 재생해 A/B/C와 연결 구간의 무관통·환경/적 노출을 육안 확인하는 단위다. 필요하면 경로를 조정한 뒤 같은 최종 경로로 출발 기준선을 잡는다. 실제 NonUFS 스테이징·정규 빌드·패키지·적 0 진단·출발 기준선·환경 반입은 아직 하지 않았다.

### 추가 — 고정 경로 육안 검증 승인·수동 PIE 시작 요청

- 사용자가 다음 단위 진행을 승인했다. PIE=false와 하네스=0·Profile=Low·Warmup/Duration=60을 확인했다. 기존 하네스는 시드 1·PCG offset 0·선택 편성/밀도 1을 강제하고 1인 Standalone만 허용한다.
- 수동 PIE가 더 효율적이므로 별도 테스트 장치 없이 사용자에게 Profile=Medium·Warmup=60·Duration=60·하네스=1 설정과 Play 시작을 요청한다. 경로는 각 루프에서 A=0초·B=20초·C=40초·A=60초이며 워밍업과 캡처에 같은 경로를 사용한다. 원복 정보는 `PathPreviewPreparation.json`에 저장했다.
- 아직 시작/관통/구도 결과는 미확인이다. 시작하면 실제 runtime 시드/slot/편성·저장된 Viewpoints.json을 확인한다. 기존 에디터 기본 출력 루트는 `Saved/Profiling/Graphics/Phase00/Medium_<GUID>`이며 이번 회차 경로를 로그에 남긴다. 이 결과는 성능 기준선에 합치지 않는다. 소스 추가 변경·컴파일·패키징은 하지 않았다.

### 추가 — 첫 경로 확인 PIE 크래시·복구 검증 준비

- 사용자가 설정 후 PIE 시작을 알렸고 이어 크래시 콜스택을 전달했다. MCP 조회 일부가 연결 실패로 끝났으며 파일 로그/크래시 보고서와 사용자 첨부를 대조했다. LootNPop 에디터 프로세스가 종료된 상태도 확인했다.
- 2026-11-14 15:13:25 KST Medium 하네스가 waiting으로 시작했다. 실제 Ridge slot 1/7·Medium 오버라이드·WorldDevice seed=24300(시드 1)을 확인했다. 모든 엔티티 생성 뒤 플레이어 컴포넌트 등록/애니메이션 초기화 중 `ULNPAnimInstance::UpdateStates` → `LNPAnimInstance.cpp:733`의 CVar 읽기에서 null 접근으로 종료됐다. 카메라 Prepare 완료/경로 재생/프레임 캡처 전에 실패했으므로 육안 확인은 미검증이다.
- 실패 회차는 `Saved/Profiling/Graphics/Phase00/Medium_0D773E3F40ECA2FB2E3D8D8942A3128B`이며 Run.json은 waiting·0프레임·performance_eligible=false다. 크래시 로그/컨텍스트·사용자 콜스택·manifest·요약을 `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/PreviewCrash_20261114/`에 보존했다. 원래 minidump 위치도 Summary.json에 기록했다.
- Live Coding patch_0 코드의 정적 애니메이션 CVar 상태 문제 가능성을 가설로 두었다. 애니메이션 소스는 변경하지 않았다. 우선 에디터 종료 상태에서 정규 Editor 빌드로 패치를 분리해 재검증한다. 프로젝트 실행 직전 확인 규칙에 따라 사용자에게 다른 빌드/Live Coding/쿡의 병행 여부를 질문했으며 답변 대기다.

### 추가 — 사용자 풀빌드/재시작 완료·새 에디터 자동화 통과

- 사용자가 “에디터 풀빌드하고 다시 띄웠어”라고 알렸다. 빌드 로그 원문은 수집하지 않았으며 사용자 완료 보고와 새 프로세스 조회를 구분해 기록한다. 추가 소스 변경·Live Coding은 하지 않았다.
- MCP 읽기 호출로 연결 복구와 `/Game/Maps/TestMap03`·PIE=false를 확인했다. 하네스=0·Profile=Low·Warmup/Duration=60으로 초기화됐고 `LNP.Anim.DebugLocomotion`은 등록돼 값 0을 반환했다. 새 에디터 로그에는 이 실행에서의 Live Coding 컴파일/성공 또는 크래시 기록이 없었다.
- 새 에디터에서 테스트를 재발견하고 `LootNPop.Graphics.Benchmark.Viewpoints` **1/1 통과·오류/경고 0**, 0.0151692초를 확인했다. 근거는 `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/FullBuildRestartAutomation.json`이다. 이 검사는 플레이어 스폰을 다루지 않으므로 크래시 해결로 표시하지 않는다.
- 다음은 Profile=Medium·하네스=1만 다시 설정하고(현재 Warmup/Duration=60 유지) 1인 Standalone PIE를 재시도하는 수동 단계다. 시작 뒤 실제 편성/seed/slot·Prepare/카메라·manifest 및 사용자 무관통/구도 결과를 확인한다. Live Coding 관련성은 이 재검증 결과 전까지 가설로 유지한다.

### 추가 — 풀빌드 재시도 정상 종료·사용자 B-C 관통 보고·B +50cm 조정

- 재시도는 시드 1·PCG offset 0·Complete·플레이어 1명, Medium·Ridge slot 1/7을 확인했다. 실제 CameraActor가 생성됐고 CameraComponent의 MaintainYFOV override=true·FOV/aspect를 읽었다. 경로 해시 B5023004E585775F56128FED2763A9A52F66C4303F8AE124B267A92751D0AAFA가 당시 소스와 일치했다.
- 회차 `Saved/Profiling/Graphics/Phase00/Medium_05B4D1C44AE0193BE8E9428109137E3D`는 complete·3494프레임·초기/마지막 적 2002·Pod 120, CSV 저장을 확인했고 AnalyzeGraphicsBenchmark -AllowPreview가 통과했다. 에디터 1147×859·performance_eligible=false로 성능 기준선에 합치지 않는다. 풀빌드 재시도에서 스폰 크래시는 재발하지 않았으나 원인은 확정하지 않았다. 시작·상태·실효 렌즈 근거는 FullBuildRetryStartLog.json·FullBuildRetryState.json·FullBuildRetryEffectiveLens.json이다.
- 사용자가 B→C에서 B의 절벽 모서리 스침을 보고하고 B를 공중으로 조금 옮기는 안을 제안했다. SceneTools의 실제 PIE world·Visibility 복잡 충돌 라인 추적으로 기존 B-C의 첫 충돌 거리 351.586cm를 확인했다. 25/50/100/200cm의 현지 Up 이동에서는 A-B/B-C 중심선 충돌이 없었다. 작은 근접면 여유를 위해 50cm를 채택했다.
- B 카메라 로컬 위치를 `(11108.874780012287, 15419.040760600383, 20165.76275865537)`로 조정했다. 원점 방향으로 정확히 50cm이며 시선·렌즈·A/C·원본 플레이어 pose 불변을 정적으로 확인했다. A-B/B-C/C-A의 정/역방향 중심선 6개도 모두 비충돌이다. 수정 전 원본과 근거는 RidgeViewpoints_Revision0.json·RidgeViewpoints_Revision1_Validation.json에 보존했다.
- 자동화 검사 소스의 B 기대값도 원본 캡처+현지 Up 50cm로 갱신했으나 **재컴파일/자동화 재실행은 하지 않았다.** 기존 1/1은 수정 전 결과다. 하네스 런타임 변경이 없으므로 JSON만 다시 읽도록 PIE를 재시작해 사용자 육안 확인을 먼저 받는다. 최종 경로 수용 후 새 정규 빌드/검사를 한다. 현재 경로는 아직 최종 수용 전이다.

### 추가 — B +50cm 재생 완료·관통 해소 확인, 16:9 재검증 요청

- 수정 회차 `Medium_B4FCBD224B58C589CEA2FC84C99DA29C`는 complete·3467프레임·생존 적 2002로 정상 종료됐고 CSV 저장/AllowPreview 집계를 통과했다. 초기 적/Pod도 2002/120으로 이전 회차와 같다. 소스/실행 경로 해시는 C1E46E0BCEAAB9D9C3FE2F2BCE80BAD37E959663C4775895814350B17E023CE6이며 B의 현지 Up +50cm·원본 회전 보존을 확인했다. with_editor=true·performance_eligible=false로 성능 판정에서 제외한다.
- 사용자는 “관통이 사라지긴 했는데 … 16:9처럼 가로가 긴 화면비로도 확인해보고 싶어”라고 답했다. 기존 화면비에서 해소된 것은 확인하되 전체 경로 최종 수용은 16:9 확인 뒤로 둔다.
- 수직 FOV 유지 규약이라 16:9에서는 좌우가 더 보인다. 렌즈/경로는 바꾸지 않고 출력 창을 2560×1440으로 설정해 새 PIE를 시작하도록 요청한다. 시작 후 실제 viewport 크기와 해시를 대조한다. 자동화 B 기대값 갱신 뒤의 재컴파일은 여전히 미실행이며 최종 경로 확인 뒤 진행한다.

### 추가 — 가로형 화면 관통 없음·정상 종료, 경로 육안 검증 마감

- 사용자가 1280×720에서 관통이 없다고 확인했고, 이어 Complete 로그 확인 후 PIE를 종료했다고 알렸다. MCP에서 PIE=false를 재조회했다.
- 마지막 회차는 `Saved/Profiling/Graphics/Phase00/Medium_E55B2F1442E65F2309BBAC84A4F92FDE`이며 complete·3174프레임·시드 1·PCG offset 0·Medium·초기/마지막 적 2002·Pod 120이다. 경로 해시 C1E46E0BCEAAB9D9C3FE2F2BCE80BAD37E959663C4775895814350B17E023CE6이 소스와 일치하고 AllowPreview CSV 집계도 통과했다. 근거는 `PathVisualValidation.json`이다.
- 사용자 설정 크기 1280×720과 manifest 실측 1280×722를 구분한다. 기존 화면비 및 가로형 화면에서의 사용자 관통 해소 확인으로 에디터 경로 육안 검증 단위를 마감한다. 정확 2560×1440 출력·화질·성능은 패키지 단계에 남긴다. 해상도만 높인 동일 화면비의 추가 PIE 반복은 생략한다.
- 다음은 B +50cm 보정 검사 기대값의 재컴파일·Viewpoints 검사와 패키지 준비 단위이며 사용자에게 계속 진행 여부를 묻는다. 현재 하네스=1·Profile=Medium·Warmup/Duration=60을 재조회했으므로 하네스 0·Profile Low 원복을 요청한다. 후속 실행 전에는 실제 값을 다시 확인한다.

### 추가 — 최종 정규 빌드·검사·패키지 준비 단위 완료

- 사용자 진행 승인과 에디터 종료·병행 작업 없음 확인 후 시작했다. 최초 프로세스 검사는 다른 프로젝트 Electric Dreams를 함께 감지해 빌드 실행 전에 멈췄다. 권한 있는 읽기 전용 실행 인자 조회로 ElectricDreamsEnv만 남은 것을 확인하고 LootNPop만 대상으로 검사해 진행했다. 다른 프로젝트를 종료/수정하지 않았다.
- 소스/보존 에셋 13파일 해시, HEAD/SourceChanges.patch 해시, 17 cook root의 실제 파일 존재와 새 아카이브 경로를 기록했다. 현재 작업 트리는 미커밋 변경을 포함한다.
- 정규 Editor 빌드와 최종 B +50cm 기대값의 Viewpoints 자동화가 종료 코드 0으로 통과했다. 테스트 1/1·오류/경고 0이며 이번에는 Live Coding을 사용하지 않았다. 자동화는 NoLiveCoding·NullRHI로 정규 DLL을 로드했다.
- Game Development 빌드·cook·stage·archive도 종료 코드 0으로 성공했다. BuildCookRun 약 114초이며 cook 최종 요약은 오류 0·경고 1이다. 경고는 VSM physical pages=4096의 ProjectSetting 우선순위 유지다. 설정 변경은 하지 않았다. Foley notify 누락 의존성 Display 메시지는 로그에 보존했고 이번 작업에서 수정하지 않았다.
- 아카이브는 `Saved/Graphics/Phase01StartPackage_20261114`, 49파일·1388.06MiB(약 1.36GiB)다. 실제 Game exe와 컨테이너 5개·NonUFS 경로를 검증/해시 기록했다. Ridge 경로 해시가 소스와 동일하고 소스/기본 SpawnConfig/OctantPool/GameState 디스크 해시가 전후 동일함을 확인했다.
- 근거는 `Saved/Profiling/Graphics/Phase01/Build_20261114/`의 SourceSnapshot.json·BuildStatus.json·PackageVerification.json과 EditorBuild.log·Automation 보고서·PackageBuild.log다. 패키지 실행·적 0·정확 1440p·정식 기준선은 미검증이다.
- 이번 준비 단위를 마감하고 다음 짧은 Medium/적 0 패키지 실행 검증 진행 여부를 묻는다. 해당 실행이 통과한 뒤 정식 출발 기준선을 별도 단위로 수행한다.

### 세션 마감 — 새 세션 인수인계·통합 커밋(2026-11-14)

- 사용자가 새 세션 인계 준비, 필요한 문서 전체 갱신, 이전 메시지 형식에 맞춘 한 번의 커밋을 요청했다. 패키지 실제 실행은 시작하지 않고 1-A의 경로·빌드·패키지 준비까지를 이번 범위로 고정했다. 1-A 전체/Phase 1 완료로 표시하지 않는다.
- Current를 현재 완료 상태와 다음 행동 중심으로 정리하고 Phase 1 실행서 §8을 최종 결과로 압축했다. 단계별 경과는 이 로그에 보존했다. §9에 완료/미검증 구분·패키지/소스 식별·Saved 부재 시 처리·명시적 런처 인자·짧은 Medium/적 0 검증 항목·새 세션 시작 프롬프트를 추가했다.
- README 라우팅·Roadmap 상태·1-B0 문서의 다음 세션 링크를 최신 §9로 연결했다. 기존 선정/결정·코드 구현·카메라/렌더 값은 인계 정리 중 변경하지 않았다.
- 패키지는 HEAD c6981a6 + 기록한 소스 변경으로 빌드됐다. 인계 커밋으로 HEAD가 달라져도 빌드 대상 파일/패키지 해시가 일치하면 기존 바이너리를 사용한다. 소스/패키지·로그는 Saved에 남기고 git에는 소스·JSON·문서만 넣는다.
- 최근 커밋의 `feat:`/`fix:`/`docs:` + 한국어 제목·본문 bullet 형식을 확인했다. 커밋 범위는 Ridge 하네스/경로·검사·스테이징 참조와 위 문서로 제한한다. 다음 세션은 §9의 짧은 Medium/적 0 패키지 검증부터 시작한다.
- 커밋 전 문서 상대 링크 73개·새 패키지 실행 예제 2개의 PowerShell 구문·빌드 소스/보존 에셋 13파일 해시·스테이징 경로 해시·git diff 검사를 통과했다. 인계 정리 중 런타임 실행이나 재빌드는 하지 않았다.

### 추가 — 기존 패키지 해시 재검증·짧은 Medium 패키지 검증 완료(2026-11-14)

- HEAD `aaf6267`·깨끗한 작업 트리에서 시작했다. PackageVerification.json·HandoffCommit.json·SourceSnapshot.json을 대조해 소스/보존 에셋 13파일과 패키지 핵심 7파일(실행 파일·경로 JSON·컨테이너 5개)의 SHA256·기록 크기가 일치했다. 아카이브 전체 49파일·1,455,482,688바이트도 기록과 같다. 49개 전부에 대한 개별 해시 목록이 있다는 뜻은 아니다. 재빌드하지 않았다. 근거: `Saved/Profiling/Graphics/Phase01/PackageValidation/Preflight.json`.
- 샌드박스 첫 실행 `Medium_20261114_074729_973d6d`는 Runtime.log/Run.json 생성 전 종료해 런처 코드 1이었다. 게임 종료 코드는 확보하지 못했다. 같은 인자의 권한 확장 재시도는 정상 실행됐으나 첫 실패 원인을 샌드박스로 확정하지 않는다. 실패 폴더의 AttemptFailure.json과 Launch.json을 보존했다.
- 사용자 화면 확인 준비 승인을 받은 뒤 §9의 명시적 패키지/출력 경로·워밍업 10초·캡처 60초로 실행했다. 성공 회차는 `PackageValidation/Medium/Medium_20261114_074758_eb262d/Medium_6256DA5B4E3BD37CC16CFBB0A38C5689`다(공통 루트 `Saved/Profiling/Graphics/Phase01/`). 종료 코드 0·complete·with_editor=false·CSV 3065프레임/개체 분할·초기/마지막 적 2002·Pod 120을 확인했고 AllowPreview 집계를 통과했다. performance_eligible=false로 성능 기준선에서 제외한다.
- manifest의 정확 2560×1440·TSR=4·ScreenPercentage=100·Epic 그룹 11개=3·시드 1·PCG offset 0·Medium·실제 Ridge slot 1, 소스/스테이징/실행 경로·실행 파일·컨테이너 해시 일치를 확인했다. 사용자가 A/B/C와 연결 구간의 구도·관통에 문제없음을 확인하고 짧은 적 0 실행을 승인했다. 성공 회차 부모의 RuntimeValidation.json에 기록했다.
- 런타임 경고는 VSM pages=4096의 ProjectSetting 우선순위 유지, GameplayCueNotifyPaths 미지정의 /Game 폴백, NS_BurnLoop 누락/적 화상 VFX 비활성화다. Error/Fatal은 관측되지 않았다. 이번 관찰 경로는 정상 종료했지만 화상 VFX 검증으로 해석하지 않는다. 설정/콘텐츠는 바꾸지 않았다.
- 다음은 같은 패키지·Medium 공간 계획에 `-LNPGraphicsNoEnemies`만 추가한 짧은 검증과 Pod 계획/초기화 대조다. 사용자 승인 뒤 시작했으며 정식 기준선·환경 반입은 시작하지 않았다.

### 추가 — 짧은 적 0 정상 종료·Medium과 계획/Pod 수량 대조 완료(2026-11-14)

- 사용자 승인 뒤 같은 패키지·Medium·워밍업 10초·캡처 60초에 `-LNPGraphicsNoEnemies`만 추가했다. 회차는 `Saved/Profiling/Graphics/Phase01/PackageValidation/NoEnemies/Medium_20261114_075024_b41126/Medium_5EE712854276290E35BB0593D184EBD3`다. 종료 코드 0·complete·with_editor=false·3155프레임으로 AllowPreview 집계가 통과했다.
- no_enemies=true·performance_eligible=false·초기/마지막/캡처 프레임 Alive/Active/FrustumCandidates 모두 0, Population.csv 헤더만·initial_spawns 적 행 없음을 확인했다. 정확 2560×1440·렌더 CVar 전체·시드/PCG offset/Medium·밀도 1·옥탄트 배정·실행 파일/컨테이너 5개/경로 해시가 Medium과 같다.
- set별 공간 계획 로그와 미사용 generic anchor 11개가 정확히 일치했다. 계획의 적 수는 아래처럼 유지되지만 적 0의 실제 생성 수는 0이다.

| set | Pod 요청/계획 배치(두 회차 동일) | generic | authored/random/shortfall | 적 요청/계획 배치(두 회차 동일) |
|:---|---:|---:|:---|---:|
| Bench01 | 20/20 | 20 | 0/0/0 | 317/317 |
| Bench01_2 | 40/40 | 40 | 0/0/0 | 819/819 |
| Bench01_3 | 30/30 | 30 | 0/0/0 | 430/430 |
| Bench01_4 | 30/30 | 30 | 0/0/0 | 436/436 |

- Pod의 config/slot별 post_plan_requested·initialized·rejected 행 8개가 정확히 일치했다. slot 0~7은 각각 26·12·12·22·7·8·21·12개, 거부 0·총 120개다. 소스상 BuildPlan 완료/계획 로그 뒤 AssembleSpawnQueueFromAsyncResult에서 적 큐만 생략한다. **개별 Pod 위치/회전/스케일의 런타임 덤프·해시가 없어 모든 Transform의 직접 일치를 증명하지 않았다.** 수량 비교와 소스 흐름 근거를 직접 배치 비교로 확대하지 않는다.
- 입력 manifest/계획 로그 해시·16개 교차 검사 결과·배치 검증 한계를 `PackageValidation/Comparison.json`에 보존했다. Postflight.json에서 소스/불변 에셋 13파일·패키지 핵심 7파일 해시 재일치와 게임 프로세스 0개를 확인했다. 두 성공 회차의 런타임 경고는 같은 종류이며 Error/Fatal은 관측되지 않았다.
- 짧은 검증 단위를 마감하고 실행서 §10·Current를 갱신했다. 정식 성능 수치로 사용하지 않으며 **1-A 전체는 미완료**다. 다음은 사용자 진행 확인과 전원/성능 모드 확인 뒤 정식 180초 워밍업·60초 캡처·회차별 냉각을 적용한 Medium 3회+적 0 1회다. 아직 정식 기준선·환경 반입·PCG·재질 변경·녹화·추가 빌드/쿡/Live Coding은 하지 않았다.

### 추가 — 정식 출발 측정 3+1·Medium 재현성 통과·센서 보완 승인 대기(2026-11-14)

- 사용자가 정식 측정 진행·전원 어댑터 연결·Windows Best Performance를 확인했고 진행 중 외부 모니터 사용도 확인했다. 제조사 도구 모드와 실제 외부 패널 연결 GPU는 미확인으로 남겼다. CIM은 AMD 780M 1440p/165Hz·RTX 4060 1440p/59Hz를 보고했으며 RHI는 네 회차 모두 RTX 4060 Laptop을 사용했다. 전원 계획 Balanced와 사용자 확인 Windows 전원 모드를 구분했다.
- 최초 `StartBaseline_20261114` 세션은 작성한 실행 스크립트가 빈 ExtraArguments를 명시적으로 넘겨 기존 런처에서 null 오류가 났다. Launch.json/게임 실행/성능 표본 생성 전 실패다. 원래 폴더의 Session.json·AttemptFailure.json을 보존하고, 일반 Medium에서는 추가 인자를 아예 넘기지 않도록 고친 별도 `StartBaseline_20261114_Retry1` 세션에서 실제 측정을 수행했다. 기존 런처·게임 소스·패키지는 변경하지 않았다.
- 워밍업 180초·캡처 60초·명시적 기존 패키지/새 출력 경로·회차별 냉각으로 Medium 3회와 적 0 1회를 완료했다. 네 회차 모두 종료 코드 0·complete·with_editor=false·정확 1440p·TSR100%·Epic·시드 1/PCG offset 0·Ridge slot 1·CSV 검증 통과다. Medium은 performance_eligible=true의 일반 집계, 적 0은 false/AllowPreview 진단으로 분리했다. 냉각 조건과 2회차 이후 최소 300초를 모두 충족했고 1200초 상한 예외는 없었다.
- 실제 Medium 회차/프레임은 `Medium_20261114_081638_b7ca9c`/3097, `Medium_20261114_082546_a14098`/3048, `Medium_20261114_083454_6efb36`/3038이다. 모두 초기/마지막 적 2002·Pod 120이다. 지표별 3회 중앙값 Frame/GT/RT/GPU=19.687/5.022/19.681/19.223ms, 반복 편차 1.88/3.59/1.89/1.91%로 5% 기준을 통과했다. Frame은 예산 118.60%다. 각 회차를 보존했고 예비 실행/과거 월드와 합치지 않았다.
- 적 0은 `NoEnemies/Medium_20261114_084402_b2b09f`, 3074프레임·적 수 0·Pod 120·Population 헤더만·initial_spawns 적 행 없음을 통과했다. 모든 회차의 set별 계획 로그 4개/미사용 generic 11개·Pod slot별 요청/초기화/거부·Medium 초기 편성·실효 렌더 CVar·실행 파일/컨테이너 5개/경로 및 Viewpoints 사본 해시가 같다. 개별 Pod Transform 직접 비교가 아니라는 범위를 유지한다.
- 소스/보존 에셋 13파일·패키지 핵심 7파일 해시가 종료 뒤 다시 일치했다. 네 회차 런타임 Error/Fatal 0, VSM pages 우선순위·GameplayCue 경로 폴백·NS_BurnLoop 누락/화상 VFX 비활성화 경고는 유지됐다. 게임/해당 센서 프로세스는 종료했다. 새 빌드·쿡·Live Coding·환경 반입·PCG·재질 변경·녹화는 하지 않았다.
- **센서 기록 제한 발견:** Medium 캡처 센서는 59/60/60표본·양끝 공백 1초 미만이지만 마지막 적 0 센서는 38표본·후반 22.210초 공백과 마지막 불완전 행이 남았다. 엔진 CSV 3074프레임/GPU 시간과 별도 제한이다. 파일 버퍼링/프로세스 종료 손실 가능성은 가설로 남긴다. 센서 평균은 확보된 부분으로만 보고한다.
- 보완안은 `NoEnemyTelemetryRetry_20261114/RunSession.ps1`과 DrainGpuTelemetry.ps1이다. 직전 실행 후 300초 경과·연속 안정 60초를 확인하고 적 0 한 회차만 수행한 뒤, 게임 종료 이후의 완전한 센서 행이 디스크에 저장된 것을 확인하고 센서 프로세스를 종료한다. 센서 전용 probe는 대기 34.397초 후 목표 이후 행 저장 확인을 통과했다(TelemetryDrainProbe.json). **추가 게임 실행은 아직 하지 않았고 사용자 승인 대기다.**
- 상세 표/식별/한계는 `analysis/Phase01_StartBaselineReport.md`, 원자료/18개 교차 검사/입력 해시는 `StartBaseline_20261114_Retry1/RepeatedSummary.json`·ValidationStatus.json·Session.json·각 회차 파일에 남겼다. Current·실행서 §11·README/Roadmap·Measurement의 센서 후반 저장 확인 절차를 갱신했다. 1-A/Phase 1 마감으로 표시하지 않는다.

### 추가 — 적 0 센서 보완 회차·최종 교차 검증 통과(2026-11-14)

- 사용자가 계속 진행을 승인했다. 준비된 NoEnemyTelemetryRetry_20261114/RunSession.ps1을 읽고 원본 패키지/소스 해시·중복 프로세스 없음·온도 약 43°C/전력 약 4W를 확인했다. 권한 확장으로 기존 패키지·명시적 보완 출력 경로·Medium 계획 적 0 한 회차를 실행했다. 빌드·쿡·Live Coding은 하지 않았다.
- 실제 회차는 `NoEnemyTelemetryRetry_20261114/NoEnemies/Medium_20261114_093743_7c4929/Medium_EE2E7E2D40FF719CCA9C3A8E227AAD0E`다(공통 상위 `Saved/Profiling/Graphics/Phase01/`). 직전 게임 종료 후 2975.649초가 지났고 추가 60.56초 연속 안정 조건을 확인했다. 시작 43.00°C/4.04W·180초 워밍업/60초 캡처·출력 2560×1440·TSR100%·Epic·시드 1/PCG offset 0·Ridge slot 1을 유지했다.
- 종료 코드 0·complete·with_editor=false·performance_eligible=false·CSV 3133프레임/AllowPreview 검증 통과. 초기/마지막/측정 프레임 적 수 0·Population.csv 헤더만·initial_spawns 적 행 없음·Pod 120을 확인했다. 진단 평균 Frame/GT/RT/GPU=19.157/2.947/19.151/18.666ms이며 단일 진단 값으로만 보고한다.
- 캡처는 18:40:47.683~18:41:47.683 KST, 센서 60표본·시작/끝 공백 0.461/0.166초·최대 표본 간격 1.016초다. 후반 저장 확인 단계에서 33.413초를 기다려 게임 종료 이후 완전한 행을 디스크에서 확인한 뒤 센서 프로세스를 종료했다. 최초 적 0의 후반 22.210초 누락은 보완 실행으로 해소했으며 최초 자료도 보존했다.
- FinalValidation 폴더에 원래 Medium 3회와 보완 진단을 가리키는 파생 분석 Session.json·두 센서 원본을 연결한 파생 CSV를 만들었다. 원본 경로를 명시했으며 파일을 옮기거나 덮어쓰지 않았다. 기존 적 0과 보완 적 0은 합쳐 평균내지 않는다. 보완 진단과 기존 Medium 사이의 18개 교차 검사를 통과했다: 동일 실행 파일/컨테이너 5개/경로 및 Viewpoints 사본, 실효 렌더 조건·어댑터·옥탄트 배정·계획 수량·Pod 초기화 행·Medium 초기 편성·적 0 자료·냉각/센서 존재·소스/패키지 해시. 개별 Pod Transform 직접 검증은 여전히 하지 않았다.
- 기존 Medium의 Frame/GT/RT/GPU 중앙값 19.687/5.022/19.681/19.223ms·편차 1.88/3.59/1.89/1.91%는 변하지 않는다. 보완 회차 런타임 Error/Fatal 0이며 기존 경고 종류도 유지됐다. 게임/해당 센서 프로세스 종료와 해시 보존을 확인한다.
- 보고서·Current·실행서 §12/체크리스트·README/Roadmap을 보완 완료 상태로 갱신했다. **1-A 측정·검증 조건은 충족했고 사용자 마감/다음 단위 진행 확인 대기다.** 환경 반입·PCG·재질 변경·파라곤 선정·녹화는 하지 않았다. Phase 1 전체 완료로 표시하지 않는다.

### 1-A 마감 — 사용자 완료 확인·문서 정합·커밋(2026-11-14)

- 사용자가 “1-A 마감하고 문서 반영 다한 다음에 커밋해줘”라고 요청했다. 출발 기준선/센서 보완과 기록한 검증 범위를 수용해 1-A를 완료 상태로 전환했다. Phase 1 전체는 진행 중이며 이번 요청은 1-B 실행 승인이 아니다.
- Current·README/Roadmap·출발 기준선 보고서·Phase 1 실행서 §13과 체크리스트·1-B0 선정 문서의 다음 세션 인계를 완료 상태로 맞췄다. 중간 대기/실패 상태는 시점별 기록으로 보존하고 현재 행동과 구분했다. Measurement에는 앞서 검증한 센서 후반 저장 확인 절차를 반영했다.
- Frame 19.687ms/예산 118.60%와 재현성 통과, 적 0 보완 3133프레임/센서 60표본을 마감 근거로 유지했다. 개별 Pod Transform 직접 비교·제조사 도구 모드·외부 패널 연결 GPU의 미확인과 런타임 경고를 삭제하지 않았다.
- 최종 Saved의 ValidationStatus/FinalChecks에 사용자 마감을 반영하고 Closure.json·아티팩트 해시를 기록한다. 원래 측정 파일과 센서 부분 진단을 덮어쓰거나 재집계하지 않는다. 소스/패키지 파일은 동일하므로 재빌드하지 않는다.
- 최근 커밋의 `접두어: 한국어 제목`과 본문 bullet 형식을 확인했다. 이번 변경은 그래픽스 문서 8개이며 `docs:`로 커밋한다. Saved의 원자료/실행용 스크립트와 라이선스 에셋은 커밋하지 않는다.
- 다음 단위는 사용자 착수 확인 뒤 1-B 환경 반입 준비다. 선정 재검토·환경 반입·PCG·재질 변경·파라곤 선정·녹화·새 빌드/쿡/Live Coding은 하지 않았다.
- 커밋 전 문서 8개·상대 링크 93개·소스/보존 에셋 13파일·패키지 핵심 7파일 해시·변경 범위와 git diff --check를 통과했다. 원자료는 Saved에 보존하고 문서만 커밋한다.

### 1-B1 — 환경 반입 준비 완료·다음 단위 확인 대기(2026-11-14)

- 사용자가 Phase 1-B 진행과 구현 단위별 진행 확인·수동 조작 우선을 요청했다. 필수 문서·1-A 마감/출발 기준선·1-B0 §8을 읽고 기존 구역/톤/6종 조건부 팔레트를 유지했다. 첫 단위는 반입 준비로 한정했다.
- 기존 P4 자료와 실제 원본을 읽기 전용으로 검사했다. 104파일·1,785,971,111 bytes가 일치했고 타깃 동일 경로 파일은 0개다. 현재 SHA256 104개를 기록했으며 과거 해시가 있던 14개는 모두 일치했다. 나머지 90개는 과거 내용 일치까지 검증한 것이 아니다.
- `Scripts/Profiling/Graphics/ElectricDreamsPalette.json`에 14시작점·104패키지의 클래스/크기/해시·외부 참조를 추적 가능한 목록으로 남겼다. 원본 에셋 데이터는 포함하지 않았다.
- `.gitignore`에 실제 원본 경로와 타깃 파생본 폴더를 추가했다. 원본 경로 유지 Migrate를 택했으며 폴더 이동·참조 치환·원본 편집은 하지 않는다. 자동 검사에서 원본 104경로·파생/WIP 대표 3경로가 제외되고 원본 추적 파일 0개·자체 제작/식별 목록 비제외를 확인했다. 첫 검사는 PowerShell의 CRLF 파이프 입력 때문에 단일 파일 제외 규칙 4개를 놓쳤다. 인자 배열로 전달해 재검사했으며 제외 규칙을 넓혀 우회하지 않았다.
- `Phase01_EnvironmentImport.md`에 Migrate 절차와 재질→소수 수동 배치→PCG→전체 확장, Decoration 계약·보호 영역·시험/마감 기준을 작성했다. 원본 지형 재질 파생본을 우선하고 Cliff는 부분 보강만 시험한다. 정식 Ridge와 Support source를 보존한다.
- VT 65/66이며 원본 DefaultEngine.ini는 `r.VirtualTextures=True`다. 타깃 프로젝트/사용자 Engine.ini에 명시 값이 없어 다음 단위에서 실효값·로드/컴파일을 확인한다. 전역 설정 변경이 필요하면 별도 단위와 같은 조건의 대조 측정을 검토하도록 남겼다. 현재 설정은 바꾸지 않았다.
- Codex의 도구 목록에 Unreal MCP가 없고 프로젝트 `.mcp.json`에는 8000/8001/8002가 있다. CIM 프로세스 조회는 접근 거부, 일반 Get-Process는 결과가 없었으며 라이브 에디터 상태를 확인했다고 쓰지 않았다. 다음 실제 조작 전 연결 복구 또는 사용자 수동 확인이 필요하다.
- 검증 근거: `Saved/Profiling/Graphics/Phase01/EnvironmentPreparation_20261114/Preflight.json`·`Validation.json`. 1-A Build SourceSnapshot의 13파일 해시 보존도 확인했다. 소스/렌더 설정/기존 에셋은 바꾸지 않았으므로 빌드·쿡·PIE·패키지 재측정은 하지 않았다.
- Current·README 라우팅·Phase 1 실행서/체크리스트·1-B0 인계를 갱신했다. **1-B1 완료, 1-B 전체는 미완료다.** 다음은 사용자에게 1-B2 진행을 확인하고 더 빠른 수동 GUI Migrate를 요청하는 것이다. 실제 반입·파생 재질·PCG·시험 배치는 아직 하지 않았다.

### 1-B2 착수 — 라이브 사전 확인·수동 Migrate 요청(2026-11-14)

- 사용자가 Electric Dreams(8001)와 LootNPop(8000) 에디터 준비 및 진행을 승인했다. 양쪽 MCP가 응답했고 UE 5.8.4·PIE=false, 현재 맵은 원본 Startup/타깃 TestMap03이다.
- 원본 14시작점의 전이 의존성을 다시 조회해 104패키지 목록이 식별 JSON과 같음을 확인했다. 원본 맵/104패키지 dirty=false, 디스크 해시 104개 일치, 타깃 동일 경로 파일 0개다.
- 타깃 TestMap03·정식 Ridge LVI/SurfaceData·풀·기본 SpawnConfig가 dirty=false다. Ridge 구성/기본 데이터/설정/카메라 12파일의 반입 전 해시를 기록했다. 양쪽을 저장하거나 원본 에셋을 수정하지 않았다.
- 타깃 r.VirtualTextures=1을 실효 조회했다. VT 활성화 설정 변경/재시작 없이 반입 후 로드·샘플러 검증을 할 수 있다. 현재 값만으로 실제 반입 재질의 컴파일 성공을 주장하지 않는다.
- 원본 Content Browser에 14시작점을 선택하고 재조회했다. 수동 GUI가 더 효율적이므로 사용자에게 Asset Actions→Migrate, 의존성 유지, 정확한 LootNPop/Content 목적지를 안내했다. 충돌/목록 차이는 복사 전에 멈추도록 요청했다. 아직 반입 완료 답변이나 타깃 파일을 검증하지 않았으며 1-B2는 진행 중이다.
- 근거는 Saved/Profiling/Graphics/Phase01/EnvironmentImport_20261114/BeforeMigrate.json·ProtectedFilesBefore.json이다. 다음은 사용자 완료 답변 뒤 파일/의존성/로드 검증이며 1-B3는 시작하지 않는다.

### 1-B2 완료 — 104패키지 반입/로드·미리보기와 보존 검증(2026-11-14)

- 사용자가 Migrate 완료를 알렸다. 기대한 104파일이 모두 존재하고 추가/누락은 0개, git 제외 104·추적 원본 0개다. 원본 104파일과 반입 전 보호 12파일의 해시가 유지됐다.
- 반입본은 104파일 모두 원본과 해시/크기가 달라졌다. 엔진 AssetTools.cpp의 SaveInstancedPackagesIntoDestination(1063행)·GEditor->Save(1107행)와 실제 Migrate 성공 로그를 대조해 대상 재직렬화 경로임을 확인했다. 원본 해시 목록을 타깃 해시로 덮어쓰지 않고 MigratedFiles.json에 별도로 기록했다.
- 타깃 104패키지 로드·클래스가 통과했고 /Game 전이 closure는 104개, 누락 0이다. 재저장 뒤 일부 직접 의존성에 전이 참조가 추가되고 텍스처의 Interchange 스크립트 참조 일부가 사라졌다. 기존 /Game 의존성 삭제와 팔레트 밖 새 /Game 패키지는 없다.
- 선정 6메시를 양쪽 라이브 로드 뒤 비교했다. bounds·LOD0 삼각형/정점·슬롯/MI·Nanite/fallback·DF/RT·complexCollisionMesh 설정이 같다. LOD0 삼각형은 Cliff 9424, Sandstone 7166, MossyForest 7414, DryRiverBed 27846, Clover 1716, Fern 306이다. 6개 모두 Nanite=true·슬롯 1개다. 원본 최적화는 하지 않았다.
- 텍스처 66개·VT 65개와 주요 Format/Dimensions/SRGB/CompressionSettings/VT/SourceFormat 태그가 기존 기록과 같다. 저장 형식 SourceCompression은 PNG→UE Delta로 바뀌었다. 이를 픽셀 데이터 바이트 동일성 검증이라고 쓰지 않았다.
- Migrate 후 원본 get_asset_class가 레지스트리 엔트리 부재로 실패했다. 소스 태그 비교는 읽었으나 현재 로드/레지스트리 상태가 달라 저장 전 태그와 그대로 비교하지 않았다. is_dirty 104개 true는 실제 dirty뿐 아니라 레지스트리상 디스크 엔트리 부재도 true로 반환하는 도구 구현의 영향을 받는다. 실제 source memory dirty는 분리 확인하지 못했다. 파일 해시는 유지됐고 원본을 저장/복원하지 않았다.
- 첫 MI 썸네일은 미리보기 셰이더 요청 직후 기본 재질이었고 재조회에서 사암 텍스처가 표시됐다. 이후 두 에디터의 고용량 에셋 로드 상태에서 VRAM 부족 경고가 발생해 추가 캡처를 중단하고 사용자에게 원본만 저장 없이 종료를 요청했다. 사용자가 종료 완료를 확인했다. GPU 메모리는 7652/8188MiB→종료 직후5567/8188MiB→최종5666/8188MiB였다. 에디터 메모리 관측이며 패키지 성능 판정은 아니다.
- LootNPop만으로 메시 MI 6개·지형 MI/부모의 미리보기 총8개를 다시 확인했고 VRAM 경고 없이 텍스처/Masked 식생이 표시됐다. 첫 메모리 부족 이미지는 Previews/, 최종은 PreviewsSingleEditor/로 나눴다. API가 에셋/재질 컴파일 완료를 기다리는 구현도 확인했다. 타깃 104패키지와 보호 대상 5패키지의 dirty 0, 재질/셰이더/텍스처/패키지 로드 오류 로그 0, r.VirtualTextures=1이다.
- 최종 원본 104파일·보호 12파일 해시 재일치, 반입본 104파일의 로드/미리보기 전후 해시 불변을 통과했다. 근거는 Saved/Profiling/Graphics/Phase01/EnvironmentImport_20261114/의 AfterMigrateFiles.json·MigratedFiles.json·AssetVerification.json·FinalFileChecks.json·FinalLiveChecks.json·Completion.json·PreviewsSingleEditor/다. 큰 원자료 저장 시 셸 실행 인자 길이 및 Node 64000-byte 제한에 걸린 시도가 있어, 필요한 비교값으로 줄인 보고서와 작은 이미지 데이터 청크를 파일로 기록했다. 이 실패를 에셋 검증 실패로 집계하지 않았다.
- Current·반입 실행서 §7·상위 실행서/체크리스트·Roadmap·1-B0 인계를 갱신했다. **1-B2 완료, 다음은 사용자 진행 확인 뒤 1-B3 지각 재질 시험**이다. 파생 재질·WIP 배치·정식 Ridge 적용·PCG·베이크·빌드/쿡·패키지 성능/구면 품질 검증은 하지 않았다.

### 1-B3a 진행 — 좌표/마스크 진단 구현·두 실패와 복구 대기(2026-11-14)

- 사용자 1-B3 진행 승인으로 텍스처 투영 전 좌표·현지 Up 경사·대표 구역 마스크 진단을 첫 단위로 나눴다. BuildRidgeSurfaceProbe.py에 좌표 함수·Unlit 진단 재질·Scope/Slope/Grid MI를 작성했다. float32 역투영 48표본 최대 XY 오차 0.001953125cm·8변 convex/반시계·Python 구문 검사를 통과했다. GPU/회전 검증은 아니다.
- 첫 사용자 실행은 HLSL 예약어 line 때문에 컴파일 실패하고 scalar setter 반환값 assert에서 멈췄다. gridLine으로 수정한 라이브 재질 recompile은 성공했다. UE 5.8 MaterialEditingLibrary.cpp:1485~1493에서 scalar setter가 실제 변경 후에도 false를 반환함을 확인해 getter 재조회로 바꿨고 ProbeMode=0을 확인했다. 첫 미완성 3에셋만 정리하고 부재를 재조회했다. 보호 12파일 해시는 같았다.
- 두 번째 사용자 실행은 수정된 진단 5에셋 저장 뒤 새 World duplicate_asset→load_level에서 에디터가 World Memory Leaks:2로 종료됐다. 로그의 참조 체인은 WIP standalone World의 GC keep flags와 패키지 잔류를 가리킨다. Python strong reference가 직접 원인이라고까지 확인한 것은 아니다. WIP umap 파일은 없고 진단 에셋 5파일은 저장됐다. 보호 12파일은 모두 그대로다.
- 크래시 로그/컨텍스트를 Saved/Profiling/Graphics/Phase01/RidgeSurfaceProbe_20261114/Crash_20261114/에 보존하고 Recovery.json을 남겼다. 원본 Saved/Crashes 디렉터리도 보존한다.
- 빌더에서 World 복제·맵 전환·적용을 제거하고 에셋 생성만 남겼다. 기존 스크립트 재실행이나 같은 자동 전환을 요청하지 않는다. 사용자에게 LootNPop 재시작, 정식 Ridge LVI를 열고 Save Current Level As로 WIP를 저장해 달라고 요청했다. 이는 아직 완료 답변 전이다.
- 1-B3a/1-B3는 미완료다. 사용자 수동 WIP 저장 뒤 현재 맵 경로 확인→기존 지각 컴포넌트만 MI 적용→컴파일/구역/경사/격자 표시와 정식 자산 보존 검증이 남았다. 텍스처/노멀 원본 그래프 파생은 후속 단위다.

### 1-B3a 완료 — 수동 Save As 복구·GPU 좌표/마스크 진단(2026-11-14)

- 사용자가 LootNPop을 재시작하고 정식 Ridge LVI를 Save Current Level As로 WIP에 저장했다. MCP 읽기로 정확한 WIP 경로·PIE=false·dirty=false를 확인했다. 보호 12파일 해시도 그대로였다. 자동 World 복제/맵 로드는 재시도하지 않았다.
- WIP의 StaticMeshActor 3개 루트 컴포넌트에서 실제 StaticMesh를 대조해 지각 1개를 식별했다. 변경 전 actor transform·메시·overrideMaterials·componentTags·상대 transform·BodyInstance를 WipBefore.json에 저장한 뒤 지각에만 Slope MI를 적용하고 WIP를 저장했다.
- A에서 분지 바닥 초록·절벽 빨강·중간 띠 노랑, Grid의 1000cm 접평면 격자를 확인했다. C에서는 Scope 구역 안 초록·밖 회색이 보였다. 다른 암반 Floor/Shell의 재질 바인딩은 M_Ridge_Rock을 유지했다. 진단 Emissive의 주변 조명 영향과 실제 재질 바인딩 변경을 구분한다.
- 세 메시 액터와 카메라를 월드 Z +90도로 함께 회전해 Grid/Slope 각각을 캡처했다. 격자/구역 경계·경사 분류가 같은 표면 위치에 남는 것을 육안 확인했다. 8개 런타임 slot·픽셀 동일성 검증은 아니다. try/finally로 원래 transform·A 카메라를 복원하고 Slope로 저장했다.
- 최종 WipAfter.json 비교에서 actor transforms는 모두 원래 값이고 component 차이는 지각 overrideMaterials 1곳뿐이다. 다른 StaticMesh·componentTags·상대 transform·BodyInstance/profile/response는 같다. WIP dirty=false, 재시작 후 셰이더/World 오류 로그 0이다.
- 현재 스크립트의 METRIC_CODE/DISPLAY_CODE와 저장된 Custom 노드 코드가 일치했다. MI ProbeMode=0/1/2를 재조회했고 Emissive 연결·WPO/Displacement 미연결을 확인했다. 보호 12파일·반입 원본 104파일의 최종 해시도 유지됐다.
- 근거는 Saved/Profiling/Graphics/Phase01/RidgeSurfaceProbe_20261114/의 Completion.json·WipBefore/After.json·FinalFileChecks.json·ExpectedCode.json·Views PNG 5개다. 최종 WIP SHA256=ED10DD7C46E3C9951B05B0A9855DE8F2C7EF66797D10F117233AB02F9B95CBA6. 첫 CaptureViewport 호출은 optional 입력 기본값 문제로 거부돼 실제 transform과 비활성 annotation 값을 명시한 호출로 캡처했다. 에셋/코드를 바꾸는 우회는 하지 않았다.
- 빌더는 진단 에셋만 만들며 레벨 복제/전환을 하지 않는다. 재현은 에셋 생성→사용자 Save As→WIP 지각 Element0에 MI 지정 순이다. 현재 5에셋이 있어 빌더 재실행을 요청하지 않는다. 코드는 저장소에, 에셋/WIP/원자료는 git 제외 경로에 남겼다.
- **1-B3a 완료, 1-B3/1-B/Phase 1 전체는 미완료다.** 접평면 Grid가 급경사에서 늘어나므로 최종 절벽 UV로 채택하지 않는다. 다음은 사용자 진행 확인 뒤 1-B3b 원본 지형 그래프 파생본의 텍스처 투영·노멀 처리 시험이다. 암반 외부/지하 마스크·근경/경계 전이·적도/전체 slot·이음매·PIE/SupportData·패키지 비용 검증은 남아 있다.

### 1-B3b 원본 파생·투영/노멀 기술 검증, 반복감 미통과(2026-11-14)

- 사용자 승인으로 BuildRidgeSurfaceMaterial.py와 원본 지형 부모/MI 파생본 2개를 만들고 WIP 지각에만 적용했다. 엔진 WorldAlignedTexture/WorldAlignedNormals_HighQuality에 명시 로컬 좌표/법선을 공급하며 색/거칠기 조절은 원본을 재사용했다. BeachCliff 2000cm·MossyRockyGround 400cm, 경사 30~70도는 첫 시험값이다.
- 초기 World→Tangent 출력의 중립 노멀 검사는 29,417표본·RGB8 p95=7/max=109로 실패했다. 월드 출력·기하 월드 노멀 블렌드로 수정한 재검사는 평균0.4330·p95=1/max=2로 통과했다. 표시 RGB 차이이며 각도 오차/전체 프레임 동일성은 아니다. 실제 노멀 텍스처를 복원했다. 실패 자료와 초기 Creation 기록은 보존했다.
- 사용자가 Unlit/WorldNormal/Lit 전환을 수행했다. WIP에 에디터 전용 검사용 점광원 1개를 추가하고 A/B 및 한 +90도 회전 사례를 캡처했다. 3메시·광원 transform은 복원했다. B의 큰 삼각형 음영은 기존 단색 재질에서도 보여 기존 지각 후속 항목으로 남겼다. 게임 조명·전체 런타임 slot·PIE·패키지 성능 검증은 아니다.
- 최종 저장 부모/MI·WIP dirty=false·관련 셰이더/World 오류 필터 빈 목록, 보호 12파일·반입 104파일 해시 보존을 확인했다. 기존 3메시 컴포넌트 차이는 지각 overrideMaterials뿐이다. 추가 광원은 별도다.
- 사용자 근경 판정: 늘어짐이나 눈에 띄는 어색함은 없으나 누가 봐도 타일이라고 느낄 정도로 규칙적이고 기계적인 반복이 매우 명확하다. **투영/노멀 기술 검증 통과·반복감 미통과.** 최종 재질/팔레트 채택이나 1-B3/Phase 1 완료로 처리하지 않는다.
- 다음 1-B3c는 무늬 배치/방향의 반복 완화 후보와 노멀 정합·이음새·추가 샘플 비용을 확인하는 단위다. 아직 착수하지 않았으며 사용자 진행 확인을 받는다. 구현/재현 상세는 환경 반입 실행서 §10, 근거는 Saved/Profiling/Graphics/Phase01/RidgeSurfaceMaterial_20261114/의 Verification.json·WipBefore/After.json·FinalFileChecks.json·NeutralNormalWorldComparison.json·Views PNG 10개다.

### 1-B3c 착수 — 엔진 Texture_Bombing 재사용 시험(2026-11-14)

- 사용자가 반복 완화 단위 진행을 승인했다. 기존 엔진 함수를 조사해 위치/방향 변형과 Is Normalmap 보정 입력을 확인했다. 원래 구면 투영/월드 노멀 프레임을 유지하며 각 투영의 샘플을 Texture_Bombing으로 바꾸는 별도 V002 스크립트를 작성했다. 높이 블렌드를 꺼 색/거칠기/노멀의 가중치를 맞춘다. 함수/부모/MI 4개만 생성하며 맵 조작은 없다.
- PIE=false·정확한 WIP 경로·V001 부모/MI와 WIP dirty=false·지각 V001 적용을 확인했다. 사용자 근경·A/B의 기존 Lit 3개와 Preflight.json을 보존했다. 보호 12·반입 104파일·V001 부모/MI·WIP 해시가 직전 단계와 같다.
- 첫 수동 실행은 함수 복제 직후 Material 전용 노드 조회에 MaterialFunction을 전달해 TypeError가 났다. MaterialTools.get_expressions로 수정하고 미완성 MF_RidgeBombAlignedTexture 1개만 삭제·부재 재조회했다. Python 구문 검사는 통과했다. 사용자에게 수정본 실행을 요청했다.
- 아직 최종 생성·컴파일·WIP 적용/노멀·반복감·비용 통과가 아니다. 원자료는 Saved/Profiling/Graphics/Phase01/RidgeSurfaceAntitile_20261114/의 BeforeImages.json·Views/Before_*.png·Preflight.json·FirstRunError.json이다. 세부 구현/인계는 환경 반입 실행서 §11을 따른다.

### 1-B3c 초기 V002 적용·바닥 보완, 중간 검증 대기(2026-11-14)

- 수정본 생성 성공·MCP 재컴파일 통과·함수 2개 각 bombing 호출3개·색/마스크4/노멀2 교체·Height Lerp=false·노멀 경로 Is Normalmap=true를 확인했다. WIP 지각에만 적용·저장하고 기존과 같은 근경/A/B 3개를 캡처했다.
- 보완 전 MI 통계는 V001→V002 픽셀 명령1565→5270·샘플러2→7·픽셀 샘플21→75·VT 조회18→72, 정점 명령148/샘플0이다. 에디터 플랫폼의 셰이더 추정이며 패키지 GPU 시간으로 확대하지 않는다. 높은 비용으로 성능 적합 채택은 보류했다.
- 사용자 첫 판정은 절벽 수용 가능, 바닥은 여전히 규칙적 타일 반복이다. 바닥에만 로컬 위치 기반 두 크기의 부드러운 값 노이즈로 색/거칠기 변화를 추가했다. Ground Macro Size1800cm/Strength0.5, 보조 크기 약48.6m다. 추가 텍스처 조회 없이 UV/노멀을 유지했고 컴파일/저장·Custom 코드 일치를 확인했다. 현재 빌더에도 같은 보완을 반영했다.
- MCP Custom 입력 배열 변경이 거부돼 기본 구조 전체를 유지한 크기 확장→이름 변경으로 적용했다. 잘못된 배열은 저장하지 않았다. 처음에는 매크로 노드만 추가됐고 나머지 4노드는 수정 뒤 연결했다.
- 사용자의 바닥 재판정/World Normal 전환을 기다린다. 중립 노멀 비교·실제 노멀 복원 확인·보완 후 통계·최종 보존 확인이 남아 있다. 아직 1-B3c/1-B3 완료가 아니다. Creation.json은 바닥 보완 전 통계이며 최종 통계와 구분한다. 상세는 환경 반입 실행서 §11, 코드 검증은 MacroVerification.json·캡처는 Macro_User.png다.

### 1-B3c 기술 검증 마감 — 잔여 반복/비용 조건부 후보(2026-11-14)

- 사용자 보완 후 판정은 바닥과 벽에 잔여 반복이 있으며 프랍/식생 포함 장면에서는 덜 느껴질 수 있다는 의견이다. 최종 화질 승인으로 해석하지 않는다. V002를 후속 소수 배치의 조건부 시각 후보로 유지한다. 동선/Pod/훅 공간을 모두 덮는 계획은 없다.
- TSR 켜짐의 엄격한 내부 마스크 비교는29,043표본·p95=4/max98, V001 임시 사본은p95=1/max2였다. 변경 없는 중립 재캡처끼리 max109·기하 노멀끼리 max98도 관측돼 AA/캡처 영향을 분리했다. 초기 넓은 마스크·무효 PowerShell 읽기·TSR 켜짐 자료는 구분 보존했다.
- 사용자가 AA4→0을 수행했고 실효값0을 확인했다. 같은 내부29,043표본 AA 끄기 비교는 평균0.3801·p95=1·최대2, 절벽13904/바닥10168/전이4971 각각도p95=1로 통과했다. 표시RGB 차이이며 각도/전체프레임 동일성은 아니다. 원래 허용오차를 넓히지 않았다. 실제 노멀을 복원·컴파일하고 V001 임시 사본2개를 삭제/부재 확인했다.
- 사용자 AA4·Lit 복구와 CaptureStats.py 읽기 실행 뒤 실효값4·Lit 캡처를 확인했다. 콘솔 변수의 원래 우선순위 복원은 미확인, INI는 보존됐다. 최종 MI 통계는 V001→V002 픽셀명령1565→5367·샘플러2→7·픽셀샘플21→75·VT조회18→72, 정점명령148/샘플0이다. 바닥 보완은 명령97만 늘고 조회 수는 같다. GPU ms/패키지 성능 판정은 아니다.
- 3메시+검사용 점광원+카메라 월드Z +90도 Lit 한 사례에서 무늬 정합을 확인하고 transform을 finally로 복원/재조회했다. GI/노출 이력의 밝기 차이를 픽셀 불변으로 해석하지 않는다. 전체 런타임 slot은 남아 있다.
- 파생4에셋·WIP 저장/dirty=false, 기존3메시 차이는 지각 overrideMaterials V001→V002뿐이다. 보호12·반입104·V001 부모/MI 해시가 유지됐고 V0024파일 git 제외를 확인했다. Bomb Offset1·Ground Macro Size1800/Strength0.5·editor-only 광원·실제 두 노멀 텍스처 복원을 재조회했다. 관련 shader compile/World Memory 오류 필터는 빈 목록이다.
- 재생성 코드는 BuildRidgeSurfaceAntitile.py에 반영했다. 저장된 매크로 코드 일치·Python 구문 검사와 라이브 셰이더 컴파일을 통과했으며, 최종 보완 후 깨끗한 경로에서 전체 재생성을 다시 하지는 않았다. 기존 산출물이 있어 재실행하지 않는다.
- 근거는 Saved/Profiling/Graphics/Phase01/RidgeSurfaceAntitile_20261114/의 Verification.json·FinalFileChecks.json·WipBefore/After.json·ComponentComparison.json·FinalStatistics.json·NeutralNormalNoAAComparison.json·MacroVerification.json·Views PNG19개다. 상세는 환경 반입 실행서 §12가 소유한다.
- **1-B3c 구현/기술 검증 마감, 최종 재질/성능·1-B3/1-B/Phase1 미완료.** 다음은 진행 확인 뒤 1-B4a 낮은 사암·하상·클로버 소수 배치 시험을 제안한다. 접지/노출·보호 영역·장면 속 반복감을 확인하고 대표 구역 PCG/전체 확장 전 샘플 비용 축소/최종 화질 판정을 한다. 아직 배치를 시작하지 않았다.

### 1-B4a 착수·소수 배치 기술 확인, 근경 판정 대기(2026-11-15)

- 사용자 진행 승인 뒤 TestMap03·PIE=false를 확인했다. 기존 WIP 열기를 수동 요청해 완료했고, dirty=false·V002 지각 바인딩·보호12/반입104 해시 보존을 확인했다. 새 World 복제/자동 로드는 하지 않았다.
- BuildRidgeGroundProbe.py에 사암2·하상2·클로버6의 개별 시험점을 작성했다. 경로 초안+전체 bounds·구역/이음매/암반 footprint·Pod3D(사암)·착지/입구·실제 경사를 생성 전에 검사한다. 실제 complex trace hit normal에 현지 Z를 정렬하며 지각/원본 에셋은 수정하지 않는다.
- 첫 실행은 spec/실제 지각 높이 오차30cm 가드에서 사암2점(실제62.82/39.98cm)이 멈춰 액터0개였다. 가드를 실제 hit component == 기존 지각으로 바꿨고 수정본 생성은 성공했다. 원래 높이장 근사치를 실제 지각 확인에 쓰지 않는다. 첫 계획과 로그를 보존했다.
- 추가10개 모두 Decoration 단독·LNPDecoration/NoCollision·Nav영향=false, 원래3메시 스냅샷 보존·WIP 저장/dirty=false를 확인했다. 경로 여유 초안+bound 바깥 최소112.60cm·사암 Pod3D 구+bound 바깥169.27cm·최대 실제경사16.36도다. 2m/계수1.2는 시험 가드이며 최종 PCG 규약이 아니다.
- 새10개 모두 CastShadow/VisibleInRayTracing/AffectDistanceFieldLighting=true를 상속했다. 원본 메시/MI·Nanite/DF/충돌을 수정/최적화하지 않았다. 관련 shader/WorldMemory/VRAM 오류 필터는 생성 성공 로그 외 빈 목록이다. TSR실효4를 재조회했다.
- 배치 전 BeforePlacement.umap은 PreflightFiles.json의 WIP해시와 같다. 자료는 Saved/Profiling/Graphics/Phase01/RidgeGroundProbe_20261115/의 Plan.json·Placement.json·TechnicalVerification.json·BackupVerification.json·ExecutionLog.json·Views/Before_A.png다.
- 자동 FocusOnActors가 멀어 접지를 판정하지 않았다. 사용자에게 선택10개 F/직접 근접 관찰로 사암 뜸/매몰·하상 테두리/색·클로버 부유/판 모양을 확인해 달라고 요청했다. 현재 답변 대기이며 1-B4a/최종 팔레트 완료가 아니다. 전체 훅 시야/접근, PIE 발/카메라/탄환, 런타임 SurfaceData·패키지 성능·전체밀도 반복감은 미검증이다.

### 1-B4a 근경 시험 마감 — 사암/클로버 통과·Riverbed 제외(2026-11-15)

- 사용자가 전체적으로 들뜨거나 이질감이 없으며 근경에서는 지각 반복도 거슬리지 않는다고 확인했다. 역할을 구분한 뒤 Sandstone은 매우 자연스럽고, Riverbed는 표면 자체는 자연스러우나 너무 네모라 주변과 어울리지 않는다고 명확히 판정했다.
- 실패 역할을 자동 대체/원본 수정하지 않고 WIP의 직접 생성한 Riverbed_02/03 시험 액터2개만 제거·저장했다. 원본 하상 메시/MI와 반입104패키지/최초 식별 기록은 보존했다. 최종은 사암2/클로버6이다. 재생성 코드 POINTS도 고정ID를 유지한8개로 맞췄으며 기존 배치 위 재실행은 하지 않았다.
- 최종 라이브8개 모두 Static·Decoration 단독·NoCollision/LNPDecoration·내비 영향 꺼짐을 확인했다. 원래3메시의 transform과 기존 스냅샷 필드 차이0, 보호12·반입104·V002재질4파일 해시 변화0이다. WIP dirty=false를 확인했다. 배치 전 사본도 당시 해시와 같다.
- 초기10개 Plan/Placement·WipAfter_Initial10.json·Initial10Images.json을 보존하고 최종8개는 WipAfter.json·Verification.json·FinalFileChecks.json에 기록했다. 초기/최종 같은A·사용자 근경 등 PNG6개가 Views에 있다. 재생성 코드 Python AST는 통과했고, 역할 제외 후 전체 재생성은 중복 방지 때문에 다시 하지 않았다.
- **1-B4a는 배치·기술 확인·사용자 근경 시험 범위에서 마감.** 전체 밀도/원거리 반복, PIE 발/카메라/탄환, 전체 훅 접근/회복, WIP 런타임 SurfaceData, 패키지 비용까지 통과한 최종 팔레트는 아니다. 다음은 진행 확인 뒤 이끼 바위와 절벽 부분 보강 소수 시험을 제안한다. Fern 바람/변위와 재질 샘플 비용 축소도 남아 있다.
- 현재 팔레트는 Phase01_RidgeZonePalette.md §10, 실행/제외/재현 근거는 Phase01_EnvironmentImport.md §13을 따른다. 소수 시험 맵/원본/원자료는 git 제외이며 커밋은 하지 않았다.

### 새 세션 인수인계·단일 커밋 정리(2026-11-15~16)

- 사용자 요청으로 Current.md를 현재 Phase1-B 상태·로컬 WIP/재생성 순서·다음1-B4b·미검증/재발 방지 중심으로 줄였다. 완료된1-A 상세는 기존 보고서/실행서를 참조한다. 다음 단위는 아직 승인/착수하지 않았고 이번 요청은 문서/커밋 마감이다.
- README 라우팅·상위 실행서의 현재 상태 표·환경 반입 실행서의 반입 전/최종 상태 표현을 정합했다. 재반입/기존 빌더 재실행 금지, World복제→로드 크래시, 실제 지각 hit 검증, AA 진단/복원, 배치/런타임/성능의 검증 경계를 인계했다.
- ElectricDreamsPalette.json의 원본104개·파생11개·WIP맵1개·Saved 예시가 모두 Git 제외이고 이미 추적된 에셋0개임을 확인했다(Handoff_20261115/RepositoryAudit.json). 실패한 하상 원본도 로컬 반입 이력으로 보존하며 Git에는 넣지 않는다.
- 커밋 범위는 문서·재생성 Python4개·식별JSON·.gitignore의 명시 목록만이다. 이전 커밋의 feat/docs/fix 접두어+한글 제목·항목형 한글 본문 양식을 확인했다. 이번 작업은 구현 코드가 포함돼 feat 접두어로 단일 커밋한다. 실제 해시는 git log에서 확인한다.
- Python AST·식별 목록/파일/해시 형식·로컬 문서 링크·Git diff와 스테이징 에셋 제외를 검사한다. UE 생성/노멀/근경 검증은 앞선 해당 단위 결과를 사용하며, 세션 마감 때문에 빌드/쿡/재생성을 반복하지 않는다.

### 1-B4b 착수 — 이끼 바위·절벽 부분 보강 계획 및 경사 가드 중단(2026-11-15)

- 사용자 요청으로1-B4b를 승인받았다. TestMap03·PIE=false에서 기존 WIP 열기를 즉시 수동 요청했고 사용자가 완료했다. WIP 경로·dirty=false·직전 최종맵 해시 E53D5F53… 일치를 확인했다.
- BuildRidgeRockProbe.py를 작성했다. 이끼 MI09·Cliff MI를 기본 슬롯에서 확인하고 원본/기존11메시를 보존한다. 이끼2개·축소0.15 절벽1개만 개별 배치하며 원본 에셋/충돌/Support/Nav·정식 LVI는 수정하지 않는다. 보호12/반입104/V0024파일의 시작 해시120개를 기록했다.
- 첫 수동 실행은 이끼2점의 실제 경사33.34°/48.56° 때문에 생성 전 가드에서 중단됐다. 절벽은 실제79.96°·AABB 앞면9점의 지각 간격5.40~12.00cm로 시험 가드를 통과했다. 실제 메시 노출·접촉의 통과 근거는 아니다. 첫 Plan을 별도 보존했다.
- 이끼 경사30° 기준을 유지하고 인근 후보8점 trace·경로/Pod 여유 조회 후 통과2점을 선택하는 Saved/SurveyAndBuild.py 실행을 수동 요청했다. 아직 성공·근경 판정 대기다. 성공 시 고정 좌표를 추적되는 빌더에 반영해 Saved 보조 의존성을 없앤다.
- 현재 Python AST를 통과했다. 실행·재현·검증 범위는 환경 반입 실행서 §14, 원자료는 Saved/Profiling/Graphics/Phase01/RidgeRockProbe_20261115/다. 이후 단위는 아직 승인받지 않았다.

### 1-B4b 배치·기술 확인 — 이끼2/절벽1 근경 판정 대기(2026-11-15)

- 사용자 보조 스크립트 실행으로 실제 후보8점 조회 후 이끼(2650,-2750)/(2850,-2760)·절벽(0,0) 배치/저장에 성공했다. 이끼 경사는12.82°/16.24°, 경로 초안/bounds 밖 최소50.80cm·Pod3D 구/bounds 밖 최소220.59cm다. 최초 경사 기준은 유지했다.
- 최종 좌표를 BuildRidgeRockProbe.py POINTS에 반영하고 AST를 통과했다. 로컬 Saved 보조 없이 기존1-B4a WIP에서 고정 좌표로 재생성할 수 있는 코드이며, 이미3개가 있어 최종 코드 전체를 다시 실행하지 않았다.
- 라이브14컴포넌트의 저장 직후 기록 대조 차이0·신규3개 Decoration 단독/NoCollision/LNPDecoration/Static/Nav영향false를 재조회했다. 신규3개 그림자/RT/DF=true 기본값은 유지한다. 기존11메시 스냅샷·보호12/반입104/V0024파일 해시 보존, 복구 사본/시작 WIP 해시 일치, WIP dirty=false·성공 로그를 확인했다.
- 사용자에게 개별 액터 선택/F로 이끼 뜸·사암 겹침·색/이끼, 절벽 실제 노출·매몰·네모 테두리/옆면 틈을 확인해 달라고 요청했다. 근경 판정 대기이며1-B4b/팔레트 최종 채택을 마감하지 않았다. PIE/런타임 SurfaceData/패키지 비용도 미검증이다.
- 근거: RidgeRockProbe_20261115/MossSurvey.json·Plan.json·Placement.json·WipBefore/After.json·TechnicalVerification.json·FinalFileChecks.json·BeforePlacement.umap. 상세는 환경 반입 실행서 §14다.

### 1-B4b 근경 시험 마감 — 이끼2/축소 절벽1 유지(2026-11-15)

- 사용자는 전체적으로 자연스럽다고 확인했다. Moss_00은 기존 사암과 겹치지만 이끼 난 부분이 연결돼 자연스럽고 지면에 떠 있다는 느낌이 없으며, Cliff_02도 아주 자연스럽다는 판정이다. 이끼2/절벽1을 현재 위치에 유지하며 겹침을 없애는 이동은 하지 않았다.
- **1-B4b의 소수 배치·기술 확인·사용자 근경 시험을 마감했다.** 종료 시 기존 WIP 경로·PIE=false·dirty=false를 재확인했다. WIP 파일도 배치 직후와 같은 해시이며 보호12/반입104/V0024파일 총120개 해시 변화0이다. 추가 에셋/맵 저장은 필요하지 않았다.
- 최종 판정/미검증 범위는 RidgeRockProbe_20261115/Verification.json, 관찰 중 이끼/기존 장식 구도는 Views/User_Current.png다. 이 캡처가 절벽 개별 관찰의 근거를 대신하지는 않는다. 앞선 TechnicalVerification은 근경 판정 전 체크포인트로 보존했다.
- 실제 통과 범위는 WIP 검사용 조명의 소수 근경이며 전체 밀도/원거리·PIE 발/벽보행/카메라/탄환·SurfaceData 런타임·패키지 비용은 미검증이다. 절벽 벽 전체 외장이나 최종 팔레트/Phase1 완료로 확대하지 않는다.
- Current·현재 팔레트·상위 실행서·환경 반입 실행서 §14를 마감 상태로 정합했다. 다음 제안은1-B4c Fern 구면 바람/WPO·변위 소수 시험이다. 사용자에게 진행 여부를 확인하며 아직 착수하지 않는다. 재질 샘플 비용 축소와 최종 화질 게이트도 그대로 남긴다.

### 1-B4c 착수 — 원본 Fern 바람 경로·실제 표면 조회(2026-11-15)

- 사용자 승인으로1-B4c에 착수했다. 기존 WIP·PIE=false·dirty=false·보호/반입/V002120파일 해시 보존을 확인했다. 원본 바람 함수/실효 MI 파라미터를 읽고 원본은 편집하지 않았다.
- 원본 MI는 Animate Foliage/Plant/Use Vertex RED=true, Noise/Gust/Gust Noise0.2/0.4/0.3이다. 일반 렌더 WPO는 MF_SimpleWind→SimpleGrassWind이며 고정Z와(0,0,-10) 피벗 보정이 있다. RayTraced 경로는 별도 상수다. 원본 한 표본의 자연스러움을 전체 구면/slot 정합으로 판정하지 않는다.
- 수동 InspectFern.py는 실제 지각 trace·보호 가드·라이브 MPC 조회만 하며 성공했다. 후보20점 중(3000,-2700)1점만 전부 통과했다. 실제 경사16.47°이며 바람은 Direction0/Noise5/StrengthPlants2/SpeedPlants0.3/TilingPlants0.5다. survey 보호 반경은 메시+30cm 시험 여유로서 최대 WPO 측정값은 아니다.
- BuildRidgeFernProbe.py에 그1점·원본 메시/MI·Decoration/NoCollision/Static/Nav영향false·WPO평가true 시험을 작성했다. 현재14메시/dirty/지각pose/보호 가드 확인→복구 사본→1개 생성→WIP만 저장한다. Python AST를 통과했다. 사용자에게 실행과 Realtime 근경 움직임/뿌리/부유/판 모양 관찰을 요청했으며 아직 결과 대기다.
- 상세는 환경 반입 실행서 §15, 원자료는 Saved/Profiling/Graphics/Phase01/RidgeFernProbe_20261115/다. 이 단위 이후 진행은 별도 확인한다.

### 1-B4c 원본 근경 동작 시험 마감 — Fern1개 유지·구면 방향 채택 보류(2026-11-15)

- 사용자 빌더 실행으로 원본 Fern1개를 생성·WIP만 저장했다. 실제 경사16.47°·시험 바람30cm 포함 반경196.04cm·경로 초안/bounds 밖267.53cm·Pod3D 구/bounds 밖21.06cm로 가드를 통과했다. 최대 WPO 변위를 측정한 수치가 아니다.
- 라이브 신규1개는 Decoration 단독/NoCollision/LNPDecoration/Static/Nav영향false·WPO평가true/거리제한0이다. 원본 MI override 없음, 그림자/RT/DF=true·WPO RT평가false·boundsScale1 기본값을 기록했다. 원본 메시/MI/MPC/공유함수와 기존14메시를 보존했다. 저장 직후15컴포넌트 필드 대조 차이0·보호120파일 해시 변화0·시작 WIP와 복구 사본 해시 일치를 확인했다.
- 사용자는 Realtime 근경에서 잎이 자연스럽게 움직이고 다른 어색한 부분도 없다고 확인했다. 현재1개를 WPO 켜진 채 유지한다. 문제가 관찰되지 않아 WPO 끄기 비교나 원본/바람 값 수정은 하지 않았다. 실제 정점별 최대 변위·뿌리 이동량을 수치 측정하지 않았다.
- **1-B4c의 원본 소수 배치·기술 조사·근경 동작 시험을 마감했다. 구면 방향 최종 채택은 보류다.** 원본 SimpleGrassWind의 고정 월드축/피벗 보정이 남아 있으므로 임의 slot/적도 정합을 통과로 처리하지 않는다. 전체 밀도/게임 조명/PIE/SurfaceData/패키지 성능도 별도 미검증이다.
- 종료 시 WIP dirty=false·PIE=false·배치 직후 WIP 해시 유지와 보호120파일 보존을 재확인했다. 근거는 RidgeFernProbe_20261115/TechnicalVerification.json·FinalFileChecks.json·Verification.json·Views/Final_User.png다. 정지 캡처가 사용자 동작 관찰의 근거를 대신하지 않는다.
- 재생성 코드는 Scripts/Profiling/Graphics/BuildRidgeFernProbe.py다. 조사 Saved에 의존하지 않는 고정 좌표이며 이미 생성돼 재실행하지 않는다. Current·현재 팔레트·상위 실행서·환경 반입 실행서 §15를 정합했다. 다음1-B4d Fern 바람 구면 방향 보정/회전 비교는 제안만 했으며 사용자 진행 확인 전 착수하지 않는다.

### 1-B4d 마감 — Fern 구면 바람 파생본·현재 근경/+90° 사례 통과·복원(2026-11-15)

- 사용자 승인으로 BuildRidgeFernWind.py와 파생 함수/부모/MI3개를 만들었다. 원본 MF_SimpleWind의 기하 위치/피벗/Tree 출력만 구면 접선 프레임으로 옮겼고 원본 노이즈/MI 파라미터/정점 R·공유 함수/MPC·레이 트레이싱 경로는 보존했다. 원본71노드에3개를 더한74노드·최종 Custom 코드/배선을 재조회했다.
- 최초 생성 때 SimpleGrassWind 입력 index0 미연결의 MI 컴파일 경고1건이 있었다. 저장 성공을 바로 통과로 처리하지 않고 부모 재컴파일·Plant=true 재설정/배선 확인 후, VerifyAndApply.py에서 실효 MI Update/GetStatistics와 원본 override 배열/정적 스위치 일치를 확인했다. 이후 같은 경고는 재발하지 않았다. 빌더에는 부모 컴파일→MI 갱신→통계 양수 확인 순서를 반영했다. 기존 산출물이 있어 수정 후 깨끗한 전체 재생성은 반복하지 않았다.
- MI 통계는 원본→보정 정점475→497·픽셀686→686·샘플러4·정점/픽셀 샘플4/4·VT조회4로 나머지는 같다. 현재 에디터 플랫폼 통계이며 GPU ms/패키지 비용으로 판정하지 않는다.
- 기존 Fern1개의 Element0 override만 보정 MI로 적용·저장했다. 나머지14메시와 자신의 transform/태그/충돌 등은 보존했다. 사용자는 적용 후 여전히 자연스럽다고 확인했다.
- 표시 메시15개/검사용 광원1개/카메라를 월드 Z +90°로 함께 임시 회전해 Realtime 관찰을 요청했고, 사용자는 여전히 아주 자연스럽다고 확인했다. 회전 상태의 맵 미저장을 디스크 해시로 확인했다. 기록한 원래16 transform/camera로 즉시 복원해 최대 위치 오차0cm·회전5.68e-14°·scale0·카메라0·재질/태그/충돌/렌더 필드 차이0을 확인하고 WIP를 저장했다. Preview 최종상태는 restored_saved다.
- 파생3에셋/WIP dirty=false·PIE=false·원본104/보호12/V0024파일 해시 변화0·원본 MI 상태 복구 사본/시작 WIP 해시 일치를 확인했다. 회전 전 보정 MI 복구 사본은 별도로 보존한다. 캡처3개/카메라 메타데이터는 Views에 있다.
- **1-B4d 구현·컴파일/배선·현재 근경 및 한 회전 사례·복원 범위 마감.** 모든 런타임 slot/적도/극점/기준축 전환·최대 변위/뿌리 이동량·전체 밀도/게임 조명·PIE/SurfaceData·패키지 성능은 미검증이다. PCG/정식 Ridge/원본 에셋은 수정하지 않았다.
- 재현/범위는 환경 반입 실행서 §16, 최종 근거는 Saved/Profiling/Graphics/Phase01/RidgeFernWind_20261115/Bindings.json·FinalStatistics.json·Application.json·Preview.json·Restoration.json·FinalFileChecks.json·Verification.json·Views다. Current/팔레트/상위 실행서/README를 정합했다. 다음1-B3d 지각 재질 샘플 비용 축소는 제안만 했고 진행 확인 전 착수하지 않는다.

### 1-B3d 착수 — V003 두 샘플 반복 완화 후보 준비(2026-11-15)

- 사용자 승인으로 지각 재질 샘플 비용 축소에 착수했다. 기존 WIP·PIE=false·dirty=false를 확인했다. 보호12/반입104/기존 파생14파일 총130개 해시와 보정 Fern을 포함한 현재 V002 맵 복구 사본을 남겼다.
- V002의 축당4샘플 Texture_Bombing을2샘플 UV 이동/부드러운 혼합으로 줄이는 BuildRidgeSurfaceReduced.py를 작성했다. V002 부모/MI·원래3축 투영 함수2개의 파생본 총4개만 생성하며 맵·원본·이전후보·장식은 변경하지 않는다.
- 엔진 TextureVariation/RotateUV/RotateNormals를 조회했고 높이/경계 샘플과 분기가 있는 원형 대신 이번 최소2샘플 경로를 택했다. UV 회전/반전·높이 비교·추가 노이즈 텍스처 없이 색/마스크/노멀 공통가중치·노멀 정규화·원래 UV 명시적 DDX/DDY를 쓴다. 기존 좌표/경사/구역/바닥 macro와 Bomb Offset 강도는 유지한다.
- Python AST/언어 검사를 통과했다. 사용자에게 생성 콘솔 실행을 요청했으며 아직 실행 결과 대기다. 실효 MI의 VT 조회/픽셀 명령이 줄었을 때만 새 에셋을 저장한다. 같은 구도 화질·중립 노멀 검증 전에는 채택/단위 완료로 처리하지 않는다.
- 상세는 환경 반입 실행서 §17, 원자료는 Saved/Profiling/Graphics/Phase01/RidgeSurfaceReduced_20261115/다. 캡처 JSON의 base64 본문은 다시 출력하지 않고 카메라 메타데이터만 읽는다. 다음 단위는 승인받지 않았다.

### 1-B3d 마감 — V003 바닥 실패·V004 혼합 비용 축소/중립 노멀 통과(2026-11-15)

- V003 두 샘플 UV 이동 혼합은 생성/컴파일/배선 검사를 통과했다. V002 대비 VT72→36·픽셀5367→2972로 줄었으나 사용자가 바닥 원경을 거의 체크무늬에 가깝다고 판정했다. 절벽은 괜찮다는 결과였으며 전체 V003 적용은 미채택했다. UV 혼합 노이즈와 기존 macro 변조의 원인은 분리하지 않아 확정하지 않았다.
- BuildRidgeSurfaceHybrid.py로 V004 부모/MI만 파생했다. 바닥 색/마스크/노멀3경로는 V002의4샘플 Texture_Bombing으로 복원하고 절벽3경로는 V003 두 샘플을 유지한다. 바닥의 기존 macro 색/거칠기와 MI 값은 보존했다. 사용자는 절벽은 여전히 괜찮고 바닥은 V003보다 확실히 개선됐다고 확인했다. 바닥 최종 화질 승인으로 확대하지 않았다.
- V004 실효 에디터 통계는 정점148·픽셀4170·샘플러5·픽셀 샘플57·VT54다. V003보다 비용이 늘지만 V002 대비 VT25%·픽셀 명령22.3%를 줄인 절충안이라고 설명했다. 원래 절반 감소 목표는 바닥 실패로 채택하지 않았다. GPU ms/패키지 성능은 측정하지 않았다.
- 사용자 수동 AA0/World Normal 전환 후 같은 구도의 기하 노멀과 V004 중립 노멀을 비교했다. stride4/구역 내부 마스크22,779표본에서 전체/바닥/절벽/전이 모두 p95=1·최대2(RGB8)로 기준 p95≤2를 통과했다. 표시 RGB 차이이며 법선 각도 오차나 실제 노멀맵 전체 정합의 측정은 아니다. 첫 회색 경사 마스크는 무효로 보존하고 독립 호출로 재캡처한 정상 마스크만 사용했다.
- 실제 노멀 텍스처2개·V004 바인딩·사용자 카메라를 복원하고 부모/MI/WIP를 저장했다. 사용자가 AA4/Lit 복귀를 완료했고 실효4·최종 Lit 캡처를 확인했다. NormalDiagnosticState의 최종상태는 restored다. 진단 후 기존15 actor transform/컴포넌트 차이0·MI 파라미터 값 보존·dirty=false·PIE=false를 확인했다. 원래 CVar 우선순위까지 검증한 것은 아니다.
- 보호12/반입104/기존 파생14/V0034파일 총134개의 해시 변화0이며 단위 시작 V002 복구 사본/시작 WIP 해시도 일치한다. V0034/V0042에셋은 Git 제외로 보존한다. V003는 V004 절벽 함수와 재생성의 선행 조건이므로 삭제하지 않는다. 두 신규 빌더는 AST를 통과했으며 기존 산출물 때문에 깨끗한 전체 재생성은 반복하지 않았다.
- **1-B3d 구현·비용 축소/중립 노멀·보존/복원 기술 검증 마감, V004 조건부 유지.** 바닥 원경 최종 화질·모든 slot/적도/이음매·게임 조명/밀도·PIE/SurfaceData·패키지 성능은 미완료다. 다음 제안1-B3e는 같은 구도의 macro 유무 비교로 바닥 반복 원인을 분리하는 단위이며 아직 승인/착수하지 않았다.
- 상세/재현은 환경 반입 실행서 §17이다. Saved/Profiling/Graphics/Phase01/RidgeSurfaceReduced_20261115/의 HybridCreation·V003VisualFailure·HybridApplication·NeutralNormalComparison·NormalDiagnosticState·FinalLiveVerification·WipAfter·FinalFileChecks·Verification과 Views14PNG에 근거를 남겼다. 복구 사본은 단위 시작 V002와 실패 V003를 구분한다. Current·상위 실행서·팔레트 요약을 정합했다.

### 새 세션 인수인계·단일 커밋 정리(2026-11-15~16)

- 사용자 요청으로 이번 세션의 1-B4b/c/d·1-B3d 결과와 재생성 코드5개를 한 커밋으로 정리한다. 최근 커밋의 `feat: 한국어 제목`과 본문 글머리표 양식을 따른다. 새1-B3e는 승인/착수하지 않고 다음 세션에서 진행 여부부터 확인한다.
- Current에 읽을 문서·읽기 전용 에디터 점검·V004/보정 Fern·복구 기록·기존 빌더 재실행 금지·새 세션 요청 예시를 남겼다. 로드맵·상위 실행서·팔레트 요약·README 라우팅을 현재 결과와 정합했다. Phase1/최종 팔레트/패키지 성능은 미완료 상태를 유지한다.
- 커밋 대상은 관련 문서와 Python 재생성 코드뿐이다. 원본/파생 에셋·WIP·Saved·캡처·복구 사본은 로컬 Git 제외로 유지하며, 이번 인계의 구문/공백·문서 링크·포함/제외 검사는 Saved/Profiling/Graphics/Phase01/Handoff_20261115/RepositoryAudit_RockFernSurface.json에 남긴다. 기존 산출물의 빌더 재실행·전체 빌드/쿡·새 에디터 변경은 하지 않는다.

- 2026-11-16 한도 초기화 후 인계/커밋 작업만 재개했다. 에디터 마지막 확인은11월15일 결과이며 새 구현이나 에디터 변경은 하지 않았다.

### 1-B3e~g 마감 — 바닥 원인 분리·V005 삼각3샘플·소수 회전(2026-11-16)

- 사용자 승인으로 V003/V004의 macro0.5/0을 A/B 동일 구도8장 비교했다. 기존 MI를 보존한 진단 MI2개만 만들었다. V003의 규칙적인 바닥 무늬는 macro를 꺼도 남아 추가 macro만의 문제로 볼 수 없고 UV 혼합 경로를 우선 조사할 근거가 됐다. 정확한 UV 하위 원인/원본 텍스처/삼각형 음영의 기여까지 분리한 것은 아니다.
- `BuildRidgeSurfaceTriangle.py`와 V005 함수2/부모/MI를 만들었다. 바닥에 삼각 격자3개의 이동 UV·공통4제곱 무게중심 가중치를 사용하고 노멀을 정규화했다. 각 함수의 신규9샘플에 원래 UV 미분을 연결했고 절벽 V0032샘플·구면 프레임·경사/구역·macro·MI 파라미터는 유지했다. 정식 히스토그램 보존 기법 전체를 구현한 것은 아니다.
- V004→V005 실효 MI 추정은 VT54→45·픽셀4170→3401·샘플러5→2·정점148 동일이다. 사용자 원경 개선/근경 자연스러움 확인과 AA0 중립 노멀22,731표본·전체/각 경사 p95=1·최대2(RGB8)를 통과했다. 실제 노멀2개·V005·검사 전 카메라·AA4/Lit를 복원했다. 에디터 통계는 GPU ms가 아니다. Python AST, CPU 참조25,921개 가중치/297경계쌍 검사와 실제 셰이더 컴파일/배선 재조회를 확인했다.
- V005 적용 WIP 저장은 자동 승인 검토에서 시각 확인만으로 영속화 승인이 명시되지 않았다는 사유로 거부됐다. 우회 저장하지 않았고 사용자가 WIP/V005 부모를 직접 저장한 뒤 dirty=false·실제 노멀·바인딩을 재조회해 해소했다.
- slot5(Pitch180/Yaw90)와 적도 임의 회전(Roll54.337285)에서 사용자 재질/Fern 확인을 받았다. 메시15/검사용 광원1/카메라를 복원 대조·저장했고 최대 수치 차이는7.11e-14였다. 보호142파일 해시는 그대로다.
- 경계 Lit/Scope 쌍으로 같은 지각의 구역 밖과 별도 보라색 암반을 구분했다. 사용자는 시험 범위의 끊김은 당연하며 완화 의미가 없다고 지적했다. 임시 시험 마스크를 최종 아트 전이와 섞은 미관 질문을 철회하고 이진 경계를 유지했다. 경계 완화는 구현하거나 승인받지 않았다. 진단도 V005로 복원·저장했다.
- 상세/재현은 환경 실행서 §18~20, 근거는 `RidgeSurfaceRepetition_20261116/`·`RidgeSurfaceTriangle_20261116/`·`RidgeSurfaceOrientation_20261116/`다. 소수 회전을 전체 slot/극점/런타임 접촉/패키지 통과로 확대하지 않는다.

### 1-B4e/4e-1 마감 — WIP 전용 데이터·검사용 풀·1인 PIE 접촉·복원(2026-11-16)

- 최초 연결 점검에서 정식6개 정의에 그래픽스 WIP가 없고 SurfaceData42개 중 WIP SourceLevel 일치0개였다. WIP GameMode=None·프로젝트 기본 Engine.GameMode였으며, 원본 Ridge 데이터를 연결하는 방식은 런타임 Level/SourceLevel 쌍 검증에 맞지 않았다. manifest 검사를 우회하지 않고 WIP 전용 베이크/풀 선행 단위를 별도 승인받았다.
- 사용자가 정상 루트 TestMap03에서 기존 `BakeOctant` 명령을 실행했다. 버전7·WIP SourceLevel1개, Support2소스/44층·전체593,257샘플·Nav73,836셀·Spawn authored12/candidates9,373/noNavRejected0을 확인했다. `DA_OctantSurface_LVI_Ridge_SurfaceProbe`는5,217,284B다. 형상/기존 데이터/정식 풀은 변경하지 않았다.
- 정식 풀에서 `DA_GraphicsRidgeValidationPool`을 파생해 Ridge 정의 하나의 Level/SurfaceData만 바꾸고 새 풀만 저장했다. LNPSettings의 풀을 메모리에서 임시 선택하고 원래 선택을 먼저 기록했다. SaveConfig를 호출하지 않았고 INI/정식 풀 해시도 보존했다.
- 첫1인 PIE는 WIP slot5/7·166 bindings, 사용자가 EnemyDensity0 적용을 위해 재시작한 다음 PIE는 slot2/6·195 bindings였다. 둘 다8 validated slots의 snapshot 게시와 seam delta0.000cm·blockedSeamNodes/Edges0을 확인했다. 매 실행의 slot을 다시 읽어 두 번째 이동 명령을 slot2로 안내했다.
- 사용자는 두 번째 PIE에서 보행·착지·카메라·사격 모두 문제없다고 확인했다. WhereAmI는 WIP slot2·local X3000/Y-2699/H-3821·실제 Ridge 지각을 확인했다. 장식12개의 NoCollision/내비 영향 꺼짐을 유지한 검토다.2인 호스트/게스트·적 전투/부하·전체 slot의 미관·패키지 성능은 미검증이다.
- native CaptureViewport는 별도 PIE 게임창 대신 검은 에디터 화면을 잡아 무효로 분리했다. 사용자 `Shot`의 실제 게임 화면을 확인·복사하고 원본도 보존했다. 사용자 PIE 종료 뒤 임시 풀 선택·원래 WIP/카메라를 복원했다. 사용자 EnemyDensity0은 유지했고15메시 변화0·보호142+추가3파일 변화0·관련 dirty=false다.
- 상세/재현은 환경 실행서 §21~22, 근거는 `RidgeRuntimeProbe_20261116/`·`RidgeRuntimeValidation_20261116/`다. `SelectionState.json=restored`, 실제 게임 화면은 `Views/pie_game_shot.png`다. 새 데이터/풀은 모두 Git 제외이며 재생성 절차만 추적한다.

### 1-B5a 새 세션 인수인계·단일 커밋 정리(2026-11-16)

- 사용자가1-B5a를 새 세션에 넘기고 관련 문서/코드를 한 번에 커밋하도록 요청했다. **PCG는 착수하지 않았다.** 기존 대표 구역/팔레트·수동12개·V005/보정 Fern을 유지하고 전체 bounds 보호를 포함한 고정 시드 소수 배치부터 시작한다. 새 실행서는 착수 시 구체화한다.
- Current는 완료 이력 대신 현재 자산/복원 상태·새 세션 절차·근거 표 중심으로 정리했다. README·로드맵·상위 실행서·팔레트·환경 실행서·이 로그를 정합했다. 기존 빌더 재실행 금지·동적 slot 재조회·원래 풀 복원·EnemyDensity0 유지·무효 캡처 구분·임시 경계 보존과 미검증 범위를 넘긴다.
- 최근 커밋의 `feat: 한국어 제목`과 본문 글머리표 양식을 따른다. 커밋은 재생성 Python1개와 관련 문서만 포함한다. 원본/파생/WIP/SurfaceData/풀·Saved·캡처·복구 사본은 제외한다. 마지막 읽기 전용 에디터/보존 검사와 소스 구문·문서 링크·포함/제외 검사는 `Saved/Profiling/Graphics/Phase01/Handoff_20261116/RepositoryAudit.json`에 남긴다. 새 전체 빌드/쿡·베이크·PCG·추가 에디터 변경은 하지 않는다.

### 1-B5a 마감 — 구면 구역 PCG 노드·대표 구역 소수 배치·재베이크·1인 PIE(2026-11-16)

- 사용자가 구현 방식으로 C++ PCG 노드를 골랐다. 기존 테마 샘플러(옥탄트 전체·보호 없음)와 엔진 Z-Up 샘플러를 쓰지 않고 `LNPDecorationRuleData`·`LNPDecorationSampler`를 추가했다. 게임 타깃 컴파일 뒤 사용자가 에디터를 닫고 에디터 빌드를 두 번 했다(첫 빌드, `UseSeed` override 추가). 경고는 0이었다.
- 보호 형상은 Ridge spec에서 2D 49개·3D 40개를 만들었다. 여유는 캡슐 반지름·문 폭·착지부·카메라 근거로 정했고, 이전 계수1.2·경로2m 대신 메시 전체 상자에 적용한다. 오프라인 배치점 추정으로 구역8,600m² 중 지피 약4,490m²·나머지 약2,656m²가 후보였다. 석교·부유섬처럼 지면을 자르지 않는 몸체는 2D 보호에서 뺐다.
- Python 프로퍼티 이름(`b` 접두어 제거, `keep_out2d`)과 SelfPruning `pruning_type` 미노출로 스크립트를 세 번 고쳤다. 첫 실패가 메모리에 남긴 자기 파생 에셋만 dirty 예외로 허용했다.
- 첫 생성은 시드42·Clover6개였다. 엔진 `UseSeed()` 기본 false와 PCGVolume 브러시 QueryOnly 충돌을 발견해 WIP를 저장하지 않고 버렸다. 디스크 WIP와 복구 사본의 해시 일치를 확인했다. 시도 확률을 1.0으로 올린 시드51001 생성은 후보410개 중 통과27개, 프루닝 뒤 20개(Clover15·Sandstone2·Moss2·Fern1)였다. 계약·재질·볼륨 NoCollision을 재조회했다.
- 사용자 근경 판정은 자연스러움이었고, 승인 뒤 WIP를 저장했다. TestMap03 재베이크는 수집기 오류0, 모든 수치가 §22와 같았다. 검사용 풀 메모리 임시 선택 1인 PIE(EnemyDensity0, slot2 Y180)에서 보행·착지·카메라·사격 이상 없음과 자연스러운 접지를 확인했다. PIE 뒤 원래 풀을 복원했고 INI·정식 풀 해시는 그대로다.
- 사용자는 바위 관통이 어색하다고 지적했다. GX-013은 1차 장식 범위이므로 최종 확정이 아니라고 답했다. 적 SurfaceData·계약·판정 대칭 때문에 단순 충돌 켜기가 아니라 바위 역할 지형 소스 승격이 필요하다는 1-B5b 계획 초안을 환경 실행서 §24에 반영했다. 새 GX 결정은 아직 만들지 않았다.
- 상세/재현은 환경 실행서 §23~24, 근거는 `Saved/Profiling/Graphics/Phase01/RidgeDecorationPCG_20261116/`다.


### 1-B5b 마감 — 바위 Static+Blocker 승격·재베이크·1인 PIE(2026-11-16)

- 착수 전 사용자와 정했다: 대상 Moss·Sandstone·Cliff(Sandstone도 현재 크기로는 관통이 어색), 역할은 전부 Static+Blocker(ISM Support는 SSN D-058로 막힘), Phase 15는 독립 진행, 막는 역할만 2D 동선 여유 +100cm. GX-014를 추가하고 GX-013을 바위 역할에 한해 일부 대체됨으로 표기했다.
- 메시 충돌을 읽기 전용으로 확인했다. Moss는 컨벡스3, Cliff는 단순64, Sandstone은 ComplexAsSimple(렌더7,166 tris)이라 원본의 생성 컨벡스를 쓰는 파생 메시를 만들었다. 실측 높이는 Moss 약48~62cm, Sandstone 약20cm, Cliff 약2.1m다.
- C++ 규칙/샘플러에 막는 역할 플래그·추가 2D 패딩·`TerrainBlocker` 속성·수동 장식 표식·이전 PCG 출력 무시를 넣었다. 게임 타깃 컴파일 뒤 에디터가 꺼져 있어 바로 에디터 빌드했다(성공).
- 빌더에 파생 메시·V002 그래프(필터 → Spawner2)·수동 바위 승격·재생성 모드를 추가했다. 선택자 `set_attribute_name`이 Python에서 ref를 바꾸지 않아 `import_text`로, 디스크립터 BodyInstance가 프로필 이름만 바뀌어 Blocker ISM이 NoCollision으로 나온 것을 임시 컴포넌트의 적용된 BodyInstance 복사로 고쳤다(`repair-graph`). 저장 전 재검사로 충돌 활성을 확인했다.
- PCG 결과는 Clover15·Moss2·Fern1이 1-B5a와 같은 위치, Sandstone은 2→1(추가 여유로 1개 거부)이다. 사용자 근경에서 Sandstone·Moss 충돌은 자연스러움, Cliff는 광선 측정(보이는 면 기준 앞 최대5.9cm·안쪽 최대10.5cm에서 차단) 뒤 유지하기로 했고 WIP를 저장했다.
- TestMap03 재베이크는 수집기 오류0, Nav 셀 −1·payload −8B 외 수치가 같았다. PIE①(slot3, 밀도0)에서 접촉·막힘·착탄·카메라 이상 없음, PIE②(slot0, 밀도1)에서 DrawNav 3000으로 바위 주변 노드·간선 차단을 확인했다. DrawNav 1200은 기준 반지름 거르기 때문에 Ridge 분지에서 `nodes=0`이었다.
- 벽보행 적은 바위 표면에 붙지만 지상형 적은 발이 바위를 관통한다. 측면 sweep이 `MaxStepUp`45cm 아래를 턱으로 넘기고 Support(지각)로 접지하기 때문이다. 사용자가 별도 SSN 세션에서 다룬다(권장: Support 없는 Blocker 넘기 금지 → ISM Support). 종료 뒤 원래 풀을 복원했고 INI·정식 풀·WIP·SurfaceData 해시를 확인했다.
- 상세/재현은 환경 실행서 §24, 근거는 `Saved/Profiling/Graphics/Phase01/RidgeRockBlocker_20261116/`이다.

### 1-B5c 마감 — 대표 구역 밀도 확인·동선 등급별 완화 결정(2026-11-16)

- 사용자 요청은 "1-B5b 진행"이었지만 문서상 이미 마감이라 확인했고, 사용자가 밀도·원거리 확인을 골랐다. 시작 시 unreal-mcp가 연결되지 않아 사용자가 에디터와 MCP를 연결했다.
- 새 `ExploreRidgeDecorationDensity.py`로 규칙 간격·시드를 메모리에서만 바꿨다. 기준(×1.0)은 1-B5b 검사와 인스턴스까지 같았다(19개). ×0.7 23개, ×0.5 45개. 거부는 경사·2D 동선 보호·지각 없음이 대부분이고, Moss는 Self Pruning에서 Clover에 밀려 2개에 머문다(추정).
- A/B/C·Close를 캡처해 비교 이미지를 만들었다. A/B/C는 절벽·암반 위주라 장식이 거의 보이지 않아 밀도 판정 구도로 맞지 않았다(바뀐 픽셀 A0.3%·B1.5%·C0.7%).
- 사용자는 에디터에서 직접 보고 훨씬 더 풍성하게, 동선 근처에도, 구석도 더, 종류도 더 늘리기를 원했다. 결정: 동선 등급별 완화(GX-015 추가, GX-014 동선 추가 여유 대체), 간격+겹침 정리 분리+지피 경사 확대, 같은 계열 변형 먼저.
- 원래 값으로 복원해 `Inspection_V002`와 인스턴스·계약·수동 바위 일치, 보호 파일6개 해시 불변을 확인했다. 규칙·WIP는 같은 내용으로 메모리 dirty이며 저장하지 않았다.
- 상세는 환경 실행서 §25~26, 근거는 `Saved/Profiling/Graphics/Phase01/RidgeDecorationDensity_20261116/`이다.

### 1-B5d 마감 — 동선 등급별 완화·밀도·Fern 우선 그래프 V004·재베이크·1인 PIE(2026-11-16)

- C++: 역할 `KeepOut2DPadding`(부호 있음)으로 규칙 수준 Blocker 여유를 대체하고, 샘플러가 `GroundCover` 속성을 남긴다. 게임 타깃 컴파일 뒤 사용자 허가로 에디터 빌드를 두 번 했다(V003용·V004용).
- 빌더: GX-015 값(지피 동선 안 허용·Fern −50·바위 0), 간격 Clover280/Sandstone450/Moss500/Fern500, Clover·Fern 경사 0~40°, 그래프 V003(바위 > 식생) → V004(Fern > 바위 > 지피). `rules`가 재생성 직후 dirty WIP를 허용하게 했다.
- V003은 239개였으나 Fern이 바위 범위 제외로 1개였다. 사용자 요청(바위 20~30%를 Fern으로)으로 V004를 만들었고, 간격 축소는 Fern을 늘리지 못해(통과3) 경사 확대로 Fern5·바위18(22%)을 채택했다. 최종 Clover222·Sandstone12·Moss6·Fern5 = 245.
- 툴바 저장 클릭이 반영되지 않아 Python `save_current_level`로 WIP를 저장했다. TestMap03 재베이크는 수집기 오류0, Nav 셀75,886→75,877·component70→71·portal424→422(나머지 같음).
- 1인 PIE(EnemyDensity1, WIP slot5/6): 사용자는 대부분의 바위가 바위 크기만큼만 Nav를 끊고, 바위 하나(PCG Moss와 30cm 옆 수동 Moss_01) 주변만 구멍이 커서 적이 평지를 크게 우회하는 것이 어색하다고 했다. 추격은 정상이다. `ExplainNavGap`으로 근거를 남기고 사용자 결정에 따라 바위는 옮기지 않고 SSN `Current.md`에 경로 다듬기 후보만 기록했다.
- 원래 풀 복원·해시 확인. 사용자 요청으로 Electric Dreams 수준 우거진 식생 비교를 낮은 우선순위로 실행서 §26에 남겼다.
- 상세는 환경 실행서 §26, 근거는 `Saved/Profiling/Graphics/Phase01/RidgeDecorationExpand_20261116/`이다.
