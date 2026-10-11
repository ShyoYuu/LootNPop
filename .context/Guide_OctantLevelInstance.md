# Octant Level Instance 제작 가이드

2026-10-11 개정. 옥탄트의 설계 원본에서 LVI·표면 데이터·런타임 배치까지 재현하는 절차다. **형상 검사 → 엔진 반입 → 실제 충돌 → 베이크 → PIE**를 따로 통과시킨다.

- 공간 설계와 검토 요청: [레벨 디자인 가이드](Guide_OctantLevelDesign.md)
- 역할 태그·profile·경계의 기준: [TerrainContract](SurfaceSupportNavigation/design/TerrainContract.md)
- 런타임 선택·회전·스폰: [WorldGeneration](TechDesign_WorldGeneration.md)
- 현재 실행 상태: [Current](SurfaceSupportNavigation/Current.md)

## 1. 제작 원본과 도구의 역할

빌더는 `Source/LootNPopEditor/SurfaceNavigation/LNPSliceOctant.cpp`의 spec과 승인된 `Authored` 메시로 LVI를 재생성한다. 재생성 가능하다는 사실이 접합·충돌·이음매의 정상 동작을 보장하지는 않는다. 생성 후 검사와 베이크가 필요하다.

| 구성 | 제작 원본·도구 | 담당 범위 |
|:---|:---|:---|
| 지각·이음매·평탄화 자리 | C++ spec / `BuildSliceOctant` | Meadow 이음매 보존, 내부 구면 높이 |
| 규칙적인 구조물·초기 배치 | C++ spec | 건물·다리·경사로·검토 지점, 반복 가능한 치수 |
| 이어진 암반·굴·복잡한 접합 | Blender 원본 → FBX | 최종 외피, 공유 접합 단면, 내부 면 제거 |
| Pod·앵커·이동 패널 | C++ spec / Placement Marker·`ALNPMassSpawnPoint` | 위치·편성·장치 설정. 동적 요소는 LVI에 직접 배치하지 않는다(D-026) |
| 정식 배경 외형 | 원본 에셋의 작업 사본 / Blender·UE 재질 | 실루엣·UV·표면 표현. 보행 충돌 변경 여부를 명시 |

코드는 배치와 재생성에, Blender는 이어진 형태의 편집과 외피 정리에 쓴다. Boolean Union은 내부 면 제거, Difference는 굴 공간을 파는 후보 방식이다. Boolean·Remesh·단순 Actor 병합 자체를 품질 보증으로 취급하지 않는다. 최종 토폴로지와 실제 충돌을 검사한다.

프로토타입의 기본 형상은 간결한 닫힌 덩어리다. 판·직육면체·쐐기·누운 삼각기둥으로 이동/전투 공간을 만들고, 복잡한 단면·장식 밑동·세밀한 곡면은 필요할 때만 추가한다. 틈이나 함정은 큰 덩어리의 위치·폭·간격을 바꾸고 동선을 조정해 해결하는 것을 우선한다. 작은 보정 메시의 반복 추가를 기본 수리 방식으로 삼지 않는다. 분할 밀도는 실제 보행/충돌·이음매 계약에 필요한 수준으로 정하고 정식 아트의 표면 정밀도를 전면 적용하지 않는다.

제작·반입 절차가 검증된 일괄 재제작은 전체 후보를 먼저 완성하고 생성·베이크·자동 검증을 끝낸 뒤 사용자 PIE를 묶어서 요청할 수 있다. 제작 중 토폴로지·접합 검사와 복구 저장은 유지한다. 반입 절차 검증과최종 조립·충돌 검증을 구분한다. Hollow의2026-10-10 전체 단순화는 이 순서를 사용한다.

**한 지형 구역의 최종 외피 소유자는 하나로 정한다.** 구역별로 코드 part ID, Blender 원본, 대체 대상, 인접 구역의 접합 단면을 기록한다. `Authored`를 추가해도 같은 자리에 있는 `Generated` part는 자동 제거되지 않는다. 승인 시 해당 코드 형상을 생성하지 않도록 바꾸고, 겹친 기존 액터·Support가 없는지 검사한다. LVI만 수동 수정하면 다음 빌드에서 사라진다.

## 2. 폴더·버전·복구

현재 빌더가 사용하는 경로:

```text
/Game/Maps/<Name>/
  LVI_Octant_<Name>
  DA_OctantSurface_<Name>
  SM_Octant_<Name>_R30000
  Generated/SM_<Name>_<Id>_Floor|Shell|Coarse|Deco
  Authored/SM_<Name>_<Id>_<Role>
```

새 제작에서는 아래 원본/작업 공간을 구분한다. 기존 에셋을 이 규칙 때문에 일괄 이동하지 않는다.

| 위치 | 용도 |
|:---|:---|
| `Art/Octants/<Name>/<Region>/` | 수작업한 원본 .blend와 사용 에셋·좌표·역할 기록. 유일한 원본을 Saved에 두지 않는다 |
| `Scripts/Blender/` | 재생성 가능한 모델링·검사·출력 스크립트 |
| `Saved/SliceOctant/<Name>/<Version>/` | FBX·검사 manifest·로그·임시 복구본 |
| `/Game/Maps/_WorkInProgress/OctantWorkflow/<Version>/` | 독립 반입 시험. Git 제외이며 production Authored로 자동 채택되지 않는다 |
| `/Game/Maps/_WorkInProgress/<Name>/` | 구버전 Authored·Generated의 로컬 보관소. 최종 LVI에서 참조하지 않는다 |
| `/Game/Maps/<Name>/Authored` | 통과한 현재 버전만 배치. 빌더는 이 폴더 바로 아래를 비재귀로 스캔한다 |

2026-10-11 사용자 요청으로 Hollow의 미채택 메시와 기존 `OctantWorkflow` 시험편을 `_WorkInProgress` 아래로 이동했다. 최종 Hollow/Bastion의 에셋 경로는 유지한다. `/Content/Maps/_WorkInProgress/`는 `.gitignore`에 등록하며, 새 중간본도 이 위치에 반입한다. 검증 후 최종 경로로 채택하고, 최종 맵이 Git 제외 보관소를 참조하지 않는지 확인한다. 과거 단계 기록에 나오는 이전 경로는 당시 위치다. 이번 이동 전 복구본·경로 대응표·최종 에셋 해시는 `Saved/AssetSeparation/20261011/`에 보존한다.

정리 결과: 실제 메시314개를 UE 에디터 API로 이동·저장하고 미참조 리다이렉터14개를 제거했다. 최종 에셋233개의 SHA256는 변경되지 않았고, 최종 에셋에서 보관소로 향하는 참조와 원래 위치의 중간본은0개다. `HollowContent`·`BastionEncounters`·`ProductionSurfaceDataDefinitions` 자동 검사3개가 오류/경고0으로 통과했다. 세부 결과는 위 복구 폴더의 `postflight.json`·`file-verification.json`·`formal-tests.json`에 있다.

변경 전 관련 코드·원본·LVI·SurfaceData를 저장하고 복구 위치를 기록한다. 외부 에셋 원본은 보존하고 사본을 수정한다. 수작업 원본의 저장소 추적·재배포 가능 범위는 해당 에셋 조건에 맞춰 정한다. 자동 생성 시험편의 .blend는 코드에서 재현할 수 있으므로 Saved 산출물이어도 된다.

같은 이름 재임포트 가능 여부를 도구별로 확인한다. 현재 `StaticMeshTools.import_file`은 신규 이름 반입에 사용한다. 기존 production 에셋을 먼저 지우지 않는다. 새 버전으로 검증한 뒤 UE의 재임포트/참조 교체 절차를 선택하고, 해당 절차의 지원 여부와 복구 방법을 확인한다. **새 버전 파일을 여러 개 Authored 바로 아래에 남기면 모두 배치된다.**

## 3. 지각과 구면 좌표

### 3.1 이음매 보존

`LNPSurfaceDataSubsystem::ValidateSeams`는 이웃 slot의 양자화 반지름과 Valid 비트가 일치해야 snapshot을 게시한다. 새 지각은 Meadow `SM_Octant_Meadow_00_R30000`을 기준으로 만든다.

- `SeamMargin` 기본 15m 안쪽 정점은 바꾸지 않는다.
- 그 바깥 `SeamBand` 기본 40m에서 smoothstep으로 내부 목표 지형으로 연결한다. **Bastion/Hollow 현재 spec은 20m**이므로 항상 해당 spec을 읽는다.
- 내부 fBm 높이와 Pad로 지각을 정리한다. 지각의 방사형 높이 변화만으로 overhang·굴을 만들 수 없다.
- 지각 절단과 봉합은 의도한 입구와 내부 경계만 다룬다. 이음매 보존 띠에 닿는 경계를 임의로 봉합하지 않는다.
- Meadow 원본은 동굴 입구가 이미 절단된 콘텐츠 메시다. 다른 옥탄트로 지각만 복제할 때는 원본 전용 입구 경계를 복구한 뒤 새 옥탄트의 의도된 입구를 절단한다. 지각 전체의 내부 열린 경계를 새 구조물 외피와 별도로 검사한다. Bastion에서는 이 단계가 빠져 Meadow 입구가 외우주 추락 구멍으로 남았다.

세 변의 seam signature를 Meadow 및 이웃 definition과 비교하고 PIE 게시 로그에서 `maxSeamRadiusDelta=0.000cm`를 확인한다. 문서에 복사된 특정 hash만 신뢰하지 않는다. 옛 `BP_OctantGenerator`의 시드만 바꿔 새 지각을 만들면 Meadow와 이음매가 달라질 수 있다.

### 3.2 코드와 Blender가 공유하는 변환

현재 `GenerateSliceOctant.py`와 C++의 좌표는 아래 프레임이다. X/Y는 접평면 파라미터이며 정확한 구면 호 길이·실제 보행 거리가 아니다.

```text
R = 30000 cm
r = normalize( 1,  1, 1)
t = normalize(-1,  1, 0)
b = normalize(-1, -1, 2)
direction = normalize(R*r + X*b + Y*t)
Point(X,Y,H) = direction * (R-H)
UE (x,y,z) cm → Blender (x,-y,z) / 100 m
```

H는 구 중심 쪽으로 높아진다. Blender 전역 +Z를 현지 Up으로 사용하지 않는다. 폭·경사·캡슐 여유는 투영된 최종 형상에서 측정한다. `Authored`는 **옥탄트 원점 피벗·identity 배치**이므로 정점에 옥탄트 로컬 최종 좌표를 담는다. 원점을 모델 중앙으로 옮기면 위치가 틀어진다.

반입 시험에는 비대칭 형상을 써서 축·부호·단위 오류를 잡는다. 적어도 bounds와 삼각형 수를 비교하고, production 접합은 기준점과 공유 경계 정점도 비교한다. bounds 일치만으로 모든 정점·법선·충돌 일치를 보장하지 않는다.

## 4. 형상과 충돌을 만드는 원칙

### 4.1 이어진 외피

경사로·암반·굴 입구가 맞닿으면 한 닫힌 몸체에서 시작하거나 공유 단면으로 접합한다. 단순히 박스를 덧대거나 액터를 합치는 것으로 내부 면이 없어지지 않는다. 평면적인 높이장으로 벽·천장·굴곡을 억지로 표현하지 않는다.

굴은 몸체에서 공간을 빼고 바닥·측벽·천장·입구를 같은 경계로 만든다. 얇은 단면을 양면 재질로 보이게 하는 것은 일방 충돌과 갇힘의 해결책이 아니다. 지면까지 채운 암반 경사로는 윗면뿐 아니라 측면·밑동과 절벽 접합도 포함한다.

구역을 닫힌 외피로 검사한 뒤 Floor/Shell로 나눈다. **나눈 각 역할 메시가 따로 닫힐 필요는 없다.** 두 역할을 합친 표면에 누락·중복이 없고 경계 좌표가 같아야 한다. 구역 경계를 열린 상태로 제작한다면 맞물릴 단면과 예외를 명시하고 조립된 전체에서 폐쇄를 검사한다.

닫힌 외피에서 분리한 열린 역할 조각에 법선 재계산을 다시 적용하지 않는다. Hollow 전체 재제작 V009에서는 이 단계가 바닥 방향을 뒤집어 Pod 아래 Support 보간이 실패했다. 분리 전후 모든 면의 winding·법선 방향 일치를 검사하고, 반입 후 단면 방향의 실제 충돌도 확인한다.

구면 분할은 곡률 오차·통로 폭·착지부로 결정한다. 모든 면을 임의로 1m 이하로 나누지 않는다. 분할과 Boolean 적용 전후의 원본을 보존한다. 정밀도를 늘리기 전에 내부 겹침과 불필요한 바닥을 제거한다.

### 4.2 역할과 실제 판정의 일치

| 접미사 | Component Tag | Profile |
|:---|:---|:---|
| `_Floor` | `Support`·`Blocker`·`Static` | `LNPStaticTerrain` |
| `_Coarse` | 위 + `CoarseSupport`(D-072) | `LNPStaticTerrain` |
| `_Shell` | `Blocker`·`Static` | `LNPStaticBlocker` |
| `_Deco` | `Decoration`만 | `LNPDecoration` |

`_Coarse`는 넓고 완만한 공동 바닥을 100cm로 굽는 명시적 선택이다. 기본 비지각 Support는 25cm다. 촘촘한 구조물을 비용 때문에 무조건 Coarse로 바꾸지 않는다. 역할 접미사가 없으면 빌더 오류다.

자동 Floor 분류는 면 법선과 현지 Up의 내적 ≥0.71을 사용한다. 경사 한계 근처·수직 벽·굴 천장의 분류를 눈으로도 확인한다. 현재 빌더는 **모든 Authored 역할에** `CTF_UseComplexAsSimple`을 검사한다. Deco는 profile로 충돌에서 제외되지만 해당 검사 예외가 구현된 것은 아니다.

실제 보행면은 `Support+Blocker`로 저작한다. Blocker 없는 보행 proxy와 별도 exact geometry를 연계하는 규칙은 현재 없다(D-058). 계단(D-070)처럼 **단순한 실제 경사로 충돌 + 충돌 없는 Decoration 외형**은 가능하다. 별도 UCX/simple collision을 만들었다는 이유만으로 Support 베이크와 런타임이 그것을 동일하게 쓰는 것으로 간주하지 않는다.

외형을 Decoration으로 분리하면 탄환·벽보행을 막지 않는다. 실제 바위 돌출이 이 판정에도 필요하면 Blocker로 저작하고 충돌 소유자를 정한다. 고밀도 아트 전체를 Support로 베이크하거나 같은 면에 중복 Blocker를 남기지 않는다. ISM/HISM Support는 현재 오류이며 Blocker 전용은 허용한다.

### 4.3 전체 폴리곤 단순화의 재사용 기준

단순화는 공간 역할을 큰 닫힌 판·몸체·경로 단면으로 다시 표현하고, 겹친 내부 면과 불필요한 분할/보정 부품을 제거하는 작업이다. 생성된 전체 메시의 일괄 Decimate나 모든 연결체의 한 번 Union을 기본 절차로 삼지 않는다. 규칙적인 구조는 코드로 간결하게 유지할 수 있고, 복잡하게 이어진 접합은 Blender 외피로 정리할 수 있다. 구역별 소유자·교체 목록을 먼저 정하며 독립 섬/건물까지 하나의 연결 성분으로 강제하지 않는다.

곡면을 덜 나누기 전에 최종 구면의 변/면 오차·예상 법선·실제 보행 경사를 함께 확인한다. Floor 높이 띠가 주 착지면·좁은 접합을 자르지 않도록 분리하고, 넓은 공동의 Coarse 해상도를 좁은 문/진입부까지 적용하지 않는다. 지각 절단은 새 외피의 실제 부피와 매입 경계를 맞추며 원래 빈 공간과 이음매는 보존한다.

비용 기준선은 지각/Generated/Authored별 삼각형과 Support/Layers/샘플/payload·Nav/포털 후보/베이크 시간·Spawn·경고를 같은 옵션으로 기록한다. Hollow의 삼각형·포털 베이크 시간 감소는 확인됐지만 Support/Nav payload가 함께 감소한 것은 아니다. 최종 프레임/메모리 개선은 별도 측정으로 판단한다. 재사용할 절차·실패 원인·Bastion 구역별 주의점은 [Bastion 단순화 인계](SurfaceSupportNavigation/phases/Phase09_BastionLowPolyRebuild.md)가 소유한다.

## 5. Blender 제작·출력·반입

1. 장면·단위·선택·원본 저장 상태를 확인한다. 작업 전 저장하고 새 scene/collection에 구역을 만든다. 장면 전체 삭제를 생성 과정에 넣지 않는다.
2. 몸체·경사로·굴 cutter를 편집 가능한 상태로 보존한다. 승인 형상에서 Boolean 또는 직접 토폴로지를 정리하고 내부 면을 없앤다.
3. 투영·분할·삼각분할 후 §7의 형상 검사를 한다. 최종 외피를 Floor/Shell로 나누고 역할별 triangle 수 합계가 원본과 같은지 확인한다.
4. 오브젝트 변환은 identity, 단위는 meter·scale_length=1로 유지한다. 정점에는 §3.2의 좌표를 쓴다. export는 선택한 역할 메시만 대상으로 한다.
5. 현재 시험한 FBX 설정: `apply_scale_options="FBX_SCALE_NONE"`, `global_scale=1.0`, `axis_forward="-Z"`, `axis_up="Y"`, `mesh_smooth_type="FACE"`, `add_leaf_bones=False`, `bake_anim=False`.
6. FBX·manifest·원본 버전을 함께 저장한다. manifest에는 역할·삼각형 수·UE 좌표 bounds·토폴로지 검사·좌표 규약·검사 한계를 담는다.
7. 독립 staging 폴더에 `StaticMeshTools.import_file`로 반입한다. 이번 시험은 `import_materials=false`, `import_textures=false`, `combine_meshes=true`로 역할별 FBX 하나씩 반입했다. UE 재질은 별도로 연결한다. 복잡한 UE 재질 그래프의 FBX 왕복 보존을 가정하지 않는다.
8. `ObjectTools.list_properties`로 실제 필드명을 확인한 뒤 StaticMesh의 `bodySetup` 참조를 얻는다. 해당 객체를 다시 조회하고 `collisionTraceFlag=CTF_UseComplexAsSimple`을 설정·재조회한다. collision LOD와 별도 complex mesh도 확인하고 저장한다. `BodySetup_0` 이름을 임의로 가정하지 않는다.
9. bounds·삼각형 수·외형·법선·실제 충돌을 확인한 뒤 production에 채택한다. 대체한 Generated 형상을 제거하고 전체 접합을 재검증한다.

`Scripts/Blender/GenerateSliceOctant.py`의 MESAS 목록은 현재 Bastion/Hollow 모두 비어 있다. 지금 호출하면 명시적으로 중단하며 장면을 변경하지 않는다. 이 파일만 실행해서 현재 Hollow 외피를 재현할 수 있다는 뜻이 아니다. 기존 메시 생성은 C++ spec이 담당한다.

독립 시험 재현:

```python
from pathlib import Path
namespace = {}
exec(Path(r"<repo>/Scripts/Blender/BuildOctantWorkflowPilot.py").read_text(encoding="utf-8"), namespace)
result = namespace["build_pilot"](r"<repo>", "V003")  # 존재하지 않는 새 버전을 선택한다.
```

버전 scene이나 출력 폴더가 있으면 중단한다. 새 버전을 선택하며 사용자 장면을 삭제하지 않는다. 자동 생성 메사 경로도 빈 spec·기존 collection·이름 충돌에서 중단한다. `build_pilot`은 편집용 Boolean 원본과 구면 출력물을 새 scene에 보존한다. **이 시험편은 Hollow 대체 지형이 아니다.**

## 6. 생성·베이크·풀 등록

1. 코드 수정은 Live Coding 성공을 확인한다. 새 콘솔 명령처럼 등록이 필요한 변경은 별도 전체 빌드 판단이 필요하다. 에디터가 열려 있을 때 전체 빌드와 Live Coding을 동시에 돌리지 않는다.
2. PIE를 끄고 관련 에셋을 저장한다. 대상 LVI 대신 TestMap03 같은 다른 맵을 연다.
3. 생성 전 `LNP.SurfaceNav.LintSliceOctant <Name>`, 생성은 `LNP.SurfaceNav.BuildSliceOctant <Name>`. 생성 로그의 lint=0과 Authored 오류를 확인한다.
4. **생성된 최종 지형의 외형·충돌·단일 소유자를 먼저 검사한다.** 통과 후 `LNP.SurfaceNav.BakeOctant /Game/Maps/<Name>/LVI_Octant_<Name>`.
5. 로그에서 오류·경고, Support sources/Layers/TotalSamples/payload, Nav cells/components/portals, 포털 후보·기각·PortalBakeSeconds, Spawn authored/candidates를 기록한다. 동일 옵션·동일 범위로 전후 비교한다. collect/raster만 보고 전체 베이크가 빠르다고 판단하지 않는다.
6. 해당 옥탄트의 자동 검사와 production definitions 검사를 실행한다. Hollow는 `LootNPop.SurfaceNavigation.Bake.HollowContent`를 사용한다. Bastion 전용 테스트로 Hollow 검사를 대체하지 않는다.
7. `OctantPoolData.OctantDefinitions`의 LevelAsset·SurfaceData·AllowedSlotRotations=255·SeamSignature를 확인·저장한다. 기존 항목을 중복 등록하지 않는다. slot 제한 도입 전 D-043 제약 할당이 필요하다.
8. PIE에서 `LNPSurfaceDataSubsystem: Published generation … validated slots`와 seam delta=0을 확인한다. `LNPOctantSpawnSubsystem: slot N … <- LVI_…`로 해당 옥탄트 slot을 찾는다.
9. 실제 양방향 통행·조준·착지·회복·NPC 교전을 §7대로 검토한다. 기술 통과와 사용자 확인 범위를 인계에 따로 적는다.

현재 MCP에는 프로젝트 콘솔 명령 실행 API가 없다. 생성·베이크가 필요할 때는 AGENTS.md의 수동 조작 규칙에 따라 사용자에게 위 명령 실행을 요청한다. 명령 부재를 메우기 위해 새 자동화 도구를 먼저 만들지 않는다. 한 번의 검토 가능한 변경으로 묶고, 같은 오류의 반복 베이크 요청 전에 원본·최종 형상 단계에서 원인을 해결한다.

검토 이동은 서버 콘솔을 사용한다.

- `LNP.Debug.TeleportToReview [Name] [Slot]`: 인자 없이 검토점/slot 목록.
- `LNP.Debug.TeleportToOctant <Slot> <X> <Y> [H]`: 옥탄트 로컬 좌표로 이동.
- `LNP.Debug.WhereAmI`: slot·LVI·local X/Y/H·발밑 메시. PIE 재시작마다 slot을 다시 확인.

## 7. 단계별 검사와 통과 증거

| 단계 | 필수 확인 | 통과해도 보장하지 않는 것 |
|:---|:---|:---|
| 원본 형상 | 조립 외피의 열린 경계·중복/퇴화 면·면 방향·분리된 내부 덩어리, 의도하지 않은 자기 교차, Floor/Shell 분할 보존 | 엔진 충돌·플레이 |
| UE 반입 | 좌표·단위·피벗·bounds·triangle 수·법선·역할/profile·실제 collision mesh | 접합 전체와 캡슐 통행 |
| 실제 충돌 | 최종 조립 상태에서 통로 전체 폭·양방향·바닥/벽/천장·굴 굽이·낙하 지점 | 카메라 조준·Mover 동작·재미 |
| 베이크 | seam·Support·Nav·Spawn 계약, 비용 전후 비교, 관련 자동 검사 | 훅 조작·NPC 공격 성공 |
| PIE | 자연 진입·왕복 보행·훅 발견/해제/착지·추락 후 자력 복귀·Pod 전투·장치 역할 | 미구현 보스·보상·아트 완료 |

토폴로지는 한 면당 edge 연결 수뿐 아니라 공유 정점·방향·중복 면·내부 면도 본다. manifold 검사만으로 자기 교차가 없다고 선언하지 않는다. 역할별 면 집합의 합을 원본과 비교하고 실제 결합 경계를 확인한다.

Boolean 전에도 모든 원본의 중복·퇴화·방향·연결성을 검사하고 원본blend를 저장한다. 긴 modifier 스택 대신 한 operand씩 확정하며 진행 기록을 남긴다. EastRim 초기 크래시는 잘못된 반복 세분화의 대량 중복 면과 사전 검사 누락을 동반했다. 공유 중점/명시적 삼각형 분할로 고친 뒤 같은 Exact 연산이 완료됐다. 크래시 로그와 마지막 완료 operand는 재시작 전에 보존한다.

부분 교체에서는 같은 절단 평면만 사용해도 단면이 일치하지 않을 수 있다. 기존 코드의 폭 분할·중간 윤곽·삼각형 대각선과 구면 투영을 포함한 최종 교선을 새 외피와 비교하고, 꺾임마다 정점을 공유한다. 넓은 끝면을 단순 사각형으로 바꾸면 독립 메시 검사와 통행이 통과해도 조립 틈으로 내부가 보일 수 있다. BackPocketV015는 남쪽 Departure01 경계만 기존 교선에 맞춘 사례이며 다른 경계의 검증을 대신하지 않는다.

지면까지 채운 경사 몸체의 내부 지각은 지하 밑동의 넓은 평면 윤곽 전체가 아니라 **실제 지면 높이에서의 몸체 부피**로 잘라낸다. 밑동 투영 전체를 제거하면 기울어진 벽과 지각 사이에 노출 틈이 남는다. 최종 Boolean·구면 투영된 외벽과 절단 경계를 비교하고 필요한 매입 깊이를 측정해 지각 끝을 몸체 안에 둔다. V006 동쪽은 정식 경로 검사와 경사 내부 하단 구 진단이 통과했어도 바닥 구멍·벽 통과 후 갇힘이 발생했다. 부피 표본 검사 뒤에도 실제 재생성된 지각과 외벽의 조립·양방향 접근·추락 후 복귀를 확인한다.

바닥과 끝막음의 폭 분할도 일치시킨다. 구면에서 넓은 밑면 한 장과 나뉜 끝막음은 같은 단면을 만들지 않는다. 동쪽Rim 길3개는 두 분할을 맞춰 매립 단면을 닫았다. 경사→평지의 급한 꺾임은 토폴로지가 정상이어도 베이크 캡슐 sweep을 막을 수 있다. EastRimV004는 실제Nav 좌표에서 이를 재현했고V005는 경사 높이 변화를 완만하게 수정했다. BVH 하단 구 진단과 최종 UE 베이크/실제 캡슐 검토는 구분한다.

변 중점의 현 오차만으로 구면 분할을 판정하지 않는다. Hollow 전체 후보V011의 평평한 접합부는 현 오차4cm를 만족하면서 가느다란 두 삼각형의 법선이49~53° 기울었다. 같은 평면 안의 대각선을 정리하고 투영의 미분으로 예상한 법선과 실제 면 법선의 차이가 큰 공유 변만 추가 분할해 V016에서 접합부 최대22.644°를 확인했다. 높이 띠도 산 정상 같은 주 착지 높이와 겹치지 않게 정하며, 좁은 진입부에 본실의 성긴 격자를 그대로 적용하지 않는다. 최종 UE Nav 연결과 캡슐 검사를 함께 통과시킨다.

전체 외피의 소유 경계는 램프와 굴의 최종 윤곽을 감싸야 한다. 기본 육각형 밖까지 Union한 램프를 둔 채 지각만 육각형으로 자르면 경계 검사에 램프 벽·높은 바닥이 들어온다. Hollow V016은 큰 경계를 넓히고 외곽을 지각 아래로 매입했다. 실제 조립 경계 전체를 가로지르는 광선/캡슐로 확인하며, 내부 도로 검사만으로 접합부 통과를 선언하지 않는다.

캡슐은 플레이어 **반지름 42cm·반높이 96cm**를 쓰고 현지 Up에 정렬한다. 중앙 한 줄만 검사하지 않는다. 양 가장자리·굽이의 안/밖·천장 변화·접합 전후와 보고된 낙하 공간을 포함한다. 표면 법선 방향으로 양쪽에서 확인해 일방 통과를 찾는다. 40cm 높여 sweep이 통과한 결과를 실제 Mover 턱 오르기 성공으로 기록하지 않는다.

훅의 조준 광선과 몸체 접근·종료 후 낙하를 따로 검사한다. Hollow V020은 BP의 정지150cm/종료600cm/s와 중력2000cm/s², 무입력 감속200cm/s²·종단2000cm/s를 기준으로60Hz 캡슐 낙하를 예측한다. BVH 축5점 근사는 Chaos sweep과 구분하고, 실제 Mover/입력/착지 후 보행은 PIE로 확인한다. 벽에 밀착한 보고점에서 몸체 접근이 막히면 몇 걸음 물러난 실제 시작점을 검토점으로 명시한다. 원점의 조준선 통과를 원점에서 당기기 성공으로 기록하지 않는다.

근사 접근 판정으로 제외한 합성 방향은 실제 Chaos가 통과시킬 수 있다. V020 원본547낙하 통과 뒤 실제652낙하에서 WestMid5방향 실패를 발견했다. 후보 검색은 광선 접근 방향을 넓게 포함하고, 최종 접근 제외와 낙하 판정은 실제 Chaos로 확인한다. 임시 후보의 통과는 재생성·베이크한 저장 LVI의 정식 검사와 구분한다.

현재 도구로 캡슐 검사가 불가능하거나 직접 플레이가 더 빠르면 AGENTS.md에 따라 대표 구역의 짧은 PIE 검토를 먼저 요청한다. 이때 남은 검증을 명시하며, 검사 장치를 새로 구현하는 데 먼저 시간을 쓰지 않는다. 자동 캡슐 검사 미실시와 사용자 통행 확인을 구분해 기록한다.

LVI 재생성으로 Support 원본 액터 ID가 바뀌면 이전 SurfaceData로는 PIE snapshot 게시가 실패할 수 있다(`missing runtime Support source`). 직접 PIE가 캡슐 검증 수단인 경우에는 형상·반입·배치/충돌 설정을 확인한 뒤 **캡슐·조립 경계 미확인을 명시하고 같은 옵션으로 베이크하여 런타임 참조부터 맞춘다**. 이 선행 베이크를 충돌 통과로 기록하지 않으며, 이어지는 실제 통행에서 오류가 나오면 형상을 수정하고 재생성·재베이크한다.

외형 검토는 지상 시점·위/옆 단면·굴 내부에서 한다. 단색 암반만 보이는 캡처나 내부가 가려진 사진은 통과 증거가 아니다. 장치 검토는 이동 패널의 전체 경로·탑승 여유·인접 모든 가장자리의 점프 우회와 훅 복귀를 포함한다.

패널 폭/길이를 키우면 중심 탑승선뿐 아니라 앞/뒤·좌/우 모서리의 캡슐을 전 경로에서 양방향 검사한다. 고정 자세로 평행 이동하는 판은 구면의 양 끝 지면과 높이가 달라지므로 접안 겹침·높이차도 다시 검사한다. Hollow 장치 V018은400×400→1200×1200cm 확대 후 기존 끝 좌표에서 모서리 간섭을 발견해 접안 위치/높이를 조정했고,9트랙2214회와 네 접안 끝을 검사했다. 이 결과는 실제 Mover 탑승·넉백 중 사용성의 PIE 확인을 대체하지 않는다.

장거리 왕복 판은 양 끝의 현지 중력 수평을 맞추고 중간 자세를 보간한다. 회전한 모서리와 현지 Up으로 탑승 캡슐·접안 끝을 다시 검사하며, 물리 base와 DynamicSupport에 각속도를 함께 전달한다. 열린 경로/닫힌 루프의 자세 규칙을 명시하고 왕복·정차·동일 서버 시각의 자세 재구성을 검증한다. 착지용 훅은 실제 원본/Chaos에서 중심 반대 방향 첫 바닥·낙하 캡슐·발 디딜 여유를 검사한다. Hollow의 `LNP.Grapple.AuthoredOnly` 마커는 해당 LVI에 추가 시드 훅을 금지하며 런처에는 적용하지 않는다.

Pod 주변 250cm 샘플은 walkable·exact 불필요 조건을 만족해야 한다. 이것과 실제 Pod 캡슐 충돌·적 출현 공간·편성 전량 배정·전투 합류를 별도로 검사한다. 현재 지정 벽보행 편성은 바닥 출현이며 벽 스폰을 가정하지 않는다.

반복 수리가 실패하면 **추가 채움 중단 → 해당 접합의 소유 메시 확인 → 구역 외피 재제작 → 작은 반입 시험 → 최종 조립 충돌 → 베이크** 순서로 전환한다. 해상도를 낮추거나 양면 재질을 켜서 균열을 감추지 않는다.

## 8. 치수와 lint의 범위

플레이어 값은 2026-10-07 BP_LNPPlayer/Mover에서 확인한 값이다. 이동 설정이 바뀌면 다시 읽는다.

| 항목 | 현재 값·설계 기준 |
|:---|:---|
| 플레이어 캡슐 | 반지름 42cm·전체 높이 192cm |
| 턱 오르기 / 걷기 경사 | 40cm / 44.8°(cos 0.71) |
| 명목 점프 높이 | 약 140cm(초속 750cm/s·중력 2000cm/s²), 수평 점프 거리와 다름 |
| 벽보행 NPC 이동 캡슐 | 반지름·반높이 80cm, 적 config별 출현 여유는 별도 |
| 문·통로 최소 폭(D-070) | 200cm. 주요 이동 길은 더 넉넉하게 설계 |
| 경사로 옆 통로 / 문 높이 | 300cm 이상 / 250cm 이상 |
| 다리 경사 | 35° 이하를 설계 기준으로 사용 |
| 위아래 이동 여유 | 400cm 기준, 실제 점프·천장·착지는 따로 확인 |
| 벽 | 기존 빌더 닫힌 판 두께 100cm. 복잡한 외피는 실제 두께/토폴로지 검사 |
| 경사로 | 기존 생성기 30°·상단 층계참. 모든 신규 암반 경사에 자동 적용되는 규칙은 아님 |

lint는 `Walkway`·`BridgeSlope`·`BridgeOnStair`·`Clearance`·`SeamBand`·`OffPad`·`DoorHeight` 등의 **해당 spec 항목**을 검사한다. 복잡한 Blender 외피·모든 메시 교차·자기 교차·탈출 가능성을 자동 검증하지 않는다. 경고 형식은 `[SliceOctant] lint <Name> <Rule>: … at local (X,Y,H)`다. lint=0을 구역 완료로 해석하지 않는다.

## 9. 확인된 제작 경로와 다음 적용

### 9.1 독립 시험 — 당시 범위

2026-10-10, Blender 5.2.0 LTS → UE 5.8 독립 시험 V001:

| 항목 | 결과 |
|:---|:---|
| 형상 | 암반 + 붙은 채움 경사로 Union, 굴 Difference, 구면 투영 |
| 토폴로지 | 2908 정점·5812 삼각형·연결 덩어리 1, non-manifold/방향 불일치/중복/퇴화 각각 0 |
| 분할 | Floor 1438 + Shell 4374 = 5812 |
| 구면 근사 | 분할 변 중점의 투영 대 현 오차 최대 0.166cm |
| UE 반입 | 두 역할 triangle 수 일치, bounds 차이 최대 0.0037cm |
| 설정 | 두 메시 complex-as-simple·collision LOD 0 확인/저장, 별도 complex mesh 없음 |
| 장면 보존 | 기존 Cube·Camera·Light 유지. 빈/미등록 메사 spec 호출 시 객체·출력 폴더 불변 |
| 재현성 | V002 재생성의 형상 지표·역할별 bounds가 V001과 동일, 동일 버전 재호출 차단 |
| 미확인 | 자기 교차 검사, UE 캡슐 통행, 최종 지각 접합, 베이크·PIE·아트 에셋 교체 |

산출물은 `Saved/OctantWorkflow/V001/`의 .blend·역할별 FBX·manifest.json·unreal-verification.json·Pilot.png, UE 에셋은 `/Game/Maps/OctantWorkflow/V001`이다. 원본 Boolean을 남겨 재편집할 수 있다. production Authored·Hollow spec·풀 등록은 변경하지 않았다.

이 시험 후 Hollow 국소 접합을 재제작했지만 사용자 검토에서 틈/갇힘이 반복되어 전체 단순화로 전환했다. 위 미확인은 독립 시험 당시 범위이며, 현재 Hollow 제작 미완료를 뜻하지 않는다. 국소 경위는 [외피 재제작 계획](SurfaceSupportNavigation/phases/Phase09_HollowExteriorRebuild.md)에 보존한다.

Bastion의 과거 치수·편성·검토 경위는 [별도 기록](SurfaceSupportNavigation/history/Phase09_BastionGeometryNotes.md)에 보존했다. 다음 옥탄트에 그 값을 그대로 복제하지 않는다.

### 9.2 Hollow 완료와 Bastion 새 세션 인계

[Hollow V017](SurfaceSupportNavigation/phases/Phase09_HollowLowPolyV017.md)은 전체 지형165086 tris·이전 대비63.61% 감소, 생성·베이크·Chaos/데이터 검사와 사용자 전체 외피 탐색을 통과했다. 후속 패널 크기/거리/중력 회전과 훅 역할·관성 착지는 [V019](SurfaceSupportNavigation/phases/Phase09_HollowDevicesV019.md)·[V020](SurfaceSupportNavigation/phases/Phase09_HollowDevicesV020.md)에서 호스트/게스트 및 마지막 변경 구간 PIE까지 통과했다. 지형/장치/훅 제작 완료와 NPC 교전·풀 조합·패키지/부하 Gate는 구분한다.

2026-10-11 사용자는 이 방식을 Bastion에도 적용하도록 새 세션용 문서 갱신을 요청했다. [Bastion 전체 단순화 인계](SurfaceSupportNavigation/phases/Phase09_BastionLowPolyRebuild.md)에 보존할 경험·소유권 제안·기준선/복구·검사/PIE 순서와 실패 교훈을 정리했다. Bastion 실제 재제작은 아직 미착수이며 새 세션은 현재 원본/수동 마커 확인과 기준선 확보부터 시작한다.
