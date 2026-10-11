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
- 스폰 시점은 현재 World Device 배치와 같은 자리다(`ALNPGameMode::BeginEntitySpawning`, `SpawnDevices` 다음).
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
                      → Mass PrePhysics 페이즈(모든 요소 Actor 틱이 prerequisite, D-050): 적 exact 이동
TG_StartPhysics~      그 밖의 Mass worker exact query(투사체 등)
```

  - 시뮬레이션 전(`OnWorldPreActorTick`)에 옮기거나 Actor 틱 없이 옮기면 Mover가 이동량 일부를 놓친다. 측정에서는 절반만 추종했다. Mover의 `AddTickDependency`는 base에 컴포넌트/Actor 틱이 있을 때만 prerequisite를 건다.
  - Mass PrePhysics 페이즈는 `DynamicSupport` 게시 tick 자체를 선행 조건으로 둔다. 각 패널 Actor tick은 게시 tick의 선행 조건이므로 최종 순서는 `패널 -> 게시 -> Mass`다. 패널만 게시와 Mass 양쪽에 각각 걸면 둘이 같은 패널 뒤 동시에 시작해 `Frame` shared ref 교체와 worker 복사가 경쟁할 수 있다(Phase 6 2P에서 검출·수정). 선행 조건은 tick function 멤버라 페이즈 tick 재등록에도 남는다. 요소 Actor는 Mass에 의존하지 않아 사이클이 없다.
  - 순서는 런타임에 검증한다. PrePhysics 페이즈 시작(`OnProcessingPhaseStarted`, 게임 스레드)마다 이번 프레임에 아직 틱하지 않은 패널을 세고 월드 종료 시 `[DynamicTerrain] MassPrePhysicsOrder checks=… violations=…`를 남긴다. 2026-09-25 2P 스모크에서 호스트·게스트 모두 위반 0이다.
  - 요소 Actor의 transform 쓰기는 PrePhysics exact query 전에 끝나므로 Gate 0의 쓰기 겹침이 생기지 않는다. Mover 폰·ActorPromoted 적의 이동 쓰기는 여전히 겹칠 수 있다(2P·적 300 스모크에서 락 합 P95 0.153ms).
- teleport로 옮긴 kinematic body의 물리 속도는 0이다. Mover는 base를 떠날 때(걸어 나가기·점프) `GetMovementBaseVelocityAtPoint`로 물리 속도를 관성에 더하므로, 매 갱신 뒤 물리 선속도를 결정론적 속도로 설정한다. `ComponentVelocity`도 함께 둔다.
- 경로 revision과 시작 시각을 초기 복제에 포함해 late join도 같은 자세를 재구성한다. server world time이 곧 server epoch다(매치·월드마다 0에서 시작). 움직이는 패널은 상태가 없다. 상태 복제는 상태형 요소(§4)에서 추가한다.
- `ReplicatedMovement`와 결정론적 transform 갱신을 동시에 사용하지 않는다. 큰 server-time 보정이나 상태 revision 불일치는 snap/짧은 보정 중 하나를 명시적으로 적용하고 진단한다.

## 3. 움직이는 패널

움직이는 패널은 경로 탐색 대상이 아니지만 NPC와 플레이어가 우연히 착지할 수 있다.

### 구현 규약(Phase 3, `ALNPMovingPanel`)

- 경로는 마커 루트 스플라인의 위치 곡선(`FInterpCurveVector`, 마커 로컬)을 그대로 초기 복제하고 마커 월드 transform을 원점으로 쓴다. 스폰 번치의 Actor 위치는 복제 시점 위치라 원점으로 쓰지 않는다.
- 양쪽이 같은 곡선에서 등속 거리 표를 만든다. 열린 경로는 양 끝에서 정지하며 왕복한다. 출발 자세의 Z축을 출발 위치의 구체 중심 방향에 맞추고, 도착 중력 방향까지 최소 회전한 두 quaternion을 거리 비율로 Slerp한다. 닫힌 루프는 순환하며 기존 마커 회전을 유지한다. 마커 scale은 무시하고 경고한다.
- 왕복 각속도는 두 자세의 회전축/각도와 경로 거리·속도로 계산한다. 돌아올 때 부호를 뒤집고 정차 시0이다. 물리 body와 DynamicSupport에 게시하며 이탈/진단의 접촉점 속도는 `LinearVelocity + AngularVelocity cross ContactOffset`을 사용한다. 경로와 시작 시각 외에 매 프레임 회전 복제를 추가하지 않는다.
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
- patch는 `MarkerId`를 키로 옥탄트 SurfaceData의 `ConditionalPatchPayload`에 저장된다(Phase 8 단위 2, codec v1·DataVersion 7).
- 런타임 overlay는 `(slot, MarkerId)`의 patch를 켜고 끄며 영향 tile의 revision을 증가시킨다.
- 여러 patch가 같은 cell에 겹치면 blocker occupancy는 reference count/활성 source 집합으로 합성하고 하나가 꺼져도 다른 blocker를 지우지 않는다. Support 추가는 stable patch surface ID로 구분하며 서로 다른 sheet를 반지름만으로 병합하지 않는다.
- patch 경계 edge, capsule clearance, base surface와의 높이·법선 오차를 베이크 차단 조건으로 검증한다.

| 요소 | patch 활성 조건 | patch 내용 |
|:---|:---|:---|
| 상태형 기둥·다리 | Bridge 상태 | 다리 윗면 Support, 양쪽 Walk Link |
| 파괴 가능한 바닥 | 파괴 전 | 바닥 Support, 통과 link |
| 파괴 가능한 blocker | 파괴 전 | Nav 점유 |

### 마커 로컬 공간 규약(Phase 4 공통 전제에서 확정, 구현은 Phase 8)

좌표계는 세 단계다. 뒤 단계로 갈수록 slot과 무관해진다.

| 단계 | 좌표계 | 담는 것 |
|:---|:---|:---|
| authoring | 마커 로컬 | 요소 클래스가 정의하는 patch source mesh와 상태별 transform(기둥의 안정 transform, 파괴 바닥의 온전한 형상). 마커 배치와 무관하다 |
| bake | 옥탄트 로컬(= LVI 로컬) | `마커의 LVI 내 transform × 마커 로컬`로 옮긴 형상을 base Atlas·Nav Tile에 투영한 결과 |
| runtime | slot 인스턴스 | `(slot, MarkerId)`로 해당 slot SurfaceData의 patch를 켠다. slot 회전은 옥탄트 데이터 전체에 걸리므로 patch를 따로 회전하지 않는다 |

- patch 데이터는 좌표가 아니라 **base 데이터의 인덱스**로 저장한다. Support는 base Atlas 격자 샘플, Nav는 base Nav cell을 가리킨다. 그래서 같은 definition을 쓰는 모든 slot이 같은 patch를 공유하고, 런타임에 형상을 변환하거나 로컬 grid를 병합하지 않는다(D-041).
- patch가 추가하는 보행면은 새 Support Layer다. 격자·codec은 4b의 섬·동굴 sparse Atlas를 그대로 쓰고 "patch 소유" 표시와 활성 여부만 더한다. patch Layer ID는 정적 Layer ID 뒤에 MarkerId 오름차순으로 붙여 결정론을 유지한다. 반지름이 비슷해도 정적 Layer와 병합하지 않는다.
- 파괴 바닥처럼 **처음부터 있는** 보행면도 patch다. 정적 payload에는 `Destructible` source가 없으므로(`TerrainContract.md`) 온전한 상태의 Support를 patch로 굽고 파괴 시 끈다.
- 런타임 활성화 조건은 상태 값만 본다(Bridge 도달, 파괴 전). 요소 Actor의 실제 transform과 베이크 transform의 비교는 debug 진단으로만 둔다. 결정론적 움직임(D-027)이라 안정 transform은 authoring 값과 같다.
- patch footprint와 영향 범위는 옥탄트 경계를 넘지 않는다(D-030). 같은 방향에서 정적 Layer와 patch Layer가 캡슐 높이 안쪽으로 겹치면 Layer 해석이 모호하므로 베이크 오류로 막는다.
- stale 입력: MarkerId, 마커 transform, 요소 클래스, 경로·상태 파라미터, patch source mesh와 상태별 transform을 canonical hash에 넣는다(D-041, `DataModel.md`). 현재 DataVersion 7·BakerSchemaVersion 9다.

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

### Nav 점유 합성 기반(Phase 8 단위 1)

`LNPNavOverlay::BuildOverlay`는 현재 활성 Pod 목록과 `FLNPNavMarkerBlocker` 목록을 입력으로 받아 차단 mask를 새로 합성한다. 마커 점유는 `(slot, MarkerId)`와 snapshot generation을 포함한 `FLNPNavNodeRef` 목록이다. 서로 다른 source의 점유를 OR하므로, 겹친 blocker 하나를 제거해도 남은 source의 점유는 유지한다. 같은 mask의 재적용은 revision을 올리지 않고, 실제 차단 상태가 바뀐 Tile만 revision을 올린다. 기존 Pod 전용 함수는 이 합성 함수를 마커 목록 없이 호출한다.

잘못된 MarkerId·slot·다른 slot의 node·stale node ref는 전체 입력을 거부하며 출력 overlay를 수정하지 않는다. 이전 overlay는 같은 snapshot generation에서만 넘겨야 한다. 이 함수는 순수 데이터 기반이며, 마커 Actor 상태 수집·patch stream·Mass phase 경계 게시·Support와 Walk Link 활성화는 후속 단위에서 연결한다.

---

### Phase 8 단위 2 구현

마커의 `PatchSources`는 Mesh·마커 로컬 안정 transform·Support 참여 여부를 가진다. 모든 source는 해당 상태에서 blocker이고 Support 참여 source만 sparse 보행면을 만든다. `PatchCondition`은 Intact 또는 Bridge다. 한 마커는 하나의 안정 상태 patch를 소유하며 `ALNPStatefulTerrain`이 같은 authoring 형상을 소비한다(단위 4).

베이커는 정적 Atlas와 동일한 옥탄트 좌표계에서 형상을 굽는다. 추가 Layer ID는 정적 Layer 뒤 GUID A/B/C/D 오름차순으로 붙는다. patch stream에는 중복 지각 없이 sparse Support, source ordinal별 exact face map, 추가 Nav, 정적 node 차단, 정적 edge 차단, patch와 연결된 양방향 Walk edge를 저장한다. Nav component ID는 patch 안에서 0부터 저장한다. base cell·tile·edge의 canonical bytes가 달라지면 베이크를 거부한다.

source 경계가 옥탄트 seam에 닿거나 int16 위치 상한을 넘으면 거부한다. 정적 보행면과 캡슐 높이 안에서 내부가 겹치면 오류다. 경계 접합은 exact Support 경로와 중력 캡슐 polyline clearance로 Walk edge를 검증하며, Bridge에 유효 연결이 없으면 거부한다. blocker-only patch는 보행면을 추가하지 않는다.

로더는 stream hash·size·count·codec·GUID 순서·Layer 범위·sparse row·face map·node/edge endpoint를 검사하고 같은 asset의 decoded patch를 slot 사이에 공유한다. resident memory 계수에도 포함한다. 단위 2는 비활성 데이터 로드까지이며 활성 view는 아래 단위 3이 소유한다.

### Phase 8 단위 3: 활성 view와 게시

`LNPConditionalPatchRuntime::Prepare`는 로드 때 정적 payload 뒤에 모든 patch Layer·component를 붙인 공유 view와 조밀 graph를 만든다. 아직 비활성인 patch node도 주소를 예약해 `(slot, Layer, Tile, cell, generation)`과 조밀 node index가 상태 전환으로 바뀌지 않는다. patch가 없는 asset은 기존 정적 로드 경로를 그대로 쓴다. source 표와 정적 face map은 보존한다.

`BuildActiveSnapshot`은 비활성 기준 snapshot과 활성 `(slot, MarkerId)` 집합으로 새 snapshot을 합성한다. 활성 Support mask, 추가 node, 명시적 양방향 Walk edge와 base node/edge 차단을 함께 바꾼다. 여러 blocker는 전체 활성 집합의 합으로 합성한다. 실제 node/edge 상태가 같으면 연결성 version을 유지하고, 실제 차단·edge 또는 Support 활성 상태가 달라진 Tile만 revision을 올린다. 동일 상태·중복 입력은 게시·revision을 반복하지 않는다.

patch 점유·edge 차단은 기존 정적 component 내부를 갈라놓을 수도 있다. 따라서 현재 ReachabilityGroup은 Pod 점유를 제외한 활성 보행 graph의 연결 성분을 다시 계산하고 `ReachabilityGroupByNode`에 저장한다. `StaticNavComponent` 식별자는 유지하며, 하나가 여러 group으로 갈라진 경우 component→group 요약은 `MAX_uint32`다. 도달성·접근점·Pod 선택은 node의 현재 group을 사용한다. 실제 Nav topology 변경 때만 `ConnectivityGraphVersion`을 올린다.

서버 `ULNPSurfaceDataSubsystem::SetPatchActive`는 상태 요청을 모을 뿐 shared ref를 교체하지 않는다. `ULNPNavPathSubsystem`의 Mass PrePhysics 시작 delegate가 worker dispatch 전에 Support·Nav snapshot과 Pod/patch Tile revision overlay를 순서대로 게시한다. Pod 차단 제거도 같은 경계까지 기다린다. 이전 reader는 보유한 immutable snapshot을 계속 읽는다. Actor 상태·exact identity·collision의 연결은 아래 단위 4 구현을 따른다.

완성된 성공 경로는 generation과 통과 Tile revision을 검사해 무관한 지역의 link 전환을 견딘다. 실행 중 A*는 connectivity version과 overlay revision 변경 시 Stale이다. 접근점 경로와 도달 불가 결과는 연결성 변경 시 다시 계획해 새 길을 놓치지 않는다. 지상 NPC는 connectivity version 변경 뒤 Home 도달성을 한 번 재확인한다.

### Phase 8 단위 4: 상태형 Actor와 파괴

마커의 `ElementClass`를 `ALNPStatefulTerrain`으로 설정한다. `PatchSources`의 mesh·source transform으로 서버와 클라이언트가 같은 이름의 Movable component를 만든다. component는 net-addressable이고 물리 시뮬레이션을 쓰지 않는다. 마커 월드 transform·형상·조건·StandingTransform·TransitionSeconds와 상태/revision/시작 server world time을 복제하며 ReplicatedMovement는 사용하지 않는다.

Bridge 조건은 Standing에서 시작한다. StandingTransform은 안정 형상 전체에 적용하는 마커 로컬 자세이며 scale은 1이다. 서버 `StartTransition()` 요청 뒤 Translation은 선형, Rotation은 최단 quaternion slerp로 identity까지 보간한다. 서버 Actor 틱에서 시간이 끝나면 Bridge 상태를 확정한다. 전이 중 collision과 우연한 DynamicSupport 접촉은 유지하지만 patch·계획적 traversal은 비활성이다. GameState 수신 전 클라이언트는 복제 시작 시각 자세를 유지한다.

Intact 조건은 온전한 형상과 활성 patch로 시작한다. 서버 `BreakTerrain()`은 모든 상태에서 Destroyed로 전환하며 다음 Actor 틱에서 collision·표현·exact 등록·patch를 제거한다. 동일 전이/파괴 요청은 상태 revision을 반복 증가시키지 않는다. 파편이나 새 보행면은 만들지 않는다.

상태 API는 복제 상태만 바꾸고 실제 형상·registry·patch 요청은 TG_PrePhysics Actor 틱에서 적용한다. `요소 틱 → DynamicSupport/dirty exact registry 게시 → Mass PrePhysics 시작에서 Support/Nav/경로 overlay 게시 → worker` 순서다. 기존 PostPhysics registry 게시도 유지한다. 각속도와 접촉점 선속도를 물리 body·DynamicSupport에 넣어 회전 기둥 이탈 관성을 반영한다.

안정 상태에서는 마커 source ordinal의 베이크 face map과 SurfaceData generation을 runtime exact entry에 연결한다. 따라서 patch 착지의 LocalLayerId는 베이크 Layer와 같고, NPC는 안정 상태를 DynamicSupport 대신 SurfaceHandle로 소비한다. 전이 상태는 face map 없이 `(slot, MarkerId)` 동적 접촉으로 소비한다. blocker-only source는 Support 역할을 갖지 않는다. collision envelope는 전이 구간 전체를 포함하는 보수적 반지름이다.

Standing 자세·전이 시간은 Bridge patch가 있는 마커의 canonical semantic hash에 포함한다. stream 형식·베이크 geometry 알고리즘은 그대로이고 DataVersion 7/schema 9를 유지한다. 이 입력이 바뀐 Bridge 마커는 stale이며 다시 굽는다. patch가 없는 현재 production 마커의 semantic bytes는 바뀌지 않는다. 실제 replicated component base 참조·네트워크 late join·PIE 틱 순서·패키지 검증은 단위 5 Gate에서 확인한다.
