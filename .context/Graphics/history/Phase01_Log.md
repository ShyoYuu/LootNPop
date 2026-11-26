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

### 1-B5e-1 마감 — 같은 계열 변형 반입·로드 검사(2026-11-17)

- 원본 참조를 읽어 반입 목록을 메시18개로 정했다. Moss_07은 원본에 없는 MI를 참조해 제외했다. 예상 새 파일48개(약1.21GB)와 반입 전 타깃 파일 목록을 Saved에 남겼다.
- 사용자가 8001에서 Migrate하며 전부 덮어쓰기를 골랐다. 덮어쓴47개는 첫 반입 뒤 저장된 적이 없었고 현재 해시가 첫 반입본 `MigratedFiles.json`과 모두 같아 바뀐 것이 없다. 새 파일48개는 예상과 정확히 같다.
- 읽기 전용 `InspectRidgeVariantImport.py`로 18개를 로드했다. 누락 의존성·로그 경고·dirty 변화0, ISM·Nanite 재질 사용 켜짐, 바위 변형은 모두 단순 컨벡스 충돌이라 파생 메시가 필요 없다.
- Sandstone_02는 01과 같은 형상이라 쓰지 않는다. Sandstone_04·05·Moss_03·04는 2~3.6m 큰 바위이고, Clover_04·05는 5m 깔개다.
- 사용자 결정: 큰 바위도 쓴다(맵의 2~3m 이상 엄폐 지형을 나중에 큰 바위로 교체할 것을 감안, 더 키울 수도 있음). Clover_04·05는 근경 시험 뒤 정한다.
- 상세는 환경 실행서 §27, 근거는 `Saved/Profiling/Graphics/Phase01/RidgeVariantImport_20261117/`이다.

### 1-B5e-2 마감 — 변형 근경 시험(2026-11-17)

- 새 `ProbeRidgeVariantCloseup.py`로 WIP 대분지 바닥에 변형19개(Clover_04·05 원점 기준 비교본 포함)를 메모리에서만 놓았다. 첫 시도는 지형 암반 메시의 거대 bounds를 장식으로 세어 전부 거부돼, 지형 메시를 제외하고 다시 놓았다.
- 경사 0~3°, 모서리 뜸 최대 1cm, 큰 바위는 높이 비례로 7~16cm 묻혔다. 사용자 판정은 모두 문제없음(충돌 외곽·접지·Clover_04·05 샘플러 규칙). 큰 바위는 생각보다 작지만 원래 크기로 괜찮다.
- `clear`로 지웠고 WIP 디스크 파일은 그대로다(메모리 dirty는 저장 없이 버림).
- 사용자 질문으로 원본의 더 큰 바위를 `ApproxSize` 태그로 조사했다(SmartAssets Cliff·사암 Ridge/Mesa/HugeCliff·Nordic/Tundra 보울더 등, 실행서 §28 표).

### 1-B5e-3 마감 — 변형 역할 추가·보호 완화·바위 축소(2026-11-17)

- 사용자 결정: 큰 바위도 PCG에 소수, 작은 변형은 계열 간격 그대로(밀도 증가). 역할21개(기존 ID 유지·변형마다 새 ID)로 생성하니 337개였지만 바위·Fern은 겹침 정리에 묶여 거의 늘지 않았고 큰 바위는 간격을 6m까지 줄여도 1개였다.
- 사용자 질문으로 보호 형상을 에디터 디버그 선으로 그리는 `DrawRidgeDecorationKeepOuts.py`를 만들었다. 평지의 6%만 바위가 놓일 수 있었고, Pod 편성 구(구역61%)와 지형 깎는 동선 폭(40%)이 원인이었다. 사용자 결정 GX-017: 동선 반폭50%·Pod 중심9m.
- 완화 뒤 V004(Fern > 바위)가 큰 바위를 지워(정리 전88 → 2) 그래프 V005(큰 바위 > Fern > 작은 바위 > 지피, `$Extents.Z > 55`)를 만들었다(GX-016). 404개, 사용자: 이전보다 훨씬 나음.
- 저장·재베이크·1인 PIE에서 적 이동이 부자연스러웠다(blocker_rejects 51,950/416,788 = 12.5%, 1-B5d 0.9%). 사용자 결정 GX-018: 바위 여유 +300/+450·간격2배, Fern 간격350. 384개(작은 바위27·큰 바위2·Fern52·Clover303).
- 다시 저장·재베이크(Nav 셀75,870·component70·portal424·noNav0)·1인 PIE: blocker_rejects 12,122/460,164 = 2.6%. 사용자: 훨씬 나아짐, 작은 바위를 가끔 못 돌아가는 것은 SSN 세션에서 본다. 풀 복원·해시 불변.
- Fern은 후보를 2배로 해도 Fern끼리 겹침 정리로 52개에 묶였다. 사용자 결정: Fern 겹침 허용·동선 안쪽 허용·새 계열 반입을 1-B5f에서 함께 한다.
- 중간에 에디터가 `QUIT_EDITOR`로 저장 없이 재시작됐고, 디스크의 규칙·그래프로 같은 결과를 재생성했다.
- 상세는 환경 실행서 §29, 근거는 `Saved/Profiling/Graphics/Phase01/RidgeDecorationVariants_20261117/`이다.

## 2026-11-19 — 1-BR U1 설계도 추출

- GX-021 계획으로 새로 시작했다. `RidgeBlueprint_V001.json`에 단 7개와 외곽 절벽선 51개, 분지 1개, 지형 경로 14개와 보행 연결 18개, 마커 45개, 카메라 24개와 사선 기대값 8개, 보존 영역(동굴 2개·부유섬 2개·석교 1개)을 기록했다. JSON 재생성 결과가 일치했다.
- LootNPop 8000에서 정식 V011 Pod 12개·훅 6개·검토점 27개를 이름으로 대조했다. 검토점은 조립 스크립트의 200cm 띄우기를 반영했고, 마커 위치 오차 최대는 `5.15×10⁻¹²cm`였다.
- 새 `EDTerrain_V001/LVI_Ridge_EDTerrain_V001`에 V011 지각·암반 Floor·Shell을 반투명 고스트로 저장했다. 세 컴포넌트의 `Decoration`·`LNPDecoration`·충돌 없음과 액터의 `HiddenInGame`·Editor Only를 확인했다. 뷰포트에서 고스트와 V006 동선 보호선을 함께 확인했다. 보호선은 에디터 디버그 표시다.
- 정식 Ridge·풀·이전 그래픽스 WIP 등 19개 파일의 SHA-256이 작업 전후 일치했다. 새 레벨과 고스트 재질은 Git 제외다. U2는 사용자 진행 확인을 기다린다.

## 2026-11-19 — 1-BR U2 지형 에셋 선정·반입

- LootNPop을 닫고 ElectricDreamsEnv 8001에서 원본 후보 55개를 읽었다. `EDTerrainCatalog_V001.json`에는 로컬 bounds, Nanite fallback LOD0 삼각형 수, 충돌 설정, LOD0 실효 재질을 기록했다. 원본 에디터 dirty 패키지는 전후 모두 0개다.
- 치수와 재질로 메시 17개를 선정했다. 절벽은 Massive 01·03·05(높이 29~37m)와 Huge·SmartAssets를 조합한다. 능선·메사는 CanyonSandstoneRidge 03과 MassiveCanyonSandstoneMesa 01, 둑·완사면은 GrassyEmbankment 01과 SandstoneRockyGround 02·05를 쓴다. 첨탑은 높은 절벽 조각, 낮은 암괴는 ForestRockFormation 06과 NordicBoulder 02를 후보로 삼았다. 하상은 이전 혼합 재질을 재사용한다.
- HugeSandstoneCliff 07의 에셋 기본 재질과 원본 구도의 재질은 다르다. 구도 재현용 `MI_HugeSandstoneCliff_07`을 메시 17개와 함께 명시 시작점으로 선택했다. 선정 메시의 fallback LOD0은 3,056~63,672삼각형이며 단순 충돌이 없다.
- 사용자가 18개 시작점에서 Migrate하고 `No All`을 선택했다. 새 파일 96개(3.912GiB)가 예상 목록 및 Migrate 로그와 일치했다. 기존 의존성 43개와 원본 139개 해시가 보존됐다. 새 물리 재질 4개 경로를 `.gitignore`에 추가했고 139개 대상 경로 모두 Git 제외다.
- 원본과 반입본 해시는 `ElectricDreamsPalette.json`의 `ed_terrain_v001`에 기록했다. 원본 실측·반입 전후 목록·Migrate 로그는 `Saved/Profiling/Graphics/Phase01/EDTerrain_V001/`에 있다.
- ElectricDreamsEnv 종료 뒤 LootNPop 8000에서 메시 17개와 하상 혼합 재질을 로드했다. 첫 Nanite·텍스처 파생 데이터 생성에 약 7분 44초가 걸려 MCP의 5분 응답 제한 뒤에도 에디터 로그에서 완료까지 확인했다. 치수·fallback·실효 재질 경로·충돌 설정·의존성 139개 클래스가 원본과 일치했다. 관련 로드/재질/셰이더 경고와 dirty 패키지 0개, 로드 전후 대상 139개 해시 일치를 `TargetVerification.json`에 남겼다.

## 2026-11-19 — 1-BR U3 V012 기반면과 조기 베이크

- V011 높이장에 8.5m 격자의 분리형 저역 통과 필터를 적용했다. PATHS·ROUTES 노면, 지상 Pod 반경 6.5m, 훅 착지 반경 6m, 동굴 경계와 석교 교대를 보호하고 14m에 걸쳐 평활 높이와 연결했다. 석교 양쪽 접근로도 보호에 포함했다. 보존 영역 밖 내부의 4m 격자 954표본에서 45° 초과 높이장 기울기 수가 131→59로 줄었다.
- Blender 검증 결과는 진입 광선 285개, 경로 18개 바닥 광선 3,678개, Pod 12개, 훅 착지 6개 통과다. 경로에서 잰 최대 경사는 44.43°이고 보행면 최고점은 +76.22m다. 더 촘촘한 보조 표본의 북쪽 잔도 측면 1점은 V011·V012가 같은 동굴 지붕 Shell을 맞혔다(경사 45.23°, 높이 18.96m).
- U1 WIP에 경계 고리와 기반면을 잇는 지각 82,105삼각형과 보존 Rocks Floor 6,494·Shell 31,838삼각형을 반입했다. 지각에 RidgeSurface V005를 지정하고 Pod 12·훅 6·검토점 27개를 Blender 실제 바닥에 재투영해 저장했다. Meadow native 정점 13,989개의 최대 스냅 보정은 0.00763cm였다.
- TestMap03에서 조기 베이크 성공. Support 2소스·22층·576,088샘플, Nav 76,620셀·22 components·507 portals, Spawn 앵커 12개·후보 9,591개·noNavRejected 0이다. Meadow의 양자화 반지름과 Valid 비트 2,208개가 정확히 일치했다. 기존 정식 Ridge·Meadow·풀·이전 WIP 등 보호 파일 41개 해시가 일치했다. 근거는 `Saved/SliceOctant/Ridge_00/V012/`의 `geometry-verification.json`, `native-import.json`, `native-assembly.json`, `early-bake.json`이다.

## 2026-11-19 — 1-BR U4 Z1 초벌 배치

- 산괴 동쪽 절벽, 대분지 남쪽, 협곡 하류 둑을 한 구역으로 잡고 시각 메시 13개를 `EDTerrain/Z1`에 배치했다. 초안에서 협곡 PATHS·ROUTES 보호선과 겹친 네 조각을 옮긴 뒤 Pod·훅·사선과 함께 재검사했다. 배치 상자에서 가장 가까운 seam 평면까지 112.15m였다.
- `PlaceRidgeEDTerrain.py`는 현지 Up 기준 회전, V012 높이 오프셋, 스케일, 재질 덮어쓰기를 기록한다. 13개 모두 `Decoration`·`LNPDecoration`·충돌 없음과 배치 transform을 확인했다. WIP에 초벌을 저장했고 사용자가 에디터에서 구도와 이음을 확인 중이다.

### Z1 조형 비교본

- 사용자는 V012 지각의 형상은 좋지만 덧붙인 메시가 대부분 어울리지 않는다고 판정했다. Z1에서 기존 플레이 동선을 바꿔도 된다고 하여 절벽과 암봉을 먼저 배치하고 기반면을 맞추는 비교본을 만들었다. 사용자가 시험한 미저장 transform은 저장하지 않기로 했다. 앞서 저장한 Z1 초벌 레벨은 SHA-256이 일치했다.
- V012 서쪽 사면을 뒤로 물려 대분지 발치를 낮췄다. 현지 `(-1600, -250)` 표본은 H `+46.52m`에서 `-39.50m`로 바뀌었다. 바깥은 폭 14m 전이로 V012에 연결하고, 동굴 절개선의 높이는 유지했다.
- 별도 `Z1Composition_V001/LVI_Ridge_EDTerrain_Z1Composition_V001`에 서쪽 두 단 절벽, 북쪽 동굴 외벽 덧댐, 독립 암봉, 하류 양쪽 둑 등 시각 메시 14개를 저장했다. 하상 혼합 재질 사본 `MI_Ridge_Z1_Channel`은 새 곡선을 따라 이어지도록 바꿨다. 협곡 입구 시점의 Unlit 뷰포트에서 암봉·하상·동굴 가림을 확인했다. 상단 절벽과 배경의 접합, 이 구도의 보행·충돌 검증은 후속이다.
- 지각 81,437삼각형의 Blender 기본 토폴로지 검사와 UE 반입 검사를 통과했다. Meadow 경계 최대 스냅은 0.00763cm이고, 14개 배치의 가장 가까운 seam 평면 여유는 135.63m다. 근거는 `Saved/SliceOctant/Ridge_00/V012_Z1Composition/`의 `manifest.json`, `native-import.json`, `composition.json`이다.

### Electric Dreams 원본 배치 대조

- 사용자는 전체 구도는 개선됐다고 보고 두 MassiveSandstoneCliff 조각의 벽면 사용과 SmartAssets Cliff 06의 독립 암봉 사용을 다시 확인해 달라고 했다. LootNPop의 렌더 LOD0에서 세 메시 모두 단면 재질, 열린 경계, 주 방향에 편중된 투영 면적을 확인했다. 에셋 이름이나 두께만으로 용도를 판정하기 어려워 ElectricDreamsEnv 8001에서 실제 월드 배치와 원본 조립체를 읽었다.
- `WestFace`의 MassiveSandstoneCliff 05는 CloseRange `BP_PCG_LargeAssembly`와 `PL_LargeCliff_01b`·`02e`에서 상부 선반과 덮개로 쓰였다. 원본 조립체 두 곳의 8개에서 주 표면은 수직면 기준 60.88~84.33° 기울었다.
- `NorthFace`의 MassiveSandstoneCliff 03은 `ASM_RiverEmbankment_02B`와 CloseRange `PCGDemo_DitchBP`에서 하천 둑의 바닥과 낮은 경사면으로 쓰였다. CloseRange 78개에서 수직면 기준 기울기는 중앙값 88.02°였다.
- `BasinPillar`의 SmartAssets Cliff 06은 `PL_LargeCliff_01b`·`02e`, `Asmbl_BG_Hoodoo_01b_SmartAsset`의 세로 외피였다. 수직면 기준 5.09~18.62°였다. Ridge에서는 열린 뒤편을 산괴에 접하는 절벽 면으로 검토한다.
- 근거는 `Saved/SliceOctant/Ridge_00/V012_Z1Composition/`의 `MeshFaces.json`과 `ElectricDreams_*.json`이다. 원본 레벨과 메시 파일 8개 SHA-256과 레벨별 로드 후 dirty 상태를 대조했다. 원본 배치 결과를 팔레트에 기록했고 Z1 조형 비교본은 LootNPop에서 역할을 교정할 차례다.

### Z1에 원본 역할 적용

- `ED_Z1_WestFace`의 MassiveSandstoneCliff 05를 Roll −75°로 눕혀 `ED_Z1_WestUpperShelf`로 옮겼다. `ED_Z1_NorthFace`의 03은 Roll −74°로 눕혀 하천 북쪽의 `ED_Z1_NorthBankShelf`에 접지했다.
- `ED_Z1_BasinPillar`의 SmartAssets Cliff 06을 서쪽 산괴에 붙인 `ED_Z1_WestWallNorth`로 옮겼고, 같은 메시로 아래쪽 벽의 `ED_Z1_WestWallSouth`를 한 개 더 배치했다. 두 외피의 상단을 산괴 쪽으로 기울였다.
- 상단 `WestCrown`·`NorthCrown`은 새 외피와 겹치도록 옮겼다. 뷰포트 광선으로 입구에서 떠 보이는 부분이 `NorthCrown`임을 확인해 산괴 쪽으로 더 넣었다. 조정 뒤 같은 입구 시점에서 돌출됐던 모서리가 가려졌다.
- LootNPop에서 비교본 시각 메시 15개의 transform·재질·`LNPDecoration` 상태와 seam 여유 최솟값 132.74m를 확인했다. 역할 교정 전 비교본은 `BeforeSourceRoles.umap`, 현재 검사 결과는 `composition.json`에 있다. 기존 초벌 저장본은 해시가 일치했다. 다음은 사용자 구도 검토와 U4 배치 재현이다.

## 2026-11-19 — 1-BR U4 Z1 이질감 원인과 ED 조립체 시험 준비

- 사용자는 V002 서쪽 절벽에서 같은 계열끼리만 어울리고 모아 두면 이질적이라고 판정했다. 방향은 B안(ED 원본 조립체를 상대 배치 그대로 단위로 옮겨 쓰기)을 우선하고, ED 구역 통째 이식(C안)은 최후 수단으로 두기로 했다. 첫 단계로 조립체 하나를 그대로 넣는 시험을 한다.
- 어울리던 세 묶음은 각각 같은 메시의 반복이었다(Cliff_06 ×3, MassiveSandstoneCliff_01 ×2, MassiveSandstoneCliff_05 ×2). V002는 MassiveSandstoneCliff_05를 0.66~1.03배, Cliff_06을 1.2~1.6배(Z 비균등)로 썼다. ED 조립체는 같은 메시를 각각 0.10~0.19배, 0.46배 균등 스케일로 쓴다. 따라서 텍셀 밀도와 층리 크기가 원본보다 3~10배 거칠다.
- ElectricDreamsEnv를 헤드리스(NullRHI)로 열어 `PCGAssemblies` 49개 레벨의 전체 구성을 읽었다. `PL_LargeCliff_01b`는 23×21×46m에 인스턴스 505개·메시 34종이다. 그중 암석은 SmartAssets 7종 51개뿐이다. 나머지는 MoneyPlant 324개, IvyTest 덩굴 28개, Hornbeam 36개, 이끼 바위·뿌리 같은 식생과 잔재가 암석 이음을 덮는다. `02e`는 인스턴스 9,148개·메시 156종이다. 원본 절벽의 완성도는 암석 조합보다 식생·잔재 층에서 크게 나온다.
- `PL_LargeCliff_01b`의 재귀 의존성은 274패키지이고, 그중 178개 3.93GiB가 LootNPop에 없다(`01c` 3.76GiB, `02e` 9.37GiB). 새로 생기는 `Asmbly`·`Custom/IvyTest`·`MSPresets`·`PCG`·외부 액터 경로를 `.gitignore`에 추가했고, 178개가 모두 제외되는 것을 확인했다.
- 이전 Z1 검토는 Unlit 뷰포트였고 V002 레벨에는 조명 액터가 없다. 조립체 시험은 조명 조건을 고정해서 판정한다.
- 근거는 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`의 `EDAssemblies.json`(배치 원자료), `EDAssemblyDeps.json`, 덤프 스크립트 2개다. 다음은 사용자가 ED 에디터에서 `PL_LargeCliff_01b`를 Migrate하는 것이다.

### 조립체 시험 레벨

- 사용자가 `PL_LargeCliff_01b`를 Migrate했다. 미반입 178개가 모두 들어왔고 기존 96개는 수정 시각이 바뀌지 않았다. Git 추적 파일은 늘지 않았다.
- TestMap03 조명은 구 중심 점광원 1개, 보조 점광원 4개, PostProcessVolume이다. Sky Light·대기·안개는 없다. 따라서 어느 옥탄트에서나 주광이 현지 머리 위에서 내려오고 절벽 면은 비스듬히 스치는 빛만 받는다.
- 새 `Z1AssemblyTest_V001/Z1AssemblyTest_V001`에 TestMap03 조명 6개를 복사했다. V002와 `PL_LargeCliff_01b`는 Level Instance로 참조만 한다. 조립체는 서쪽 벽 앞 분지 바닥 로컬 `(0, -1600)`에 현지 Up 기준 원본 스케일로 세웠다. 재생성은 `Scripts/Profiling/Graphics/BuildRidgeZ1AssemblyTest.py`의 `build` → `instances`이고, 배치값은 `AssemblyTestLevel.json`에 있다.
- 에디터 Python의 `AutomationLibrary.take_high_res_screenshot`이 셰이더를 강제 컴파일하다가 V002의 `MI_MassiveSandstoneCliff_01`(Megascans DynamicLayering)에서 `FAnisotropyPS` assert를 내며 크래시했다. 화면 확인은 콘솔 `HighResShot`으로 한다. 첫 로드에서는 새 4K VT 텍스처 빌드에 약 5분이 걸렸다.
- 사용자 판정은 Lit 뷰포트에서 받는다.
- 사용자 판정: `PL_LargeCliff_01b`는 원하던 품질이다. 이 수준으로 Ridge를 채우면 목표를 초과한다고 했다. GX-022(원본 조립체 단위 조립)로 확정했다. 01b 반입 뒤 남은 추가 반입량은 `01c` 4패키지(약 0MiB), `02e` 334패키지(5.57GiB)다.

### 01b·01c 서쪽 벽 재구성 시험

- `PL_LargeCliff_01c`를 반입했다(서어나무 묘목 3개와 레벨 1개). 01b와 01c는 암석 약 100개 중 85개를 같은 자리에 공유하는 같은 절벽 변형이고, 01c는 꼭대기가 약 4.5m 낮다. `02c`와 `02d`는 공유 조각이 0개다.
- `Z1AssemblyWall_V001/Z1AssemblyWall_V001`은 V002에서 서쪽 벽·왕관·Talus 9개를 빼고 나머지 10개를 NoCollision 사본으로 옮긴 레벨이다. 벽 발치 로컬 x≈−1,750~−1,800에 01b·01c를 18.5m 간격으로 0·90·180·270° 돌려 세웠다. 재생성은 `BuildRidgeZ1AssemblyTest.py wall V001`, 배치값은 `AssemblyWallLevel_V001.json`이다.
- Unlit 확인 결과 조립체는 각각 독립된 기둥으로 읽혔고, 이어진 벽이 되지 않았다. 사이와 위로 V002 기반면의 벽 뒤 사면이 넓게 드러났다. 네 기둥은 폭·높이·윤곽이 거의 같아 회전만으로는 반복이 남는다.

### 02 계열 서쪽 벽 재구성 시험

- 사용자가 01 계열 두 종만으로는 벽이 어렵다고 보고 02 계열 반입을 정했다. `02b`·`02c`·`02d`·`02e`를 함께 반입했다(추가 335패키지, 5.57GiB). 의존성이 거의 같아 `02e` 하나와 용량 차이가 없었다. Git 제외에 `PhysMat_Gravel`을 더했다.
- `Z1AssemblyWall_V002/Z1AssemblyWall_V002`는 V001과 같은 V002 사본 위에 `02c`·`02e`·`02d`를 벽 발치 로컬 x≈−2,000~−2,100에 25m 간격(약 10m 겹침)으로 세웠다. `02b`는 뒤쪽 x −3,100에 180° 돌려 6m 높게 두었다. 재생성은 `BuildRidgeZ1AssemblyTest.py wall V002`이다.
- Unlit 확인 결과 이어진 덩굴 암벽으로 읽혔다. 조립체 사이 아래쪽 V자 틈 두 곳과 중앙 상단으로 기반면 사면이 조금 보이고, 뒤쪽 `02b`는 정면 시점에서 거의 보이지 않는다.
- 사용자 판정: 여전히 덩치가 미묘하다. 더 큰 절벽이나 산을 찾기로 했다.

### 배경 조립체 산괴 시험

- ED `PCGAssemblies/Background`에는 원경용 조립체가 있다. Hoodoo 01b는 145×113×190m, Mountain 계열은 폭 287~369m, Vista Cliff는 약 300×300×210m다. SmartAssets 절벽을 4~13배 키워 만들었고, 수목은 PCGVolume이 만든 서어나무 ISM이다. 옥탄트에 넣으려면 0.3~0.5배로 줄여야 한다.
- 사용자가 6종(`Asmbl_BG_Hoodoo_01b`·`02b`, `Asmbl_BG_Mountain_01b`·`04b`, `PL_Vista_Cliff_01`·`01c`)을 반입했다. 미반입 52패키지(1.26GiB)가 모두 들어왔고 Git 추적 파일은 늘지 않았다.
- `Z1AssemblyWall_V003/Z1AssemblyWall_V003`은 V002의 02 계열 벽 3개를 앞에 두고, 뒤쪽 그 자리 기반면에 10m 묻어 Hoodoo 01b(0.4배, 로컬 −5,200, −600)와 Mountain 04b(0.3배, 로컬 −5,500, 3,500)를 세웠다. 둘 다 경계에서 약 114~116m 떨어져 있다. 재생성은 `BuildRidgeZ1AssemblyTest.py wall V003`이고, 확인 시점은 같은 스크립트의 `look`으로 바꾼다.
- Unlit 확인 결과 벽 뒤에 수목 덮인 봉우리 두 개와 Hoodoo 첨탑이 하늘선을 만든다. 정면 저시점에서는 벽 위로 산괴 아랫부분만 보인다. 벽 남쪽의 V002 기반면 큰 사면은 여전히 화면 왼쪽을 크게 차지한다. 0.3~0.4배로 줄이면서 수목도 같은 비율로 작아졌다.
- 사용자 판정: 배경 조립체를 줄여 나무까지 작아진 것은 어색하다. 원래 크기로는 쓸 수 없다. Mountain 04b는 287×177×221m인데 구 반지름이 300m라 봉우리가 중심 광원 79m 앞까지 솟는다. 옥탄트 중심에서 경계까지 호 길이는 약 185m이고, 긴 변의 절반 143m 구간에서는 곡률 때문에 가장자리가 약 33m 묻힌다. 근경 조립체(Forest·Ditch·LargeAssembly)는 가장 큰 것도 높이 약 53m다.

### ED 조립체 이음 방식 조사

- 열려 있던 ED 에디터(8001)에서 `ElectricDreams_PCGCloseRange`를 읽기 전용으로 열어 구성을 덤프했다. 변경 패키지는 0개다. 근거는 `AssemblyTest_20261119/`의 `EDCloseRangeJoins.json`·`EDCloseRangeDetail.json`·`EDAssemblyUse.json`·`EDMainMapClasses.json`·`EDPcgAssetUse.json`·`EDForestCliffsGraph.json`과 같은 이름의 덤프 스크립트다.
- ED는 조립체 레벨을 손으로 이어붙이지 않는다. 조립체 레벨마다 `*_PCG` 데이터 에셋(레벨→PCG 포인트 데이터)이 있고, PCG 그래프가 이를 점 위에 복사한다(`PCGAsset` 요소·`SG_CopyPointsWithHierarchy`·`ApplyHierarchy`). `02b`·`02c` 레벨을 직접 참조하는 맵은 없다.
- 본 맵 `ElectricDreams_PCG`는 랜드스케이프(스트리밍 프록시 256개)가 지형 전체 형상을 맡는다. 그 위에 `PCGDemo_Forest`(01b·01c·02d·02e·LargeBoulder·MediumGround_02), `PCGDemo_ForestCliffs`(01b·01c·02b·02c), `PCGDemo_Background`(Hoodoo·Mountain·Vista), Ditch·Ground 그래프가 얹힌다. ForestCliffs는 지표 샘플러(점 범위 13×13×20m)·이웃 거리·밀도 필터·법선으로 회전 재구성·투영·차집합을 쓴다. 가파른 지표에 절벽 조립체를 경사 방향으로 덮어 씌우는 구조로 보이며, 실제 배치 기울기는 아직 확인하지 않았다.
- CloseRange에는 랜드스케이프가 없다. 바닥은 `PCGDemo_GroundBP`(78×87m, 인스턴스 15,259)와 `PCGDemo_DitchBP`(96×106m, 23,697, `SM_ForestGround_01` 4,483개 포함)가 깐 메시다. 산괴는 `BP_PCG_LargeAssembly` 하나다. 닫힌 스플라인 5점(둘레 86m)에서 약 70×70×46m 산괴를 만들고, 인스턴스는 10,234개다. 절벽 데이터는 `PL_LargeCliff_01b_PCG` 하나이고 나머지는 Embankment·EmbankmentRoots·EmbankmentTree·FallenTree·HornbeamJungle·LargeGround 조립체와 낙엽·자갈·MoneyPlant·덩굴이다. 이음과 바닥 경계는 식생·둑·잔재 층이 덮는다.
- 정리: ED에서 큰 형상은 랜드스케이프나 바닥 메시가, 절벽 조립체는 경사면의 외피가, 이음은 PCG 식생·잔재 층이 맡는다. 지금까지 Ridge 시험처럼 조립체를 벽돌로 이어 큰 형상을 만드는 방식은 ED에 없다. 구면이라 랜드스케이프를 쓸 수 없는 Ridge에는 랜드스케이프 없이 바닥 메시로 만든 CloseRange가 가장 가까운 선례다.
- CloseRange 맵 통째 반입을 검토했다. 맵과 외부 액터·객체 113개를 루트로 한 재귀 의존성은 1,216패키지이고, LootNPop에 없는 것은 551개 10.03GiB다(Megascans 3D 5.0GiB, `Meshes` 2.6GiB, `Custom` 1.6GiB). 기존 패키지 665개는 크기만 다르고(재저장 차이) Git 추적 파일과 겹치는 경로는 없다. 현재 Git 제외 밖에 새로 생기는 경로는 `Audio`·`Blueprints/BP_DroneInstructions`·`Custom/Lighting`·`Custom/RS_Assets`·`Custom/SM_CliffPart_01`·`Levels/PCG`·`Meshes/_GENERATED/RockFormation`·`PhysicalMaterials/PhysMat_Water*`·`Textures/EnvRnD`·`__ExternalActors__`·`__ExternalObjects__/Levels/PCG`다. ED에서 켜져 있고 LootNPop에서 꺼진 플러그인은 AudioModulation·SP_HoverDrone 등 15개다. PCG는 양쪽 모두 켜져 있다. 근거는 `EDCloseRangeDeps.json`이다.
- 통째 Migrate는 대화상자 목록에 World Partition 외부 액터가 실리지 않아 중단했다. 대신 ED에서 CloseRange의 PCG 결과를 그룹별 일반 레벨 5개(`/Game/LNPBake/CloseRange/CR_LargeAssembly`·`SmallAssembly`·`Ground`·`Ditch`·`Props`)로 구웠다. 원본 컴포넌트를 메시별 ISM/HISM으로 옮겼고, 재질 덮어쓰기·커스텀 데이터·컬링·그림자·충돌 설정을 복사했다. 다섯 레벨 모두 LargeAssembly 위치 `(90250, -4360, 1660)`을 원점으로 쓴다. 재로드 대조에서 메시별 인스턴스 수가 다섯 레벨 모두 원본과 일치했다(10,234·364·15,259·23,697·51, 데칼 23). 재생성은 `Scripts/Profiling/Graphics/BakeEDCloseRange.py bake`·`verify`이고, 기록은 `EDCloseRangeBake.json`이다.
- 구운 레벨의 반입 의존성은 925패키지이고, 그중 미반입 306개 9.72GiB다(`EDCloseRangeBakeDeps.json`). 맵 통째 반입용으로 넣었던 Git 제외 규칙을 이 범위(`LNPBake`·`Custom/RS_Assets`·`Custom/SM_CliffPart_01`·`Meshes/_GENERATED`·`PhysMat_Water*`)로 줄였다. 306개가 모두 제외되는 것을 확인했다.

### CloseRange 구운 레벨 반입과 Ridge 배치 시험

- 사용자가 `CR_*` 5개를 Migrate했다. 미반입 306개가 모두 들어왔고 Git 추적 파일은 늘지 않았다.
- `CloseRangeBake_V001/CloseRangeBake_V001`은 평면 맵 원점에 5개 레벨을 놓고 태양·스카이라이트·대기·안개를 기본값으로 둔 비교 레벨이다(`BuildRidgeZ1AssemblyTest.py crflat`). Unlit 확인 결과 숲 덮인 둔덕 위에 01b 계열 절벽 기둥이 선 CloseRange 산괴가 그대로 재현됐다. 첫 로드 때 물 재질의 레이 트레이싱 히트 셰이더 컴파일 오류(`GetSingleLayerWaterMaterialOutput*` 미정의)가 로그에 남았다.
- V002 서쪽 기반면은 분지 바닥(x > −2,000)에서 x −2,500~−3,000 사이에 약 58m까지 솟고, 그 뒤는 고원(58~74m)이다. CloseRange 산괴는 바닥 위 높이 약 30~40m의 둔덕이라 이 벽보다 작다.
- `Z1AssemblyWall_V004`: 산괴를 로컬 (−3,500, −200)의 분지 바닥 높이에 두었다. 기반면 사면 속에 묻혀 정면에서 보이지 않았다.
- `Z1AssemblyWall_V005`: 02 계열 벽 3개 뒤 고원 위(로컬 −5,500, −200, 동쪽 고원 높이 기준)에 산괴를 숲 왕관으로 얹었다. 정면에서는 벽 위로 절벽 기둥만 보였다. 원경에서는 산괴 둔덕 대부분이 기반면 봉우리 속에 묻혔고, 기둥과 얇은 바닥 띠·쓰러진 나무 일부만 봉우리 위로 드러났다.
- 정리: 구운 산괴 자체는 ED 품질 그대로 들어왔다. 지금 V002 기반면 위에서는 산괴가 들어갈 자리가 없다. 산괴를 쓰려면 GX-022대로 산괴 아래 기반면을 산괴 바닥(원점 +4m 평면)에 맞춰 낮추거나 평탄하게 고쳐야 한다.

### 산괴 고원 기반면(Z1MassifCover)

- 사용자가 산괴 자리 기반면을 깎는 방향을 정했다. `RidgeZ1CompositionSpec.massif_cover_ground`는 서쪽 덮개 위에 분지 바닥 기준 +46m 평탄 고원을 덮는다(02 벽 상단 약 +48m보다 조금 낮게). 남북 범위는 서쪽 덮개와 같고, x −10,000 서쪽에서 V012 사면으로 돌아간다. 동굴 절개 마스크는 그대로 따른다. 산괴 중심은 남쪽 동굴 절개 부근을 피해 로컬 (−6,500, 300)으로 정했다. 이 범위의 V012 정상부(+78~94m)는 고원 높이로 깎였다.
- 재생성은 Blender `BuildRidgeZ1Composition.py -- massif-cover`(버전은 `MASSIF_COVER`)와 UE `BuildRidgeZ1AssemblyTest.py importcrust <버전>`이다. 반입 위치는 `EDTerrain_V001/Z1MassifCover_<버전>`이다. V001은 81,003삼각형, V002는 80,809삼각형이고 둘 다 비다양체 0이다. Meadow 경계 최대 보정은 0.0076cm다.
- V001(고원 시작 x −3,000)은 02 벽 바로 뒤의 서쪽 덮개 봉우리(약 +57m)가 남아 정면·원경 모두 산괴를 가렸다(`Z1AssemblyWall_V006`). V002는 벽 안쪽 x −2,200부터 0.5m 폭 안에서 고원으로 올라간다. 이 절벽은 02 벽 몸체(x −4,300~−250) 안에 들어간다. `Z1AssemblyWall_V007`에서 봉우리가 사라졌고, 하늘선은 02 벽 상단과 산괴 숲·기둥이 만든다.
- CloseRange 바닥 그룹은 산괴 원점 ±3m다. ED 일반 지면은 원점 높이이고, +4m는 산괴 둔덕 자체의 높이였다. V007의 원점 −4m는 잘못이었고, `Z1AssemblyWall_V008`은 원점을 고원 높이에 두고 Ground·SmallAssembly·Props를 같은 원점에 함께 놓았다.
- V008 부감에서도 산괴 둘레 바닥 식생이 거의 보이지 않았다. 원인은 곡률로 본다. 고원면 반지름이 약 293.5m라 평면 배치는 중심에서 d만큼 떨어지면 d²/(2×293.5m)만큼 고원 아래로 들어간다(25m에서 1.1m, 40m에서 2.7m, 55m에서 5.2m). 바닥 그룹 대부분이 중심에서 20~55m 거리에 있다. 고원 맨 지면은 기반면 재질의 평지색(보라)으로 드러난다.
- 다음 후보: 넓은 평면 그룹(Ground·Ditch·Props, LargeAssembly 둘레 둑)은 인스턴스마다 구면 지각 높이로 다시 앉힌다(드레이프). 산괴 암석은 강체로 둔다. 서쪽 덮개 바깥 남북 끝 절개 사면의 노출은 아직 보지 않았다.

### 평면 그룹 구면 드레이프

- `BuildRidgeZ1AssemblyTest.py drape <버전>`은 CR 평면 그룹 레벨을 차례로 열어 인스턴스(와 데칼)를 읽는다. 각 인스턴스는 V008과 같은 원점·회전으로 평면 배치한 위치를 옥탄트 로컬 x, y로 되돌린다. 그 자리 Z1MassifCover V002 지각 높이에 ED 지면 위 높이(원점 기준 z)를 더해 다시 앉히고, 원점 Up에서 그 자리 Up으로 돌린 만큼 회전한다. 결과는 월드 좌표의 `Z1MassifDrape_<버전>` 레벨이다. 산괴(LargeAssembly)는 강체 Level Instance로 둔다. 기록은 `MassifDrape_<버전>.json`이다.
- Drape V001(Ground·SmallAssembly·Props)과 V002(+Ditch)는 그룹별 메시 인스턴스 수가 원본과 모두 일치했다(15,259·364·74, Ditch 23,697). 시험 레벨은 `Z1AssemblyWall_V009`·`V010`이다.
- V009 부감: 산괴 둘레 바위 띠와 바닥 조각이 고원 위에 드러났다. 바닥 그룹만으로는 덮개가 듬성했다.
- V010 부감: Ditch의 지면 덮개(`SM_ForestGround` 등)와 둑·나무가 산괴를 U자로 감싸 고원 위 덮개가 크게 늘었다. 남은 문제는 세 가지다. ① 하상(ED 지면 아래)은 고원 속에 묻혀 U자 안쪽이 맨 지면(보라)으로 남는다. ② 고원 밖 북쪽 오름 사면에 앉은 지면 타일은 경사가 아니라 구면 Up에 맞춰져 선반처럼 겹쳐 보인다. ③ 데칼 일부가 회색 사각형으로 보인다(Unlit, 원인 미확인).
- 사용자가 V010을 보고 "멋있다"고 판정하고, 양옆 보라색 사면에 기존 절벽 재질을 입혀 보기를 요청했다.

### 산괴 고원 지각 재질(Z1MassifSurface)

- 보라색은 재질이 없어서가 아니다. 하상 재질(`M_Ridge_Z1_Channel`)은 V005 절벽·지면 재질에 하상을 얹은 것이다. 이 재질의 좌표 함수 `MF_RidgeSurfaceCoordinates`가 1-B0 대표 구역(x −3,200 동쪽)과 이음매 15m 밖을 구역 밖으로 보고 평지색으로 칠한다. 고원과 남북 사면은 모두 x −2,200 서쪽이라 구역 밖이다. V005 MI를 그대로 입혀도 같은 보라색이 나왔다.
- `BuildRidgeZ1AssemblyTest.py surface`는 하상 재질 부모·MI를 `Z1MassifSurface_V001/`로 복제한다. 사본에서 구역 Custom 노드만 바꿔 로컬 x −14,000~−1,500, y ±8,000 직사각형을 구역에 더한다. 하상 가중치는 원래 구역 마스크를 그대로 쓴다. 원본 V005·하상 재질은 수정하지 않았다. 더한 직사각형에는 이음매 제외가 없다(시험 레벨 전용).
- `wall V011`은 V010과 같은 배치에 이 재질을 지각 슬롯 0에 입힌다. Unlit 부감에서 고원·남북 사면·분지가 사암 절벽/지면 재질로 바뀌었다. 보라색 평면으로 보이던 양옆이 실제로는 높은 사면이었음이 드러났다. 사면에 선반처럼 앉은 지면 타일(문제 ②)이 더 눈에 띈다. 직사각형 밖 원경은 보라색으로 남는다. Lit 판정은 하지 않았다.
- 사용자 판정(2026-11-20): V011이 "엄청 맘에 든다". 이 방향(CloseRange 굽기·고원 기반면·드레이프·지각 재질 구역 확장)을 GX-023으로 확정했다. CloseRange에 더 어울리는 재질이 있으면 적용해 봐도 좋다고 했다. 다음 세션은 사면 선반 타일 정리부터 시작한다([Current](../Current.md) §3).

### 사면 선반 타일 정리(Drape V003·V004)

- 원인: Z1MassifCover V002 고원(분지 기준 +46m) 남북 가장자리는 약 15m 폭에서 V012 사면(+30~45m 더 높음)으로 오르는 70~85° 사면이다. CloseRange는 평지에서 배치한 것이라 드레이프가 사면 위 인스턴스도 현지 Up으로 세웠다. 그래서 지면 타일이 수평 판으로 층층이 겹쳤다.
- `drape`에 경사 규칙(`DRAPE_SLOPE`)을 더했다. 지각 법선은 1m 격자 점의 중심 차분으로 구하고 점은 캐시한다(지각 높이 함수가 호출당 약 1ms). 인스턴스 스케일을 곱한 메시 상자 높이/수평 최대 폭이 0.35 미만이면 납작한 지면 타일로 보고, 데칼도 여기에 넣는다. `drape <버전> dry`는 레벨 없이 경사 대역·제외 집계만 `MassifDrape_<버전>_dry.json`에 쓴다.
- Drape V003(`Z1AssemblyWall_V012`): 납작한 인스턴스를 그 자리 지각 법선으로 눕히고 45°를 넘으면 뺀다. 39,394개 중 35,350개를 남겼다. 뺀 것은 Ditch 3,015개(ForestGround 1,432·GrassyEmbankment 866)와 Ground 922개 등이다. 북 사면 선반과 회색 데칼 사각형은 사라졌다. 남 사면 협곡에는 60° 넘는 자리에 세운 암석·식생 3,616개(Amaryllis 479·LichenedForestBoulder 387·MassiveSandstoneCliff 04 58 등)가 기둥처럼 붙었고, 꼭대기 판 하나는 하늘로 튀어나왔다.
- 회전을 반영한 납작함(돌린 상자의 Up 폭/수평 폭)은 dry로만 보고 기각했다. ED가 기울여 둔 둑 조각(GrassyEmbankment)이 오히려 납작하지 않은 것으로 빠졌고, MassiveSandstoneCliff 04는 여전히 세운 쪽이었다.
- Drape V004(`Z1AssemblyWall_V013`): V003에 더해 경사가 60°를 넘는 자리의 인스턴스는 종류와 관계없이 뺀다. 31,734개를 남겼다(Ground 13,348·SmallAssembly 96·Props 51·Ditch 18,239). 남북 사면의 선반과 협곡 기둥이 모두 사라졌고, 사면은 지각 절벽 재질로 드러난다. 능선 꼭대기의 판 몇 개와 고원 동쪽 가장자리 지면 타일 테두리는 남았다.
- 화면은 V011과 같은 부감(`look 1500 -1500 9000 -6500 300 -2500`)의 Unlit이다. 근거는 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Shots/`(`Compare_V011_V012_V013.png`, `V012_SouthSlope.png`)와 `MassifDrape_V003.json`·`V004.json`·`V003_dry.json`이다.
- 사용자 판정(2026-11-20): V013 채택. 기준 시험본을 `Z1AssemblyWall_V013`으로 바꿨다.

### 하상 자리 지각 골(Z1MassifCover V003)

- 원인: ED CloseRange 맵에서 Ditch 스플라인(닫힌 고리, 길이 273m)을 2m마다 좌우 ±14m 단면 트레이스했다. 하상 바닥은 원점 −6~+10cm였고 둑만 1~5m 솟았다. 하상 띠(원점 +40cm 미만)는 폭 5~7m이고 스플라인 왼쪽 법선으로 약 4.5m 치우쳤다. 하상이 고원 아래에 묻힌 것이 아니다. 얇은 바닥 메시(NordicBeach·진흙 웅덩이 평면 `MI_MudPuddle`)가 드레이프 뒤 고원 지각과 같은 높이에 놓여 지각에 덮였다.
- `RidgeZ1CompositionSpec`: `massif_cover_weight`를 분리했다(고원 결과는 V002와 같음, 무작위 2,000점 대조). `CLOSERANGE_CHANNEL_CR`은 트레이스에서 뽑은 하상 띠 중심 69점(4m 간격, CR 원점 기준)이다. `closerange_to_local`은 드레이프와 같은 평면 배치로 옥탄트 로컬에 옮기며 UE `compose_transforms`와 오차 0.00cm였다. `massif_channel_ground`는 V002 고원에서 반폭 4.5m·가장자리 2m·깊이 50cm로 판다. 고원 가중치 0.8 미만(남북 사면·동굴 절개 둘레)은 파지 않는다. 4m 격자로는 골이 뭉개져 띠 둘레에 1m 간격 특징점을 더했다.
- Blender `BuildRidgeZ1Composition.py -- massif-channel` → `Z1MassifCover_V003`: 82,691삼각형(V002 80,809), 비다양체·불일치·중복·퇴화 0, 경계 변 1,385개로 같다. UE `importcrust V003`의 Meadow 경계 최대 보정은 0.0076cm다.
- 드레이프 배치 높이는 계속 파지 않은 V002를 쓰고(Drape V004 그대로), 렌더링 지각만 V003으로 바꾼 시험 레벨이 `Z1AssemblyWall_V014`다. 수직 부감에서 산괴를 감싸는 하상 고리가 ED 원본처럼 자갈 바닥 띠로 드러났고, 지각에 덮였던 진흙 웅덩이 평면이 어두운 회색 반점으로 보인다(Unlit). 기준 부감에서도 산괴 앞 하상이 띠로 읽힌다.
- 근거: `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`의 `CloseRangeChannelTrace.json`·`CloseRangeHeights.json`·`CheckChannelMapping.json`, `Shots/Compare_V013_V014_Channel.png`(ED 원본 부감 포함), 빌드 로그 `Saved/SliceOctant/Ridge_00/BuildZ1MassifCover_V003.log`.
- 사용자 판정(2026-11-20): V014 채택. 기준 시험본을 `Z1AssemblyWall_V014`로 바꿨다.

### V014 Lit 판정

- 뷰포트를 Lit으로 바꿔 TestMap03 조명(구 중심 점광원)에서 기준 부감·수직 부감·하상 사선·분지 정면 눈높이(`look 4500 -400 170 -1700 -300 1800`)를 찍었다. 근거는 `Shots/V014_Lit_*.png`와 묶음 `V014_Lit_Review.png`다.
- 산괴·식생·하상은 Lit에서도 잘 읽힌다. 하상 사선에서는 자갈 하상에 작은 물웅덩이가 자연스럽게 보이고, 50cm 판 지각이 바닥 메시 틈으로 드러난 곳은 보이지 않았다. 수직 부감에서는 웅덩이가 하늘 없이 어둡게 반사되어 ED 원본보다 눈에 띈다.
- 회색 데칼: Lit에서는 회색 사각형이 보이지 않았다. 하상 둘레의 작은 흰 사각형은 `ShowFlag.BillboardSprites 0`에서 사라지는 에디터 데칼 아이콘이다.
- 고원 동쪽 가장자리의 지면 타일 테두리(ForestGround)는 Lit 부감에서 고원 밖 허공으로 튀어나와 아래에 큰 그림자를 드리워 Unlit보다 훨씬 두드러진다. 분지 정면 눈높이에서는 02 벽 위에 가려 보이지 않는다.
- 조명: 분지 정면에서 02 벽 정면이 대부분 그늘이고, 오른쪽(북동)에 재질 구역 밖 V012 지각의 보라 평지색이 크게 보인다. 둘 다 남은 문제로 넘긴다.

### 고원 동쪽 테두리(Drape V005·Z1MassifCover V004)

- Drape V005(`Z1AssemblyWall_V015`)는 V004에 더해, 납작한 타일 상자 바닥 네 모서리 아래 지각이 중심 접평면보다 1m 넘게 꺼지면 뺐다. 211개(ForestGround 68·RockFormation 32 등)가 빠졌지만 Lit 부감의 테두리는 거의 그대로였다. 기각.
- 원인: ED 원본을 옆에서 보니 CloseRange는 떠 있는 섬이고 테두리 아래가 원래 허공이다(`Shots/ED_CloseRange_EastRim.png`). CR 인스턴스상 ForestGround 4,483개는 모두 원점 약 +3m(p10 293·p90 306cm)에 있다. 하상 바깥 둑 고리 전체가 +3m 단이고, 그 위 타일이 V002 고원 3m 위에 떠 있었다.
- 지지면 높이장: ED 맵에서 산괴를 빼고 1m 격자 11,970칸을 위에서 트레이스했다. ForestGround에 충돌이 없어 68%가 빗나갔다. 그래서 `Scripts/Profiling/Graphics/BuildCloseRangeSupport.py`가 납작한 타일(16,762개)의 상자 윗면을 직접 래스터화하고, 둑·하상 암석 트레이스(식생 제외 3,730칸)와 합쳐 지지면 5,354칸을 만든다. 지지면 칸은 '지지면 − 50cm', 없는 칸은 둘레 지지면에서 45°로 내려오되 0 아래로는 내리지 않는다. 결과는 `CloseRangeSupport_V001.json`(CR 원점 기준 105×114, Git 추적)이다.
- `RidgeZ1CompositionSpec.massif_island_ground`: V002 고원에 이 오프셋(역변환 `local_to_closerange`, 왕복 오차 0)을 더한다. 하상 띠에서는 V003처럼 50cm 아래를 넘지 않고, 고원 가중치 0.8 아래에서는 반영하지 않는다. 특징점은 하상 띠에 더해 오프셋이 이웃과 20cm 넘게 다른 칸을 1m 간격으로 넣는다.
- Blender `-- massif-island` → `Z1MassifCover_V004`: 86,459삼각형, 비다양체·불일치·중복·퇴화 0, 경계 변 1,385. UE 반입 Meadow 경계 최대 보정 0.0076cm. `Z1AssemblyWall_V016`은 V014(Drape V004)에서 지각만 V004로 바꿨다.
- V016 Lit: 동쪽 테두리 아래가 흙 경사로 받쳐져 허공에 뜬 판과 큰 그림자가 사라졌다. 대신 테두리 타일 사이 틈으로 50cm 아래 지각이 밝은 연두(지면 재질)로 비친다. 테두리 양 끝과 산괴 왼쪽 위에는 타일 없이 올라온 지각 면이 드러났다. 지지면은 있으나 드레이프가 급경사로 뺀 자리로 보인다. 비교는 `Shots/Compare_V014_V016_Rim.png`다.
- 사용자 판정(V016): 공중에 뜬 느낌은 완전히 없어졌다. 대신 바깥 테두리의 둥근 ForestGround 타일 가장자리가 바로 밖에서 내려가는 지각 위로 살짝 튀어나와 버섯 갓처럼 둥근 그림자가 진다(`Shots/V016_User_Rim.png`, 사용자 카메라는 `Cameras.json`의 `UserRim`).
- 지지면 높이장 V002(`BuildCloseRangeSupport.py V002`): 50cm 격자, 타일 발자국을 상자 중심 기준 80%로 줄였다. 격자 테두리에서 빈 칸끼리 이어진 섬 바깥 칸은 지지면 +10cm를 1m 폭으로 유지한 뒤 45°로 내린다. 섬 안쪽 빈 칸은 V001처럼 지지면 −50cm에서 내린다. 빌더는 버전별 매개변수를 갖고, V001 매개변수로 다시 만든 높이장은 V004 지각 표본값과 같았다.
- `Z1MassifCover_V005`(Blender `-- massif-island-v005`): 95,459삼각형, 위상 결함 0, 경계 변 1,385. `Z1AssemblyWall_V017`은 V016에서 지각만 V005로 바꿨다. 같은 사용자 시점에서 버섯 갓 그림자가 사라지고, 바깥 타일 띠가 자갈 흙 지각 둔덕에 묻혀 흙 둔덕 가장자리로 읽힌다. 지각이 바깥 타일 띠를 1m 안팎 넓게 덮었고, 타일과 지각 경계 일부에 50cm 격자 톱니가 보인다(`Shots/Compare_V016_V017_Rim.png`).
- 사용자 판정(2026-11-20): V017 채택. 기준 시험본을 `Z1AssemblyWall_V017`로 바꿨다.

## 2026-11-20 — 1-BR U4 Z1 지각 재질 후보(CloseRange 지면)

### CloseRange 지면 재질 조사

- 덤프 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/DumpSurfaceMaterials.py` → `SurfaceMaterials.json`(읽기 전용). 대상은 산괴 지각 재질 체인과 CR 지면·둑·하상 메시 10종이다.
- 산괴 지각 재질(`Z1MassifSurface_V001`)의 실제 층은 세 가지다. 지면은 MossyRockyGround BC·N·AoRDp(400cm)를 TextureObject로 받아 V005 삼각 격자 함수(`MF_RidgeTriangleTexture/Normal`)로 샘플링한다. 절벽은 BeachCliff(2,000cm)를 2탭 함수로, 하상은 DryRiverBed를 쓴다. 좌표는 `MF_RidgeSurfaceCoordinates`의 옥탄트 로컬 좌표·현지 Up이라 구면에서 바르게 동작한다. 원본 ED 랜드스케이프의 JungleGround·MossyGrass 가지는 사본에 남아 있지만 출력에 연결되지 않는다.
- CR 지면 메시(`SM_ForestGround_01` 72×47×6cm, GrassyEmbankment, DryRiverBed 계열 `SM_RockFormation_04` 등)는 Megascans 3D 스캔 재질 `M_MS_Default_Material_VT_DynamicLayering`의 MI다. 4K BC·N·DpRF는 메시 고유 UV 아틀라스라 지각에 반복 투영할 수 없다. 이끼·지의류·산화 층은 월드 공간 마스크(`Mask Tiling` 950 등)로 칠하므로, 월드 Z≠Up인 구면에서 맞는지는 확인하지 않았다.
- 반복 가능한 Megascans 표면은 LootNPop과 ElectricDreamsEnv가 같은 12종이다. ForestGround의 갈색 낙엽·잔가지 흙에 가장 가까운 것은 DriedGrassOnDampSoil, 다음은 JungleGround다(`Shots/SurfaceCandidates_A.png`).

### 지면층 교체 시험(Z1MassifSurface V002·Z1AssemblyWall_V018) — 기각

- `BuildRidgeZ1AssemblyTest.py surface V002`는 V001 부모·MI를 `Z1MassifSurface_V002/`로 복제하고, MossyRockyGround 텍스처 노드 6개(TextureObject 3·남은 TextureSample 3)를 DriedGrassOnDampSoil로 바꾼다. 절벽층·하상·구역/경사·MI 값은 V001과 같다. 버전 표는 `SURFACE_VERSIONS`이고 결과는 `MassifSurface_V002.json`이다.
- `wall V018`은 V017과 지각 슬롯 0 재질만 다르다(`AssemblyWallLevel_V017.json`·`V018.json` diff 한 줄).
- 첫 실행에서 폴더 이름 치환이 상위 `EDTerrain_V001`까지 바꿔 `EDTerrain_V002/` 아래에 재질 2개가 생겼다. 그 2개만 지우고(`DeleteMisplacedSurface.py`) 경로 계산을 고쳐 다시 만들었다.
- 같은 Lit 부감(`look 1500 -1500 9000 -6500 300 -2500`)과 근경(`Cameras.json`의 `UserRim`)을 비교했다. 근경에서 V018 테두리 둔덕은 잔가지·마른 풀이 섞인 갈색 흙이다. V017의 회베이지 자갈 흙보다 위쪽 ForestGround 타일 띠와 잘 이어지지만, 타일보다 조금 어둡고 붉다. 부감에서는 둔덕과 그 아래 사면이 짙은 갈색이 되고 V017 테두리 왼쪽의 연두 이끼 얼룩이 사라진다. 화면 대부분을 차지하는 절벽층은 같다.
- 근거: `Shots/Compare_V017_V018_Overview.png`·`Compare_V017_V018_UserRim.png`와 각 원본 `V017_Lit_*.png`·`V018_Lit_*.png`.
- 사용자 판정(2026-11-20): 기각, V017 유지. 처음에는 색 차이가 걸려 MI 값 조정을 고려했다. 다시 보니 텍스처 모양부터 V017이 더 어울렸다. V018의 마른 풀 무늬는 늪지대 같은 느낌이라 MI 조정 시험도 하지 않는다. 기준 시험본은 그대로 `Z1AssemblyWall_V017`이다. V018·`Z1MassifSurface_V002`는 비교본으로 남긴다.

## 2026-11-20 — 1-BR U4 Z1 조명·구역 밖 재질(V019~V021, GX-025)

V017 Lit에서 남은 세 문제(02 벽 정면 그늘, 북동 보라 평지색, 수직 부감의 어두운 웅덩이)를 원인부터 확인했다. 조사 스크립트와 결과 JSON은 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`의 `ProbeLighting`·`ProbeSurfaceScope`·`ProbePuddle*`이다(읽기 전용). 실험용 `SetSky.py`·`MoveCrust.py`·`TempSun.py`·`CenterShadow.py`는 V019를 잠시 바꾼 뒤 원래 값으로 되돌렸다. 시점 인자는 `ShotViews.md`에 있다.

### 02 벽 정면 그늘 — TestMap03 조명 구조

- 시험 레벨 조명은 TestMap03 사본이다. 구 중심 점광원은 1.7e7cd(그림자 켬, 감쇠 반경 40,000cm)이고, 보조 점광원 4개는 각 1.1e6cd(그림자 끔, 45,000cm)다. PostProcessVolume은 노출 EV100 6.0 고정(최소=최대, 바이어스 0)이고 GI·반사는 Lumen, 스카이라이트 누출은 0이다. Sky Light·방향광·대기는 없다. 메모리에 남은 값(1.0e7cd·EV 6.2)과 다르다.
- 02 벽 발치(로컬 x −2,000, 지표 위 15m)에서 다섯 광원 방향과 현지 Up의 cos는 0.93~1.00이다. 광원이 모두 구 안쪽이라 어느 지표에서나 거의 머리 위에 있다. 분지를 보는 벽 정면 법선(현지 동쪽)과의 cos는 중심 0.00, Warm_A −0.07, Cool_B −0.10, Cool_C +0.27, Warm_D −0.27이다. 정면을 비추는 광원은 Cool_C 하나이고, 그마저 거리 40,900cm에서 감쇠 창이 0.10이다.
- 중심 점광원은 감쇠 반경(40,000cm)이 지표 거리(32,400~33,900cm)에 가까워 InverseSquared 감쇠 창 (1−(d/r)⁴)²이 0.24~0.32다. 바닥 직접광은 약 35lux이고, 벽 정면은 Lumen 바닥 반사광만 받는다. Sky Light가 없어 앰비언트와 Lumen 추적 실패 시 하늘값이 0이다.
- 판단: 시험 레벨만의 문제가 아니라 TestMap03 조명 설계(구 안쪽 광원 + 하늘 없음)의 결과다. 모든 옥탄트의 수직 벽이 같은 조건이다.
- 시험 레벨 대응: 균일 회색 큐브맵(`/Engine/EngineResources/GrayTextureCube`) Sky Light를 더한다. 균일 큐브맵은 방향과 무관해 구면에서도 쓸 수 있다. 아래 반구 검정(기본 true)은 월드 −Z 쪽을 지우는데, Ridge의 현지 Up은 월드 −Z 성분을 가지므로 끈다.
- 세기 비교(분지 정면, 8비트 평균 벽/바닥): 0 → 9.2/50.2, 8 → 9.5/52.2, 30 → 10.9/62.1, 50 → 15.9/70.7. 100은 바꾼 직후 찍어 14.4/63.2로 덜 수렴했다(Lumen 누적, 이후 촬영은 두 장째를 쓴다). 50에서 벽 정면이 읽히고 바닥 밝기가 약 2배가 된다. 400은 웅덩이 시험(수직 부감)에서만 썼다.
- `wall V019`: V017 + `SkyLight_UniformGray`(지정 큐브맵, Movable, 세기 50). 처음 세기 8로 만든 뒤 레벨 안에서 비교하고 50으로 맞춰 저장했다(`AssemblyWallLevel_V019.json`에 기록). 스펙 `WALL_SKY`도 50이다.

### 북동 보라 평지색 — 재질 구역 밖 상수색

- `M_Ridge_Z1_MassifSurface`(V001)는 구역 Custom 노드가 0인 곳에서 기본색·법선을 재질 상수(보라 0.29, 0.23, 0.38)로 칠한다. 구역은 `MF_RidgeSurfaceCoordinates`의 1-B0 다각형(x −3,200~9,000, y −4,300~3,800, 이음매 제외)과 V001이 더한 서쪽 직사각형(x −14,000~−1,500, y ±8,000)뿐이다. 분지 북쪽 오름 사면(y > 3,800)과 남·동쪽 V012 지각은 모두 구역 밖이다.
- 1km 격자 지도로 보면 이음매 띠(세 이음매 평면까지 메시 로컬 거리 5,500cm 미만 = 보존 15m + 전이 40m)는 옥탄트 가장자리에만 있고 Z1 시야에서 멀다.
- 판단: GX-021의 교체 범위는 경계 전이부를 뺀 Ridge 지표 전체다. 그래서 구역 밖 기본색을 바꾸지 않고, 구역을 원래 이음매 제외만 남긴 옥탄트 전체로 넓혔다. 이음매 띠는 교체 대상 밖 표시로 평지색을 유지한다.
- `surface V003`: V001 사본의 구역 노드에 메시 로컬 위치 입력을 더하고 `step(5500, min(P.x, P.y, P.z))`로 바꿨다. 하상 가중치는 원래 1-B0 구역 그대로다. 핀을 바꾸는 중간 단계에서 컴파일 실패 경고가 두 번 났지만 연결 뒤 최종 컴파일은 성공했고, 화면에서도 기본 재질로 보이는 곳이 없다.
- `wall V020`: V019에서 지각 재질만 Surface V003으로 바꿨다. 분지 정면 오른쪽 위와 수직 부감 북·동쪽의 보라 지각이 사암 절벽·지면으로 바뀌었다. 부감 원경에는 이음매 띠의 보라와 보존 구조물(동굴·부유섬·석교, 자체 재질)의 보라가 남는다.

### 수직 부감 어두운 웅덩이 — 재질 컴파일 실패

- 웅덩이는 Drape `Ground` 그룹의 `SM_WaterPlane_01` 46개(×5 스케일, `MI_MudPuddle_01`)다. 부모 `M_MudPuddle_01_PCG`는 Substrate Single Layer Water BSDF이고 거칠기 0.02, 기본색은 진흙물 텍스처 + 거의 검은 색, 불투명도는 아래 표면까지 깊이에 비례한다.
- 기각된 가설: 하늘 반사(스카이 0·100·400에서 웅덩이/주변 자갈 선형 밝기 비 0.40·0.48·0.51로 포화), 방향광 부재(임시 방향광 30lux에서 0.48), 하상 띠 50cm 골(웅덩이는 고원 −6cm, 렌더링 지각은 그 44cm 아래다. 지각을 30cm 올려도 그대로), 중심 광원 그림자(꺼도 0.39), SLW 렌더링(`r.Water.SingleLayer 0`에서도 어두움).
- 원인: 3m 근접 시점에서 웅덩이가 회색 체커로 그려졌다. 로그에 `Failed to compile Material M_MudPuddle_01_PCG for platform PCD3D_SM6, Default Material will be used`가 있고, 오류는 `RayTracingMaterialHitShaders.usf`의 `GetSingleLayerWaterMaterialOutput0~2` 미정의다. `r.RayTracing=True`면 SLW 재질의 히트 셰이더가 `Single Layer Water Material` 출력 노드를 요구하는데(엔진 주석 `SUBSTRATE_TODO`), ED 재질에는 그 노드가 없다. ED 맵 첫 로드 때 보인 같은 오류도 이것이다. ED 원본 부감에서 웅덩이가 안 보인 것은 밝은 햇빛 아래 기본 재질 체커가 자갈처럼 보였기 때문이다.
- `puddle`: ED 원본은 두고 `Z1Puddle_V001/`에 재질·MI 사본을 만들어 출력 노드만 더했다(입력 미연결. Substrate에서는 허용되고 히트 셰이더용 기본값이 들어간다. 래스터 셰이딩은 기존 BSDF가 맡는다). 컴파일 실패 경고는 없었다.
- `drape V006`: V004 규칙 그대로, 실제로 쓰는 재질이 `MI_MudPuddle_01`이면 사본으로 바꾼다(`DRAPE_MATERIAL_SWAP`). 인스턴스 수는 V004와 같고(Ground 13,348·SmallAssembly 96·Props 51·Ditch 18,239), 웅덩이 46개가 바뀌었다(`MassifDrape_V006.json`).
- `wall V021`: V020에서 드레이프만 V006으로 바꿨다. 근접 시점에서 체커가 사라지고 얕은 물 아래로 바닥이 비치며 중심 광원 반사광이 맺힌다. 수직 부감의 검은 반점은 사라졌다.

### 비교와 판정

- 시점은 기준 부감·분지 정면·수직 부감·웅덩이 근접이다. 묶음은 `Shots/Compare_V017_V019_Light.png`(조명), `Compare_V019_V020_Scope.png`(구역), `Compare_V020_V021_Puddle.png`(웅덩이), `Compare_V017_V021_All.png`(전체)이고 원본은 `V017·V019·V020·V021_Lit_*.png`다.
- 사용자 판정(2026-11-20): V021 채택. 기준 시험본을 `Z1AssemblyWall_V021`로 바꿨다(GX-025).
- TestMap03에 같은 Sky Light를 넣는 안은 사용자가 보류했다. 넣으면 모든 옥탄트 밝기와 VFX 대비가 바뀐다. 대안으로 옥탄트별 비스듬한 주광을 LVI에 싣는 안도 기록만 했다. 게임 맵 조명은 별도 단위다.

## 2026-11-20 — 1-BR U4 Z1 정면 구도(분지 저시점, V022~V023, GX-026)

지금까지 Z1은 부감과 분지 정면(62m) 한 장으로만 판정했다. 이번에는 분지 바닥 눈높이(구면 현지 Up 기준 170cm)에서 서쪽 산괴와 02 벽을 처음 봤다. 시점 기록과 보조 스크립트(`LowView.py`), 촬영본은 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`의 `ShotViews.md`와 `Shots/`에 있다(로컬).

### 저시점 선정

`RidgeBlueprint_V001.json`의 Pod `DeepBasin`과 분지에 닿는 동선(`CanyonRun`·`HallFromBasin`)을 기준으로 정했다. 인자는 모두 `BuildRidgeZ1AssemblyTest.py look`이다. 분지 바닥은 벽 발치(x −1,800)까지 V002 기반면 −3,950cm 근처로 평탄하다.

| 이름 | look 인자 | 근거 |
|:---|:---|:---|
| BasinPod | `3300 -300 170 -2000 -300 1200` | Pod 중심에서 서쪽 02 벽 정면 |
| CanyonArrival | `1200 -2600 170 -2000 600 1200` | CanyonRun 분지 진입점에서 북서. 화면 왼쪽이 북 테두리 |
| HallGate | `2500 1300 170 -2000 -1600 1200` | HallFromBasin 출발점에서 남서. 화면 오른쪽이 남 테두리 |
| WallFoot(보조) | `300 -300 170 -2000 -300 900` | 벽 발치 23m 근경 |

화면 좌우가 어느 쪽인지는 에디터 `WorldPosToScreenCoords`로 남쪽 지점(2500, −5000)을 투영해 확인했다. 옥탄트 로컬 (x, y, Up)은 UE와 같은 손 방향이라 시선 오른쪽이 (−f.y, f.x)다.

### V021 저시점 문제 목록

고원 위(드레이프 타일·하상 경계·사면 선반)는 분지에서 보이지 않는다. 저시점에서만 드러난 문제는 다음과 같다. 근거는 `Shots/V021_Lit_{BasinPod,CanyonArrival,HallGate,WallFoot}.png`와 묶음 `V021_LowView_Review.png`(시점 지도 포함)다.

1. 분지 남북 테두리가 맨 V012 지각이다. 분지는 남북 폭이 약 75m라 높이 40~60m의 각진 기반면이 정면 화면의 절반 이상을 차지한다. 02 벽은 35~45%뿐이다. 테두리 서쪽 끝(x < −2,200)은 고원 남북 끝 절개 사면으로 이어진다.
2. 02 벽 반복: 같은 계열 3개가 "기둥 두 개 + 세로 홈" 리듬을 세 번 되풀이하고 꼭대기 높이가 같아 일렬 담장처럼 읽힌다.
3. 왕관 접합부: 기둥 꼭대기 사이 V자 틈으로 검은 하늘이 보인다.
4. 스카이라인: 02 벽(조립체 경계 높이 49~53m) 위로 고원 산괴가 거의 안 보인다. 벽 위는 왕관 식생·검은 하늘·보라 보존 구조물(V011 형상)이다. 설계도 사선 `DeepBasin → LandmarkTop 보임`의 대상도 다시 정해야 한다.
5. 벽 발치 잔돌 판: 조립체 원본 바닥 타일·바위 묶음이 평평한 지각 위에 얇은 판으로 얹혀 두께 단이 보인다.
6. 분지 바닥이 넓은 단색 지각이다(시험 레벨에 1-B5 장식 없음).

사용자는 2·3번만 이번에 고치기로 했다. 1·4·5·6번은 다음 단위 항목으로 남긴다.

### V022 — 벽 yaw·높이 차 + 왕관 기둥 두 개(기각)

- 세 벽을 yaw 남 +10°·가운데 −8°·북 +6°로 틀고, 가운데 벽을 2.5m 올리고 북쪽 벽을 3.5m 묻었다. 두 접합부 뒤 고원(x −3,200)에 `01b`(y −1,450)·`01c`(y 1,050)를 고원 아래 15m 묻어 세웠다. 01 계열은 암석을 공유하므로 yaw를 +20°·−25°로 달리 했다.
- 결과: Front에서 01 기둥 두 개가 벽 위 왕관이 되어 하늘 틈이 줄었다. 그러나 가운데 벽 뒤(두 기둥 사이)에 틈이 남았고, 가운데 벽을 올린 탓에 발치 잔돌 판 아래가 떠 검은 빈 공간이 생겼다. 근거 `Shots/V022_Lit_{BasinPod,Front}.png`.

### V023 — 벽은 묻기만 하고 왕관 셋의 높이를 다르게(채택)

- 앞 벽 yaw는 V022와 같고 높이는 남 −1m·가운데 −1.5m(V021 값)·북 −4.5m다. 벽은 올리지 않는다.
- 왕관: 남 `01b`(x −3,200, y −1,450, yaw 20°, 고원 −15m), 가운데 `02b`(x −3,600, y −300, yaw 170°, 고원 −25m), 북 `01c`(x −3,200, y 1,050, yaw −25°, 고원 −10m). 꼭대기 높이는 북 > 남 > 가운데다.
- 재생성: `Z1AssemblyTest_V001`을 연 뒤 `wall V023`. 나머지(CR LargeAssembly·Drape V006·지각 V005·Surface V003·Sky Light)는 V021과 같다.
- 결과: Front·BasinPod에서 앞 벽 위로 바위·식생 왕관이 한 단 더 서서 두 겹 산괴로 읽히고, 하늘 틈이 막혔다. 북쪽 벽이 낮아져 꼭대기 선도 갈린다. 사선 시점(CanyonArrival·HallGate)은 차이가 작다. 부감에서는 고원 앞 가장자리에 왕관 기둥이 보여 바위·나무가 늘었다. 같은 계열 앞 벽 3개의 반복감은 완전히 사라지지 않았다.
- 비교 묶음 `Shots/Compare_V021_V023_LowView.png`(Front·BasinPod·CanyonArrival·HallGate·Overview), 원본 `V023_Lit_*.png`.
- 사용자 판정(2026-11-20): V023 채택. 기준 시험본을 `Z1AssemblyWall_V023`으로 바꿨다(GX-026).

## 2026-11-20 — 1-BR U4 Z1 남북 테두리(V024~V029, GX-027)

V023 저시점 문제 1번(분지 남북 테두리 + 고원 남북 끝 절개 사면)을 다뤘다. 보조 스크립트(`RimView.py`·`ShotSeries.py`·`CollectShots.py`·`CompareShots.py`·`MeasureCRPieces.py`)와 촬영본·JSON은 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`에 있다(로컬).

### 현장 확인

- V005 렌더링 지각 높이표: 분지 바닥 −40m, 북 테두리는 x −2,000~1,000에서 y 3,000→4,000 사이에 +20m(60m 단), 남 테두리는 y −4,800→−6,000 사이에 −32→−18→−3m 두 단(약 35m)이다. 북동은 동굴 입구(HallFromBasin, x≈2,500)와 NorthLedge 길이 지난다.
- 부감 좌우의 거대한 각진 기둥은 테두리가 아니라 고원 남북 끝 V012 능선(남 (−4,000, −5,000) +50m, 북 (−3,000, 4,500~5,000) +54~57m)이다. 부감 가운데 오른쪽 기둥은 석교 교각(V011 보존 구조물)이라 범위 밖이다.
- 테두리 정면 시점 두 곳을 더했다: 북 `look 1500 -2500 170 500 4500 2500`, 남 `look 1500 2000 170 500 -5500 1500`(이후 남 둑 근경 `look 2500 -2500 170 2900 -5300 1000`). 한 시점당 두 장을 자동으로 찍는 `ShotSeries.py`(슬레이트 틱 콜백으로 4초·3초 대기)를 썼다.

### V024 — 지각 그대로 + 조립체 9개(기각)

- 높이 기준 `abs`(절대 H)를 더하고, 남 테두리 발치 02 계열 3개, 협곡 서안 01b, 능선 면 02c·02e, 북 테두리 02c + 왕관 01c, 북동 02b를 세웠다(`RIM_V024`).
- 결과: 남 테두리(약 35m)보다 조립체(49~52m)가 높아 사면을 덮지 못하고 따로 선 기둥 3개로 읽혔다. 능선 기둥은 아래쪽만 가려졌다(`Shots/Compare_V023_V024_Rim.png`). 사용자는 B안(능선 깎기 + 테두리 물리기 + 조립체)을 골랐다.

### V025 — 지각 V006 + 테두리 조립체(테두리만 기각)

- `RidgeZ1CompositionSpec`: `ridge_cap_weight`·`RIDGE_CAP_CM`(남 650·북 2,000cm)으로 고원 남북 끝 능선을 깎는다(섬 지형·동굴 절개 둘레 제외). `RIM_FACES`·`rim_recess`는 테두리 앞면 선 앞 사면을 분지 바닥으로 눌러 발치를 앞면 3m 뒤로 물린다. Blender `-- massif-rim` → `Z1MassifCover_V006`(95,605삼각형, 비다양체·퇴화 0, 경계 변 1,385, Meadow 경계 최대 보정 0.0076cm).
- 남 테두리 02 계열 4개를 약 19m 간격으로 겹쳐 꼭대기를 테두리 꼭대기에 맞춰 10~20m 묻고, 북은 벽 + 왕관 2단, 능선 자리에 단독 조립체 3개를 세웠다. V002 하구 조각 3개(`Outlet*`)는 뺐다(`WALL_REPLACED_EXTRA`).
- 결과: 남 테두리가 이어진 암벽 띠가 됐고 북쪽 능선 기둥이 사라졌다(`Shots/Compare_V023_V025_Rim.png`).
- 사용자 판정: 테두리를 같은 계열 조립체로 반복한 긴 절벽(`RimSouth*`·`RimNorth*`)은 확실히 어색하다. 앞면 02 벽 연속 배치도 아슬아슬했다. 조립체는 원래 긴 절벽을 표현하는 수단이 아니라 가끔 변칙으로 쓰는 수단이다. 긴 절벽 지형을 다른 형태로 바꾸거나 다른 표현 수단을 찾는다. 긴 절벽 외의 부분(지각 V006, 능선 자리 단독 조립체, 하구 조각 제외)은 괜찮다.

### ED가 긴 형상을 만드는 방식

- CloseRange 둑(Ditch, 둘레 약 270m)은 큰 조립체가 아니라 중간 크기 암석 수백 개를 하상을 따라 촘촘히 깔고 둑 메시·식생으로 덮었다. `MeasureCRPieces.py`(→ `CRPieces.json`, 203메시·18,737행) 실측: MassiveSandstoneCliff_04 313개(스케일 0.12~0.36, 높이 3.6~10m, 기울기 8~56°), HugeSandstoneCliff_07 245개(약 1m 판), HugeSandstoneCliff_04 187개(3.3~4.8m), SandstoneCliff_01·03 192개(2~4m), GrassyEmbankment 2,243개. 같은 메시 최근접 간격은 1.5~2m다.
- CloseRange 산괴(46m)는 수직 벽이 아니라 둑·암석층·식생으로 덮은 둔덕이고 01b는 강조용이다. 본 맵은 랜드스케이프 경사면이 형상을 맡고 절벽 조립체는 듬성듬성 얹는다.

### V026 — 둑 묶음 띠를 급경사 면에(기각)

- `band <버전>`: CR_Ditch 인스턴스를 읽어 지지면 경사가 기준 이상인 암석·둑 인스턴스를 묶음 중심으로 삼고, 반경 3m 안의 인스턴스(식생 포함)를 테두리 지각 표본점마다 옮긴다. 원본 좌표계는 하상 중심선 접선·ED 섬 지지면(V002 높이장 + 50cm) 법선·하상에서 멀어지는 오르막이고, 목표는 앞면 선 접선·지각 법선·분지에서 멀어지는 오르막이다. 식생은 현지 Up으로 다시 세운다. 결과는 월드 좌표 `Z1RimBand_<버전>` 레벨이다.
- 띠 V001(V006 지각, 간격 450cm): 표본점 175·인스턴스 46,256. `Z1AssemblyWall_V026`은 V025에서 테두리 조립체 7개를 빼고 띠를 넣었다.
- 결과: 판·돌무더기가 벽에 붙은 것처럼 보였다. 원본 묶음 경사 중앙값 38°, 목표 면 중앙값 73°(60~87°)로 경사가 맞지 않았다. ED 둑 조각은 완만한 둑을 덮는 도구다.

### V027 — 남 테두리 둑 3단 + 띠(방향 채택)

- `RIM_TERRACES`·`rim_terrace`: 남 앞면 선 앞 10m에서 시작해 둑(수평 10m)·단(4.5m)을 3번, 남쪽 대지(y≈−7,500)까지 오른다. 둑 하나는 약 12m·50°다. Blender `-- massif-terrace` → `Z1MassifCover_V007`(97,781삼각형, 위상 결함 0, 경계 변 1,385, 최대 보정 0.0076cm).
- 띠 V002(V007, 묶음 중심 경사 35° 이상): 표본점 216·인스턴스 53,038. 결과: 나무가 자라는 바위 둑으로 읽혔다(`Shots/Compare_V026_V027_Band.png`).
- 사용자 판정: 충분히 가능성 있다. 큰 바위를 섞고 밀도를 올린다.

### V028 — 밀도·큰 바위(판 회전 기각)

- 띠 V003: 간격 320cm(표본점 479, 남 326·북 153), 큰 바위 층 40개(원본 둑 큰 암석을 1.6~2.4배, 약 11m 간격, 높이 25% 묻음). 인스턴스 116,596.
- 사용자 판정: 밀도는 충분하다. 그러나 얇은 판 조각이 벽에 동전처럼 꽂히고, 한쪽 면만 있는 조각이 세워져 꽂혔다. 표면 법선을 지키는 선에서 회전 변화를 줘야 한다.

### V029 — 판 법선 정렬·재안착(채택)

- 띠 V004(`band_seater`, 표본·묶음 선택은 V003과 같음): 판 조각(인스턴스 스케일 반영 가장 얇은 축/가장 긴 축 < 0.4)은 얇은 축을 지각 법선에 맞춰 눕히고, 원본에서 지면 법선·하상 쪽을 향하던 면을 바깥으로 한다. 회전 변화는 법선 둘레 자유 회전과 ±6° 기울기이고 두께의 30%를 묻는다. 덩어리 바위·식생은 옮긴 수평 위치의 지각 높이(1m 격자 캐시)에 원본 지면 위 높이를 더해 다시 앉힌다.
- 결과: 동전처럼 꽂힌 판과 세운 한쪽 면 조각이 사라지고 둑 표면이 바위 지면 위 수풀로 읽힌다. 큰 판 바위도 눕혀져 큰 바위의 존재감은 V028보다 약하다(`Shots/Compare_V028_V029_Seat.png`, 전체 `Compare_V023_V029_Rim.png`).
- 사용자 판정(2026-11-20): 뷰포트에서도 훨씬 자연스러워졌다. V029 채택. 기준 시험본을 `Z1AssemblyWall_V029`로 바꿨다(GX-027).
- 남은 점: 북 테두리는 V006 급경사 면 위 띠(돌무더기)라 둑으로 바꿔야 한다. 남서 모서리 봉우리와 깎은 북쪽 단(+20m) 평지는 맨 지각이다. 둑 3단으로 남 발치가 분지 쪽으로 약 10m 나왔다. 띠 인스턴스 비용은 미측정이다.

## 2026-11-21 — 1-BR U4 Z1 북 테두리(V030~V033, GX-028)

V029 남은 문제 중 북 테두리 둑 전환을 다뤘다. 보조 스크립트(`RimView.py` 북 시점 3개 추가·`ShotSeries.py`·`CollectShots.py`·`CompareShots.py`)와 촬영본·JSON은 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`에 있다(로컬). 남서 모서리·북쪽 단 맨 평지는 사용자 판정으로 다음 단위로 넘겼다.

### 현장 확인 — 둑 5단이 들어가지 않는다

- V007 지각 등고 측정: 북 테두리는 x −2,100~1,700(약 38m)에서 분지 −39.5m(y≈3,300)가 2~6m 안에 +19m로 오르는 거의 수직 면이다. 뒤는 동굴 절개(BasinHall, x −600 동쪽·y 3,500~3,900부터, 홀 천장 −12m)와 NorthLedge(x 0~2,200), 서쪽은 고원(+6.5m)·북쪽 단(+20m), 앞은 분지다.
- 남쪽과 같은 50°·12m 둑으로 60m를 오르려면 5단·깊이 약 52m가 필요하다. 북쪽 단으로 파고들면 폭 14m 골짜기 양옆에 측벽이 생기고, 북서 모서리 중심 원형극장(시안 평면도 `Shots/Plan_FanA_Rejected.png`, `PlanView.py`·`PlanFanA.py`)은 동굴 지붕 옆 45m 측벽·고원 북향 절벽·좁은 목 웅덩이가 생겼다. 분지 쪽으로 내밀면 분지 북서 절반이 사라진다.
- 사용자 결정(B안): 분지 쪽으로 둑 2단만 덧대고, 위 급사면은 단독 조립체 하나로 덮는다. NorthLedge·동굴 입구는 손대지 않는다.

### V030 — 지각 V008 + 띠 V005·V006 + 01b

- `RIM_BANK_LINES`·`RIM_FILL_BANKS`·`rim_fill`: 급사면이 둑 맨 위 높이(≈ −15.7m)에 닿는 등고선을 따라 선을 두고, 선 앞(분지 쪽)으로 둑 2단(높이 11.9m·수평 9m·단 2.5m)을 쌓는다. 지금 지각보다 낮은 곳만 올리고 급사면은 깎지 않는다. 동굴 절개 2m 안은 건드리지 않는다. Blender `-- massif-bank` → `Z1MassifCover_V008`(98,487삼각형, 위상 결함 0, 경계 변 1,385, 최대 보정 0.0076cm).
- 띠 V005: V004와 표본·난수·seat 호출을 똑같이 하고 남 테두리만 놓는다(`place_lines`, 표본 479 동일, 79,136). 남 시점은 V029와 같게 나왔다. 띠 V006: 북 둑 선, 경사 25~65°(`max_slope`), 표본 84·21,220.
- 결과: V029 북 돌무더기 기둥이 사라지고 둑 2단이 생겼다. 그러나 01b는 기둥형이라 북쪽 단에 묻혀 뾰족한 끝만 보였고, 둑 맨 위 묶음이 반경 3m만큼 뒤 급사면으로 번져 돌 조각이 흩어졌다(`Shots/Compare_V029_V030_North.png`).

### V031 — 02c + 띠 V007

- 01b 대신 벽형 02c를 x −1,600·y 3,800(yaw −86, 바닥 H −2,900)에 묻어 세우고, 띠 표본을 둑 맨 위 3m 앞까지로 줄였다(V007, 17,697).
- 결과: 화면 가운데에 갈색 첨탑이 남았다. 조립체가 아니라 V007부터 있던 지각이었다. 동굴 절개 남서 모서리 옆 V012 바위 돌출(+30.8m, x −750·y 3,825)이 절개 둘레 7.5m 보호 구역 안이라 능선 상한이 닿지 않았고, V029에서는 북 띠 돌무더기가 가리고 있었다(`Shots/Compare_V029_V031_North.png`).

### V032 — 지각 V009 + 02c 서쪽 이동

- `SPUR_CAP`·`spur_cap`: 상자(x −1,800~400·y 2,800~4,800) 안에서 상한을 절개 경계 +25m(경계 최고 +24.3m 이상) → 4m 밖 +20m로 둔다. 상자 가장자리 6m에서 원래 지각으로 돌아간다. Blender `-- massif-spur` → `Z1MassifCover_V009`(위상은 V008과 같음). 최고점은 절개 경계 +24.9m.
- 02c를 x −1,950으로 옮겼다(동쪽 끝 x 약 −190). 띠 V008은 V007과 같은 표본이다.
- 결과: 첨탑이 낮아졌다. 동굴 지붕 가장자리 위에 아치 모양 바위가 떠 보였는데, 북 띠 큰 바위(MassiveSandstoneCliff 04 ×2.36)를 판으로 눕힌 것이 높이 12m 둑 밖 허공으로 삐져나온 것이었다. 동굴 입구 옆 언덕(x 약 1,400, 47~63°) 표본은 돌무더기가 됐다.

### V033 — 북 띠 V009(채택)

- 띠 V009: 북 큰 바위를 덩어리형(HugeSandstoneCliff 04·SandstoneBoulder 05·MossyForestBoulder 04) 1.4~2.0배로 줄이고 x 1,100 동쪽 표본을 뺐다(`x_range`). 표본 67·16,797(큰 바위 9). 남은 표본 위치는 V008과 같지만 묶음 선택은 난수 순서가 밀려 달라졌다.
- 결과: 아치가 사라지고 북 둑이 남쪽과 같은 바위 둑 위 수풀로 읽힌다. 남 시점은 V029와 같다(`Shots/Compare_V029_V033_North.png`·`Compare_V029_V033_Rest.png`). 띠 인스턴스 합은 V029 116,596 → V033 95,933이다.
- 사용자 판정(2026-11-21): V033 채택, 이 단위는 여기서 마감한다. 기준 시험본을 `Z1AssemblyWall_V033`으로 바꿨다(GX-028).
- 남은 점: 동굴 지붕 아래 동쪽 급사면(x −600~1,100)은 NorthLedge 때문에 V029와 같은 맨 지각이다. 북 발치가 분지 쪽으로 약 17~20m 나왔다. 02c 동쪽 끝이 홀 북서 모서리 위에 걸칠 수 있다(동굴 내부 미확인). V002 `NorthTalus`는 둑에 묻혀 뺐고 `NorthBankShelf`·`PillarShoulder`·`NorthCaveFace`는 북 둑과 겹치지 않아 후보로 남겼다.
- 도구 함정: 같은 Cmd 실행 안에서 `load_level` 직후 `wall`을 부르면 Level Instance가 아직 로드되지 않아 V002 배치를 못 읽는다(`assert kept and skipped`). 레벨을 연 다음 별도 실행으로 돌린다.

## 2026-11-22 — 1-BR U4 Z1 맨 지각 평지(V034, GX-029)

V033 남은 문제 중 남서 모서리·북쪽 단 맨 평지를 다뤘다. 보조 스크립트(`SurveyBare.py` 덮임 수집·수직 부감, `BarePlan.py` 덮임 평면도, `RimView.py` 맨 평지 시점 추가)와 촬영본·JSON은 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/`에 있다(로컬).

### 현장 확인

- V033의 모든 정적 메시·ISM 인스턴스(조립체 9개·CR 강체·드레이프 31,718·띠 95,933)를 옥탄트 로컬 좌표로 모아(`Coverage_Z1AssemblyWall_V033.json`) 지각 V009 경사 평면도에 겹쳤다(`Shots/BarePlan_V033*.png`·`CoverDots_V033.png`).
- 북쪽 단: x −7,900~−800·y 4,300~8,400이 25° 미만 +20m 평지이고 아무것도 덮이지 않았다. 남쪽 경계는 고원(+6.5m)으로 내려가는 단 사면, 북쪽은 V012 오름 사면(+37m)이다.
- 남서 모서리: 깎은 고원 남쪽 끝 봉우리(x −5,000~−2,300·y −2,800~−5,800, +6.5m)는 01c·02b 기둥 사이 마루가 맨 지각이었다. 동·남 면은 60° 넘는 급사면이다. 드레이프는 x −5,000 서쪽까지만 닿는다.
- 띠는 25° 이상 사면 표본만, 드레이프는 CR 섬 자리만 덮으므로 두 평지에는 맞지 않았다. 평지는 드레이프와 같은 CR 평면 그룹을 옮겨 심는 한 방향이라 초안 없이 진행했다.

### 02c 동굴 간섭 확인

- 북 급사면 02c(ASM_Z1_NorthRimWall) 인스턴스 6,209개의 경계 상자 8모서리를 옥탄트 로컬로 바꿔 볼록 껍질을 홀 절개 다각형과 챔버(x −300~4,400·y 4,200~8,000·H −4,000~−1,200)에 대조했다. 겹침 0, 가장 동쪽 모서리 x −141, 절개선까지 최소 75cm(절개 밖)다. 동굴 안으로 뚫고 들어가지 않는다.
- 02c 바닥 지면 타일 일부(SM_ForestTerrain)는 H 약 −27m, 지각(+20m) 속에 묻혀 있다. 절개 밖이라 보이지 않는다.

### V034 — 평면 묶음 Flat V001 + 지각 V010(채택)

- `RidgeZ1CompositionSpec`: `FLAT_REGIONS`(북쪽 단·남서 모서리 다각형, 기존 드레이프 점에서 1m 넘게 떨어뜨림)·`FLAT_CARVE`·`massif_flat_ground`. Blender `-- massif-flat` → `Z1MassifCover_V010`(V009에서 다각형 안만 50cm 낮춤, 안쪽 1m 페이드, 45~60°에서 낮춤을 줄임, 100,063삼각형, 위상 결함 0, 경계 변 1,385, Meadow 경계 최대 보정 0.0076cm).
- `flat V001`: CR Ground·SmallAssembly·Props·Ditch의 인스턴스와 데칼을 읽어, 섬 안에서 하상 띠 중심선과 칸 외접원 + 3m 넘게 떨어지고 지지면 오프셋 80cm 넘고 칸 안 지지면 경사 중앙값 12° 미만인 10m 원본 칸 233개를 골랐다(지지면 경사 최댓값 기준은 타일 가장자리 단 때문에 후보가 0이었다). 대상 다각형을 10m 칸으로 나눠 칸마다 원본 칸 하나를 90° 배수 회전으로 옮기고, 인스턴스마다 원본 지지면 위 높이(dz)를 V009 지각 위에 더했다(하상 둑 +3m 단을 빼기 위해 드레이프처럼 원점 높이를 그대로 쓰지 않는다). 기울기 규칙은 Drape V004와 같고, 다각형 밖·동굴 절개 2m 안으로 옮겨진 인스턴스는 뺐다. 대상 칸 58개(북 42·남서 16), 인스턴스 14,850(북 11,611·남서 3,239). 제외: 다각형 밖 9,687·60° 초과 3,745·타일 45° 초과 722.
- `Z1AssemblyWall_V034` = V033 + Flat V001 + 지각 V010(`WALLS["V034"]`).
- 결과: 북쪽 단이 숲과 지면 타일로, 남서 봉우리 마루가 기둥 사이 지면·수풀로 덮였다(`Shots/Compare_V033_V034_North.png`·`Compare_V033_V034_SouthWest.png`). 분지 저시점 3곳·테두리 정면·남 둑 근경·북 둑 근경·동쪽 동굴 입구는 V033과 같고, 부감·북 테두리 부감에서만 북쪽 단 숲이 보인다(`Compare_V033_V034_Standard1.png`·`Standard2.png`).
- 사용자 판정(2026-11-22): V034 채택. 기준 시험본을 `Z1AssemblyWall_V034`로 바꿨다(GX-029).
- 남은 점: 석교 아래 협곡 바닥(Z2)·남 테두리 위 남쪽 대지·협곡 남벽·북쪽 단 뒤 V012 오름 사면은 범위 밖이라 맨 지각이다. 남서 봉우리 급사면은 지각 절벽 재질로 둔다. 평면 묶음에는 동선·Pod 보호를 적용하지 않았다(북쪽 단 NorthRamp2·StairTerrace2). 근경에서는 원본 칸 사이 틈으로 50cm 낮춘 지각이 조금 보인다(ED 원본 흙바닥 틈과 같은 수준).
- 도구 함정: 첫 남서 근경 시점은 드레이프 식생 안에 눈이 들어가 쓸 수 없었다(바꾼 값은 `RimView.py`). 수직 내려다보기는 `look`의 `make_rot_from_xz`가 퇴화하므로 `SurveyBare.py top`처럼 화면 위쪽을 +y로 따로 준다(이때 화면 오른쪽이 −x로 뒤집힌다).

## 2026-11-22 — 1-BR U4 Z1 배치 재현 원본(V034, GX-030)

V034 구도를 U5 충돌 프록시·U6 베이크로 넘길 재현 원본으로 확정했다. 화면은 바뀌지 않았다. 대조 근거(`Verify_*.json`·`Regen_*.json`·`Fingerprint_V034.json`·`ProxyExclude_V034.json`)는 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Layout/`에 있다(로컬).

### 사용자 결정

- 재현 원본은 혼합 방식으로 둔다. 손배치 층은 결과 transform을 담고, 인스턴스 층(약 14.2만 개)은 생성 규칙과 지문만 담는다. 인스턴스 전량 덤프(약 15MB, 버전마다 통째 교체)와 규칙·지문만 두는 안(손 다듬기 되읽기 없음)은 고르지 않았다.
- 동선·Pod 보호(GX-017)는 보류한다. V011 설계도의 산괴·북쪽 단 마커가 새 지형과 이미 어긋났기 때문이다. CrownSummit Pod는 H 76.1m인데 지각이 9.6m이고, Terrace3Hub 착지는 56.4m인데 20m다. NorthRamp2 위 끝은 56.9m인데 20m, 고원 StairRamp4·CrownToSaddle 구간은 76m인데 6~10m다. 아직 맞는 것은 StairTerrace2(37.1m, 지각 35.4m)·NorthSaddle·RiftCanyon이다. Z1 동선·마커를 새 지형에 다시 정하는 단위에서 드레이프·띠·평면 묶음에 규칙으로 한 번에 적용한다. 재현 대상은 V034 그대로다.
- V011 설계도와 겹치는 표본 수(XY만 보고 보호 반경 기준)는 JSON `protection.v011_overlap_samples`에 남겼다. 평면 묶음은 CrownToSaddle 281·NorthRamp2 201·StairTerrace2 Pod 56, 남 띠는 RiftCanyon 29·CanyonRun 17이다.

### 구현

- `Scripts/Profiling/Graphics/EDTerrainLayout_Z1.json`(Git, 약 20KB)에 다음을 담는다.
  - 조명 6개·Sky Light
  - 개별 메시 6개(V002 3개, 보존 바위 Floor·Shell, 렌더링 지각 V010과 재질)
  - Level Instance 15개: 조립체 10개와 CR 산괴는 로컬 x, y, yaw, tilt, 높이 기준(`floor`·`local`·`abs`)과 오프셋으로, 인스턴스 층 4개는 원점 고정으로 담는다.
  - 인스턴스 층: 생성기 명령, 설정, 시드, 입력 `CR_*.umap` 해시, 결과 지문
  - 조립체 지문, 프록시 제외, `protection`, `crust_build`
- `Scripts/Profiling/Graphics/RidgeZ1Layout.py`의 인자는 다음과 같다.
  - `export`: 열린 시험 레벨을 JSON으로 되읽는다. 사람이 정한 `protection`·`crust_build`는 유지한다.
  - `verify`: 열린 레벨을 JSON과 대조한다.
  - `build <이름> [repro]`: JSON에서 시험 레벨을 만든다. V002 Level Instance를 읽지 않아 `load_level` 직후 assert 함정이 없다.
  - `regen <drape|band|flat> <버전>`: 출력 레벨·보고서만 `_Repro`로 돌려 다시 생성하고 지문을 대조한다.
- 지문 규약: 인스턴스마다 위치 0.01cm, 회전 X·Z 축 성분 1e-4, 스케일 1e-4로 반올림하고 메시+재질 덮어쓰기 키별로 정렬해 SHA-256을 낸다. 데칼은 재질·크기·정렬 순서를 더한다. 순서 무관이고, 쿼터니언 부호 모호성이 없다.
- `BuildRidgeZ1AssemblyTest.wall`을 `wall_v002_rows`(V002 사본 읽기)·`wall_instance`(WALLS 행 → 배치)·`build_wall_level`(조명·Sky·메시·Level Instance 조립)로 나눴다. JSON 빌드와 `wall`이 같은 조립 코드를 쓴다. `wall` 동작은 그대로다(이번에 새 `wall` 버전은 돌리지 않았다).
- `Scripts/Blender/HashRidgeCrust.py`: 지각 .blend의 정점·면 해시. 원점에 놓인 옥탄트 메시(지각·보존 바위)는 로컬 좌표가 정의되지 않아 월드 자세로 담는다.

### 재현 대조

- V034 자체: `export` 뒤 `verify` PASS다. JSON 손배치 값은 `WALLS["V034"]`과 같다(yaw·오프셋·기준).
- 인스턴스 층 재생성은 모두 MATCH다. 입력 `CR_*.umap` 해시를 먼저 확인했다.

  | 층 | 결과 |
  |:---|:---|
  | 평면 묶음 V001 | 14,850 |
  | 띠 북 V009 | 16,797 |
  | 드레이프 V006 | 31,718 + 데칼 16 |
  | 띠 남 V005 | 79,136 |

  각 생성은 20~50초 걸렸다.
- 전체 재구성: `build V034 repro`로 `_Repro/Z1AssemblyWall_V034`를 만들고 재생성한 층 4개를 끼웠다. 다음 Cmd에서 `verify` PASS가 나왔다. 개별 메시·Level Instance 위치·축 오차는 0이고, 층 4개·조립체 11개 지문과 묻힘 수가 같다.
- 지각: Blender `massif-flat` 흐름을 스크래치 폴더로 다시 돌렸다. 정점 50,723·면 100,063의 정점 해시가 기존 V010 .blend와 같고 manifest도 같다.
- 재생성본은 `EDTerrain_V001/_Repro/`(Z1MassifFlat_V001·Z1RimBand_V005·V009·Z1MassifDrape_V006·Z1AssemblyWall_V034)에 남겼다. Git 제외이고 지워도 된다.

### 프록시 제외(묻힘)

- 규칙: 메시 상자 꼭대기(현지 Up 쪽 최고점)가 렌더링 지각 V010보다 50cm 넘게 아래면 뺀다. 동굴 절개 안은 판정하지 않는다. 대상은 조립체와 CR 산괴다. 인스턴스 층은 생성 때 지각에 다시 앉히므로 판정하지 않았다.
- 결과: 37,014개를 뺐다.
  - 02 계열 조립체 7개가 3,657~6,632개씩이고, 지각 아래 중앙값은 11~25m다.
  - 01 계열 기둥 3개는 0~61개, CR 산괴는 115개다.
  - 02 계열은 원본 레벨에 벽 뒤 바닥 덮개(지면 타일·식생)가 넓게 깔려 있어, 그 바닥이 고원 지각 아래로 통째로 들어간다. GX-029 때 본 02c 북 급사면 바닥 타일(H 약 −27m)은 `ASM_Z1_NorthRimWall` 6,632개에 들어 있다.
- 목록(Level Instance·메시 키·로컬 위치·묻힌 깊이)은 `Layout/ProxyExclude_V034.json`이다. JSON에는 Level Instance별 수만 담았다.

## 2026-11-22 — 1-BR U5 Z1 충돌 프록시(V035, GX-031)

V034 배치에서 Z1 충돌 지각과 `_Floor`·`_Shell` 프록시를 굽고, ED 시각 메시의 충돌을 모두 껐다. 시험 레벨은 JSON V035에서 만든 `Z1AssemblyWall_V035`다. 근거는 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Collision/`(분류·내보내기·반입·검사), `Saved/SliceOctant/Ridge_00/V012_Z1Collision_V001/`(Blender 산출·manifest `report`·기각 실행 `rejected_run1~9/`)에 있다(로컬).

### 사용자 결정

- 프록시 기준(제안 승인): 식생(`/3D_Plants/`와 Vine·Ivy·Hornbeam·MoneyPlant)·물·데칼·묻힘 37,014개·동굴 절개 안·옥탄트 메시·100cm 미만 덩어리·50cm 미만 납작한 타일을 뺀다. 남은 것은 지면층 54,203·덩어리 7,974·판 2,787개다(제외 149,747).
- `_Floor` 높이는 두 지각 어느 쪽도 아니다. 실제 타일 윗면을 래스터해 충돌 전용 지각 V011에 굽고, 렌더링은 V010 그대로 둔다.
- ED 시각 메시 충돌은 U5에서 끈다(U6으로 넘기지 않음). 대상 레벨·블루프린트의 저장은 스크립트가 한다.

### 구현

- `ExportRidgeEDCollision.py`: `stats`·`export`(인스턴스 world transform·분류, 고유 메시 109개 fallback RenderData LOD0 이진)·`import`·`recheck`·`visualoff save`·`layout`·`check`.
- `BuildRidgeEDCollision.py -- build`: 단계별 캐시(`cache/<키>/`)를 두고 다음을 한다.
  - 고유 메시를 용접한 뒤 축척 비례로 decimate한다. 오차 p95는 최대 16.1cm다.
  - 지면층 윗면을 50cm 격자로 래스터한다(61,809칸 중 46,449칸 명중).
  - 덩어리를 40m 칸(여유 150cm) voxel remesh(30cm)한다. 입력 3,641만 → 326만 → 91만 삼각형이다.
  - 발자국 절개와 V011 굽기를 반복한다. 깊은 표본 3,893 → 472 → 148 → 79 → 18 → 1 → 0으로 7회 만에 수렴했고, 절개 칸은 20,569개(5,142㎡)다.
  - 묻힌 면 437,190개를 지우고 Floor/Shell로 나눈다.
  - 마지막으로 내부 assert와 맞춤 측정을 한다. 마지막 실행은 5.7분, 최대 4.0GB였다.
- `BuildRidgeOctant.crust`에 `cut_test` 인자를 더했다(칸 단위 절개 판정). 기본 동작은 그대로다.
- `BuildRidgeZ1AssemblyTest`: 충돌 행은 태그·프로필·Static·숨김으로 놓는다. 시각 행은 `use_default_collision`을 끄고 NoCollision으로 둔다. 드레이프 생성기는 원본 충돌을 복사하지 않는다.
- `RidgeZ1Layout`: 충돌 행을 되읽고 대조한다. `collision_build`를 보존한다.

### 결과

- 메시: V011 133,671·Floor 68,776(8,120㎡, 연결 성분 359)·Shell 404,399(40,664㎡)삼각형이다.
  - 4㎡ 미만 Floor 조각 7,253개는 Shell로 넘겼다.
  - 정점 범위는 Floor·Shell 6,355~24,552cm, V011 0~30,000cm다.
- 내부 assert: 덩어리 안 50cm보다 깊은 지각 표본이 0개다(허용 안 38,756). 판정은 현지 Up 광선 교차 홀짝이다.
- 맞춤 p95: fallback 원본 위 걷는 면 표본 10,328개에서 충돌면까지 거리다.

  | 보이는 면 → 가장 가까운 충돌면 | p95 | 표본 |
  |:---|---:|---:|
  | 가장 가까운 면이 Floor(전체) | 21.5cm | 2,053 |
  | 덩어리 → Floor | 19.4cm | 1,671 |
  | 판 → Floor | 24.9cm | 276 |
  | 지면층 → 지각 | 13.3cm | 1,500 |
  | 렌더링 지각 → 지각 | 3.2cm | 5,704 |
  | 보이는 걷는 면 전체(Floor·지각) | 13.0cm | 9,331 |
  | ED 메시만(Floor·지각) | 20.0cm | 3,608 |
  | 덩어리 → Shell(경사면) | 42.3cm | 815 |

- 바깥값: 지면 타일인데 가장 가까운 충돌면이 프록시인 표본은 220개다(ground|shell 133개 p95 381cm, ground|floor 87개 p95 72cm). 산괴 위에 높이 얹힌 드레이프 타일이 래스터 창(지각 +600cm) 밖이다.
- UE 반입: 세 메시 모두 complex-as-simple 단일 trimesh, LOD 1, 옥탄트 안, 성분 32767 이하다.
  - FBX 반입이 상자 충돌을 하나씩 자동으로 만들어 첫 검사가 실패했다. `import`가 이제 지운다.
- V035: `build V035` → `verify` PASS다. `check` PASS이고 태그·프로필·Static·숨김은 Guide §4.2 표대로다.
  - 하위 Level Instance까지 충돌 3개 외 0이다. 블루프린트를 디스크에서 다시 불러온 뒤에 확인했다.

### 시각 충돌 끄기

- V034까지 Level Instance 안 ED 시각 메시 75,782개가 BlockAll이었다.
  - `CR_LargeAssembly` ISM 39개
  - `PL_LargeCliff_01b·01c·02b·02c·02d·02e`의 BPP 블루프린트 액터·서어나무
  - Drape V006(원본 충돌 복사)
- Band·Flat 층과 나머지 조립체는 이미 꺼져 있었다. 레퍼런서는 Ridge ED 시험 레벨뿐이다.
- `visualoff save`가 레벨 8개를 차례로 열어 끄고 저장한다. BPP 블루프린트 13개는 템플릿 자체를 고쳐 컴파일·저장한다.
- 값이 되돌아가던 원인은 넷이다.
  1. StaticMeshActor는 `use_default_collision`이 기본으로 켜져 있어, 재등록 때 메시 기본값(BlockAll)이 NoCollision을 덮는다. 첫 V035에서 V010 렌더링 지각까지 BlockAll이었다. 그 맵은 `Collision/rejected_V035_defaultcollision/`로 옮기고 다시 만들었다. `delete_asset`은 True를 돌려줬지만 `.umap`이 남았다.
  2. Python 속성 설정은 패키지를 더럽히지 않는다. `modify()` 없이는 저장되지 않는다.
  3. 레벨의 BPP 인스턴스 값은 템플릿이 `use_default_collision`이면 저장 때 유지되지 않는다. 템플릿을 고친다.
  4. 이름 있는 프로필(BlockAll)이 남으면 로드 때 프로필이 충돌 설정을 다시 덮는다. 프로필을 `NoCollision`으로 바꾼다.
- ED 파생 에셋은 Git 밖이므로 다시 Migrate하면 `visualoff save`를 다시 돌린다.

### 실패와 교훈

- unwelded fallback collapse decimate는 정점을 메시 밖 최대 170m로 튀겼다. 먼저 용접하고 원본 상자를 검사한다.
- 1,670만 삼각형 단일 덩어리 remesh는 TBB/OpenVDB에서 죽었다. 40m 칸으로 나눈다.
- Solidify 균등 오프셋은 퇴화 면에서 판을 약 2,450km 밖으로 밀어 25GB까지 부풀었다. **함께 열린 UE 에디터가 OOM으로 종료됐다**(저장할 변경은 없었음). numpy 판으로 바꾸고 작업 메모리 12GB 감시를 붙였다.
- 법선 방향 두께는 안쪽 법선 스캔에서 윗면을 띄웠다(Floor p95 40cm). 양쪽 반씩은 중앙값 20cm였다. 완만한 면은 현지 아래로 민다.
- 뒤가 열린 스캔 절벽은 voxel 부피가 안 생겨 사라졌다. 열린 덩어리 5,011개를 판으로 다룬다.
- 최근접 면 법선으로 안쪽을 판정하면 열린 절벽 뒤 지각을 거짓 양성으로 잡았다(최대 61m). 광선 홀짝으로 바꿨다.
- 묻힌 타일을 표본으로 쓰거나 축소본을 기준으로 재면 p95가 부풀었다. 렌더링 지각을 보이는 면에 넣고, 원본은 고유 메시별 로컬 BVH로 잰다(타일 BVH는 14GB).

## 2026-11-22~23 — 1-BR U6 Z1 베이크·러프 PIE(JSON V037, GX-032·GX-033)

V035 구성을 베이크 가능한 Ridge WIP LVI로 옮겨 굽고, 1인 러프 PIE에서 사용자가 지적한 7곳을 규칙으로 고쳐 JSON V037·LVI V010까지 만들었다. **재점검 PIE는 다음 세션이다.** 근거는 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Bake/`(LVI 빌드·베이크·후보·Nav·평면도), `Cull/`(인스턴스 내보내기·판정·사본), `Collision/Export_V037/`·`Saved/SliceOctant/Ridge_00/V012_Z1Collision_V002·V003/`에 있다(로컬).

### 사용자 결정

- LVI는 레벨 복사가 아니라 JSON에서 스크립트로 만든다. 진단 베이크를 먼저 하고 동선·마커를 정한다(제안 승인).
- Floor 최소 성분 넓이 50㎡(GX-032). 산괴는 적 Nav 밖 배경으로 두고 Pod 10개로 PIE(A안). NPC 전투 확인은 지형·프랍·동선 뒤 마지막.
- "지금까지 나온 부분 다 수정"(러프 PIE 지적 7곳). 규칙 수치는 에이전트가 정했다(GX-033, 재점검 PIE 판정 대기).

### LVI와 베이크(`BuildRidgeZ1BakeLVI.py`)

- 베이커 수집기는 LVI 최상위 액터만 읽고 중첩 Level Instance 안은 읽지 않는다. 그래서 시각 Level Instance 15개(NoCollision)를 그대로 넣었다. PIE에서 하위 레벨 15개 런타임 로드를 확인했다(`Loaded 15 levels`).
- V035의 보존 바위(`PreservedRocks_Floor/_Shell`)는 충돌이 꺼져 있었다. LVI에서는 Floor/Shell 계약으로 되돌렸다. 두 메시에는 U3 FBX 반입 때 생긴 자동 상자가 1개씩 남아 있다(complex-as-simple이라 질의에 안 쓰임, 기본 WIP와 공유라 미수정).
- Pod 투영: 베이커는 Pod마다 투영 100cm·가장자리 여유 250cm(NeedsExact 아닌 표본)를 요구한다. 트레이스로 고른 자리(V001~V004)는 CrownSummit 44.8cm·StairTerrace2 투영 실패·NorthSaddle 0cm에서 연달아 실패했다. Pod 없이 구운 판의 스폰 후보 중 가장 가까운 유효 후보(가장자리 260cm 이상)로 옮기는 두 단계로 바꿨다.
- **Nav Layer 상한.** V005(Floor 4㎡) 베이크는 Support·Nav Layer 400개였다. 런타임 Nav 그래프는 Nav Layer 256개를 넘으면 실패한다(`LNPNavGraph.cpp`). 원인은 Z1 Floor 연결 성분 359개다. Floor 성분 면적은 10㎡ 이상 157개(면적 84%), 25㎡ 76개(68%), 50㎡ 40개(53%)였다. 50㎡로 재분할(`BuildRidgeEDCollision.py -- split V002 50`, V001 캐시·V011 지각 재사용 10초)해 V006에서 Layer 79가 됐다.
- V006 Nav 섬(portal로 이은 component, `Nav_V006.svg`): 주 섬 69,910 node(89%)가 서쪽 진입·북서 저지·안부·북쪽 단·분지·협곡·남쪽 메사·보스를 잇는다. 고원 셀 1,329개 중 주 섬은 6개뿐이고(CR 산괴 덩어리가 덮어 지각 절개) +38m 받침은 고립 섬이다. 스폰 후보는 고원 4·북쪽 단 1개로 드물다.

| 판 | 내용 | Layer | Nav 셀·component·portal | 결과 |
|:---|:---|---:|:---|:---|
| V001~V004 | V036 구성, Pod 트레이스 투영 | — | — | Pod 가장자리 여유·투영 실패 |
| V005 | Pod 0, Floor 4㎡ | 400 | 80,078·348·890 | 런타임 상한 초과 |
| V006 | Pod 0, Floor 50㎡(V002) | 79 | 78,294·127·633 | 진단·survey |
| V007 | Pod 10(후보 V006) | 79 | 같음 | 러프 PIE(slot 2·0) |
| V008 | JSON V037 시각, 옛 충돌 | — | — | U5 V003 내보내기 원본 |
| V009 | V037 + 충돌 V013/V003, Pod 0 | 82 | 78,385·119·640 | 후보 |
| V010 | Pod 10(후보 V009) | 82 | 같음 | **재점검 PIE 대상**, 풀 `DA_GraphicsRidgeZ1BakePool_V010` |

모든 판에서 이음매가 Meadow와 같았고, PIE 로그 `maxSeamRadiusDelta=0.000cm`·막힌 이음매 0이었다.

### 러프 PIE 지적(1인, EnemyDensity 0, slot 0)

| # | 위치(로컬 X, Y, H) | 현상 | 원인 | 처리 |
|:---|:---|:---|:---|:---|
| 1 | -7203, 3913, 46.4m | 공중에 뜬 바위·지면 타일, 옆 타일 통과 | 드레이프가 V002 배치 높이(V012 원래 지형 44m)에 앉고 렌더링 지각은 21m로 낮아짐 | B |
| 2 | -6614, -5461, 38.8m | 지면에서 가지처럼 뻗은 프랍 덩어리 | 같음(받침 가장자리) | B |
| 3 | -7202, -2136, 58.4m | 봉우리에 얹힌 프랍 덩어리 통과 | 드레이프가 동굴 절개 둘레 V012 봉우리(56.7m)에 얹힘, 지면층은 윗면만 래스터 | B·C |
| 4 | -8127, -2993, 39.9m | 동굴 지붕 위 프랍 완전 통과 | 동굴 절개 안은 U5 충돌 제외 | C |
| 5 | -9477, -451, 38.3m | 한쪽 면만 그려지는 얇은 사암 판이 꽂힘 | 드레이프 MassiveSandstoneCliff_04(8×4.6×12.8m)가 지면 27m 위, 받침 없음 | A |
| 6 | 2584, 2137, -39.1m | BasinHall 입구를 막은 큰 바위 통과 | V002 개별 메시 `ED_Z1_NorthCaveFace`(SM_Cliff_06)가 절개 안 | JSON에서 제거 |
| 7 | 6775, -2483, -29.7m | 분지 → ValleyRise 사면을 걸어 못 오름 | V012 설계 24° 경사(x 4,000~7,800)가 분지 평탄화로 x 6,600~7,800 46~52° | 경사로(E) |

전반 점검: 지각 3m 안 얇은 인스턴스(두께/길이 <0.4, 1m 이상) 중 얇은 축이 지각 법선에서 45° 넘는 것이 4,560개였다(Embankment 계열 다수, 의도된 배치 포함 가능).

### 정리 규칙(GX-033, `CullRidgeZ1Instances.py` + `Scripts/Blender/DecideRidgeZ1Cull.py`)

- 흐름: LVI(Level Instance 로드)에서 `export V001`(214,705 인스턴스·8,419 컴포넌트·322 메시·데칼 16) → Blender `DecideRidgeZ1Cull.py -- V001` → 같은 LVI에서 `build V001`(소유 Level Instance마다 `EDTerrain_V001/Z1Cull_V001/<라벨>` 월드 좌표 ISM 레벨, NoCollision 프로필). 원본 조립체·층 레벨은 그대로다.
- A 받침: 바닥이 지각 +1m 안이면 땅. 아니면 바닥 표본 2점에서 아래 1.3m 광선으로 큰 덩어리(4m 이상, 식생·물·납작한 타일 아님, U5 축소 메시 BVH)를 찾고, 받침이 땅에 이어져야(반복 전파) 남긴다. 받침만 보던 첫 판은 큰 바위끼리 받친 채 통째로 뜬 묶음을 놓쳤다.
- B: 드레이프는 고원 가중치(`massif_cover_weight` V002) 0.95 이상만 남긴다. 첫 판의 "반경 4m 지각 높이 폭 3m"는 고원 위 ED 섬 +3m 둑 고리까지 잡아 드레이프 61%(19,218)를 지웠다.
- C: 중심 xy가 동굴 cut 다각형 안. D: 지각 3m 안 얇은 판(두께/길이 <0.35, 1.5m 이상)의 얇은 축이 지각 법선에서 60° 넘게 벌어짐. 데칼은 B·C만.
- 결과: A 20,949·B 3,820·C 3,642·D 739, 데칼 4(합 29,150, 14%). 제보 1·2·3·4·5 자리의 해당 인스턴스가 모두 빠지는 것을 진단 덤프(`Cull/Probe_V001.json`)로 확인했다.

| 소유 | 원래 | 뺌 |
|:---|---:|:---|
| 02 계열 벽 3개 | 26,077 | A 7,558·D 46 |
| CR 산괴 | 10,234 | A 5,432·D 125·C 20 |
| 드레이프 | 31,718 | C 3,620·B 3,820·A 1,988·D 274 |
| 평면 묶음 | 14,850 | A 647·D 86 |
| 띠(남·북) | 95,933 | A 966·D 158·C 2 |
| 그 밖 조립체 7개 | 35,893 | A 4,358·D 50 |

### 경사로와 충돌 재빌드

- 렌더링 지각 V012 = V010 + `spec.RAMPS["BossValley"]`(x 4,400 −40m → x 7,700 −24m, 중심선 약 26°, 반폭 4m + 섞임 6m). Blender `-- massif-ramp` → `importcrust V012`(이음매 보정 최대 0.0076cm). 정점 해시 `ffaf44ce…`.
- U5 V003(`-- build V003`): 입력 `Export_V037`(V037 시각, 지면층 52,401·덩어리 7,138·판 1,861·제외 124,163). 절개 반복이 8회 상한에서 1표본(66.7cm) 남아 상한을 12로 올렸고 9회에 수렴했다(절개 4,366㎡). 충돌 지각 V013 134,900·Floor 34,108(성분 40, 4,180㎡)·Shell 459,537삼각형, 걷는 면 p95 11.0cm. 반입 검사 통과·사용자 저장.
- V010 Nav 섬: 주 섬 70,584 node. 경사로 구간 셀 95개 모두 주 섬, 서쪽 비탈 주 섬 978/1,014(V006 478). 고원은 여전히 고립 섬(A안).

### 함정

- 에디터 GPU 크래시(DXGI_ERROR_DEVICE_REMOVED, VRAM 3.9/7.2GB)가 무거운 LVI를 반복해 만들고 연 뒤 났다. 저장 안 된 사용자 변경은 없었다.
- Python에서 `LNPSettings.OctantPool`은 보이지 않는다. 검사 풀 임시 선택은 MCP ObjectTools로 한다. 에디터를 다시 켜면 선택이 원래 값으로 돌아간다.
- build가 실패하면 저장 안 된 `/Temp/Untitled_N` 맵이 남아 다음 build를 막는다(`discard=` 옵션).
- LVI에는 조명이 없어 에디터 화면 캡처가 검게 나온다(Unlit도 같았음). 화면 확인은 시험 레벨이나 PIE로 한다.
- U5 `export`는 시험 레벨 이름에서 버전을 읽었다. 이제 `export V037 V012`로 LVI에서도 돌고, 충돌 프록시 액터(`/Z1Collision_`)는 입력에서 뺀다.

## 2026-11-24 — 1-BR U6 V010 재점검 준비

- 필수 문서·U6 실행서·이전 러프 PIE 지적 7곳과 GX-033 처리 경과를 확인했다. Git 작업 트리는 시작 시 깨끗했고 재현 원본은 V037, V010 LVI·검사 풀 파일이 존재했다.
- 라이브 에디터는 TestMap03, PIE=false, 원래 풀 `/Game/Maps/OctantPoolData.OctantPoolData`, EnemyDensity=1이었다.
- `Bake/SelectionState.json`의 이전 판을 날짜별 사본으로 보존하고 원래 풀·밀도·카메라·Config 기본 INI 8개·정식 풀 해시를 기록했다. V010 검사 풀을 `ObjectTools.set_properties`로 메모리에서만 선택하고 getter로 확인했다(SaveConfig 없음). 선택 뒤 INI·정식 풀 해시는 동일했다.
- 사용자 1인 PIE에서 실제 Ridge slot 1, EnemyDensity 0.00, 시각 레벨 15개 로드, `maxSeamRadiusDelta=0.000cm`·막힌 이음매 노드/변 0을 확인했다. 지형 묻힘 → 배경 프랍 → 동선 → 마지막 NPC 전투 순서를 유지한다. 아직 재점검 통과 판정은 없다.

### 첫 재점검 제보: 남쪽 왕관 외피 안 진입

- 사용자가 첫 순간이동 뒤 이동 중 세로 바위 모양 메시를 통과해 들어갔다. 로그(UTC 06:04:22)의 WhereAmI는 slot 1 `X=-5130 Y=-4719 H=1393`, 발밑 `SM_Ridge_EDTerrain_V001_Z1_Shell`이다. 첫 순간이동 지점 `(-7203,3913,H4600)`과는 별개 위치다.
- Cull V001 원본·제외 목록과 U5 V037 입력을 읽기 전용으로 대조했다. 주변 후보는 `ASM_Z1_PlateauSouthCrown`이다. 동굴 cut 밖, 렌더링 지각 H650cm, 고원 가중치 약 0.038이다. 가장 가까운 평면상 동선은 SkyBridgeCrossing(갑판 층) 417cm 거리이며 이 지점을 보행 공간으로 삼은 설계 근거는 없다. GX-033의 배경 산괴라는 결정은 적 Nav 제외이며 플레이어 통과 허용을 의미하지 않는다.
- 가장 가까운 큰 후보는 `SM_Cliff_01`(크기 약 14.9×10.2×15.6m, 높이 범위 H1264~2804cm)이다. 원본 fallback 용접 뒤 경계 변 146개인 열린 외피이고 U5 BODY 입력 행 431에 포함됐다. 소스의 인근 424면 표본과 충돌 몸체 캐시 간 거리 p50 5.1cm·p95 20.4cm·최대 42.4cm였다. 조사점에서 현지 +Up·−X·+Y 방향은 원본·프록시 모두 20m 안 교차가 없었다. 전체 메시 충돌 누락으로 단정할 근거는 없고 외피의 열린 쪽·조립 틈을 통한 진입이 후보이다.
- 주변 작은 Cliff_03/04/09 일부의 국소 면 표본은 프록시와 최대 약 2.3m 차이도 있었다. 따라서 통과한 면을 특정하기 전에는 열린 뒷면 진입과 국소 프록시 누락을 확정 분리하지 않는다. 현재 캡처는 사용자가 이동한 뒤여서 제보 당시의 진입면 증거가 아니다.
- 근거: `Bake/Recheck_20261124/ProbePassThrough.py·json`·`ProbeGeometry.py·json`(Blender 5.2 Python/백그라운드로 로컬 덤프만 읽음). 에셋·배치·프록시는 수정하지 않았다. 사용자에게 통과한 면 바로 바깥의 WhereAmI와 Shot을 요청한다.

### 두 외부 시점 대조

| 화면 | WhereAmI(로컬 X/Y/H, cm) | 발밑 |
|:---|:---|:---|
| `Saved/Screenshots/WindowsEditor/ScreenShot00004.png` | `(-6222,-4669,749)` | 충돌 지각 V013 |
| `Saved/Screenshots/WindowsEditor/ScreenShot00005.png` | `(-5843,-4053,749)` | 충돌 지각 V013 |

- 사용자 지시대로 캐릭터보다 오른쪽을 대조했다. 세로로 선 암석 외피의 얇은 가장자리, 열린 옆/뒤와 그 사이로 보이는 배경이 두 각도에서 확인된다. 촬영점은 정상 지각 위라 플레이어가 접근 가능한 자리이며, 배경이라는 이유로 노출·진입 문제를 수용할 근거는 없다.
- 추가 진단 `Bake/Recheck_20261124/ProbeVisibleSupport.py·json`: 앞서 찾은 `SM_Cliff_01`의 실제 fallback 정점은 지각보다 최소 631.7cm 위였다(50cm 칸 높이 표본). 상자 바닥 H1238.8cm·상자 중심의 지각 H600cm, 실제 정점 최저 H1263.9cm다. 지각 직접 접지는 아니며 A 규칙의 다른 큰 덩어리 받침 관계로 살아남은 후보이다. 이 거리만으로 받침 바위까지 없는 공중 배치라고 확정하지 않는다.
- 현재 결론: 정규 보행 공간이 아닌 배경 조립체의 열린 외피/틈이 접근 가능한 곳에 노출됐다. 소스 전체 충돌 누락은 아니며, 물리 면을 실제로 가로질렀는지 열린 쪽으로 들어갔는지는 진입 전후 연속 기록이 없어 구분하지 않는다. 수정 단위는 외피가 보이는 배치·시각 빈 공간과 내부 진입 차단을 함께 다루고, JSON→U5→베이크→같은 두 시점 PIE로 확인하는 방향을 제안한다. 아직 에셋 수정은 없다.

### 이번 PIE 제보 대장

**사용자 지시(2026-11-24): 이번 PIE에서 지적하는 부분을 모두 기록한 뒤 한 번에 수정한다.** 수집 완료를 사용자가 알릴 때까지 개별 항목의 선행 수정·배치 변경·규칙 변경·충돌 재빌드·재베이크는 하지 않는다. 조사에서는 좌표·화면·증상·확인 사실과 추정을 구분해 기록한다. 위의 R01만 먼저 수정하자는 제안은 보류한다. 수집 완료 뒤 전체 항목을 묶어 수정하고, 구현 단위별 진행 확인은 유지한다.

| ID | 지적 | 위치·화면 근거 | 상태 |
|:---|:---|:---|:---|
| R01 | 남쪽 왕관의 세로 바위 외피가 얇게 드러나고 열린 옆/뒤로 배경이 보임. 사용자가 내부까지 진입 가능 | 내부 `(-5130,-4719,H1393)`; 외부 `(-6222,-4669,H749)`·`(-5843,-4053,H749)`; `ScreenShot00004/00005.png` 오른쪽. 상세 조사: 위 "첫 재점검 제보"·"두 외부 시점 대조" | 기록 완료·일괄 수정 대기. 열린 틈 진입과 국소 충돌 누락은 미분리 |
| R02 | 덩굴·바위 선반 아래의 빈 공간에 캐릭터가 통과할 크기의 구멍이 있고 실제로 진입 가능(사용자 확인) | `(-5714,-1475,H751)`·`ScreenShot00006.png`; `(-6440,-1182,H1179)`·`ScreenShot00007.png`. 두 각도에서 같은 공간 촬영 | 기록 완료·일괄 수정 대기. 의도된 진입 동선 여부·소유 조립체·열린 틈/충돌 누락 원인은 미확정 |
| R03 | 지표 판·바위 사이 큰 구멍으로 아래 공간이 비치며 실제 진입 가능. 내부에서는 지표·바위 묶음 아래 넓은 빈 공간이 드러남 | 외부 `(-7923,418,H1119)`·`ScreenShot00008.png`, `(-7284,786,H1146)`·`ScreenShot00009.png`; 내부 `(-7463,343,H749)`·`ScreenShot00010.png` | 외부 2각도·내부 진입 화면 기록 완료·일괄 수정 대기. 사용자 기준: 안이 비치지 않고 캐릭터가 통과할 수 없어야 함 |
| R04 | R03과 같은 하부 빈 공간으로 걸어서 들어갈 수 있는 별도 구멍(사용자 확인) | 촬영점 `(-7863,1591,H746)`·`ScreenShot00011.png` | 기록 완료·일괄 수정 대기. R03 연결 진입구로 묶어 검토하며 개별 재검증 위치는 유지 |
| R05 | 길쭉한 암석의 속이 빈 외피가 드러남. R01과 빈 모양이 같아 동일 에셋 같다는 사용자 제보 | `(-5800,-881,H1286)`·`ScreenShot00012.png`; `(-7302,-68,H3497)`·`ScreenShot00013.png` | 두 각도 기록 완료·일괄 수정 대기. R01과 같은 계열 후보로 연결하며 정확한 노출 면의 메시 동일성은 미확정 |
| R06 | 바위 사이 구멍으로 내려가 외피 뒤쪽 공간에 도달함. 내부 정면은 뒷면이 안 그려져 비어 보이지만 실제 벽 충돌이 있어 통과 불가(사용자 확인) | 구멍 촬영 `(-3910,513,H2288)`·`ScreenShot00014.png`; 내려간 뒤 `(-3267,954,H1212)`·`ScreenShot00015.png` | 기록 완료·일괄 수정 대기. 진입 구멍과 내부 정면 벽을 구분. 정면을 추가 통과구로 분류하지 않음 |

검사 대상은 `Z1Bake_V010`(JSON V037), slot 1·EnemyDensity 0이었다. **사용자 지시로 이번 PIE 제보 수집은 R01~R06에서 종료했다(2026-11-24).** 이후 풀·밀도를 복원했다. 기존 V007 지적 7곳 전체 재점검·NPC 전투·U6 게이트를 통과한 것으로 해석하지 않는다. 제보는 후속 몸체 수정·재검증 입력으로 보존한다.

#### R02 화면·로그 관찰

- 첫 촬영 로그는 15:26:32 KST WhereAmI → 15:26:51 Shot(`00006`), 두 번째는 15:27:15 WhereAmI → 15:27:19 Shot(`00007`)이다. 첫 발밑은 충돌 지각 V013, 두 번째 발밑은 Z1 `_Floor`다.
- `00006`: 캐릭터 오른쪽의 덩굴 아래에 바위로 둘러싸인 어두운 빈 공간이 보이며, 입구 앞/아래에는 수평으로 돌출된 바위 선반이 있다. `00007`은 같은 공간을 다른 높은 시점에서 본 화면으로, 바위·식생 사이 열린 공간을 대조할 수 있다.
- 실제 캐릭터 진입은 사용자 제보로 기록한다. 화면만으로 정확한 개구부 치수나 물리 충돌 누락을 확정하지 않는다. R01과 위치·지적 형태가 달라 별도 항목으로 보존하며, 공통 원인 여부는 수집 완료 후 조사한다. 에셋·배치·규칙·충돌·베이크 수정 없음.

#### R03 화면·로그 관찰

| 순서 | WhereAmI(KST) | 화면(KST) | 로컬 X/Y/H(cm) | 발밑 |
|:---|:---|:---|:---|:---|
| 외부 각도 1 | 15:31:51 | 15:31:54·`00008` | `(-7923,418,1119)` | Z1 `_Floor` |
| 외부 각도 2 | 15:32:03 | 15:32:13·`00009` | `(-7284,786,1146)` | Z1 `_Floor` |
| 실제 내부 진입 | 15:32:29 | 15:32:33·`00010` | `(-7463,343,749)` | 충돌 지각 V013 |

- 외부 2장은 같은 구멍을 서로 다른 각도에서 찍었고, 마지막 장은 실제 내부로 들어간 뒤 찍었다는 사용자 설명과 연결했다. `00008/00009`에는 바닥 판·바위 경계 사이의 큰 개구부와 아래 공간이 보인다.
- `00010`에는 캐릭터가 아래쪽 평평한 지면 위에 서 있고 지표·바위 묶음이 위를 덮은 넓은 빈 공간, 가장자리 아래의 어두운 틈과 하부 노출이 보인다. 외부 발밑 Floor → 내부 발밑 충돌 지각으로 달라지는 로그도 함께 보존한다. 프록시 두께·지면층 래스터·배치 높이 중 어느 것이 원인인지는 아직 조사하지 않는다.
- 사용자 수용 기준은 **구멍을 통해 안이 비치지 않고 실제 캐릭터도 통과할 수 없어야 한다**이다. R01/R02와 별도 위치 R03으로 누적한다. 에셋·배치·규칙·충돌·베이크 수정 없음.

#### R04 화면·로그 관찰(R03 연결 진입구)

- 15:35:26 KST WhereAmI `(-7863,1591,H746)` → 15:35:28 `ScreenShot00011.png`다. slot 1·V010이며 촬영점의 발밑은 충돌 지각 V013이다.
- 화면 중앙 오른쪽, 덩굴이 걸린 바위 윗부분 아래에 지면부터 이어지는 큰 개구부와 어두운 내부가 보인다. 사용자는 이 구멍으로도 걸어서 R03 내부 공간에 들어갈 수 있다고 확인했다. 연결 사실은 사용자 재현 설명으로 기록하며, 별도 자동 동선 검증은 하지 않았다.
- 수정 시 R03 한 입구만 막고 이 진입구가 남지 않도록 두 진입구를 같은 내부 공간의 노출·진입 문제로 함께 검토한다. R04 촬영점·화면은 독립 재검증 근거로 보존한다. 에셋·배치·규칙·충돌·베이크 수정 없음.

#### R05 화면·로그 관찰(R01과 외피 형태 유사)

- 15:38:54 KST WhereAmI `(-5800,-881,H1286)` → 15:38:56 `ScreenShot00012.png`(발밑 Z1 `_Floor`), 15:39:38 WhereAmI `(-7302,-68,H3497)` → 15:39:40 `ScreenShot00013.png`(발밑 Z1 `_Shell`)이다. 두 장은 같은 대상을 다른 각도에서 찍었다는 사용자 설명과 연결했다.
- `00012`에는 위를 향해 선 길쭉한 암석 외피와 가는 가장자리, 그 사이로 보이는 배경이 보인다. `00013`은 높은 곳에서 아래를 본 화면이며 암석 외피 사이의 빈 공간을 대조할 수 있다. R01과 속이 빈 모양이 같고 동일 에셋 같다는 사용자 관찰을 기록한다. R05의 실제 통과 재현은 이번 제보에서 별도로 확인하지 않았다.
- 읽기 전용 배치 덤프 대조(`Bake/Recheck_20261124/ProbePassThrough_R05.json`): 두 번째 촬영점 주변 소유는 `CR_Z1_LargeAssembly`다. 남은 후보는 `SM_Cliff_02`(회전 상자까지 88.7cm, 길이 약 16.1m)와 `SM_Cliff_01`(332.4cm, 약 14.9×10.2×15.6m)이다. R01 남쪽 왕관과 이번 CR 산괴 양쪽에 **동일 경로 `/Game/SmartAssets/AssetMeshes/SM_Cliff_01`을 쓴 배치가 존재**함은 확인했다. 다만 촬영점과 회전 상자 거리만으로 지적한 노출 면이 그 메시인지 확정하지 않는다(`SM_Cliff_02`도 후보). 인스턴스는 서로 다른 배치다.
- 일괄 수정 때 R01/R05의 열린 절벽 계열 외피 사용과 정리 규칙의 영향을 함께 검토하되 두 위치의 재검증은 각각 유지한다. 에셋·배치·규칙·충돌·베이크 수정 없음.

#### R06 화면·로그 관찰(구멍 진입 후 외피 뒤쪽 도달)

- 15:46:54 KST WhereAmI `(-3910,513,H2288)` → 15:46:56 `ScreenShot00014.png`(발밑 Z1 `_Floor`), 15:47:50 WhereAmI `(-3267,954,H1212)` → 15:47:52 `ScreenShot00015.png`(발밑 Z1 `_Shell`)이다. slot 1·V010이다.
- 첫 화면은 바위 사이로 아래쪽 어두운 공간이 열린 구멍을 내려다본 모습이다. 두 번째는 그 구멍 안으로 들어간 뒤의 화면이라는 사용자 설명과 연결했다. 두 기록 지점의 높이는 약 10.8m 차이지만 실제 낙하 시작·착지 지점을 계측한 값은 아니다.
- 두 번째 화면에는 바위·식생 뒤쪽과 배경이 노출된다. **정면은 메시 뒷면이라 비어 보일 뿐 벽이 존재하며 실제로 통과할 수 없다**는 사용자의 현장 판정을 명시한다. 진입 구멍의 개방 문제와 정면의 비표시·충돌 존재를 구별한다. 탈출 가능 여부·고립 여부는 제보되지 않았으므로 확정하지 않는다.
- 수정 검토 때 진입 구멍과 외피 뒤쪽 접근을 함께 다루며, 정면을 충돌 누락 통로로 잘못 분류하지 않는다. 에셋·배치·규칙·충돌·베이크 수정 없음.

### GX-034 계획 반영 — 현재 큰 배치 보존·몸체와 ED 외피

- 사용자는 현재 전반적인 비주얼과 산·절벽·큰 바위의 배치를 채택했고, 세부는 조정하되 현재 큰 배치를 유지하며 몸체를 만드는 방향을 계획 문서에 반영하도록 지시했다. ED를 원거리 전용이라고 단정하는 결정은 하지 않는다. 열린 외피·조립 틈·하부 공동이 접근 가능한 곳에 드러난 이번 제보를 근거로 몸체와 플레이 경계를 함께 저작한다.
- 전체 큰 배치 초벌을 먼저 확정한 뒤 그 배치에서 몸체를 만드는 순서로 정리했다. 세부 식생·잔돌 마감은 몸체 뒤에 둔다. 몸체는 Z1 대표 구간에서 시험한 뒤 구역별로 확장해 시각/충돌·동선·내부 Support를 확인한다. 큰 덩어리의 위치·방향·크기·높이 관계·스카이라인·구도를 지키고, 의도된 동굴·석교·이동 통로는 보존한다.
- 확정 결정 GX-034와 Phase 1-BR §2.1·U4·U5a/U5b·U6 게이트·체크리스트를 갱신했다. GX-021의 몸체 구성·확장 순서와 GX-031의 충돌 파생 입력은 부분 대체로 표시하고, 기존 V035~V037 기술 검증은 과거 결과로 보존한다. Roadmap·상위 Phase 1 실행서·README 라우팅·Current/인계 예시를 맞췄다. 몸체 생성법·수치·보존 대상 목록의 세부는 구현 단위에서 확인한다.
- **이번 요청은 계획 문서 반영이다.** V037 JSON·원본 ED·시각/충돌 에셋·LVI·검사 풀의 선택은 변경하지 않았다. PIE 제보 수집 종료 지시는 아직 없고 R01~R06은 수정 대기다. 에셋 구현은 수집 종료 뒤 단위별 진행 확인을 거쳐 시작한다. 종료 시 원래 풀·밀도 복원과 해시 확인은 그대로 필요하다.

### 세션 마감 — 제보 수집 종료·U4 전체 큰 배치로 인계

- 사용자가 이번 제보를 R01~R06에서 종료하고 다음 U4 전체 큰 배치는 새 세션에서 진행하도록 지시했다. 이번 세션에서는 배치·몸체·충돌 구현을 시작하지 않았다.
- 라이브 MCP로 TestMap03·PIE=false를 확인했다. 검사 풀 V010이 남아 있어 `/Script/LootNPop.Default__LNPSettings`의 `octantPool`을 원래 `/Game/Maps/OctantPoolData.OctantPoolData`로 메모리 복원한 뒤 getter로 확인했다(SaveConfig 없음). 사용자에게 적 밀도 복원을 요청했고 콘솔 실행 뒤 `EnemyDensity=1`을 getter로 확인했다.
- Config 기본 INI 8개·정식 풀·V037 배치 JSON의 SHA-256은 검사 전 기록과 동일했다. `Bake/SelectionState.json`은 `restored`, 판정은 `collection_closed_R01_R06_not_passed`다. 카메라는 사용자가 PIE를 종료한 뒤의 에디터 시점을 유지했다.
- `Bake/Recheck_20261124/Screenshots/`에 `ScreenShot00004~00015.png` 12장을 복사하고 원본과 해시 일치를 확인했다. `ScreenshotHashes.json`과 종료 시점 `PIERun.log`도 같은 폴더에 보존했다. 제보의 실제 통과 여부·뒷면 비표시·원인 미확정 범위를 구분한 대장은 위 기록을 따른다.
- [전체 큰 배치 인수인계](../analysis/Phase01_LargeLayoutHandoff.md)를 만들고 Current·Phase 실행서·Roadmap·상위 Phase 1 실행서·README를 다음 U4 단계로 맞췄다. 첫 단위는 U4-0 실제 상태 대조·V037 큰 배치 보존 기준·Z1~Z7 미배치 구역별 작업 순서와 첫 배치 단위 구체화다. 이후 전체 큰 배치 확정→Z1 몸체 시험→구역별 몸체·충돌→전체 PIE→세부 마감 순서를 유지한다.

## 2026-11-24 — 1-BR U4-0 실제 상태·큰 배치 보존 기준·전체 순서

- **배치 구현 전 조사 단위를 마쳤다.** GX-034·§2.1·U4와 이전 인계를 읽고 [U4 큰 배치 계획](../analysis/Phase01_U4LargeLayoutPlan.md)에 보존 대상·좌표/높이·비교 기준·구역별 단위를 구체화했다. V037 JSON·V010·Cull V001·원본 ED·정식 에셋을 변경하지 않았고 생성/반입/Cull/충돌/베이크를 재실행하지 않았다. R01~R06도 수정하지 않았다.
- 시작 MCP: TestMap03·PIE=false·맵 dirty=false, 정식 풀·EnemyDensity=1 getter 일치. 기존 V010을 연 뒤 사용자가 `InspectRidgeLargeLayout.py`를 Cmd에서 실행했다. 메시 액터8개 자세/메시·LI15개 Cull 참조/원점 자세·소유자별 수 대조 PASS, Pod10·훅6·검토점27·하위 레벨15. 전체 dirty 패키지는 조사 전후 `[]`였다.
- 현재 시각 결과는 185,555인스턴스·데칼12·428키, 지문 `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88`. JSON의 `layers/assemblies`는 Cull 전 결과이므로 현재 지문으로 오인하지 않는다. V010 LI는 월드 좌표 Cull 사본이며 원본 조립체 자세를 다시 적용하면 이중 변환이다. V037 조명 시험 맵은 여전히 없다.
- 사용자가 보고한 경고는 `ASM_Z1_WallNorth`의 비 Static 루트 `Actor_0.Scene`에 Static `InstancedStaticMesh797`을 붙일 수 없다는 AttachTo 경고였다. 로드/원본 패키지 조회에서 관측, 이번 자세/수 불일치·dirty는 없었다. 기존 Shell 459,537삼각형 Navigation geometry export 경고도 기록했다. 둘 다 이번 단위에서 수정하지 않았다.
- `Saved/Profiling/Graphics/Phase01/U4_0_V001/Recovery/`에 V010·Cull15·원본 조립체/층 입력·최상위 메시·스크립트 등 51파일(974,347,442바이트)을 복사하고 SHA-256 일치를 확인했다. 공유 ED 전체 의존성 아카이브는 아니다. `RecoveryManifest.json`과 `LiveSnapshot_20261124_164625_996859.json`이 근거다. SelectionState의 INI8·정식 풀·V037 JSON 해시도 일치했다.
- 사용자가 Unlit로 전환했다. V010에서 FOV90·1305×724의 부감/분지/협곡 진입/남북 테두리/북쪽 단·남서 접속부 8시점을 촬영했다. `Views/*.png`, `ComparisonViews_V001.json`, `Views/CaptureMetadata_V001.json`에 화면·눈/목표 로컬 좌표·월드 자세를 기록했다. Lit 품질·접근·충돌/PIE의 완료 판정은 아니다.
- Scripts/현재 EDTerrain 작업 폴더 조사에서는 Z2~Z7의 전용 Layout/배치 산출물이 없었다. Z1 안의 CR 산괴/왕관·북쪽 단이 Z3/Z5와 의미상 겹치므로 구역 이름으로 다시 덮지 않고 기존 소유자를 고정한다. 기반면·보존 구조물이 있는 것과 ED 큰 배치가 끝난 것을 구분했다.
- 제안 순서: Z2 하류→석교 아래/상류→Z4 남벽·메사→Z3 남은 사면→Z5 북 대지 외곽→Z6 골짜기/보스→Z7 외곽. NorthLedge·동굴 지붕 아래 동쪽 사면은 다음 구조물 교체 패스의 인터페이스로 구분했다. 마커/동선 보류·빠진 Pod2·산괴 배경 A안은 유지한다.
- **다음 U4-1은 사용자 확인 대기.** `(1200,-2600,H−4046.8)→(-1600,-5400,H−3433.5)` 약40m 하류 한 굽이, 기존 서안 맞은편에 원래 크기 조립체1개부터 시험한다. 새 `U4Layout_V001` 사본/구역 JSON 경로와 완료 기준은 계획 §5다. 몸체·충돌·세부 마감은 후속 단계다.

## 2026-11-24 — 1-BR U4-1 새 사본 Z2 하류 초안 저장·Cmd 검사 대기

- 사용자가 다음 단계 진행을 승인했다. 시작 시 V010·PIE=false·맵 dirty=false를 확인하고 U4-0 복구51파일이 여전히 같은 해시임을 대조했다. 원본 PL_LargeCliff_02d를 읽기 전용으로 열어 크기/형상을 조사했다(전체 bounds 약37.4×33.7×52.8m, 원본 dirty=false). 이전 NoCollision 기록의 `left={}`도 확인했다.
- PL_LargeCliff_02d를 새 `U4Layout_V001/Z2Assembly_V001/PL_Ridge_Z2_LowerBank_V001`로 복제·저장했다. V010은 새 `U4Layout_V001/LVI_Ridge_EDTerrain_U4Layout_V001`로 복제했다. 새 소유자 `ASM_Z2_LowerSouthBank`를 로컬 `(1200,-5800,H−6500)`, 현지 회전 `(0,135,0)`°·스케일1로 추가하고 **새 최상위 맵만 지정 저장**했다. 원본·기존 Cull/지각/충돌·마커·정식 풀은 수정하지 않았다.
- 새 재현 원본은 `Scripts/Profiling/Graphics/LargeLayout_V001/RidgeLayout.json`·`EDTerrainLayout_Z2.json`이다. V037/V010 및 원본/사본 패키지 해시와 결과 자세 초안·협곡 중심선/반폭·후속 몸체로 채울 영역과 비울 통로를 기록했다. `RidgeLargeLayout.py`는 기존 V010 사본에 신규 소유자만 추가하는 build와 보존/상자 검사 뒤 export/verify를 담당한다. 기존 생성기·반입·Cull·충돌·베이크를 호출하지 않는다.
- 초기 새 맵/사본/JSON 4파일을 `Saved/Profiling/Graphics/Phase01/U4_1_V001/InitialDraft/`에 복구용으로 복사했다. 기존과 같은8시점의 화면과 Z2 부감1장을 `Views/`에 저장했다. 기존 분지 정면·남 테두리 화면을 대조했고 새 Z2 초안 부감에 카메라를 남겼다. 새 조립체의 최종 구도는 사용자 채택 전이다.
- 사용자에게 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V001` 실행을 요청했다. **아직 보고서가 없어 기존 시각 지문/새 NoCollision/협곡 보행 띠 검사·확정 export는 대기**다. 통과 여부를 추정하지 않으며 U4-1 완료·U4-2 진행 확인으로 넘어가지 않는다. 검사에서 REVIEW가 나오면 원인부터 보정한다.
- 확인한 것은 Python 구문·새 JSON 파싱·새 패키지 복제/저장 성공과 비교 화면이다. 몸체·충돌·SurfaceData·PIE·패키지 성능 검증은 이번 단위에 추가하지 않았다. 다음 인계는 [계획 §7](../analysis/Phase01_U4LargeLayoutPlan.md)과 Current의 Cmd 검사 대기다.

### 첫 export 오류 — 신규 Level Instance 내부 로드 누락

- 사용자 Cmd 첫 실행은 `owner_actors()`의 `Wait for loaded level: ASM_Z2_LowerSouthBank`에서 중단됐다. JSON export 전 오류이며 배치 수정은 없었다. 엔진 플러그인 `editor_toolset/toolsets/scene.py`의 `create_level_instance`를 확인한 결과 WorldAsset 참조만 지정하고 내부 로드를 요청하지 않았다. 새 배치를 만든 직후 로드 확인을 빠뜨린 것이 원인이다.
- 새 작업본 dirty=false를 확인하고 같은 저장 맵을 다시 열었다. 로그에서 새 조립체 포함 **16개 레벨 로드**를 확인했다. 원본·새 맵의 재생성/저장은 하지 않았다. `RidgeLargeLayout.py`에는 미로드가 있으면 `load_level_instance()`를 요청하고 `LoadRequested` 보고서/같은 Cmd 재실행 안내를 남기며 export 없이 반환하는 처리를 추가했다. 구문 확인을 통과했고 사용자에게 `export V001` 재실행을 요청했다. 최종 검사 결과는 대기다.
- 최초 `Views/`는 새 조립체가 빠진 화면이었다. 신규 배치의 비교 근거로 쓰지 않고 보존한다. 실제 로드 후8시점을 다시 촬영해 `U4_1_V001/Views_Loaded_V001/`와 `CaptureMetadata_Loaded_V001.json`에 기록했다. 이후 비교는 이 자료를 쓴다.

### 재실행 PASS — U4-1 구현/검사·자세 export 완료

- 사용자 재실행 `Export_20261124_174437_554656.json`: PASS, issues없음, exported=true, dirty없음. 기존 시각185,555인스턴스·데칼12·428키 지문이 U4-0과 동일하고 보존51파일 변경0. Pod10·훅6·검토점27, LI/하위레벨16개다.
- 신규 ASM_Z2_LowerSouthBank는8,206인스턴스·174키·데칼0, 지문 `1ebc931549161469de67d3ebeb2f55cb6b100f2d7281d2fda6ff0e410400bff7`, 충돌 활성 컴포넌트0. 협곡63표본과 큰 암석110개 메시 로컬 상자의 겹침0. 실제 로컬 `(1200,-5800,H−6500)`, 회전 `(약1e-6,135,약1e-6)`°·스케일1을 Z2 JSON에 export했다.
- 사용자 경고1건은 기존 북벽 ASM_Z1_WallNorth의 루트 mobility AttachTo 경고다. 이번 검사 불일치는 없었다. 원본은 수정하지 않았다.
- `Views_Loaded_V001/Z2Review.png`·`ReviewCamera_Loaded_V001.json`에 실제 로드 후 검토 시점을 남겼다. 새 조립체를 선택하고 부감 카메라를 맞췄다. U4-1 구현/검사는 마쳤으나 새 구도 육안 채택과 다음 U4-2 진행 여부는 사용자 확인 대기다. 새 충돌/몸체/PIE는 하지 않았다.

## 2026-11-24 — U4-1 사용자 채택·U4-2 V002 새 큰 배치(검사 대기)

- 사용자가 U4-1 초안을 채택하고 다음 단위를 승인했다. 시작 시 V001·PIE=false·dirty=false를 확인했다. V001 맵·JSON·README·당시 공용 스크립트를 `U4_2_V002/PreviousAccepted/`에 복사했다. V002 manifest는 V001 맵/저작 파일 해시와 하류 소유자의 채택 자세·결과 지문을 고정한다. 공용 스크립트는 버전별 manifest/보호 공간을 지원하도록 확장했으며 원본 V001 맵/저작 파일은 수정하지 않았다.
- 원본02e·01c를 읽기 전용으로 열어 bounds/형상을 조사하고 각각 dirty=false를 확인했다. 전체 bounds02e 약41.0×34.8×49.1m, 01c 약23.3×21.2×41.1m. 별도 둑 조립체3종은 현재 반입돼 있지 않아 재반입/준비 생성은 하지 않았다. 현재 팔레트의02e 독립 받침과01c 독립 암괴를 새 사본으로 사용했다.
- V001을 `U4Layout_V002/LVI_Ridge_EDTerrain_U4Layout_V002`로 복제했다. 새 소유자는 `ASM_Z2_BridgeSouthButtress`(02e, 로컬−7000/−10200/H−400, yaw90°), `ASM_Z2_UpperSouthShoulder`(01c, −12900/−9800/H−1600, yaw80°), 둘 다 스케일1·tilt0다. 기존8개 메시·15개 Cull·하류 소유자를 유지했다. 원본/공유 에셋·기존 생성기·지각·충돌·베이크·마커·풀은 바꾸지 않았다.
- 받침 절벽 첫 초안H−1800은 메사에 대부분 묻혔다. 보정 전 맵/JSON을 `BeforeHeightAdjustment/`에 보존하고 새 받침만14m 올려H−400으로 저장했다. 현재 자세는 사용자 채택 전이다. 긴 벽을 같은 조립체 행렬로 채우지 않았다. 연속 외피/세부 바닥·몸체·충돌은 후속이다.
- 지정한 새 맵만 저장한 뒤 다시 열어18개 하위 레벨 로드·dirty=false를 확인했다. `Views_Initial/`은 높이 보정 전4시점, `Views_FinalDraft/`는 보정 후 기존8+신규4시점이다. 원본 V001 맵/저작 파일 해시 불변을 재확인했다. BridgeOverview 부감에 카메라를 남겼다.
- 새 저작본은 `Scripts/Profiling/Graphics/LargeLayout_V002/`이다. 검사 입력에 RiftCanyon 중상류, SkyBridge 갑판 위/아래, 실제 CanyonChamber Pod의9m 원판, MassifCavern 남쪽 접근, MesaAscent를 추가했다. 기존 채택 하류는 자세/참조/결과 지문을 별도로 고정 대조한다. 상자 검사는 충돌/PIE 판정이 아니다.
- 사용자에게 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V002` 실행을 요청했다. **현재 최종 검사/실제 자세 export 결과 대기**이며 U4-2 완료로 표시하지 않는다. 구문·새 JSON 파싱과 복제/저장·로드/촬영은 확인했다. 검사·육안 확인 뒤 U4-3 Z4 남벽/메사 북면 진행 여부를 확인한다.

### U4-2 첫 검사 REVIEW·새 받침 위치/방향 보정

- 사용자 실행 `Export_20261124_193542_446950.json`: 기존 Z1 지문·채택 하류·보존51파일·Pod10/훅6/검토점27·18개 로드·모든 시각 NoCollision/개수는 통과했다. 새02e 받침만 MesaAscent 상자 겹침49건이라 REVIEW, exported=false였다. 나머지 보호 공간은 모두0이다. 경고1건은 기존 북벽 AttachTo다.
- 기존 Cull 전02e 월드 자세 덤프를 새 자세로 옮긴 읽기 전용 좌표 대조에서 큰 암석107개·기존49건을 동일하게 재현했다. 후보를 비교하고 Pod 위쪽도 추가 표본 검사했다. 원본/기존 배치를 삭제하거나 보호 폭을 줄이지 않았다. 근거 `DiagnosePlacement*.py`·`PlacementCandidates*.json`은 로컬 Saved다.
- 새 받침만 로컬 `(−7850,−9800,H−600)`, yaw100°로 보정했다(기존에서 서8.5m·북4m·아래2m·yaw+10°). 원래 크기1·tilt0과 상류01c·채택 U4-1·Z1은 그대로다. 수정 전 맵/JSON은 `BeforeClearanceCorrection/`에 보존하고 새 맵만 지정 저장했다. Pod9m 원판은 폭을 유지한 채+1~61m를5m 간격13층으로 검사한다.
- 후보 좌표 대조는 기존 여섯 보호 공간/추가 Pod 위쪽에서 겹침0(`PlacementCandidates_G.json`)이다. 사용자에게 같은 `export V002` 재실행을 요청했으며 실제 검사/export는 대기다. 현재 비교12장은 `Views_ClearanceCorrected/`, 이전 `Views_FinalDraft/`는 수정 전 기록이다. 카메라는 BridgeOverview다.

### U4-2 보정 재검사 PASS·실제 자세 export 완료

- 사용자 재실행 `Export_20261124_194856_426015.json`: PASS, issues없음, exported=true, dirty없음. 기존 Z1 지문185,555인스턴스·데칼12와 채택 U4-1의8,206인스턴스 지문/자세/참조가 동일하다. 보존51파일 변경0, Pod10·훅6·검토점27·18개 LI/하위 레벨 유지다.
- 새 받침9,148인스턴스·187키 지문 `6ba107dde77a0c56b3c05e4b7617c4324af01a2bce464de55515578584ab0b2b`, 상류830인스턴스·31키 지문 `a47ca98c89b19d193ea9860f79173622a0c6595ead1597ee7dd9466b34bd7140`. 둘 다 데칼0·충돌 활성 시각 컴포넌트0이다.
- 새2개 각각 하류63·중상류432·교량132·Pod637·동굴 접근120·MesaAscent810표본에서 겹침0. 검사한 큰 암석은 받침107개·상류68개다. 기존 하류63표본 겹침0도 유지했다. 보호 폭/50cm 상자 여유를 줄이지 않았다.
- 실제 받침 `(−7850,−9800,H−600)`, 현지 yaw100°·상류 `(−12900,−9800,H−1600)`, yaw80°·스케일1을 V002 JSON에 export했다. pitch 약1e-6°는 축 변환 오차다. 수정 뒤12시점 `Views_ClearanceCorrected/`와 BridgeOverview 카메라가 검토 근거다.
- U4-2 배치 저장·보존/보호 검사·export는 완료했다. 새 구도 육안 채택과 다음 U4-3 Z4 협곡 남벽/메사 북면 진행 여부는 사용자 확인 대기다. 새 몸체/충돌/SurfaceData·PIE·세부 마감은 하지 않았다.


## 2026-11-24 — U4-2 사용자 확정·U4-3 V003 메사 북동쪽 배치

- 사용자가 U4-2를 확정하고 U4-3 진행을 승인했다. V002 맵/저작3파일을 `U4_3_V003/PreviousAccepted/`에 보존하고 해시를 기록했다. V003 manifest는 채택 Z2 세 소유자의 자세/참조/지문과 V001/V002 파일 해시를 고정한다.
- 기존 덤프의02c·01b를 읽어 원래 내부 자세를 새 현지 좌표로 옮긴 후보를 대조했다. 최종 후보 두 개 모두 여덟 보호 공간 겹침0이다. 생성기·Cull·기존 배치 재실행이나 인스턴스 삭제는 하지 않았다. 근거 `CandidateSurvey_B.json`이다.
- 새 `U4Layout_V003/LVI_Ridge_EDTerrain_U4Layout_V003`는 V002 사본이다. `ASM_Z4_MesaNorthEastWall`은 PL_LargeCliff_02c 사본·로컬−1800/−10000/H−700·yaw80°, `ASM_Z4_MesaEastCrown`은01b 사본·−2600/−11800/H1000·yaw25°, 모두 스케일1·tilt0다. 새 사본은 `Z4Assembly_V003/`이며 기존/공유 에셋을 수정하지 않았다.
- 새 맵만 지정 저장하고 다시 열어20개 하위 레벨 로드·dirty=false를 확인했다. 기존8+메사4 비교화면을 `Views_Draft/`에 저장했다. 현재 카메라는 MesaNorthFace, 신규 두 소유자를 선택했다. 기존 북벽 AttachTo 경고는 계속 관측되며 원본은 고치지 않았다.
- 저작본 `LargeLayout_V003/`은 기존 Z2 JSON도 자체 사본으로 소유한다. 기존 여섯 보호 공간 외에 SouthMesa Pod 반경9m·+1~61m13층과 석교 남단 접근 반폭450cm를 추가했다. 통로/Pod 폭과50cm 상자 여유는 유지한다. 표본 상자 검사이며 충돌/PIE 통과가 아니다.
- 사용자에게 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V003` 실행을 요청했다. 현재 실제 보존/보호 검사·자세 export는 대기다. 통과 뒤 구도/다음 U4-4 오름길·남쪽 외곽 진행을 확인한다. 몸체·충돌·SurfaceData·지각 재빌드·마커·풀·세부 마감은 하지 않았다.


### U4-3 실제 검사 PASS·자세 export 완료

- 사용자 실행 `Export_20261124_221922_034419.json`: PASS, issues없음, exported=true, dirty없음. 기존 Z1의185,555인스턴스·데칼12 지문과 채택 Z2 세 소유자의 자세/참조/지문 불변, 보존51파일 변경0이다. 이전 저작 파일 해시도 불변이며 Pod10·훅6·검토점27·20개 LI/하위 레벨을 유지한다.
- 신규 북동쪽 절벽8,723인스턴스·174키 지문 `629396ff51f02c29841f1e376cc8e79cde23dab3ff690b90c6922307b5dc01f4`, 상단 암괴505인스턴스·35키 지문 `824504274c9a9fd8b4e7738a493f6fd11243f439b634b43e47749a6599bb9e40`. 모두 데칼0·충돌 활성 시각 컴포넌트0이다.
- 새2개 각각 하류63·중상류432·교량132·협곡 Pod637·동굴 접근120·MesaAscent810·SouthMesa Pod637·남단 접근66표본에서 겹침0이다. 큰 암석107/69개를50cm 여유 상자로 검사했다. 실제 충돌/PIE 판정은 아니다.
- 실제 자세 `(−1800,−10000,H−700), yaw80°`와 `(−2600,−11800,H1000), yaw25°`, 스케일1을 V003 Z4 JSON에 export했다. tilt 약±1e-6°는 축 변환 수치 오차다. V003 Z2 사본도 실제 기록과 일치하며 원본 V002는 동결했다.
- 경고1건은 기존 ASM_Z1_WallNorth의 비 Static 루트 AttachTo 경고다. 이번 대조 불일치는 없고 원본은 수정하지 않았다. U4-3 배치/검사/export는 완료했으며 새 구도 채택/다음 U4-4 메사 오름길·남쪽 외곽은 사용자 확인 대기다. 몸체·충돌·베이크·식생/잔돌 마감은 후속이다.


## 2026-11-24 — U4-3 사용자 확정·U4-4 새 세션 인계와 일괄 커밋

- 사용자가 U4-3 V003 구도를 확정하고 U4-4부터 새 세션으로 넘기도록 지시했다. U4-4 배치·V004 생성은 이번 세션에서 시작하지 않았다. 마지막 실제 상태는 V003·PIE=false·dirty=false·MesaNorthFace 카메라다.
- 실제 export 자세/결과는 유지하고 V003 manifest/구역 JSON의 채택 상태와 README를 갱신했다. V001/V002 저작 파일·맵은 불변이다. 다음 V004에서는 Z2 세 소유자와 Z4 두 소유자를 모두 고정하고 채택 V003 맵/파일 해시를 `previous_unit`에 설정한다.
- 현재 인계 원본 `analysis/Phase01_U4LayoutHandoff.md`에 채택본·5배치 자세/지문·최종 파일 해시·보고서/비교/복구 경로·여덟 보호 공간·도구 함정·U4-4 첫 범위와 시작 프롬프트를 모았다. Current·README·Roadmap·상위 Phase·U4 계획·이전 인계는 현재 진입점을 가리키도록 갱신했다. 중간 대기 기록은 시행 경과로 보존한다.
- 세션 종료 채택 사본은 `U4_3_V003/AcceptedHandoff_V001/`·`AcceptedHandoffHashes_V001.json`에 보존한다. Content/Saved의 에셋·보고서·이미지는 기존 Git 제외 규칙을 유지하며 문서/스크립트/버전별 JSON을 한 커밋으로 묶는다. 커밋 메시지는 최근 프로젝트의 `feat: 한글 요약(GX-번호)`와 항목별 본문 양식을 따른다.
- 최종 확인: Python 구문/버전별 JSON 파싱 PASS, 채택5배치 실제 자세·지문/보호 검사 PASS, 입력/동결/복구 파일 해시71건 불변, 새 인계/계획/이전 인계의 문서 링크16개 유효. 채택 복구12파일(2,395,439바이트)의 원본/복사 해시 동일. 근거 `U4_3_V003/FinalValidation_V001.json`이다. Git 공백/스테이징 범위를 확인하고 문서/스크립트/JSON만 커밋한다. U4-3 이후 에셋 수정·생성기·몸체/충돌·베이크·PIE 재실행은 하지 않았다.

- 커밋 전 EOL 검사에서 V001/V002 README만 원래 LF이고 core.autocrlf=true로 재체크아웃 시 CRLF가 될 수 있음을 확인했다. 동결 파일을 바꾸지 않고 `.gitattributes`에서 이 두 파일만 LF로 고정해 manifest의 원시 파일 해시를 보존한다.

## 2026-11-24 — U4-4 V004 메사 오름길·남쪽 외곽 초벌

- HEAD `35657fe`·작업 트리 깨끗함, 실제 MCP 대상 V003·PIE=false·V003 dirty=false·정식 풀·EnemyDensity1을 대조했다. 사용자 `verify V003`은 PASS·미저장 패키지없음·기존 Z1 지문/채택5소유자/51파일 불변·하위 레벨20개였다. V004와 U4-4 결과는 없었다. U4-0이나 기존 생성기를 재실행하지 않았다.
- 원본 Cull 전 덤프와 현재 기반면 함수를 읽어 MesaRamp1 H84→1721·1단H1862~1975·MesaRamp2 끝H3840·SouthMesa Pod H3862 관계를 조사했다. 서쪽 어깨 후보는 MesaAscent 상자 겹침으로 기각했다. 보호 폭/50cm 여유를 유지하고 1단 남쪽 끝 암괴+상단 메사 바깥 남쪽 절벽으로 첫 추가 범위를 좁혔다. 후보 대조는 `U4_4_V004/SurveyCandidates*.py`·`CandidateSurvey*.json`·`GroundSurvey.json`이다.
- 채택 V003 맵/저작본/전용 사본/보고서 등11파일을 `PreviousAccepted/`에 복사하고 해시를 기록했다. 새 `U4Layout_V004/LVI_Ridge_EDTerrain_U4Layout_V004`는 V003 사본이다. `LargeLayout_V004/`는 독립 Z2/Z4 JSON을 가지며 manifest 이전 채택 맵/저작 해시를 V003으로 갱신하고 Z2 세+Z4 두 소유자의 자세/참조/결과 지문을 모두 고정했다. V003의 V002 기준 제작 이력을 그대로 복사해 중복 생성하지 않았다.
- 신규 `ASM_Z4_MesaRampToe`: PL_LargeCliff_01c 전용 사본·로컬 `(-9900,-15100,H-1400)`·yaw−90°·스케일1. 신규 `ASM_Z4_MesaSouthWall`: PL_LargeCliff_02d 사본·`(-6500,-14500,H-1200)`·yaw−90°·스케일1. 원래 내부 배치/크기·공유 메시/재질/식생 층은 유지했다. 남 테두리 띠·기존 조립체·지각·프록시·SurfaceData·마커·풀·밀도는 바꾸지 않았다.
- 지정한 새 맵만 저장하고 다시 열어 신규2개 로드를 확인한 뒤 비교12시점을 촬영했다. 사용자 `export V004` 결과 `Export_20261124_232358_333279.json`은 PASS·issues없음·exported=true·dirty없음이다. 기존185,555인스턴스·데칼12·428키 지문과 채택5소유자의 자세/참조/내용·보존51파일이 동일하다. Pod10·훅6·검토점27·LI/로드22개다.
- 새 암괴830인스턴스·31키·지문 `1afb1d255002421a93e3ad57cd4c2df4a7658af97b2440929365b79fe56f362a`, 새 남벽8,206인스턴스·174키·지문 `910b0353d1a9386cfc105493c9d7c5be4947dc7a39dfe9c6bec85febd0d67c8a`다. 모두 데칼0·충돌 활성 시각 컴포넌트0이다. 실제 자세를 V004 JSON에 export했고 tilt 약−1e-6°는 현지 축 오차다.
- 각 신규 소유자는 하류63·중상류432·석교132·협곡 Pod637·동굴 접근120·MesaAscent810·SouthMesa Pod637·석교 남단 접근66표본에서 겹침0이다. 큰 암석68/110개를 검사했다. 원본 전체 상자의 seam 최소 거리는 약64.36/61.46m다. 보호 검사는 메시 로컬 상자+50cm 여유의 배치 표본이며 실제 충돌/PIE 판정이 아니다.
- 사용자 워닝1건은 기존 북벽 비Static `Actor_0.Scene`→Static `InstancedStaticMesh797` AttachTo다. 새 배치 경고는 확인되지 않았다. 기존 Shell459,537삼각형 경고는 맵 로드에서 관측됐다. 원본은 수정하지 않았다.
- `Views_Before/`는 V003에서 신규4시점, `Views_Draft/`는 기존8+메사4시점이다. `ReviewCameras.json`·`CaptureMetadata_{Before,Draft}.json`·`Comparison.html`에 같은 Unlit/FOV90°/1305×724 전후 비교를 정리했다. 기존 분지 정면·남북 테두리의 주요 윤곽을 유지했다. 뷰포트는 신규 두 소유자 선택·MesaRampOverview 부감으로 남겼고 맵 dirty=false다.
- `FinalValidation_V001.json`은 실제 export/JSON 자세 일치·기존 복구51파일·채택 인계11파일·이전 저작11파일·12시점 해상도/FOV를 대조했다. Current·인수인계·U4 계획§11·Phase/README/Roadmap을 현재 상태로 갱신했다. **U4-4 구도 채택과 다음 U4-5 북쪽 단 뒤→안부 진행 확인은 대기**다. 몸체/충돌/지각 재빌드·PIE·세부 식생/잔돌 마감은 전체 큰 배치 확정 뒤로 유지한다.

## 2026-11-24 — U4-4 사용자 채택·U4-5 V005 북쪽 단 뒤→안부

- 사용자가 V004를 채택하고 다음 진행을 승인했다. 실제 V004·PIE=false·맵 dirty=false를 확인한 뒤 실제 자세는 유지하고 채택 메타데이터만 갱신했다. 채택 V004 관련11파일은 `U4_5_V005/PreviousAccepted/`·`PreviousAcceptedHashes.json`으로 동결했다. `U4_4_V004/AcceptedHandoffHashes_V001.json`에도 채택 해시를 기록했다.
- 북쪽 단 뒤 H약1950→NorthSaddle H약3740의 기반면 관계를 조사했다. 실제 NorthSaddle Pod는 MCP transform을 로컬로 환산한 `(-5628,9248.1,H3739.3)`이며 설계 위치와 달랐다. 이 실제 중심으로9m·지면+1~61m 13층을 보호했다. 북서 고개 접근816·동쪽 출구222·부유섬196·동굴 서쪽 입구156·잔도 인터페이스300표본을 기존8공간에 더해 총14공간·5,224표본을 쓴다.
- 원본 덤프의01c/01b 조립체 내부 자세를 새 현지 Up 자세로 옮겨 후보를 대조했다. 안부 출구와1겹침이 있던 `(-3000,8500,H100), yaw50°`는 기각했다. 보호 폭·Pod 반경·50cm 상자 여유를 줄이지 않았다. `GroundSurvey.json`·`CandidateSurvey.json`·`ProtectionDraft.json`이 근거다. U4-0·기존 생성기·Cull·층 재실행이나 개별 삭제는 하지 않았다.
- V004를 새 `U4Layout_V005/LVI_Ridge_EDTerrain_U4Layout_V005`로 복제했다. `LargeLayout_V005/`는 기존 Z2/Z4의 독립 JSON 사본과 새 Z3 JSON을 가지며 manifest 이전 채택 맵/저작 해시와7소유자 자세/참조/결과 지문을 V004 기준으로 고정한다.
- 신규 `ASM_Z3_NorthRearShoulder`: PL_LargeCliff_01c 전용 사본·로컬 `(-5600,6200,H200)`·yaw−30°·스케일1. 신규 `ASM_Z3_SaddleSouthEast`:01b 사본·`(-3100,8000,H0)`·yaw50°·스케일1. 원본 상자 상단 H약3782/4024이며 기존 북쪽 단/산괴의 큰 윤곽과 안부 공간을 유지했다. 두 사본은 `Z3Assembly_V005/`이고 원래 내부 배치/공유 메시/재질/식생 층은 보존했다.
- 새 맵만 지정 저장·재로드해 신규 로드와24개 하위 레벨을 확인했다. 사용자 `export V005`의 `Export_20261124_233749_888432.json`은 PASS·issues없음·exported=true·dirty없음이다. 기존185,555인스턴스·데칼12·428키 지문, 채택7소유자 자세/참조/지문·보존51파일 불변이다. Pod10·훅6·검토점27도 유지했다.
- 새 북쪽 어깨830인스턴스·31키·지문 `a3a0744155971e685bf21572a51946541ba9ef00c82e076253f6e9f54ea61ce5`, 동남쪽 암괴505인스턴스·35키·지문 `ae5e4c005a99b287f9c86c75bba8afbe7b5ea2d766ccec78a659c7a32ab079f7`다. 데칼0·충돌 활성 시각 컴포넌트0, 각14공간·5,224표본과 큰 암석68/69개에서 겹침0이다. 원본 전체 상자의 seam 최소 거리는 약98.92/102.81m다. 실제 충돌/PIE는 후속이다.
- 실제 자세를 V005 Z3 JSON에 export했다. tilt 약±1e-6°는 현지 축 오차다. 워닝1건은 기존 Z1 북벽 비Static 루트와Static `InstancedStaticMesh797` AttachTo이며 새 배치 경고는 없다. V004와 이전 에셋을 수정하지 않았다.
- 비교12시점(기존8+북쪽4)은 `Views_Draft/`, V004에서 신규4시점의 전 화면은 `Views_Before/`다. `ReviewCameras.json`·`CaptureMetadata_{Before,Draft}.json`·`Comparison.html`에서 같은 Unlit/FOV90°/1305×724 전후를 대조한다. 기존 분지 정면·남북 테두리·산괴 윤곽을 유지했다. 마지막 뷰포트는 신규 두 소유자 선택·NorthRearOverview 부감·dirty=false다.
- `FinalValidation_V001.json`은 실제 export/JSON 자세·복구51파일·채택 인계11파일·이전 저작15파일·12시점 해상도/FOV를 대조해 PASS했다. Current·인수인계§0·U4 계획§12·Phase/README/Roadmap을 현재 기준으로 갱신했다. **V005 구도 채택 및 다음 U4-6 서쪽 접근 사면 진행 확인은 대기**다. 몸체·충돌·지각/SurfaceData 재빌드·PIE·세부 마감은 전체 Ridge 큰 배치 초벌 확정 뒤로 유지한다. Crown/WestStair 동선 재정의·제외2Pod도 보류다.

## 2026-11-25 — U4-5 사용자 채택·U4-6 V006 서쪽 접근 사면

- 사용자가 V005 신규 두 소유자를 에디터에서 확인하고 채택·다음 진행을 승인했다. V005·PIE=false·맵 dirty=false를 확인했다. 실제 자세는 유지하고 채택 메타데이터만 갱신해 동결했으며 `U4_6_V006/PreviousAccepted/`·해시 목록에 관련12파일을 보존했다.
- 현재 기반면의 서쪽 낮은 진입 H약0과 사면 H약500~3000을 대조했다. 기존14공간에 낮은 진입/북 외곽/협곡 외곽 경로·실제 서쪽 섬 Pod `(-14849.2,4724,H6000.3)`·동굴 내부 방 남북2영역을 추가했다. 방 바닥−840~천장510 사이3층 표본과1/3겹침이 있던 발치 후보를 기각했다. 보호 폭/Pod9m/50cm 여유를 줄이지 않았다. 로컬 조사 JSON·기존 원본 덤프를 읽었으며 기존 생성기·층·Cull은 재실행하지 않았다.
- V005를 새 `U4Layout_V006/LVI_Ridge_EDTerrain_U4Layout_V006`로 복제했다. `LargeLayout_V006/` manifest는 이전 채택본과9소유자의 자세/참조/지문을 V005 기준으로 고정하며 모든 구역 JSON은 독립 사본이다. 새 낮은 암괴는01c 사본 `ASM_Z3_WestFootShoulder`·`(-11800,-800,H-1600)`·yaw160°, 상단 어깨는01b 사본 `ASM_Z3_WestUpperShoulder`·`(-10400,1800,H-900)`·yaw150°, 모두 크기1이다. 원본 내부 배치/공유 메시/재질/식생 층을 유지했다.
- 새 맵만 저장·재로드했고 사용자 `export V006`의 `Export_20261125_002236_501185.json`은 PASS·issues없음·exported=true·dirty없음이다. 기존 Z1의185,555인스턴스·데칼12·428키 지문, 채택9소유자 자세/참조/지문·보존51파일 불변이다. Pod10·훅6·검토점27·LI/로드26개다.
- 신규830/505인스턴스·31/35키·데칼0·충돌 활성 시각 컴포넌트0, 각각20개 보호 공간·6,995표본과 큰 암석68/69개에서 겹침0이다. 원본 전체 상자의 seam 최소 거리는 약58.68/68.21m다. 지문은 낮은 암괴 `79c4f448a8eb975f8c6b7eb57ef1c3a4220322a5d11c8ef885f5ddc45d2efbce`, 상단 어깨 `0604312dc5123e3b84d6977f1e0f68713112b3e417ec578b1347fb265fdb2ffc`다. 실제 자세를 V006 Z3 JSON에 export했다. 실제 충돌/PIE 검증은 후속이다.
- 워닝1건은 기존 Z1 북벽 비Static 루트→Static `InstancedStaticMesh797` AttachTo다. 새 배치 경고는 확인되지 않았다. V001~V005·원본/공유 에셋·지각/충돌/SurfaceData·마커/풀/밀도·기존 층/생성기는 수정하지 않았다.
- 기존8+서쪽4시점의 전후를 `Views_Before/`·`Views_Draft/`·`Comparison.html`과 카메라/메타데이터로 정리했다(Unlit/FOV90°/1305×724). 마지막 에디터에는 신규 두 소유자 선택·WestLowEntry 시점을 남겼고 선택 화면은 `SelectedWestLowEntry.png`다. 기존 산괴/왕관의 큰 윤곽을 늘리지 않았다.
- `FinalValidation_V001.json`: export/JSON 자세·복구51파일·채택 인계12파일·이전 저작20파일·12시점 대조 PASS다. Current·인수인계§0·U4 계획§13·Phase/README/Roadmap을 갱신했다. **V006 구도 채택과 다음 U4-7 북 대지 외곽 진행 확인은 대기**다. 전체 큰 배치 확정 뒤 몸체·충돌·전체 베이크/PIE·세부 식생/잔돌 마감 순서를 유지한다.

### 2026-11-25 — V006 기존 경사로 제보·보호 검사 누락 확인

- 사용자가 선택한 낮은 신규 암괴 LevelInstance_24의 기존 경사로 가림을 제보했다. 실제 transform은 export값과 같았다. 사용자 선택/카메라를 읽고 현재 화면을 `U4_6_V006/RouteReview_V001/UserSelectedView.png`에 보존했다. 배치/맵·선택·카메라는 변경하지 않았다.
- 기존 StairRamp1/2가20개 보호 공간에서 누락돼 있었다. WestStair 재정의 보류를 현존 경사로 보호 생략으로 잘못 적용한 것이다. 낮은 암괴는StairRamp1 660상자/표본쌍·중심18측점·약16.9m 범위, 상단 어깨는StairRamp1 끝51쌍·약4.0m와StairRamp2 720쌍·중심23측점·약22.0m 범위에서 겹쳤다. StairRamp3/4는 신규2개와 겹침0이다.
- 재대조 `ReviewWestRamps.py`·`WestRampOverlap.json`은 원본 상자를 V006 export자세로 옮겨50cm 여유·100cm 종간격·중앙/±350cm·현재 지면+100/250cm를 검사했다. 중심 범위 길이는 보수적 상자 표본 수치다. 실제 암석 표면 점유 길이·물리 충돌/PIE 판정은 아니다. 신규는 아직 NoCollision이다.
- 이전 Export/FinalValidation PASS를 보존하되 그 검사 범위가 불완전했음을 기록했다. V006 manifest/Z3를 `review_existing_ramp_overlap`으로 갱신하고 Current·계획§13·인수인계·Phase/README/Roadmap을 REVIEW로 정정했다. 기존 경사로 보존을 포함한 신규2개 보정·재검사와 재채택이 필요하며 U4-7은 대기다.

## 2026-11-25 — GX-035 시각 우선 경사로 보정·U4-7 V007 북 대지 외곽

- 사용자가 비주얼을 동선 완전 보존보다 우선하며 가장자리 겹침은 허용하고, 새 배치 보정 또는 별도 경사로로 해결한 뒤 다음으로 진행하도록 승인했다. GX-035로 기록했다. 이번 초벌2개의 자세를 조정해 중앙3m 통행 띠를 확보했다. 다른20공간의 보호 폭/여유는 유지했다.
- 낮은 암괴는 `(-11800,-800,H-1600)`→`(-13200,400,H-1600)`로 서14m·북12m, 상단 어깨는 `(-10400,1800,H-900)`→`(-9000,1600,H-300)`로 동14m·남2m·위6m 보정했다. yaw160/150°·크기1·내부 구성/식생 층과 기존 채택9소유자는 유지했다. 보정 전 저작5파일/맵은 `U4_6_V006/RouteCorrection_V001/BeforeCorrection/`에 보존했다.
- 실제 `Export_20261125_004223_261540.json`: PASS·issues/dirty없음·exported=true·새2개 각각24공간 겹침0, 기존 Z1/9소유자/51파일 불변·LI/로드26개. 사용자의 수정 후 계속 진행 지시에 따라 보정 V006을 확정·동결했다. 기존7m 띠 가장자리 겹침을 허용하는 대신 중앙3m를 본 구현 선택이며 실제 충돌/PIE는 후속이다. 별도 경사로/지각/몸체/충돌 재빌드는 하지 않았다.
- 새 V007은 보정 V006 사본이다. manifest는 V006 맵/저작본과11소유자 자세/참조/지문을 고정하고 모든 구역 JSON은 독립 사본이다. 채택 V006 관련12파일 복구본과 해시를 `U4_7_V007/PreviousAccepted/`에 남겼다.
- 새 Z5 `ASM_Z5_TablelandEastRim`:02c 사본 `(7500,9500,H-1800)`·yaw15°·크기1, `ASM_Z5_TablelandNorthRim`:01c 사본 `(2500,13500,H-1500)`·yaw−45°·크기1. 실제 북 대지 Pod `(1904.9,6998.5,H1958.2)`·안부/북동 접근·동굴 지붕/개구부/방을 포함해30공간을 검사했다. 북동 진입로1겹침 후보는 기각했다.
- `Export_20261125_004807_896252.json`: PASS·issues/dirty없음·exported=true, 기존185,555인스턴스·데칼12 지문·11소유자 자세/참조/지문·51파일 불변, Pod10/훅6/검토점27·LI/로드28개다. 신규8,723+830인스턴스·174/31키·데칼0·충돌 활성 시각 컴포넌트0, 각30공간·10,105표본·큰 암석107/68개 겹침0이다.
- 신규 지문은 동쪽 절벽 `c1abf4a3f82e877b3d4a43de4ebf005b5e3b8075920f55114aad7fa889a9940a`, 북쪽 암괴 `6348c1f7f2c47281bc06e3f73b8c7dd45ecf1326a1d113171214c80b3a9779fe`다. 실제 자세를 Z5 JSON에 export했다. 워닝1건은 기존 북벽 AttachTo이고 새 배치 경고는 없다.
- 기존8+서쪽4+북 대지4의16시점과 V006 보정본의 이전 화면을 대조했다. 현재 뷰포트1127×724·Unlit/FOV90°로 맞췄으며 `Comparison.html`·메타데이터·`SelectedTablelandEast.png`에 정리했다. 마지막 상태는 V007·PIE=false·dirty없음·신규 Z5 두 소유자 선택·TablelandEast다.
- `FinalValidation_V001.json`: export/JSON 자세·복구51파일·채택12파일·이전 저작25파일·16시점 대조 PASS. Current·인수인계·계획§13~14·Phase/README/Roadmap과 Decisions GX-035를 갱신했다. V007 구도 채택/U4-8 ValleyRise 측면/입구 진행 확인은 대기이며 몸체/충돌/전체 베이크/PIE·세부 마감은 후속이다.

## 2026-11-25 — U4-7 사용자 채택·U4-8 V008 골짜기 측면/입구

- 사용자가 V007을 확인·채택하고 다음 진행을 승인했다. V007·PIE=false·맵 dirty=false를 대조하고 자세를 유지한 채 채택 메타데이터를 갱신·동결했다. 관련13파일을 `U4_8_V008/PreviousAccepted/`·해시 목록으로 보존했다. 새 V008 manifest는 V007 맵/저작본과13소유자 자세/참조/지문을 고정하며 기존 구역 JSON은 독립 사본이다.
- 기존 BossValley 경사로−4000→−2400과 실제 ValleyRise Pod `(8225.5,-749.8,H-2402.5)`·부유섬 Pod `(7792.6,-5498.9,H5500.3)`·앵커 `(8700,-3800,H8500.475)`를 읽었다. 실제 설계 loft 고리로 Hall 동쪽 통로 바닥~천장을 대조하고 그래플은 실제 월드 직선을 로컬 표본으로 환산했다. 남동 하강/잔도 출구 등과 합계40공간이다. 가까운 북동 후보가 동굴 통로26표본쌍과 겹쳐 기각했다.
- 신규 `ASM_Z6_ValleyLowerSouth`:02d 사본 `(5400,-3000,H-5900)`·yaw90°·크기1, `ASM_Z6_ValleyUpperNorthEast`:01b 사본 `(11200,2800,H-3000)`·yaw−135°·크기1. 기존 경사로/동굴/부유섬·원본/공유 에셋·기존 층/생성기는 변경하지 않았다. 몸체/접지는 Z6 handoff에 남기고 보스 고원 본체는 U4-9로 둔다.
- 새 맵만 저장·재로드한 뒤 사용자 `Export_20261125_014700_368095.json`: PASS·issues/dirty없음·exported=true. 기존185,555인스턴스·데칼12 지문·13소유자 자세/참조/지문·보존51파일 불변, Pod10/훅6/검토점27·LI/로드30개다. 신규8,206+505인스턴스·174/35키·데칼0·충돌 활성0, 각40공간·13,912표본·큰 암석110/69개 겹침0이다. 실제 충돌/PIE는 후속이다.
- 신규 지문: ASM_Z6_ValleyLowerSouth `4ac37bb935d265e78b12f85bed3aa5d3e81aa76fcd31a6aba6dce75eafbc091b` / ASM_Z6_ValleyUpperNorthEast `1c9ed058b5182f8572fe04affb495e5562da909e3dfc91b9ee4ac89b3a2acd5e`. 실제 자세를 Z6 JSON에 export했다. 워닝1건은 기존 북벽 AttachTo이며 새 배치 경고는 없다. 기존 지각/충돌/SurfaceData·마커/풀/밀도·V001~V007은 보존했다.
- 기존8+북 대지4+골짜기4의16시점과 V007 이전 화면을 Unlit/FOV90°/1127×724로 대조했다. `Comparison.html`·카메라/메타데이터·`SelectedValleyOverview.png`에 정리했고 신규 두 소유자 선택·ValleyOverview·PIE=false·dirty없음으로 남겼다.
- `FinalValidation_V001.json`: export/실제 자세·복구51파일·채택13파일·이전 저작31파일·16시점 대조 PASS. Current·인수인계·계획§15·Phase/README/Roadmap을 갱신했다. V008 구도 채택/U4-9 BossPlateau 진행 확인은 대기다. 전체 큰 배치 확정 뒤 몸체·충돌·전체 베이크/PIE·세부 마감 순서를 유지한다.

## 2026-11-25 — U4-8 사용자 채택·U4-9 V009 보스 고원 측면

- 사용자가 V008을 채택하고 U4-9를 승인했다. 실제 V008·PIE=false·맵 dirty=false·채택15소유자 자세와21에셋 dirty없음을 대조했다. 채택 메타데이터만 갱신하고14파일 복구본/해시를 남겼다. V009 manifest는 V008 맵/저작본과15소유자·누적 이전 저작38파일을 고정한다.
- 새 V009에 남쪽02c 사본 `(13900,-3200,H-3300)`·yaw30°와 북쪽01c 사본 `(14900,3200,H-1600)`·yaw−60°를 원래 크기로 더했다. 신규9,553인스턴스이며 내부 식생 층을 유지한다. 기존 BossValley 경사로와 상단 중앙14m·진입 반경15m·부유섬 그래플 사선을 비웠다. 검토점 H200과 지면 H0을 구분해 지면 눈높이 사선을 검사했다.
- 저장·재로드 뒤 사용자 `Export_20261125_015907_376276.json`: PASS·issues/dirty없음·exported=true. 기존 Z1/15소유자/51파일 불변, Pod10/훅6/검토점27·LI/로드32개, 신규 각43공간·14,752표본·큰 암석107/68개 겹침0·데칼0·NoCollision이다. 워닝1건은 기존 북벽 AttachTo다. 실제 자세를 V009 Z6 JSON에 export했다.
- 지문: ASM_Z6_BossSouthWall `d92446503f6e07b4163ddec7bae2947d1e3ca7f2b6d6e4d3985654229d5c5eab` / ASM_Z6_BossNorthShoulder `3860365a638385b73d076c43e07ddb9c21f21c43728aed5aa2737b3da11584fd`. 이음매 최소 거리72.54/66.77m다. 충돌/PIE 통과는 후속이며 원본/공유 에셋·V001~V008·지각/몸체/충돌/SurfaceData·마커/풀/밀도·기존 생성기를 보존했다.
- Unlit/FOV90°/1127×724로 기존16+보스 고원4의20시점을 대조했다. `Comparison.html`·카메라/메타데이터·`SelectedBossOverview.png`와 `FinalValidation_V001.json`을 정리했다. 복구51파일·채택14파일·이전 저작38파일 대조 PASS이며 에디터는 신규 두 소유자 선택·BossOverview다.
- V009 구도 채택/U4-10 남서·북서 외곽 진행 확인 대기다. 채택 뒤 새 판은 V009와17소유자를 고정한다. 전체 큰 배치 확정 후 몸체·충돌·전체 베이크/PIE·세부 마감 순서를 유지한다.

## 2026-11-25 — U4-9 사용자 채택·U4-10 V010 남서·북서 외곽

- 사용자가 V009를 채택하고 U4-10을 승인했다. 실제 V009·PIE=false·맵 dirty=false·채택17소유자 자세와23에셋 dirty없음·저작7파일 해시를 대조했다. 채택 메타데이터만 갱신하고14파일 복구본/해시를 남겼다. V010 manifest는 V009 맵/저작본과17소유자·누적 이전 저작45파일을 고정한다. Z1Bake_V010은 변경하지 않았다.
- 남서02c 사본 `(-16000,-13400,H-3500)`·yaw30°와 북서01c 사본 `(-15700,11500,H-1700)`·yaw−60°를 원래 크기로 더했다. 신규9,553인스턴스이며 내부 식생 층을 유지한다. 임의 외곽 연장 봉투와 실제 설계 경로를 구분하고 실제 남서 협곡 진입·북서 저지대 접근·서쪽 진입 눈높이 사선을 추가해46공간을 대조했다. 메사 접근로와 겹친 남서 후보2개를 기각했다.
- 저장·재로드 뒤 사용자 `Export_20261125_021731_356481.json`: PASS·issues/dirty없음·exported=true. 기존 Z1/17소유자/51파일 불변, Pod10/훅6/검토점27·LI/로드34개다. 신규 각46공간·15,286표본·큰 암석107/68개 겹침0·데칼0·NoCollision이며 실제 자세를 V010 Z7 JSON에 export했다. 워닝1건은 기존 북벽 AttachTo다.
- 지문: ASM_Z7_SouthwestEntryShoulder `1bb96c5fecd73677a1d84e69d94ac29c327468a498e92f3795fd1f08c06e531c` / ASM_Z7_NorthwestEntryShoulder `62808de20b3c2de05243b3e833546442a1b2d861aa42f99b742037fa25d6b234`. seam 최소17.12/22.11m로 시각5m 금지를 통과한다. 지각 native15m/전이55m를 보존했으며 후속 충돌20m 제한과 남서 시각 상자의 차이는 Z7 body_handoff로 넘겼다. 몸체/충돌/SurfaceData·마커/풀/밀도·원본/공유 에셋·V001~V009·기존 생성기는 보존했다.
- 기존20+외곽5의25시점을 Unlit/FOV90°/1127×724로 대조했다. `Comparison.html`·카메라/메타데이터·더 넓은 SelectionCamera의 `SelectedWestOuterOverview.png`·`FinalValidation_V001.json`을 정리했다. 복구51파일·채택14파일·이전 저작45파일·25시점 카메라/크기/FOV 대조 PASS다. 에디터는 신규2개 선택·서쪽 외곽 전체 선택 시점이다.
- V010 구도 채택/U4-11 남동·북동 외곽 진행 확인 대기다. 채택 뒤 새 판은 V010과19소유자를 고정한다. 전체 큰 배치 초벌 확정 후 몸체·충돌·전체 베이크/PIE·세부 마감 순서를 유지한다.

## 2026-11-25 — U4-10 V010 사용자 채택·U4-11 새 세션 인계

- 사용자가 V010을 채택하고 U4-11부터 새 세션으로 넘기며 이번 변경을 한 번에 커밋하도록 지시했다. U4-11/V011 맵·저작 폴더·배치는 만들지 않았다.
- V010 manifest/6구역 status·acceptance만 갱신하고19소유자 자세/지문을 유지했다. 채택 전8저작 파일과 채택 후21파일 복구본·AcceptedHandoffHashes/AcceptanceValidation을 남겼다. 이전 복구51파일·이전 저작45파일과 실제 export 자세는 불변이다.
- 종료 직전 LiveHandoff: U4Layout_V010·PIE=false·19자세 일치·관련25에셋 dirty없음. 사용자 선택 StaticMeshActor_2 한 개와 카메라를 유지했다. 저장된 신규2개 선택 화면/25시점 비교는 그대로 보존했다.
- Current·README·Roadmap·Phase U4/체크리스트·계획§17 채택 상태를 갱신하고 현재 인수인계를 U4-11 기준으로 다시 정리했다. 19개 보존 표·46공간·복구/비교 근거·도구 제약·새 세션 프롬프트를 포함한다. 다음 manifest는 V010/19소유자·누적 이전 저작53파일 기준이다.
- U4-4~10 저작42파일과 문서를 이번 단일 커밋에 포함한다. Content/Saved는 기존 ignore 정책에 따라 로컬 보존한다. 새 채택 저작본의 기존 CRLF 속성을 고정해 체크아웃 뒤 원시 해시를 유지한다. 전체 큰 배치·몸체·충돌·전체 PIE·세부 마감은 아직 미완료다.

## 2026-11-25 — U4-11 V011 남동·북동 외곽 초벌

- 시작 에디터 TestMap03·PIE=false·현재 맵 dirty=false, HEAD07bbe7e·Git clean. 로컬117개 해시 불변·V011 미존재를 확인한 뒤 V010을 열었고 사용자 읽기 전용 Verify_20261125_110916_391854.json은 PASS·전체 dirty없음·기존 Z1/19소유자/LI34개 불변이었다.
- 채택 V010을 새 V011 맵으로 복제하고 원본02c/01c의 전용 사본에 신규2개만 더했다. 남동 `(11000,-11800,H-3500)`·yaw−135°, 북동 `(10500,12500,H-1800)`·yaw135°, 모두 크기1이다. manifest 이전 채택본은 V010/19개/누적53저작으로 갱신했고 Z7 기존2개는 중복 추가하지 않았다.
- 후보12개에서 남동 seam4.58m 후보를 기각하고 최종21.30/23.70m를 골랐다. 기존46공간은 유지하고 진입부2·남동 하강로 전체 폭1·추가 시야선2를 더한51공간·16,552표본을 대조했다. 후속 충돌20m·지각 native15m/전이55m는 유지하며 새 시야선은 그래플 도달 가능성을 확정한 경로가 아니다.
- V011 맵을 지정 저장·재로드했고 로그에36개 레벨 로드를 확인했다. 실제 자세 export Cmd를 사용자에게 요청했으며 보고서는 아직 없다. 오프라인 원본 덤프 후보 조사는51공간·16,552표본 겹침0이지만 실제 지문·NoCollision·최종 export PASS를 대신하지 않는다.
- 원본/공유 에셋·기존 U4 V001~V010·Z1 V037/Z1Bake_V010·지각/프록시/SurfaceData·풀/밀도를 보존했다. 보존142파일 해시 불변, PreviousAccepted38파일 복구본, 동일 카메라30시점/FOV90°/1127×724 비교와 선택 화면을 U4_11_V011에 정리했다. 기존 생성물 재실행·R01~R06 삭제·몸체/충돌/지각/베이크/PIE/세부 마감은 하지 않았다.
- 열린 V011에서 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V011`을 실행한다. PASS를 확인한 뒤 V011 구도 채택과 Ridge 전체 큰 배치 검토 진행을 확인한다. 현재 U4-11 V011 초벌 배치·실제 검사/export 대기이며 U4 전체 완료로 표시하지 않는다. Content/Saved는 로컬에 보존한다.

## 2026-11-25 — V011 사용자 채택·외곽 종료와 내부 U5a-0 입력 조사(GX-036)

- 사용자가 V011을 채택하고 외곽 보라색 전이 띠의 추가 장식을 현 정도에서 종료했다. 삼각형 안쪽 보라색과 부유섬은 계속 장식하고 내부 위주로 다음 단계를 진행하도록 지시했다. GX-036에 기록했으며 기존 큰 배치/이음매/충돌 계약과 GX-034 순서를 유지한다.
- 실제 V011·PIE=false·맵 dirty=false·사용자 선택 PreservedRocks_Floor를 확인했다. 선택 Floor의 채널 재질 오버라이드는 이전 기준과 같았다. 사용자 카메라/선택을 유지한 내부6시점 촬영과5구조물 구분을 Review.html/InteriorScope/LiveStatePreservation에 남겼다.
- 사용자 `Export_20261125_120226_919321.json`: PASS·issues/dirty없음·exported=true. 기존 Z1/19소유자 불변, LI/로드36개·추가21개. 신규8,723+830인스턴스·데칼0·NoCollision·각51공간/16,552표본 겹침0, 실제 자세를 V011에 기록했다. FinalValidation·30시점 비교·보존142파일 해시 불변을 확인하고 채택 전8파일·채택 후11파일 복구본/해시와 AcceptanceValidation을 남겼다.
- U5a-0은 입력 조사까지 완료했다. BodyTrial.json에 채택 V011 맵/실제 export·21자세/참조/지문·저작8+이전53파일·51보호 공간과 기술 계약을 고정했다. Cull 이전 덤프를 당시 제외 결정으로 읽기 전용 대조해185,555개를 재구성했고 제보점 근처 메시 상자 후보를 조사했다. 원본 생성기 재실행·Cull 복원·에셋 변경은 하지 않았다.
- 첫 몸체 생성 시험은 R01/R05 열린 세로 외피와 R03/R04 같은 하부 공동 두 입구다. R02/R06도 회귀 감시하며 의도된 동굴/교량 아래/통로·착지/그래플 공간을 유지한다. 생성법·수치는 미정, U5aBody_V001 맵/몸체/충돌·지각·베이크·세부 식생/잔돌은 아직 없다. 다음 구현 단위는 제한된 생성 시험 결과를 보고하고 후속을 확인하는 것이다.

## 2026-11-25 — U5a-1 닫힌 몸체 생성 시험·경사로 REVIEW

- 사용자가 대표 열린 외피·하부 공동의 몸체 생성 시험을 승인했다. 실제 V011/PIE=false/맵 dirty=false·카메라/선택·입력203해시 불변을 확인하고 새 사본만 만들었다. 기존 생성물 재실행·Cull 복원·큰 배치 이동/삭제·외곽 확대는 하지 않았다.
- 새 Blender 스크립트로 원래 표면 경계 cap·5cm 안쪽 몸체, Cliff02의 파생 비다양체10cm voxel 복구, 지표 판을 포함한 구면25cm 천장/지면 연결을 시험했다. 초기 넓은V004 몸체는2,271m³·경사로44겹침/큰 벽 노출이 있어 V006을 가까운 천장과 제한 공동으로 줄였다. 현재 하부564m³·경사로32겹침, R03/R04 기록 위치는 같은 닫힌 몸체 안이다.
- 사용자 Cmd로 U5aBody_V001/생성V004, 이어 새 U5aBody_V002/생성V006을 반입했다. 현재4메시96,344삼각형·NoCollision·world identity, 경계/비다양체 변0이다. 실제 Verify_20261125_123804_405087.json preservation_pass=true·issues/dirty없음·기존185,555/데칼12/428키 지문·채택21개 불변·LI/로드36개·보존156파일 불변이다.
- 기존51공간16,552표본 부피 검사에서 StairRamp3 중앙32표본만 겹친다. 보호 폭을 줄이지 않았다. 나머지50공간의 표본 겹침0이며 실제 충돌/보행 통과 판정은 아니다. 원래 표면→몸체 각3,000표본 p95는 Cliff01 약5cm/Cliff02 약9.39cm다.
- 초기V004의 거의0 탄젠트/바이노멀2종 경고는 새 몸체 UV를4m 우세 축 투영으로 보정한 V006 반입에서 사라졌다. 남은 것은 기존 북벽 AttachTo1건이다. File not found ComparisonCameras.json은 에이전트가 복사 전에 촬영을 호출한 순서 오류였고 복사 후16시점 성공을 확인했다. 자동 승인 검토 촬영 시간 초과도1회 재시도에서 성공했다. 로그는 지우지 않았다.
- 같은 카메라16시점·Unlit/FOV90°/1127×724 비교와 경사로 평면/단면 도식·실제 자세·FinalValidation/해시/복구본을 U5a_1_V002에 남겼다. 현재맵 V002·하부 몸체 선택·UnderVoidOverview다. 상세는 analysis/Phase01_U5aBodyTrial.md다.
- 후보는 REVIEW·미채택이며 U5a 전체는 미완료다. 다음 단위는 몸체 경계/캡 외관 다듬기와 StairRamp3 별도 연결 검토다. 충돌/내부 지각·Support/Floor50㎡/전체Layer256·베이크·수동 접근 PIE·다른 구역 확대는 후속이다. 외곽 추가 장식 종료와 내부 보라 구조물/부유섬 마감 대상은 유지한다.


## 2026-11-25 — U5a-2 경계 보정 준비·연결 착지점 수동 확인 대기

- 사용자 승인 뒤 실제 V002/PIE=false/맵 dirty=false와 이전174파일 해시를 대조했다. V011과 이전 V001/V002는 보존하고 V011에서 새 U5aBody_V003을 복제·지정 저장했다.
- 이전 V006 이진 형상만 읽어 하부 경계1.25m를 지면으로 낮췄다. 새 생성V007은4메시96,344삼각형·경계/비다양체 변0, 하부 부피468.33m³다. 기존51공간16,552표본 중 StairRamp3만28겹침·나머지50공간0·R03/R04 기록점 내부를 유지했다. 새 전용 재질을 복제·저장했고 공유 부모/원본은 수정하지 않았다.
- 단순 서쪽/기존Ramp2 연결은 지형·채택 서쪽 어깨에 걸렸다. 최종 북쪽 착지3안 중(-7600,3000,H1006) 후보는 구면 최대37.05°·폭3m·2,079표본 근접0이지만 긴 고가 우회이며 발판 접속/자체 교차·전체 보호 공간/구도는 미검증이다. 추가 경로 자동 생성 대신 수동 착지점 확인을 요청하기로 했고 새 길은 배치하지 않았다.
- Generation_V003.json과 기존16카메라 파일을 먼저 준비하고 현재 열린 V003에서 사용자 Cmd import V003을 요청했다. 실제 반입·자세/지문 보존 검사·경고·동일 카메라 비교는 아직 대기다. 준비 검사를 실제 PASS로 표시하지 않는다. 충돌·지각/Support·베이크·PIE·세부 마감은 후속이다.


## 2026-11-25 — U5a-2 V003 실제 반입·보존/동일16카메라 완료

- 사용자 import V003 완료 뒤 실제 Verify_20261125_131822_798705.json 보존 PASS·issues/dirty없음·Z1 지문/채택21개/Pod10/훅6/검토점27/LI36 불변을 확인했다. 새4개96,344삼각형·identity·NoCollision·FBX 상자 오차 최대0.0033cm 미만이다. 기존174파일 불변이며 실제25자세를 MCP로 별도 export했다.
- 반입 구간의 새 메시 경고0·기존 북벽 AttachTo1건이다. 기존 카메라 파일을 먼저 준비한 뒤 같은16시점·Unlit/FOV90°/1127×724/자세0.001 이내 비교를 완료했다. Comparison/ManualReview.html·FinalValidation·FinalReviewHashes/새20파일 복구본을 남겼다.
- 하부 경계 보정·부피468.33m³·경사로28겹침·다른50공간0·R03/R04 내부는 유지한다. 넓은 캡 평면감/외피와의 재질 차이·R03 입구/아래면 직선 경계가 남아 외관도 REVIEW다. 자동 채택하지 않았다.
- 북쪽 착지부 전체/근접2시점을 촬영하고 V003·PIE=false·맵 dirty=false·선택없음·LandingClose를 남겼다. 사용자에게 현재 화면 중앙 후보를 사용할지, 원하는 착지면을 화면 중앙으로 옮겨 지정할지 요청했다. 새 연결 미배치·추가 경로 자동 생성 중단·충돌/지각/Support/베이크/PIE 후속이다.


## 2026-11-25 — U5a-3 별도 연결 초안·V004 실제 검사 완료

- 사용자가 구상대로 먼저 배치해 결과로 검토하도록 승인했다. 시작 V003·PIE=false·맵 dirty=false·이전200파일 불변과 사용자 시점을 기록하고 새 V004에만 연결1개를 추가했다. 기존4몸체·21채택 배치·Z1/원본/공유 에셋은 고정했다.
- 폭을 적용한 안쪽 코너 경사와 잔존 북쪽 디딤면 바위·합류면을 보정했다. 첫 V008 실제 화면의 남쪽 끝이 보이지 않는 보존 Floor 위여서, 보이는 지각 평탄부로 연장한 연결V011을 최종 초안으로 삼았다. 새4,000삼각형·약129.9m·중앙3m/시각5m·최대43.06°·NoCollision이다. 기존 생성기는 재실행하지 않았다.
- 새 통로12,726표본 근접/몸체 부피 겹침0·자기 교차 후보0·다른50공간 새 겹침0이다. 기존51입력은 유지하고 원래 StairRamp3의 몸체28+새연결1=29표본은 별도 REVIEW로 남겼다. 최소 seam83.64m·native15m/전이55m·시각5m/후속충돌20m 계약을 유지했다.
- 등록된 StaticMeshTools.import_file로 MCP 반입했다. bUseDefaultCollision=false까지 명시하고 실제 NoCollision5개를 재조회·지정 저장했다. 사용자 읽기 전용 Verify_20261125_165639_785717.json 전체 보존 PASS·Z1 185,555/데칼12/428키·채택21개/몸체4개 불변·Pod10/훅6/검토점27/LI36·전체 dirty없음이다.
- 실제26자세·같은22카메라/90°/1127×724 전후·선택2화면/도식·FinalValidation/FinalDraft16파일을 정리했다. 기존200파일 불변이다. 새 메시 경고0이며 도구 delegate schema21건/기존459,537삼각형 Z1 Shell의 에디터 Nav export2건은 별도 기록했다.
- V004·PIE=false·dirty없음·새 연결 선택/마지막 사용자 시점을 유지했다. 배치/검사는 완료했으나 시각 채택과 U5a 전체 완료는 아니다. 다음은 동선 유지 검토 후 석재/모서리/받침·몸체 캡/재질 다듬기다. 충돌 역할/지각·Support/베이크/PIE·세부 식생은 후속이다.

## 2026-11-25 — U5a-4 동선 유지·연결 석재 외형 V005 검토본

- 사용자가 V004 동선 유지/후속 진행을 승인했다. 시작 V004·PIE=false·맵 dirty=false와 이전267파일을 대조하고 새 V005 사본에서 연결 메시1개만 교체했다. 채택21소유자·기존4몸체·Z1/원본/공유 에셋과 V004 맵/메시/저작본을 보존했다.
- 동결 Connection_V011 이진 형상의 중앙3m858정점/상면을 복사하고 바깥 가장자리·측면·받침4곳을 조형했다. 저작 오차0.00110cm·최대43.066°·두께0.90~6.01m·새6,288삼각형·경계/비다양체 변0·자기 교차 후보0이다. 기존 생성기는 재실행하지 않았다.
- 등록된 MCP 반입기로 새 FBX만 반입하고 상면에 기존 전용 몸체MI·측면에 MI_Cliff_01을 참조했다. 두 재질/삼각형/범위 오차0.003663cm·실제 NoCollision/defaultfalse를 확인했다. 기존 재질은 수정하지 않았고 새 맵/메시만 지정 저장했다.
- 실제26자세 오차0·이전267파일 불변·동일26카메라/90°/1127×724 비교·전체/사선 선택2화면·검토19파일 복구본을 남겼다. 새 메시 반입 경고0, 기존 Shell Nav export1/북벽 AttachTo1건은 기록했다.
- 원래51보호 공간16,552표본은 유지하고 StairRamp3의 기존몸체28+새외형3=31표본을 REVIEW로 남겼다. 다른50공간 신규 겹침0·새 통로13,680표본 침범0·최소 거리 하한71.96cm·seam83.55m다. 충돌/실제 보행 통과로 표시하지 않았다.
- 전체 지문/미저장 패키지와 실제 반입858정점·두 재질을 한 번에 확인하는 읽기 전용 `RidgeStoneRefinement.py verify V005`를 사용자 Cmd로 요청했다. 현재 최종 실제 검사 대기·외형 미채택이며 몸체 캡/재질과 충돌/지각·Support/베이크/PIE·세부 식생은 후속이다.

## 2026-11-25 — U5a-4 V005 실제 보존/반입 보행면 검사 완료

- 사용자 실행 Verify_20261125_181219_222557.json·StoneVerify_20261125_181219_280794.json을 실제로 확인했다. 기존 Z1 185,555인스턴스/데칼12/428키/SHA·채택21개 자세/참조/지문·몸체4개 불변, 시험5개 identity/NoCollision/102,632삼각형·Pod10/훅6/검토점27/LI36 유지·issues없음·전체 dirty없음이다.
- 실제 반입858보행 정점 최대차이0.004367cm(허용0.05cm), 3,146정점/6,288삼각형·범위오차0.003663cm·두 재질 참조가 일치했다. 보존 PASS와 기존 StairRamp3 31표본 REVIEW·충돌/PIE 미통과·외형 미채택을 구별했다.
- 이전267파일과 직전 검토19파일/복구본을 대조한 뒤 이번 manifest/자세의 검사 상태만 갱신했다. 최종 검토22파일 복구본과 화면/카메라 등 근거44파일 해시를 남겼다. 이전19파일의 검사 전 복구본도 유지했다. 비교26시점과 실제 배치는 변경하지 않았다.
- Cmd 구간 기존 북벽 AttachTo1건이며 새 Python 오류는 없다. 이번 구현/실제 검사는 마쳤고 사용자에게 현재 석재 외형을 유지하며 몸체 캡/직선 경계/재질을 다음 구현 단위로 진행할지 확인한다. 충돌/지각·Support/전체 베이크/수동 PIE·세부 식생은 후속이다.

## 2026-11-25 — U5a-5 V005 연결 외형 유지·V006 몸체 보정 검토본

- 사용자 승인 뒤 실제 V005/PIE=false/맵 dirty=false·26자세와 이전333파일을 대조했다. 새 V006 사본에서 연결의 메시/두 재질/컴포넌트와 채택21개·Z1을 고정하고 새4몸체만 적용했다. 이전 모든 맵/저작/공유 에셋은 보존했다.
- 큰 Scan 캡2개 각289.19㎡를25cm 복셀 수리하고 최대36.53cm 굴곡을 줬다. 원래 외피 가까운35cm에는 추가 굴곡을 주지 않았지만 새 몸체 테두리 정점의 완전 동일성을 주장하지 않는다. 세 번째 작은 캡은 형상을 유지했다. 하부816정점 최대44.09cm 보정·R03/R04 반경2.25m 중심/바닥 유지다.
- 첫 세분 후보는 기존 교차를 확대해 미반입, 첫 V003 부분 반입은 극소1면 생략/탄젠트 경고로 중단했다. 그 맵/저작/메시2개를4파일 복구본으로 남겼다. 퇴화 변과 느슨한 변을 정리한 형상V005의4메시145,806삼각형은 실제 반입 수와 일치·범위 오차0.003357cm·NoCollision이다. 새 최종 FBX 경고0이다.
- 전용 Cliff MI·4m UV를 비교했다. 중립 텍스처의 VT sampler 불일치는 엔진 텍스처를 새 사본2개로 복제해 VirtualTextureStreaming=true로 맞췄고, 이후26시점 촬영 구간 경고0이다. 초기 경고28로그/실패 후보는 기록했으며 로그를 지우지 않았다.
- 기존51보호 공간16,552표본을 유지했다. 다른50공간 새 겹침0·기존 StairRamp3 몸체28+연결3=31표본 REVIEW, 실제 고정 통로13,221표본 새 몸체 침범0·최소 거리 하한252.32cm다. 자기 교차 후보1,291→339(12+16+311)·R01 경계점 민감성·캡 평면감/재질 접합은 별도 REVIEW로 남겼다. 충돌/PIE 통과로 확대하지 않았다.
- 실제26자세 오차0·연결 컴포넌트 불변·333파일 불변·동일26카메라 비교·몸체 선택2화면·검토33파일 복구본을 정리했다. 마지막 `RidgeBodyCapRefinement.py verify V006`를 사용자 Cmd로 요청했다. 전체 지문/실제 반입 정점/미저장 검사는 대기이며 외관도 미채택이다. 충돌/지각·Support/전체 베이크/PIE·식생/잔돌은 후속이다.

## 2026-11-25 — U5a-5 V006 실제 전체 보존/반입 정점 검사 완료

- 사용자 실행의 최신 Verify_20261125_193752_088779.json·CapVerify_20261125_193753_710370.json을 확인했다. 앞선18:51 실행도 PASS이며 최신19:37 결과를 최종 기준으로 삼았다. 기존 Z1 185,555인스턴스/데칼12/428키/SHA·채택21개 자세/참조/지문·유지한 V005 연결/두 재질 불변, Pod10/훅6/검토점27/LI36·시험5액터identity/NoCollision/152,094삼각형 유지·issues없음·전체 dirty없음이다.
- 실제4몸체의73,111정점은 각각 저작 정점 수와 같고 최대차이0.004367cm(허용0.05cm)·범위 최대0.003357cm·145,806삼각형·전용 재질 참조가 일치했다. 최신 Cmd 구간은 새 경고/오류0이며 배치·저장을 하지 않았다.
- 최종화 전 이전333파일·검토33파일/복구본 해시 불변을 확인한 뒤 이번 manifest/26자세의 검사 상태만 갱신했다. FinalValidation/FinalReviewHashes/FinalDraft의36파일 복구본과 이전 실행을 포함한 검토 근거44파일을 남겼다. 검사 전33파일 복구 사본도 유지한다.
- 보존/실제 반입 PASS와 자기 교차 후보339개·기존 StairRamp3 31표본·R01 경계점/캡/재질 접합 REVIEW·외관 미채택을 구별했다. 다음은 사용자에게 V006 외관 유지 여부와 남은 몸체 자기 교차 후보 검토/정리의 다음 단위 진행을 확인한다. 충돌/지각·Support/전체 베이크/수동 PIE·식생/잔돌은 후속이다.

## 2026-11-26 — V006 유지 확정·U5a-6 새 세션 인계·통합 커밋

- 사용자가 V006 현재 외관 유지를 확정하고 다음 단위는 새 세션으로 넘기며 문서/저작/도구를 한 번에 커밋하도록 지시했다. U4-11 이후 U5a-0~5 결과를 함께 정리했고 이 세션에서 U5a-6/V007을 시작하지 않았다.
- 채택 판정은 새 Acceptance_V006.json, 동결 원시는 FrozenInputs_V006.json의412파일로 구별했다. Generation_V006/FinalValidation/Verify의 accepted=false·previous_trial V005는 제작 이력이므로 덮어쓰지 않았다. 기존333+최종36+근거44의 중복을 제거했으며 모든 원시 해시가 일치했다.
- 채택 직전 읽기 전용 MCP 대조는 실제 V006·PIE=false·26자세 오차0·시험5컴포넌트 동일·관련9에셋 dirty없음이었다. 사용자 카메라와 선택 몸체4개는 유지했다. 전체 dirty없음은19:37 최종 Verify, 채택 직전 검사는 관련9에셋 기준으로 구별했다.
- AcceptedHandoff43파일 복구본·AcceptanceValidation을 추가하고 다음 보존 목록을412+43 중복 제거417파일로 고정했다. 다음 manifest 이전 채택본은 V006/Generation_V006·Acceptance_V006/26자세/몸체4+연결1이며 V005 제작 이력을 복사하거나 소유자를 중복 추가하지 않는다.
- 현재 인수인계 Phase01_U5aBodyHandoff.md와 새 세션 프롬프트·Current/README/Roadmap/Phase/계획/이력/과거 결과 문서의 최신 라우팅을 갱신했다. 다음은339후보12/16/311/0의 실제 관통/공면/접촉·퇴화 면 분류 후 새 V007의 최소 보정이다. 시각 유지와 교차/경사로 REVIEW·충돌/PIE 통과를 구별했다.
- 커밋 범위는 U4-11 채택 저작8파일, U5a 입력/판별 manifest·도구·현재 채택 sidecar와 문서다. 기존 커밋 형식의 한국어 feat 제목/항목별 본문을 사용한다. 실제 CRLF/LF에 맞춰 .gitattributes를 고정해 재체크아웃으로 보존 해시가 바뀌지 않게 한다. Content/Saved의 작업 맵·형상·원본·검사/비교/복구본은 기존 ignore 정책대로 로컬에 남긴다.

## 2026-11-26 — U5a-6 자기 교차 분류·오프라인 최소 보정안, 라이브 적용 대기

- HEAD9bc573a/추적 변경 없음에서 시작했다. 활성 TestMap03·PIE=false이며 맵을 바꾸지 않았다. FrozenInputs412+AcceptedHandoff43의 중복 제거417파일과 복구43파일 SHA를 대조했다. 저장 V006 참조의 실제26자세 오차0·몸체4컴포넌트/145,806삼각형·연결 전체 컴포넌트/두 재질·관련9에셋 dirty없음·VT 재질 계약 일치다.
- 기존12/16/311/0 후보는 float64 단면/무게중심과 독립 선분/삼각형 판정에서 모두 관통이었다. 공유 정점 제외126쌍(4/1/121/0)도 공유점 밖을 관통한다. 총465는 교차 면 쌍 수이며 독립적인 결함 위치 수로 단정하지 않았다. 면 번호별465쌍 투영 검토 화면·대표3쌍의 전후6진단 이미지를 만들었다.
- 원본 사본에서 현재 교차면+인접1고리만 이완하고 해결 영역/축별 극값을 고정한 국소 보정안을 만들었다. 이동76/68/2,010/0정점·최대5.834/6.445/7.258/0cm, 위상/면 연결·정점/면 수·경계 상자 유지, 단정도/중앙 원점 BVH 정밀 검사 잔여 교차0이다. 전체 복셀화·캡 재구성은 하지 않았다.
- 기존51공간16,552표본과 겹침 목록 동일·다른50공간 새 겹침0·원래 StairRamp3의31표본 REVIEW 유지다. 고정 중앙3m/858정점에서13,221표본 침범0·최소 거리 하한252.322128cm를 대조했다. R01~R06 점/최근접 거리·내외 판정과 R01 방향 민감성을 원본과 대조했다. R03/R04는 변경 없는 하부 몸체 안이다.
- 전체 dirty 확인용 첫 수동 Cmd는 입력 로그만 남고 결과가 없었다. Python 모드의 읽기 전용 InspectRidgeU5a6.py 실행을 요청했으며 결과는 아직 대기다. 새 V007 맵·Generation_V007·반입·실제26카메라 비교/정점 검사·새 패키지 저장은 하지 않았다. 검토 manifest의 이전 채택본은 V006/26자세/몸체4+연결1로 고정했다. V006/Generation 이력/417파일·원본/공유 에셋은 보존한다.
- Review_U5a6_V001.json·새 검토 문서와 Saved/U5a_6_V001의 근거/국소 보정안25파일 복구본을 정리했다. 오프라인 결과를 실제 V007 적용/시각 채택·충돌/PIE 통과로 확대하지 않는다. 다음은 전체 dirty 확인 뒤 새 V007의 최소 반입과 실제 보존/동일26카메라/복구 마감이다.

## 2026-11-26 — U5a-6 V007 최소 보정·실제 전체 보존/정점·교차0·26비교 마감

- 사용자 실행 초기 상태 파일에서 TestMap03·PIE=false·전체 dirty없음을 확인한 뒤 V006 활성 상태/26자세를 재대조했다. V006을 새 U5aBody_V007으로 복제해 지정 저장하고 수정 전 사본을 보존했다. 기존 BodyCaps.blend에서 세 몸체의76/68/2,010정점만 수정해 최대7.258cm 이동·UV 루프 해시/면 연결/재질 슬롯을 유지했다. 새3메시는 IntersectionRepair_V001에 반입했고 하부/연결은 기존 메시를 참조한다. 소유자를 중복 추가하지 않았다.
- 새 맵의 실제26자세 오차0·네 몸체 컴포넌트/연결 전체 컴포넌트·두 재질/NoCollision을 대조하고 새3메시/맵만 저장했다. 이전417파일은 불변이다. 전후 모두 새로 촬영한 동일26카메라/FOV90°/1127×724의 위치/회전 오차0과52화면·4모음을 확인했다. 외관 채택은 사용자 검토 대기다.
- 사용자 실행 Verify_20261126_184701_900533.json/NativeVerify_20261126_184703_322994.json은 전체 보존/73,111정점 PASS·최대차이0.004367cm·145,806삼각형/연결6,288·858보행 정점/두 재질 유지·전체 dirty없음이다. 기존 Z1의185,555인스턴스/데칼12/428키/SHA853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88·채택21소유자/Pod10/훅6/검토점27/LI36을 유지한다.
- 실제 UE4몸체 덤프를 다시 검사해 교차 면 쌍0·경계/비다양체 변0·51공간16,552표본/다른50공간 새 겹침0·고정 통로13,221표본 침범0·거리 하한252.320969cm를 확인했다. StairRamp3의31표본 REVIEW와 R01 방향 민감성·R03/R04 중심 및 통로/Pod/동굴/석교/부유섬/그래플 사선은 유지한다.
- 새 FBX/재질 경고0이다. MCP 입력 크기 제한 경고1건은 필드 축소로 해결했다. 실제 검사 직후 RHI 예약 가상 리소스258GB>256GB 경고1건은 별도 REVIEW로 남겼다. Blender brush 상대 경로/썸네일 캐시 경고는 실제 산출 파일 성공/검증과 구별했다.
- Generation_V007 이전 채택본은 V006/Generation·Acceptance/26자세/몸체4+연결1이다. V006 채택 원본과 제작 이력/417파일·U4/Z1/이전 U5a·공유 에셋은 보존했다. FinalValidation_V007·최종 새 맵/메시/도구/실제 근거/화면 복구본을 정리하고 Current/Phase/인계/검토 문서를 갱신했다. 충돌/지각·Support/전체 베이크/PIE·다른 구역 확대·식생/잔돌은 시작하지 않았으며 다음 단위는 외관 결정 뒤 확인한다.

## 2026-11-26 — V007 사용자 채택·U5a-7 R01 국소 보강 V008 검토

- 사용자의 V007 유지/다음 진행 지시를 Acceptance_V007/FrozenInputs_V007에 기록했다. Generation_V007 제작 이력은 수정하지 않았다. Frozen529+AcceptedHandoff29의 중복 제거534파일을 새 보존 목록으로 대조했다.
- 사용자 확인 빈틈과 카메라 시선을 기록하고 정확한 Cliff09 안쪽에만162정점/320삼각형/3.617643m³ 보강1개를 추가했다. 기존4몸체/연결/ED·26자세·두 재질/858정점을 유지했다. 돌출 중간안은 제외/보존하고 최종 형상V006을 새 맵V008에 적용했다.
- 실제 전체 보존/73,273정점 PASS·held4형상 해시 동일·자기 교차0/닫힌 위상·51공간16,552표본/다른50공간 새 겹침0·고정 통로13,221표본 침범0·R01 기존81광선의 미검출9→0이다. 기존길31표본은 중앙선도 겹쳐 REVIEW이며 전체R01 해결/보행 통과로 확대하지 않는다.
- 실제27자세/동일27카메라54화면·4모음·복구를 정리했다. 검사 구간 Error0, 기존 북벽 AttachTo1건과 PASS 뒤 Epic 통계 업로드 libcurl65를 구별했다. 기존 RHI/Lumen 예산 경고는 별도 REVIEW다. V008 외관 확인 대기이며 충돌/지각·Support/베이크/PIE·다른 구역/세부 장식은 미착수다. [경계 검토](../analysis/Phase01_U5aBoundaryReview.md)를 따른다.

## 2026-11-27 — V008 외관 채택·GX-037 부유석 표현 적극 활용

- 사용자가 재시작한 에디터에서 V008 새 보강 몸체를 확인했다. 약간 어색함은 판타지 부유섬/부유석이 있는 LootNPop 지형의 허용 범위로 수용했고, 앞으로 전체 맵에서 공중에 뜬 암석을 적극 활용하도록 지시했다.
- GX-037과 공통 옥탄트 설계 가이드/1-BR §2.1에 반영했다. 의도된 부유 덩어리에 GX-033 A의 받침·GX-034의 기반면 연결을 강제하지 않는다. 과거 Cull/채택 배치의 생성 규칙과 산출은 재실행/복원하지 않는다. 실질적 막힘·열린 내부/뒷면과 충돌/Support 검사는 유지한다.
- V008 맵/최종 보강 메시 dirty=false·PIE=false와 이전534파일/최종168복구본의 해시를 대조했다. Acceptance_V008/FrozenInputs_V008·채택25파일 복구본으로 기록하고 제작 당시 Generation_V008/FinalValidation accepted=false는 보존했다. 동결696+채택25의 다음 중복 제거 보존 기준은701파일, 이전 채택 입력은 V008/27자세/몸체5+연결1이다.
- 기존31표본/전체R01 접근·RHI/Lumen REVIEW와 시각 채택을 구별한다. 이번 사용자 결정으로 형상·충돌을 추가 변경하지 않았고 다음 구현 단위도 시작하지 않았다.

## 2026-11-27 — U5a-6/7 마감·다음 작업 새 세션 인계·일괄 커밋

- 사용자가 다음 작업을 새 세션에 넘기고 문서 갱신/인수인계와 이번 변경 전체의 단일 커밋을 요청했다. 이전 커밋의 feat: 한국어 제목·항목별 본문 형식을 따른다.
- 현재 인수인계를 V006 당시 프롬프트에서 V008 채택/실제27자세/몸체5+연결1·다음701파일 기준으로 다시 작성했다. README/Current/Roadmap/Phase와 U5a-6/7 결과의 오래된 대기·미착수 라우팅을 갱신했다. 다음 첫 작업은 실제 상태/미저장·로컬 보존 대조 후 남은 R01/기존31표본 REVIEW의 한 단위 범위 제안이며 신규 형상/충돌 단계는 미착수다.
- 마지막 라이브 재확인은 활성V008·PIE=false·보강 선택·관련2패키지 dirty=false이며 사용자 카메라는 그대로 뒀다. 11월26일 실제 전체 Verify의 dirty=[]와 구별했다. 지형 생성/반입·물리/베이크/PIE는 반복하지 않았다.
- 문서·도구·저작 JSON·Acceptance/FrozenInputs를 커밋하고 Content/Saved는 기존 ignore 정책대로 로컬 보존한다. 원시 해시/복구·LF/CRLF 체크아웃 재현·새 Python/JSON 구문·diff 공백을 검증한다.
