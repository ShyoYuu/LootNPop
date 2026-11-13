# Phase 1 환경 원본 후보 비교

> 상태: 대상 확정(2026-11-13) — 기존 Ridge + Electric Dreams, 새 옥탄트 제작은 채택하지 않음(GX-013)
> 조사일: 2026-11-11, 재조사 2026-11-13
> 계기: 사용자가 기존 옥탄트 선택보다 Fab 원본을 먼저 고르고 새 옥탄트를 설계하는 방향을 제안함

2026-11-12 후속: 사용자가 Electric Dreams를 설치했고 8001 에디터에서 [실물 검토](ElectricDreamsReview_20261112.md)를 완료했다. 협곡/숲 제작 원본으로 조건부 적합이며, 아래 웹 조사와 실제 확인 범위를 구분한다.

2026-11-13 후속: Fab 상품 정보를 다시 조회해 라이선스·지원 버전을 확인하고 Goddess Temple 등 후보를 추가했다(§5). 사용자가 **기존 Ridge를 Electric Dreams 기준으로 꾸미는 안**을 확정했다. §3의 새 옥탄트 방향은 Phase 1에서 채택하지 않는다.

## 1. 확인한 후보

Fab의 [Epic Games 퍼블리셔 페이지](https://www.fab.com/sellers/Epic%20Games)와 개별 상품 페이지를 조회했다. 퍼블리셔 페이지의 검색 도구용 본문에는 상품 목록이 없어 개별 상품을 추가 검색했다. 전체 카탈로그를 빠짐없이 조사한 결과는 아니다. 아래 상품은 조회 시점에 무료로 표기돼 있었다. 엔진 5.8 반입·실제 메시/머티리얼 구성·사용자 확보 상태는 아직 검사하지 않았다.

| 원본 | 퍼블리셔 | 확인한 구성 | 프로젝트 적용 판단(제안) |
|:---|:---|:---|:---|
| [Electric Dreams Env](https://www.fab.com/listings/d79688f5-29be-4fb2-a650-2d4a813f5306) | Epic Games | 정글, Quixel 에셋, PCG, 7개 레벨 | 자연 환경 주력 후보. 새 바위 협곡/수풀 옥탄트 또는 Ridge 장식. 기존 Grove의 거목 규모를 충족하는지는 별도 검사 |
| [Paragon: Agora and Monolith Environment](https://www.fab.com/listings/6f401fb5-88b5-41b4-bf1b-62321414e1f0) | Epic Games | 500개 이상 세트 조각·프랍/FX, 샘플 맵에 Kite Demo 식생/추가 프랍 포함 | Acropolis의 판타지 석조 유적과 파라곤 적의 시각적 통일에 유력 |
| [Valley of the Ancient](https://www.fab.com/listings/0c19880e-21bd-42ba-8287-1caccc3951b1) | Epic Games | 2×2km Megascans 환경, 암석·협곡·신화적 어두운 영역 | Hollow의 암석/폐허에 적합한 후보. 전체 다운로드/프로젝트 공간은 상품 설명상 약 205GB라 선택 반입 준비 부담이 큼 |
| [Project Titan](https://www.fab.com/listings/c05aac82-4c1a-4e42-96b3-be668dc40fca) | Epic Games | 8×8km 글로벌 아트잼 월드, 다수 제작자의 콘텐츠 | 보조 원본 후보. 작은 벤치마크용 팔레트를 먼저 추려야 하므로 이번 단계 첫 원본으로는 우선순위를 낮춤 |
| [Open World Demo Collection](https://www.fab.com/listings/3262ab8f-f64a-4124-8efd-82cb19df6249) | Epic Games | 스캔 암석·지면·절벽·줄기, 사진 기반 식생, LOD/충돌 | Agora의 식생 보충 후보. 2015년 워크플로 기반이라 현행 렌더 설정과 실물 품질 검토 필요 |
| [Dark Ruins Megascans Sample](https://www.fab.com/listings/836ed2f8-e2d6-49be-98d3-59d104bd351e) | **Quixel Megascans** | 무료 샘플 프로젝트, 동굴·사원·폐허·절벽·던전 테마 | Epic Games 정확 일치 필터에서는 제외되는 추가 후보. Hollow 또는 새 유적 옥탄트에 검토 가치가 큼 |

위 적용 판단과 우선순위는 프로젝트 목적에 따른 추론이며 상품의 호환성 보장이 아니다. 후보별 정확한 적용 라이선스 원문은 반입 전에 확인한다. 무료 표기와 공개 저장소 재배포 권한은 구분한다.

## 2. Electric Dreams의 지형은 무엇인가

[공식 샘플 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine)에 따르면 전체 환경과 PCG 버전은 4×4km이고, `ElectricDreams_PCGForest`는 작은 Landscape 패치 위의 숲이다. 반면 `ElectricDreams_PCGCloseRange`에는 하상·개울 영역과 큰 절벽 구조가 있고, `ElectricDreams_PCGLargeAssembly`에는 절벽을 구성하는 Assembly들이 분리돼 있다. 따라서 샘플에는 지형·공간 구성이 있으며, 전부 단일 Landscape이거나 전부 바로 반입할 수 있는 지형 메시라고 단정하면 안 된다.

첫 실물 검토는 다음 두 레벨을 우선한다.

1. `/Game/Levels/PCG/ElectricDreams_PCGCloseRange`
2. `/Game/Levels/PCG/Breakdown_Levels/ElectricDreams_PCGLargeAssembly`

원본의 개울/유체 시스템을 새 기능으로 도입하자는 제안은 아니다. 바위와 식생으로 만드는 지형 단면·배치·시선 구성을 검토한다. 공식 [PCG 분석](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-in-electric-dreams)에는 Landscape 표면뿐 아니라 바위 메시 표면의 클로버 배치도 설명돼 있다. 원본 그래프가 구면 중력에 그대로 맞는지는 미검증이다.

Valley of the Ancient도 평면 Landscape만으로 이해하면 안 된다. [공식 지형 제작 설명](https://dev.epicgames.com/documentation/en-us/unreal-engine/valley-of-the-ancient-sample-game-for-unreal-engine)은 Megascans 메시와 Packed Level Instance로 작은 조합부터 큰 지형 덩어리까지 만드는 방식을 명시한다.

## 3. 원본을 먼저 고르는 설계 방향

사용자의 제안에 동의한다. 사용할 수 있는 실제 바위·절벽·아치·식생의 크기와 조합을 먼저 확인하고 이에 맞춰 한 옥탄트의 공간을 설계하면, 블록아웃을 나중에 원본 에셋으로 덮으며 생기는 형상/재질 불일치를 줄일 수 있다.

다만 원본 샘플 전체를 구면에 복제하기보다 대표 구역 하나를 재구성하는 범위를 권한다. 예를 들면 Electric Dreams의 바위 협곡·수풀을 바탕으로 열린 전투장 2곳과 연결 동선·상부 훅 구간을 설계한다. 개수와 형상은 아직 채택된 사양이 아니다. 구면 지각·충돌·Support·이음매는 기존 제작 규약을 따르며, 큰 지형 조각을 평면 위치 그대로 회전 배치하는 것으로 해결됐다고 간주하지 않는다.

선택 방향은 다음과 같다.

- 자연 지형과 PCG·Nanite 렌더 비용이 주목적: **Electric Dreams를 첫 실물 검토 원본으로 추천**.
- 파라곤 적과 어울리는 판타지 유적 완성과 기존 동선 재사용이 우선: **Agora and Monolith + Acropolis**.
- 더 어두운 동굴/폐허 컨셉이 우선이고 Quixel 퍼블리셔도 포함: **Dark Ruins와 Valley를 비교**.

새 옥탄트 제작은 기존 장식 작업보다 형상·충돌·베이크·통행 검증 범위가 늘어난다. 확정할 경우 Phase 1의 포함 범위를 다시 합의한다. GX-004의 옥탄트 1개 원칙을 여러 테마 동시 제작으로 확대하지 않는다.

## 4. 측정 순서에 미치는 영향

아직 정식 출발 기준선을 재지 않았으므로 지금 대상 방향을 정하는 것이 좋다. 새 옥탄트가 채택되면 **실물 원본 확인 → 형상/동선 블록아웃·풀/고정 시드·카메라 확정 → 출발 기준선 → 고밀도 장식/적 → 같은 경로 재측정** 순서를 제안한다. 새 지형이 들어가기 전 측정과 들어간 뒤 측정을 장식 에셋만의 효과로 해석하지 않는다.

이는 제안이며 기존 로드맵의 비교 기준을 확정 변경한 것은 아니다. 다음은 사용자가 원본 후보를 고른 뒤 실제 프로젝트에서 대표 메시를 확인하는 단계다.

2026-11-13 확정으로 새 옥탄트를 만들지 않으므로, 위 순서 가운데 "형상/동선 블록아웃"은 빠진다. **Ridge 경로·카메라 확정 → 출발 기준선 → Electric Dreams 장식·적 → 같은 경로 재측정** 순서로 진행한다.

## 5. Fab 재조사와 대상 확정(2026-11-13)

### 5.1 조회 결과

Fab 상품 페이지와 그 페이지가 쓰는 상품 정보 응답(`/i/listings/<id>`, `/asset-formats/unreal-engine`)을 브라우저에서 조회했다. Epic Games 판매자에서 무료·"environment"로 검색하고, Megascans 샘플은 "megascans sample"·무료로 검색했다. 전체 카탈로그를 빠짐없이 조사한 결과는 아니다. 아래 팩은 모두 **Standard License이며 Personal·Professional 두 등급 모두 0원**이었다.

| 팩 | 퍼블리셔 | 지원 UE | 성격 | 맞는 테마(제안) |
|:---|:---|:---|:---|:---|
| [Electric Dreams Env](https://www.fab.com/listings/d79688f5-29be-4fb2-a650-2d4a813f5306) | Epic Games | 5.2–5.8 | Megascans 실사 스캔·PCG. "Allows usage with AI: No" | Ridge·Grove |
| [Valley of the Ancient](https://www.fab.com/listings/0c19880e-21bd-42ba-8287-1caccc3951b1) | Epic Games | 5.0–**5.7** | Megascans 2×2km, 약 205GB | Hollow |
| [Dark Ruins Megascans Sample](https://www.fab.com/listings/836ed2f8-e2d6-49be-98d3-59d104bd351e) | Quixel Megascans | 5.5–5.8 | 동굴·폐허·던전 | Hollow |
| [Goddess Temple Megascans Sample](https://www.fab.com/listings/471eaad7-3c05-45bd-ac37-02dddb17780f) | Quixel Megascans | 4.25–5.8 | 실사 판타지 신전, 레이 트레이싱 켜짐(RTX 필요) | Acropolis |
| [Paragon: Agora and Monolith](https://www.fab.com/listings/6f401fb5-88b5-41b4-bf1b-62321414e1f0) | Epic Games | 4.19–5.8 | Paragon 세트 조각 500개 이상, Nanite 이전 세대. PARAGON 상표 사용 금지 | Acropolis |
| [Medieval Village Megascans Sample](https://www.fab.com/listings/2e11a225-a6ea-4781-a3e1-fe975b461894) | Quixel Megascans | 5.3–5.8 | 중세 마을 | 보조 |
| [Open World Demo Collection](https://www.fab.com/listings/3262ab8f-f64a-4124-8efd-82cb19df6249) | Epic Games | ~**5.2** | 2015년 스캔 | 보조(오래됨) |

제외: Stylized Lake Village(UEFN 참조 전용 등급), Soul: Cave(2014년 모바일용, 우선순위 낮음).

### 5.2 라이선스 해석

- Standard License는 수정·상업적 이용을 허용한다. 원본 에셋 재배포 권한은 아니므로 GX-004의 git 제외는 그대로다([Fab 라이선스](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab)). 세부 조건의 원본은 Fab EULA다.
- Electric Dreams는 AI 사용 불허로 표시돼 있다. 이 에셋이나 렌더를 ComfyUI 같은 생성 AI의 입력으로 쓰지 않는다.

### 5.3 결정과 근거

**Phase 1 대상은 기존 Ridge, 환경 원본은 Electric Dreams다(GX-013).** 팩을 먼저 고르는 원칙은 유지하되, 새 옥탄트가 아니라 기존 옥탄트를 그 팩의 모듈 크기에 맞춰 손본다.

- **새 옥탄트를 만들지 않는 이유:** 평면·단면 설계·지각·충돌·Support 베이크·앵커/Pod·이음매·풀 등록·Phase 15 경로 재측정이 다시 필요하다. Grove는 SSN Phase 9에서 V001~V042가 걸렸다. 장식 전 기준선도 결국 직접 만든 블록아웃이 된다. Electric Dreams의 "지형"은 작은 Landscape 패치와 약 70m 조합이라 따라 만들 고정 지형이 없다.
- **Grove가 아닌 Ridge인 이유:** Grove의 상징인 거목은 높이 100~170m인데 Electric Dreams 나무(Hornbeam)는 약 16m라, 하층 식생·뿌리·바위만 채울 수 있다. Ridge는 지층 단(가장자리 6m 절벽)·협곡·넓은 볼록 지면·분지로 이뤄져 절벽 외장·묻힌 바위·지피식물 PCG가 그대로 맞물린다. 시야가 길어 Nanite·Lumen 부하 시험에도 유리하다.
- **1차 장식은 보행 충돌을 바꾸지 않는다.** Ridge는 Phase 15 경로 재측정 대상이다. 벽 외장과 충돌 없는 장식으로 시작하고, 보행면을 바꾸면 Support/Nav 재베이크와 그 영향 기록을 별도 단위로 다룬다.
- **월드 전체 스타일:** 플레이어는 구 안쪽 벽 위에 있어 반대편 옥탄트가 한 화면에 함께 보인다. 나중에 다른 옥탄트를 꾸밀 때는 Megascans 계열(Electric Dreams·Valley·Dark Ruins·Goddess Temple)로 맞추는 쪽이 섞임이 적다. Acropolis의 Goddess Temple과 Agora 비교, Bastion·Pyramid용 팩 조사는 그래픽스 Phase 1 범위 밖이다.

Goddess Temple·Dark Ruins는 상품 정보만 봤고 실제 메시·크기는 확인하지 않았다.
