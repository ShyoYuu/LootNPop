# Surface Support·Navigation 데이터 모델

> 상태: 초안
> 읽기 조건: Octant Definition, 베이크 에셋, handle 또는 node 식별자를 구현·변경할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 에셋과 데이터 모델

아래 이름은 계획 단계의 제안이며 구현 중 프로젝트 명명 규칙에 맞춰 조정할 수 있다.

### Octant 정의 확장

현재 월드 목록만 가진 Octant Pool을 정의 목록으로 확장한다.

```cpp
USTRUCT()
struct FLNPOctantDefinition
{
    TSoftObjectPtr<UWorld> LevelAsset;
    TSoftObjectPtr<ULNPOctantSurfaceData> SurfaceData;
    TSoftObjectPtr<ULNPOctantThemeData> ThemeData;

    FGuid SourceContentHash;
    uint32 DataVersion = 0;

    uint8 AllowedSlotRotations = 0;
    FName SeamSignature;
};
```

런타임 선택 결과는 Level asset만이 아니라 동일한 정의 전체를 보존해야 한다.

### 옥탄트 SurfaceData

```cpp
UCLASS()
class ULNPOctantSurfaceData : public UPrimaryDataAsset
{
    FSurfaceBakeHeader Header;
    TArray<FSupportAtlas> SupportAtlases;
    TArray<FNavLayer> NavLayers;
    FTraversalGraph TraversalGraph;
    FSpawnSurfaceData SpawnData;
};
```

`SurfaceData`와 `NavData`를 별도 asset으로 분리할 수도 있지만 다음 이유로 처음에는 한 asset 안의 별도 스트림을 권장한다.

- 같은 source hash와 bake version을 공유
- Support와 Nav의 불일치를 원천 차단
- 옥탄트 정의 참조 수 감소
- 베이커와 검증 도구 단순화
- BulkData 내부 스트림으로 선택적 로딩 가능

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

