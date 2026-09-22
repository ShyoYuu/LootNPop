# Mesh Terrain·Surface Support·Mass 월드 충돌·내비게이션 통합 개발 계획

> 보관 상태: 최초 통합 계획 원본. 현재 설계나 진행 상태를 확인할 때는 이 문서를 직접 사용하지 않는다.
> 최신 진입점: `../README.md`
> 보존 목적: 최초 설계 배경과 분할 이전의 전체 문맥을 추적하기 위한 기록

> 상태: 구현 전 계획 확정
> 작성일: 2026-09-21
> 대상 엔진: Unreal Engine 5.8
> 구현 단위: 여러 개발 세션에 걸친 단계적 전환

## 1. 문서 목적

이 문서는 현재의 런타임 `SurfaceCache`를 다음 요구사항을 수용하는 지형·이동 기반으로 교체하기 위한 구현 계획이다.

- Mesh Terrain을 활용한 옥탄트 제작 처리량과 지형 다양성 향상
- 기본 지각과 같은 방사 방향에 겹칠 수 있는 부유섬
- Mass NPC가 실제로 출입하고 추격할 수 있는 단순 동굴
- Mass worker에서 대규모로 사용할 수 있는 저비용 접지 조회
- 기존 Chaos scene query를 이용한 정밀 월드 충돌
- 나무·바위·절벽·동굴을 우회하는 지상 NPC 길찾기
- 추후 계층형 탐색으로 확장 가능한 별도 Nav Grid
- 쓰러진 기둥·기믹 다리처럼 런타임에 새로운 길이 열리는 구조
- 우연히 움직이는 패널에 착지한 NPC의 운반
- 서로 연결되지 않은 부유섬 사이의 원거리 타게팅과 사격
- 장기적으로 추가할 완전 공중형 NPC의 3D 이동

이 문서는 상세 구현 설계의 기준이지만, 구체적인 클래스명·압축 비트 수·격자 해상도처럼 프로파일링과 스파이크가 필요한 값은 확정값으로 간주하지 않는다.

### 관련 문서

- [Idea_Backlog.md](../../Idea_Backlog.md): Mesh Terrain, 동굴, 랜덤 조합 다양성의 최초 검토
- [TechDesign_SurfaceCache.md](../../TechDesign_SurfaceCache.md): 현재 런타임 표면 캐시
- [TechDesign_WorldGeneration.md](../../TechDesign_WorldGeneration.md): 옥탄트 조합과 월드 생성
- [TechDesign_InitSequence.md](../../TechDesign_InitSequence.md): 월드 생성·표면 베이크·Mass 스폰 초기화 순서
- [TechDesign_EnemyNPC.md](../../TechDesign_EnemyNPC.md): 현재 Enemy Mass 이동·타게팅·LOD
- [TechDesign_HitDetection.md](../../TechDesign_HitDetection.md): 투사체와 근접 공격 판정
- [Guide_OctantLevelInstance.md](../../Guide_OctantLevelInstance.md): 옥탄트 레벨 제작 절차

---

## 2. 결론 요약

최종 구조는 하나의 시스템이 모든 충돌과 길찾기를 담당하는 형태가 아니다. 역할을 다음 네 층으로 분리한다.

```text
[에디터 제작]
Mesh Terrain / Modeling Tools / Geometry Script / PCG
                         │
                         ▼
       독립 Static Mesh + 지형 의미 정보
                         │
             Editor Surface Baker
                         │
                         ▼
ULNPOctantSurfaceData
├─ Fine Support Atlases       접지·스폰·충돌 예측
├─ Coarse Tiled Nav Layers    지상 경로 탐색
└─ Traversal Graph            동굴 입구·경계·다리

[런타임]
랜덤 옥탄트 선택
├─ Level Instance 로딩
└─ 사전 베이크 SurfaceData 로딩
             │
             ▼
Immutable Runtime Snapshot ── Mass worker의 저비용 조회
             │ 불확실·정밀 판정
             ▼
MassWorldCollision ────────── 기존 Chaos scene acceleration 사용
             │
             ▼
Runtime Overlay
├─ 파괴된 바닥 무효화
├─ 동적 장애물
├─ 움직이는 Support
└─ 상태형 Traversal Link
```

핵심 원칙은 다음과 같다.

1. 정적 지형의 Support와 Nav 데이터는 옥탄트별로 에디터에서 베이크한다.
2. 런타임 랜덤 조합은 사전 베이크 데이터를 선택·회전·조립하는 과정일 뿐이다.
3. `SurfaceSupportCache`는 충돌체가 아니라 걷기 가능한 지지면의 가속 자료다.
4. 벽·천장·섬 밑면·프랍·동적 지형의 정확한 충돌은 Chaos가 담당한다.
5. 지상 길찾기는 Support Atlas와 다른 해상도의 Tiled Nav Grid를 사용한다.
6. 첫 버전은 일반 A*를 사용하되 주소 체계와 타일 구조는 계층형 탐색을 고려해 설계한다.
7. 지상 NPC의 섬 간 이동은 실제로 연속된 안정적인 보행면이 있을 때만 허용한다.
8. 타게팅과 경로 도달 가능성은 분리한다. 원거리 NPC는 연결되지 않은 섬의 플레이어도 사격할 수 있다.
9. 완전 비행 NPC는 지상 Nav Grid를 공유하지 않고 3D local planner를 사용한다.
10. 독자적인 두 번째 BVH는 초기 범위에 포함하지 않는다.

---

## 3. 확정된 제품·기술 결정

| 주제 | 결정 |
|:---|:---|
| Mesh Terrain | 런타임 월드 시스템보다 에디터 제작 도구로 우선 활용한다. |
| World Partition | 현재 비-WP 옥탄트 LVI 구조는 유지한다. 필요하면 별도 WP 제작 맵에서 산출물을 베이크한다. |
| Surface 베이크 | 머신별 런타임 베이크를 제거하고 옥탄트별 에디터 베이크로 전환한다. |
| 부유섬 | 필수 기능이다. 기본 지각과 같은 방사 방향에 여러 Support Layer가 존재할 수 있다. |
| 동굴 | 도입 시 Mass NPC가 들어갈 수 있어야 한다. 플레이어 전용 동굴은 만들지 않는다. |
| 동굴 범위 | 복잡한 복층, 수직 통로, Y자 분기는 요구하지 않는다. 단일 corridor 또는 단순 출입구 연결을 우선한다. |
| 정밀 충돌 | 기존 Chaos scene query와 cooked collision/BVH를 사용한다. |
| 추가 BVH | scene query가 실제 병목이라는 프로파일 근거가 생기기 전에는 만들지 않는다. |
| 지상 Nav | Support Atlas와 별도 해상도의 Tiled Nav Grid를 사용한다. |
| 계층형 탐색 | 1차 구현 이후 확장한다. 처음부터 stable node reference, tile, portal, 지역 revision을 준비한다. |
| 지상 traversal | `Walk`만 지원한다. 점프·낙하·발사대·훅·텔레포트로 섬을 건너지 않는다. |
| 부유섬 연결 | 자연 지형·다리·완전히 정지한 기둥처럼 실제 보행면이 연결될 때만 추격 가능하다. |
| 움직이는 패널 | NPC가 의도적으로 사용하지 않는다. 우연히 착지한 NPC는 패널과 함께 이동할 수 있다. |
| 파괴 | 바닥·길 제거와 blocker 제거는 지원한다. 임의 파편이 새 보행면을 만드는 기능은 지원하지 않는다. |
| 새로운 길 | 쓰러진 기둥·기믹 다리처럼 정해진 안정 상태가 새 Traversal Link를 열 수 있다. |
| 넉백 후 귀속 | 다른 정적 NavComponent에 착지하면 가장 가까운 도달 가능한 활성 Pod로 재귀속한다. |
| 원거리 타게팅 | Nav 연결 여부와 무관하게 가능하다. 현재 위치에서 시야와 사거리가 확보되면 사격한다. |
| 전술 사격 위치 | 필수 범위에서 제외한다. 플레이테스트 결과가 특별히 불편할 때만 검토한다. |
| 완전 비행 NPC | 지상 Nav와 별도 이동 도메인으로 구현한다. 우선 3D steering/local planner만 사용한다. |
| 투사체 최적화 | 먼저 exact segment query로 정확성 기준선을 만들고, Support 기반 충돌 horizon은 후속 최적화로 둔다. |

---

## 4. 현재 구현과 구조적 한계

### 4.1 현재 SurfaceCache

현재 `ULNPSurfaceCacheSubsystem`은 월드 생성 완료 후 다음 과정을 수행한다.

- 구면을 등장방형 위도·경도 격자로 분할
- 중심에서 바깥쪽으로 `AsyncLineTraceByChannel` 발사
- 방향마다 첫 번째 지표면 위치 하나 저장
- 완료 후 배열을 immutable로 게시
- Mass worker에서 4점 보간으로 `GetSurfacePoint(Direction)` 조회

현재 설정은 약 100cm 간격, 785×1571, 총 1,233,235개 샘플이다. `FVector`가 LWC double인 Win64에서 현재 `FPoint`는 대략 32바이트이므로 순수 샘플 배열만 약 37.6MiB다. 베이크는 머신마다 약 7초가 걸린다.

### 4.2 현재 표현이 깨지는 경우

`방향 → 표면점 하나`는 다음을 표현하지 못한다.

- 기본 지각 위의 부유섬
- 동굴 천장 뒤의 동굴 바닥
- 같은 방향에 겹친 여러 섬
- 절벽 위·아래의 불연속
- 섬의 측벽과 밑면
- 움직이는 패널
- 파괴로 사라진 바닥

해상도를 높여도 층과 불연속의 의미가 생기지 않으므로 근본 해결이 아니다.

### 4.3 깨지는 기존 소비자

전환 대상은 SurfaceCache 자체뿐 아니라 최소 다음 소비자들이다.

- Enemy grounded 이동과 경사 판정
- Enemy 공중 넉백·착지
- Idle 배회 지점 선택
- Loot Pod와 Enemy 초기 스폰
- 투사체 `IsUnderSurface`
- 투사체 탄도 예측과 지면 VFX
- PCG 표면 투영
- World Device 배치와 주변 지형 검사
- 추후 지상 경로 탐색

특히 `Pos.SizeSquared() >= SurfacePoint.SizeSquared()` 형태의 지하 판정은 다층 환경에서 의미가 없다. 최종적으로 제거해야 한다.

---

## 5. 엔진 소스 분석에서 확정된 제약

### 5.1 Mesh Terrain

UE 5.8의 Mesh Partition/Mesh Terrain에는 구 기준 Height Sculpt가 존재하지만 Mesh Partition의 생성·변환 경로는 World Partition을 요구한다. 현재 옥탄트 LVI는 비-WP이므로 다음 두 경로를 스파이크한다.

- C안: 일반 Static Mesh에서 Sphere Height Sculpt 도구만 사용
- B안: 별도 WP 제작 맵에서 Mesh Terrain 편집 후 독립 Static Mesh asset으로 베이크

전면 MegaMesh 런타임 도입은 다음 이유로 우선 제외한다.

- 현재 옥탄트 랜덤 조합과 런타임 회전 모델에 맞지 않음
- compiled section이 월드 좌표 actor 중심으로 생성됨
- 월드 크기에 비해 인프라가 과함
- 실험 기능에 대한 런타임 의존도가 높아짐

Mesh Terrain 결과의 cooked collision은 complex-as-simple 및 double-sided 구성이 가능하므로 정적 지형 exact query에는 적합하다.

### 5.2 `LineTraceMulti`는 모든 blocking surface를 반환하지 않음

언리얼의 multi trace는 overlap hit들과 가장 가까운 blocking hit까지만 반환한다. 첫 blocking hit 뒤의 테스트는 수행하지 않는다.

따라서 기본 지각과 부유섬이 모두 blocking일 때 한 번의 `LineTraceMulti`로 모든 층을 베이크하는 설계는 사용할 수 없다.

에디터 베이커는 다음 중 하나를 사용해야 한다.

- Support source별 독립 트레이스
- 첫 hit 뒤 시작점을 전진시키는 반복 트레이스
- 삼각형의 방사형 아틀라스 직접 rasterization
- 명시적인 Support Proxy Mesh

동굴과 merged Mesh Terrain까지 고려하면 장기적으로 triangle rasterization 또는 명시적 Support Proxy가 가장 안정적이다.

### 5.3 Chaos scene query와 동적 물체

Chaos의 scene acceleration collection은 static, dynamic, query-only body를 포함한다. Movable Static Mesh, kinematic panel, rigid body, Geometry Collection 조각도 collision 설정이 맞으면 일반 `UWorld::LineTrace/Sweep`에 포함된다.

동적 물체가 늘면 별도 BVH가 필요한 것이 아니라 다음 비용이 증가한다.

- top-level broadphase entry 갱신
- 큰 swept bounds
- query candidate 증가
- Geometry Collection 파편 수
- 물리 collision pair
- scene read lock과 physics update 사이의 경합

Mass 쪽은 우선 기존 scene query를 사용하고, Unreal Insights와 전용 통계를 통해 실제 병목을 확인한다.

---

## 6. 용어와 식별자

여러 의미를 모두 “리전”이라고 부르면 설계가 혼동되므로 다음 용어를 분리한다.

### 6.1 Support Atlas

하나의 방사형 좌표계로 샘플링되는 저장 단위다. 기본 지각 옥탄트, 부유섬 하나, 동굴 바닥 하나가 각각 Support Atlas가 될 수 있다.

### 6.2 Support Layer

동일한 방사 방향에서 구분되는 걷기 가능한 surface sheet다. Atlas 하나가 Layer 하나를 표현하는 것을 기본으로 한다.

### 6.3 Nav Layer

같은 2D 파라미터화와 해상도를 사용하는 길찾기 격자다. Support Atlas보다 성긴 해상도를 사용할 수 있다.

### 6.4 NavComponent

현재 활성화된 Walk 연결만으로 서로 도달 가능한 지상 영역이다. Support Atlas 경계와 일치할 필요가 없다.

- 다른 Atlas라도 지면이 연속이면 같은 NavComponent가 될 수 있다.
- 같은 mesh 안이라도 절벽으로 끊기면 다른 NavComponent가 될 수 있다.
- 동적 다리가 활성화되면 도달 가능 관계가 바뀔 수 있다.

### 6.5 Nav Tile과 Cluster

- Tile: 저장·스트리밍·국소 revision의 최소 공간 단위
- Cluster: 계층형 탐색에서 여러 Tile 또는 셀을 묶는 추상 영역

1차 버전에서는 Tile만 사용하고 Cluster graph는 후속 단계에서 생성한다.

### 6.6 Traversal Link

두 Nav 영역을 연결하는 명시적 edge다. LootNPop 지상 NPC는 Walk 가능한 연결만 사용한다.

- 동굴 입구
- 옥탄트/Atlas seam
- 자연석 다리
- 고정 구조물
- 완전히 정지한 기둥·기믹 다리

### 6.7 Dynamic Support

움직이는 패널처럼 NPC가 그 위에 설 수 있으나 AI가 계획적으로 이용하지는 않는 보행면이다.

### 6.8 Home Anchor

Enemy가 배회하고 세력권을 판단하는 Loot Pod다. 현재 지면·NavComponent와 별개로 관리한다.

---

## 7. 에셋과 데이터 모델

아래 이름은 계획 단계의 제안이며 구현 중 프로젝트 명명 규칙에 맞춰 조정할 수 있다.

### 7.1 Octant 정의 확장

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

### 7.2 옥탄트 SurfaceData

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

### 7.3 Support sample

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

### 7.4 런타임 Surface Handle

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

### 7.5 Nav node reference

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

### 7.6 Nav cell

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

## 8. Editor Surface Baker

### 8.1 입력 의미 체계

베이커는 모든 `WorldStatic`을 지면으로 취급하면 안 된다. 최소 다음 의미를 구분한다.

| 의미 | Support 베이크 | Nav blocker | Chaos exact collision |
|:---|:---:|:---:|:---:|
| Terrain Support | 예 | 형상에 따라 | 예 |
| Terrain Wall/Ceiling | 아니오 | 예 | 예 |
| Static Prop Blocker | 아니오 | 예 | 예 |
| Decoration | 아니오 | 아니오 | 보통 아니오 |
| Dynamic Terrain Source | 별도 local patch | runtime overlay | 예 |
| Destructible | 기본 상태만 | runtime invalidation | 예 |

전용 collision profile만으로 베이크 의미를 모두 표현하기 어렵다면 Actor/Component tag, material slot, vertex attribute, 전용 authoring component를 함께 사용한다.

### 8.2 베이크 파이프라인

```text
옥탄트 맵 로딩
    ↓
source geometry·transform·tag 수집
    ↓
walkable triangle 분류
    ↓
연결된 surface sheet/명시적 region 분리
    ↓
Support Atlas rasterization
    ↓
coverage·normal·risk mask 계산
    ↓
성긴 Nav Grid 파생
    ↓
agent radius 기준 obstacle dilation
    ↓
local connected component 분석
    ↓
seam·동굴 입구·정적 다리 portal 생성
    ↓
spawn candidate·clearance 생성
    ↓
source hash·통계·오류 기록
    ↓
ULNPOctantSurfaceData 저장
```

### 8.3 Support Atlas 파라미터화

기본 지각은 등장방형보다 옥탄트 단위 octahedral/삼각 파라미터화를 우선 검토한다.

이유:

- 극점 과밀 제거
- Octant LVI 소유권과 자연스럽게 대응
- slot rotation 적용이 명확함
- 옥탄트 단위 asset과 seam 검증이 쉬움

부유섬과 동굴은 전체 구체 격자를 점유하지 않고 자신의 angular footprint만 가진 sparse Atlas로 저장한다.

### 8.4 해상도 정책

Support 해상도는 지형별로 다르게 둘 수 있다.

- 기본 지각: 100cm 전후에서 시작
- 부유섬: 25~50cm 후보
- 동굴 바닥: 25~50cm 또는 콘텐츠 폭에 맞춤
- 경계와 급격한 곡률 구간: risk 표시 후 exact 폴백

전 구체를 25cm로 만드는 대신 정밀도가 필요한 Atlas에만 고해상도를 사용한다.

### 8.5 보간 규칙

다음 조건을 모두 만족할 때만 일반 보간을 허용한다.

- 동일 Support Atlas
- 필요한 corner sample 전부 valid
- coverage hole 없음
- 높이 차가 연속 지형 허용 범위 안
- normal 변화가 허용 범위 안
- risk/edge 플래그 없음

조건을 만족하지 않으면 nearest sample로 지면을 연장하지 않고 `NeedsExact`를 반환한다. 섬 가장자리 밖에 유령 지면이 생기는 것을 방지한다.

### 8.6 동굴 베이크

동굴은 중심에서 첫 hit만 수집하는 방식으로 만들 수 없다. 외부 내벽·동굴 천장·동굴 바닥이 같은 방향에 겹칠 수 있기 때문이다.

우선순위는 다음과 같다.

1. authoring 단계에서 동굴 바닥 Support Proxy 또는 surface attribute 제공
2. walkable triangle connectivity를 이용한 별도 Support Layer 생성
3. 방사 방향으로 여러 walkable 교차점을 저장

동굴 한 Layer는 다음 규약을 만족해야 한다.

- 해당 Layer 내부에서는 방향당 walkable floor가 최대 하나
- 바닥 법선이 지역 Up과 walkable slope 조건을 만족
- 수직 갱도 없음
- 복잡한 겹침 없음
- 입구는 Exterior Layer와 Walk Portal로 연결

### 8.7 베이크 검증 실패 조건

- source hash 불일치
- 한 Layer가 같은 방향에서 여러 바닥을 생성
- NaN/Inf 좌표 또는 법선
- 반지름 양자화 범위 초과
- 동굴 입구 portal의 capsule clearance 부족
- 옥탄트 seam 높이·법선 오차가 허용값 초과
- NavComponent에 portal 없는 고립 spawn candidate 존재
- SupportData가 참조하는 source actor 누락
- runtime collision과 Support 표면의 오차가 허용값 초과

개발 초기에는 경고로 시작하되 Shipping cook/CI 단계에서는 핵심 항목을 오류로 승격한다.

---

## 9. 런타임 로딩과 snapshot 게시

### 9.1 초기화 순서

기존 `SurfaceBaking` 단계는 점진적으로 다음 의미로 바꾼다.

```text
WorldGeneration
├─ 선택된 Level Instance 로딩
└─ 선택된 SurfaceData 로딩
              ↓
SurfaceDataValidation
├─ asset version 확인
├─ source/content hash 확인
├─ slot transform 등록
└─ seam 위험 구역 등록
              ↓
SurfaceSnapshotPublish
              ↓
EntitySpawning
              ↓
Complete
```

Enum 이름 변경으로 네트워크 초기화가 한 번에 깨지는 것을 피하기 위해 첫 전환에서는 기존 `SurfaceBaking` 이름을 유지하고 내부 의미만 로딩으로 바꿀 수 있다. 전체 소비자 전환 후 이름을 정리한다.

### 9.2 zero-copy 조립

샘플을 slot 회전만큼 복사하지 않는다.

- 선택된 `ULNPOctantSurfaceData`의 immutable payload를 shared reference로 유지
- runtime snapshot에는 slot transform과 Atlas bounds만 저장
- query direction을 inverse slot rotation으로 변환
- 같은 asset이 여러 slot에서 사용되면 payload 공유

### 9.3 게시 규약

현재 SurfaceCache의 장점인 immutable read model을 유지한다.

- 게임 스레드에서 모든 asset·transform·index 조립
- 완료 전 Mass 조회는 `NotReady`
- 완료 시 release store로 snapshot 게시
- worker는 acquire 후 read-only 접근
- 진행 중 snapshot payload 수정 금지
- 매치 리셋은 Mass 접근을 중단하는 별도 lifecycle gate 뒤에 수행

---

## 10. Surface query API

기존 `GetSurfacePoint(Direction)`는 다층 환경에서 제거 대상이다.

개념 API:

```cpp
struct FSupportQuery
{
    FVector WorldPosition;
    FSurfaceHandle PreferredSurface;
    float MaxStepUp;
    float MaxDrop;
    float CapsuleRadius;
};

struct FSupportQueryResult
{
    ESupportQueryStatus Status;
    FVector Point;
    FVector Normal;
    FSurfaceHandle Surface;
    float RadialDelta;
    ESupportRiskFlags RiskFlags;
};
```

상태 후보:

- `HighConfidence`
- `NeedsExact`
- `NoSupport`
- `OutsideCoverage`
- `InvalidatedByRuntimeOverlay`
- `NotReady`

조회 순서:

1. `PreferredSurface`가 유효하면 같은 Layer 우선
2. 현재 방향과 Atlas angular bounds로 후보 축소
3. 현재 반지름·step/drop 규약으로 가능한 Layer 선택
4. 안전한 coverage 내부면 보간
5. edge/risk/invalidated면 exact 요청

SupportCache가 다른 Layer로 임의 스냅해서는 안 된다. Layer 전환은 exact landing 또는 유효한 Walk 연결을 통해서만 일어난다.

---

## 11. MassWorldCollision

### 11.1 초기 구현 원칙

- 기존 `UWorld::LineTrace`·`Sweep` 사용
- worker에서 동기 scene query 사용
- 별도 triangle/BVH 복제 없음
- `AsyncLineTrace`를 Mass worker 경로에서 사용하지 않음
- 결과에서 UObject를 worker가 역참조하지 않음
- 위치·법선·거리·time·blocking 여부 같은 POD 결과만 반환

### 11.2 서브시스템 API

```cpp
RaycastWorld(...)
SweepSphereWorld(...)
SweepCapsuleWorld(...)
ProbeSupport(...)
```

서브시스템은 다음을 캡슐화한다.

- trace/object channel
- query params
- scene query 통계
- dynamic terrain 분류
- debug draw queue
- worker-safe POD result

### 11.3 충돌 프로파일

최소 다음 의미를 분리한다.

- `LNP_MassWorld`: NPC·투사체를 막는 지형과 선택된 프랍
- `LNP_DynamicSupport`: 움직이는 패널·기믹
- Decoration: Mass world query에서 제외
- 작은 파괴 파편: 기본적으로 Mass world query에서 제외

Support 베이크는 collision channel 하나에 의존하지 않고 authoring metadata를 사용한다.

### 11.4 쿼리 분류

| 분류 | 생략 가능 | 용도 |
|:---|:---:|:---|
| ProjectileMandatory | 아니오 | 현재 이동 segment 충돌 |
| AirborneMandatory | 아니오 | 넉백·낙하 착지 |
| GroundRiskFallback | 아니오 | edge·절벽·불연속·동적 overlay |
| DynamicSupportContact | 아니오 | 움직이는 패널 위 접촉 |
| PeriodicGroundValidation | 가능 | 고신뢰 지면의 주기 검증 |
| DebugValidation | 가능 | Support와 Chaos 오차 측정 |

정확성 필수 쿼리와 품질 향상용 쿼리의 예산을 섞지 않는다.

### 11.5 프로파일 결과에 따른 후속 선택

다음 조건이 확인될 때만 custom immutable Chaos snapshot/TLAS를 별도 과제로 검토한다.

- scene read lock 대기가 프레임 병목
- query filter로 줄일 수 없는 broadphase 비용
- Mass worker 동시 query가 physics thread 진행을 반복적으로 방해
- 전용 channel과 query budget 최적화 후에도 목표 프레임을 넘음

메모리와 lifetime 복잡성 때문에 추측만으로 도입하지 않는다.

---

## 12. 동적 지형과 Runtime Overlay

### 12.1 동적 지형 Actor

움직이거나 파괴되는 지형은 별도 Actor 또는 명확한 전용 Component로 둔다.

이유:

- lifetime·복제·권위 주체 명확화
- static SurfaceData에서 제외 가능
- collision profile 분리
- stable dynamic ID 부여
- nav/support invalidation 이벤트 발행

### 12.2 움직이는 패널

움직이는 패널은 경로 탐색 대상이 아니지만 NPC가 우연히 착지할 수 있다.

```text
Airborne
   ↓ exact capsule sweep
DynamicSupport 착지
   ↓
플랫폼 local contact 저장
   ↓
매 프레임 platform transform delta 적용
   ↓
접촉 상실·경사 초과·가장자리 이탈
   ↓
Airborne
```

동적 지형 서브시스템은 worker가 읽을 immutable per-frame snapshot을 게시한다.

```cpp
struct FDynamicSupportSnapshot
{
    FDynamicSupportId Id;
    FTransform PreviousTransform;
    FTransform CurrentTransform;
    FVector LinearVelocity;
    FVector AngularVelocity;
    bool bWalkable;
};
```

NPC는 패널 위에서 상대 이동을 할 수 있지만 AI는 패널을 기다리거나 목적 경로로 선택하지 않는다.

### 12.3 쓰러지는 기둥·기믹 다리

```text
Standing
  - collision 활성
  - bridge traversal 비활성

Transitioning
  - kinematic 이동
  - bridge traversal 비활성
  - 위의 NPC는 DynamicSupport로 운반

Bridge
  - 안정 transform 검증
  - local Support Patch 활성
  - 양쪽 Nav 영역 Walk Link 활성
  - 영향 cluster revision 증가
```

이 구조는 문·내려오는 다리·회전 통로에도 재사용한다.

### 12.4 파괴

지원 범위:

- blocker 제거로 기존 통로 열기
- 바닥 제거로 기존 길 닫기
- 고정 상태 기믹으로 명시적 길 열기
- 임시 장애물로 cell 차단·해제

제외 범위:

- 물리 잔해가 우연히 새 보행면 생성
- 매 프레임 임의 형상 Nav 재생성
- 파편 더미 위 walkability 분석

파괴 AABB와 겹치는 Support/Nav cell을 runtime overlay에서 invalidate한다. 정적 asset은 수정하지 않는다.

### 12.5 지역 revision

전역 revision 하나는 사용하지 않는다.

- Support Atlas 또는 Nav Tile/Cluster별 revision
- 경로가 실제 통과하는 지역 revision만 기록
- 무관한 섬의 변경으로 모든 NPC가 재탐색하지 않음
- 계층형 단계에서는 dirty cluster만 portal 비용 재계산

---

## 13. 지상 Nav Grid

### 13.1 Support Atlas와 분리하는 이유

Support 해상도는 접지 정확도를 위해 결정되고 Nav 해상도는 agent 크기와 통로 폭을 위해 결정된다.

25cm Support Atlas를 2m Nav Grid와 비교하면 같은 면적에서 node 수가 최대 64배 차이 난다. 직접 fine-grid A*는 저장 데이터 중복은 줄이지만 다음 비용이 커진다.

- A* 확장 node 수
- open/closed 작업 메모리
- 병렬 path request의 scratch memory
- 동적 overlay 셀 수
- 과도한 지형 세부에 따른 경로 흔들림

별도 Nav Grid는 위치·법선을 중복 저장하지 않고 Support 참조와 연결 정보만 저장하므로 메모리 증가가 제한적이다.

### 13.2 초기 해상도 후보

- 넓은 야외: 150~200cm
- 부유섬: 100~200cm
- 좁은 동굴: 100~150cm
- 기둥·다리: 명시적 corridor/waypoint

최종 값은 가장 좁게 허용할 통로, NPC 캡슐 지름, 프랍 간격을 기준으로 테스트 맵에서 결정한다.

### 13.3 direct path 우선

모든 NPC가 항상 A*를 요청하지 않는다.

```text
목표까지 Nav/Support direct-path 검사
        │
        ├─ 통과 가능 → 기존 steering
        │
        └─ 막힘 또는 진행 교착
                     ↓
                 Grid A*
                     ↓
              Waypoint steering
                     ↓
          Fine Support + Chaos 검증
```

현재 Idle 배회의 미도달 timeout을 일반적인 stuck/path request 신호로 확장할 수 있다.

### 13.4 일반 A* 1차 구현

첫 버전은 다음 기능에 집중한다.

- Tiled Nav Grid
- stable `FNavNodeRef`
- 4방향 또는 8방향 암묵적 이웃
- slope·step·clearance 기반 edge cost
- bounded path request
- request scratch pool
- waypoint 단순화
- path cache
- runtime overlay 반영
- debug visualization
- path expansion 통계

A* 구현이 raw grid 배열을 직접 참조하지 않도록 graph view API를 둔다.

```cpp
class FLNPNavGraphView
{
    bool IsWalkable(FNavNodeRef Node) const;
    void GetNeighbors(FNavNodeRef Node, FNeighborBuffer& Out) const;
    float GetTraversalCost(FNavNodeRef From, FNavNodeRef To) const;
};
```

### 13.5 경로 공유

다수 Enemy가 같은 플레이어나 Pod를 향하므로 다음 key의 path cache를 우선 검토한다.

```text
(NavComponent, StartTile/Cluster, GoalTile/Cluster, RelevantRevisionSet)
```

초기에는 개별 A* + 결과 cache로 시작한다. 요청 수가 많으면 플레이어나 Pod를 goal로 하는 reverse flow field를 별도 최적화로 검토한다.

---

## 14. 계층형 탐색 확장

### 14.1 포트폴리오 목표

계층형 탐색의 가치는 구현 자체가 아니라 LootNPop의 구면·다층·Mass 규모에 적용하고 단일 A* 대비 개선을 수치로 증명하는 데 있다.

비교 지표:

- 확장 node 수
- P50/P95 path latency
- scratch memory
- 동시 요청 처리량
- 경로 길이 오차
- 동적 변경 재계산 범위
- cache hit rate

### 14.2 Cluster 생성

- 여러 Nav cell/Tile을 Cluster로 묶음
- 경계의 연속 walkable span을 Portal로 압축
- Portal별 clearance 저장
- 같은 Cluster 내부 Portal 간 비용 사전 계산
- 동굴 입구·다리·seam은 고수준 edge로 등록

Cluster 크기와 Portal 압축 규칙은 1차 A* 프로파일을 보고 정한다.

### 14.3 계층형 query

```text
Start/Goal 저수준 node 찾기
        ↓
시작·목표 Cluster 연결
        ↓
고수준 Portal Graph A*
        ↓
Cluster corridor 확정
        ↓
해당 corridor 안에서만 저수준 A*
        ↓
Waypoint 단순화
```

### 14.4 동적 변경

- 변경된 Cluster만 dirty
- 해당 Cluster의 local portal connectivity/cost 재계산
- 재계산 중에는 저수준 A*로 폴백하거나 affected edge 비활성화
- 일반 moving obstacle은 hierarchy를 갱신하지 않고 local collision avoidance로 처리
- 기둥·다리 상태 전환은 명시적 high-level edge toggle로 처리

### 14.5 시각화

에디터와 PIE에서 최소 다음을 그릴 수 있어야 한다.

- Nav cell walkability
- NavComponent 색상
- Tile/Cluster 경계
- Portal span과 대표 node
- 고수준 경로
- 저수준 refinement corridor
- dirty Cluster
- runtime overlay
- 각 query의 expanded node

---

## 15. 지상 NPC 행동 통합

### 15.1 grounded 이동

1. 현재 `FSurfaceHandle`의 Support를 우선 조회
2. high-confidence interior면 cache로 접지
3. edge/risk/discontinuity면 exact floor probe
4. 정적 obstacle risk 또는 runtime blocker가 있으면 capsule sweep
5. 같은 Layer에서 유효 support가 없으면 유령 지면으로 보간하지 않음
6. 실제 낙차가 발생하면 airborne로 전환

### 15.2 airborne와 착지

- `PreviousPosition → ProposedPosition` capsule sweep
- earliest blocking hit 사용
- normal이 walkable이면 착지
- hit 위치에서 가장 가까운 Support Layer를 resolve
- 새 `SurfaceHandle`과 NavComponent 기록
- DynamicSupport면 dynamic contact 상태로 진입
- non-walkable hit면 반사/슬라이드/정지 정책을 별도 설정

현재 반지름 비교 착지는 제거한다.

### 15.3 넉백 후 Pod 재귀속

```text
Airborne
   ↓
착지
   ├─ 같은 정적 NavComponent → 기존 Pod 유지
   ├─ 다른 정적 NavComponent → 재귀속 대기
   └─ DynamicSupport → Displaced 유지

재귀속 대기 후
   ↓
도달 가능한 활성 Pod 후보 조회
   ↓
가장 낮은 path cost의 Pod로 Parent 갱신
```

직선거리만으로 고르면 절벽 건너편 Pod를 선택할 수 있으므로 다음 순서를 사용한다.

1. 활성 Pod만 후보
2. 현재 Nav graph에서 도달 가능한 후보만 유지
3. 직선거리로 소수 후보 축소
4. 실제 또는 근사 path cost로 최종 선택

후보가 없으면 `Orphaned` 상태로 둔다.

- 근처 플레이어가 있으면 현재 위치에서 전투
- 비전투 시 현재 NavComponent 안에서 제한 배회
- 나중에 link가 열리거나 Pod가 생기면 재귀속
- 장시간 고립되고 비가시 상태면 despawn/reinsert를 선택적으로 적용

### 15.4 배회

배회 목표는 Parent Pod 주변의 임의 방향을 즉시 Support에 투영하지 않는다.

- Parent Pod와 같은 reachable Nav 영역의 spawn/wander candidate 사용
- edge clearance 보장
- path 없이 direct 이동 가능한 후보 우선
- 도달 불가능하면 path request 또는 재추첨

### 15.5 타게팅과 도달 가능성 분리

타게팅은 다음을 기준으로 하며 Nav 연결 여부로 후보를 삭제하지 않는다.

- 시야
- 거리
- FOV
- 위협도
- target slot
- Pod 세력권

이후 engagement 단계에서 reachability를 평가한다.

#### 근접 NPC

- 지상 경로 있음: 추격
- 경로 없음: 가장자리로 무작정 돌진하지 않음
- Alert 유지 후 포기 또는 다른 target 선택

#### 원거리 NPC

- 경로 있음: 기존 사거리까지 접근
- 경로 없음 + 현재 위치에서 LoS와 사거리 충족: 사격
- 경로 없음 + 사격 불가: Alert 후 포기

전술 사격 위치 탐색은 구현하지 않는다. 플레이테스트에서 반복적으로 부자연스러운 상황이 확인될 때만 optional backlog로 올린다.

LoS는 SupportCache가 아니라 Chaos exact trace를 사용한다.

---

## 16. 동굴 내 NPC 이동

동굴은 별도 Nav Layer 또는 exterior와 연결된 local layer로 표현한다.

```text
Exterior Nav Layer
        │ Walk Portal
        ▼
Cave Floor Nav Layer
        │
        └─ 단일 corridor / 단순 반대편 출구
```

규칙:

- 입구 바닥이 실제로 연속되면 베이커가 Walk Portal 자동 생성
- 자동 검출이 불안정하면 명시적 Portal Actor로 보정
- 천장·벽은 exact collision 및 Nav blocker
- 동굴 바닥은 별도 Support Layer
- 수직 갱도·복잡한 복층·Y자 분기 제외
- 내부 정적 프랍은 Nav bake에 포함
- 동굴 입구로 들어간 플레이어는 지상 경로가 있으면 NPC가 추격

분기 없는 좁은 동굴에서 Grid가 불필요하게 무거우면 centerline corridor를 보조 데이터로 추가할 수 있으나 첫 구현은 같은 Tiled Nav 구조를 유지한다.

---

## 17. 완전 비행 NPC

비행 NPC는 지상 NPC가 일시적으로 Fly 모드로 전환하는 형태가 아니다. 새·박쥐·드론처럼 처음부터 3D 이동을 전제로 하는 별도 archetype이다.

```cpp
enum class ELNPNavigationDomain : uint8
{
    GroundSupport,
    FreeFlight,
    FlightCorridor
};
```

### 17.1 1차 이동 방식

전역 3D voxel navigation을 만들지 않고 3D local planner를 사용한다.

```text
목표 방향 desired velocity
        ↓
전방 lookahead sphere sweep
        │
        ├─ clear → 그대로 이동
        └─ blocked → 후보 heading 평가
```

후보 방향:

- 좌/우 yaw
- 중심 쪽/지각 쪽 pitch
- 좌상/우상
- 좌하/우하
- 이전 프레임 회피 방향

점수 요소:

- target progress
- obstacle clearance
- turn cost
- preferred radial altitude
- 이전 회피 방향 유지

대부분의 NPC는 전방 sweep 하나만 수행하고 막힌 NPC만 추가 후보를 검사한다.

### 17.2 교착 복구

- 일정 시간 target progress가 없으면 후보 cone 확대
- 반대 radial band 시도
- 짧은 orbit/후퇴
- 그래도 실패하면 home 또는 다른 target 선택

### 17.3 Flight Corridor 도입 조건

다음 요구가 생길 때만 추가한다.

- 박쥐가 긴 동굴 안쪽까지 추격
- 특정 입구를 찾아야 함
- 큰 U자형 장애물을 반드시 우회
- 도달 가능성을 보장해야 함

이 경우 전역 3D grid 대신 동굴 중심선 waypoint/spline을 사용한다.

---

## 18. 투사체 월드 충돌

### 18.1 정확성 기준선

첫 구현은 모든 투사체가 매 프레임 다음을 수행한다.

- `PreviousPos → CurrentPos` world segment/sphere sweep
- Mass target capsule 수학 판정
- world hit time과 entity hit time 비교
- 가장 이른 hit만 채택

이로써 섬 측벽 뒤 적, 동굴 벽, 나무·바위, 움직이는 패널을 올바르게 처리한다.

`IsUnderSurface`와 반지름 기반 착탄 판정은 제거한다.

### 18.2 Support 기반 충돌 horizon

Support 조회가 Chaos query보다 저렴한 것은 맞지만 SupportData는 벽·천장·동적 blocker를 모두 표현하지 않는다. 따라서 horizon은 exact query를 완전히 대체하는 판정으로 사용하지 않는다.

프로파일링 후 선택적 최적화:

1. 탄도 궤적을 저해상도로 Support에 샘플링
2. 정적 walkable surface까지 예상 충돌 시간 계산
3. 임계 시간 안에 들어오면 미래 탄도 구간을 Chaos로 exact pretrace
4. static hit의 시간·점·법선 캐시
5. dynamic blocker와 Mass target은 별도 검사

다음 경우 horizon과 무관하게 exact를 유지한다.

- homing 또는 궤적 변경
- bounce
- 고속탄
- cave/island edge/risk 영역
- dynamic terrain 인접
- Support coverage 밖

1초는 초기 실험값일 뿐이며 projectile speed, lookahead distance, query 비용으로 조정한다.

---

## 19. Spawn·PCG·World Device 전환

### 19.1 Mass Spawn

현재의 임의 방향→단일 표면 투영을 region-aware candidate 방식으로 바꾼다.

- Support Layer별 spawn weight
- slope·edge·capsule clearance
- NavComponent ID
- Pod 허용 여부
- Enemy 허용 여부
- 동굴/부유섬 인구 예산

Pod와 Enemy에 초기 `SurfaceHandle`, `NavNodeRef`, `NavComponent`를 부여한다.

### 19.2 PCG

- 지각과 부유섬을 별도 support source로 처리
- 정적 PCG 프랍은 Nav bake 전에 확정
- 장식 프랍은 Support와 Mass collision에서 제외
- blocker 프랍은 agent radius만큼 Nav occupancy에 반영
- runtime PCG가 walkable terrain 자체를 생성하는 기능은 제외

### 19.3 World Device

중심→외부 첫 hit에 의존하는 배치를 제거한다.

- 허용 Support Layer 명시
- edge·slope·clearance 사용
- 동굴/섬별 배치 정책
- exact collision으로 주변 여유 공간 확인

---

## 20. 단계별 개발 계획

각 단계는 독립적으로 검증·커밋할 수 있어야 한다. 예상 세션 수는 한 세션의 범위에 따라 달라질 수 있으며 일정 약속이 아니라 작업 분할 기준이다.

### Phase 0. 지형 계약과 회귀 테스트 맵

예상: 1세션

산출물:

- 정적/동적 지형 의미 규약
- Support/Atlas/Nav/Component 용어 확정
- collision profile 초안
- 부유섬·동굴 authoring 제한 문서화
- 공통 회귀 테스트 맵

테스트 맵 구성:

- 기본 지각
- 같은 방향의 부유섬 하나와 둘
- 섬 가장자리·측벽·밑면
- 간단한 동굴 입구·천장·바닥
- 나무·바위 blocker
- 쓰러지는 기둥
- 움직이는 패널
- 파괴 가능한 바닥 조각
- 옥탄트 seam

완료 조건:

- 모든 이후 단계가 같은 맵과 좌표 규약을 사용
- 지원하지 않는 지형 형상이 명시됨

### Phase 1. Mesh Terrain 제작 스파이크

예상: 1~2세션

검증:

- 비-WP Static Mesh에서 Sphere Height Sculpt 사용 가능 여부
- WP 제작 맵 Convert·Sculpt·Boolean
- 독립 Static Mesh 산출물 추출
- 기본 지각과 부유섬/동굴 의미 보존
- 기존 비-WP LVI에서 8 slot 회전
- seam 반지름 보존
- Nanite·cooked collision·패키지 빌드
- PCG 입력 회귀

결정 게이트:

- C안 성공: 일반 mesh에 Sphere Sculpt 차용
- C안 실패/B안 성공: WP authoring map→asset bake
- 둘 다 불안정: Mesh Terrain은 제한적으로 사용하고 Modeling Tools/Geometry Script 병행

완료 조건:

- 런타임이 소비할 static mesh/collision/metadata 산출물 계약 확정

### Phase 2. Octant Definition과 베이크 스키마

예상: 1~2세션

- `FLNPOctantDefinition`
- `ULNPOctantSurfaceData`
- stable local Layer/Tile ID
- data version/source hash
- collision/tag 규약
- 기존 OctantPool 호환 또는 변환 도구
- asset validation 뼈대

완료 조건:

- 기존 옥탄트를 새 정의로 로드 가능
- 빈 SurfaceData라도 server/client가 같은 정의를 선택

### Phase 3. MassWorldCollision 정확성 기준선

예상: 2세션

- worker-safe POD query wrapper
- line/sphere/capsule query
- 전용 collision profile
- query count/time 통계
- 투사체 exact segment 판정
- world/entity earliest hit 비교
- 기존 `IsUnderSurface` 제거 시작

완료 조건:

- 회귀 맵의 벽·섬·동굴·프랍·동적 패널을 투사체가 정확히 충돌
- query별 Unreal Insights marker 확보

### Phase 4. Editor Support Baker

예상: 3~5세션

- source 수집
- Support Layer 분리
- sparse Atlas rasterization
- radius/normal packing
- coverage/risk mask
- seam 검증
- 동굴 Support
- debug visualization
- source hash/stale 검출
- 메모리·오차 보고서

완료 조건:

- 기본 지각+부유섬+동굴을 한 asset에 베이크
- 같은 방향에서 올바른 Layer들을 개별 조회
- island edge에 유령 보간 없음
- Support/Chaos 오차 통계 생성

### Phase 5. 런타임 로더와 기존 SurfaceCache 교체

예상: 2~3세션

- selected SurfaceData async load
- slot transform 기반 zero-copy snapshot
- worker-safe query
- 기존 `GetSurfacePoint` 임시 adapter
- InitSequence의 베이크를 로딩/검증으로 교체
- server/client ready gate 유지
- runtime bake 개발용 검증 폴백

완료 조건:

- 정상 실행에서 123만 runtime trace가 발생하지 않음
- 초기 Mass Spawn 전에 snapshot 준비
- client/server가 같은 SurfaceData 조합과 hash 사용

### Phase 6. Enemy 접지·공중·넉백 전환

예상: 3~4세션

- `SurfaceHandle` fragment
- high-confidence cache 접지
- risk exact fallback
- capsule sweep 이동
- exact airborne landing
- Layer/NavComponent 변경
- dynamic support contact
- 다른 Component 착지 후 Pod 재귀속
- Orphaned 정책

완료 조건:

- 부유섬에서 걷고 가장자리에서 정상 낙하
- 기본 지각·부유섬·동굴 중 어느 곳에도 반지름 스냅 매몰 없음
- 과장된 넉백 후 올바른 지면에 착지·재귀속
- 움직이는 패널이 NPC를 운반

### Phase 7. Coarse Tiled Nav Grid와 일반 A*

예상: 3~5세션

- Editor Nav bake
- obstacle dilation
- connected component
- `FNavNodeRef`
- graph view
- direct-path 검사
- 일반 A*
- path request scratch pool
- waypoint following
- path cache
- runtime tile overlay
- 디버그 시각화와 통계

완료 조건:

- 나무·바위·절벽을 제한적으로 우회
- 동굴 출입구를 따라 추격
- 연결된 섬만 추격
- 연결되지 않은 섬의 근접 NPC가 절벽으로 돌진하지 않음

### Phase 8. 상태형 Traversal Link와 파괴 Overlay

예상: 2~3세션

- 자동 seam/cave portal
- 명시적 traversal actor/component
- 기둥 상태 기계
- Bridge 상태 Walk Link
- destroyed floor invalidation
- blocker 제거에 따른 cell/link 갱신
- 지역 revision

완료 조건:

- 기둥이 움직이는 동안 경로로 사용되지 않음
- 완전히 정지하면 양쪽 Nav 영역이 연결
- 길이 열린 후 NPC가 재탐색
- 무관한 지역 NPC는 경로를 버리지 않음

### Phase 9. 부유섬·동굴 Vertical Slice

예상: 2~3세션

- 실제 품질의 옥탄트 하나
- 여러 부유섬
- 단순 동굴
- 정적 프랍
- Pod·Enemy·World Device·PCG 배치
- 근접/원거리 AI
- 넉백·낙하·재귀속
- multiplayer 초기화

완료 조건:

- 설계가 테스트 geometry가 아닌 실제 콘텐츠에서도 성립
- 성능과 메모리 기준선 확정

### Phase 10. 계층형 탐색

예상: 3~5세션

- Cluster partition
- boundary portal 생성·압축
- intra-cluster portal cost
- high-level graph
- hierarchical query
- local refinement
- dirty Cluster 재계산
- 단일 A* 비교 벤치마크

완료 조건:

- 긴 경로에서 node expansion과 P95 latency가 의미 있게 감소
- 경로 품질 저하가 허용 범위
- 동적 변경이 지역적으로만 재계산
- 포트폴리오용 시각화·측정 자료 확보

### Phase 11. 완전 비행 NPC 기반

예상: 2~3세션

- FreeFlight navigation domain
- lookahead sphere sweep
- 3D 후보 heading 평가
- hysteresis와 stuck recovery
- radial altitude band
- 지상 Nav와 독립된 타게팅/이동

완료 조건:

- 열린 공간에서 저비용 이동
- 섬·기둥과 충돌하지 않음
- 단순 장애물에서 좌우 진동 없이 우회

Flight Corridor는 실제 동굴 비행 요구가 생길 때 별도 단계로 추가한다.

### Phase 12. 선택적 투사체 최적화

예상: 1~3세션

- exact query 기준선 분석
- Support 충돌 horizon 실험
- static trajectory pretrace
- projectile 유형별 정책
- query 횟수·latency·정확성 비교

완료 조건:

- 정확성 기준선을 유지하면서 측정 가능한 이득이 있을 때만 채택
- 이득이 없거나 복잡성 대비 가치가 낮으면 exact baseline 유지

---

## 21. 테스트 계획

### 21.1 단위 테스트

- 방향→Atlas coordinate 변환
- slot rotation round trip
- radius quantization 오차
- normal packing 오차
- coverage boundary
- 동일 방향 여러 Layer 선택
- PreferredSurface 우선 규칙
- step/drop 제한
- Nav node encoding/decoding
- tile 경계 neighbor
- local revision 검증
- earliest hit time 비교

### 21.2 에디터 베이크 테스트

- source hash 안정성
- 변경 후 stale 검출
- 같은 asset 반복 베이크 결과 결정론
- seam 오차
- cave floor 분리
- island footprint coverage
- static prop dilation
- connected component
- portal 자동 생성

### 21.3 기능 테스트 맵

| 시나리오 | 기대 결과 |
|:---|:---|
| 같은 방향의 기본 지각+섬 | 현재 Layer를 유지하며 올바른 지면 조회 |
| 섬 가장자리 | 유령 보간 없이 exact/airborne 전환 |
| 겹친 섬 둘 | 위치와 PreferredSurface에 맞는 Layer 선택 |
| 동굴 입구 | 외부↔동굴 Walk 경로 생성 |
| 동굴 천장 | NPC는 바닥 유지, 투사체는 천장 충돌 |
| 벽 뒤 적 | 투사체가 벽을 먼저 맞음 |
| 나무·바위 | direct path 실패 후 우회 |
| 끊긴 부유섬 | 근접 추격 불가, 원거리 사격 가능 |
| 쓰러지는 기둥 | 정지 전 link 없음, 정지 후 경로 생성 |
| 움직이는 패널 | 우연히 착지한 NPC가 함께 이동 |
| 다른 섬으로 넉백 | 착지 후 가까운 reachable Pod로 재귀속 |
| 파괴된 바닥 | cell 무효화 후 NPC가 진입하지 않음 |
| 옥탄트 회전 | 모든 slot에서 Support/Nav가 mesh와 일치 |
| 멀티플레이 | server/client 초기화와 asset 조합 일치 |

### 21.4 성능 측정

현재 기준:

- runtime bake sample: 1,233,235
- runtime bake 시간: 약 7초
- 현재 sample 메모리: 약 37.6MiB

신규 측정:

- SurfaceData asset 크기와 runtime resident memory
- initial data load/validation 시간
- cache high-confidence hit 비율
- exact fallback 비율
- query 종류별 count/time
- physics scene read-lock 대기
- Enemy movement P50/P95
- 일반 A* expanded node/P50/P95
- path cache hit rate
- 동적 변경 dirty cell/Tile/Cluster 수
- 계층형 도입 전후 동일 경로 비교

초기 목표:

- 정상 실행의 runtime surface bake 제거
- 실제 부유섬 콘텐츠를 포함한 SurfaceData resident memory가 현재 약 38MiB 범위 안에 들어오도록 시도
- 정적 smooth interior의 대다수가 exact query 없이 처리
- correctness-mandatory query는 예산 초과를 이유로 생략하지 않음

CPU의 절대 합격값은 목표 플랫폼과 최대 Enemy/Projectile 수의 기준 캡처를 만든 뒤 확정한다.

---

## 22. 마이그레이션 전략

### 22.1 한 번에 모든 소비자를 바꾸지 않음

전환 중에는 새 snapshot 위에 기존 API 호환 adapter를 둘 수 있다.

```text
새 QuerySupport
    ↓
Legacy GetSurfacePoint adapter
```

단, adapter는 기본 지각 Layer만 반환하고 다층 환경에서 사용하면 경고하도록 한다. 새 기능이 legacy API에 의존해 출시되지 않게 한다.

### 22.2 소비자 전환 순서

1. 투사체 exact world collision
2. Editor SupportData와 runtime query
3. Mass Spawn
4. Enemy grounded 이동
5. Enemy airborne/landing
6. Idle 배회
7. PCG/World Device
8. Nav Grid와 path following
9. legacy SurfaceCache 제거

### 22.3 제거 대상

- 머신별 123만 async trace 베이크
- 등장방형 전역 단일 배열
- `GetSurfacePoint(Direction)` 직접 사용
- `IsUnderSurface`
- 반지름 비교 기반 공중 착지
- `ECC_WorldStatic`을 곧바로 지면 의미로 사용하는 코드

---

## 23. 주요 위험과 완화

### Mesh Terrain 실험 기능 의존

- 완화: authoring-only 사용, 독립 Static Mesh 산출물 commit
- 폴백: Modeling Tools/Geometry Script

### stale SurfaceData

- 완화: source hash, version, cook validation

### 옥탄트 seam 불일치

- 완화: seam signature, 자동 비교, seam risk band exact fallback

### 동굴 floor 분리 실패

- 완화: Support Proxy/attribute authoring, triangle connectivity 검사

### Support와 Chaos 불일치

- 완화: 베이크 후 샘플 검증 trace, 오차 통계, high-risk exact fallback

### scene query 경합

- 완화: 전용 channel, 쿼리 분류, interior cache, Insights 계측

### Nav Grid가 좁은 통로 삭제

- 완화: 동굴별 해상도, clearance-aware bake, 명시적 corridor

### 계층형 사전 비용의 동적 무효화

- 완화: cluster-local revision, dirty cluster 재계산, 저수준 폴백

### 움직이는 패널에서 NPC 이탈

- 완화: local contact, transform delta, exact contact 검증, 이탈 시 velocity 상속

### 넉백 후 Pod 부재

- 완화: Orphaned 상태와 비가시 재삽입 정책

### 경로 요청 폭증

- 완화: direct-path 우선, stuck-triggered request, cache, request budget, 추후 hierarchy/flow field

---

## 24. 명시적 비목표

다음은 이 계획의 필수 범위가 아니다.

- Recast NavMesh의 구면 개조
- 전 구체 sparse voxel navigation
- 전 세계 3D SDF를 CPU Mass collision에 사용
- 임의 파괴 잔해가 새 보행면 생성
- 지상 NPC의 점프·발사대·훅·텔레포트 traversal
- 움직이는 패널을 계획적으로 기다리고 탑승하는 AI
- 지상 NPC의 일시적 비행 모드
- 원거리 NPC의 전술 사격 위치 탐색
- 전면 runtime MeshPartition/MegaMesh 전환
- 근거 없는 custom Chaos BVH 복제

---

## 25. 전체 완료 정의

다음 조건을 모두 만족하면 본 계획의 핵심 목표가 완료된 것으로 본다.

- 랜덤 옥탄트 조합에서 runtime surface bake가 없음
- 기본 지각·부유섬·단순 동굴의 SupportData가 에디터에서 베이크됨
- Mass worker가 immutable snapshot을 안전하게 조회
- 부유섬 가장자리와 동굴에서 유령 보간·반지름 매몰이 없음
- 투사체가 world와 Mass target 중 실제 첫 충돌을 선택
- Enemy가 부유섬·동굴에서 접지·낙하·착지
- 다른 NavComponent로 날아간 Enemy가 가까운 reachable Pod로 재귀속
- 나무·바위·절벽을 제한적으로 우회하는 별도 Nav Grid A* 동작
- 실제 Walk 연결이 있는 부유섬만 추격
- 연결되지 않은 섬의 원거리 NPC가 LoS/사거리 조건에서 사격
- 쓰러진 기둥이 정지한 뒤 새 길이 활성화
- 움직이는 패널이 우연히 착지한 NPC를 운반
- 파괴가 기존 길을 열거나 닫을 수 있음
- 일반 A*와 계층형 탐색의 성능 비교 자료 확보
- server/client 초기화와 데이터 버전이 일치

---

## 26. 다음 세션의 권장 범위

다음 세션은 구현 전체를 시작하지 않고 Phase 0~1의 불확실성을 제거하는 데 집중한다.

1. 공통 회귀 테스트 맵 구성
2. 지형 authoring tag/profile 초안 확정
3. Mesh Terrain C안 비-WP Sphere Sculpt 실험
4. B안 WP authoring map→독립 asset 베이크 실험
5. 기본 지각·부유섬·동굴 source 분리 가능성 확인
6. 8개 slot 회전과 seam 검증
7. 결과에 따라 runtime 산출물 계약 확정

이 게이트를 통과한 뒤에만 `FLNPOctantDefinition`과 `ULNPOctantSurfaceData`의 실제 C++ 스키마를 확정한다.
