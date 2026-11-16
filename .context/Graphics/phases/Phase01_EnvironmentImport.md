# Phase 1-B — 환경 반입과 작은 시험

> 상태: 1-B5a 대표 구역 PCG 소수 배치 마감 · 1-B5b 바위 Blocker 승격·재베이크·1인 PIE 마감(지상 적 발 관통은 SSN 인계)
> 작성: 2026-11-14
> 마지막 갱신: 2026-11-16
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
| 1-B5b 바위 충돌 승격 | 바위 역할만 Static+Blocker 지형 계약으로 승격(§24, GX-014) | 별도 GX 결정·SurfaceData 재베이크·발/탄환/적 이동/카메라 일치·동선 재측정 영향 확인 |
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

위 폴더는 이번 원본용으로 사용한다. 자체 제작 에셋은 원본 제외 폴더에 넣지 않는다. 실제 재생성 코드/설정과 수동 보정은 §9~20, 새 세션의 읽기 순서와 마지막 로컬 상태는 [Current](../Current.md)를 따른다. V005는 사용자 저장을 마쳤다. 1-B3g 임시 회전/진단은 복원·저장까지 끝났으며 상세 근거는 §20을 따른다. 사암2/클로버6/이끼2/절벽1·구면 보정 MI를 적용한 Fern1개는 유지한다. 지각은 비용 축소/중립 노멀 통과·패키지 최종 채택 보류, Fern은 근경/+90° 회전 사례 통과·조건부 유지다. 1-B5a에서 대표 구역 PCG 소수 배치(장식20개)를 WIP에 저장했다(§23). 정식 LVI 적용은 시험 채택 뒤 별도 단위에서 한다.

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

## 14. 1-B4b 이끼 바위·절벽 부분 보강 소수 시험(2026-11-15, 근경 시험 마감)

사용자가 이 단위 진행을 요청했다. 현재 맵이 TestMap03·PIE=false여서 기존 WIP 열기를 수동 요청했고, 사용자가 완료했다. WIP 경로·dirty=false·최종 1-B4a 맵 해시 일치를 확인했다. 새 맵 복제/로드·정식 Ridge 변경·PCG·Support/Nav 베이크·빌드/쿡은 하지 않는다.

- 재생성 코드: `Scripts/Profiling/Graphics/BuildRidgeRockProbe.py`. 기존 WIP·원래 지각 identity pose·V002 바인딩·기존 메시11개·dirty 패키지 없음·같은 시험 라벨 부재를 요구한다. 이끼 바위는 원본 MI09, 절벽은 MI_Cliff_01을 기본 슬롯에서 확인한다. 원본 메시/재질을 편집하지 않는다.
- 범위: 이끼 바위2개(스케일0.8)·절벽1개(스케일0.15, 높이 약2.13m). 실제 complex Visibility trace의 지각 component와 normal을 사용한다. 이끼는 현지 법선에 정렬·3cm 매몰, 절벽은 로컬+Y를 법선·Z를 접선 Up에 정렬하고 AABB 앞면 중심을 지각보다12cm 노출한다. 실제 메시 면의 노출/접촉을 AABB만으로 통과시키지 않는다.
- 생성 전 가드: 구역·이음매·암반 footprint·경로 반폭+2m 및 hit 중심 bounds 반경×1.2·Pod 3D 편성 구·입구/앵커 착지 이격. 이끼 실제 경사<30°, 절벽70~90°이며 앞면 AABB 9점의 실제 지각 hit와 간격 -10~30cm를 검사한다. 이는 소수 시험용 가드이며 최종 PCG/훅 접근/회복 공간 검증이 아니다.
- 첫 수동 실행은 이끼 후보(3100,-2660)/(3700,-2680)의 실제 경사33.34°/48.56° 때문에 생성 전에 중단됐다. 액터 생성/맵 저장은 없었다. 절벽 후보(0,0)는 실제 경사79.96°·앞면9점 간격5.40~12.00cm로 가드를 통과했지만 실제 메시 노출은 미검증이다. 첫 계획은 `Plan_FirstSlopeStop.json`에 보존했다.
- 기준을 완화하지 않고 인근8점을 실제 trace로 조회한 로컬 보조 `SurveyAndBuild.py`를 사용자가 실행해 배치/저장에 성공했다. 선택된 이끼 좌표(2650,-2750)/(2850,-2760)는 실제 경사12.82°/16.24°·Pod 구/bounds 밖 여유260.40/220.59cm·경로 초안/bounds 밖 여유50.80/250.81cm다. 절벽까지 전체 가드를 통과했다. 최종 고정 좌표를 추적되는 빌더 `POINTS`에 반영해 Saved 보조 의존성을 없앴고 Python AST를 확인했다. 이미 배치돼 있어 최종 빌더를 재실행하지 않는다.
- 라이브3개 모두 Decoration 단독·LNPDecoration/NoCollision·Static·내비 영향 꺼짐이다. 기존11개와 새3개의 저장 직후 컴포넌트 필드 대조도 차이0이다. 기존11메시 배치 전후 스냅샷과 보호12·반입104·V0024파일 해시는 그대로이며 복구 사본 해시는 시작 WIP와 일치한다. WIP dirty=false·배치 성공 로그를 확인했다. 신규3개 CastShadow/VisibleInRayTracing/AffectDistanceFieldLighting=true 기본값을 유지하며 비용 적합으로 판정하지 않는다.
- 사용자에게 개별 액터 선택/F로 이끼의 뜸·사암과의 겹침·이끼/색 이질감과 절벽의 실제 노출·과도한 매몰·네모 테두리·틈/떠 있는 옆면 확인을 요청했다. 사용자는 **전체적으로 자연스럽고, Moss_00은 기존 사암과 겹치지만 이끼 부분이 연결돼 자연스러우며 지면에 떠 있다고 느껴지지 않고, Cliff_02도 아주 자연스럽다**고 확인했다. 이끼2/절벽1은 **수동 근경 시험 통과·조건부 유지**다. 겹침을 없애기 위한 이동은 하지 않았다. AABB 검사는 배치 가드이며 이 육안 판정을 대신하는 근거로 쓰지 않는다.

근거는 `Saved/Profiling/Graphics/Phase01/RidgeRockProbe_20261115/`다. 시작 `FilesBefore.json`은 보호12·반입104·V002 재질4파일, `PreflightFiles.json`은 WIP 해시를 기록한다. `BeforePlacement.umap` 복구 사본·기존11메시 스냅샷·계획/배치 결과·`TechnicalVerification.json`·`FinalFileChecks.json`·최종 사용자 판정 `Verification.json`을 남겼다. 관찰 중 캡처 `Views/User_Current.png`는 이끼/기존 장식 구도이며 절벽 개별 캡처를 대신하지 않는다. 종료 시 기존 WIP·PIE=false·dirty=false를 재확인했다.

**1-B4b의 배치/기술 확인·사용자 근경 시험을 마감했다.** 이끼2·절벽1을 유지하며 추가 편집/저장은 필요하지 않았다. 전체 밀도·원거리 반복·PIE 발/벽보행/카메라/탄환, SurfaceData 런타임 로드·패키지 비용은 별도 미검증이다. 다음 제안은 진행 확인 뒤 **1-B4c Fern 구면 바람/WPO·변위 소수 시험**이며 아직 착수하지 않았다. 대표 구역 PCG/전체 확장 전 재질 샘플 비용 축소·최종 화질 검증도 남아 있다.

## 15. 1-B4c Fern 구면 바람/WPO 소수 시험(2026-11-15, 원본 근경 시험 마감)

사용자가1-B4b 마감 뒤 이 단위 진행을 승인했다. 기존 WIP·PIE=false·dirty=false를 확인했다. 원본 메시/MI·공유 함수/MPC와 기존14메시를 보존하고 원본 Fern 소수 표본부터 확인한다. 새 정식 LVI 적용·PCG·베이크·PIE 장치 구현·빌드/쿡은 범위에 넣지 않는다.

### 원본 바람 경로와 검증 기준

- 원본 `/Game/Megascans/3D_Plants/Fern/SM_Fern_01`은 로컬 AABB 약156.25×187.99×103.21cm, 기본 MI는 `MI_Fern_01`이다. 부모는 `M_MS_CustomFoliage`다. Animate Foliage/Plant/Use Vertex RED for Animation/optimize WPO=true, Use Procedural Gradient/Invert Gradient=false를 getter로 확인했다. MI 바람 Noise/Gust/Gust Noise는0.2/0.4/0.3이다.
- 부모의 일반 렌더 WPO는 `MF_SimpleWind`의 WPO Tree (V3) 출력, RayTraced 경로는 별도 상수다. Plant=true 분기에서 엔진 `SimpleGrassWind`와 정점 R 가중치를 사용한다. 이 함수는 CrossProduct의 Up에(0,0,1), 피벗 보정에(0,0,-10)을 쓰고 WorldPosition으로 위상을 계산한다. 조회한 함수 내부에는 현지 구면 Up/접선 변환이 없다. **이 구조만으로 임의 slot 회전에 공변하는 구면 바람이라고 판정할 수 없다.** 원본에서 자연스러운 한 표본과 전체 구면 방향 적합을 구분한다.
- 원본 MI/함수/MPC를 수정하지 않는다. 먼저 실제 지각 hit·보호 공간 밖 원본 표본에서 접지/움직임/카드 노출·뿌리 이동을 확인한다. 필요하면 컴포넌트의 WPO 평가를 끈 진단 표본과 비교한다. 가시적인 문제가 없더라도 고정 월드축 가정을 전체 구면 통과로 확대하지 않는다.
- 읽기 전용 `InspectFern.py`를 사용자가 실행했다. 실제 후보20점 중(3000,-2700)1점만 전체 가드를 통과했다. 라이브 MPC는 Direction0·Noise5·Strength Plants2·Speed Plants0.3·Tiling Plants0.5이며 변경하지 않았다. WPO용30cm 여유는 시험 초안이며 실제 최대 변위의 측정값이 아니다.

### 배치·기술 확인·사용자 판정

- 재생성 코드 `Scripts/Profiling/Graphics/BuildRidgeFernProbe.py`를 사용자가 실행해 원본1개를 생성·WIP만 저장했다. 1-B4a/b가 있는 기존 WIP·14메시·지각 identity pose/V002 바인딩·dirty 패키지 없음·동일 라벨 부재를 요구한다. 현재 산출물이 있으므로 재실행하지 않는다. Saved 조사 자료 없이 고정 좌표로 재생성하며 Python AST를 통과했다.
- 라벨은 `GraphicsFernProbe_Original_00`, 폴더는 `Graphics/FernProbe_V001`이다. 실제 지각 normal에 정렬·회전67°·스케일1·1.5cm 매몰이다. 실제 경사16.47°·메시 전체 bounds와 시험 WPO 여유30cm를 더한 반경196.04cm·경로 초안/bounds 밖 여유267.53cm·Pod3D 구/bounds 밖 여유21.06cm로 가드를 통과했다. 2m 경로 여유·bounds→spec 계수1.2와 WPO30cm는 소수 시험용이며 최종 PCG/실제 최대 변위 규약이 아니다.
- 원본 메시/MI·WPO 평가true·거리 제한0, Decoration 단독·LNPDecoration/NoCollision·Static·내비 영향false를 라이브 재조회했다. CastShadow/VisibleInRayTracing/AffectDistanceFieldLighting=true·WPO의 RayTracing 평가false·boundsScale1 기본값을 기록했다. 실제 Lumen/RT 표현이나 비용 적합으로 해석하지 않는다.
- 기존14메시 스냅샷과 저장 직후15컴포넌트 라이브 필드 대조는 차이0, 보호12/반입104/V0024파일 해시는 유지됐다. 배치 전 복구 사본 해시는 시작 WIP와 일치하며 최종 WIP dirty=false·PIE=false를 확인했다.
- Realtime 근경 관찰을 요청했고 사용자는 **잎이 자연스럽게 움직이고 그 외에도 어색한 부분이 없다**고 확인했다. **원본 Fern1개는 근경 동작/외관 시험 통과·조건부 유지**다. 문제가 관찰되지 않아 WPO 끄기 비교나 바람 값/원본 수정은 수행하지 않았다. 실제 정점별 최대 변위·뿌리 이동량을 수치 측정한 것은 아니다.

**1-B4c의 원본 소수 배치·기술 조사·사용자 근경 동작 시험을 마감했다. 구면 방향의 최종 채택은 보류다.** 원본의 고정 월드축이 남아 있으므로 한 위치의 육안 통과를 회전 slot/적도 전체의 방향 정합으로 확대하지 않는다. 다음 제안은 진행 확인 뒤 **1-B4d Fern 바람 구면 방향 보정·회전 비교**다. 원본을 보존한 별도 파생본에서 현지 Up/바람 방향·뿌리/변위와 회전 전후를 비교하며 아직 착수하지 않았다. 대표 구역 PCG 전 재질 샘플 비용 축소·최종 화질/플레이 접촉·SurfaceData 검증도 남아 있다.

원자료는 `Saved/Profiling/Graphics/Phase01/RidgeFernProbe_20261115/`다. `SimpleWindGraph.json`·`OriginalWindWiring.json`·`EffectiveMaterialParameters.json`은 원본 배선/실효 파라미터, `Survey.json`·`Plan.json`·`Placement.json`·`WipBefore/After.json`·`TechnicalVerification.json`·`FinalFileChecks.json`·`Verification.json`은 실제 조사/배치/최종 판정이다. `BeforePlacement.umap`은 기존11장식을 포함한 복구 사본이며 `Views/Final_User.png`는 사용자 근경 구도다. 정지 캡처가 동작 관찰을 대신하지 않으며 에디터 표시로 패키지 성능을 판정하지 않는다.

## 16. 1-B4d Fern 바람 구면 방향 보정·회전 비교(2026-11-15, 소수 시험 마감)

사용자가 이 단위를 승인했다. 기존 WIP·PIE=false·dirty=false를 확인하고 원본/공유 함수/MPC를 보존한 파생 바람 함수·부모·MI3개를 생성했다. 정식 Ridge/PCG/베이크/빌드/쿡은 범위 밖이다.

- 코드: `Scripts/Profiling/Graphics/BuildRidgeFernWind.py`. 원본 `MF_SimpleWind`71노드에서 Custom2개·영점3벡터1개만 추가한다. `RotateAboutAxis_2`의 Position은 셰이더 오프셋을 제외한 WorldPosition과 ObjectPivotPoint의 차이를 구면 접선 프레임에 투영한 값, PivotPoint는 영점이다. 기존 Tree 출력 변위는 같은 프레임에서 월드 벡터로 복귀한다. 엔진 SimpleGrassWind·공유 MF_WindGust·원본 파라미터/정점 R 가중치·스칼라 월드 공간 노이즈 위상은 유지한다.
- 현지 Up은 `-normalize(PivotWS)`다. 행성 Z축은 Up이 아닌 공통 방향 기준으로 쓰며, `abs(Up.z)>=0.95`에서는 X축을 대체 기준으로 쓴다. 두 축과 Up으로 직교 프레임을 구성한다. 극점 부근의 기준 전환은 남은 검토 항목이며 전 구면에서 매끄러운 전역 바람장을 확정한 것은 아니다. 식생 표면 normal을 중력 Up으로 대신하지 않는다.
- 산출물은 `/Game/Graphics/Environment/ElectricDreams/RidgeFernWind_V001/`의 `MF_Ridge_FernWind`·`M_Ridge_Fern`·`MI_Ridge_Fern`이다. 이 Fern 부모가 쓰는 Tree 출력만 보정하며 Impostor 출력/기타 식생에 일반화하지 않는다. RayTraced 상수 경로·쉐이딩 그래프·기존 노이즈 텍스처 조회는 보존한다. 현재 생성돼 있으므로 빌더를 재실행하지 않는다.
- 최초 생성 시 MI가 `SimpleGrassWind call input with index 0 is unset` 컴파일 경고를 냈다. 예외 없이 저장됐지만 성공 근거로 바로 채택하지 않았다. 이후 부모 재컴파일은 통과했고 WindIntensity/Weight/Speed/AdditionalWPO 배선·Plant=true를 재확인한 뒤 같은 경고가 재발하지 않았다. 빌더는 부모 컴파일 완료→MI 부모 변경/갱신→실효 통계 양수 확인 순으로 보강했다. 기존 산출물이 있어 수정본 전체 재생성은 반복하지 않았다.
- 실효 MI 컴파일 통계·원본 scalar/vector/texture override 및 정적 스위치 일치를 조회하고, 통과 시에만 기존 Fern1개에 적용/저장하는 `Saved/.../RidgeFernWind_20261115/VerifyAndApply.py` 실행을 수동 요청했다. 이 요청 시점에는 결과 대기였고, 최종 결과는 아래 절을 따른다. 에디터 통계는 GPU ms로 해석하지 않는다.

회전 비교는 먼저 보정 MI의 현재 구도 움직임/뿌리 접지를 사용자에게 확인받고, WIP 표시 메시와 검사용 광원/카메라를 월드 원점 주위로 함께 임시 회전해 관찰한 뒤 복원한다. 회전된 상태로 맵을 저장하지 않는다. 원본 공간 노이즈 위상이 남으므로 회전 전후 같은 시각의 무늬/흔들림 위상 일치를 통과 기준으로 삼지 않는다. 현지 방향·뿌리 고정·비정상 변형과 복원을 확인한다. 전체 런타임 slot·극점·PIE/SurfaceData·패키지 성능은 별도다.

근거는 `Saved/Profiling/Graphics/Phase01/RidgeFernWind_20261115/`의 `Creation.json`·`WipBefore.json`·`FilesBefore.json`·`PreflightFiles.json`이다. `BeforeMaterialApplication.umap`은 원본 Fern MI 적용 상태의 복구 사본이다. 원본 에셋104/보호12/V0024파일은 해시로 보호한다.

### 실효 MI 검증·적용·회전 관찰·복원

`VerifyAndApply.py` 수동 실행이 성공했다. 부모 컴파일 오류 목록은 비었고, 원본과 scalar/vector/texture override 배열·정적 스위치6개가 일치했다. 실제 MI의 셰이더 통계를 강제로 컴파일 완료한 뒤 읽었다. WIP에서는 기존 Fern1개의 overrideMaterials만 보정 MI로 달라졌고 나머지14메시·자기 transform/충돌/태그 등은 보존했다. 파생3에셋과 WIP를 명시 저장했으며 사용자는 **여전히 자연스럽다**고 확인했다.

| 에디터 MI 셰이더 통계 | 원본 | 보정 |
|:---|---:|---:|
| 최대 정점 명령 | 475 | 497 |
| 최대 픽셀 명령 | 686 | 686 |
| 샘플러 | 4 | 4 |
| 정점/픽셀 텍스처 샘플 추정 | 4/4 | 4/4 |
| VT 조회 추정 | 4 | 4 |

GPU ms·패키지 성능 증가율이 아니다. 최초 경고는 보존하고 이후 실제 MI 컴파일/통계/표시 결과와 구분한다. 수정된 빌더의 깨끗한 전체 재생성은 기존 산출물 때문에 다시 수행하지 않았다.

표시 메시15개·검사용 광원1개·카메라를 월드 Z +90°로 함께 임시 회전했고, 사용자는 Realtime 관찰에서 **여전히 아주 자연스럽다**고 확인했다. 이 상태를 저장하지 않았음을 디스크 맵/복구 사본 해시로 확인한 뒤 원래16 transform/camera로 즉시 복원했다. 재조회 결과 최대 위치 오차0cm·최대 회전 오차5.68e-14°·scale 차이0, 재질/충돌/태그/렌더 필드 차이0이었다. 복원된 WIP를 명시 저장·dirty=false로 확인했다. `Preview.json`의 최종 상태는 `restored_saved`이며 `Restoration.json`에 근거를 남겼다.

`BeforeRotation.umap`은 보정 MI 적용·회전 전 복구 사본이고 `BeforeMaterialApplication.umap`은 원본 MI 상태다. 이 두 복구점을 혼동하지 않는다. `Views/BeforeRotation.png`·`Rotated_User.png`·`Restored.png`와 각 카메라 메타데이터를 남겼다. 사용자가 관찰 중 카메라를 움직일 수 있으므로 세 캡처를 픽셀 불변의 수치 비교로 해석하지 않는다. 정지 캡처는 사용자 동작 관찰의 대체 근거가 아니다.

### 재현·마감과 남은 범위

깨끗한 체크아웃에서는 기존 반입/지각 재질/1-B4a/b/c의 원본 Fern1개까지 만든 뒤 `BuildRidgeFernWind.py`를 실행한다. 부모/MI 컴파일·실효 원본 override/정적 스위치 일치·통계 양수를 확인하고 WIP Fern의 Element0에 `/Game/Graphics/Environment/ElectricDreams/RidgeFernWind_V001/MI_Ridge_Fern`만 적용·WIP를 저장한다. `VerifyAndApply.py`는 이번 세션의 Saved 검증 보조이며 재생성 의존성이 아니다. 원본 메시/MI/공유 함수/MPC는 편집하지 않는다. 현재 산출물은 이미 있으므로 어느 빌더도 재실행하지 않는다.

**1-B4d의 구현·실효 컴파일/배선 확인·사용자 근경 및 한 +90° 회전 사례·복원 검증을 마감했다.** 현재 Fern1개에 보정 MI를 유지한다. 원본104/보호12/V0024파일 해시는 보존했고 원래15메시 중 의도한 Fern overrideMaterials 외 변경은 없다. 아직 모든 회전 slot/극점/기준축 전환·최대 변위/뿌리 이동량·전체 밀도·게임 조명·PIE 접촉/SurfaceData·패키지 성능을 통과한 최종 팔레트는 아니다. 다음 제안은 진행 확인 뒤 **1-B3d 지각 재질 샘플 비용 축소**다. V002의 높은 조회 수/픽셀 비용을 줄인 후보와 반복감·노멀 정합을 함께 확인하며 아직 착수하지 않았다.

최종 근거는 같은 Saved 폴더의 `FinalStatistics.json`·`Application.json`·`WipApplied.json`·`ActivePreviewFileChecks.json`·`Preview.json`·`Restoration.json`·`Bindings.json`·`FinalFileChecks.json`·`Verification.json`과 `Views/`다. 최초 경고와 수정 후 확인 결과를 분리해 보존한다.

## 17. 1-B3d 지각 재질 샘플 비용 축소(2026-11-15, 구현/기술 검증 마감)

사용자가1-B4d 마감 뒤 이 단위 진행을 승인했다. 기존 WIP·PIE=false·dirty=false를 확인했다. V001/V002·기존 장식/Fern/바람·정식 Ridge를 보존하고 V003 후보만 만든다.

- V001은 VT 조회 추정18·픽셀 명령1565, V002는72·5367이다. V002의3축×색/마스크4·노멀2 호출마다 엔진 Texture_Bombing이 축당4샘플을 사용한다. 이번에는 투영/재질 레이어 수를 바꾸지 않고 축당2샘플로 줄여 조회 수 절반을 목표로 한다. 에디터 추정이며 GPU ms는 아니다.
- 엔진 TextureVariation/RotateUV/RotateNormals를 읽기 전용 조사했다. TextureVariation에는 높이/경계 비교용 샘플5개와 추가 분기가 있어 이번 후보의 최소 경로로 채택하지 않았다. 이 조사는 모든 스위치 조합의 실효 비용을 재어 기각한 결과는 아니다.
- 코드 `Scripts/Profiling/Graphics/BuildRidgeSurfaceReduced.py`. 기존 V002 부모/MI를 파생하고 원래 WorldAlignedTexture/WorldAlignedNormals_HighQuality의 사본2개에서 각3축 샘플을2개씩 교체한다. `/Game/Graphics/Environment/ElectricDreams/RidgeSurface_V003/`에 투영 함수2·부모·MI4개만 생성한다. 맵/액터/원본을 변경하지 않는다.
- UV당 부드러운 값 노이즈로 선택한 인접 이동2개를 혼합한다. UV 회전/반전·높이 비교·추가 노이즈 텍스처는 쓰지 않는다. 색/마스크/노멀에 동일한 UV/가중치를 사용하고 노멀 혼합 뒤 정규화한다. 원래 UV의 DDX/DDY를 각 TextureSample에 명시해 선택 경계의 mip 오염을 피한다. 노이즈 주기는4타일·선택 범위8이며 기존 Bomb Offset 파라미터를 이동 강도로 유지한다.
- V002의 구면 좌표/노멀/경사/구역·레이어/거칠기·바닥 macro는 그대로 복제한다. 4점 회전/반전 bombing보다 반복 억제력이 낮거나 혼합이 흐려질 수 있으므로 조회 감소만으로 채택하지 않는다. 같은 구도 근경/원거리·노멀 정합과 함께 판정한다.
- Python AST를 통과했고 사용자에게 생성 콘솔 실행을 요청했다. 실효 MI VT 조회 수와 픽셀 명령이 모두 줄었을 때만 새 에셋을 저장한다. 현재는 생성/컴파일/수치·화질/중립 노멀 검증 대기이며 기존 WIP는 V002다. 성능 최종 판정은 패키지에서만 한다.

원자료는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceReduced_20261115/`다. `TextureVariationInventory.json`은 엔진 후보 조사, `FilesBefore.json`·`PreflightFiles.json`은 보호12/반입104/기존 파생14파일 총130개와 WIP 시작 해시다. `BeforeMaterialApplication.umap`은 보정 Fern을 포함한 V002 지각 상태의 복구 사본이다. 캡처 JSON에는 base64가 포함될 수 있어 메타데이터만 추려 읽는다.

### V003 화질 실패·V004 바닥 복원 혼합 후보

V003 생성/컴파일은 오류 없이 통과했다. 실효 MI 통계는 정점148 동일·픽셀5367→2972·샘플러7→2·픽셀 샘플75→39·VT조회72→36이다. 코드/각 함수의 좌표3노드·명시적 미분이 연결된 신규 샘플6개를 재조회했다. WIP 기존15 actor transform은 같고 지각 Element0 override만 V003로 바뀌었다. 기존130파일 해시 변화0이다. A/B/근경 전후6캡처를 남겼다.

사용자는 **바닥 원경이 거의 체크무늬에 가까울 정도로 규칙적이고, 절벽은 괜찮은 것 같다**고 판정했다. V003 전체 적용은 실패로 기록하고 중립 노멀 검증/최종 채택을 진행하지 않았다. 원경 바닥의 회귀를 비용 절감으로 정당화하지 않는다. UV 혼합의4타일(바닥 약16m) 노이즈 격자가 읽힐 가능성은 있지만 macro와의 분리 실험을 하지 않아 원인을 확정하지 않는다.

`BuildRidgeSurfaceHybrid.py`는 V003 부모/MI만 V004로 파생해 바닥 색/마스크/노멀3호출을 V002 함수로 돌리고, 절벽3호출은 V003 함수로 유지한다. 기존 함수는 복제/수정하지 않는다. V004 통계는 정점148·픽셀4170·샘플러5·픽셀 샘플57·VT54로 V002 대비 조회25%·픽셀 명령 약22.3% 감소다. 50% 조회 감소 목표를 유지하기보다 실패 역할의 최소 복원으로 범위를 줄였다. 모두 에디터 추정이며 GPU ms가 아니다.

V004 생성/부모 재컴파일을 통과했고 기존 지각 Element0에만 시험 적용·저장했다. 기존15 actor transform과 재질 외 기존 필드는 보존했다. A/B/근경의 Hybrid 캡처를 남기고 A 원경에서 바닥 반복/절벽 재판정을 요청했다. 적용 직후에는 사용자 답변·중립 노멀 검증 대기였으며, 최종 결과는 아래 마감 절을 따른다. V003 실패 에셋4개/캡처/통계는 보존한다. `HybridFilesBefore.json`은 기존130+V0034파일 보호, `BeforeHybridApplication.umap`은 실패 V003 적용 상태다. 원래 채택 전 복귀 기준은 단위 시작 V002 복구 사본이다.

사용자의 적용 기법 질문 시점에는 실제 바인딩이 V003임을 재조회했다. V003 바닥은2샘플 UV 이동 혼합+18m/48.6m macro 색/거칠기 변화이며 회전/반전은 없다. 이후 비교를 이어 V004를 적용했으므로 현재 바닥은 V002의4샘플 Texture Bombing(이동/회전/반전)+같은 macro이고 절벽만2샘플이다. 3축 투영과 반복 억제를 구분해 설명했다.

캡처 이름은 MCP에서 소문자로 저장됐다. 변환 코드의 대소문자 처리 누락으로 PNG 바이트가 일시적으로 _base64.txt에 저장됐으나 PNG 시그니처 확인 후 올바른 .png와 원래 base64 텍스트로 복구했다. 이미지 픽셀/에셋/맵 변경은 없으며 변환에는 대소문자 무관한 접미사 처리를 사용한다.

### V004 중립 노멀 검증·복원·마감

사용자는 V004에 대해 **절벽은 여전히 괜찮고 바닥은 V003보다 확실히 개선됐다**고 확인한 뒤 비용 증가 여부를 물었다. V004는 실패 V003보다 VT36→54·픽셀2972→4170으로 증가하지만, 단위 출발점 V002보다 VT72→54(25% 감소)·픽셀5367→4170(약22.3% 감소)이다. 바닥의 비용 절감은 되돌리고 절벽의 절감을 유지한 후보이며 GPU 시간 감소율로 해석하지 않는다고 설명했다. 이 답변을 바닥 최종 화질 승인으로 확대하지 않는다.

사용자가 AA4→0을 수행했고 실효값0을 조회했다. 현재 A 구도의 경사 진단 마스크를 새로 캡처하고, World Normal 전환을 수동 요청해 기하 노멀 재질과 V004 중립 노멀을 같은 구도로 비교했다. V004 부모의 정상 텍스처2개만 원래 사용했던 `/Game/MSPresets/MSVTTextures/Placeholder_Normal`로 잠시 바꾸고 검사 후 실제 BeachCliff/MossyRockyGround 노멀을 복원했다. V002/V003 함수와 원본 텍스처는 수정하지 않았다.

| AA0 표시 RGB8 비교(stride4·구역 내부/4방향 이웃 마스크) | 표본 | 평균 최대 채널 차이 | p95 | 최대 |
|:---|---:|---:|---:|---:|
| 전체 | 22,779 | 0.4176 | 1 | 2 |
| 절벽 | 9,675 | 0.1745 | 1 | 1 |
| 바닥 | 9,149 | 0.7328 | 1 | 2 |
| 전이 | 3,955 | 0.2834 | 1 | 1 |

기준 p95≤2를 통과했다. World Normal 표시의 RGB 차이이며 법선 각도 오차·실제 노멀맵 무늬의 색/방향 정합 전부·전체 프레임 동일성을 측정한 것은 아니다. 이전 V002 비교와 표본/뷰포트 크기가 같다고 전제하지 않는다.

첫 경사 마스크 캡처는 회색 기하 재질 화면이어서 무효로 보존했다(`diagnosticslope_first_gray.png`). 재질 변경과 캡처를 독립 MCP 호출로 분리한 뒤 빨강/초록/노랑의 정상 마스크를 다시 캡처해 수치에 사용했다. 같은 배치 안의 변경/캡처/후속 변경으로 정확한 렌더 상태를 보장한다고 가정하지 않는다.

실제 노멀2개·V004 바인딩·검사 전 사용자 카메라를 복원하고 V004 부모/MI·WIP를 저장했다. 사용자가 AA4·Lit로 복귀했고 실효값4/최종 Lit 캡처를 확인했다. 원래 콘솔 변수 우선순위까지 확인한 복구는 아니며 프로젝트 INI를 수정하지 않았다. `NormalDiagnosticState.json`의 최종 상태는 `restored`다. 최종15 actor transform/컴포넌트는 V004 적용 직후와 차이0이고, V002와 V004 MI의 scalar/vector/texture 파라미터 값도 같았다. 진단 후 정상 텍스처 참조·부모/MI/WIP dirty=false·PIE=false를 재조회했다.

깨끗한 재생성은 기존 반입/진단/V001/V002→V002 지각 상태에서 소수 장식/Fern 및 구면 바람→`BuildRidgeSurfaceReduced.py`(V0034개)→`BuildRidgeSurfaceHybrid.py`(V0042개)→지각 Element0에 V004 적용 순서다. V003 전체 MI는 실패했지만 V004의 절벽 함수와 재생성이 V003를 요구하므로 V003 산출물을 삭제하지 않는다. V003/V004 빌더는 맵을 변경하지 않으며 이미 생성돼 재실행하지 않는다. 소수 배치 빌더의 V002 가드를 깨지 않도록 V004 적용은 마지막에 한다.

**1-B3d의 구현/셰이더 비용 축소·중립 노멀·보존/복원 기술 검증을 마감했다. V004를 조건부 후보로 유지한다.** 최초 VT 절반 감소 목표는 바닥 화질 실패로 채택하지 않았고 최종은 V002 대비 VT25%·픽셀 명령22.3% 축소다. 바닥 최종 화질/원경 반복·전체 slot/적도/이음매·게임 조명/밀도·PIE/SurfaceData·패키지 성능은 미완료다. 다음 제안은 진행 확인 뒤 **1-B3e 바닥 원경 반복 원인 분리**로, V003/V004의 매크로 변조 유무를 같은 구도에서 비교해 UV 혼합과 큰 크기 변조의 기여를 구분한 뒤 다음 저비용 후보를 고른다. 아직 착수하지 않았다.

최종 근거는 `HybridCreation.json`·`V003VisualFailure.json`·`HybridApplication.json`·`TechnicalVerification.json`·`HybridWipApplied.json`·`NeutralNormalComparison.json`·`NormalDiagnosticState.json`·`FinalLiveVerification.json`·`WipAfter.json`·`FinalFileChecks.json`·`Verification.json`이다. 캡처는 최초 전후6개·Hybrid3개·진단 정상 마스크/기하/중립3개·무효 회색1개·최종 Lit1개 총14PNG다. 복구 사본은 단위 시작 V002(`BeforeMaterialApplication.umap`)와 실패 V003(`BeforeHybridApplication.umap`)을 구분한다.

## 18. 1-B3e 바닥 원경 반복 원인 분리(2026-11-16, 기술 점검 마감)

사용자가 이 단위 착수를 승인했다. 기존 빌더는 재실행하지 않고, V003/V004의 바닥 macro 색·거칠기 변조 유무만 같은 원경에서 비교했다. 새 Anti-Tiling 구현은 시작하지 않았다.

### 비교 조건과 재현

- 시작 맵은 기존 WIP, PIE=false, AA4다. V003/V004 부모·MI와 WIP는 dirty=false였고, 기존 메시 액터15개의 transform/컴포넌트는 1-B3d 인계 기록과 차이0이었다. Fern 보정 MI도 유지했다. 사용자 카메라는 별도로 기록했다.
- `/Game/Graphics/Environment/ElectricDreams/RidgeSurfaceRepetitionProbe_V001/`에 기존 V003/V004의 `MI_Ridge_Surface`를 각각 `MI_Ridge_V003_MacroOff`·`MI_Ridge_V004_MacroOff`로 복제했다. **진단용 MI2개만 만들었고 부모/함수는 복제·수정하지 않았다.**
- 두 진단 MI에서 `Ground Macro Strength`만0으로 설정했다. 기존 MI의 실효값은0.5다. getter로0/0.5를 확인했고, 복제본의 macro override를 제외한 scalar/vector/texture override와 부모가 원래 MI와 같음을 재조회했다. `Ground Macro Size`는 기존1800cm를 유지했다.
- `BuildRidgeSurfaceAntitile.py`의 `MACRO_CODE`에서 Strength=0이면 multiplier=1이다. 색 곱셈이 중립이 되고 거칠기의 `(Macro - 1) * 0.2`도0이므로 이 값 하나로 두 변조를 함께 끈다. UV 혼합·노멀·나머지 재질 파라미터는 유지한다.
- `RidgeSurfaceReduced_20261115/ViewPoses.json`의 A/B 두 카메라를 사용했다. WIP 지각 `StaticMeshActor_2.StaticMeshComponent0`의 `overrideMaterials[0]`만 V003·V003 MacroOff·V004·V004 MacroOff로 교체했다. 재질 변경과 캡처는 별도 MCP 호출로 분리했다. 각 구도4조건, 총8장이다. 모두 같은 카메라/FOV90·1010×859·AA4의 Lit 화면이다. AA0 픽셀 차이/중립 노멀 검증은 이번 단위에서 수행하지 않았다.
- 재현하려면 기존 V003/V004 MI에서 위 이름의 진단 MI를 복제하고 macro strength0을 적용·저장한 뒤, 같은 A/B 구도에서 네 MI를 지각 Element0에 교대로 적용한다. 기존 진단 MI가 있으면 다시 만들지 않는다. 종료 때 지각을 원래 V004 MI로, 카메라를 세션 시작 위치로 복원하고 WIP만 저장한다. 원본 V003/V004 MI의 파라미터를 직접 바꾸지 않는다.

### 관찰과 다음 후보

**에이전트 육안 관찰:** V003 바닥의 규칙적인 무늬는 macro를 꺼도 남는다. V004도 macro 유무로 무늬 배치 전체가 사라지지 않는다. 따라서 이번 A/B에서는 **추가 macro 색·거칠기 변조만을 V003 체크무늬의 원인으로 볼 수 없다.** V003/V004 바닥 UV 혼합 경로의 차이를 우선 조사할 근거다. 사용자의 원경 화질 판정은 아직 받지 않았다.

이 실험은 UV 선택 노이즈의4타일 격자, 두 샘플 이동 혼합, 원본 텍스처의 반복, 기존 지각 삼각형 음영 중 어느 항목이 얼마만큼 기여하는지까지 분리하지 않는다. 특히 화면의 큰 직선/삼각형 명암을 모두 UV 결함으로 해석하지 않는다. macro가 반복 지각에 전혀 영향을 주지 않는다고도 결론 내리지 않는다.

**다음 제안은 1-B3f 바닥 한정 삼각 격자3샘플 혼합 후보 소수 시험**이다. V004의 절벽2샘플 경로와 구면 좌표·경사/구역 마스크를 유지하면서 바닥의 정사각 선택장/두 샘플 혼합을 바꾸어 같은 A/B와 근경에서 비교한다. [Heitz·Neyret의 원 연구](https://eheitzresearch.wordpress.com/722-2/)는 삼각 격자의 세 무작위 패치를 혼합하며, [후속 연구](https://jcgt.org/published/0008/04/02/paper.pdf)는 단순 혼합의 대비 손실을 지적한다. 따라서 사각 격자 억제와 함께 흐려짐·대비 손실·노멀 정합을 통과 조건으로 둔다. 이 프로젝트에서의 최종 혼합식·통계·화질은 미확정이며, **사용자 판정과 별도 진행 승인 전에는 구현하지 않는다.** 조회 수 예상만으로 채택하지 않고 성능은 패키지에서 판정한다.

### 복원과 근거

원자료는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceRepetition_20261116/`다. `BeforeComparison.umap`은 단위 시작 V004 WIP의 복구 사본이며 `CameraBefore.json`은 시작 카메라다. `WipBefore.json`·`WipRestored.json`·`TechnicalVerification.json`으로 기존15메시의 transform/컴포넌트 차이0과 진단 파라미터 격리를 확인했다. `FilesBefore.json`·`FinalFileChecks.json`의 보호136파일은 변경0이다. WIP는 복원 후 에디터 저장으로 파일 해시가 달라졌으므로 바이트 동일 복원이라고 표현하지 않는다.

V004 바인딩·시작 카메라를 복원하고 WIP만 저장했다. `FinalLiveVerification.json`은 PIE=false·AA4·WIP/V003/V004/진단MI2개 dirty=false다. 원래 macro0.5는 유지했고 노멀 텍스처·광원·장식·정식 Ridge·Support source·기본 편성·확정 카메라 경로는 변경하지 않았다. 새 빌드/쿡/패키지 성능 측정은 없다.

`Views/`의 `a_`·`b_` × `v003_`·`v004_` × `macro_on`·`macro_off`는8개 원본 PNG와 카메라/base64 JSON이다. `b_ground_comparison.png`는 B 원본의 동일 영역(x480,y509,530×350)을 확대 없이 배열한 비교판으로, 위 V003/아래 V004·왼쪽0.5/오른쪽0이다. 진단 MI·WIP·Saved는 모두 Git 제외다. 저장소에는 이 재현 절차와 현재 상태만 남긴다.

## 19. 1-B3f 바닥 삼각 격자3샘플 후보(2026-11-16)

사용자가 후속 단위 진행을 승인했다. `Scripts/Profiling/Graphics/BuildRidgeSurfaceTriangle.py`를 작성했고 사용자가 콘솔에서 실행해 V005 함수2/부모/MI4개를 생성·저장했다. 기존 빌더는 재실행하지 않았다. 코드 생성 단계는 맵을 변경하지 않는다.

### 구현과 재생성

- V004 부모/MI를 파생하고, 바닥 색·AoRDp·노멀3호출만 `MF_RidgeTriangleTexture`·`MF_RidgeTriangleNormal`로 바꾼다. 절벽의 V003 두 샘플3호출과 기존 구면 좌표/노멀 프레임·경사/구역·macro는 유지한다.
- 엔진의 원래 투영 함수2개를 파생해 축마다 삼각 격자의 세 정점에서 결정되는 UV 이동3개를 샘플링한다. 원본 타일 크기400cm는 유지한다. `UV * 3.464101615`를 삼각 격자 좌표로 변환하고 정점 ID의 hash로 UV를 이동한다. 회전·반전은 없다.
- 세 무게중심 좌표를4제곱 후 합으로 나눈 고정 가중치를 색/마스크/노멀에 공통 사용한다. 노멀은 혼합 뒤 정규화한다. 삼각 경계의 세 번째 가중치는0으로 수렴하고 공유 정점의 UV는 같게 유지한다. 이는 **히스토그램 보존 기법 전체의 구현이 아니며**, 흐려짐·대비 손실은 화질 검증 대상이다. 추가 노이즈 텍스처/LUT·원본 텍스처 전처리는 없다.
- 모든 신규 샘플에 원래 투영 UV의 DDX/DDY를 명시한다. 라이브 배선 재조회에서 두 함수 각각 좌표 Custom3개·명시적 미분 샘플9개를 확인했다. V004/V005 MI의 scalar/vector/texture override도 같았다.
- 새 체크아웃은 기존 반입/진단/V001/V002·소수 장식/Fern→V003→V004를 먼저 준비한다. 그 뒤 콘솔에서 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeSurfaceTriangle.py"`를 한 번 실행한다. 현재는 V0054개가 있으므로 재실행하지 않는다. 생성 전 PIE/dirty/동일 버전 존재 가드가 있으며 에셋만 만든다. 적용 대상은 WIP 지각 Element0 한 곳이다. 정식 Ridge/Support source는 변경하지 않는다.

### 검증 결과

| 실효 MI 에디터 추정 | V004 | V005 |
|:---|---:|---:|
| 정점 명령 | 148 | 148 |
| 픽셀 명령 | 4170 | 3401 |
| 샘플러 | 5 | 2 |
| 픽셀 샘플 | 57 | 48 |
| VT 조회 | 54 | 45 |

V004 대비 VT 조회16.7%·픽셀 명령18.4% 감소이며 **패키지 GPU ms 감소율이 아니다.** Python AST·실제 재질 컴파일을 통과했다. 별도의 CPU 참조 검사에서 가중치25,921개 지점의 비음수/합1과 삼각선·격자 변의297쌍 연속성을 확인했다(최대 가중치 차이 약4.0e-7). CPU 검사가 GPU HLSL의 수치 일치나 실제 화질을 증명하지는 않는다.

기존 A/B·근경·사용자 시작 카메라4구도에서 V004/V005 전후8장을 AA4/Lit로 촬영했다. 사용자는 **원경이 개선됐고 근경도 자연스럽다**고 확인했다. 사용자의 이 답변을 모든 회전 slot·전체 밀도·게임 조명·패키지 성능 승인으로 확대하지 않는다.

AA0에서 새 A 경사 마스크를 촬영하고 사용자가 World Normal로 전환했다. 기존 단색 `M_Ridge_Ground`의 기하 노멀과, V005 부모의 실제 노멀2개를 `Placeholder_Normal`로 잠시 바꾼 중립 노멀을 같은 카메라로 비교했다. 원본/V002/V003/V004 함수·재질은 수정하지 않았다.

| AA0 표시 RGB8 비교(stride4·구역 내부/4방향 이웃 마스크) | 표본 | 평균 최대 채널 차이 | p95 | 최대 |
|:---|---:|---:|---:|---:|
| 전체 | 22,731 | 0.4816 | 1 | 2 |
| 절벽 | 9,629 | 0.1744 | 1 | 1 |
| 바닥 | 9,151 | 0.8611 | 1 | 2 |
| 전이 | 3,951 | 0.3516 | 1 | 2 |

전체/각 경사 구간 p95≤2를 통과했다. 표시 RGB 차이이며 법선 각도 오차·실제 노멀맵의 모든 방향 정합·전체 프레임 동일성 검증은 아니다. 실제 노멀2개와 검사 전 사용자 카메라를 복원했고 AA4·Lit 복귀를 요청했다.

사용자가 AA4·Lit 복귀를 완료했고 실효값4·최종 Lit 캡처를 확인했다. 실제 노멀2개·V005 바인딩·검사 전 카메라·MI 파라미터 보존을 재조회했다. 진단 후 기존15메시 상태는 V005 시험 적용 직후와 차이0이며 보호138파일 해시는 모두 같다. V0054개는 생성 시 저장돼 있고 V005 부모는 진단 복원 후 dirty=true, MI는 dirty=false다. WIP는 V005 시험 적용·dirty=true이며 디스크는 여전히 V004다. `NormalDiagnosticState.json`은 `restored_unsaved`, `FinalLiveVerification.json`은 PIE=false·AA4·Lit·저장 승인 대기다. CVar 실효값 복원이며 원래 우선순위까지 복원했다고 전제하지 않는다.

### 저장 상태와 복구 근거

다음은1-B3f 저장 거부 당시의 기록이다. 저장 대기는 아래 **사용자 저장 확인·1-B3f 마감**에서 해소됐으며, 현재 상태는 [Current](../Current.md)를 따른다.

V005 적용 WIP의 `save_assets`는 **자동 승인 검토가 거부**했다. 명시 사유는 미저장 시험 적용의 시각 검증 승인만으로 후보 적용의 영속화를 승인받았다고 볼 수 없다는 것이다. 우회 저장하지 않았다. 현재 V0054개는 생성 시 저장됐지만 WIP 디스크 저장본은 단위 시작 V004이며, 진단 복원 뒤 V005 적용 WIP 저장을 사용자에게 명시적으로 확인한다. 저장 승인 전에는 단위를 저장 완료로 마감하지 않는다.

근거 폴더는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceTriangle_20261116/`다. `FilesBefore.json`·`ApplicationFileChecks.json`의 보호138파일 변경0, `WipBefore.json`·`WipApplied.json`·`Application.json`의 기존15메시 transform/컴포넌트 차이는 지각 overrideMaterials 한 곳뿐이다. `Creation.json`·`GraphVerification.json`은 비용/배선, `NeutralNormalComparison.json`은 노멀 검사다. `Views/before_`·`candidate_`의 A/B/Close/User8장과 진단3장, `ground_comparison.png`를 보존한다.

`BeforeComparison.umap`과 `BeforeNormalDiagnostic.umap`은 모두 **디스크의 V004 WIP**다. 두 번째 복사 전 V005 저장이 거부됐으므로 V005 복구 사본이라고 해석하지 않는다. `NormalDiagnosticRecovery.json`에 이 차이를 기록했다. `BeforeNormalDiagnostic_M_Ridge_Surface.uasset`은 실제 노멀이 있는 생성 직후 V005 부모다. 검사 전 카메라·실제 노멀 참조/복원 진행은 `NormalDiagnosticState.json`이 소유한다.

**1-B3f의 구현·원경/근경 사용자 확인·중립 노멀·복원 검증은 마쳤고 저장 승인을 기다린다.** V005를 WIP의 조건부 후보로 유지할지 저장을 명시적으로 확인한 뒤 마감한다. 다음 제안은 별도 진행 확인 후1-B3g V005 회전/적도·경계 소수 시험이다. 모든 slot·게임 조명/밀도·PIE 발/탄환/카메라·SupportData·패키지 성능은 미검증이며 정식 Ridge 적용·PCG·새 빌드/쿡은 수행하지 않았다.

### 사용자 저장 확인·1-B3f 마감

사용자가 WIP `LVI_Ridge_SurfaceProbe`와 V005 `M_Ridge_Surface`를 직접 저장하고 다음 단위 진행을 승인했다. 읽기 전용 재조회로 WIP/V005 부모/MI dirty=false·지각 V005 바인딩·실제 BeachCliff/MossyRockyGround 노멀을 확인했다. 위 저장 대기는 해소됐고, 앞선 자동 승인 검토 거부를 우회 호출로 재시도하지 않았다. `UserSaveVerification.json`에 확인을 남겼고 `NormalDiagnosticState.json`은 `restored_saved`로 갱신했다. **1-B3f를 저장까지 마감하며 V005는 WIP 조건부 후보로 유지한다.**

## 20. 1-B3g 회전/적도·경계 소수 시험(2026-11-16, 마감)

사용자 승인 뒤 에디터 소수 시험으로 범위를 잡았다. 런타임 slot5에 해당하는 Pitch180°·Yaw90° 회전, 대표 식생 근처 기준점을 적도에 놓는 임의 회전, 기존 구역 경계를 확인한다. 실제8 slot 전체·인접 옥탄트 로드/이음매 접합·PIE/SupportData·패키지 성능 검증으로 확대하지 않는다. 기존 빌더는 재실행하지 않는다.

- 코드의 slot 회전은 `LNPOctantSpawnSubsystem.cpp`의8개 회전에서 읽었다. 회전 합성은 엔진 `RotationTranslationMatrix.h`의 축 규약에 따라 계산했다. 구면 원점을 중심으로 기존 메시15개·검사용 PointLight1개·카메라를 같은 회전으로 옮긴다. 스폰/Support 마커 등 런타임용 액터는 이 렌더링 전용 시험에서 옮기지 않으며 PIE를 실행하지 않는다.
- 시작 시 메시15개의 transform/컴포넌트가1-B3f 마지막 상태와 같고 WIP/V005 부모/MI dirty=false임을 확인했다. 보호142파일의 시작 해시와 **V005 저장 WIP** 복구 사본을 남겼다. 회전된 상태는 저장하지 않는다.
- slot5 적용 재조회는16개 액터의 최대 위치 오차3.64e-12cm·최대 회전 기저 성분 오차1.17e-15, 재질/충돌/태그/렌더 필드 변화0이다. 실험 중 디스크 WIP와 복구 사본 해시가 같음을 확인했다. 원경/근경을 촬영하고 사용자에게 재질/Fern 동작 확인을 요청했다.
- 적도 사례의 계획 회전은 Roll54.337285°다. 기존 Fern 근처 기준점(20185.296,15905.266,22164.943cm)을 회전해 월드 Z≈0cm에 놓는다. 이 임의 회전은 런타임 slot 회전 그 자체가 아니라 월드 Z를 Up으로 쓰는 가정의 스트레스 시험이다.

원자료는 `Saved/Profiling/Graphics/Phase01/RidgeSurfaceOrientation_20261116/`의 `Before.json`·`FilesBefore.json`·`BeforeRotation.umap`·`slot5Plan.json`·`equatorPlan.json`·`Preview.json`·`slot5Verification.json`·`Views/`다. 현재 상태와 미복원 여부는 `Preview.json`을 먼저 읽는다. 모든 조작이 끝난 뒤 원래16 transform/카메라와 메시 컴포넌트를 대조해 복원한다. 다음 단위는 별도 진행 확인한다.

### 회전 관찰과 복원 결과

사용자는 slot5와 적도 사례 모두 **재질과 Fern이 자연스럽다**고 확인했다. 각 사례는 근경 Realtime 관찰이며 정지 캡처만으로 움직임을 통과시킨 것이 아니다. 원경/근경을 각1장씩 기록했다. 동일 프레임/픽셀 불변이나 바람 위상 일치를 판정하지 않았다. 적도 캡처에는 월드 Z=0 에디터 격자가 외곽 일부에 겹칠 수 있어 질감 결함과 구분한다.

적도 재조회는16개 액터의 최대 위치 오차3.64e-12cm·회전 기저 성분 오차3.26e-14, 재질/충돌/태그/렌더 필드 변화0이다. 두 임시 회전 동안 디스크 WIP가 `BeforeRotation.umap`과 같은 해시임을 확인했다.

이후 원래16 transform과 단위 시작 카메라로 복원했다. 메시15개 전체 컴포넌트·광원 transform·카메라를 시작 상태와 재귀 대조한 결과1e-7 허용치 초과 변화0·최대 수치 차이7.11e-14였다. 복원된 WIP만 명시 저장했고 WIP/V005 부모/MI dirty=false·보호142파일 해시 변화0을 확인했다. `Preview.json`은 `restored_saved`다. 새 재질/메시를 만들거나 정식 Ridge/Support source를 수정하지 않았다.

남은 경계 확인을 위해 사용자에게 원래 구역의 질감/단색 접합 경계로 카메라만 옮기도록 요청했다. 실제 좌표 함수 재조회에서 구역 다각형은 `step(0, cross)`·이음매 제외는 `step(5500, min(PositionLocal))`인 것을 확인했다. 따라서 이번 경계 관찰은 원래 이진 마스크의 표현을 평가하며 아직 경계 전이를 새로 구현하지 않았다.

사용자가 두 영역이 함께 보이는 구도를 준비했다. 같은 카메라의 Lit·Scope 진단 쌍으로 **중앙 큰 절벽 오른쪽 외곽은 실제 지각의 구역 경계**, 왼쪽 앞 보라색 기둥은 기존 별도 암반 메시라는 점을 구분했다. Scope에서 구역 안은 초록·구역 밖 지각은 회색이며 별도 암반은 보라색을 유지한다. 진단 뒤 V005를 즉시 복원했고,15메시 transform/컴포넌트가 회전 복원 직후와 정확히 같음을 확인한 뒤 WIP만 저장했다. 최종 WIP/V005 부모/MI dirty=false·PIE=false·AA4, 실제 노멀2개와 보호142파일 해시를 확인했다. 임시 진단 상태도 저장하지 않았다.

회전 시험은 단위 시작 카메라로 복원했고, 이후 사용자가 선택한 경계 관찰 카메라는 그대로 유지했다. `BoundaryDiagnosticState.json`은 `restored`, `BoundaryRestoration.json`에 진단 복원을 기록했다. 캡처는 시작/slot5/적도 각 원경·근경6개, 회전 복원1개, 경계 Lit/Scope/복원3개 총10개다. 이 시점에는 경계 미관 질문의 답변 대기였으며, 이후 아래 정정 절에서 그 질문을 철회했다.

### 임시 경계 질문 정정·단위 마감

사용자는 시험 부분에만 텍스처를 덮었으므로 경계가 끊기는 것은 당연하며, 지금 완화하는 작업이 어떤 의미인지 지적했다. **임시 시험 마스크를 최종 아트 전이와 섞은 질문이었음을 인정하고 경계 미관 수용 질문을 철회했다.** 이 단계의 목적은 적용 범위/범위 밖 보존과 회전 정합 확인이다. 의도된 이진 마스크는 유지하며, 경계 완화는 구현하거나 승인받은 작업이 아니다. 이를 사용자의 경계 화질 통과 판정으로 기록하지 않는다. 최종 서로 다른 지형 재질을 연결할 때 전이가 필요한지는 별도로 판단한다.

**1-B3g의 제한된 에디터 회전/적도 사례·구역 확인·복원/저장을 마감했다.** slot5/적도 사례는 사용자 재질/Fern 확인을 받았고, 임시 회전/진단 뒤 원래 액터와 V005·실제 노멀을 복원·저장했다. `Preview.json=restored_saved`, `BoundaryDiagnosticState.json=restored`, 최종 dirty=false·PIE=false·AA4/Lit·보호142파일 해시 변화0이다. 최종 WIP 파일은 복원 저장/사용자 경계 카메라 저장으로 시작 사본과 해시가 달라졌으며 바이트 동일 복원이라고 표현하지 않는다. 정식 Ridge·Support source·기본 편성·풀·확정 경로는 보존했다.

남은 전체8 slot/극점·바람 기준축 전환·런타임 LevelInstance·인접 옥탄트 이음매·게임 조명/밀도·PIE/SurfaceData·패키지 성능은 별도다. 다음 제안은 진행 확인 뒤 **1-B4e WIP 런타임 연결/SurfaceData 현재성 점검과 수동 PIE 소수 접촉 검증**이며 미승인·미착수다. 새 재질/메시·PCG·Support/Nav 베이크·빌드/쿡은 이번 단위에서 하지 않았다.

## 21. 1-B4e 런타임 연결/SurfaceData 초기 점검(2026-11-16, 선행 준비는 §22에서 해소)

아래는 베이크 전 초기 점검 기록이다. 이후 §22에서 승인받아 전용 데이터를 생성하고 런타임·수동 접촉·복원까지 마쳤다.

사용자가1-B4e 진행을 승인했다. 먼저 연결을 읽기 전용으로 점검했고, **현재 WIP를 정상 게임 경로로 검사할 선행 데이터가 없어 PIE는 시작하지 않았다.** 베이크·정식 Ridge 적용은 별도 단위로 분리한다고 착수 시 알렸으며 아직 수행하지 않았다.

### 확인한 사실

- 현재 맵은 기존 `Graphics/V001/LVI_Ridge_SurfaceProbe`, PIE=false·WIP/V005 부모/MI dirty=false다. 액터15개의 transform/컴포넌트는1-B3g 복원 상태와 정확히 같다. Support2개·Blocker 전용1개·Decoration12개이며, Decoration12개는 모두 NoCollision·내비 영향 꺼짐을 유지한다. 이는 설정 확인이고 실제 발/카메라/탄환 접촉 통과가 아니다.
- `/Script/LootNPop.Default__LNPSettings.OctantPool`은 `/Game/Maps/OctantPoolData`다. 정식6개 정의(Bastion/Hollow/Acropolis/Pyramid/Grove/Ridge)에는 원래 `/Game/Maps/Ridge_00/LVI_Octant_Ridge_00`와 원본 Ridge SurfaceData가 연결돼 있고 그래픽스 WIP는 없다. 모든 정의의 slot mask255·SeamSignature=`DysonSphere_R300m_V1`을 확인했다.
- `/Game` 아래 등록된 `ULNPOctantSurfaceData`42개 header를 조회했으나 `SourceLevel`이 이 그래픽스 WIP인 데이터는0개다. 과거 Ridge V001~V011 작업본 데이터도 각각 자기 작업본 경로를 가리킨다. **현재 WIP의 데이터가 낡았다는 판정이 아니라 대응 데이터가 없다는 판정**이다. 기존 Ridge 데이터의 전체 현재성/재베이크 검증을 대신 수행한 것은 아니다.
- 런타임 `LNPSurfaceDataSubsystem.cpp::ValidateLevelPair`는 SourceLevel manifest가 정확히1개이고 `Definition.LevelAsset` 패키지와 일치해야 통과한다. 원본 Ridge SurfaceData를 WIP LevelAsset과 연결하면 이 검사에서 실패한다. 태그/형상이 같다는 이유로 manifest를 고치거나 검사를 우회하지 않는다.
- WIP의 `WorldSettings.defaultGameMode=None`, INI의 `GlobalDefaultGameMode=/Script/Engine.GameMode`다. 또한 `LNPOctantSpawnSubsystem`은 설정의 풀로8개 LevelInstance를 만든다. 현재 WIP를 독립 PIE로 열었다는 사실만으로 LNP의 초기화·SurfaceData snapshot·플레이어/장식 접촉 검증이 성립하지 않는다.

### 다음 선행 구현 단위 제안: 1-B4e-1

다음은 **별도 승인 전 실행하지 않는다.** 현재 형상/충돌과 정식 풀을 유지하며 그래픽스 WIP만 대상으로 한다.

1. 저장된 WIP를 보존하고 다른 정상 루트 맵을 연 뒤 기존 콘솔 명령 `LNP.SurfaceNav.BakeOctant /Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe`로 WIP 전용 데이터를 생성한다. 기존 베이커의 기본 옵션을 쓰며 형상을 재생성하지 않는다. 베이크 로그/Support source/층·seam·Nav·Spawn 결과를 확인한다. 충돌/PIE 통과로 해석하지 않는다.
2. 베이커의 이름 규칙은 `LVI_Octant_` 접두어만 제거하므로 이 WIP의 실제 예상 출력은 `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/DA_OctantSurface_LVI_Ridge_SurfaceProbe`다. 생성 전 경로 충돌을 다시 확인한다.
3. `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/DA_GraphicsRidgeValidationPool` 같은 별도 풀을 정식 풀에서 파생하고 Ridge 정의만 WIP/새 SurfaceData 쌍으로 교체한다. 정식 풀·원본 SurfaceData·기본 편성·설정 파일은 보존한다. `LinkPoolSurfaceData`는 정식 풀을 저장하므로 이번 격리 시험에 사용하지 않는다.
4. 검사용 풀의 임시 선택·복원 경로와 정상 게임 루트를 확인한 뒤 사용자가 수동 PIE를 시작한다. slot 로드 로그·SurfaceData snapshot 게시 성공을 확인하고 대표 구역의 발/카메라/탄환을 검토한다. 아직 임시 선택이나 맵 전환도 하지 않았다. 이 단계의 결과가 나오기 전1-B4e PIE 완료로 마감하지 않는다.

원자료는 `Saved/Profiling/Graphics/Phase01/RidgeRuntimeProbe_20261116/`의 `RuntimePreflight.json`·`SurfaceDataInventory.json`·`CurrentWipContract.json`·`FileChecks.json`·`Verification.json`이다. 자산/설정 변경0·보호142파일 해시 변화0·PIE 미실시다. 이 읽기 전용 점검 뒤 새 베이크나 자동 검사 장치를 만들지 않고 다음 단위 진행을 확인한다.

## 22. 1-B4e-1 WIP 전용 베이크/검사용 풀 준비(2026-11-16, 마감)

사용자가 선행 단위를 승인했다. WIP·정식 풀·V005 부모/MI dirty=false·PIE=false와 새 데이터/풀 경로 부재를 재조회했다. WIP/정식 풀 복구 사본과142파일·INI 해시를 `Saved/Profiling/Graphics/Phase01/RidgeRuntimeValidation_20261116/`에 남겼다. 현재 복구/진행 상태는 이 폴더의 기록을 우선 확인한다.

사용자에게 TestMap03을 연 뒤 `LNP.SurfaceNav.BakeOctant /Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/LVI_Ridge_SurfaceProbe`를 실행하도록 요청했다. TestMap03의 GameMode는 `/Game/Gameplay/BP_LNPGameMode`임을 읽기 전용으로 확인했다. 생성 결과·풀 구성·임시 선택·PIE는 아직 진행 전이다. 정식 풀/원본 SurfaceData/기본 편성·설정 파일을 보존한다. 기존 형상/재질 빌더를 재실행하지 않는다.

### 베이크·풀 생성과 런타임 로드

사용자 콘솔 실행이 성공해 WIP 전용 `DA_OctantSurface_LVI_Ridge_SurfaceProbe`를 저장했다. DataVersion7·SourceLevel=WIP1개·dirty=false를 재조회했다. 베이크 로그는 지각85,867 tris·Support2소스/44층·전체593,257샘플·Support4,202,801B, Nav73,836셀/109 components/350 portals, Spawn authored12/candidates9,373/noNavRejected0이다. 이는 데이터 생성 결과이며 실제 접촉 통과가 아니다.142개 보호 파일과 INI2개/WIP 추가 확인은 변경0이었다.

`DA_GraphicsRidgeValidationPool`을 정식 풀에서 복제하고6개 정의 중 index5 Ridge의 LevelAsset/SurfaceData만 WIP/새 데이터로 바꿨다. 나머지 정의·AllowedSlotRotations255·SeamSignature는 유지했고 새 풀만 저장했다. 기존 풀의 구조를 JSON으로 읽어 파생값을 만들었으므로 원본 struct 배열을 수정하지 않았다.

`/Script/LootNPop.Default__LNPSettings`의 `octantPool`만 새 풀로 임시 선택하고 재조회했다. **SaveConfig를 호출하지 않았으며 정식 풀/INI 해시도 그대로다.** 원래 선택과 복원 필요 상태를 `SelectionState.json`에 먼저 기록했다. 이 파일이 `active_memory_override`이면 PIE 종료 후 원래 풀 선택을 복원해야 한다. 재현 때도 먼저 복구 기록을 남기고 임시 선택의 비영속성과 파일 해시를 확인한다.

사용자가 TestMap03에서1인 PIE를 시작했다. 실제 WIP가 slot5/7에 선택됐고8개 LevelInstance가 모두 로드됐다. `Published generation 1 with 8 validated slots, 166 source bindings, registry generation 2`를 확인했다. Nav runtimeLayers664·seamLinks4,428·blockedSeamNodes/Edges0·maxSeamRadiusDelta=0.000cm·minSeamNormalDot0.95296이다. 런타임의 Level/SurfaceData 쌍·payload·source binding·이음매 게이트를 통과해 snapshot이 게시된 근거다. 모든 게임플레이·전체 slot의 미관 통과로 확대하지 않는다.

사용자에게 `LNP.Debug.TeleportToOctant 5 3000.3 -2699.5 -3936.8`과 `LNP.Debug.WhereAmI`를 실행한 뒤 걷기/점프·착지/카메라/사격을 확인하도록 요청했다. 이동점은 저장된 Fern base 위치를 현재 debug 명령의 구면 프레임으로 역산한 값이며 명령이 현지 Up으로150cm를 더한다. 장식12개는 NoCollision이므로 장식 자체를 통과하는 것은 실패가 아니고, 지형 접촉과 장식이 판정을 방해하지 않는지를 확인한다. 현재 수동 확인·PIE 종료·임시 선택 복원은 아직 대기다.

사용자가 `LNP.Spawn.EnemyDensity 0` 적용을 위해 PIE를 종료/재시작했다. 새 편성에서 WIP는 slot2/6이고, snapshot은8 validated slots·195 bindings·registry generation2로 게시됐다. seam delta0.000cm·blockedSeamNodes/Edges0, CVar 실효값0·검사용 풀 임시 선택 유지를 다시 확인했다. 첫 실행의 slot5를 재사용하지 않고 **`LNP.Debug.TeleportToOctant 2 3000.3 -2699.5 -3936.8`**을 안내했다. 시드를 강제하거나 사용자 밀도 설정을 에이전트가 바꾼 것은 아니다. 접촉 검증은 이 재시작 후 실행을 기준으로 기록한다.

사용자는 재시작 후 **보행·착지·카메라·사격 모두 문제없음**을 확인했다. `WhereAmI` 로그도 플레이어가 slot2 `LVI_Ridge_SurfaceProbe`, local X3000/Y-2699/H-3821에서 발밑 `SM_Octant_Ridge_00_V011_R30000`에 있음을 보여 준다. 이 수동 검토는1인/EnemyDensity0 조건이며 적 전투·2인 호스트/게스트·전체 slot·패키지 성능의 검증이 아니다.

`CaptureViewport`의 검토 캡처는 별도 PIE 게임창 대신 검은 에디터 뷰포트를 잡아 무효로 분리했다(`CaptureStatus.json`, `Views/pie_review.png`). 게임플레이 통과의 시각 근거로 쓰지 않는다. 사용자에게 게임 콘솔 `Shot`으로 한 장 저장하고 PIE를 종료하도록 요청했으며, 실제 게임 화면 확인·임시 풀 복원은 아직 대기다.

### 화면 확인·복원·마감

사용자의 `Shot`은 `Saved/Screenshots/WindowsEditor/ScreenShot00000.png`로 저장됐다. 플레이어·지각 텍스처·Fern/바위/클로버가 있는 실제 게임 화면임을 확인하고 근거 폴더의 `Views/pie_game_shot.png`로 복사했다. 원본 캡처는 유지한다. 화면의 디버그 표시를 포함한 PIE 캡처이며 성능 판정/무결한 최종 아트 이미지가 아니다.

사용자가 PIE를 종료했고 IsPIERunning=false를 재조회했다. 메모리의 LNPSettings.OctantPool을 원래 `/Game/Maps/OctantPoolData`로 복원·재조회했으며 `SelectionState.json=restored`다. SaveConfig는 호출하지 않았다. TestMap03 dirty=false를 확인한 뒤 원래 WIP 맵과 베이크 전 카메라를 복원했다. **사용자가 설정한 `LNP.Spawn.EnemyDensity=0`은 그대로 유지했다.**

최종 WIP·V005 부모/MI·새 SurfaceData·검사용 풀·정식 풀 모두 dirty=false, PIE=false다. 기존15메시 transform/컴포넌트는 회전 시험 마감 상태와 정확히 같고 보호142파일+WIP/INI 추가3개 확인은 변경0이다. 새 데이터(5,217,284B)/풀(4,029B)2개만 WIP 경로에 저장돼 있으며 모두 Git 제외다. `FinalLiveVerification.json`·`FinalFileChecks.json`·`Verification.json`에 근거를 남겼다.

**1-B4e와 선행1-B4e-1을 제한된1인/EnemyDensity0 조건의 런타임 로드·수동 접촉 검증·복원까지 마감했다.** 정확한 Level/SurfaceData 쌍을 새 베이크로 만들었고 런타임8개 slot snapshot 게시와 사용자 slot2 지형 접촉을 확인했다. 이를2인 호스트/게스트·적 전투/부하·모든 slot의 미관·PCG·패키지 GPU 성능의 통과로 확대하지 않는다.

재현은 생성된 데이터/풀을 확인한 뒤 정상 게임 루트(TestMap03)에서 **검사용 풀을 메모리에서만 임시 선택**하고 수동 PIE로 진행한다. 매 실행의 실제 WIP slot을 로그에서 다시 읽는다. 위5→2 변경처럼 시드/편성이 바뀔 수 있어 예전 slot 번호를 재사용하지 않는다. 종료 뒤 PIE=false와 원래 풀 선택 복원·INI/정식 풀 해시 보존을 반드시 확인한다. WIP/Support 입력이 바뀌면 기존 데이터의 현재성을 다시 확인하고 필요한 범위의 베이크를 별도 판단한다. 현재 산출물이 있으므로 기존 형상/재질 빌더나 이번 베이크를 무조건 재실행하지 않는다.

새 체크아웃의 풀 재생성은 `AssetTools.duplicate`로 `/Game/Maps/OctantPoolData`를 위 WIP 풀 경로에 복제한 뒤, `ObjectTools.get_properties`로 읽은 `octantDefinitions`6개를 복사하고 **원래 Ridge인 정의 한 개의 `levelAsset`/`surfaceData`만 위 WIP/데이터의 전체 object path로 바꾼다.** `allowedSlotRotations`·`seamSignature`·다른5개 정의는 보존하고 파생 풀만 저장한다. 임시 선택은 `ObjectTools.set_properties`의 대상 `/Script/LootNPop.Default__LNPSettings`, 값 `{"octantPool":"/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/DA_GraphicsRidgeValidationPool.DA_GraphicsRidgeValidationPool"}`다. 복원 값은 `{"octantPool":"/Game/Maps/OctantPoolData.OctantPoolData"}`다. SaveConfig를 호출하지 않고 getter와 INI 해시로 선택/비영속성/복원을 확인한다. 실행 전 `SelectionState.json`에 원래 선택을 기록한다.

다음 제안은 별도 진행 확인 후 **1-B5a 전체 bounds 기반 보호 마스크를 포함한 대표 구역 PCG 소수 배치**다. 아직 미승인·미착수이며, 기존 소수 시험을 전체 밀도/확장 승인으로 해석하지 않는다.

### 새 세션 이관(2026-11-16, 사용자 요청)

사용자가1-B5a를 새 세션으로 넘기고 관련 재생성 코드/문서의 단일 커밋을 요청했다. 이번 세션에서 PCG 그래프·배치·보호 마스크를 구현하지 않았다. 다음 세션의 읽기 순서·마지막 자산/풀/카메라 상태·단위별 확인·재생성 금지·복구 근거와 시작 프롬프트는 [Current](../Current.md)를 따른다. 전체 bounds의 회전/스케일/WPO 여유·동선/Pod/훅 보호 조건을 그때 구체화하고 고정 시드의 작은 배치부터 검증한다. 이번 소수 사례를 전체 팔레트/밀도/Phase1 완료로 해석하지 않는다.

## 23. 1-B5a 대표 구역 PCG 소수 배치(2026-11-16, 마감)

사용자가 새 세션에서 1-B5a를 승인했다. 구현 방식은 사용자가 **C++ 커스텀 PCG 노드**를 골랐다. 기존 `ULNPOctantThemeSampler`는 옥탄트 전체 균등 샘플링·구 중심 Up·보호 마스크 없음이고, 엔진 표면 샘플러는 월드 Z-Up 또는 복셀 투영을 전제하므로([TechDesign_WorldGeneration](../../TechDesign_WorldGeneration.md) §6.6) 둘 다 쓰지 않았다. 기존 빌더·정식 Ridge/풀·원본 메시/MI는 수정하지 않았다.

### 구현

- `Source/LootNPop/PCG/LNPDecorationRuleData.h`: 규칙 데이터 에셋. 지각 메시·옥탄트 반지름·구역 다각형·구역 안쪽 여유·이음매 띠·수동 장식 간격·머리 위 개방 높이, 2D 보호 형상(통로/원/다각형, 여유, 지피 허용), 3D 보호 형상(구/캡슐, 지피 허용), 역할(메시·재질·격자 간격·확률·스케일·경사·매몰·WPO 여유·바닥 모서리 뜸/묻힘 허용치·지피 여부·법선 정렬)을 담는다. 수치를 데이터에 두어 재빌드 없이 바꾼다.
- `Source/LootNPop/PCG/LNPDecorationSampler.h/.cpp`: 커스텀 PCG 노드. 옥탄트 로컬 X/Y 지터 격자에서 칸마다 독립 스트림(시드·역할 Id CRC·칸 좌표)을 쓴다. 역할이나 다른 칸을 바꿔도 그 칸의 결과는 유지된다. 구 중심→바깥 지각 trace로 플레이 표면을 찾고 현지 Up·지각 법선으로 정렬한다. **메시 로컬 범위×스케일×회전, 옆/위로 WPO 여유를 더한 상자 전체**로 검사한다. 8모서리 로컬 투영 볼록 껍질로 구역·통로·다각형을, 회전 상자-캡슐 거리로 3D 형상을 본다. 이어 바닥4모서리+중심의 현지 Up 지각 trace(뜸/묻힘), 머리 위 개방(지각 돌출·암반 지붕), 상자 overlap(암반·차단 메시), 수동 장식 AABB 간격, 이음매 띠를 검사한다. 거부 사유 집계는 역할별 로그 `LogLNPDecoration`, 통과 점의 여유값은 PCG 속성(`ZoneClearance`·`Clearance2D/3D`·`CornerFloat/Sink`·`Slope`)으로 남긴다. 레벨 상태를 읽으므로 메인 스레드·비캐시다. 지각이 identity가 아니면(회전된 LevelInstance 안) 오류로 중단한다.
- **엔진 함정:** `UPCGSettings::UseSeed()` 기본 false면 노드·컴포넌트 시드를 무시하고 고정값 42를 쓴다(`PCGSettings.cpp` `GetSeed`). 첫 생성이 이 상태였고 노드에 `UseSeed()=true` override를 추가했다. 실효 시드는 `PCGHelpers::ComputeSeed(노드42, 컴포넌트51001)` = -1241783430이다.
- 그래프: 샘플러 → 엔진 Self Pruning(기본 LargeToSmall, 점 범위에 WPO 여유 포함) → Static Mesh Spawner(By Attribute `MeshPath`, 재질 `MaterialPath`). ISM 디스크립터는 `LNP.Surface.Decoration` 단독·`LNPDecoration`(NoCollision)·Static·내비 영향 false·WPO 평가 true다. PCG가 붙이는 `PCG Generated Component`·`PCG Component` 태그는 수집기가 금지하는 계약 태그가 아니다.
- **PCGVolume 함정:** 브러시 컴포넌트 기본 충돌이 Custom/QueryOnly다. 계약 태그 없는 충돌은 수집기가 거부하고 게임 trace도 맞을 수 있으므로 `place`가 NoCollision으로 바꾼다. 생성 트리거는 GenerateOnDemand라 런타임 재생성이 없다.
- 재생성: `Scripts/Profiling/Graphics/BuildRidgeDecorationPCG.py <rules|graph|place>`. `rules`는 Ridge spec에서 보호 형상을 다시 계산해 `/Game/Graphics/Environment/ElectricDreams/RidgeDecoration_V001/DA_RidgeDecorationRules`를 덮어쓴다. `graph`는 같은 폴더 `PCG_RidgeDecoration`을 디스크에 없을 때만 만든다. `place`는 기존 WIP에 `GraphicsDecorationPCG_V001`(폴더 `Graphics/DecorationPCG_V001`, 컴포넌트 시드51001)을 만들고 생성하며 맵을 저장하지 않는다. 결과 검사는 읽기 전용 `InspectRidgeDecorationPCG.py`다. 에셋/WIP는 Git 제외이고 코드·절차만 추적한다. 새 체크아웃은 §22까지 재현한 뒤 에디터 빌드 → WIP 열기 → `rules`→`graph`→`place` → 육안 확인 뒤 저장 → TestMap03에서 WIP 재베이크 순서다.

### 보호 형상과 여유 근거

메시 전체 범위에 적용하므로 이전 소수 시험의 bounds→spec 계수1.2·경로2m는 쓰지 않는다. 형상은 `RidgeOctantSpec`이 원본이며 구역 밖 형상도 넣고 C++이 판정한다. 2D 49개·3D 40개다.

| 대상 | 형상·값 | 근거 |
|:---|:---|:---|
| 선형 경로(PATHS, 착지부 포함) | 정점 반폭 + 50cm | 플레이어 캡슐 반지름42cm |
| 연결표 보행 중심선(지표 ROUTES) | 반폭200 + 50cm | 캡슐 2개가 나란히 지나가는 폭168cm |
| 지면 문(HallBasinGate 등) | 반폭500 + 50cm | 남쪽 문 폭10m |
| 지면을 자르는 암반·동굴 몸체 외곽 | 엄폐 +100cm, 그 외 +50cm | BasinMonolith 실루엣/판정 보존. 석교·부유섬·섬 위 엄폐(지면을 자르지 않음)는 제외하고 실제 overlap 검사에 맡김 |
| 지표 Pod 3D 편성 구 | 편성 반경, **지피만 허용** | 1-B0 Z3 "낮은 지피·표면 패치만". 사암을 Pod 밖에 두던 소수 시험 결정을 역할 플래그로 유지 |
| 앵커 착지부 / 출발점(지면+170)→앵커 | 구 400cm / 캡슐 100cm | 4m 착지부 / 훅 조준·이동 공간 |
| 벤치마크 카메라 A→B→C→A·지면 검토 시점 | 캡슐/구 150cm | 근거리 장식이 화면을 덮지 않게 함(경로는 선형 보간) |
| 구역 안쪽 / 이음매 / 수동 장식 | 100cm / 성분 ≥5500cm / 30cm | 임시 이진 재질 경계 안 / 기존 이음매 보존 띠 / 관통 방지 |

역할은 근경 통과한 Clover·Sandstone·Moss(MI09)·Fern(보정 MI)만이다. 절벽은 수동 부분 보강 역할이라 넣지 않았다. 경사는 모두 0~30°, 법선 정렬1이다.

| 역할 | 간격(cm)·확률 | 스케일 | 매몰 | WPO 여유 | 뜸/묻힘 허용 |
|:---|:---|:---|---:|---:|:---|
| Clover(지피) | 800·1.0 | 0.85~1.1 | 0.7 | 0 | 8/15 |
| Sandstone | 900·1.0 | 0.9~1.1 | 2.0 | 0 | 10/25 |
| Moss | 1000·1.0 | 0.7~0.9 | 3.0 | 0 | 25/30 |
| Fern | 1000·1.0 | 0.9~1.1 | 1.5 | 30 | 15/25 |

간격·확률은 작은 배치 판정용이며 밀도 승인이 아니다. 첫 값(확률0.3~0.4)은 Clover6·나머지0으로 표본이 부족해 시도 수만 늘렸고 보호 규칙은 완화하지 않았다. Moss의 큰 뜸 허용은 둥근 바위의 AABB 바닥 모서리가 실제 접촉점이 아니기 때문이다. Fern WPO30cm는 여전히 실측 최대 변위가 아닌 시험 초안이다.

### 결과·검증

- 첫 생성(시드42, 확률0.3~0.4)은 Clover6개였다. 계약은 맞았지만 시드 함정과 볼륨 브러시 충돌을 발견해 WIP를 **저장하지 않고 버렸다.** 디스크 WIP와 `BeforePCG.umap` 해시 일치를 확인한 뒤 수정·에디터 재빌드했다.
- 시드51001 생성: 후보/통과는 Clover135/18·Sandstone102/4·Moss87/4·Fern86/1이다. 주요 거부 사유는 경사, 동선·Pod 보호, 지각 없음(대전당 몸체 자리)이다. Self Pruning 뒤 **Clover15·Sandstone2·Moss2·Fern1 = 20개**다. ISM 4개 모두 계약 필드, 재질(MI09·`MI_Ridge_Fern`), 볼륨 NoCollision, 컴포넌트 시드51001·GenerateOnDemand를 재조회했다. Clover 다수는 DeepBasin Pod 안 분지 바닥(지피 허용)이고 사암·이끼·Fern은 Pod 3D 구 밖이다.
- 사용자 근경 판정은 **자연스러움**이었다. 사용자 승인으로 WIP를 저장했다(미저장 변경이 WIP뿐인지 확인).
- TestMap03에서 WIP를 재베이크했다. 수집기 오류0이고 지각85,867 tris·Support2소스/44층·샘플593,257·Support payload4,202,801B·Nav73,836셀/109 components/350 portals·Spawn authored12/candidates9,373/noNavRejected0으로 **§22 베이크와 수치가 모두 같다.** PCG 장식은 지형 데이터에 영향이 없다. 데이터 파일은 재저장으로 해시가 바뀌었고 이전 파일은 복구 사본으로 남겼다.
- 검사용 풀 메모리 임시 선택(SaveConfig 없음, INI 해시 불변)으로 1인 PIE를 했다. EnemyDensity0이고 WIP는 slot2(Y180)·8 validated slots·274 bindings·seam delta0이었다. `TeleportToOctant 2 1150 400 -3998` 뒤 WhereAmI는 local H-3901·발밑 실제 지각이었다. 사용자는 **보행·착지·카메라·사격 이상 없음, 장식이 땅에 자연스럽게 붙어 있음**을 확인했다. PIE 종료 뒤 원래 풀을 복원·재조회했고 INI·정식 풀·저장 WIP 해시를 확인했다(`SelectionState.json=restored`).
- 미검증: 생성 결과의 독립 재검산(노드 로그·속성에 의존), 전체 밀도·원거리 반복, 다른 slot/극점, 2인·적 전투, 게임 조명, 패키지 GPU 비용. Self Pruning은 점 범위 기반이며 바닥 접촉은 AABB 모서리 근사다. 이번 20개를 전체 밀도나 Ridge 확장 승인으로 해석하지 않는다.

근거는 `Saved/Profiling/Graphics/Phase01/RidgeDecorationPCG_20261116/`의 `Rules.json`·`BeforePCG.umap`·`Inspection_Seed42.json`·`Inspection_Seed51001.json`·`BeforeRebake_SurfaceData.uasset`·`SelectionState.json`·`ManualVerdict.json`·`FinalLiveCheck.json`(종료 시 TestMap03·PIE false·dirty0·EnemyDensity0)이다. 시작 대조(WIP·PIE false·dirty0·메시15·V005 바인딩·새 클래스 로드)는 같은 스크립트로 했지만 그 파일은 종료 검사로 덮어썼다. 생성 로그는 에디터 로그의 `LogLNPDecoration`이다.

## 24. 1-B5b 바위 Blocker 승격(2026-11-16, 마감)

1-B5a PIE에서 사용자는 풀·이끼 같은 식생의 NoCollision은 어색하지 않지만, **바위 프랍을 발·사격이 관통하는 것은 꽤 어색하다**고 지적했다. 1-B4e 때도 같은 인상이었다고 했다. GX-013의 "**1차** 장식은 보행 충돌을 바꾸지 않는다"는 첫 장식 범위의 결정이라 새 GX 결정(GX-014)과 이 단위로 바꿨다.

### 충돌만 켜면 안 되는 이유

- **적이 모른다.** 적의 이동·벽보행·접지는 런타임 물리가 아니라 베이크한 SurfaceData(Support/Nav)를 쓴다. 플레이어만 막는 바위는 적이 통과하거나 바위 안에 서게 만든다.
- **계약 위반이다.** Decoration은 NoCollision이 계약이고, 계약 태그 없는 충돌 컴포넌트는 수집기가 거부한다. 충돌을 주려면 Static/Blocker/Support 지형 계약 역할로 승격하고 재베이크해야 한다.
- **판정이 비대칭해진다.** 플레이어 탄만 바위에 막히고 적 시야·사격은 바위를 무시하면 한쪽만 엄폐가 된다.

### 결정(GX-014, 사용자 2026-11-16)

| 항목 | 결정 | 근거 |
|:---|:---|:---|
| 대상 | Moss·Sandstone·Cliff 바위. Clover·Fern은 Decoration 유지 | 사용자: Sandstone(약20cm)도 현재 크기로는 관통이 너무 어색하다 |
| 역할 | 전부 Static+Blocker(`LNPStaticBlocker`). Support는 주지 않는다 | PCG ISM Support는 SSN D-058로 베이크 오류. Blocker 전용 ISM은 수집기·여유 검사·hit registry가 지원 |
| 충돌 형상 | Moss 원본 컨벡스3·Cliff 원본 단순64. Sandstone은 파생 메시가 원본의 생성 컨벡스1개를 쓴다 | 원본 Sandstone은 ComplexAsSimple(렌더7,166 tris). 삼각형 sweep 함정·비용을 피한다. 원본 에셋은 수정하지 않는다 |
| 동선 여유 | 막는 역할만 2D 보호 형상 Margin+50(동선·문 반폭+100cm). 3D 착지부·훅·카메라·Pod는 그대로 | 캡슐 지름84cm를 올림. 암반 외곽에도 같이 적용돼 좁은 틈을 줄인다 |
| SSN Phase 15 | 독립 진행. 재측정 필요 여부는 정식 Ridge 적용 때 판단 | 이 단위는 WIP 전용 LVI만 바꾸며 정식 Ridge·풀은 그대로다 |

### 구현

- C++: 역할 `bTerrainBlocker`·규칙 `BlockerKeepOut2DPadding`을 추가했다. 샘플러는 막는 역할의 2D 여유를 추가 패딩 이상으로 요구하고 점마다 `TerrainBlocker`(0/1)를 남긴다. 승격한 수동 바위는 Decoration 태그가 빠지므로 비계약 표식 `LNP.Graphics.ManualDecoration`으로 기존 30cm 간격 대상에 남긴다. 이전 생성의 PCG 컴포넌트는 trace·overlap에서 무시한다(정리 전 Blocker ISM에 자기 결과가 막히지 않게).
- 그래프 V002(`PCG_RidgeDecoration_V002`): 샘플러 → Self Pruning(계약 분리 전 한 번에) → 속성 필터(`TerrainBlocker` > 상수 Double 0) → Blocker Spawner(Static·Blocker 태그, `LNPStaticBlocker`) / Decoration Spawner(기존 계약). V001은 1-B5a 복구용으로 남긴다.
- 빌더 모드: `blocker-mesh`(파생 Sandstone, `CTF_UseDefault`) · `rules` · `graph` · `place` · `promote`(수동 바위5개: Sandstone_00/01·Moss_00/01·Cliff_02, 모두 확인 뒤에만 변경·transform 보존 검사) · `regenerate` · `repair-graph`. 새 체크아웃 순서는 §22까지 재현 → 에디터 빌드 → WIP 열기 → `blocker-mesh`→`rules`→`graph`→`place`→`promote` → 확인 뒤 WIP 저장 → TestMap03 재베이크다.
- **UE5.8 Python 함정 2개:** (1) `PCGAttributePropertySelectorBlueprintHelpers.set_attribute_name`은 ref 인자를 바꾸지 않는다. 선택자는 `import_text("PCGBegin(TerrainBlocker)PCGEnd")`로 넣는다. (2) ISM 디스크립터 `BodyInstance`의 프로필 이름만 Python으로 바꾸면 충돌 활성·응답이 로드되지 않아 Blocker ISM이 NoCollision으로 생성됐다. 임시 `StaticMeshComponent`에 `set_collision_profile_name`을 호출해 프로필이 적용된 BodyInstance 전체를 복사한다(`repair-graph`로 저장된 그래프도 고쳤다).
- `PrimitiveComponent.line_trace_component`의 Python 반환은 `(hit_location, hit_normal, bone_name, hit_result)`이고 맞지 않으면 None이다.

### 결과·검증

- PCG 시드51001: 샘플러 통과는 Clover18·Sandstone3(이전4)·Moss4·Fern1. 프루닝 뒤 **Clover15·Moss2·Fern1은 1-B5a와 같은 위치**, Sandstone은 2→1(남은 1개는 같은 위치, 빠진 1개는 추가 50cm 여유로 거부). PCG Blocker ISM과 수동 바위5개 모두 `LNPStaticBlocker`·QueryAndPhysics, 식생은 NoCollision이다.
- 사용자 근경(Show Collision): Sandstone·Moss 충돌선은 원래 형상에 아주 근접. Cliff는 선이 어지러워 보여 7.5cm 격자 광선으로 렌더 면·단순 충돌·지각을 비교했다. 보이는 면485개 광선에서 처음 막히는 면과 보이는 면의 차이는 중앙값 −0.1cm, 앞으로 튀어나옴 최대5.9cm(5cm 초과3), 안쪽으로 최대10.5cm 들어간 뒤 단순 충돌이나 뒤쪽 지각이 막는다. 보이지 않는 돌출은 34개 광선에서 최대9.2cm다. 사용자가 **Cliff를 그대로 유지**하기로 했고 WIP를 저장했다.
- TestMap03 재베이크: 수집기 오류0(바위 태그·프로필·응답이 계약과 일치). Support2소스/44층·샘플593,257·payload4,202,801B, Nav component109·portal350, Spawn9,373/noNav0은 같고 **Nav 셀73,836→73,835·Nav payload −8B**, 데이터 5,217,284→5,219,831B다. Blocker는 Support를 만들지 않는다. 간선 비트는 요약에 나오지 않아 PIE DrawNav로 확인했다.
- 1인 PIE①(slot3·EnemyDensity0): 수동·PCG 바위의 발 접촉·옆 막힘·사격 착탄·카메라 이상 없음. PIE②(slot0·EnemyDensity1): DrawNav 반지름3000에서 평지는 노드·간선이 촘촘하고 **바위 주변은 끊겨 있다**. 벽보행 적은 바위 표면에 잘 붙는다. **지상형 적은 발이 바위를 관통한다.** 두 실행 모두 8 validated slots·seam delta0이고, 종료 뒤 원래 풀을 복원했으며 INI·정식 풀·WIP·SurfaceData 해시를 확인했다.
- **DrawNav 함정:** 층 기준 반지름으로 먼 노드를 먼저 거르므로(문턱 = 반지름×1.5+500), 기준 반지름에서 약39m 바깥인 Ridge 분지에서는 반지름1200이 `nodes=0`이다. 3000 이상을 쓴다.

### 알려진 한계 — 지상형 적의 바위 발 관통(SSN 인계)

- 원인: 지상 적 이동(`LNPEnemyExactMovement.cpp` `SweepLateral`)은 캡슐을 `MaxStepUp`45cm 올려 측면 sweep하므로 그보다 낮은 장애물을 턱으로 넘긴다. 이어서 `LNPEnemySurfaceMovement::StepGrounded`가 Support 캐시(지각)로 접지한다. 바위는 Blocker 전용이라 디딜 면이 지각에 남아 발이 바위 안에 놓인다. Sandstone은 전체, Moss(48~62cm)는 대부분이 이 높이 안이다. Nav는 바위를 막힌 곳으로 보지만 근접 추격·분리력으로 바위 위에 올라서면 생긴다.
- 사용자가 별도 SSN 세션에서 다룬다. 권장 순서: ① "Support 없는 Blocker는 넘지 않는다"를 측면 sweep의 불변 조건으로(hit registry 역할 조회, LootPod 충돌 프록시 같은 기존 낮은 Blocker 회귀 확인) → ② 1-B6 확장 전 ISM Support(D-058 해제: 인스턴스별 Support 소스·층 대응 D-037·source binding 키·payload/베이크 시간·Phase 15 측정 조건) 후 역할별로 Sandstone은 Support+Blocker, Moss는 Blocker 유지를 다시 정한다.
- 미검증: 2인 호스트/게스트, 적 시야·사격 엄폐 대칭, 다른 slot의 바위 접촉, 극점/적도, 패키지 GPU·물리 비용.

근거는 `Saved/Profiling/Graphics/Phase01/RidgeRockBlocker_20261116/`의 `Preflight.json`·`BlockerMesh.json`·`Rules.json`·`Promotion.json`·`BeforeBlocker.umap`·`Inspection_V002.json`·`CliffCollisionFit.py/.json`·`SaveWip.json`·`BeforeRebake_SurfaceData.uasset`·`HashesBeforeRebake.txt`·`Rebake.json`·`SelectionState.json=restored`·`PieVerdict.json`이다. 원본/파생 메시·그래프·WIP·SurfaceData는 Git 제외다.
