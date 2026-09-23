# Surface Support·Navigation 데이터 모델

> 상태: 기준 설계
> 읽기 조건: Octant Definition, 베이크 에셋, handle 또는 node 식별자를 구현·변경할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 에셋과 데이터 모델

아래 이름은 계획 단계의 제안이며 구현 중 프로젝트 명명 규칙에 맞춰 조정할 수 있다.

### Octant 정의

현재 월드 목록만 가진 Octant Pool을 정의 목록으로 점진적으로 확장한다. content migration 전에는 기존 `OctantPool`을 유지하고 새 `OctantDefinitions`를 병존시킨다.

```cpp
USTRUCT(BlueprintType)
struct FLNPOctantDefinition
{
    TSoftObjectPtr<UWorld> LevelAsset;
    TSoftObjectPtr<ULNPOctantSurfaceData> SurfaceData;
    uint8 AllowedSlotRotations = 0xff;
    FName SeamSignature;
};
```

런타임 선택 결과는 Level asset만이 아니라 동일한 정의 전체를 보존해야 한다.

- `AllowedSlotRotations`는 `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)` 순서의 8비트 mask다.
- `SeamSignature`는 서로 조립 가능한 경계 규약의 식별자다. source freshness hash와 섞지 않는다.
- `DataVersion`과 source hash는 `SurfaceData.Header`가 단일 원본으로 소유한다. definition에 복제하지 않는다.
- `ThemeData`는 현재 runtime Level Instance가 직접 소비하지 않으므로 최소 definition에서 제외한다.

#### 기존 Pool 마이그레이션과 선택

`ULNPOctantPoolData`는 호환성 검증이 끝날 때까지 새 `OctantDefinitions`와 legacy `OctantPool`을 함께 저장한다. 새 목록이 하나라도 있으면 그것만 사용한다. 새 목록이 비어 있을 때만 legacy Level 목록을 같은 순서, 전체 slot 허용, 빈 SurfaceData와 seam signature를 가진 임시 definition으로 승격한다. 두 목록을 합치지 않아 중복 선택을 막는다.

runtime 선택은 slot 0부터 7까지 고정 순서로 수행한다.

- Level reference가 유효하고 해당 slot bit가 켜진 definition만 후보로 사용한다.
- pool index 순서로 후보를 만든 뒤 `FRandomStream`의 Fisher-Yates shuffle로 하나를 선택한다.
- slot별 허용 후보로 결정론적 제약 할당을 수행해 현재 batch에서 사용할 수 있는 고유 definition 수를 최대화한다(D-043). slot 순서 greedy는 제한이 강한 definition을 먼저 소비해 불필요한 중복을 만들 수 있으므로 사용하지 않는다.
- 고유 후보를 모두 소진한 뒤에만 새 batch를 시작하므로 8개보다 작은 pool도 지원한다. 같은 최대 해가 여러 개면 pool index 순서와 `FRandomStream`으로 결정론적으로 하나를 선택한다.
- 특정 slot에 유효한 후보가 하나도 없으면 부분 결과를 폐기하고 명시적 오류로 종료한다.
- seed 0을 포함해 같은 seed와 같은 pool 순서에서는 같은 definition/source index 결과를 만든다.

현재 Phase 2 구현은 slot 순서 greedy 기준선이다. production pool에 서로 다른 slot mask를 가진 definition을 둘 이상 넣기 전에 위 제약 할당으로 교체하고, 유효한 고유 배치가 있을 때 중복이 발생하지 않는 자동화를 추가한다.

`ULNPOctantSpawnSubsystem`은 선택한 `FLNPOctantDefinition` 8개를 slot 순서로 보존하고, 그 안의 `LevelAsset`으로 Level Instance를 생성한다. Level Instance 로드 완료 후에는 actor 대기 배열만 비우며 선택된 definition 배열은 이후 SurfaceData 로더가 사용할 수 있도록 유지한다.

### Source hash와 manifest

Asset Registry의 package saved hash는 BLAKE3-160인 `FIoHash`다. 이를 `FGuid`로 자르지 않고 reflected `FLNPContentHash`의 고정 20바이트로 그대로 저장한다.

```cpp
USTRUCT()
struct FLNPOctantSourcePackage
{
    FName PackageName;
    ELNPOctantSourcePackageKind Kind;
    FLNPContentHash PackageSavedHash;
};
```

manifest package는 long package name, kind 순으로 정렬하고 중복을 제거한다. 포함 범위는 다음으로 제한한다.

- source LVI package
- LVI가 직접 참조하는 external actor·external object package
- Terrain Contract 역할 component가 참조하는 Static Mesh package

Component Tag, component transform, collision profile은 package manifest에 억지로 넣지 않고 canonical byte stream의 `SourceSemanticHash`로 저장한다. baker schema version과 품질 설정은 `BakeSettingsHash`로 저장한다. `SourceContentHash`는 정렬된 package manifest와 두 hash를 합친 최종 stale 검출 값이다.

Phase 2 수집기는 Asset Registry의 source LVI **직접 package dependency**만 조회하고, `ULevel::GetExternalObjectsPaths()`가 반환한 해당 LVI 소유 경로 아래의 `__ExternalActors__`·`__ExternalObjects__` package만 허용한다. 재귀 dependency를 순회하지 않는다. 현재 3-kind schema의 `ExternalActor` 값은 두 종류의 UE external package를 함께 나타낸다.

현재 schema는 correctness를 우선해 LVI가 직접 참조하는 owned external package를 역할과 무관하게 모두 manifest에 넣는다. 따라서 decoration external actor의 저장 변경도 stale을 만들 수 있다. "decoration 제외"는 decoration component가 참조하는 mesh/material 하위 dependency를 추가하지 않는다는 뜻이지 그 actor package 자체를 제외한다는 뜻이 아니다. 실제 제작에서 false stale이 빈번하다는 측정이 있을 때만 contributor actor package 필터를 도입한다.

Terrain mesh는 `LNP.Surface.Support` 또는 `LNP.Surface.Blocker` 역할 태그와 정확히 하나의 수명주기 태그를 가진 `UStaticMeshComponent`에서만 수집한다. `Decoration` 단독 component와 태그 없는 component는 Terrain mesh dependency에서 제외하고, `Decoration`과 역할 태그의 혼용이나 수명주기 태그 오류는 수집 실패로 처리한다. Material, PCG graph와 그 하위 dependency는 manifest에 넣지 않는다.

canonical hash byte stream은 domain/version 문자열로 서로 분리하며 다음 규약을 사용한다.

- 문자열과 `FName`: 길이가 앞선 UTF-8 byte sequence
- 정수와 IEEE 754 실수 bit: little-endian
- 실수의 `-0`: `+0`으로 정규화, NaN·Infinity는 거부
- 회전 quaternion: normalize 후 `q`와 `-q` 중 하나로 부호 정규화
- Component Tag: Terrain Contract tag만 정렬·중복 제거
- component semantic row: tag, source-level component transform, collision profile, Static Mesh package 연결을 직렬화한 뒤 row byte 순으로 정렬한다. 같은 의미의 component가 여러 개면 개수도 의미이므로 row 중복은 제거하지 않는다.
- bake setting: 이름과 typed value를 정렬하고 완전히 같은 중복만 제거한다. 같은 이름의 서로 다른 값은 오류다.
- manifest: package name, kind, saved hash 순으로 canonicalize한다. 같은 package name·kind의 서로 다른 saved hash는 오류다.

### 옥탄트 SurfaceData

```cpp
UCLASS()
class ULNPOctantSurfaceData : public UPrimaryDataAsset
{
    FLNPSurfaceBakeHeader Header;
    TArray<uint8> SupportPayload;
    TArray<uint8> NavigationPayload;
    TArray<uint8> TraversalPayload;
    TArray<uint8> SpawnPayload;
};
```

header는 `DataVersion`, source hash·manifest와 각 stream의 element count, uncompressed size, content hash만 가진다. 현재 payload가 일반 `UPROPERTY TArray<uint8>`인 동안에는 asset 로드 시 네 payload도 함께 역직렬화된다. "payload를 읽지 않고 판정"은 codec decode 없이 header만 검사한다는 뜻이며 선택적 I/O를 뜻하지 않는다. 실제 선택 로딩은 측정상 필요할 때 `FByteBulkData` 또는 stream별 asset으로 전환한다.

Phase 2에서는 codec을 고정하지 않고 네 stream을 byte array로 직렬화한다. Support Atlas는 Phase 4, Nav cell은 Phase 7의 실제 베이크 데이터로 layout을 결정한다. Conditional Patch stream은 Phase 8에서 `DataVersion`을 올려 추가한다. 이때 MarkerId·transform·Actor class·상태/경로 파라미터·patch source mesh와 안정 transform을 source semantic/settings hash에 포함하고 모든 production SurfaceData를 재베이크한다(D-041).

규모 추정: 월드 반지름이 250m라 구 전체 면적은 약 0.785km²다. 지각 Nav를 2m 셀로 잡으면 구 전체 약 19.6만 셀, 지각 Support를 100cm로 잡으면 약 78.5만 샘플이다. 다만 이 수치는 sparse index, coverage, Atlas metadata, decoded buffer, 다층 섬·동굴, overlay와 allocator overhead를 제외한 하한 추정이다. Phase 4·5에서 cooked asset 크기와 peak/resident memory를 측정하기 전에는 "10MB 안쪽"을 합격 가정으로 사용하지 않는다. BulkData 선택 로딩이나 client Nav stream 제외는 측정상 이득이 확인될 때만 도입한다.

`SurfaceData`와 `NavData`를 별도 asset으로 분리하지 않고 한 asset 안의 별도 stream으로 두는 이유는 다음과 같다.

- 같은 source hash와 bake version을 공유
- Support와 Nav의 불일치를 원천 차단
- 옥탄트 정의 참조 수 감소
- 베이커와 검증 도구 단순화

### Support sample

개념적으로 다음 정보가 필요하다.

```cpp
struct FSupportSample
{
    uint16 RadiusQ;
    FPackedNormal Normal;
    uint8 Flags;
};
```

플래그 후보:

- Valid
- Walkable
- NearCoverageEdge
- HeightDiscontinuity
- SteepSlope
- NearStaticBlocker
- NeedsExact
- SpawnAllowed

`SurfaceLayerId`는 샘플마다 저장하지 않고 Atlas 단위로 둔다. coverage는 bitset 또는 sparse row로 분리할 수 있다.

정확한 반지름 양자화 범위와 normal packing은 spike 데이터로 오차를 측정한 뒤 확정한다.

### 런타임 Surface Handle

```cpp
struct FSurfaceHandle
{
    uint16 OctantSlot;
    uint16 LocalLayerId;
    uint32 Generation;
};
```

- asset 내부에서는 stable local layer ID를 사용한다.
- 런타임에는 Octant slot과 조합해 전역 식별자를 만든다.
- `Generation`은 매치 리셋이나 snapshot 교체 시 stale handle 검출에 사용한다.

### Nav node reference

```cpp
struct FNavNodeRef
{
    uint16 NavLayerId;
    uint16 TileId;
    uint16 LocalCellIndex;
    uint16 Generation;
};
```

전역 1차원 인덱스를 외부 API에 노출하지 않는다. 이 주소 체계가 일반 Grid A*와 계층형 탐색의 공통 기반이 된다.

`NavLayerId`는 asset 로컬 ID가 아니라 런타임 전역 ID다. 같은 asset이 여러 slot에 들어가므로 snapshot 게시 때 `(slot, 로컬 Nav Layer)`에 전역 ID를 발급한다. `Generation`은 snapshot 교체용 stale 검출이며, 동적 변경을 나타내는 지역 revision과 다르다.

### 옥탄트 이음매 연결

베이크 시점에는 이웃 옥탄트를 알 수 없으므로 이음매 연결은 런타임 snapshot 게시 때 만든다.

- 옥탄트 삼각형의 세 변을 같은 해상도·같은 순서 규약으로 샘플링한다. 이음매 프로필이 기준 반지름의 단일 대칭 프로필이므로(D-030) 모든 변을 같은 규약으로 다룰 수 있다.
- 8개 slot 회전에서 12개 변이 어느 slot의 어느 변과 만나는지, 샘플 순서가 역순인지를 고정 표로 만든다. 회전 집합이 고정이라 이 표도 상수다.
- 게시 때 이 표로 Support 보간 이웃과 Nav 이웃 셀을 연결하고 StaticNavComponent를 병합한다.
- `SeamSignature` 문자열 일치만 신뢰하지 않고 ordered seam sample의 계산 hash와 높이·법선 허용 오차를 함께 검증한다.
- definition 조합 단계에서 signature가 호환되지 않는 후보는 제약 할당에서 제외하고, 실제 계산 seam hash·오차 검증은 SurfaceData 로드 뒤 snapshot 게시 전에 수행한다.

### Nav cell

Nav cell은 위치와 법선을 중복 저장하지 않는다. 해당 셀의 대표 Support 좌표 또는 Support Atlas 매핑만 가진다.

후보 정보:

- Walkable
- 4방향 또는 8방향 edge mask
- Clearance class
- Slope/step class
- Static blocker
- Local connected component
- Representative support coordinate

목표는 셀당 4~8바이트 수준이다. 실제 layout은 베이크된 테스트 데이터로 결정한다.

---
