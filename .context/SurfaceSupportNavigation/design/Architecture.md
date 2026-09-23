# Surface Support·Collision·Navigation 아키텍처

> 상태: 기준 설계
> 읽기 조건: 데이터 흐름, 용어, 런타임 초기화 또는 시스템 경계를 변경할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 결론 요약

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

## 용어와 식별자

여러 의미를 모두 “리전”이라고 부르면 설계가 혼동되므로 다음 용어를 분리한다.

### Support Atlas

하나의 방사형 좌표계로 샘플링되는 저장 단위다. 기본 지각 옥탄트, 부유섬 하나, 동굴 바닥 하나가 각각 Support Atlas가 될 수 있다.

### Support Layer

동일한 방사 방향에서 구분되는 걷기 가능한 surface sheet다. Atlas 하나가 Layer 하나를 표현하는 것을 기본으로 한다.

### Nav Layer

같은 2D 파라미터화와 해상도를 사용하는 길찾기 격자다. Support Atlas보다 성긴 해상도를 사용할 수 있다.

### StaticNavComponent와 ReachabilityGroup

도달성은 두 단계로 나눈다.

- **StaticNavComponent**: 정적 Walk 연결만으로 서로 도달 가능한 지상 영역. 옥탄트별로 베이크하고, snapshot 게시 때 이웃 slot과의 지각 이음매 연결로 병합한다. 매치 중 바뀌지 않는다.
- **ReachabilityGroup**: StaticNavComponent에 현재 활성 Traversal Link를 더해 union-find로 묶은 그룹. 상태형 다리나 파괴로 link가 바뀔 때만 다시 계산한다. 컴포넌트 수가 적어 비용이 작다.

두 개념 모두 Support Atlas 경계와 일치할 필요가 없다.

- 다른 Atlas라도 지면이 연속이면 같은 StaticNavComponent가 될 수 있다.
- 같은 mesh 안이라도 절벽으로 끊기면 다른 StaticNavComponent가 될 수 있다.
- 동적 다리가 활성화되면 ReachabilityGroup이 바뀐다.

D-016의 "다른 정적 NavComponent"는 StaticNavComponent를 뜻한다. Pod 재귀속과 추격 가능 판정은 ReachabilityGroup을 사용한다.

### Nav Tile과 Cluster

- Tile: 저장·스트리밍·국소 revision의 최소 공간 단위
- Cluster: 계층형 탐색에서 여러 Tile 또는 셀을 묶는 추상 영역

1차 버전에서는 Tile만 사용하고 Cluster graph는 후속 단계에서 생성한다.

### Traversal Link

두 Nav 영역을 연결하는 명시적 edge다. LootNPop 지상 NPC는 Walk 가능한 연결만 사용한다.

- 동굴 입구
- 옥탄트/Atlas seam
- 자연석 다리
- 고정 구조물
- 완전히 정지한 기둥·기믹 다리

### Dynamic Support

움직이는 패널처럼 NPC가 그 위에 설 수 있으나 AI가 계획적으로 이용하지는 않는 보행면이다. 모든 동적 요소는 LVI의 Placement Marker 위치에 서버가 스폰하는 복제 Actor다(`DynamicTerrain.md`).

### Surface authoring 의미

메시 종류나 collision object type만으로 Support·Blocker를 추론하지 않는다. source `UPrimitiveComponent`는 역할과 수명주기를 직교 Component Tag로 명시하며, 베이커와 validation이 이를 소비한다. 태그 조합, collision profile, 부유섬·동굴 제한은 `TerrainContract.md`가 소유한다.

### Home Anchor

Enemy가 배회하고 세력권을 판단하는 Loot Pod다. 현재 지면·NavComponent와 별개로 관리한다.

---

## 런타임 로딩과 snapshot 게시

### 초기화 순서

기존 `SurfaceBaking` 단계는 점진적으로 다음 의미로 바꾼다.

```text
WorldGeneration
├─ 선택된 Level Instance 로딩
└─ 선택된 SurfaceData 로딩
              ↓
SurfaceDataValidation
├─ DataVersion 확인
├─ definition의 Level과 SurfaceData 짝 확인
├─ slot transform 등록
└─ seam 위험 구역 등록
              ↓
SurfaceSnapshotPublish
├─ 옥탄트 간 지각 이음매 연결
├─ StaticNavComponent 병합
└─ exact hit identity registry 게시
              ↓
DynamicElementSpawn (서버: 마커 스캔·요소 Actor 스폰)
              ↓
EntitySpawning
              ↓
Complete
```

런타임은 source hash를 재계산하지 않는다(D-029). 런타임에는 cooked 패키지만 있어 에디터 source 패키지의 saved hash를 다시 만들 수 없기 때문이다. stale 검출은 cook·CI에서 source manifest와 header hash를 비교해 차단 오류로 처리한다.

Enum 이름 변경으로 네트워크 초기화가 한 번에 깨지는 것을 피하기 위해 첫 전환에서는 기존 `SurfaceBaking` 이름을 유지하고 내부 의미만 로딩으로 바꿀 수 있다. 전체 소비자 전환 후 이름을 정리한다.

### zero-copy 조립

샘플을 slot 회전만큼 복사하지 않는다.

- 선택된 `ULNPOctantSurfaceData`의 immutable payload를 shared reference로 유지
- runtime snapshot에는 slot transform과 Atlas bounds만 저장
- query direction을 inverse slot rotation으로 변환
- 같은 asset이 여러 slot에서 사용되면 payload 공유
- payload는 공유하지만 Runtime Overlay·revision·Conditional Patch 활성 상태는 slot 인스턴스마다 따로 둔다
- 런타임 Layer·Nav Layer 식별자는 slot과 asset 로컬 ID를 조합해 만든다
- payload UObject와 decoded buffer의 lifetime은 snapshot보다 길게 strong reference로 유지한다

### 게시 규약

현재 SurfaceCache의 장점인 immutable read model을 유지한다.

- 게임 스레드에서 모든 asset·transform·index 조립
- 완료 전 Mass 조회는 `NotReady`
- 완료 시 release store로 snapshot 게시
- worker는 acquire 후 read-only 접근
- 진행 중 snapshot payload 수정 금지
- 매치 중 바뀌는 overlay snapshot은 Mass phase 경계의 게임 스레드 지점에서만 교체
- 매치 리셋은 Mass 접근을 중단하는 별도 lifecycle gate 뒤에 수행
- slot→Level Instance/Loaded Level weak reference는 marker 스캔과 hit registry 재구축을 위해 match lifecycle 동안 보존

---

## 실행 경로별 소유권

같은 지형 데이터를 여러 경로가 소비한다. 각 경로가 무엇을 쓰는지 고정한다.

목표 구성에서 적의 90% 이상은 Actor 없이 표현되는 PureEntity이고, ActorPromoted는 소수 엘리트에만 적용한다. 이 구성은 아직 콘텐츠에 적용되지 않았다. 따라서 지형 query 비용의 대부분은 순수 엔티티 경로에서 나온다.

| 경로 | 머신 | 지면·충돌 | Nav | 로드 데이터 |
|:---|:---|:---|:---|:---|
| 순수 엔티티 적 이동 (90% 이상) | 서버 | Support snapshot + worker 동기 exact | 사용 | Support·Nav·Traversal·Spawn |
| Actor 승격 엘리트 이동 | 서버 | Mover(exact) | 경로를 AI 입력으로 전달 | 위와 같음 |
| Actor 적 재시뮬레이션 | 게스트 | Mover(exact) | 사용 안 함 | Support만 필요한지 Phase 5에서 확정 |
| 투사체 판정 | 서버 | worker 동기 exact | 사용 안 함 | 없음 |
| ghost 투사체 | 클라이언트 | worker 동기 exact | 사용 안 함 | 없음 |
| 탄도 가이드 | 로컬 클라이언트 | 게임 스레드 동기 exact. 카메라 노드는 가이드의 착탄점을 읽기만 한다 | 사용 안 함 | 없음 |
| 동적 요소 상태 | 서버 권위 | 결정론적 transform | overlay는 서버 | 마커 |

LOD 전환 규칙:

- Actor → 엔티티: Mover floor hit의 component/shape, face index, ISM instance index를 hit identity registry에서 조회해 `FSurfaceHandle`을 설정한다. component 하나가 여러 Layer를 만들 수 있으므로 component 단독 역방향 매핑은 사용하지 않는다.
- 엔티티 → Actor: 엔티티 위치에 Actor를 스폰하고 Mover가 floor를 다시 찾는다. `FSurfaceHandle`은 유지해 둔다.

ReachabilityGroup을 다시 계산할 때는 active link 집합으로 union-find를 처음부터 재구축한다. link 삭제는 기존 union-find에서 역연산하지 않는다. 결과에는 단조 증가하는 `ConnectivityGraphVersion`을 붙이고 path request·cache·근접 슬롯 판정이 이전 version을 재사용하지 않게 한다.
