# Phase 1-B — 환경 반입과 작은 시험

> 상태: 1-B4a 근경 시험 마감(사암/클로버 통과·하상 제외) · 다음 이끼 바위/절벽 소수 시험 확인 대기 · 1-B3c 화질/비용 조건부
> 작성: 2026-11-14
> 마지막 갱신: 2026-11-15
> 상위: [Phase 1 실행서](Phase01_BenchmarkContent.md)
> 선정 원본: [1-B0 결과](Phase01_RidgeZonePalette.md) §8

## 1. 범위와 단위

기존 Ridge 대분지를 먼저 꾸민 뒤 전체로 확장한다. 사암 주조·분지/하상에 제한한 이끼·낮은 식생, 원본 지형 재질 파생본과 6종 조건부 메시를 유지한다. 형상·보행 충돌·Support source·확정 카메라 경로는 보존한다. 각 단위가 끝나면 사용자에게 다음 단위 진행을 확인한다.

| 단위 | 작업 | 통과 기준 |
|:---|:---|:---|
| 1-B1 준비 | git 제외·원본 식별 목록·반입/배치 방식·작은 시험 범위 | 원본 파일/기존 해시·경로 충돌·git 제외 검사, 절차 기록 |
| 1-B2 반입/로드 | 수동 Migrate, 타깃 의존성·재질·VT 확인 | 반입 목록 대조, 누락 참조/컴파일 오류 없음, VT 실효값 및 원본 보존 확인 |
| 1-B3 지각 재질 시험 | 원본 그래프 파생본의 좌표·노멀·구면 경사/구역 마스크 | 대표 구역 근경·급경사·회전 slot에서 반복/색/노멀 확인, WPO 없음, 범위 밖/지하 보존 |
| 1-B4 메시 소수 시험 | 6종을 하나씩 배치해 채택 여부 판단 | 접지/노출/카메라/벽보행·발/착지·바람/시야 확인, 실패 역할 축소/제외 |
| 1-B5 대표 구역 PCG | 시험을 통과한 역할만 고정 시드로 배치 | 전체 bounds 보호 마스크·동선·SurfaceData 현재성·런타임 로드 검증 |
| 1-B6 전체 확장 | 승인한 대표 구역 규칙을 Ridge로 확장 | 적도·이음매·다른 slot·전투/훅 동선 및 패키지 검증 |

1-B3·4는 결과를 보고 더 작은 구현 단위로 나눈다. 성능 판정은 패키지에서만 한다. 파라곤 선정은 1-C다.

## 2. 원본·파생본·git 소유권

반입 원본은 `D:/UnrealProjects/ElectricDreamsEnv`(UE 5.8.4)다. 원본 프로젝트에서 에셋 편집·저장·PCG 재생성을 하지 않는다. 미저장 변경이 있다면 사용자 작업과 구분하며 임의로 저장/복원하지 않는다.

원래 `/Game/...` 패키지 경로를 유지해 LootNPop `Content`로 Migrate한다. 준비 당시 동일 경로 충돌0개를 확인한 방식이다. 현재는 반입돼 있으므로 재반입하지 않는다. 전용 하위 폴더로 파일만 복사하거나 반입 직후 이동하지 않는다. 새 체크아웃/다음 반입 직전에는 충돌을 다시 확인하고, 있으면 덮어쓰지 말고 먼저 비교한다.

원본 식별은 저장소의 [ElectricDreamsPalette.json](../../../Scripts/Profiling/Graphics/ElectricDreamsPalette.json)이 소유한다. 명시 시작점 14개, 전이 패키지 104개(클래스·원본 바이트·SHA256), 엔진/스크립트 외부 참조가 있다. Saved 조사 자료 없이도 원본을 식별하고 반입 결과를 대조할 수 있다. 파일 크기/해시는 반입 전 원본 기준이다. UE 5.8 Migrate 자체가 대상 패키지를 재저장하므로 원본과 해시가 다를 수 있다(§6). 재생성 때 바이트 동일성을 강제하지 않고 파일/클래스/의존성·주요 에셋 데이터를 함께 검사한다.

| 용도 | 위치 | 추적 정책 |
|:---|:---|:---|
| 원본/공유 의존성 | `Content/Custom/CustomMaterials/`, `Content/Megascans/`, `Content/SmartAssets/` | `.gitignore` 제외 |
| 식생 공유 의존성 | `Content/MSPresets/MS_Foliage_Material_LATEST/`, `Content/MSPresets/MSVTTextures/` | 제외 |
| 지형 부모/MI | `Content/Landscape/M_BGLandscape_Auto.uasset`, `MI_BGLandscape_Auto_01.uasset` | 두 파일만 제외 |
| 하상 충돌 참조 | `Content/Meshes/_GENERATED/RockFormation/SM_DryRiverBed_02.uasset` | 이 파일만 제외·원본 참조 유지 |
| 물리 재질 | `Content/PhysicalMaterials/PhysMat_SolidRock.uasset` | 이 파일만 제외 |
| 타깃 파생 재질·함수·PCG | `/Game/Graphics/Environment/ElectricDreams/` | 제외·재생성 코드/설정만 추적 |
| 시험 LVI·재질 적용·소수 배치 | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/` | 기존 WIP 제외 규칙 사용 |
| 재생성 목록·실행 코드·절차 | `Scripts/Profiling/Graphics/`, 이 문서 | 추적 |

위 폴더는 이번 원본용으로 사용한다. 자체 제작 에셋은 원본 제외 폴더에 넣지 않는다. 실제 재생성 코드/설정과 수동 보정은 §9~13, 새 세션의 읽기 순서와 마지막 로컬 상태는 [Current](../Current.md)를 따른다. 현재 WIP에는 V002 재질과 최종 사암2/클로버6이 있다. PCG는 아직 없고 정식 LVI 적용은 시험 채택 뒤 별도 단위에서 한다.

## 3. 새 체크아웃의 수동 Migrate(현재 반입 완료)

의존성 확인 GUI는 수동 조작이 더 효율적이므로 사용자에게 요청한다. 자동화용 복사기나 GUI 제어 장치는 만들지 않는다.

1. Electric Dreams 프로젝트를 열고 원본에 미저장 변경이 있는지 확인한다. LootNPop의 병행 편집/PIE도 확인한다. 원본은 저장하지 않는다.
2. Content Browser에서 JSON의 `roots` 14개를 선택한다. 이름/경로는 [선정 결과](Phase01_RidgeZonePalette.md) §8.6~8.7에도 있다. 6메시·실효 MI 6개·`M_BGLandscape_Auto`/`MI_BGLandscape_Auto_01`이며 이끼 바위 MI는 **09**다.
3. 우클릭 → Asset Actions → Migrate를 실행한다. Asset Report를 P4의 104개 목록과 대조하며 의존성을 임의로 해제하지 않는다. 수가 다르거나 추가 의존성이 있으면 복사 전에 목록 차이를 확인한다. `_GENERATED` 하상 메시 1개도 의존성이므로 유지한다.
4. 목적지는 정확히 **`D:/UnrealProjects/LootNPop/Content`**다. 패키지 이름이 겹친다는 안내가 나오면 덮어쓰기를 진행하지 않고 충돌을 확인한다. 원본 맵·PCG 그래프·Landscape Actor·팩 전체는 선택하지 않는다.
5. 완료 후 타깃 파일 목록/해시·누락/추가 파일·git 제외를 검사한다. 별도 타깃 저장 전에 반입본 해시를 기록하고 원본과 비교한다. Migrate 재저장에 따른 차이는 §6처럼 구분하며 원본 해시 일치만을 반입 통과 조건으로 쓰지 않는다. 에디터가 이미 열려 있었다면 Asset Registry 갱신 여부도 확인한다.
6. 타깃에서 원본 메시 6개와 MI·지형 부모를 로드하고 누락 참조·재질 컴파일·VT/샘플러·Nanite 원본 상태를 확인한다. 에디터가 쓸 수 있는 DDC와 셰이더 컴파일 완료를 확인한 뒤 표시를 판정한다. 정식 Ridge에는 아직 적용하지 않는다.

**VT 선결 확인:** 아래는 반입 전 확인 절차다. 원본66텍스처 중65개가 VT다. 타깃은 §6~7에서 설정 변경 없이 실효 `r.VirtualTextures=1`과 로드를 통과했다. 새 환경에서는 설정 파일 부재만으로 실행 중 실효값을 확정하지 말고 CVar와 재질 로드를 확인한다. 활성화/재시작이 필요하면 별도 구현 단위로 다루며, 원본 VT 일괄 비VT 변환이나 원본 재질 수정으로 우회하지 않는다.

전역 렌더 설정을 바꾸면 기존 1-A와 비교 조건이 달라질 수 있다. 이 경우 콘텐츠 적용 전에 같은 새 설정의 대조 측정을 정하고, 기존 1-A 원자료는 보존한다. 아직 설정 변경·빌드·쿡·추가 측정은 하지 않았다.

1-B1 준비 당시에는 Unreal MCP가 노출되지 않았다. 이후 1-B2에서 8000/8001 라이브 호출을 확인했고, 원본 에디터8001은 VRAM 확보를 위해 종료했다. 다음 세션의 마지막 상태/주의사항은 Current를 따른다.

## 4. 작은 시험과 배치 방식

첫 시험은 정식 Ridge를 보존한 WIP 사본에서 한다. 116m 대절벽을 메시로 전부 덮지 않는다. 지각 재질로 큰 면을 만들고 필요한 부분만 보강한다.

- **재질 먼저:** 지형 원본 부모/MI의 파생본을 만든다. 구면 현지 Up·표면 좌표/노멀·대표 구역/외부 표면 마스크를 시험한다. 기존 UV0나 원본 WorldAligned의 XY 출력만으로 적합하다고 판단하지 않는다. WPO/변위는 쓰지 않는다.
- **소수 수동 배치:** 각 메시 1개부터 역할별로 확인한다. Cliff 부분 보강, Sandstone/MossyForest 바위, DryRiverBed 접지, Clover 낮은 지피, Fern 바람/회전 순이다. 필요할 때만 비교용 인스턴스를 더한다. 정확한 돌출/스케일/밀도는 관측 뒤 정한다.
- **배치 계약:** `LNP.Surface.Decoration` 태그 단독 + `LNPDecoration` profile·NoCollision. Support/Blocker/Static 태그를 섞지 않는다. 원본 StaticMesh 충돌 데이터나 하상의 complexCollisionMesh 참조는 준비 단계에서 제거하지 않는다.
- **구면 검증:** 현재 Ridge slot 1과 회전된 다른 Ridge에서 확인하고 적도 부근을 포함한다. 월드 Z를 Up으로 사용하지 않는다. 카메라·벽보행 적·발/착지·탄환의 시각 정합이 나쁘면 외장 역할을 줄인다.
- **PCG는 채택 후:** 수동 시험 결과를 고정 시드·재현 가능한 PCG 규칙으로 옮긴다. 경로/착지, Pod의 3D 편성 공간, 훅, 입구, 카메라, 이음매 보호를 메시 전체 bounds로 검사한다. 수량/보호 폭은 1-B0 초안을 확정값으로 사용하지 않는다.

원본 메시의 DF/카드 생성·원본 MI·Nanite/fallback과 타깃 컴포넌트의 Lumen/RT·그림자 기여는 적용 시 기록한다(O-3). 최적화 절제와 표시 적합성 시험을 섞지 않는다. 접촉·시야·바람 판단은 사용자 수동 PIE를 우선한다.

## 5. 1-B1 검증 결과

- 2026-11-14, P4의 104파일 모두 존재하고 기록된 바이트 합계 **1,785,971,111**과 같다. 현재 104파일의 SHA256을 목록에 기록했고 기존 SHA256이 있던 14파일도 일치했다. 나머지 90파일은 이번에 처음 해시를 기록한 것이며 과거 내용 일치를 증명한 것은 아니다.
- 반입 대상과 같은 경로의 기존 파일은 **0개**다. 원본은 읽기만 했으며 실제 반입·재질/PCG 변경은 하지 않았다.
- 자동 검사 근거는 `Saved/Profiling/Graphics/Phase01/EnvironmentPreparation_20261114/Preflight.json`·`Validation.json`이다. git 제외·목록 완전성·보존 에셋 확인 결과는 Validation을 따른다.
- 다음은 **1-B2 진행 확인과 사용자 수동 Migrate**다. 그 뒤 로드·VT 실효값을 검증하며, 1-B3 구현까지 이어서 시작하지 않는다.

## 6. 1-B2 진행 상태(2026-11-14)

- 사용자 진행 승인 및 두 에디터 준비 확인 뒤 MCP로 8000=TestMap03, 8001=Startup·원본 104개 의존성 응답을 확인했다. 양쪽 UE 5.8.4, PIE=false다.
- 원본의 현재 전이 목록 104개가 식별 JSON과 일치한다. 원본 Startup과 대상 104패키지에 미저장 변경이 없다. 디스크 원본 104해시 일치·타깃 경로 충돌 0을 다시 확인했다.
- LootNPop TestMap03·정식 Ridge LVI/SurfaceData·OctantPoolData·기본 SpawnConfig도 dirty=false다. 정식 Ridge 구성과 설정 등 12파일의 반입 전 해시를 보존했다.
- **타깃 `r.VirtualTextures=1`**이다. 현재 세션에서 설정 변경 없이 확인했으며 활성화를 위한 재시작은 요청하지 않았다. 실제 재질/샘플러 로드 성공은 반입 후 확인한다.
- 원본 Content Browser에 14시작점을 선택했고 선택 목록을 재조회했다. 수동 Migrate를 사용자에게 요청한 상태이며 반입 완료·타깃 로드 검증은 아직 확인하지 않았다.
- 근거: `Saved/Profiling/Graphics/Phase01/EnvironmentImport_20261114/BeforeMigrate.json`·`ProtectedFilesBefore.json`.

### 반입 후 기술 대조·표시 확인 중단

- 사용자가 수동 Migrate 완료를 알렸다. 예상 104파일 모두 존재하며 추가/누락 파일은 0개다. 104경로 모두 git 제외, 추적된 원본은 0개다. 원본 104해시와 보호 12파일 해시는 보존됐다.
- 반입본은 104파일 모두 원본과 해시/크기가 달라졌다. UE 5.8 `AssetTools.cpp`의 `SaveInstancedPackagesIntoDestination`(1063행), `GEditor->Save`(1107행) 및 원본 에디터의 같은 성공 로그로 재직렬화 경로를 확인했다. 복사 실패로 단정하거나 원본을 파일 복사로 덮어쓰지 않았다. 반입본 해시는 `MigratedFiles.json`에 따로 기록했다.
- 타깃 104패키지 로드/클래스 일치·dirty=false, `/Game` 의존성 closure 104개·누락 0을 확인했다. 재저장 뒤 일부 직접 의존성 목록에는 함수/레이어의 전이 참조가 추가되고 일부 텍스처의 Interchange 스크립트 참조가 빠졌지만, 기존 `/Game` 의존성 삭제나 팔레트 밖 추가 패키지는 없다.
- 텍스처 66개·VT 65개, 기존 Format/Dimensions/SRGB/CompressionSettings/VT/SourceFormat 태그가 일치한다. 저장 형식인 SourceCompression은 66개가 PNG→UE Delta로 바뀌었다. 이를 픽셀 원본의 바이트 동일성 검증으로 확대하지 않는다.
- 선정 메시 6종은 양쪽 라이브 로드 뒤 bounds·LOD0 삼각형/정점·슬롯/MI·Nanite/fallback·DF/RT 설정·complexCollisionMesh가 일치했다. 원본/타깃 비교 결과와 O-3 원본 설정은 `AssetVerification.json`에 있다. 타깃 재질/로드 오류 로그는 관측되지 않았다.
- Migrate 뒤 원본 `get_asset_class`는 레지스트리 엔트리 부재로 실패했다. `is_dirty`도 104개 true였지만 이 도구는 레지스트리상 디스크 엔트리가 없어도 true를 반환한다(`asset.py`의 `_is_on_disk`·`is_dirty`). 실제 패키지 dirty 여부를 분리 확인한 것은 아니다. 원본 파일은 변하지 않았으며 저장/복원하지 않았다.
- 사암 MI의 첫 썸네일은 비동기 미리보기 컴파일 전 기본 재질이었고 재조회에서 정상 텍스처가 나타났다. 이어 다른 미리보기에서 **Video memory has been exhausted** 경고가 발생해 캡처를 중단했다. 두 에디터 고용량 에셋 로드 상태에서의 관측이며 패키지 성능 판정이 아니다. 사용자에게 Electric Dreams만 원본 저장 없이 종료해 달라고 요청했다.
- **1-B2는 아직 미완료다.** 메모리 확보 뒤 타깃 재질 표시/컴파일을 다시 확인한다. 현재 `Previews/`의 메모리 경고 포함 이미지는 최종 화질 통과 근거로 사용하지 않는다. 1-B3 재질 파생본은 만들지 않았다.

## 7. 1-B2 완료 결과와 다음 단위

사용자가 Electric Dreams를 종료했다고 확인한 뒤 LootNPop만으로 다시 검증했다. GPU 메모리는 종료 전 **7,652/8,188MiB**, 종료 직후 **5,567/8,188MiB**, 최종 **5,666/8,188MiB**였다. 같은 에디터 세션에서 설정 변경·재시작·빌드 없이 진행했다.

- **미리보기 8개 통과:** 메시용 MI 6개·지형 MI/부모 각 1개의 썸네일에서 텍스처·Masked 식생이 표시되고 VRAM 경고가 없었다. 컴파일 전 기본 썸네일과 메모리 부족 회차는 구분해 보존했다. 최종 이미지는 `PreviewsSingleEditor/`다. 썸네일 해상도/VT 스트리밍 특성까지 포함한 관찰이며 구면 지각의 근경 품질·노멀·반복 검증은 1-B3/4가 맡는다.
- **최종 라이브 검사:** 반입 104패키지와 TestMap03·정식 Ridge LVI/SurfaceData·풀·기본 SpawnConfig의 dirty 목록 0개, 재질/셰이더/텍스처/패키지 로드 오류 로그 0개, `r.VirtualTextures=1`이다. 썸네일 API의 에셋/재질 컴파일 완료 대기와 표시 결과를 함께 확인했다. cook/런타임 전체 셰이더 조합을 검증한 것은 아니다.
- **디스크 보존:** 원본 104파일·보호 12파일 해시가 반입 전과 같고 반입본 104파일도 로드/미리보기 전후 해시가 같다. 원본의 Migrate 후 레지스트리 조회 문제를 저장/복원으로 해결하지 않았다. 사용자가 원본 에디터를 종료했고 후속 파일 해시 검사는 통과했다.
- **반입/로드 완료:** 현재 조건부 팔레트 104파일은 모두 로컬 git 제외 경로에 있다. 원본 메시의 충돌/DF/RT/Nanite 설정을 최적화하지 않았다. 정식 Ridge 적용·파생 재질·PCG·Support/Nav 베이크·빌드/쿡·패키지 성능 측정은 이번 단위에 포함하지 않았다.

검증 원본은 `Saved/Profiling/Graphics/Phase01/EnvironmentImport_20261114/`의 `AfterMigrateFiles.json`, `MigratedFiles.json`, `AssetVerification.json`, `FinalFileChecks.json`, `FinalLiveChecks.json`, `PreviewsSingleEditor/`다. `AssetVerification.json`에는 6메시의 비교값과 원본 DF/RT/Nanite/fallback 설정도 있다. 컴포넌트 Lumen 기여/카드 설정 등 O-3 나머지 항목은 실제 적용 단계에 기록한다.

**1-B2 완료. 다음은 사용자 진행 확인 뒤 1-B3 지각 재질 시험이다.** WIP 사본과 파생 재질에서 구면 좌표·현지 Up 경사·대표 구역/외부 표면 마스크를 먼저 시험한다. 1-B0의 재질 방침과 보호 영역을 유지하며, 시험 결과에 따라 1-B3를 더 작은 구현 단위로 나눈다. 전체 1-B와 Phase 1은 미완료다.

## 8. 1-B3a 좌표·경사·구역 진단

사용자 진행 승인 뒤 1-B3를 작은 단위로 나눴다. 첫 단위는 원본 그래프의 텍스처 투영을 바꾸기 전 좌표/마스크 진단이다. 원본 지형 부모/MI 파생 방침을 바꾸는 것이 아니며 실제 텍스처/노멀 재사용은 다음 단위다.

- 재생성 코드: `Scripts/Profiling/Graphics/BuildRidgeSurfaceProbe.py`. LootNPop 콘솔에서 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceProbe.py"`로 진단 에셋만 생성한다. 기존 사용자 미저장 변경이나 동일 버전 산출물이 있으면 중단한다. 현재 5에셋은 이미 저장돼 있으므로 재실행하지 않는다.
- 새 에셋: `/Game/Graphics/Environment/ElectricDreams/RidgeProbe_V001/`의 `MF_RidgeSurfaceCoordinates`, `M_RidgeSurfaceProbe`, `MI_RidgeProbe_Scope`·`Slope`·`Grid`.
- 시험 레벨: `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe`. **에디터에서 정식 LVI를 연 뒤 File → Save Current Level As로 사본을 만든다.** 생성 스크립트는 레벨 복제/전환을 하지 않는다. 사본의 기존 지각 컴포넌트에만 재질 오버라이드를 바꾼다. 암반 Floor/Shell·지하·장치·Pod·검토점은 적용 대상이 아니다. 메시/태그/profile/transform은 보존한다. 이 사본의 런타임 SupportData 연결이나 PIE 통과를 전제하지 않는다.
- 좌표 함수는 `RidgeOctantSpec.point/local`의 역투영을 사용한다. `PositionLocal`·`NormalLocal`은 엔진 World→Local 변환을 거쳐 받고, 구면 Up은 `-normalize(PositionLocal)`이다. 출력 float4는 로컬 X(cm)·Y(cm)·법선과 Up의 내적·구역 마스크다. Z를 Up으로 쓰지 않는다.
- 구역은 1-B0 §8.2의 반시계 8각형 안쪽이며, canonical 성분 최소값 5,500cm 미만(이음매 1,500+전이 4,000cm)은 제외한다. 첫 단위의 외부/내부 구분은 **지각 컴포넌트에만 적용하는 소유권 제한**으로 구현한다. 암반 외부/지하 표면을 구분하는 범용 마스크까지 완성한 것은 아니다.
- 진단은 Unlit·변위/WPO 없음이다. Scope는 구역 초록/밖 회색, Slope는 구역 내 ≤30° 초록·30~70° 노랑·>70° 빨강, Grid는 접평면 X/Y의 1,000cm 격자다. 경사는 장식 검토용이며 보행 판정이 아니다. 격자 간격은 구면 호 길이가 아니다.
- Python 문법·경계 convex/반시계 8변·역투영 float32 48표본을 검사했다. 최대 X/Y 오차 0.001953125cm다. GPU 셰이더나 회전 slot 검증으로 확대하지 않는다. 근거는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceProbe_20261114/MathPreflight.json`·`ProtectedFilesBefore.json`이다.

### 첫 실행 실패와 수정

첫 수동 실행은 HLSL 예약어 `line`을 변수로 써 컴파일이 실패했고, 이어 scalar setter 반환값 assert에서 중단됐다. 레벨 복제/저장 전이며 생성 에셋은 메모리에만 있었다. `gridLine`으로 수정한 코드를 라이브 재질에 적용해 `MaterialTools.recompile` 성공을 확인했다.

별개로 UE 5.8 `MaterialEditingLibrary.cpp`의 `SetMaterialInstanceScalarParameterValue`(1485~1493행)는 값을 설정해도 `bResult=false`를 반환한다. 엔진은 수정하지 않고 재생성 코드에서 getter로 실제 값을 확인하도록 고쳤다. 라이브 `ProbeMode=0` 재조회도 통과했다.

첫 시도의 미완성 Scope MI·진단 재질·좌표 함수 3개만 삭제했고 모두 부재를 재조회했다. 정식/보호 12파일 해시는 유지됐다. 사용자에게 수정본 재실행을 요청했으며 **아직 최종 생성·WIP 표시·회전 검증 완료가 아니다.**

### 두 번째 실행 크래시와 복구 경로

수정본은 진단 5에셋을 저장한 뒤 `duplicate_asset(SOURCE_LEVEL, LEVEL)` → `load_level(LEVEL)`에서 **World Memory Leaks: 2 leaks objects and packages**로 종료됐다. 로그는 새 WIP World의 standalone/GC keep flags와 그 패키지가 맵 전환 시 제거되지 않았음을 가리킨다. Python 강한 참조 때문이라고까지 확인한 것은 아니다. 디스크에는 진단 에셋 5개가 있고 WIP umap은 없었다. 정식/보호 12파일 SHA256은 유지됐다.

생성 스크립트에서 World 복제·레벨 전환·적용 코드를 제거하고 진단 에셋만 생성하도록 줄였다. 같은 자동 맵 전환을 재시도하지 않는다. 사용자에게 LootNPop 재시작 → 원본 LVI 열기 → **Save Current Level As로 위 WIP 경로 저장**을 요청했다. 그 뒤 원본과 다른 현재 맵임을 확인하고 진단 MI를 적용한다. 현재는 이 수동 조작 완료를 기다린다.

크래시는 `Saved/Crashes/UECC-Windows-DD89C5FD48C9C88BBFFEE4A01FB351B4_0000`, 보존 사본은 `RidgeSurfaceProbe_20261114/Crash_20261114/`의 LootNPop.log·CrashContext.runtime-xml·Recovery.json이다(공통 상위 `Saved/Profiling/Graphics/Phase01/`). **1-B3a는 미완료이며 사용자에게 추가 생성 스크립트 재실행을 요청하지 않는다.**

## 9. 1-B3a 진단 완료와 후속 단위(2026-11-14)

사용자가 LootNPop을 재시작하고 정식 LVI를 **Save Current Level As**로 WIP 경로에 저장했다. 현재 맵의 WIP 경로·PIE=false·저장 상태를 확인한 뒤, 실제 메시 참조로 지각 컴포넌트 1개를 식별해 `MI_RidgeProbe_Slope`만 적용했다. 원본 레벨을 자동 복제/전환하는 코드는 다시 실행하지 않았다.

### 확인한 결과

- **구역/경사/격자 표시:** A에서 분지 바닥 초록·급경사 절벽 빨강·중간 띠 노랑을 확인했다. C의 Scope는 구역 안 초록·밖 회색이었다. Grid의 1,000cm 접평면 격자가 표면을 따라 표시됐다. 암반 Floor/Shell 재질 오버라이드는 원래 `M_Ridge_Rock`이다. 진단 Emissive가 주변 조명에 영향을 줄 수 있으므로 주변 색만으로 재질 교체 여부를 판정하지 않는다.
- **회전 진단:** WIP의 StaticMeshActor 3개와 카메라를 월드 Z축으로 +90° 함께 회전해 Grid와 Slope를 각각 캡처했다. 격자/구역 경계와 경사 색 분류가 같은 표면 위치에 유지됨을 육안 확인했다. 이는 에디터의 한 회전 사례이며 8개 런타임 slot 전체나 픽셀 단위 동일성 검증이 아니다. 최종에는 모든 액터 transform을 원래 identity 값으로 복원했다.
- **변경 범위:** 변경 전후 3메시 액터/컴포넌트 비교에서 차이는 지각 컴포넌트 `overrideMaterials` 1곳뿐이다. StaticMesh·태그·상대 transform·BodyInstance(충돌 profile/response 포함)와 액터 transform은 같다. 현재 Slope 모드·A 카메라로 저장했고 WIP dirty=false다. 카메라 위치/회전은 확정 JSON에서 가져왔지만 에디터 렌즈/화면비를 패키지 기준선과 동일하게 맞춘 검증은 아니다.
- **코드/셰이더:** 저장된 함수/표시 Custom 코드가 현재 재생성 스크립트와 일치한다. MI 모드 값은 Scope=0·Slope=1·Grid=2다. Emissive만 연결돼 있고 WPO·Displacement 출력은 미연결이다. 재시작 후 세션의 셰이더 컴파일/World Memory 오류는 0개다.
- **디스크 보존:** 보호 12파일과 반입 원본 104파일의 SHA256이 유지됐다. 진단 5파일·WIP umap은 git 제외 경로다. WIP 최종 SHA256은 `ED10DD7C46E3C9951B05B0A9855DE8F2C7EF66797D10F117233AB02F9B95CBA6`이다.

근거는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceProbe_20261114/`의 `Completion.json`, `WipBefore.json`·`WipAfter.json`, `FinalFileChecks.json`, `ExpectedCode.json`, `Views/`의 Scope_C·Slope_A·Grid_A 및 Grid/Slope_A_Rotated90 PNG다. 실패/크래시 자료와 성공 검증을 구분해 보존했다.

### 재현·다음 범위

1. 에셋이 없는 체크아웃에서 빌더로 진단 5에셋만 생성한다. 현재 산출물이 있으면 재실행하지 않는다.
2. 정식 LVI를 열고 Save Current Level As로 위 WIP 사본을 만든다. 맵 전환이 끝난 뒤 기존 지각 액터(`SM_Octant_Ridge_00_V011_R30000` 참조)의 Material Element 0만 진단 MI로 바꾸고 WIP만 저장한다. 현재 사본은 이 과정을 완료했다.
3. Scope/Slope/Grid MI로 표시를 비교한다. 현재 기본은 Slope다. 정식 레벨/메시/풀/SurfaceData는 수정하지 않는다.

**1-B3a 완료. 1-B3 전체는 미완료다.** Grid에서 급경사 접평면 투영의 늘어짐이 보이므로 이를 최종 절벽 UV로 채택하지 않는다. 다음 단위는 사용자 진행 확인 뒤 **1-B3b 원본 지형 그래프 파생본의 텍스처 투영·노멀 처리 시험**이다. 이 함수의 좌표/구역/경사 데이터를 재사용하되 절벽의 투영과 노멀을 별도로 검증한다. 근경 화질·경계 전이·암반 외부/내부 마스크·적도/모든 slot·이음매·PIE/SupportData·패키지 성능은 남아 있다.

## 10. 1-B3b 원본 파생·투영/노멀 시험 — 반복감 미통과(2026-11-14)

사용자 진행 승인 뒤 원본 `/Game/Landscape/M_BGLandscape_Auto`·`MI_BGLandscape_Auto_01`을 파생했다. 최종 재질 채택 전 기술 시험이며, 사용자 근경 확인에서 반복감이 미통과했다.

### 구현과 재현

- 재생성 코드: `Scripts/Profiling/Graphics/BuildRidgeSurfaceMaterial.py`. 콘솔에서 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceMaterial.py"`로 **에셋 2개만 생성**한다. 현재 산출물이 있으므로 재실행하지 않는다. 레벨 복제/전환/적용은 하지 않는다.
- 산출물: `/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V001/`의 `M_Ridge_Surface`·`MI_Ridge_Surface`. 진단 좌표 함수를 선행 생성한 뒤 원본 부모 137노드와 MI를 복제한다. 원본의 색/채도·거칠기 조절과 Substrate Slab을 재사용하고, 나머지 원본 레이어 가지는 파생 에셋에 남되 Slab 입력에서는 분리했다. 의존성/셰이더 비용 제거를 증명한 것은 아니다.
- BeachCliff·MossyRockyGround의 BaseColor/Normal/AoRDp를 사용한다. 거칠기는 원본 ground 배선에서 확인한 G를 쓴다. 절벽 2,000cm, 지면 400cm는 첫 시험값이다(`Mud Tiling` 파라미터를 지면 cm 크기로 재사용). 경사는 현지 Up의 30~70° 구간으로 블렌드한다.
- 엔진 `WorldAlignedTexture`의 XYZ 출력과 `WorldAlignedNormals_HighQuality`를 재사용하되 Position/Normal에는 **옥탄트 로컬 좌표/법선**을 명시한다. 노멀 함수의 위치 divisor는 음수 타일 크기다. 투영 노멀은 Local→World 변환하고 기하 월드 노멀과 강도/레이어 블렌드한다. 최종 `bTangentSpaceNormal=false`다. WPO 미연결·테셀레이션 꺼짐을 확인했다.
- 대표 구역 밖은 기존 단색의 실제 선형색·기하 월드 노멀·기본 거칠기/F0/F90를 연결했다. 구역 마스크 경계는 아직 단단하다. 구역 밖 전체 Lit 픽셀/BRDF 동일성 검증은 아니다.
- 사용자 생성 실행 뒤 WIP 지각의 Material Element 0에 새 MI만 적용했다. 재현 시 §9의 수동 Save As가 끝난 사본에서 적용한다. 정식 LVI·지각 메시·Floor/Shell 재질·Support source는 유지한다.

### 노멀 수정과 검증 범위

- 초기 Local→World→Tangent 경로는 중립 노멀 비교에서 실패했다. WorldNormal 표시의 내부 29,417표본(stride 4, 경계 주변 제외)에서 최대 채널 오차 평균 1.6327·p95=7·최대109(RGB8)였다. 실패 자료도 보존했다.
- 최종은 월드 출력과 같은 공간의 기하 노멀 블렌드로 바꾸고 불필요한 두 World→Tangent 노드를 제거했다. 중립 텍스처로 재검사한 결과 같은 표본의 평균 **0.4330·p95=1·최대2**로 기준 p95≤2를 통과했다. 이는 **WorldNormal 표시 RGB 차이**이며 각도 오차나 전체 프레임 동일성은 아니다. 보간/TBN이 단독 원인이었다고 확정하지 않는다. 검사 후 실제 두 노멀 텍스처를 복원했다.
- 사용자 수동 Unlit→WorldNormal→Lit 전환으로 A/B를 캡처했다. WIP 3메시·점광원·카메라를 월드 Z +90° 회전한 한 사례에서 무늬/노멀 반응이 표면에 유지됨을 육안 확인하고 원래 transform으로 복원했다. 전체 8개 런타임 slot 검증은 아니다.
- 독립 LVI 재질 확인용 점광원 `GraphicsPreview_SurfaceLight` 1개는 **WIP·에디터 전용**이다. Movable·100,000cd·반경30,000cm·그림자 꺼짐, A 카메라에서 구면 현지 Up으로 2,000cm 떨어진 위치다. 게임 조명/성능의 통과 근거로 쓰지 않는다. B의 큰 삼각형 음영은 같은 광원의 기존 단색 재질에서도 보여 기존 지각 후속 항목으로 남겼다.
- 저장한 파생 부모/MI·WIP dirty=false, 최종 재질/셰이더 컴파일 및 World Memory 오류 필터 결과는 빈 목록이다. 3메시 액터/컴포넌트의 차이는 지각 overrideMaterials뿐이며 메시/태그/transform/BodyInstance는 보존했다(추가 광원은 별도). 보호 12파일·반입 104파일 해시도 그대로다.

### 사용자 화질 판정과 다음 단위

사용자는 근경에서 **늘어짐이나 눈에 띄는 어색함은 없지만, 누가 봐도 타일이라고 느낄 정도로 규칙적이고 기계적인 반복이 아주 명확하다**고 확인했다. 따라서 투영/노멀 기술 검증은 통과했으나 **반복감은 미통과**다. 최종 재질·팔레트 채택이나 1-B3/Phase 1 완료로 처리하지 않는다.

다음 제안은 **1-B3c 반복 완화**다. 무늬의 배치/방향을 불규칙하게 만드는 후보를 검토하고 노멀 정합·이음새·추가 샘플 비용을 함께 확인한다. 타일 크기 조정만으로 해결 판정을 하지 않는다. 아직 구현하지 않았고 사용자 진행 확인을 받는다. 경계 전이·기존 지각 음영·전체 slot·PIE/SupportData·패키지 비용은 후속 항목이다.

근거는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceMaterial_20261114/`의 `Verification.json`, `WipBefore.json`·`WipAfter.json`, `FinalFileChecks.json`, 실패 `NeutralNormalComparison.json`·수정 후 `NeutralNormalWorldComparison.json`, `Views/`의 10개 PNG다. `Creation.json`은 최초 생성 시점 기록이며 최종 월드 노멀 보정은 위 설명과 현재 코드가 기준이다. 재질/MI·WIP·원자료는 git 제외다.

## 11. 1-B3c 반복 완화 시험 진행 중(2026-11-14)

사용자가 다음 단위 진행을 승인했다. 엔진 `Texture_Bombing`의 위치/방향 변형과 노멀 보정 기능을 기존 구면 투영에 연결하는 방식을 시험한다. `BuildRidgeSurfaceAntitile.py`는 V001을 보존하고 별도 `/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V002/`에 투영 함수 사본 2개·부모/MI 2개를 생성한다. 높이 블렌드는 끄고 세 텍스처 종류에 같은 배치를 사용하며, 노멀맵 보정은 노멀 경로에 켠다. 맵 전환/지각 적용은 하지 않는다. 기존 엔진 함수·반입 원본은 수정하지 않는다.

PIE=false·현재 WIP 경로·V001 부모/MI와 WIP dirty=false·지각 V001 바인딩을 확인했다. 사용자가 잡은 근경과 A/B의 기존 Lit 표시 3개를 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceAntitile_20261114/Views/Before_*.png`에 보존했다. 첫 실행은 함수 복제 직후 Material 전용 `get_material_expressions`에 MaterialFunction을 전달해 중단됐다. 공용 `MaterialTools.get_expressions`로 수정했고 미완성 함수 1개만 삭제·부재 재조회했다. WIP 적용 전이다. 사용자에게 수정본 실행을 요청했으며, 아직 최종 생성/컴파일·반복감/노멀·샘플 비용 통과가 아니다.

### 생성·초기 비교와 바닥 보완

수정본 수동 실행으로 4에셋을 생성했고 MCP 재컴파일도 성공했다. 원본 투영의 3축 샘플을 각 Texture_Bombing 호출로 바꾼 함수 2개를 부모의 색/마스크 4곳·노멀 2곳에서 사용한다. Height Lerp=false·노멀 경로 Is Normalmap=true를 배선으로 재조회했다. WIP 지각에 적용하고 기존 근경/A/B의 `After_*.png`를 보존했다.

| MI 셰이더 통계(초기 V002, 바닥 보완 전) | V001 | V002 |
|:---|---:|---:|
| 최대 픽셀 셰이더 명령 수 | 1,565 | 5,270 |
| 샘플러 수 | 2 | 7 |
| 픽셀 텍스처 샘플 추정 | 21 | 75 |
| VT 조회 추정 | 18 | 72 |

정점 명령은 둘 다 148·정점 텍스처 샘플은 0이다. `MaterialEditingLibrary.get_statistics`의 현재 에디터 플랫폼 통계이며 패키지 GPU 시간/배수로 해석하지 않는다. 비용 증가가 커 현재 후보를 성능 적합으로 채택하지 않는다.

사용자는 **절벽은 수용 가능한 수준까지 반복이 줄었으나 바닥은 여전히 규칙적 타일 느낌이 남는다**고 확인했다. 바닥은 미통과로 남겼다. 같은 단위에서 바닥만 로컬 위치 기반의 부드러운 3D 값 노이즈 두 크기로 색/거칠기를 변조했다. `Ground Macro Size=1800cm`·`Ground Macro Strength=0.5`, 보조 크기는 약48.6m다. UV/노멀·절벽 경로는 유지하고 추가 텍스처 조회는 없다. 반복 무늬 재배치 위에 더하는 보완이며 이 변조만으로 반복 무늬를 제거했다고 보지 않는다. 코드/배선·컴파일 확인 뒤 사용자에게 바닥 재판정과 World Normal 전환을 요청했다. 아직 그 결과와 최종 중립 노멀·비용 검증은 대기 중이다.

바닥 보완은 현재 빌더의 `add_ground_macro`에 반영했고 저장된 Custom 코드 일치를 `MacroVerification.json`에 기록했다. `Creation.json`·위 표는 보완 전 기록이며 최종 통계와 구분한다. `Macro_User.png`는 보완 직후 캡처다. MCP에서 Custom 입력 배열을 늘릴 때 기본 입력 전체 구조를 유지해 크기를 늘린 뒤 이름을 바꾸는 두 단계가 필요했다. 배열 변경 거부는 저장 전 수정했고 맵 전환/원본 수정은 없었다.

## 12. 1-B3c 기술 검증 마감과 조건부 인계(2026-11-14)

### 사용자 판정·유효 범위

사용자는 바닥 보완 후에도 **바닥과 벽면에 잔여 반복이 보이며, 후속 프랍/식생이 더해지면 덜 느껴질 수 있다**고 답했다. 이를 최종 화질 승인으로 해석하지 않는다. V002는 후속 소수 배치에서 장면 맥락을 평가할 **조건부 후보**다. 통로·Pod·훅/회복 공간은 보존하므로 식생으로 지각 전체를 덮는 방침은 아니다. 재질 반복 최종 판정은 같은 구도의 소품/식생 포함 장면에서 다시 받는다.

### 중립 노멀 비교와 AA 진단

- V002 실제 노멀·기존 단색 기하 노멀·Placeholder_Normal을 쓴 중립 노멀을 같은 A 구도에서 비교했다. 처음의 넓은 색 분류 마스크에는 구역 밖 간접광이 포함돼, 명확한 진단 빨강/초록/노랑과 4픽셀 이웃으로 내부를 한정했다. 실패 자료도 보존했다.
- TSR 켜짐에서는 중립 비교 p95=4·최대98이 나왔다. V001의 임시 부모/MI 사본은 같은 조건 p95=1·최대2였다. 이후 변경 없는 V002 중립 재캡처에서도 최대109, 기하 노멀끼리도 최대98이 나타나 캡처 프레임/AA 영향을 분리했다. 단순히 허용 오차를 넓혀 통과시키지 않았다.
- 사용자에게 콘솔의 `r.AntiAliasingMethod=4`를 기록한 뒤 잠시 0을 요청했고 실효값을 재조회했다. **AA 끄기 비교는 내부 29,043표본·stride4에서 평균0.3801·p95=1·최대2(RGB8)**로 통과했다. 절벽13,904·바닥10,168·전이4,971표본 각각도 p95=1이며 최대1/2/1이다. 표시 RGB 차이이며 법선 각도 오차·전체 프레임 동일성은 아니다.
- 실제 두 노멀 텍스처를 복원·컴파일하고 V002 바인딩을 재조회했다. 임시 V001 진단 사본 2개는 삭제/부재 확인했다. 원본 V001을 수정/저장하지 않았다. 사용자가 AA=4·Lit로 복귀했고 실효값과 Lit 캡처를 확인했다. 같은 실효값 복구이며 콘솔 변수의 원래 우선순위까지 복원됐는지는 확인하지 않았다. 프로젝트 INI 변경은 없다.
- 숫자 비교의 첫 PowerShell 읽기는 Bitmap에 PathInfo를 넘겨 표본을 읽지 못했고 다음 시도는 예약 변수 `$Error` 대입에서 중단됐다. 최종 분석은 문자열 경로·별도 표본 변수·0표본 중단 조건을 사용했다. 무효 읽기 기록을 성공 근거에서 제외했다.

### 최종 비용·회전·보존

| MI 셰이더 통계(바닥 보완 포함 최종) | V001 | V002 |
|:---|---:|---:|
| 최대 픽셀 셰이더 명령 수 | 1,565 | **5,367** |
| 샘플러 수 | 2 | 7 |
| 픽셀 텍스처 샘플 추정 | 21 | 75 |
| VT 조회 추정 | 18 | **72** |

정점 명령148·정점 샘플0은 같다. 바닥 보완 전 대비 명령은97 늘었고 텍스처/VT 조회는 늘지 않았다. 현재 에디터 플랫폼의 `MaterialEditingLibrary.get_statistics` 값으로 GPU ms·패키지 비용을 판정하지 않는다. 비용이 커 **대표 구역 PCG/전체 확장 전 샘플 비용 축소가 필요하며 성능 채택은 보류**다.

- 파생 함수2/부모/MI·WIP를 명시 저장했고 5패키지 dirty=false다. 부모는 월드 노멀 출력·테셀레이션 꺼짐, WPO는 기존 미연결을 유지한다. Bomb Offset=1·Ground Macro Size=1800·Strength=0.5를 getter로 확인했다.
- 3메시·검사용 점광원·A 카메라를 월드 Z +90°로 함께 회전해 Lit 한 사례에서 무늬가 표면에 유지됨을 확인했다. 액터/광원 transform은 finally로 복원·재조회했다. 조명/GI/노출 이력에 따른 밝기 차이를 픽셀 불변으로 해석하지 않으며 전체 런타임 slot 검증은 아니다.
- 최종 기존 3메시 비교는 지각 `overrideMaterials` 한 곳만 V001→V002로 달라졌다. StaticMesh·태그·BodyInstance/충돌·transform은 같다. 기존 검사용 점광원은 editor-only를 유지한다. 보호12·반입104·V001 부모/MI 해시가 유지됐고 V002 4파일은 git 제외다. 관련 셰이더 컴파일/World Memory 오류 필터는 빈 목록이다. 개별 조회/배열 조작 오류까지 없었다는 뜻은 아니다.

### 재현·다음 단위

깨끗한 체크아웃에서는 원본 반입→진단 빌더→V001 빌더→`BuildRidgeSurfaceAntitile.py` 순으로 에셋을 만든다. 현재는 이미 생성됐으므로 재실행하지 않는다. 수동 Save As가 끝난 WIP 지각 Element0에 V002 MI만 적용한다. 현재 빌더에는 라이브 바닥 보완과 동일한 코드/배선을 반영했지만, 최종 보완 후 새 경로에서 전체 재생성을 다시 수행하지는 않았다. 맵 복제/로드는 빌더가 하지 않는다.

**1-B3c 구현/기술 검증은 마감, 최종 재질 채택과 1-B3/1-B 전체는 미완료다.** 다음 제안은 사용자 진행 확인 뒤 **1-B4a 낮은 사암·하상·클로버 소수 배치 시험**이다. 보호 영역/접지/노출과 장면 속 반복감을 함께 확인한다. 절벽 보강·Fern 바람/변위는 별도 소수 시험으로 남긴다. 경계 전이·지각 음영·샘플 비용 축소·전체 slot·PIE/SupportData·패키지 검증도 남아 있다.

원자료는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceAntitile_20261114/`다. 최종은 `Verification.json`·`WipBefore/After.json`·`ComponentComparison.json`·`FinalFileChecks.json`·`FinalStatistics.json`·`NeutralNormalNoAAComparison.json`·`MacroVerification.json`, 캡처는 `Views/` 19개다. TSR 켜짐/반복 캡처/마스크 초기 실패와 `FirstRunError.json`은 구분해 보존했다. `CompareNeutral.ps1`·`CaptureStats.py`는 Saved 분석 보조 자료이며 통계 API/분석 조건은 이 절이 소유한다.

## 13. 1-B4a 낮은 사암·하상·클로버 소수 시험(2026-11-15, 근경 시험 마감)

사용자가 진행을 승인했다. 에디터는 TestMap03·PIE=false여서 기존 WIP를 수동으로 열어 달라고 요청했고, 사용자가 완료했다. 새 맵 복제/로드·원본 변경·PCG·Support/Nav 베이크는 하지 않았다.

- 코드: `Scripts/Profiling/Graphics/BuildRidgeGroundProbe.py`. 기존 WIP에서만 실행하며, 현재는 이미 배치돼 재실행하지 않는다. 미저장 변경/기존 시험 라벨/원래3메시 identity pose를 검사한 뒤 계획을 검증한다. 최초 실행은10개, 최종 코드는 통과한8개를 생성·선택하고 WIP만 저장한다.
- 최초 개별 배치: 사암2·하상2·클로버6. 초기10개 값은 `Plan.json`·`Placement.json`, 최종 통과8개 spec XY/회전/스케일은 코드 `POINTS`가 소유한다. 사암은 Pod 3D 편성 구 밖, 낮은 하상/클로버만 Pod 구 안에 허용했다. 원본 메시/MI·Nanite·충돌/DF 설정은 수정하지 않았다.
- 실제 지각을 향해 현지 Up을 따라 ±500cm complex Visibility trace를 하고 hit location/normal로 Z축을 정렬했다. 첫 시도는 사암 두 지점의 spec/실제 지각 차이62.82/39.98cm가 임의의30cm 판정을 넘어 생성 전에 중단됐다. 기준을 실제 hit component가 기존 지각인지 확인하는 방식으로 바꿨고 수정본은 전부 통과했다. spec은 근사 계획용이며 실제 접지를 대신하지 않는다. 첫 계획은 `Plan_FirstGuardStop.json`에 보존했다.
- 보호 가드: 구역/이음매, 암반 대체 footprint, 경로 반폭+2m 초안과 bounds 반경×1.2, Pod 3D 구(사암), 앵커 착지·입구 이격, 실제 경사<30°다. 경로 초안 여유 바깥의 추가 최소 이격112.60cm, 사암의 Pod 구/bounds 추가 최소 이격169.27cm, 실제 최대 경사16.36°였다. bounds→spec 계수1.2·2m는 시험용 보수 가드이며 최종 PCG 마스크 규약이 아니다. 전체 훅 시야/접근·회복 공간을 검증한 것은 아니다.
- 새 컴포넌트10개는 `LNP.Surface.Decoration` 단독·`LNPDecoration`/NoCollision·`bCanEverAffectNavigation=false`다. 원본 3메시의 mesh/material/transform/태그/BodyInstance·렌더 플래그 스냅샷은 배치 전후 같았다. Outliner 폴더는 `Graphics/GroundProbe_V001`, 라벨은 `GraphicsGroundProbe_*`다. 적용 후 WIP dirty=false다.
- O-3 컴포넌트 기록: 새10개 모두 CastShadow/VisibleInRayTracing/AffectDistanceFieldLighting=true를 상속했다. 최적화하지 않았으며 실제 Lumen 기여/패키지 비용은 별도다. 원본 MI는 기본 슬롯에서 유지한다. 소수 표본으로 장면 전체 밀도나 반복감 최종 판정을 하지 않는다.

자료는 `Saved/Profiling/Graphics/Phase01/RidgeGroundProbe_20261115/`의 `PreflightFiles.json`(보호12·반입104 해시 변화0), `BeforePlacement.umap`, `WipBefore.json`, `Plan.json`, `Placement.json`, `TechnicalVerification.json`, `Views/Before_A.png`다. 저장 전 사본은 읽기용 복구 자료이며 에디터에서 자동 전환하지 않는다. 자동 포커스가 멀어 접지를 판정할 수 없었으므로 사용자에게 F/직접 근접 관찰로 사암 뜸/매몰·하상 테두리/색·클로버 부유/판 모양을 확인해 달라고 요청했다. **이 시점에는 수동 판정 대기였으며, 최종 결과는 아래 절을 따른다.** PIE 발/카메라/탄환·런타임 SurfaceData·패키지 성능도 아직 검증하지 않았다.

### 최종 근경 판정·실패 역할 제외

위 내용은 최초10개 배치 직후 기록이다. 이후 사용자가 가까이에서 **전체적으로 들뜨거나 이질감이 없으며, 근경에서는 지각 반복도 거슬리지 않는다**고 확인했다. 사암과 하상 이름을 구분한 뒤 **Sandstone은 매우 자연스럽고, Riverbed는 표면 자체는 자연스럽지만 외곽이 너무 네모라 주변과 어울리지 않는다**고 명확히 답했다.

- **Sandstone 2개·Clover 6곳은 근경 시험 통과.** 현재 WIP에 유지한다. 원거리·밀도·PIE 발/카메라/탄환·런타임 SurfaceData·패키지 비용까지 통과한 최종 팔레트는 아니다.
- **DryRiverBed_02는 현재 팔레트에서 제외.** 직접 만든 시험 액터2개(`GraphicsGroundProbe_Riverbed_02/03`)만 제거·저장하고 부재/최종8개를 확인했다. 원본104패키지·원래 하상 메시/MI와 의존성 목록은 보존했다. 네모 외곽을 숨길 수 있다고 가정해 채택하거나 원본 메시를 수정하지 않았다.
- `BuildRidgeGroundProbe.py`는 최종8개와 기존 고정 ID를 재현하도록 맞췄다. 이미 배치돼 있어 재실행하지 않았다. 첫10개 `Placement.json`·`Plan.json`과 `WipAfter_Initial10.json`·`Initial10Images.json`은 그대로 보존했다. 최종8개는 `WipAfter.json`, 판정은 `Verification.json`이다.
- 마지막 비교에서 원래3메시의 transform·전체 기존 스냅샷 필드 차이0, 보호12/반입104/V002 재질4파일 해시 변화0이다. 최종8개 모두 Decoration 단독·NoCollision/LNPDecoration·Static·내비 영향 꺼짐을 재조회했다. WIP는 저장/dirty=false, 배치 전 백업은 당시 WIP 해시와 일치한다.
- 최종 캡처는 같은 A와 사용자 근경이며 `Views/Final_A.png`·`Final_Close.png`다. 초기10개/근경 포함 총6개 PNG를 보존했다. 점광원 검사 구도이며 게임 조명 완성도/멀리서의 반복감 최종 판정은 아니다.

**1-B4a의 배치/기술 확인·사용자 근경 시험을 마감했다.** 다음 제안은 진행 확인 뒤 **1-B4b 이끼 바위·절벽 부분 보강의 소수 시험**이다. Fern은 별도 바람/변위 시험으로 남긴다. 실패한 하상 역할의 재도입/다른 패치 선정은 별도 확인 대상으로 두며, 대표 구역 PCG 전 조건부 재질의 비용/화질과 실제 플레이 접촉 검증이 남아 있다.
