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

### 3.4 FMassReplicatedAgentHandle — 버블-로컬 핸들

서버 측 버블은 자기 배열의 슬롯을 가리키는 핸들을 발급합니다(`AgentHandleManager`, 인덱스 0부터 시작하는 프리 리스트). **이 핸들은 발급한 버블 안에서만 의미가 있습니다.** 서로 다른 버블(클래스가 다르든, 클라이언트가 다르든)의 핸들 `{Index=0}`은 완전히 다른 대상을 가리킵니다.

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
- `FMassReplicationSharedFragment`: `CachedReplicator` 인스턴스, 리플리케이터의 `AddRequirements`로 구성된 `EntityQuery`, LOD 계산기를 보유. Shared Fragment이므로 **Params가 값-동일하면 여러 템플릿이 공유**하고, 다르면(예: LOD 거리 차이) 별도 인스턴스가 생깁니다.
- `NM_Standalone`에서는 `BuildTemplate`이 조기 반환합니다 — 싱글플레이 분기를 따로 만들 필요가 없습니다.

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
- `ProcessClientReplication`은 **청크 단위로 여러 번 호출**됩니다. 따라서 그 안의 파괴 루프도 여러 번 돌 수 있는데, 처리한 엔트리를 즉시 무효화하므로 멱등입니다.

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
- LOD 거리·최대 개체 수는 `FMassReplicationParameters`로 타입별 설정이 가능합니다.
- ⚠️ **기본 `LODDistance[Off]`는 5,000cm**입니다 (`MassReplicationFragments.cpp:62`). 월드 스케일이 이보다 크면 클라이언트가 받는 범위가 서버 화면(로컬이라 시각화 LOD 거리까지 다 보임)보다 훨씬 좁아져, "서버엔 보이는데 클라엔 근접해야만 보임"으로 나타납니다. 시각화 트레잇의 `VisibleLODDistance[Off]`와 짝을 맞추는 것이 기본입니다. 개수 캡 `LODMaxCountPerViewer`(기본 Low=300)가 거리보다 먼저 걸릴 수 있다는 점도 함께 확인해야 합니다 — `AdjustLODFromCount`가 캡에 맞춰 거리를 되레 줄입니다.

---

## 7. 작업 시 유의점 (Pitfalls)

### 7.1 ⚠️ 복제 스트림은 월드당 하나만 — 다중 버블 클래스의 파괴 경로는 깨져 있다 (가장 중요)

`RegisterBubbleInfoClass` 주석(`MassReplicationSubsystem.h:197-204`)은 "타입별로 다른 버블에 복제할 수 있다"고 하지만, **엔티티 파괴 경로가 이 구성을 깨뜨립니다.** 근거 체인:

1. 파괴 통지는 타입 무구분 — `NotifyEntityDestroyed`는 클라 장부에서 엔티티를 찾아 `bPendingDestruction`만 켠다 (`MassReplicationSubsystem.cpp:592`).
2. 각 리플리케이터의 파괴 루프는 **장부 전체(모든 타입)를 순회**하며, 만나는 모든 파괴 대기 엔트리를 **자기 버블**의 `RemoveAgentChecked(Handle)`로 제거하려 하고, 처리한 엔트리를 **무조건 무효화**한다 (`MassReplicationProcessor.h:176-191`).
3. 핸들은 버블-로컬이다 (§3.4).

버블 클래스가 2개 이상이면 세 가지 증상이 조합됩니다:

- **크래시**: A 타입 리플리케이터가 B 타입 엔트리의 핸들로 자기 버블을 제거 시도 → `checkf(AgentHandleManager.IsValidHandle(Handle))` 실패 (`MassClientBubbleHandler.h:322`).
- **Silent corruption**: 두 버블의 핸들 번호 체계가 독립(0부터 시작)이라, 남의 핸들이 **내 버블에서 우연히 유효**할 수 있음 → 엉뚱한 에이전트를 조용히 제거. 크래시보다 나쁩니다.
- **유령 + 릭**: 4-인자 Remove 콜백으로 "내 소유가 아니면 skip" 가드를 넣어도, 엔진이 엔트리를 무효화해버리므로 **진짜 주인 버블은 제거 기회를 영영 잃습니다** → 클라이언트에 유령 개체 잔존 + 서버 버블 슬롯 릭.

**권장**: 복제할 엔티티 타입이 여럿이어도 **버블 클래스·리플리케이터를 월드당 하나로 통합**하십시오. §5에서 봤듯 TemplateID 그룹핑이 이종 아키타입 스폰을 자동 처리하므로, 통합 버블은 엔진이 자연스럽게 지원하는 구성입니다. 타입별 차이는:

- 서버: 타입 전용 Fragment를 `EMassFragmentPresence::Optional`로 쿼리에 추가하고, 청크의 Fragment 뷰가 비었는지로 분기.
- 클라: `SetSpawnedEntityData`의 `FMassEntityView::GetFragmentDataPtr<T>()`로 조건부 기록.

이렇게 하면 파괴 루프의 실행자와 핸들 소유자가 항상 일치해 세 증상이 모두 원천 차단됩니다. (참고로 Epic의 실전 사용례인 CitySample 군중도 복제 스트림이 하나입니다.)

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

## 8. 최소 구현 체크리스트

새 프로젝트에서 Mass 엔티티 복제를 붙일 때 필요한 것:

1. **공통(서버·클라)**: ReplicatedAgent 구조체, FastArrayItem, BubbleHandler, BubbleSerializer(+TStructOpsTypeTraits), BubbleInfo Actor(생성자에서 `Serializers.Add`, `GetLifetimeReplicatedProps`에서 `DOREPLIFETIME` 등록)
2. **서버**: Replicator 클래스 (`AddRequirements` + `ProcessClientReplication`)
3. **클라**: BubbleHandler의 `PostReplicatedAdd`/`PostReplicatedChange` 구현 (`PostReplicatedAddHelper` 활용)
4. **배선**: 엔티티 템플릿에 `UMassReplicationTrait` 추가 (Params에 BubbleInfoClass/ReplicatorClass 지정), 월드 초기화 시 `RegisterBubbleInfoClass` 호출
5. **코드 분리**: 서버 전용 경로는 `UE_REPLICATION_COMPILE_SERVER_CODE`, 클라 전용 경로는 `UE_REPLICATION_COMPILE_CLIENT_CODE`로 감싸기 (에디터 빌드는 둘 다 컴파일됨)

복제 타입이 여러 개라도 **버블·리플리케이터는 하나만** 만들고 (§7.1), 타입 차이는 Optional Fragment와 EntityView로 흡수하는 것이 안전한 기본 구성입니다.
