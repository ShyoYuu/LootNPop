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
