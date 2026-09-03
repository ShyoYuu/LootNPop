# Engine Analysis - Unreal Engine 5.8 Mass Replication

## 1. 개요 (Overview)

Mass Replication은 MassGameplay 플러그인의 서브모듈로, **서버의 Mass 엔티티를 클라이언트로 복제**하는 프레임워크입니다. Mass 엔티티는 Actor가 아니므로 UE 표준 복제 채널(Actor Channel)을 가질 수 없습니다. 이 모듈은 그 공백을 "클라이언트별 버블(Bubble) Actor + Fast Array 델타 직렬화"로 메웁니다.

핵심 철학:
- **서버 권위**: 엔티티 시뮬레이션은 서버에서만 돌고, 클라이언트는 스냅샷을 수신해 재구성합니다.
- **관련성 기반 선별**: 클라이언트마다 뷰어 위치 기준 LOD를 계산해, 가까운 엔티티만 그 클라이언트의 버블에 실립니다.
- **Actor 1개당 엔티티 N개**: 엔티티마다 Actor 채널을 여는 대신, 클라이언트당 Actor 1개(버블)에 수백 개체를 배열로 실어 나릅니다.

전형적인 용도: 대규모 군중/NPC의 Low LOD 시각화, Actor 복제와 병행되는 엔티티-Actor(퍼펫) 연결.

---

## 2. 아키텍처 큰 그림

```
[서버]                                                  [클라이언트 A]
Mass 엔티티 (수백~수천)
   │
   │  ① UMassReplicationProcessor (클라이언트별 루프)
   │     - 해시 그리드로 뷰어 주변 엔티티 수집
   │     - 클라 A 기준 LOD 계산
   ▼
BubbleInfo(A) ─ FastArray[농부#3, 늑대#7 …] ═══복제═══▶ BubbleInfo(A) 복제본
BubbleInfo(B) ─ FastArray[늑대#7, 곰#1 …]   ═▶ (클라 B에게만)      │
                                                       ② PostReplicatedAdd/Change
                                                          - TemplateID별 배치 스폰
                                                          - NetID 등록 → 퍼펫 링크
                                                       ▼
                                                     클라 로컬 Mass 엔티티
```

구성 요소 요약:

| 구성 요소 | 역할 |
|:---|:---|
| `AMassClientBubbleInfoBase` | 클라이언트별 복제 컨테이너 Actor ("버블") |
| `FMassClientBubbleSerializerBase` | 버블 안의 Fast Array 본체 + 델타 직렬화 |
| `TClientBubbleHandlerBase<Item>` | 버블 배열의 추가/제거/조회 로직 (서버) + 수신 콜백 (클라) |
| `FReplicatedAgentBase` | 개체별 복제 데이터 (NetID, TemplateID + 커스텀 필드) |
| `UMassReplicatorBase` | 타입별 복제 정책 (Add/Modify/Remove 시 무엇을 실을지) |
| `UMassReplicationSubsystem` | 클라이언트 핸들·버블 수명·NetID↔엔티티 매핑 관리 |
| `UMassReplicationProcessor` | 서버 틱마다 클라이언트별 LOD 계산·복제 실행 |
| `UMassReplicationTrait` | 엔티티 템플릿에 복제 Fragment 세트를 부착 |

---

## 3. 핵심 개념 (Core Concepts)

### 3.1 Client Bubble — 클라이언트별 복제 컨테이너

**"버블"은 특정 클라이언트를 둘러싼 거품**이라고 생각하면 됩니다. 거품 안에는 "그 클라이언트에게 지금 복제 중인 에이전트들"이 들어 있고, 엔티티가 가까워지면 거품 안으로 들어오고(Add), 멀어지거나 파괴되면 거품 밖으로 나갑니다(Remove).

구현체는 `AMassClientBubbleInfoBase` 파생 **Actor**입니다:

- 클라이언트가 접속하면 서버가 **클라이언트마다 1개씩** 스폰합니다 (`UMassReplicationSubsystem::AddClient`).
- Owner = 해당 클라이언트의 PlayerController, `bOnlyRelevantToOwner = true` → **오직 그 클라이언트에게만 복제**됩니다.
- 서버에는 접속 클라이언트 수만큼 버블이 존재하고(`BubbleInfoArray[클래스].Bubbles[ClientHandle.GetIndex()]`), 각 클라이언트 머신에는 자기 버블의 복제본 1개만 존재합니다.

**왜 Actor인가?** Fast Array 델타 직렬화는 복제 `UPROPERTY`를 담을 net-addressable 객체가 필요합니다. 버블 Actor는 순수하게 "복제 채널 역할"만 하는 빈 껍데기이며, 시각적 표현이 없습니다.

버블 내부는 두 층으로 나뉩니다:

```cpp
// ① Serializer — Fast Array 본체. 복제 UPROPERTY 배열을 소유
USTRUCT()
struct FMyClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
    GENERATED_BODY()

    FMyClientBubbleSerializer() { Bubble.Initialize(Agents, *this); }

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FMyFastArrayItem, FMyClientBubbleSerializer>(
            Agents, DeltaParams, *this);
    }

    FMyClientBubbleHandler Bubble;   // ② Handler

protected:
    UPROPERTY(Transient)
    TArray<FMyFastArrayItem> Agents; // 실제 복제되는 배열
};

// ② Handler — 배열 조작 로직. 서버: 핸들 발급/제거, 클라: 수신 콜백
class FMyClientBubbleHandler : public TClientBubbleHandlerBase<FMyFastArrayItem>
{
protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
    virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
    virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;
#endif
};
```

서버 흐름: LOD 판정 → `Bubble.AddAgent(Entity, Agent)` / `RemoveAgentChecked(Handle)` → Fast Array dirty 마킹 → 다음 NetUpdate에 델타 전송.
클라 흐름: Fast Array 수신 → `PostReplicatedAdd` / `PostReplicatedChange` / `PreReplicatedRemove` 콜백 → 로컬 엔티티 스폰/갱신/제거.

### 3.2 FMassNetworkID — 머신 간 개체 식별자

엔티티 핸들(`FMassEntityHandle`)은 **머신마다 다릅니다** (서버의 늑대#7이 클라에서는 #3일 수 있음). 그래서 서버가 복제 대상 엔티티마다 전역 고유 `FMassNetworkID`를 발급하고, 이 ID가 에이전트에 실려 왕복합니다.

- 서버: `FMassNetworkIDFragment`에 저장, 버블의 `NetworkIDToAgentHandleMap`으로 역참조.
- 클라: 수신 시 `UMassReplicationSubsystem::SetEntity(NetID, Entity)`로 등록 → NetID로 서버·클라 엔티티를 대응시킬 수 있음 (퍼펫 링크의 기반).

### 3.3 ReplicatedAgent — 개체별 복제 페이로드

`FReplicatedAgentBase` 파생 구조체가 "개체 하나에 대해 전송되는 데이터"입니다. 베이스에 NetID와 TemplateID가 내장돼 있고, 여기에 커스텀 필드를 추가합니다.

```cpp
USTRUCT()
struct FMyReplicatedAgent : public FReplicatedAgentBase
{
    GENERATED_BODY()

    // TMassClientBubbleTransformHandler 요구 접근자
    FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable() { return PositionYaw; }
    const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const { return PositionYaw; }

private:
    UPROPERTY(Transient)   // ⚠️ UPROPERTY 없으면 직렬화에서 빠진다
    FReplicatedAgentPositionYawData PositionYaw;
};

USTRUCT()
struct FMyFastArrayItem : public FMassFastArrayItemBase
{
    GENERATED_BODY()

    FMyFastArrayItem() = default;
    FMyFastArrayItem(const FMyReplicatedAgent& InAgent, const FMassReplicatedAgentHandle InHandle)
        : FMassFastArrayItemBase(InHandle), Agent(InAgent) {}

    typedef FMyReplicatedAgent FReplicatedAgentType; // 필수 typedef

    UPROPERTY()
    FMyReplicatedAgent Agent;
};
```

위치/Yaw처럼 흔한 데이터는 엔진이 헬퍼 쌍을 제공합니다: 서버 측 `FMassReplicationProcessorPositionYawHandler`(Fragment→Agent 기록) + 클라 측 `TMassClientBubbleTransformHandler`(Agent→Fragment 반영).

### 3.4 FMassReplicatedAgentHandle — 핸들러-로컬 핸들

서버 측 버블 핸들러는 자기 배열의 슬롯을 가리키는 핸들을 발급합니다(`AgentHandleManager`, 인덱스 0부터 시작하는 프리 리스트). **이 핸들은 발급한 핸들러 안에서만 의미가 있습니다.** 서로 다른 핸들러의 핸들 `{Index=0}`은 완전히 다른 대상을 가리킵니다.

⚠️ **정확한 소유 단위는 버블 Actor가 아니라 핸들러 인스턴스입니다.** `AgentHandleManager`는 `TClientBubbleHandlerBase`의 인스턴스 멤버이므로(`MassClientBubbleHandler.h:221`), 버블 클래스가 하나여도 그 안에 핸들러를 여러 개 두면 발급자도 여러 개가 됩니다. 이 구분이 §7.1의 핵심입니다.

핸들→`{엔티티, NetID, 배열 인덱스}` 역참조는 `AgentLookupArray`가 담당하며, 제거는 RemoveAtSwap이므로 배열 순서는 보존되지 않습니다 (식별은 항상 NetID로).

### 3.5 UMassReplicationSubsystem — 수명과 장부의 관리자

- **버블 클래스 등록**: `RegisterBubbleInfoClass(클래스)` — 접속(AddClient) 전에 호출해야 함. 등록된 클래스마다 클라이언트당 버블 1개가 스폰됩니다.
- **클라이언트 핸들**: `FMassClientHandle` — 접속 클라이언트를 가리키는 프리 리스트 핸들.
- **`FMassClientReplicationInfo`** — 클라이언트당 1개 존재하는 "복제 장부":

```cpp
struct FMassClientReplicationInfo
{
    TArray<FMassViewerHandle> Handles;        // 이 클라의 뷰어들 (스플릿스크린 포함)
    TArray<FMassEntityHandle> HandledEntities; // 지난 틱에 처리한 엔티티들
    FMassReplicationAgentDataMap AgentsData;   // 엔티티 핸들 → {버블 핸들, LOD, bPendingDestruction}
};
```

⚠️ **`AgentsData`는 타입 구분이 없습니다.** 이 월드에서 복제되는 **모든 엔티티 타입이 한 맵에 뒤섞여** 기록됩니다. 이 사실이 §7.1의 함정으로 직결됩니다.

- **파괴 통지**: 복제 대상 엔티티가 파괴되면 옵저버(`UMassReplicationEntityDestructionObserver`)가 `NotifyEntityDestroyed`를 호출하고, 모든 클라이언트 장부에서 해당 엔트리의 `bPendingDestruction`을 켭니다 — 역시 타입 무구분 (`MassReplicationSubsystem.cpp:592-601`).

### 3.6 Replicator — 타입별 복제 정책

`UMassReplicatorBase` 파생 클래스가 "이 타입은 Add/Modify/Remove 시 무엇을 하는가"를 정의합니다. 핵심은 템플릿 헬퍼 `CalculateClientReplication<Item>`에 콜백 4개를 넘기는 것:

```cpp
void UMyReplicator::ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext)
{
#if UE_REPLICATION_COMPILE_SERVER_CODE
    auto CacheViewsCallback  = [](FMassExecutionContext&) { /* Fragment 뷰 캐싱 */ };
    auto AddEntityCallback   = [](FMassExecutionContext&, int32 EntityIdx, FMyReplicatedAgent&, FMassClientHandle) -> FMassReplicatedAgentHandle
                               { /* Agent 채우고 Bubble.AddAgent */ };
    auto ModifyEntityCallback= [](FMassExecutionContext&, int32 EntityIdx, EMassLOD::Type, double Time, FMassReplicatedAgentHandle, FMassClientHandle)
                               { /* 변경분 반영 + dirty 마킹 (불필요하면 no-op) */ };
    auto RemoveEntityCallback= [](FMassExecutionContext&, FMassReplicatedAgentHandle, FMassClientHandle)
                               { /* Bubble.RemoveAgentChecked */ };

    CalculateClientReplication<FMyFastArrayItem>(Context, ReplicationContext,
        CacheViewsCallback, AddEntityCallback, ModifyEntityCallback, RemoveEntityCallback);
#endif
}
```

`CalculateClientReplication`의 내부 동작 (`MassReplicationProcessor.h:108-193`):

1. 쿼리에 매칭된 청크의 엔티티를 순회하며 —
   - LOD가 Off 미만이고 핸들이 없으면 → Agent 생성(NetID·TemplateID 자동 세팅) 후 `AddEntity`
   - 핸들이 있으면 → `ModifyEntity`
   - LOD가 Off면 → `RemoveEntity` + 핸들 무효화
2. **파괴 처리 루프**: 청크 순회가 끝나면 **클라이언트 장부(`AgentsData`) 전체를 순회**하며, `bPendingDestruction`인 엔트리마다 `RemoveEntity` 호출 + 엔트리 무효화 (`MassReplicationProcessor.h:176-191`).

Remove 콜백은 3-인자 외에 **엔티티 핸들을 함께 받는 4-인자 오버로드**도 지원합니다 (`std::is_invocable_v` 분기) — 제거 대상이 어느 엔티티인지 알아야 할 때 사용합니다.

### 3.7 UMassReplicationTrait와 FMassReplicationSharedFragment

`UMassReplicationTrait`를 엔티티 템플릿에 넣으면 복제에 필요한 Fragment 세트가 부착됩니다:

- `FMassNetworkIDFragment`, `FReplicationTemplateIDFragment`(빌드 시 TemplateID 기록), `FMassReplicatedAgentFragment`(장부 스왑 버퍼), `FMassReplicationLODFragment`, `FMassReplicationGridCellLocationFragment`
- `FMassReplicationParameters`: **`BubbleInfoClass`와 `ReplicatorClass`** 지정 + LOD 거리/개체 수 설정
- `FMassReplicationSharedFragment`: `CachedReplicator` 인스턴스, 리플리케이터의 `AddRequirements`로 구성된 `EntityQuery`, LOD 계산기를 보유. **이 프래그먼트 1개가 "복제 구성 1개"의 단위입니다** (§3.8).
  ⚠️ 중복 제거는 Params 값이 아니라 **이 구조체의 UPROPERTY만으로 만든 CRC**로 이뤄집니다. LOD 거리는 UPROPERTY가 아닌 `LODCalculator`에 들어가 **CRC에 반영되지 않으므로**, 실질적인 유일 키는 `ReplicatorClass`(의 CDO) 하나입니다 — 상세는 §7.11.
- `NM_Standalone`에서는 `BuildTemplate`이 조기 반환합니다 — 싱글플레이 분기를 따로 만들 필요가 없습니다.
- ⚠️ **`GetNetMode()`는 월드 초기화 중 신뢰할 수 없습니다.** `UEngine::LoadMap`은 `InitWorld()`(월드 서브시스템 `Initialize()`) → `Listen(URL)`(NetDriver 생성) 순서로 진행하고, NetDriver가 없을 때 폴백하는 `UWorld::AttemptDeriveFromURL()`은 `NextURL`·`PendingNetGame->URL`만 볼 뿐 현재 실행 URL의 `?Listen` 옵션은 보지 않습니다. 따라서 `-game` 리슨 서버는 `Initialize()` 시점에 `NM_Standalone`을 반환해 위 조기 반환에 걸립니다(PIE는 바로 뒤 `WITH_EDITOR` 분기가 `PlayInEditorNetMode`를 돌려줘 드러나지 않음 — `UWorld::InternalGetNetMode`). 템플릿은 `ConfigGuid` 키로 월드 수명 내내 캐시돼 소급 수정도 불가능합니다 — **엔티티 템플릿 생성은 반드시 `OnWorldBeginPlay` 이후에** 하세요.

### 3.8 엔진이 의도한 타입 분리 축 — "공유 프래그먼트 1개 = 복제 구성 1개"

§7.1과 §7.11이 왜 서로 다른 방향을 가리키는지는 이 절을 알아야 이해됩니다. **엔진이 타입을 가르라고 만든 축은 버블이 아니라 공유 프래그먼트(= 리플리케이터 클래스)입니다.**

`UMassReplicationProcessor::PrepareExecution` (`MassReplicationProcessor.cpp:95-113`):

```cpp
EntityManager.ForEachSharedFragment<FMassReplicationSharedFragment>([this](FMassReplicationSharedFragment& RepSharedFragment)
{
    if (!RepSharedFragment.bEntityQueryInitialized)
    {
        RepSharedFragment.EntityQuery = EntityQuery;          // ① 프로세서의 공통 쿼리를 복사한 뒤
        RepSharedFragment.EntityQuery.SetChunkFilter([&RepSharedFragment](const FMassExecutionContext& Context)
        {
            return &Context.GetSharedFragment<FMassReplicationSharedFragment>() == &RepSharedFragment;
        });                                                   // ② 자기 프래그먼트를 쓰는 청크로 한정하고
        RepSharedFragment.CachedReplicator->AddRequirements(RepSharedFragment.EntityQuery);  // ③ 요구사항은 리플리케이터가 채운다
        RepSharedFragment.bEntityQueryInitialized = true;
    }
    // ... 이하 클라이언트 핸들 ↔ BubbleInfos 캐시 동기화
});
```

세 가지가 한 번에 읽힙니다:

1. **프로세서는 월드에 하나뿐이고, 분리 단위는 공유 프래그먼트다.** 프래그먼트마다 전용 쿼리를 만들어 자기 청크만 처리합니다.
2. **그 쿼리의 요구사항을 리플리케이터가 결정한다.** 타입마다 필요한 Fragment가 다르면 리플리케이터도 달라야 합니다.
3. 따라서 **"타입마다 `UMassReplicatorBase` 서브클래스 1개"가 정석**입니다. 엔진 자신도 그렇게 적어 뒀습니다 — *"You should derive from this **per entity type** (that require different replication processing)"* (`MassReplicationProcessor.h:23`). 문구는 분리 축이 프로세서에 있던 시절의 잔재지만, 의도한 축은 동일합니다.

⚠️ 이 정석을 벗어나 **여러 타입이 리플리케이터 클래스 하나를 공유하면 §7.11의 조용한 결함**에 걸립니다. 뒤집어 말하면 §7.11은 정석 구성에서는 애초에 발화하지 않는, **비정석 사용에서만 드러나는 잠복 결함**입니다.

**엔진이 실제로 배송한 사용례는 하나뿐입니다.** UE 5.8 엔진 트리 전체에서 `UMassReplicatorBase` 파생은 `UMassCrowdReplicator`(`MassCrowdReplicator.h:12`), `AMassClientBubbleInfoBase` 파생은 `AMassCrowdClientBubbleInfo`(`MassCrowdBubble.h:97`) — 각각 **단 하나씩**이고, 그 버블이 가진 핸들러도 하나입니다(`MassCrowdBubble.h:17,111`).

> 즉 검증된 구성은 **"타입 1개 = 리플리케이터 1개 = 버블 1개 = 핸들러 1개"** 뿐입니다.
> 여기서 벗어난 조합 중 **타입별 리플리케이터는 엔진이 명시적으로 의도한 확장**이지만(위 ③),
> **타입별 버블·타입별 핸들러는 주석에만 있고 샘플이 없으며 실제로 깨져 있습니다**(§7.1).

---

## 4. 서버 측 틱 흐름 (UMassReplicationProcessor::Execute)

```
for each 클라이언트:                                  (MassReplicationProcessor.cpp:184)
    장부(ClientReplicationInfo) 획득                   (:191 — 클라당 1개)
    뷰어 위치로 해시 그리드 쿼리 → 대상 엔티티 수집     (:214-224, +지난 틱 HandledEntities)
    SyncToMass: 장부 → FMassReplicatedAgentFragment    (:244-266, 이 클라 기준 값으로 스왑)
    LOD 수집·계산 (이 클라 뷰어 기준)                  (:269-314)
    for each SharedFragment, for each 청크:
        CachedReplicator->ProcessClientReplication()   (:317-328)
          └ CalculateClientReplication: Add/Modify/Remove + 파괴 루프
    SyncFromMass: Fragment → 장부                      (:331-343)
    장부 클린업: LOD Off·파괴 완료 엔트리 삭제          (:367-376)
```

주의점 두 가지:
- `FMassReplicatedAgentFragment`는 엔티티당 1개인데 클라이언트는 여럿이므로, **클라이언트 루프마다 장부↔Fragment 스왑**(SyncToMass/SyncFromMass)으로 재사용합니다. Fragment에 남아 있는 값은 "마지막으로 처리한 클라이언트" 기준입니다.
- `ProcessClientReplication`은 **(공유 프래그먼트 × 청크) 횟수만큼 호출**됩니다. 각 공유 프래그먼트의 쿼리는 §3.8의 청크 필터로 자기 청크만 보지만, **파괴 루프가 순회하는 클라이언트 장부는 필터가 걸리지 않은 공용 자료구조**입니다.
  → 리플리케이터를 N개로 나누면 **넷 틱마다 장부 전체를 N번 순회**합니다. 처리한 엔트리를 즉시 무효화하므로 결과는 멱등이지만(2·3회차는 건너뜀), **O(N × 장부 크기)의 순회 비용 자체는 남습니다.** 장부가 커지는 프로젝트(수백 에이전트 이상)에서는 리플리케이터 분할의 CPU 대가로 계산에 넣으십시오.

---

## 5. 클라이언트 측 수신 흐름

`PostReplicatedAdd`의 엔진 헬퍼(`PostReplicatedAddEntitiesHelper`, `MassClientBubbleHandler.h:560-632`)가 하는 일:

1. 추가된 에이전트들을 **TemplateID별로 그룹핑** (:572-581)
2. 그룹마다 `SpawnerSubsystem->SpawnEntities(TemplateID, 개수)` — **클라이언트 로컬에 엔티티 배치 스폰** (:594)
3. 스폰 쿼리(`GetSpawnQuery`)로 스폰된 엔티티들을 순회하며:
   - `FMassNetworkIDFragment`에 NetID 기록 + `ReplicationSubsystem->SetEntity(NetID, Entity)` 등록 (:622-623)
   - `SetSpawnedEntityData(EntityView, Agent, EntityIdx)` — 위치 등 초기 데이터 반영 (:626)

여기서 중요한 함의:

- **버블 하나로 이종(異種) 엔티티 타입을 복제할 수 있습니다.** TemplateID 그룹핑이 아키타입별 스폰을 자동 처리하기 때문입니다. 타입별 추가 데이터는 `SetSpawnedEntityData`가 받는 `FMassEntityView`로 조건부 접근(`GetFragmentDataPtr<T>()`)하면 됩니다.
- **클라이언트에도 같은 EntityTemplate이 등록돼 있어야 합니다.** TemplateID는 해시 기반이므로, 서버·클라가 같은 EntityConfig 에셋을 로드하면 일치합니다.

**퍼펫 링크**: 복제 Actor(폰 등)와 Mass 엔티티를 병행하는 타입은, 클라이언트에서 "복제로 도착한 Actor"와 "버블로 스폰된 엔티티"를 연결해야 합니다. `UMassAgentComponent`의 NetID 핸드셰이크가 이를 담당합니다 — Actor에 복제된 NetID로 `ReplicationSubsystem->GetEntity(NetID)`를 조회해 링크하고, 링크 시점에 **엔티티 Transform으로 Actor 위치를 초기화**합니다.

`PostReplicatedChange`는 변경 인덱스만 받아 갱신을 반영하고, `PreReplicatedRemove`는 로컬 엔티티 제거를 담당합니다. 순서 뒤바뀐 Add/Remove에 대비한 `AgentsRemoveDataMap`(NetID 기반 타임아웃) 안전망도 내장돼 있습니다.

---

## 6. LOD와 관련성 (Relevancy)

- `FReplicationHashGrid2D`: 복제 대상 엔티티의 위치가 등록되는 2D 해시 그리드. 클라이언트 뷰어 주변 `MaxLODDistance` 박스로 쿼리해 후보를 추립니다.
- LOD는 **클라이언트별로** 계산됩니다 (같은 엔티티라도 클라 A에겐 High, 클라 B에겐 Off일 수 있음).
- `EMassLOD::Off`가 되면 그 클라이언트의 버블에서 제거됩니다 — 엔티티가 살아 있어도 멀어지면 클라에서는 사라진다는 뜻입니다. 클라이언트 측 표현(빛기둥 등)은 이 추가/제거 반복을 견디도록 설계해야 합니다.
- LOD 거리·최대 개체 수는 `FMassReplicationParameters`로 타입별 설정이 가능합니다. ⚠️ **단, 타입마다 `ReplicatorClass`를 따로 두지 않으면 이 타입별 설정이 통째로 무효화됩니다** (§7.11). 또한 개수 캡(`LODMaxCount*`)과 `AdjustLODFromCount`의 거리 보정은 **공유 프래그먼트 단위**로 계산되므로, 리플리케이터를 나누면 캡도 타입별 독립 예산이 됩니다 — 나누기 전의 "전 타입 합산 예산"을 전제한 값이 있었다면 다시 잡아야 합니다.
- ⚠️ **기본 `LODDistance[Off]`는 5,000cm**입니다 (`MassReplicationFragments.cpp:62`). 월드 스케일이 이보다 크면 클라이언트가 받는 범위가 서버 화면(로컬이라 시각화 LOD 거리까지 다 보임)보다 훨씬 좁아져, "서버엔 보이는데 클라엔 근접해야만 보임"으로 나타납니다. 시각화 트레잇의 `VisibleLODDistance[Off]`와 짝을 맞추는 것이 기본입니다. 개수 캡 `LODMaxCountPerViewer`(기본 Low=300)가 거리보다 먼저 걸릴 수 있다는 점도 함께 확인해야 합니다 — `AdjustLODFromCount`가 캡에 맞춰 거리를 되레 줄입니다.

---

## 7. 작업 시 유의점 (Pitfalls)

### 7.1 ⚠️ 핸들 발급자는 클라이언트당 하나만 — 다중 버블·다중 핸들러의 파괴 경로는 깨져 있다 (가장 중요)

`RegisterBubbleInfoClass` 주석(`MassReplicationSubsystem.h:197-204`)은 확장 방법을 **두 가지** 제안합니다:

> *"…in that case each client will get multiple BubbleInfoClasses. This can be useful for **replicating different Entity types in different bubbles**, although it's also possible to have **multiple `TClientBubbleHandlerBase` derived class instances per BubbleInfoClass**."*

**둘 다 실제로는 쓸 수 없습니다.** 엔티티 파괴 경로가 그 분리를 지키지 않기 때문입니다. 근거 체인:

1. 파괴 통지는 타입 무구분 — `NotifyEntityDestroyed`는 클라 장부에서 엔티티를 찾아 `bPendingDestruction`만 켠다 (`MassReplicationSubsystem.cpp:592`).
2. 각 리플리케이터의 파괴 루프는 **장부 전체(모든 타입)를 순회**하며, 만나는 모든 파괴 대기 엔트리를 **자기 핸들러**의 `RemoveAgentChecked(Handle)`로 제거하려 하고, 콜백이 무엇을 했든 **엔트리를 무조건 무효화**한다 (`MassReplicationProcessor.h:176-191`).
3. 핸들은 발급한 핸들러 안에서만 의미가 있다 (§3.4).

**진짜 불변식은 "버블 클래스 1개"가 아니라 "핸들 발급자 1개"다.**
`AgentHandleManager`는 `TClientBubbleHandlerBase`의 **인스턴스 멤버**이므로(`MassClientBubbleHandler.h:221`), 발급자를 세는 단위는 버블 Actor가 아니라 **핸들러 인스턴스**입니다. 그래서 위 주석의 두 번째 제안 — 버블 클래스는 하나로 두고 그 안에 타입별 핸들러를 여러 개 두는 구성 — **도 똑같이 깨집니다.** 버블이 하나여도 핸들러가 둘이면 `AgentHandleManager`가 둘이고, 파괴 루프는 여전히 클라이언트 장부 전체를 순회하기 때문입니다.

발급자가 2개 이상이면 세 가지 증상이 조합됩니다:

- **크래시**: A 타입 리플리케이터가 B 타입 엔트리의 핸들로 자기 핸들러를 제거 시도 → `checkf(AgentHandleManager.IsValidHandle(Handle))` 실패 (`MassClientBubbleHandler.h:322`). 엔진 자신의 MassCrowd도 방어 없이 `RemoveAgentChecked`를 씁니다 (`MassCrowdReplicator.cpp:60`).
- **Silent corruption**: 두 발급자의 핸들 번호 체계가 독립(0부터 시작)이라, 남의 핸들이 **내 쪽에서 우연히 유효**할 수 있음 → 엉뚱한 에이전트를 조용히 제거. 크래시보다 나쁩니다.
- **유령 + 릭**: 4-인자 Remove 콜백으로 "내 소유가 아니면 skip" 가드를 넣어도 소용없습니다. 엔진이 콜백 직후 `AgentData.Invalidate()`를 **무조건** 부르므로, 진짜 주인은 제거 기회를 영영 잃습니다 → 클라 유령 개체 잔존 + 서버 슬롯 릭.
  ⚠️ **이것이 "탈출구가 없다"는 뜻입니다.** 4-인자 오버로드는 소유 판별의 재료(엔티티 핸들)를 주지만, 판별에 성공해도 무효화를 막을 수 없습니다. 엔진 수정 없이 다중 발급자를 안전하게 만드는 방법은 확인되지 않았습니다.

**권장 구성** — 나눠야 할 축과 나누면 안 되는 축이 다릅니다:

| 요소 | 개수 | 근거 |
|:---|:---|:---|
| 버블 클래스 | 월드당 1개 | 이 절 |
| 버블 핸들러 (= FastArray) | 버블당 1개 | 이 절 — **진짜 제약은 여기다** |
| ReplicatedAgent 구조체 | 1개 (공용) | 핸들러가 하나면 자동으로 따라온다 |
| **리플리케이터 클래스** | **타입마다 1개** | **§3.8 · §7.11 — 여기만 나눠야 한다** |

§5에서 봤듯 TemplateID 그룹핑이 이종 아키타입 스폰을 자동 처리하므로, 통합 버블은 엔진이 자연스럽게 지원하는 구성입니다. 공용 구조체 하나로 타입별 차이를 흡수하는 방법:

- 서버: 타입 전용 Fragment를 `EMassFragmentPresence::Optional`로 쿼리에 추가하고, 청크의 Fragment 뷰가 비었는지로 분기.
- 클라: `SetSpawnedEntityData`의 `FMassEntityView::GetFragmentDataPtr<T>()`로 조건부 기록.

이렇게 하면 파괴 루프의 실행자와 핸들 소유자가 항상 일치해 세 증상이 모두 원천 차단됩니다.

**실무 파급 — 이 제약이 봉쇄하는 최적화가 있습니다.** Iris FastArray의 체인지마스크는 원소를 63비트에 `ElementIt % 63`으로 접어 쓰므로(`ArrayPropertyNetSerializer.cpp:121-124`), 배열 하나에 **갱신이 잦은 타입과 정적인 타입이 섞여 있으면 정적 원소가 남의 dirty에 끌려 함께 재직렬화됩니다.** 이를 푸는 자연스러운 수단이 "타입별 FastArray 분리"인데, 그것이 곧 핸들러 분리이고 위 결함에 정면으로 걸립니다. **UE 5.8에서는 엔진 수정 없이 이 길이 막혀 있습니다** — 대역폭 대책은 원소 수(컬 거리)·갱신 빈도·원소 크기 쪽에서 찾아야 합니다.

### 7.2 FMassReplicatedAgentHandle을 버블 밖으로 유통하지 말 것

핸들은 발급한 버블에서만 유효합니다. 저장·전달이 필요하면 NetID나 엔티티 핸들을 쓰고, 버블 접근 직전에 `NetworkIDToAgentHandleMap`으로 변환하십시오.

### 7.3 RegisterBubbleInfoClass는 첫 클라이언트 접속 전에

`AddClient`/`SynchronizeClientsAndViewers` 이후 호출은 checkf로 거부됩니다. 월드 초기화 초기에(예: WorldSubsystem Initialize) 등록해야 합니다. 같은 클래스 중복 등록은 경고 후 무시됩니다.

### 7.4 Agent 구조체 필드는 UPROPERTY 필수

Fast Array 직렬화는 리플렉션 기반입니다. `UPROPERTY()` 없는 필드는 조용히 전송에서 빠집니다. 또한 Serializer 구조체에는 `TStructOpsTypeTraits`로 `WithNetDeltaSerializer = true`를 반드시 지정해야 합니다.

### 7.5 스폰 시 위치를 1회는 반드시 실을 것

퍼펫 링크 시 엔진이 엔티티 Transform으로 Actor 위치를 초기화합니다. "위치는 Actor 복제가 따로 나르니까 버블엔 안 실어도 된다"고 생략하면, 링크 순간 Actor가 원점으로 튑니다. 위치 갱신이 불필요한 타입도 Add 시점 1회는 실어야 합니다.

### 7.6 클라 스폰 쿼리의 요구사항이 청크를 필터링한다

`PostReplicatedAdd`의 스폰 쿼리에 하드 요구사항을 넣으면, 그 Fragment가 없는 아키타입의 스폰 엔티티는 **순회에서 통째로 빠집니다**. 이 경우 NetID 기록까지 누락될 뿐 아니라, 내부의 `AgentsSpawnIdx` 증가가 어긋나 **에이전트-엔티티 대응이 전부 밀립니다**. 이종 타입을 한 버블로 다룰 때는 공통 Fragment만 요구하고, 타입별 데이터는 `FMassEntityView`로 접근하십시오.

### 7.7 ProcessClientReplication 안에서 클라이언트 전역 상태를 건드릴 때

이 함수는 (클라이언트 × SharedFragment × 청크) 횟수만큼 호출됩니다. "클라이언트당 1회"를 가정한 로직(파괴 루프 같은 장부 순회)은 반드시 멱등이어야 합니다. 엔진의 파괴 루프는 엔트리 무효화로 멱등성을 확보합니다.

### 7.8 장부 클린업 checkf — 제거 없이 소멸하면 잡힌다

틱 말미의 클린업은 `bPendingDestruction` 또는 LOD Off 엔트리에 대해 `checkf(!AgentData.Handle.IsValid())`를 겁니다 (`MassReplicationProcessor.cpp:373`). 커스텀 리플리케이터가 Remove 콜백에서 핸들을 무효화 경로 없이 방치하면 여기서 터집니다 — 이 checkf는 "버블에서 제거되지 않은 채 장부에서 사라지는" 릭을 잡는 안전망입니다.

### 7.9 ⚠️ 위치/Yaw 핸들러는 "월드 Z가 Up"을 가정한다 — 비평면 중력 월드에서 쓸 수 없다

`TMassClientBubbleTransformHandler::SetEntityData`가 클라이언트에서 자세를 복원하는 방식 (`MassReplicationTransformHandlers.h`):

```cpp
TransformFragment.GetMutableTransform().SetRotation(FQuat(FVector::UpVector, ReplicatedPositionYawData.GetYaw()));
```

서버 측 `SetBubblePositionYawFromTransform`도 대칭적으로 `Transform.GetRotation().Rotator().Yaw` — **월드 Yaw**만 싣습니다. 즉 이 핸들러 쌍은 "모든 엔티티의 Up은 월드 +Z"라는 평지 전제 위에 서 있습니다. Pitch·Roll은 애초에 페이로드에 없어 복원이 불가능합니다.

구면 중력·벽면 보행처럼 Up이 위치마다 달라지는 월드에서는 복제된 엔티티가 전부 월드 Z 기준으로 서므로, **적도 부근에서 통째로 눕습니다.** 서버 화면은 로컬 Transform을 그대로 쓰므로 멀쩡해 보여 원인 파악이 늦어지기 쉽습니다.

**대응 선택지:**

| 방식 | 대역폭 | 특이점 |
|:---|:---|:---|
| Yaw를 **접평면 로컬 기준**으로 인코딩, 기저는 서버·클라가 각자 위치에서 재구성 | +0 | 있음 (아래) |
| 압축 쿼터니언/Rotator를 페이로드에 추가 | 갱신마다 증가 | 없음 |

로컬 Yaw 방식은 기저 함수의 연속성에 의존합니다. `FRotationMatrix::MakeFromZ`는 `|Z.Z| ≥ 1 - UE_KINDA_SMALL_NUMBER`에서 참조 벡터를 `(0,0,1)`→`(1,0,0)`으로 하드 전환하므로 (`RotationMatrix.h:104`), 그 경계면 위의 엔티티는 위치 복제 허용 오차(`PositionReplicateTolerance` = 1cm) 때문에 서버·클라가 서로 다른 기저를 잡아 Yaw가 크게 어긋날 수 있습니다. Up 축 자체는 위치에서 직접 나오므로 **눕지는 않고 방향만 틀어집니다.**

다만 **구 전체에서 연속인 접평면 기저는 존재하지 않으므로**(hairy ball theorem) 어떤 함수를 골라도 특이점은 남습니다 — 위치를 옮기거나(플레이 공간 밖으로) 크기를 줄일 수 있을 뿐입니다. `FQuat::FindBetweenNormals`는 특이점이 링이 아닌 한 점이라 더 작습니다.

LootNPop 적용 사례는 `TechDesign_Networking.md` §3.5 참조.

---

### 7.10 ⚠️ 리슨 서버는 파괴 옵저버를 통째로 잃는다 — "게스트에서만 안 사라지는" 버그의 정체

**증상:** 호스트 화면은 멀쩡한데 **게스트 화면에서만** 죽은 NPC와 루팅이 끝난 LootPod이 영원히 남는다.
추가는 정상이고 **제거만** 안 된다. 호스트만 확인하면 절대 못 잡는다.

**인과 사슬:**

```
UEngine::LoadMap: InitWorld() → Listen(URL)        ← 엔진 고정 순서
  └ UMassEntitySubsystem::Initialize (월드 서브시스템)
      └ FMassObserverManager::Initialize()
          └ GetProcessorExecutionFlagsForWorld(World)   ← 이 시점 GetNetMode()는 NM_Standalone
              → Standalone 플래그는 Server를 포함하지 않음
              → UMassReplicationEntityDestructionObserver 인스턴스화 안 됨
                  → UMassReplicationSubsystem::NotifyEntityDestroyed 영영 호출 안 됨
                      → FMassReplicatedAgentData::bPendingDestruction 안 걸림
                          → 리플리케이터의 RemoveEntityCallback 0건
                              → 클라 버블에서 에이전트가 영원히 안 빠짐
```

호스트가 멀쩡해 보이는 이유: 호스트는 자기 Mass 엔티티를 `DestroyEntity`로 직접 파괴하고 그 결과를
바로 렌더링한다. 클라 버블은 **원격 클라이언트에게만** 필요한 경로다.

**왜 추가는 되는데 제거만 안 되나:** `UMassReplicationProcessor`(일반 프로세서)는 처리 페이즈 매니저가
더 늦게 — 넷 모드가 이미 `NM_ListenServer`인 시점에 — 구성하므로 정상 등록된다. 옵저버 매니저만
엔티티 매니저와 함께 이른 시점에 한 번 굳는다.

**실측 (2026-08-20, 2P Standalone):** 호스트 `RemoveEntityCallback` **0건**, 게스트
`PreReplicatedRemove`는 매 델타 호출되지만 전부 `0 removed`, 버블 에이전트 **235개 누적**.

**해결:** 넷 모드가 이미 확정된 `OnWorldBeginPlay`에서 걸러진 옵저버를 다시 단다 —
`ULNPMassSpawnSubsystem::RestoreServerOnlyMassObservers()`.
`FMassObserverManager::AddObserverInstance(UMassObserverProcessor*)`가 `ObservedTypes`/
`ObservedOperations`를 읽고 `CallInitialize`까지 수행하므로 엔진 등록 경로와 동일한 상태가 된다.
수정 후 같은 시나리오에서 서버 발행 → 게스트 수신 → 엔티티 파괴 전 구간 성립을 확인했다.

> **매니저 통째 재초기화는 불가능하다.** `FMassObserverManager::Initialize`/`DeInitialize`는
> `protected`(`friend FMassEntityManager`)이고, `UMassObserverRegistry::ElementObserverMaps`도
> `protected`라 등록 목록을 열거할 수도 없다. 공개된 것은 `AddObserverInstance`/
> `RemoveObserverInstance`/`DebugGatherUniqueProcessors`뿐이다.
> 다행히 **MassGameplay 플러그인 전체에서 Server/Client로 좁혀진 프로세서는 이 옵저버 하나뿐**이라
> (`MassReplicationProcessor.cpp:384`) 표적 복구가 곧 전체 복구다.
> ⚠️ 엔진 업데이트로 이 목록이 늘면 복구 대상도 같이 늘려야 한다.

**같은 뿌리의 다른 증상:** 이 넷 모드 오판은 이 프로젝트를 두 번 물었다. 첫 번째는
`UMassReplicationTrait::BuildTemplate`이 Standalone에서 조기 반환해 **복제 프래그먼트가 빠진 템플릿이
캐시**된 것이고(그래서 템플릿 warm-up도 `Initialize()`가 아니라 `OnWorldBeginPlay`에 있다),
두 번째가 이 옵저버 누락이다. **`Initialize()` 계열에서 `GetNetMode()`에 의존하는 코드는 전부 의심하라.**
`PlayInEditorNetMode` 폴백 덕에 **PIE에서는 재현되지 않고 `-game`에서만 드러난다.**

### 7.11 ⚠️ `FMassReplicationSharedFragment`의 중복 제거 CRC는 LOD 거리를 보지 않는다 (가장 조용한 함정)

**`FMassReplicationParameters`를 타입마다 다르게 채워도, `ReplicatorClass`가 같으면 전부 하나로 합쳐진다.**
크래시도 경고도 화면상 증상도 없다. 실측 전까지 드러나지 않는다.

`UMassReplicationTrait::BuildTemplate` (`MassReplicationTrait.cpp:44`):

```cpp
FMassReplicationSharedFragment ReplicationFragment(*ReplicationSubsystem, Params);
FSharedStruct SharedFragment = EntityManager.GetOrCreateSharedFragment(
    *FMassReplicationSharedFragment::StaticStruct(), reinterpret_cast<uint8*>(&ReplicationFragment));
```

이 오버로드는 `UE::StructUtils::GetStructInstanceCrc32`로 CRC를 만들어 `FindOrAdd`한다
(`MassEntityManager.h:1183`). CRC는 `UScriptStruct::SerializeItem`, 즉 **태그드 프로퍼티만** 훑는다.

그런데 `FMassReplicationSharedFragment`(`MassReplicationFragments.h`)의 UPROPERTY는 둘뿐이다:

| 멤버 | UPROPERTY | 빌드 시점 값 |
|:---|:---:|:---|
| `BubbleInfos` | ✅ | 빈 배열 (클라이언트 접속 전이라 항상 비어 있다) |
| `CachedReplicator` | ✅ | `Params.ReplicatorClass.GetDefaultObject()` = **CDO** |
| `LODCollector` / `LODCalculator` | ❌ | **`Params.LODDistance`가 여기 들어간다** |
| `BubbleInfoClassHandle` | ❌ | |
| `EntityQuery` | ❌ | |

→ 리플리케이터 클래스가 같으면 **`LODDistance`가 아무리 달라도 CRC가 동일**하고,
`FindOrAdd`는 **먼저 만들어진 공유 프래그먼트 하나**를 모든 타입에 돌려준다.
결과적으로 **가장 먼저 빌드된 타입의 컬 거리가 전 타입에 적용된다.**

**증상의 모양.** 컬 거리가 가장 큰 타입이 먼저 빌드되면 모든 엔티티가 모든 클라이언트 버블에 들어간다.
그 반대면 멀리 있는 엔티티가 통째로 사라진다. 어느 쪽이든 **로그도 경고도 없다.**
LootNPop 실측(2026-09-02): LootPod 60,000cm가 적 12,000cm를 덮어써 반지름 250m 월드의 엔티티
478개 전부가 각 클라이언트에 복제됐고, **교전 없는 대기 상태 송신량이 1.1 MB/s**였다.

**대응 — 타입마다 `UMassReplicatorBase` 서브클래스를 따로 둔다.**
`CachedReplicator`는 CRC에 들어가고 `GetStructInstanceCrc32`는 오브젝트를 **포인터 기준**으로
해싱하므로(`FHashBuilder::EObjectHashingMode::ObjectPtrBasedCRC`), CDO가 다르면 CRC가 갈린다.
동작이 완전히 같은 빈 서브클래스로 충분하다 — 존재 이유가 해시 분리뿐임을 주석으로 못 박을 것.

**이 대응은 우회가 아니라 정석 복귀다.** §3.8에서 봤듯 엔진이 타입을 가르라고 설계한 축이
바로 리플리케이터 클래스이고, 정석대로 타입마다 서브클래스를 두었다면 이 결함은 **애초에 발화하지 않는다.**
즉 이 함정은 "여러 타입이 리플리케이터 하나를 공유한다"는 비정석 구성에서만 드러난다.

> 다만 동기는 구분해 두는 편이 정확하다. 정석에서 서브클래스를 나누는 이유는 *쿼리 요구사항이 달라서*이고
> (§3.8 ③), 여기서 나누는 이유는 *해시 분리*다. 요구사항이 동일한 타입들이라면 결과물은
> 동작이 같은 빈 클래스 N개가 된다 — 형태는 정석, 동기는 우회다.

⚠️ **§7.1(발급자는 하나로)과 충돌하지 않는다.** §7.1의 불변식은 *핸들 발급자(버블 핸들러 인스턴스)*에
관한 것이다. 리플리케이터가 여럿이어도 핸들러가 하나면 파괴 루프가 만지는 핸들은 항상 그 하나가
발급한 것이라 유효하고, 처리 후 `AgentData.Invalidate()`가 걸려 2회차 이후는 건너뛴다 — **결과는 멱등이다.**
대가는 §4의 O(N × 장부 크기) 순회 비용뿐이다.

**실측 (2026-09-03, LootNPop `-game` 리슨 서버 + 게스트 1, 리플리케이터 3개 / 핸들러 1개):**
게스트·호스트가 각각 NPC 처치와 LootPod 루팅 완료를 수십 회 반복 — **크래시·유령·릭 없음.**
과거 다중 핸들러 시절 크래시가 나던 조건("게스트로 Pod 루팅")도 통과했다.

> ⚠️ **위험 축을 오해하기 쉽다 — "동시 파괴"는 위험 요인이 아니다.**
> 파괴 루프는 1회차에서 제거·`Invalidate()`하고 2회차 이후는 `Handle.IsValid()`가 false여서 건너뛰므로,
> 몇 개가 같은 틱에 죽든 결과가 같다. 발화 조건은 "리플리케이터 ≥2 + 파괴 1건"이고 위 실측이 그것이다.
> **남은 미검증 축은 "클라이언트 장부가 2개 이상"** — 리슨 호스트는 로컬 컨트롤러라 버블이 없으므로
> 호스트+게스트 1 구성의 장부는 1개뿐이다. 게스트를 2개 붙이면 클라이언트 루프 × 리플리케이터 N회
> 순회가 처음으로 교차한다(조작은 한쪽만 해도 성립).

> **엔진 트레잇 상속으로는 못 고친다.** 해시 원본을 따로 받는 오버로드
> (`GetOrCreateSharedFragment(FConstStructView HashingHelperStruct, TArgs&&... InArgs)`,
> `MassEntityManager.h:1272`)가 정확히 이 상황을 위해 존재하지만,
> `FMassReplicationSharedFragment`의 생성자가 `MASSREPLICATION_API`로 export되지 않아
> 외부 모듈에서 호출하면 `LNK2019`가 난다.

**일반화 — 이 함정은 이 구조체만의 문제가 아니다.**
`GetOrCreateSharedFragment`의 "인스턴스 메모리로 CRC" 오버로드를 쓰는 모든 공유 프래그먼트에서,
**구성값이 UPROPERTY가 아니면 그 값은 중복 제거에 반영되지 않는다.**
공유 프래그먼트를 설계할 때는 "이 값이 달라지면 별개 인스턴스여야 하는가"를 묻고,
그렇다면 그 값은 반드시 UPROPERTY이거나 해시 원본을 따로 넘겨야 한다.

---

## 8. 최소 구현 체크리스트

새 프로젝트에서 Mass 엔티티 복제를 붙일 때 필요한 것:

1. **공통(서버·클라)**: ReplicatedAgent 구조체, FastArrayItem, BubbleHandler, BubbleSerializer(+TStructOpsTypeTraits), BubbleInfo Actor(생성자에서 `Serializers.Add`, `GetLifetimeReplicatedProps`에서 `DOREPLIFETIME` 등록)
2. **서버**: Replicator 클래스 (`AddRequirements` + `ProcessClientReplication`) — **복제 타입마다 하나씩** (§3.8·§7.11). 로직이 동일한 타입들이라면 공통 베이스에 구현하고 빈 서브클래스만 타입 수만큼 둡니다.
3. **클라**: BubbleHandler의 `PostReplicatedAdd`/`PostReplicatedChange` 구현 (`PostReplicatedAddHelper` 활용)
4. **배선**: 엔티티 템플릿에 `UMassReplicationTrait` 추가 (Params에 BubbleInfoClass/ReplicatorClass 지정 — **BubbleInfoClass는 전 타입 공통, ReplicatorClass는 타입마다 다르게**), 월드 초기화 시 `RegisterBubbleInfoClass` 호출.
   ⚠️ 타입을 새로 추가하면서 ReplicatorClass를 빠뜨리면 **그 타입의 LOD 설정이 조용히 무시됩니다** — 크래시도 경고도 없으므로, Params를 채우는 헬퍼에 `checkf`로 베이스 클래스 직접 사용을 막아 두는 것을 권합니다.
5. **코드 분리**: 서버 전용 경로는 `UE_REPLICATION_COMPILE_SERVER_CODE`, 클라 전용 경로는 `UE_REPLICATION_COMPILE_CLIENT_CODE`로 감싸기 (에디터 빌드는 둘 다 컴파일됨)
6. **리슨 서버 보정**: `OnWorldBeginPlay`에서 ① 엔티티 템플릿 warm-up ② `UMassReplicationEntityDestructionObserver` 복구.
   둘 다 "월드 초기화 시점의 `GetNetMode()`가 `NM_Standalone`"이라는 같은 함정 대응이다 (§7.10).
   **`-game` 리슨 서버로 검증하지 않으면 둘 다 드러나지 않는다.**

복제 타입이 여러 개일 때의 안전한 기본 구성을 한 줄로 요약하면:

> **버블 클래스와 핸들러(FastArray)는 월드당 하나**로 두어 파괴 경로의 결함을 피하고 (§7.1),
> **리플리케이터 클래스는 타입마다 하나**로 나누어 LOD 설정이 실제로 걸리게 하며 (§3.8·§7.11),
> 페이로드의 타입 차이는 Optional Fragment와 `FMassEntityView`로 흡수한다.

두 축을 같은 방향으로 밀면(둘 다 하나 / 둘 다 여럿) 반드시 한쪽에서 대가를 치릅니다 —
전자는 컬 거리 무력화, 후자는 파괴 시 크래시입니다.
