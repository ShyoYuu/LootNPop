# Electric Dreams 실물 적합성 검토

> 상태: 읽기 전용 검토 완료 · 새 협곡/숲 옥탄트의 원본으로 조건부 적합
> 검토일: 2026-11-12
> 요청: 사용자가 Electric Dreams를 설치하고 8001 포트 에디터에서 실물 검토 요청
> 현재 적용: 이후 GX-013으로 **기존 Ridge 장식**을 채택했다. 아래 새 옥탄트 제안은 당시 검토 이력이며 현재 실행 방침은 [1-B0 선정 결과](../phases/Phase01_RidgeZonePalette.md) §8을 따른다.

## 1. 결론

**이 팩으로 새 협곡/숲 옥탄트를 구성할 재료가 충분하며, 조사 범위에서 반입을 막는 치명적인 의존성은 발견하지 못했다.** 절벽·바위·지면·하상·뿌리·나무·지피식물이 실제로 포함돼 있다. `PCGCloseRange`의 협곡 조합을 공간/재료 참고로 삼고, 기존 구면 지각·충돌·Support 규약에 맞춰 한 옥탄트를 설계하는 안을 권한다.

원본의 PCG 그래프·평면 배치를 그대로 사용한 구면 제작이 검증된 것은 아니다. 원본 PCG의 월드 Z 기준, 큰 조합의 곡률 적응, 시각 메시와 보행 충돌의 정합을 별도로 해결해야 한다. **LootNPop 반입·회전 실험·베이크·PIE·패키지 성능은 미검증**이다. 이번 결과는 제작 재료의 적합성 판정이며 제작 완료나 60fps 보장이 아니다.

## 2. 대상과 검토 범위

- MCP: `unreal_mcp_second`, 8001 사용자 지정 연결. 라이브 조회가 `/Game/Levels/Startup`과 Electric Dreams 콘텐츠를 반환했다. 동일 시각 소스 로그에서도 도구 호출을 확인했다.
- 프로젝트: `D:/UnrealProjects/ElectricDreamsEnv/ElectricDreamsEnv.uproject`.
- 실행 엔진: 로그 기준 UE **5.8.4-58902052**. 프로젝트 연결 버전 5.8.
- 처음 PIE 미실행·Startup 미저장 변경 없음 확인 후 `/Game/Levels/PCG/ElectricDreams_PCGCloseRange`를 열었다. 원본 에셋 편집·저장·PCG 재작성·LootNPop 반입은 수행하지 않았다.
- Asset Registry에서 `/Game` StaticMesh **600개**, `/Game/Levels` World **8개**를 확인했다. 600개는 헬퍼·시험·대체 메시를 포함한 수량이며 전부 사용 후보라는 뜻이 아니다.
- 대표 메시 **17개**의 태그/직접 의존성, 그중 **4개**의 재질 슬롯·BodySetup, 부모 재질 3종과 함수, PCG 3종의 구조/일부 설정을 확인했다.
- CloseRange에서 조회된 액터 **117개** 중 LandscapeProxy는 **0개**다. 이는 현재 로드된 액터 조회 범위다. 협곡의 큰 조합과 지면 조합에서 실제 메시/재질 오버라이드를 확인했다.

원자료는 `Saved/Profiling/Graphics/Phase01/ElectricDreamsReview_20261112/`의 `AssetInventory.json`, `AssetSamples.json`, `MaterialAndGraphs.json`, `DependencyClosure.json`, `ActiveComponents.json`, `InspectionDetails.json`, `DiskFootprint.json`에 있다. 원본 라이선스 에셋 파일은 이 저장소로 복사하지 않았다.

## 3. 실제 재료와 크기

아래 치수는 Registry `ApproxSize`를 cm→m로 환산한 **원본 로컬 AABB**다. 배치 회전·스케일 뒤의 높이/보행 폭과 다르다. Nanite 삼각형과 fallback 삼각형을 구분했다.

| 메시 | 로컬 크기(m) | Nanite 삼각형 | fallback 삼각형 | 용도 판단 |
|:---|:---|---:|---:|:---|
| `SmartAssets/AssetMeshes/SM_Cliff_01` | 13.53×9.26×14.19 | 3,996,469 | 11,599 | 실제 큰 조합이 쓰는 주력 절벽 후보. 썸네일 외형 확인 |
| `MassiveSandstoneCliff/SM_MassiveSandstoneCliff_01` | 28.28×16.08×30.13 | 2,999,999 | 16,775 | 큰 랜드마크/절벽 후보. 충돌 설정 재검토 필요 |
| `HugeSandstoneCliff/SM_HugeSandstoneCliff_01` | 15.08×7.70×10.66 | 2,000,000 | 18,063 | 중간 절벽 조각 후보 |
| `ForestTerrain/SM_ForestTerrain_01` | 1.66×2.53×0.28 | 1,998,050 | 26,024 | 얇은 지면 패치. 옥탄트 전체 지각으로 취급하지 않음. 썸네일 확인 |
| `DryRiverBed/SM_DryRiverBed_01` | 1.01×1.66×0.05 | 999,999 | 29,847 | 하상/표면 장식 패치 |
| `EuropeanHornbeam/.../SM_EuropeanHornbeam_Forest_01` | 8.58×9.98×16.15 | 755,677 | 13,206 | 일반 숲 나무 후보, 재질 슬롯 3개. 썸네일 확인 |
| `CloverVarieties/SM_CloverVarieties_01` | 1.36×1.28×0.15 | 3,668 | 1,716 | 지피식물, 재질 슬롯 1개 |
| `Fern/SM_Fern_01` | 1.56×1.88×1.03 | 1,714 | 306 | 저층 식생 후보 |

크기/형태가 다른 바위·이끼 바위·뿌리·쓰러진 나무도 있다. 대표 조사 17개 모두 NaniteEnabled=true였다. 다만 지피식물의 masked 재질·WPO·그림자 비용은 이 사실로 면제되지 않는다.

**선별 주의:** `/Game/Meshes/_GENERATED/RockFormation/SM_RockFormation_01`은 100cm 큐브·12삼각형이고, `/Game/Megascans/3D_Assets/HugeMossyForestCliff/SM_HugeMossyForestCliff_01`은 직접 로드 시 재질 슬롯이 `None`이었다. 헬퍼/시험 에셋이나 배치 오버라이드용일 수 있으므로 팩 오류로 단정하지 않는다. 이름/폴더만 보고 완성된 지형 메시로 채택하지 않는다.

## 4. 원본 조합과 구면 적응

CloseRange의 대표 `BP_PCG_LargeAssembly`에서 StaticMeshComponent 계열 **277개·서로 다른 메시 182개**를 읽었다. `PCGDemo_GroundBP`는 **59개·55종**이었다. 이는 컴포넌트/메시 종류 수이며 인스턴스 수·드로우콜과 다르다. 실제 배치에는 기본 슬롯과 다른 `overrideMaterials`가 다수 있으므로 메시 파일만 옮겨 같은 외형이 재현된다고 가정하지 않는다.

큰 조합의 액터 bounds는 약 **69.51×69.51×45.60m**다. 헬퍼/장식이 포함된 조합 bounds이며 보행 가능 범위가 아니다. 반지름 300m 구면에서 폭 69.5m를 평평한 현으로 놓았을 때 중간 높이 차는 약 **2m**다(`R-sqrt(R²-(L/2)²)`, 설명용 근사). 조합 전체를 한 번 회전시키는 방식은 넓은 밑면의 뜸/관통을 해결하지 못한다.

권장 적용:

1. 원본의 절벽/숲 재료와 공간 구도를 확인해 전투장·우회·훅 경로를 설계한다. 원본의 좁고 복잡한 개울 길을 대규모 전투 공간으로 그대로 채택하지 않는다.
2. 지각·보행/벽 외피와 Support는 기존 제작 규약에 따라 만든다. 보행 표면을 바꿀 아트는 충돌·베이크에도 반영해 시각과 판정의 소유권을 맞춘다.
3. 큰 절벽은 구간별로 방향/접합을 맞추고, 작은 식생은 접지 법선과 구면 현지 Up에 정렬한다. 300m 반지름 때문에 필요한 곡률 적응량을 실제 배치에서 측정한다.
4. 기존 풀/이음매를 보존한 독립 시험에서 한 전투장과 짧은 연결 구간부터 검증한다. 최종 형상 채택 뒤 동일 경로의 장식 전후 기준선을 잡는다.

이 검토로 새 옥탄트 이름·도면·규모·풀 등록은 확정하지 않았다. 새 제작은 Phase 1의 장식 범위보다 형상/베이크 검증이 늘어나므로 포함 범위를 다음 단위에서 정한다.

## 5. PCG — 그대로 쓸 수 없는 확인된 이유

| 그래프 | 노드 / 연결 | 확인한 제약 |
|:---|---:|:---|
| `/Game/PCG/Graphs/Forest/PCGDemo_Forest` | 412 / 514 | `Get Landscape Data`와 경사 필터. `NormalToDensity_0.normal=(0,0,1)` 직접 확인 |
| `/Game/PCG/Graphs/Ground/PCGDemo_Ground` | 237 / 272 | `WorldRayHit_32`의 원점=(0,0,1800), 방향=(0,0,-1), 길이=1500, 기본값 덮어쓰기=true |
| `/Game/PCG/Graphs/LargeAssembly/LargeAssemblyGraph` | 243 / 297 | 다수 Execute Blueprint·Spline·Assembly 입력과 계층 조립, 전체가 작은 범용 scatter 그래프는 아님 |

따라서 원본은 **조합 규칙의 참고 자료**로 삼고, 실제 배치 경로는 LootNPop의 현지 Up/표면 법선을 소비하도록 구성해야 한다. 경사 판정은 월드 Z 내적 대신 현지 Up과 표면 법선의 관계를 사용하고, 광선도 구간별 방사 방향을 따른다. 기존 그래프 전면 수정과 신규 배치 그래프 중 어느 쪽이 더 작은 변경인지는 대표 구역 범위를 확정한 뒤 정한다. 이번에는 어느 그래프도 수정/실행 재생성하지 않았다.

## 6. 재질·렌더 설정

- 소스와 LootNPop 모두 `r.Substrate=True`, DX12·SM6 설정이다. 대표 재질들의 Nanite·InstancedStaticMeshes usage는 true였다. 해당 설정만으로 반입 셰이더 검증을 대체하지 않는다.
- 바위 부모 `M_MS_Default_Material_VT_DynamicLayering`은 Opaque·단면이며 WPO 출력이 비어 있고 Substrate Legacy Conversion을 쓴다. `MF_WaterEffect` 등 커스텀 함수와 텍스처 의존성이 있다.
- 나무 부모 `M_Foliage`는 Masked·양면이며 `MF_SimpleWind`를 통해 WPO 출력이 연결돼 있다. `MF_WindGust`에는 월드 위치 **XY** 마스크가 직접 연결된다. 구면 전역에서의 바람 방향/공간 분포는 회전 검증이 필요하다. 이 사실만으로 바람이 실제로 깨졌다고 선언하지 않는다.
- Clover 부모 `M_MS_CustomFoliageNoWPO`는 Masked·양면이지만 WPO 출력은 비어 있다. 함수 그래프에 wind 함수 참조가 있다는 이유만으로 실제 WPO 비용이 있다고 판단하지 않는다.
- 대표 5개 메시 의존성의 텍스처 66개 중 **65개가 `VirtualTextureStreaming=True`**다. 해상도는 4K 29개·2K 23개·1K 5개·256² 5개·32² 4개였다. 이것은 스트리밍 가상 텍스처이며 RVT의 지면 투영과 구분한다.
- 소스 에디터 실효값은 `r.VirtualTextures=1`. LootNPop은 명시적 설정이 없지만, 설치 엔진 `Engine/Source/Runtime/Engine/Private/Texture.cpp`의 해당 CVar 기본값도 **1**이다. 따라서 설정 부재만으로 VT가 꺼져 있다고 판단하지 않는다. 반입 시험에서 대상 에디터와 패키지 실효값·샘플러 호환성을 확인한다. 이번 검토에서 설정 변경은 하지 않았다.

원본 프로젝트 설정 전체를 옮길 필요가 있다는 근거는 없다. 필요한 메시·재질/함수·텍스처를 선택해 의존성과 함께 반입하고, 현재 렌더 조건에서 표시를 확인하는 순서를 권한다. 원본의 안개·수면·유체·오디오·드론 기능은 이번 협곡 제작의 필수 반입 목록으로 취급하지 않는다.

## 7. 충돌과 Support

원본의 충돌 설정은 균일하지 않다.

- `SM_Cliff_01`: Registry상 단순 충돌 프리미티브 64개.
- `SM_MassiveSandstoneCliff_01`: BodySetup `CTF_UseDefault`, 단순 충돌 형상 0개, collision LOD 0. Nanite 약 300만 삼각형과 fallback 16,775개를 구분해야 한다.
- `SM_HugeMossyForestCliff_01`: complex-as-simple, 단순 형상 0, 양면 충돌=false.
- Hornbeam: 기본 정책, 캡슐 형상 1개.
- Clover: 기본 정책, 단순 형상 0개.

Nanite 시각 품질을 그대로 보행/Support 충돌의 정밀도로 해석하지 않는다. 얇은 바닥 패치·열린 스캔 조각을 연결된 지각으로 사용하려면 실제 형상/뒷면/접합과 양방향 캡슐 통행을 검증해야 한다. 보행/착지 표면과 배경 장식을 구분하고, 최종 아트로 충돌이 바뀌면 기존 규약에 따라 Support/Nav를 다시 베이크한다. 원본 맵에서의 충돌이 LootNPop의 Mover·NPC·Support 계약까지 통과한 것은 아니다.

## 8. 선택 반입 의존성과 비용

아래 **5개만** 출발점으로 삼아 `get_dependencies`의 `/Game` 전이 의존성을 중복 없이 추적했다.

1. `SM_Cliff_01`
2. `SM_MassiveSandstoneCliff_01`
3. `SM_ForestTerrain_01`
4. `SM_EuropeanHornbeam_Forest_01`
5. `SM_CloverVarieties_01`

결과: **122개 패키지·원본 파일 1,628.21MiB(약 1.59GiB)**. StaticMesh 5, MaterialInstance 8, Texture2D 66, MaterialFunction 32, Material 4, MaterialLayer/Blend/Instance 4, MPC 1, PhysicalMaterial 2다. 원본 `.uasset` 크기이며 cook 크기·VRAM/런타임 메모리 수치가 아니다. 가장 큰 파일은 `SM_Cliff_01.uasset` 약 365.58MiB였다.

이 5개 전이 범위에서 `ElectricDreamsSample` 커스텀 C++ 모듈이나 샘플 게임플레이 BP 의존성은 발견하지 못했다. 외부 참조는 엔진 함수/텍스처와 NavigationSystem·PhysicsCore·Interchange 계열이었다. 전체 PCG/Blueprint 조합까지 같은 결론을 확장하지 않는다. 실제 Migrate 결과·리다이렉터·누락 의존·cook 성공은 아직 검사하지 않았다.

## 9. 다음 단위의 제안과 미검증

제안: 협곡 주력 절벽 2~3종·바닥 1~2종·나무 1종·낮은 식생 2~3종으로 대표 팔레트를 정하고, 한 전투장/우회 구간을 설계한다. 실제 큰 조합에는 182종이 쓰이지만 이 수를 그대로 이번 벤치마크 범위로 삼지 않는다.

다음은 원본 에셋과 파생물의 git 제외 경로를 확정한 뒤 작은 반입 시험이다. 원점·구면 적도 부근·반대 방향에서 재질/바람/접지/충돌을 확인하고, 사용자 PIE로 Mover·카메라·NPC 접근을 검증한다. 그 후 새 옥탄트 제작 범위와 Phase 1 일정/측정 순서를 확정한다.

미검증: 원본 단위 메시의 전체 토폴로지·스캔 뒷면, 타깃 반입 셰이더·VT, 바람 회전, 실제 충돌/캡슐·Support 베이크, 최종 구면 지형·이음매, 패키지 성능·스트리밍 압박, 사용자 플레이 확인. 이번 검토 때문에 추가 C++·자동화 도구·성능 테스트를 만들지 않았다.
