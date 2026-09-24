# 동적 지형과 Runtime Overlay 설계

> 상태: 초안(§1~§3은 Phase 3 구현 기준 설계)
> 읽기 조건: 움직이는 패널, 상태형 길, 파괴, 훅 앵커 같은 배치 요소 또는 지역 revision을 구현할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 1. 동적 요소는 서버가 스폰하는 복제 Actor다

### LVI 내부 Actor를 쓰지 않는 이유

옥탄트 LVI는 서버와 클라이언트가 각자 로컬에서 스폰한다(`LNPOctantSpawnSubsystem`). 엔진은 LVI 내부 레벨의 패키지 이름을 `LevelInstanceID` hash로 만드는데, 로컬 스폰된 `ALevelInstance`는 머신마다 무작위 GUID로 이 ID를 만든다(`LevelInstanceLevelStreaming.cpp` `LoadInstance`, `LevelInstanceActor.cpp` `PostRegisterAllComponents`). 결과적으로 LVI 안의 Actor는 서버와 클라이언트에서 경로가 달라 startup actor로 복제되지 않는다.

Mover는 발밑 base 컴포넌트 참조를 네트워크로 직렬화한다(`MoverDataModelTypes.cpp`, `Ar << MovementBase`). NetGUID가 없는 패널 위에서는 플레이어 예측 이동이 깨진다. 월드 장치를 복제 Actor로 만든 이유와 같다(`../../TechDesign_WorldDevice.md` §2).

복제되는 `ALevelInstance`를 서버가 스폰하는 대안은 엔진이 지원하지만(`LevelInstanceSpawnGuid` 복제), 초기화 순서를 "seed 복제 → 양쪽 결정론적 로드"에서 "LVI 복제 대기"로 바꾸고 정적·동적 수명주기를 한 패키지에 섞는다. 채택하지 않는다.

### 구조

```text
[에디터] LVI 안에 Placement Marker 수동 배치 (MarkerId, 요소 클래스, 로컬 authoring 데이터)
[서버]   레벨 가시화 후 · Complete 이전
         로드된 레벨의 마커 스캔 → 마커 월드 transform에 요소 Actor 스폰 (bReplicates)
         초기 복제 프로퍼티: (slot, MarkerId), 경로·파라미터, 시작 시각
[클라]   마커는 아무 일도 하지 않음 · 복제된 요소 Actor 수신
```

- 마커의 월드 transform에는 slot 회전이 이미 적용돼 있으므로 서버는 그대로 사용한다.
- 스폰 시점은 현재 World Device 배치와 같은 자리다(`ALNPGameMode::OnSurfaceBakingComplete`, `SpawnDevices` 다음).
- 마커 계약은 `TerrainContract.md` §2-1이 소유한다.
- `LNPOctantSpawnSubsystem`은 완료 뒤에도 slot→Level Instance/Loaded Level weak reference를 match lifecycle 동안 보존한다. 월드 전체 Actor 검색이나 회전값 추론으로 slot을 복원하지 않는다.

### 배치 source 2종, 스폰 경로 1개

| source | 용도 |
|:---|:---|
| 수동 마커 | 레벨 디자인 의도가 필요한 패널·기둥·다리·파괴 조각·훅 앵커 |
| seed 기반 절차 배치 | 현재의 훅 앵커·런처 랜덤 배치 |

두 source 모두 같은 서버 스폰 함수(`ULNPDynamicTerrainSubsystem::SpawnPlacedActor`)를 거친다. deferred 스폰으로 초기화 콜백을 FinishSpawning 전에 불러, 거기서 채운 복제 프로퍼티가 초기 스폰 번치에 실린다. 마커 요소는 `ILNPPlacedElement::InitializeFromMarker`를 구현한다. 마커에 "그룹 내 N개 중 M개를 seed로 선택" 같은 규칙을 붙이는 것은 필요해질 때 추가한다.

### 런타임 데이터 원본

서버는 런타임에 마커 Actor를 직접 스캔한다. spline 같은 authoring 데이터를 별도 직렬화 없이 읽을 수 있고, LVI가 아닌 회귀 맵에서도 같은 경로가 동작한다. SurfaceData에는 마커 목록을 넣지 않고, 마커에 딸린 Conditional Patch만 넣는다.

## 2. 결정론적 움직임

동적 요소의 자세는 복제된 경로·시작 시각과 서버 시간의 함수다(D-027).

- 서버는 경로와 시작 시각을 한 번 복제하고, 양쪽이 같은 함수로 자세를 계산한다. 매 프레임 이동을 복제하지 않는다.
- 게임 스레드 kinematic으로 transform을 설정한다. 물리 시뮬레이션으로 구동하지 않는다. 비동기 물리에서 query가 보는 GT data와 실제 자세가 어긋나지 않게 하기 위해서다.
- Mover base 예측이 같은 시각의 같은 자세를 보게 된다.
- 쓰러지는 기둥도 물리 낙하가 아니라 사전 정의된 전이 곡선과 안정 transform을 따른다.
- 경로 revision, 상태, server epoch와 시작 시각을 초기 복제에 포함해 late join도 같은 자세를 재구성한다.
- transform 갱신은 요소 Actor 자신의 틱(TG_PrePhysics)에서 한다. 순서는 다음과 같이 고정한다(2026-09-24 2P 측정).

```text
OnWorldPreActorTick   NPP Mover 시뮬레이션(직전 프레임 자세의 base 위)
TG_PrePhysics         요소 Actor 틱: 자세 갱신 + 물리 속도 설정
                      → Mover base 추종 틱(요소 Actor 틱이 prerequisite): 이동량을 캡슐·sync state에 반영
                      → DynamicSupport snapshot 게시 틱(모든 요소 Actor 틱이 prerequisite)
TG_StartPhysics~      Mass worker exact query
```

  - 시뮬레이션 전(`OnWorldPreActorTick`)에 옮기거나 Actor 틱 없이 옮기면 Mover가 이동량 일부를 놓친다. 측정에서는 절반만 추종했다. Mover의 `AddTickDependency`는 base에 컴포넌트/Actor 틱이 있을 때만 prerequisite를 건다.
  - 현재 worker exact query는 StartPhysics 이후 페이즈에만 있어 TG_PrePhysics의 transform 쓰기와 겹치지 않는다(Gate 0). PrePhysics 페이즈에 exact 소비자를 추가하면 이 배치를 다시 검토한다.
- teleport로 옮긴 kinematic body의 물리 속도는 0이다. Mover는 base를 떠날 때(걸어 나가기·점프) `GetMovementBaseVelocityAtPoint`로 물리 속도를 관성에 더하므로, 매 갱신 뒤 물리 선속도를 결정론적 속도로 설정한다. `ComponentVelocity`도 함께 둔다.
- 경로 revision과 시작 시각을 초기 복제에 포함해 late join도 같은 자세를 재구성한다. server world time이 곧 server epoch다(매치·월드마다 0에서 시작). 움직이는 패널은 상태가 없다. 상태 복제는 상태형 요소(§4)에서 추가한다.
- `ReplicatedMovement`와 결정론적 transform 갱신을 동시에 사용하지 않는다. 큰 server-time 보정이나 상태 revision 불일치는 snap/짧은 보정 중 하나를 명시적으로 적용하고 진단한다.

## 3. 움직이는 패널

움직이는 패널은 경로 탐색 대상이 아니지만 NPC와 플레이어가 우연히 착지할 수 있다.

### 구현 규약(Phase 3, `ALNPMovingPanel`)

- 경로는 마커 루트 스플라인의 위치 곡선(`FInterpCurveVector`, 마커 로컬)을 그대로 초기 복제하고 마커 월드 transform을 원점으로 쓴다. 스폰 번치의 Actor 위치는 복제 시점 위치라 원점으로 쓰지 않는다.
- 양쪽이 같은 곡선에서 등속 거리 표를 만든다. 열린 경로는 양 끝에서 정지하며 왕복하고, 닫힌 루프는 순환한다. 회전하지 않고 마커 회전을 유지한다. 마커 scale은 무시하고 경고한다.
- world collision envelope에는 등록 시점 자세 대신 경로 swept 반지름(원점에서 가장 먼 경로 샘플 + 최대 샘플 간격 + 메시 bounding sphere)을 넣는다.
- hit identity에는 `(slot, MarkerId)`가 실린다.
- `bAlwaysRelevant`다. 멀리 있는 패널에도 클라이언트 Ghost 투사체·탄도 가이드가 맞아야 하기 때문이다.

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

순수 엔티티는 동적 지형 서브시스템이 게시하는 per-frame snapshot을 읽는다. Actor 경로는 Mover의 movement base를 사용한다.

```cpp
struct FDynamicSupportSnapshot
{
    FDynamicSupportId Id;      // (slot, MarkerId)
    FTransform PreviousTransform;
    FTransform CurrentTransform;
    FVector LinearVelocity;
    FVector AngularVelocity;
    bool bWalkable;
};
```

NPC는 패널 위에서 상대 이동을 할 수 있지만 AI는 패널을 기다리거나 목적 경로로 선택하지 않는다.

## 4. 쓰러지는 기둥·기믹 다리

```text
Standing
  - collision 활성
  - bridge traversal 비활성

Transitioning
  - 결정론적 kinematic 이동
  - bridge traversal 비활성
  - 위의 NPC는 DynamicSupport로 운반

Bridge
  - 안정 transform 도달
  - 해당 마커의 Conditional Patch 활성
  - 양쪽 Nav 영역 Walk Link 활성
  - 영향 tile 지역 revision 증가
```

이 구조는 문·내려오는 다리·회전 통로에도 재사용한다.

## 5. Conditional Patch

정적 payload는 `Dynamic`·`StatefulTraversal`·`Destructible` source를 포함하지 않는다(`TerrainContract.md`). 따라서 이런 요소가 여는 보행면·점유·link는 무효화가 아니라 **추가**로 표현해야 하고, runtime bake를 하지 않으려면 그 추가분을 미리 베이크해야 한다(D-028).

- 베이커는 배치된 마커의 patch source mesh를 해당 상태 transform(기둥은 안정 transform, 파괴 바닥은 파괴 전 상태)에 놓고 base Atlas·Nav Tile 좌표계에 투영한다. 런타임에 임의의 로컬 Nav Grid를 회전·병합하지 않는다(D-041).
- patch는 base sample/cell에 대한 support·occupancy 활성화 bitset, 추가 node가 꼭 필요할 때의 명시적 overlay node, 그리고 base node와의 명시적 Walk edge를 가진다.
- patch는 `MarkerId`를 키로 옥탄트 SurfaceData에 저장된다. stream 추가 방식은 Phase 8에서 `DataVersion`을 올려 확정한다.
- 런타임 overlay는 `(slot, MarkerId)`의 patch를 켜고 끄며 영향 tile의 revision을 증가시킨다.
- 여러 patch가 같은 cell에 겹치면 blocker occupancy는 reference count/활성 source 집합으로 합성하고 하나가 꺼져도 다른 blocker를 지우지 않는다. Support 추가는 stable patch surface ID로 구분하며 서로 다른 sheet를 반지름만으로 병합하지 않는다.
- patch 경계 edge, capsule clearance, base surface와의 높이·법선 오차를 베이크 차단 조건으로 검증한다.

| 요소 | patch 활성 조건 | patch 내용 |
|:---|:---|:---|
| 상태형 기둥·다리 | Bridge 상태 | 다리 윗면 Support, 양쪽 Walk Link |
| 파괴 가능한 바닥 | 파괴 전 | 바닥 Support, 통과 link |
| 파괴 가능한 blocker | 파괴 전 | Nav 점유 |

## 6. 파괴

지원 범위:

- blocker 제거로 기존 통로 열기
- 바닥 제거로 기존 길 닫기
- 고정 상태 기믹으로 명시적 길 열기
- 임시 장애물로 cell 차단·해제

제외 범위:

- 물리 잔해가 우연히 새 보행면 생성
- 매 프레임 임의 형상 Nav 재생성
- 파편 더미 위 walkability 분석

파괴는 해당 마커의 patch를 비활성화할 뿐 정적 asset이나 다른 셀을 수정하지 않는다.

## 7. 상태 복제

- 요소 상태(Standing/Transitioning/Bridge, 파괴 여부)는 서버 권위이며 요소 Actor의 복제 프로퍼티다.
- 클라이언트는 collision과 표현만 반영한다. Nav·overlay·revision은 서버 전용이다.
- 동일 상태 이벤트의 중복 수신은 revision을 다시 증가시키지 않는다.
- link가 닫히면 active link 집합으로 ReachabilityGroup을 다시 구축하고 `ConnectivityGraphVersion`을 증가시킨다. union-find에서 삭제 역연산을 시도하지 않는다.

## 8. 지역 revision과 snapshot 게시

전역 revision 하나는 사용하지 않는다.

- Support Atlas 또는 Nav Tile/Cluster별 revision
- revision과 overlay 상태는 asset이 아니라 slot 인스턴스 기준이다. 같은 asset을 쓰는 다른 slot에 전파되지 않는다
- 경로가 실제 통과하는 지역 revision만 기록
- 무관한 섬의 변경으로 모든 NPC가 재탐색하지 않음
- 계층형 단계에서는 dirty cluster만 portal 비용 재계산

overlay는 매치 중에 계속 바뀐다. worker가 읽는 동안 shared pointer를 다시 대입하면 경쟁 조건이므로, 새 overlay snapshot은 Mass phase 경계의 게임 스레드 지점에서만 교체한다. 이전 snapshot은 참조가 모두 풀린 뒤 해제된다.

---
