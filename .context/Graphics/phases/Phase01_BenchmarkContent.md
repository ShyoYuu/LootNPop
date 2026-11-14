# Phase 1 — 벤치마크 콘텐츠

> 상태: 진행 중 · 1-A 경로·패키지 준비 완료, 패키지 실행 검증부터 새 세션으로 인계
> 마지막 갱신: 2026-11-14
> 범위: 고밀도 옥탄트 1개·파라곤 적 2~3종(GX-004)

## 1. 목적과 비교 조건

현재 월드에 실제 렌더 부하를 대표할 콘텐츠를 넣고, 같은 조건의 투입 전후 비용을 비교한다. Phase 0의 Meadow 포함 패키지(`29305b7`)는 완료된 과거 기록으로 보존한다. 착수 시 HEAD는 `f0d0a8c`였고, 실제 출발 패키지는 `c6981a6` + 이번 Ridge 경로 변경으로 빌드했다. 최종 소스/패키지 식별과 새 세션 절차는 §9를 따른다.

측정·냉각·보고 형식은 [측정 규약](../design/Measurement.md)을 따른다. 출력 2560×1440, TSR·렌더 해상도 100%·에픽 품질, 시드 1·PCG 오프셋 0, 워밍업 180초·캡처 60초를 유지한다. 기본 `DA_MassSpawnConfig`와 실제 옥탄트 풀은 이 준비 작업에서 바꾸지 않는다.

## 2. 착수 확인과 남은 선택

사용자의 2026-11-11 작업 진행 요청으로 실행서와 출발 기준선 준비에 착수했다. 콘텐츠 선택과 빌드·측정 실행은 아래 확인 뒤 진행한다.

| 항목 | 현재 상태·확인할 내용 |
|:---|:---|
| 꾸밀 옥탄트 | **기존 Ridge로 확정(2026-11-13, 사용자, GX-013).** 새 옥탄트 제작은 채택하지 않음. 근거는 [후보 비교](../analysis/Phase01_ContentCandidates.md) §5.3 |
| 파라곤 적 | 2종부터 시작하는 안. 실제 캐릭터·기존 적 역할 대응은 미확정 |
| 환경 원본 | **Electric Dreams로 확정.** 설치/실물 확인: `D:/UnrealProjects/ElectricDreamsEnv`, UE 5.8.4, 8001. [검토 결과](../analysis/ElectricDreamsReview_20261112.md)는 조건부 적합. 1-B0의 6종 조건부 팔레트 선정 완료, 반입 시험은 미실행 |
| 라이선스 | Fab 상품 표기 Standard License·0원 확인(2026-11-13). 수정·상업적 이용은 허용, 원본 재배포는 아니므로 git 제외 유지. "AI 사용 불허" 표시라 생성 AI 입력으로 쓰지 않는다. 세부 원본은 Fab EULA |
| 빌드·쿡 | 최종 정규 Editor 빌드·Viewpoints 1/1·Game Development 패키지 생성/스테이징 완료(2026-11-14). 패키지 실행은 미검증. 다음 빌드/쿡 때도 실행 직전 병행 여부를 확인한다 |
| 수동 확인 | 카메라 무관통·콘텐츠 구도, 적 표현·전환은 사용자 PIE/패키지 육안 확인을 받는다 |

**카메라 선결 조건:** 시드 1의 실제 Ridge slot 1·7을 2026-11-13 PIE에서 확인했다. A/B/C는 이후 시드 2982의 Ridge slot 1에서 사용자 구도로 수집했고 출처 시드를 보존했다. 2026-11-14에 하네스를 Ridge·slot 1로 바꾸고 Live Coding·Viewpoints 검사를 통과했다. 시드 1/Medium의 연결 경로 검증은 아직 남아 있다(§8). 경로가 바뀌면 콘텐츠 반입 전후 모두 같은 최종 경로를 사용한다. 구도는 사용자가 직접 잡는 절차를 우선한다.

## 3. 실행 단위와 검증

| 단위 | 작업 | 완료 근거 |
|:---|:---|:---|
| 1-A 출발 기준선 | 적 0 진단·현행 cook 목록 준비 → 대상/경로 확정 → 별도 패키지 → 짧은 검증 → Medium 3회·적 0 1회 | 종료 코드 0·manifest/CSV 검증·경로 육안 확인·3회 재현성 표 |
| 1-B 환경 | Ridge 대표 구역·재질 방침·조건부 팔레트 선정([1-B0 실행 문서](Phase01_RidgeZonePalette.md)) → 제외 경로 확정·배치 방식 결정 → 작은 반입 시험·팔레트 최종 확정 → 대표 구역 PCG 장식·Nanite 적용 → Ridge 전체로 확장. 기존 지각 재질·부분 메시 보강을 우선 검토하며 1차 보행 충돌은 유지. **1-B0 완료: 대표 구역·톤·재질 방침과 6종 조건부 팔레트 선정. 반입 전 1-A 경로/출발 기준선 우선** | 구면 접지·적도 부근·이음매·전투/훅 동선 보존, Decoration 계약·기존 SurfaceData 현재성/런타임 로드 확인, 원본/재생성 절차 |
| 1-C 적 | 파라곤 2~3종·기존 애니메이션 리타깃·별도 스킨드 무기 | ISKM·ISM·Actor 정상 표시, 제자리 클립·全 LOD 인스턴싱·usage 플래그, O-3 원본 설정 표 |
| 1-D 전후 비교 | 새 콘텐츠 패키지에서 같은 측정 반복 | 출발점 대비 Frame/GT/RT/GPU·드로우콜·예산 비율, 30초~1분 영상·요약 |

각 단위를 끝내면 결과와 다음 범위를 확인한다. 콘텐츠 반입 전에 1-A를 마치며 Low/High 정식 반복은 생략한다.

## 4. 1-A — 적 0 대조군과 패키지

### 구현 범위

- 인자 `-LNPGraphicsNoEnemies`는 그래픽스 하네스가 활성일 때만 적용한다.
- Medium 편성·적 밀도 1로 공간 배치 계획을 그대로 완성한 뒤, 큐 조립에서 적 생성 요청만 생략한다. Pod 배치에 영향을 주는 적 배치 제약·난수 소비를 유지한다. 밀도 0으로 공간 계획을 다시 만들지 않는다.
- `Run.json`의 `no_enemies=true`, `performance_eligible=false`로 남긴다. 초기 적과 조회된 생존 적은 모두 0이어야 한다. `initial_spawns`는 생성 단계 계수라 적 행이 없고, 공간 계획의 적 요청/배치 로그는 Medium 계획 수를 유지한다.
- 적 0 실행의 `Population.csv`는 헤더만 가진다. 집계기는 진단 표기·초기/마지막/프레임 적 수 0을 확인한 경우에만 빈 개체 표를 허용한다. 일반 실행의 빈 개체 표는 계속 실패시킨다.
- `SpawnProfiles.json`의 명시적 cook 목록은 실제 풀에 맞춰 Meadow를 빼고 Grove·Ridge의 레벨/Support 에셋을 넣는다. 편성 값은 유지한다.

### 실행 순서

1. 대상·최종 경로·병행 작업을 확인하고 빌드 가능한 소스 상태와 변경 식별값을 남긴다. 에디터 실행 중이면 Live Coding과 전체 빌드를 겹치지 않는다.
2. 정규 `LootNPopEditor Win64 Development` 빌드와 별도 Game Development 패키지를 만든다. UBT·쿡은 처음부터 권한 확장으로 실행한다. 기존 패키지는 보존한다.
3. 짧은 Medium과 적 0 실행에서 정상 종료·1440p/TSR 실효값·초기 수·CSV·Pod 계획 일치를 확인한다. 카메라 A/B/C의 무관통과 콘텐츠 대상 구도는 사용자에게 확인받는다.
4. 회차별 냉각 규약을 적용해 Medium 3회를 각각 실행한다. 적 0은 같은 Medium 편성과 인자만 달리해 1회 실행하고 `-AllowPreview`로 별도 집계한다. 단일 진단을 3회 기준선으로 표시하지 않는다.
5. 실행 파일·컨테이너·경로 해시와 소스 변경 식별값, 전원·GPU 환경을 기록한다. 전후 패키지가 달라지는 이유는 콘텐츠 투입이며, 렌더 설정·경로·편성 변경을 함께 섞지 않는다.

출발 아카이브는 `Saved/Graphics/Phase01StartPackage_20261114`로 생성했다. 짧은 검증 결과는 §9의 PackageValidation 경로에, 정식 기준선은 실제 실행일의 별도 StartBaseline 폴더에 둔다. 런처는 기존 `RunGraphicsBenchmark.ps1`를 사용하고 적 0에만 `-ExtraArguments @('-LNPGraphicsNoEnemies')`를 추가한다. **경로 선결 조건 해결 전에는 기존 Acropolis 경로로 정식 반복을 시작하지 않는다.**

## 5. 콘텐츠 반입 규약

- 환경의 Landscape·평면 월드 시스템을 가져오지 않는다. 기존 Ridge 형상을 유지하고 구면 법선에 맞는 PCG 장식으로 제한한다. Ridge는 SSN Phase 15 경로 재측정 대상이므로 보행면을 바꾸는 배치는 별도 합의 뒤 재베이크와 함께 한다. 충돌·베이크 소유권은 [옥탄트 제작 절차](../../Guide_OctantLevelInstance.md)를 따른다.
- 에픽 원본·파생 라이선스 에셋을 추적하기 전에 정확한 폴더별 git 제외를 설정한다. 자동 재생성 코드·설정·원본 경로/버전·수동 단계만 추적한다(GX-004).
- 적은 [Low LOD 설계](../../TechDesign_EnemyNPC_LowLOD.md) §6.7과 AnimationOverhaul 리타깃 규약을 따른다. 원본 머티리얼 슬롯·LOD·삼각형 수를 측정 전에 최적화하지 않는다.
- O-3는 반입 시 카드/DF 생성 설정·컴포넌트 Lumen 기여·하드웨어 RT 실효값을 기록한다. 절제 실험은 Phase 2-3이 소유한다.

## 6. 완료 체크리스트

- [x] 1-B0 대표 구역·톤·재질 방침·6종 조건부 팔레트·의존성·후속 시험 항목 선정(반입/배치 완료는 아님)
- [ ] 대상 옥탄트·파라곤 종류/수·원본 위치/버전·라이선스·git 제외 확정
- [ ] 적 0 진단과 집계·일반 실행 회귀 검증
- [x] 최종 Ridge slot 1 경로(B +50cm)·하네스·스테이징·검사 반영, 정규 Editor 빌드·Viewpoints 1/1·Game 패키징/스테이징 해시 검증 통과(패키지 실행 미검증)
- [x] 대상이 보이는 고정 경로·구도 사용자 확인(에디터 기존/가로형 화면비, 패키지 정확 1440p 확인은 별도)
- [ ] 출발 패키지 Medium 3회 + 적 0 1회, 재현성·환경·해시 기록
- [ ] 고밀도 옥탄트 1개·PCG/Nanite·구면/동선 검증
- [ ] 파라곤 2~3종의 ISKM·ISM·Actor·리타깃 검증과 O-3 설정 기록
- [ ] 깨끗한 소스 트리의 정규 Editor 빌드와 콘텐츠 패키지 검증
- [ ] 같은 조건의 콘텐츠 투입 전후 측정·예산 비교 보고서
- [ ] 재생성 절차·30초~1분 영상·문제→분석→해결→측정 요약
- [ ] 사용자 마감 확인과 Current/Roadmap 갱신

진행 이력과 실제 검증 근거는 [Phase 1 로그](../history/Phase01_Log.md)에 쓴다.

## 7. 1-A 새 세션 인수인계(2026-11-13)

> 아래는 2026-11-13 착수 전 인계 기록이다. 현재 상태와 바로 다음 검증은 §8·§9를 우선한다.

사용자 요청으로 **1-A 실작업은 다음 세션에 시작한다.** 이번 커밋은 적 0 진단 준비와 1-B0 선정 결과를 묶는다. 1-B0을 다시 선정하거나 환경 에셋부터 반입하지 않는다.

### 7.1 이어받을 상태

| 항목 | 완료/미완료 구분 |
|:---|:---|
| 환경 선정 | 1-B0 완료. 대분지부터 Ridge 전체로 확장, 사암 주조·바닥 일부 이끼/식생, 원본 지형 재질 파생본+메시 원본 MI, 6종 조건부 팔레트. 상세는 [선정 문서](Phase01_RidgeZonePalette.md) §8 |
| 적 0 진단 | C++·manifest·집계기·현행 cook 목록 준비. 2026-11-11 Live Coding 성공. 2026-11-13 커밋 전 합성/거부·기존 Medium 재집계·cook root 존재 검사 8개 재통과 |
| 아직 없는 근거 | 정규 Editor 빌드·새 패키지·실제 적 0 실행·고정 Ridge 경로·출발 기준선. Live Coding/합성 검사를 이 근거로 대체하지 않음 |
| 실제 배정 | 일반 PIE에서 Ridge slot 5와 네 검토점 사용자 확인은 완료. 벤치마크 시드 1의 Ridge slot 1·7은 코드 재현 예측일 뿐 실제 미검증 |
| 접속 | LootNPop=8000, Electric Dreams 원본=8001. 새 세션에서 연결 프로젝트/PIE를 재확인. 1-A는 LootNPop에서 진행하며 Electric Dreams 원본은 편집/저장하지 않음 |
| 원자료 | `Saved/Profiling/Graphics/Phase01/Preparation_20261111/`, `RidgeZonePalette_20261113/`. Saved는 git 제외이므로 다른 체크아웃에는 없을 수 있음. 선택 결과는 추적 문서 §8이 소유 |

### 7.2 경로 변경 때 함께 확인할 곳

현재 실행 경로는 여전히 **Acropolis·slot 6**에 고정돼 있다. JSON만 교체하거나 경로 인자만 주어서는 Ridge로 전환되지 않는다.

| 위치 | 현재 제약/맞출 내용 |
|:---|:---|
| `Source/LootNPop/Graphics/LNPGraphicsBenchmark.cpp` | ReadViewpoints의 asset/slot 검증, Prepare의 실제 배정 검사/기본 파일, UpdateView의 slot 회전 |
| `Scripts/Profiling/Graphics/AcropolisViewpoints.json` | 대상·seed/PCG offset·slot·옥탄트 로컬 pose·구면 roll·FOV/aspect 규약. Ridge의 사용자 확정 구도에 맞춤 |
| `Source/LootNPop/LootNPop.Build.cs` | 경로 JSON의 NonUFS RuntimeDependency |
| `Source/LootNPop/Tests/LNPGraphicsBenchmarkTest.cpp` | `LootNPop.Graphics.Benchmark.Viewpoints`의 파일·slot·수동 캡처 기준 pose 기대값. 기존 잘못된 좌표/쿼터니언 거부 검사도 유지 |
| `Scripts/Profiling/Graphics/PackageGraphicsBenchmark.ps1` | 스테이징한 경로 JSON의 존재 확인 |
| `Scripts/Profiling/Graphics/RunGraphicsBenchmark.ps1` | 같은 스테이징 경로 JSON 조회와 Launch.json 해시 기록 |

파일명/경로를 정하면 C++·NonUFS·검사·패키저·런처가 **같은 최종 JSON**을 참조하게 한다. 변경은 Ridge 벤치마크에 필요한 범위로 제한한다.

### 7.3 실행 순서와 검증

1. **실제 시드/slot → 사용자 구도 확인.** 시드 1·PCG offset 0의 실제 Ridge 배정을 로그로 확인한다. 사용자가 A=DeepBasin(대절벽/바닥), B=TablelandRim(절벽 측면), C=ValleyRise(허브 전경)를 먼저 잡는 안이다. CrownEastRim은 높은 하향 비교 예비다. 이 이름들은 확정 카메라 pose가 아니다. A→B→C→A 이동 중 지형 관통과 환경/적 노출까지 확인한다.
2. **최종 JSON/하네스 정합 → 경로 검증.** 구면 roll을 보존한 로컬 카메라와 FOV/aspect를 기록하고 §7.2를 함께 맞춘다. 기존 Viewpoints 검사도 최종 캡처 근거에 맞춰 검증한다. 반입 전후는 같은 경로를 사용한다.
3. **새 바이너리 → 짧은 실행 검증.** 빌드/Live Coding/쿡 직전에 사용자 병행 작업을 확인한다. 전체 빌드/쿡은 AGENTS 규칙대로 처음부터 권한 확장으로 실행하고 Live Coding과 겹치지 않는다. 정규 빌드·새 별도 패키지 뒤 짧은 Medium/적 0에서 정상 종료·1440p/TSR·CSV·manifest·Pod 계획 일치를 확인한다.
4. **출발 기준선 → 1-A 마감.** §4와 Measurement의 냉각 규약을 따라 Medium 3회+적 0 진단 1회만 수행한다. 적 0은 `-LNPGraphicsNoEnemies`, 집계 `-AllowPreview`, `performance_eligible=false`로 분리한다. 실행 파일/컨테이너/경로 해시·전원/GPU 환경·재현성을 기록한다. 기존 Meadow 패키지와 섞지 않는다.
5. **그 뒤 환경 반입으로 이동.** 1-A 완료 확인 후 git 제외/반입·배치 방식을 정하고 작은 반입 시험으로 조건부 팔레트를 최종 확정한다. 기존 보행 충돌·Support source는 보존한다. 파라곤 종류/수는 아직 미확정이다.

### 7.4 새 세션 시작 프롬프트

```text
.context/Graphics/README.md, Current.md, Decisions.md를 읽고,
.context/Graphics/phases/Phase01_BenchmarkContent.md §7과
.context/Graphics/phases/Phase01_RidgeZonePalette.md §8을 읽어줘.

그래픽스 Phase 1의 1-A(Ridge 경로 확정·콘텐츠 반입 전 출발 기준선)를 이어서 진행해줘.
1-B0은 완료됐으니 재선정하지 말고, 먼저 시드 1·PCG offset 0의 실제 Ridge slot과
사용자 카메라 A/B/C 및 연결 구간을 확인해줘. 일반 PIE slot 5와 시드 1의 slot 1·7 예측을
확정 배정으로 재사용하지 마. 수동 PIE/GUI가 빠르면 자동화 장치 전에 요청해줘.

현재 하네스는 Acropolis·slot 6에 고정돼 있어. §7.2의 C++·JSON·NonUFS·검사·패키저·런처를
필요한 범위에서 함께 맞추고, 정식 반복 전에 최종 경로와 짧은 실행을 검증해줘.
빌드/Live Coding/쿡은 직전에 병행 작업을 확인하고 AGENTS의 권한 확장 규칙을 따라줘.
측정은 1440p·TSR100%·에픽·시드 1·PCG offset 0·워밍업 180초·캡처 60초,
Medium 3회+별도 적 0 진단 1회이며 냉각 규약을 지켜줘.

1-A를 마치기 전에는 환경 반입·PCG 배치·재질 변경을 시작하지 마.
단위 종료 시 완료/미확인 근거와 다음 행동을 실행서·Current·로그에 남겨줘.
```

## 8. 1-A 구현·검증 결과(2026-11-14)

- `RidgeViewpoints.json`의 목표는 시드 1·PCG offset 0·Ridge slot 1이다. 시드 1의 실제 배정 `[Acropolis, Ridge, Bastion, Pyramid, Grove, Hollow, Pyramid, Ridge]`를 확인했다. 세 구도의 수동 캡처 출처는 시드 2982·slot 1이며 메타데이터에 보존했다.
- A=DeepBasin·B=TablelandRim·C=ValleyRise를 사용한다. B-C 절벽 모서리 관통을 사용자/PIE 복잡 충돌 중심선(첫 충돌 351.586cm)으로 확인한 뒤 B를 `B - normalize(B) * 50cm`로 보정했다. 구면 내부의 현지 Up이며 회전·렌즈·A/C·원본 플레이어 pose는 유지했다. 수정된 정/역방향 중심선 6개와 기존/가로형 화면의 사용자 육안 확인을 통과했다.
- 하네스의 대상/실제 배정 검사·slot 회전, NonUFS, 패키저·런처, Viewpoints 검사가 같은 Ridge JSON을 쓴다. 기존 위치 직선 보간·회전 Slerp와 A→B→C→A 순서, 렌더 설정·기본 편성/풀은 유지했다. Acropolis JSON은 과거 기록으로 보존했다.
- 카메라는 FOV≈80°·reference aspect≈1.3331548·MaintainYFOV·축 제약 override=true다. 1280×720 설정의 최종 에디터 회차는 실측 1280×722, complete·3174프레임·적 2002/Pod 120·AllowPreview CSV 집계 통과다. 정확 1440p 출력/성능 검증은 아니다.
- Live Coding 뒤 첫 PIE는 `ULNPAnimInstance::UpdateStates`의 디버그 CVar 읽기에서 크래시했다. 애니메이션 코드 변경 없이 풀빌드/재시작 후 세 회차가 정상 종료했다. 원인을 Live Coding으로 확정하지 않는다. 다음 재빌드는 패치와 겹치지 않게 한다.
- 최종 정규 `LootNPopEditor Win64 Development` 빌드, B +50cm 기대값을 포함한 Viewpoints **1/1(테스트 오류/경고 0)**, Game Development build/cook/stage/archive는 모두 종료 코드 0이다. 자동화는 NullRHI·NoLiveCoding의 정규 DLL로 실행했다.
- Cook 요약은 오류 0·경고 1이다. 프로젝트 설정의 `r.Shadow.Virtual.MaxPhysicalPages=4096` 우선순위 때문에 Scalability 덮어쓰기를 무시한 경고이며 설정은 바꾸지 않았다. Foley notify 누락 의존성 Display 메시지도 로그에 남겼고 수정하지 않았다.
- 새 패키지는 49파일·약 1.36GiB이며 실행 파일·컨테이너 5개·NonUFS 경로 해시를 기록했다. 소스/기본 SpawnConfig·OctantPool·GameState의 전후 디스크 해시가 같다. 새 환경 반입·PCG 장식·재질 변경·화면 녹화는 하지 않았다.

세부 경과와 단계별 당시 상태는 [Phase 1 로그](../history/Phase01_Log.md)에만 누적한다. 다음 세션은 과거 대기 상태가 아니라 아래 §9부터 진행한다.

## 9. 새 세션 인수인계(2026-11-14)

사용자 요청으로 이번 세션은 **패키지 준비까지** 마감하고 코드·문서를 한 커밋으로 묶는다. **1-A 전체와 Phase 1은 아직 미완료다.** 다음은 짧은 Medium/적 0 패키지 실행이며 이번 세션에서는 시작하지 않았다.

### 9.1 재선정/재구현하지 않을 것

- 1-B0 선정은 완료했다. [선정 결과](Phase01_RidgeZonePalette.md) §8을 보존한다. 환경 반입은 1-A 마감 후다.
- Ridge slot 1·A/B/C·B 현지 Up +50cm·구면 roll/FOV는 확정된 에디터 검증 경로다. 정확 1440p 패키지에서 문제가 보일 때만 다시 조정하고, 경로가 바뀌면 전후 기준선도 같은 최종 경로로 맞춘다.
- 적 0 인자/집계기/현행 cook 목록은 이전 커밋에서 준비했고 이번에 정규 빌드·패키징까지 했다. 아직 실제 적 0 실행을 통과한 것은 아니다.

### 9.2 패키지·소스·근거

| 항목 | 위치/상태 |
|:---|:---|
| 새 패키지 | `Saved/Graphics/Phase01StartPackage_20261114` |
| 실제 실행 파일 | 위 패키지의 `Windows/LootNPop/Binaries/Win64/LootNPop.exe` |
| 스테이징 경로 JSON | 위 패키지의 `Windows/LootNPop/Scripts/Profiling/Graphics/RidgeViewpoints.json` |
| 경로 SHA256 | `C1E46E0BCEAAB9D9C3FE2F2BCE80BAD37E959663C4775895814350B17E023CE6` |
| 빌드 당시 소스 | HEAD `c6981a6` + Ridge 변경. SourceChanges.patch SHA256 `82A79E536BCF3776F5305965BD41A986E580CEADFE34A6CA52A1A926C9F61DFA`. 이후 인계 문서 갱신은 게임 소스를 바꾸지 않음 |
| 빌드/해시 근거 | `Saved/Profiling/Graphics/Phase01/Build_20261114/`의 SourceSnapshot.json·BuildStatus.json·PackageVerification.json·EditorBuild.log·Automation/index.json·PackageBuild.log |
| 카메라/관통/크래시 근거 | `Saved/Profiling/Graphics/Phase01/RidgePath_20261113/`의 PathVisualValidation.json·Revision 0/1 자료·PreviewCrash_20261114/ |
| 마지막 에디터 회차 | `Saved/Profiling/Graphics/Phase00/Medium_E55B2F1442E65F2309BBAC84A4F92FDE`(폴더명이 Phase00이어도 이번 경로 예비 실행이며 성능 판정 제외) |
| 종료 상태 | LootNPop 에디터 종료, Electric Dreams 에디터 유지. 새 세션에서 재확인. 기존 메모리 CVar는 LootNPop 종료로 소실 |

Saved와 패키지는 git 제외라 다른 체크아웃에는 없을 수 있다. 시작 때 파일/해시부터 확인한다. 이번 인계 커밋으로 HEAD가 바뀌어도 소스/패키지 파일 해시가 같으면 같은 바이너리를 사용한다. 문서 커밋만을 이유로 재빌드하지 않는다. 파일이 없거나 실제 소스가 달라졌다면 병행 여부·에디터 상태 확인 후 별도 새 아카이브로 재빌드한다.

### 9.3 다음 첫 단위 — 짧은 패키지 실행

새 세션에서 사용자에게 진행을 요청받으면 아래를 **한 회차씩** 실행한다. 런처 기본값은 Phase 0 경로이므로 PackageDirectory와 OutputDirectory를 반드시 명시한다. 아래 워밍업 10초·경로 60초는 **예비 검증안**이며 정식 기준선 조건이 아니다. 화면을 보아야 하므로 사용자 확인 가능한 시점에 실행하고 녹화는 하지 않는다.

먼저 Medium:

```powershell
.\Scripts\Profiling\Graphics\RunGraphicsBenchmark.ps1 `
  -Profile Medium `
  -PackageDirectory 'Saved/Graphics/Phase01StartPackage_20261114' `
  -OutputDirectory 'Saved/Profiling/Graphics/Phase01/PackageValidation/Medium' `
  -Repeats 1 -WarmupSeconds 10 -DurationSeconds 60
```

Medium 정상 종료/조건 확인 후, 같은 Medium 계획의 적 0:

```powershell
.\Scripts\Profiling\Graphics\RunGraphicsBenchmark.ps1 `
  -Profile Medium `
  -PackageDirectory 'Saved/Graphics/Phase01StartPackage_20261114' `
  -OutputDirectory 'Saved/Profiling/Graphics/Phase01/PackageValidation/NoEnemies' `
  -Repeats 1 -WarmupSeconds 10 -DurationSeconds 60 `
  -ExtraArguments @('-LNPGraphicsNoEnemies')
```

각 실행의 Run.json 부모 폴더에 `AnalyzeGraphicsBenchmark.ps1 -RunDirectory <폴더> -AllowPreview`를 적용한다. 다음을 확인한다.

1. 프로세스 종료 코드 0·status=complete·with_editor=false·CSV 캡처 행 수/개체 분할 검증·동일 경로/실행 파일/컨테이너 해시.
2. 정확 2560×1440·TSR(`r.AntiAliasingMethod=4`)·ScreenPercentage=100·Epic·시드 1/PCG offset 0·Medium·실제 Ridge slot 1. 정확 출력의 A/B/C 및 연결 구도는 사용자에게 확인받는다.
3. Medium의 실제 초기 적/Pod를 기록한다. 에디터 관측은 2002/120이지만 패키지의 수를 검증 없이 강제 기대값으로 쓰지 않는다.
4. 적 0은 no_enemies=true·performance_eligible=false·초기/마지막/프레임 적 수 0·빈 Population.csv(헤더만)·initial_spawns에 적 행 없음. 공간 계획의 적 요청/배치 로그와 Pod 계획은 Medium과 같아야 한다. `initial_spawns`는 실제 생성 계수이므로 적 행이 없는 것이 정상이다.
5. 두 실행의 set별 계획 로그·Pod 초기화/slot별 수·가능한 배치 근거를 대조한다. 개수 일치만으로 모든 Pod Transform의 일치를 증명했다고 쓰지 않는다. 직접 배치 검증 자료가 부족하면 그 범위를 분리해 기록한다.

빌드/쿡이 필요한 경우에만 직전 병행 여부를 묻고 AGENTS의 권한 확장 규칙을 따른다. 이번 세션은 빌드·cook만 통과했으므로 런타임 실패가 나오면 현재 로그로 원인을 확인하고 기본 편성이나 렌더 설정을 임의로 바꾸지 않는다.

### 9.4 그 뒤의 단위와 새 세션 프롬프트

짧은 Medium/적 0 검증을 마친 뒤 계속 진행할지 확인하고 정식 출발 기준선을 잰다. 출력 2560×1440·TSR100%·Epic·시드 1/PCG offset 0·워밍업 180초·캡처 60초, Medium 3회+적 0 1회다. 회차별 냉각/전원·GPU 환경/해시/재현성을 기록한다. 적 0은 진단으로 별도 집계한다. 1-A를 마감하기 전 환경 반입·PCG·재질 변경·파라곤 선정을 시작하지 않는다.

```text
.context/Graphics/README.md, Current.md, Decisions.md와
.context/Graphics/phases/Phase01_BenchmarkContent.md §9를 읽고 이어서 진행해줘.
1-A의 Ridge 경로(B +50cm)와 정규 빌드·Viewpoints 1/1·출발 패키지 생성은 완료됐어.
먼저 기존 Saved/Graphics/Phase01StartPackage_20261114와 PackageVerification.json의
파일/해시를 확인하고, 짧은 Medium·적 0 패키지 실행 검증부터 진행해줘.
런처의 Phase 0 기본 경로를 쓰지 말고 §9의 명시적 경로를 써줘.
패키지 1440p/TSR·실제 적 0·Pod 계획/배치·정식 기준선은 아직 미검증이야.
1-B0 재선정이나 환경 반입은 시작하지 마.
단위마다 계속 진행할지 확인하고, 수동 조작이 더 효율적이면 즉시 요청해줘.
빌드/Live Coding/쿡이 필요할 때만 실행 직전에 병행 여부를 확인해줘.
```
