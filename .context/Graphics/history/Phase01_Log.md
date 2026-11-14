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
