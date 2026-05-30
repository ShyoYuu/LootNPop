# Engine Analysis - Unreal Engine 5.7 Mass Entity Framework

## 1. 개요 (Overview)

Mass Entity는 언리얼 엔진 5에서 도입된 **데이터 지향 설계(Data-Oriented Design)** 기반의 대규모 엔티티 시뮬레이션 프레임워크입니다. ECS(Entity-Component-System) 아키텍처를 채택하여 수천 ~ 수십만 개의 엔티티를 CPU 캐시 효율을 극대화하며 처리할 수 있도록 설계되었습니다.

핵심 철학:
- **데이터와 로직의 분리**: 데이터(Fragment)와 그것을 처리하는 로직(Processor)이 완전히 분리됩니다.
- **캐시 친화적 메모리 레이아웃**: 같은 Fragment 조합을 가진 엔티티들을 연속된 메모리 블록(Archetype Chunk)에 묶어 처리합니다.
- **병렬 실행**: 데이터 의존성이 없는 Processor들은 워커 스레드에서 동시에 실행됩니다.

---

## 2. 핵심 데이터 구조 (Core Data Structures)

### 2.1 Fragment (`FMassFragment`)

엔티티가 보유하는 데이터 단위입니다. C++ 구조체로 선언하며, UE의 리플렉션 시스템에 등록됩니다.

```cpp
USTRUCT()
struct MYPROJECT_API FMyFragment : public FMassFragment
{
    GENERATED_BODY()

    float Health = 100.f;
    FVector Velocity = FVector::ZeroVector;
};
```

- Fragment 자체는 로직을 포함하지 않습니다. 순수 데이터입니다.
- 엔티티에 Fragment를 추가/제거하면 Archetype이 변경됩니다 (비용 있음, 가급적 초기화 시 한 번에 구성).
- `FTransformFragment`는 엔진 내장 위치/회전/스케일 Fragment입니다.

### 2.2 Tag (`FMassTag`)

데이터가 없는 플래그성 Fragment입니다. 메모리를 차지하지 않고 엔티티의 Archetype만 바꿉니다.

```cpp
USTRUCT()
struct MYPROJECT_API FMyDeadTag : public FMassTag
{
    GENERATED_BODY()
};
```

- 조건부 쿼리 필터링에 사용합니다 (`AddTagRequirement`).
- 런타임에 Deferred Command로 추가/제거하는 비용이 Fragment보다 낮습니다.
- **상태를 표현하는 가장 가벼운 수단**입니다.

### 2.3 Shared Fragment (`FMassSharedFragment`)

같은 설정값을 공유하는 엔티티 그룹이 함께 참조하는 읽기 전용 데이터입니다.

```cpp
USTRUCT()
struct MYPROJECT_API FMySharedFragment : public FMassSharedFragment
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UMyConfig> Config = nullptr;

    float Damage = 10.f;
};
```

- 같은 Shared Fragment 인스턴스를 가진 엔티티들은 동일 Archetype Chunk에 묶입니다.
- 인스턴스는 값 기반으로 해싱되어 중복 없이 관리됩니다.
- Processor에서는 `AddConstSharedRequirement<T>()` + `Ctx.GetConstSharedFragment<T>()`로 접근합니다.

#### ⚠️ 해시 키는 `UPROPERTY` 필드만 포함됩니다

`GetOrCreateConstSharedFragment()`는 내부적으로 `UE::StructUtils::GetStructInstanceCrc32()` → `UScriptStruct::SerializeItem()`을 호출합니다. 이 직렬화는 **`UPROPERTY()`로 선언된 필드만** CRC에 포함합니다.

**결과**: `UPROPERTY()` 없는 필드는 해시 키에 포함되지 않으므로, 해당 값이 달라도 같은 인스턴스로 판정되어 엔티티가 잘못 묶일 수 있습니다.

```cpp
// 잘못된 예 — Damage와 HitRadiusSq가 달라도 같은 인스턴스로 중복 제거됨
USTRUCT()
struct FMySharedFragment : public FMassConstSharedFragment
{
    GENERATED_BODY()

    UPROPERTY()                              // ✅ 해시 포함
    TObjectPtr<UMyConfig> Config = nullptr;

    float Damage      = 10.f;               // ❌ 해시 제외 (UPROPERTY 없음)
    float HitRadiusSq = 25.f;               // ❌ 해시 제외
};

// 올바른 예
USTRUCT()
struct FMySharedFragment : public FMassConstSharedFragment
{
    GENERATED_BODY()

    UPROPERTY() TObjectPtr<UMyConfig> Config = nullptr;
    UPROPERTY() float Damage      = 10.f;   // ✅ 해시 포함
    UPROPERTY() float HitRadiusSq = 25.f;   // ✅ 해시 포함
};
```

**확인 방법**: `UScriptStruct::SerializeItem` → `FProperty::SerializeItem`이 처리하는 것은 리플렉션 등록 프로퍼티뿐임은 UE 소스 `StructUtilsTypes.cpp:GetStructInstanceCrc32`에서 확인됨.

### 2.4 Archetype과 Chunk

- **Archetype**: Fragment + Tag + Shared Fragment의 조합으로 결정되는 엔티티 유형.
- **Chunk**: Archetype별로 엔티티 데이터가 저장되는 연속된 메모리 블록 (~128KB).
- `ForEachEntityChunk`는 Chunk 단위로 반복하므로, 같은 Archetype의 엔티티를 캐시 히트율 높게 처리합니다.

```
[Archetype A: FTransformFragment + FEnemyFragment + FEnemyTag]
  Chunk 0: [Transform0, Transform1, ..., Transform127]
           [Enemy0,     Enemy1,     ..., Enemy127    ]
  Chunk 1: [Transform128, ...]
           [Enemy128,    ...]
```

### 2.5 FMassEntityHandle

엔티티를 참조하는 경량 핸들입니다. `int32 Index`와 `int32 SerialNumber`로 구성됩니다.

```cpp
FMassEntityHandle Entity = Ctx.GetEntity(i);
bool bIsValid = Entity.IsValid();                   // null 핸들 체크
bool bIsAlive = EntityManager.IsEntityValid(Entity); // 실제 생존 여부 체크
```

- `IsValid()`는 핸들이 초기화되었는지만 확인합니다.
- `IsEntityValid()`는 EntityManager에 엔티티가 실제 존재하는지 확인합니다 (소멸 후에는 false).
- 소멸된 엔티티의 핸들을 재사용하면 SerialNumber 불일치로 잘못된 접근을 방지합니다.

---

## 3. Processor 구조 (`UMassProcessor`)

Processor는 쿼리 조건을 만족하는 엔티티에 대해 매 프레임 또는 특정 Phase에 로직을 실행하는 단위입니다.

### 3.1 기본 구조

```cpp
UCLASS()
class MYPROJECT_API UMyProcessor : public UMassProcessor
{
    GENERATED_BODY()
public:
    UMyProcessor();
protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
    FMassEntityQuery MyQuery;
};
```

### 3.2 Constructor 설정 항목

```cpp
UMyProcessor::UMyProcessor()
    : MyQuery(*this)
{
    // 실행 범위 (서버/클라이언트/에디터)
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;

    // 자동 등록
    bAutoRegisterWithProcessingPhases = true;

    // 게임 스레드 강제 실행 (Niagara, UObject 직접 조작 시 필수)
    bRequiresGameThreadExecution = true;

    // 실행 Phase (아래 섹션 참조)
    ProcessingPhase = EMassProcessingPhase::PrePhysics;

    // 그룹 내 순서
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
    ExecutionOrder.ExecuteAfter.Add(TEXT("OtherProcessorName"));
    ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
}
```

**`MyQuery(*this)` 이니셜라이저**: 쿼리를 프로세서 소유로 등록합니다. 반드시 이 형식을 사용해야 `RegisterWithProcessor`가 올바르게 작동합니다.

### 3.3 ConfigureQueries

쿼리 조건을 정의하고 프로세서에 등록합니다. 게임 시작 시 한 번 호출됩니다.

```cpp
void UMyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    // Fragment 요구 사항
    MyQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    MyQuery.AddRequirement<FMyFragment>(EMassFragmentAccess::ReadWrite);

    // Const Shared Fragment
    MyQuery.AddConstSharedRequirement<FMySharedFragment>(EMassFragmentPresence::All);

    // Tag 필터
    MyQuery.AddTagRequirement<FMyActiveTag>(EMassFragmentPresence::All);   // 반드시 있어야
    MyQuery.AddTagRequirement<FMyDeadTag>(EMassFragmentPresence::None);    // 없어야 함

    // 서브시스템 요구 사항 (접근 권한 선언 - 스레드 안전성 관련, 섹션 7 참조)
    ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

    // 반드시 마지막에 호출
    MyQuery.RegisterWithProcessor(*this);
}
```

### 3.4 Execute

실제 로직이 실행되는 곳입니다.

```cpp
void UMyProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    // 서브시스템 접근 (ProcessorRequirements에 등록된 것만 가능)
    UMassSignalSubsystem& SignalSub = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

    MyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
    {
        // Chunk 단위로 반복
        const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
        TArrayView<FMyFragment> Fragments = Ctx.GetMutableFragmentView<FMyFragment>();
        const FMySharedFragment& Shared = Ctx.GetConstSharedFragment<FMySharedFragment>();

        for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
        {
            const FMassEntityHandle Entity = Ctx.GetEntity(i);
            // 처리 로직...
        }
    });
}
```

---

## 4. 실행 스케줄링 (Execution Scheduling)

### 4.1 EMassProcessingPhase

UE 틱 그룹에 1:1로 매핑되는 **프레임 내 실행 시점**입니다.

| Phase | 틱 그룹 | 주요 용도 |
|---|---|---|
| `PrePhysics` | `TG_PrePhysics` | AI 인식, 타게팅, 이동 의도 결정, 이동 실행 |
| `StartPhysics` | `TG_StartPhysics` | 히트 판정, 발사체 이동, 시각화 |
| `DuringPhysics` | `TG_DuringPhysics` | 물리 시뮬레이션과 병렬 처리 필요한 작업 |
| `PostPhysics` | `TG_PostPhysics` | 체력/사망 처리, LOD, Representation 동기화 |
| `FrameEnd` | `TG_FrameEnd` | 최종 클린업 |

**Phase 미설정 시 기본값은 `PrePhysics`**입니다. `ExecuteInGroup`으로 그룹을 지정해도 Phase를 명시하지 않으면 PrePhysics에서 실행됩니다.

### 4.2 ProcessorGroupNames vs EMassProcessingPhase

두 개념은 **계층이 다릅니다**.

```
Phase (틱 그룹 = 프레임 내 시점)
  └─ Group (페이즈 내 버킷)
       └─ ExecuteBefore / ExecuteAfter (버킷 내 미세 순서)
```

**`ProcessorGroupNames`**: Phase 내부의 명명된 순서 버킷입니다. `ExecuteInGroup`으로 소속을 지정합니다.

| 그룹 이름 | 속하는 Phase | 용도 |
|---|---|---|
| `SyncWorldToMass` | PrePhysics | 월드 상태 → Mass 동기화 |
| `Behavior` | PrePhysics | StateTree, AI 판단 |
| `Tasks` | PrePhysics | StateTree Task 실행 |
| `Movement` | PrePhysics | 이동 처리 |
| `LOD` | PostPhysics | LOD 계산 |
| `Representation` | PostPhysics | Actor 스폰/디스폰 동기화 |
| `UpdateWorldFromMass` | PostPhysics | Mass → 월드 동기화 |

**중요**: 그룹을 지정해도 Phase를 명시하지 않으면 PrePhysics에서 실행됩니다. LOD/Representation 그룹에 속한 Processor도 `ProcessingPhase = EMassProcessingPhase::PostPhysics`를 반드시 명시해야 의도한 타이밍에 실행됩니다.

### 4.3 ExecuteAfter / ExecuteBefore

같은 Phase 내의 순서를 제어합니다. 다른 Phase의 Processor를 지정하면 무시됩니다.

```cpp
// Processor 이름으로 지정
ExecutionOrder.ExecuteAfter.Add(UOtherProcessor::StaticClass()->GetFName());

// 그룹 이름으로 지정 (해당 그룹 전체 이후에 실행)
ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
```

**크로스-Phase 순서 제약이 필요한 경우**: Phase를 다르게 지정하는 것으로 해결합니다. PrePhysics에서 출력한 데이터를 PostPhysics에서 읽는 것은 Phase 경계의 커맨드 버퍼 플러시로 자연스럽게 동기화됩니다.

### 4.4 Phase별 권장 Processor 배치

```
[PrePhysics]
  Behavior Group:  AI 인식(Scoring) 결과 읽기, 타게팅 결정, StateTree 실행
  Movement Group:  이동 의도 → 실제 이동 적용

[StartPhysics]
  발사체 이동, 히트 판정, 시각화(게임 스레드 필요 시 bRequiresGameThreadExecution = true)

[PostPhysics]
  UpdateWorldFromMass Group: AI 인식 스코어 계산 (이동 완료 후 → 다음 프레임에 사용)
  LOD Group: LOD 계산 및 오버라이드
  Representation Group: Actor 스폰/디스폰
  사망/소멸 처리 (피해는 StartPhysics에서 적용되었으므로 PostPhysics에서 체크)
  Actor 초기화
```

---

## 5. 지연 커맨드 버퍼 (Deferred Command Buffer)

### 5.1 개념

Mass는 Processor 실행 중 엔티티 구조 변경(생성, 소멸, Tag 추가/제거, Fragment 추가/제거)을 **즉시 처리하지 않습니다**. 대신 커맨드를 버퍼에 쌓아두고, **Phase가 끝날 때 일괄 처리**합니다.

```cpp
// Phase 실행 중 → 버퍼에 쌓임
Ctx.Defer().AddTag<FMyTag>(Entity);
Ctx.Defer().DestroyEntities(ToDestroyArray);
Context.Defer().DestroyEntity(SingleEntity);
```

### 5.2 플러시 타이밍

**Phase와 Phase 사이**에 커맨드 버퍼가 플러시됩니다.

```
PrePhysics Phase 실행
  └─ [커맨드 버퍼 플러시] ← PrePhysics에서 AddTag한 것이 여기서 적용
StartPhysics Phase 실행
  └─ [커맨드 버퍼 플러시]
DuringPhysics Phase 실행
  └─ [커맨드 버퍼 플러시]
PostPhysics Phase 실행
  └─ [커맨드 버퍼 플러시]
FrameEnd Phase 실행
  └─ [커맨드 버퍼 플러시]
```

**같은 Phase 내에서는** Tag/Fragment 추가 후 같은 Phase의 다른 Processor가 그것을 쿼리로 조회할 수 없습니다. 다음 Phase에서야 가시화됩니다.

이 특성을 이용해 Phase 경계를 동기화 장벽으로 활용할 수 있습니다.

### 5.3 FMassBatchedCommand — 게임 스레드 일괄 처리

Mass는 커맨드 버퍼 플러시를 **게임 스레드에서** 실행합니다. `FMassBatchedCommand`를 서브클래싱하면, 워커 스레드의 Processor에서 데이터를 안전하게 수집하고, 게임 스레드에서 UObject/Actor 조작을 일괄 수행하는 패턴을 구현할 수 있습니다.

```cpp
// .cpp 파일 로컬 정의 (헤더 노출 불필요)
struct FMyBatchedCommand : public FMassBatchedCommand
{
    struct FEntry { TWeakObjectPtr<AActor> Target; float Value; };

    FMyBatchedCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

    void Add(AActor* InTarget, float InValue)
    {
        Entries.Add({ InTarget, InValue });
        bHasWork = true; // ← 빠짐 없이 설정해야 Run()이 호출됨
    }

    virtual void Run(FMassEntityManager& EntityManager) override
    {
        // 게임 스레드 보장 — UObject, GAS 등 자유롭게 접근 가능
        for (const FEntry& E : Entries)
        {
            if (AActor* Actor = E.Target.Get())
                Actor->DoSomething(E.Value);
        }
    }

    virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
    virtual SIZE_T GetAllocatedSize() const override { return Entries.GetAllocatedSize(); }
    virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
    TArray<FEntry> Entries;
};

// 워커 스레드 Processor 내부에서:
Ctx.Defer().PushCommand<FMyBatchedCommand>(Target, Value);
```

**`bHasWork = true` 필수**: 이 플래그가 `false`이면 커맨드 버퍼가 `Run()`을 호출하지 않습니다.

**게임 스레드 보장 근거**: `FMassCommandBuffer::FlushAll()`은 `FMassEntityManager::FlushCommands()` → 각 Phase 종료 시 게임 스레드에서 호출됩니다 (`MassCommandBuffer.cpp` 확인).

#### 패턴 비교: FMassBatchedCommand vs 서브시스템 + 게임 스레드 Processor

| 기준 | FMassBatchedCommand | 서브시스템 큐 + 게임 스레드 Processor |
|---|---|---|
| **적합한 상황** | 일회성 Actor 상호작용 (GE 적용, 상태 전환) | 지속적 상태 관리 (Niagara 컴포넌트 맵) |
| **데이터 수명** | 커맨드 실행 후 즉시 해제 | 서브시스템 멤버로 프레임 간 유지 |
| **실행 타이밍** | Phase 종료 시 커맨드 버퍼 플러시 | 별도 게임 스레드 Processor 실행 시 |
| **코드 위치** | .cpp 로컬 struct (헤더 불필요) | 별도 헤더/서브시스템 클래스 필요 |
| **예시** | GAS GameplayEffect 적용, LootPod 상태 전환 | Niagara 트레일 컴포넌트 수명 관리 |

### 5.4 FMassDeferredSetCommand vs FMassDeferredAddCommand

```cpp
// FMassDeferredSetCommand: EntityManager를 const로 받음 (읽기/서브시스템 접근)
Context.Defer().PushCommand<FMassDeferredSetCommand>([entity](const FMassEntityManager& InManager)
{
    UMySubsystem* Sub = UWorld::GetSubsystem<UMySubsystem>(InManager.GetWorld());
    Sub->RegisterEntity(entity);
});

// FMassDeferredAddCommand: EntityManager를 non-const로 받음 (태그 추가 등 구조 변경)
Context.Defer().PushCommand<FMassDeferredAddCommand>([entity](FMassEntityManager& InManager)
{
    InManager.AddTagToEntity(entity, FMyTag::StaticStruct());
});
```

---

## 6. 엔티티 소멸 패턴 (Entity Destruction Patterns)

### 6.1 단순 소멸

```cpp
// 단일 엔티티
Context.Defer().DestroyEntity(Entity);

// 배열 (MoveTemp로 소유권 이전 권장)
TArray<FMassEntityHandle> ToDestroy;
// ... 수집
Context.Defer().DestroyEntities(MoveTemp(ToDestroy));
```

### 6.2 이중 소멸(Double Destroy) 크래시

**같은 Phase 내에서** 여러 Processor가 동일 엔티티에 `DestroyEntities`를 호출하면 힙 손상이 발생합니다. 이 버그는 엔티티가 동시에 여러 소멸 조건을 만족할 때 발생합니다.

```
StartPhysics Phase:
  MovementProcessor  → Entity X의 수명 만료 → Defer().DestroyEntities(X)  ← 첫 번째
  HitDetectionProcessor → Entity X가 적중   → Defer().DestroyEntities(X)  ← 두 번째 (크래시!)
```

**크래시 증상**: mimalloc의 `0xffffffffffffffff` free list sentinel이 덮어씌워지는 형태로, 보통 랜덤한 시점에 `EXCEPTION_ACCESS_VIOLATION`으로 나타납니다.

### 6.3 안전한 소멸: DeadTag + 전용 Processor 패턴

Mass-idiomatic 해결책은 **소멸 판정과 실제 소멸을 Phase로 분리**하는 것입니다.

**1단계**: Dead Tag 정의
```cpp
USTRUCT()
struct FMyDeadTag : public FMassTag { GENERATED_BODY() };
```

**2단계**: 소멸 판정 Processor들은 DestroyEntities 대신 DeadTag를 붙임
```cpp
// StartPhysics: MovementProcessor, HitDetectionProcessor 모두
Ctx.Defer().AddTag<FMyDeadTag>(Entity);
// → 같은 엔티티에 여러 번 AddTag해도 Tag는 하나만 붙음 (멱등)
```

**3단계**: HitDetection 쿼리에서 DeadTag 엔티티 제외
```cpp
ProjectileQuery.AddTagRequirement<FMyDeadTag>(EMassFragmentPresence::None);
```

**4단계**: PostPhysics에 전용 소멸 Processor
```cpp
// PostPhysics: StartPhysics 커맨드 버퍼 플러시 후 DeadTag 엔티티만 수집
void UMyDestructionProcessor::Execute(...)
{
    TArray<FMassEntityHandle> ToDestroy;
    DeadQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
    {
        for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
            ToDestroy.Add(Ctx.GetEntity(i));
    });
    if (ToDestroy.Num() > 0)
        Context.Defer().DestroyEntities(MoveTemp(ToDestroy));
}
```

**왜 안전한가**: AddTag는 멱등(idempotent)입니다. 동일 엔티티에 여러 Processor가 DeadTag를 붙여도 하나만 생성됩니다. 실제 `DestroyEntities`는 PostPhysics의 단 하나의 Processor에서만 호출됩니다.

---

## 7. 스레드 안전성과 서브시스템 접근

### 7.1 게임 스레드 vs 워커 스레드

`bRequiresGameThreadExecution = false`인 Processor는 Mass 스케줄러가 워커 스레드에서 실행할 수 있습니다. 이 경우 UObject 직접 접근, Niagara 컴포넌트 조작, `GetWorld()` 호출 등은 **스레드 안전하지 않습니다**.

워커 스레드에서 안전한 작업:
- Fragment 읽기/쓰기
- `TQueue<T, EQueueMode::Mpsc>`에 enqueue
- Deferred Command 추가
- 선언된 서브시스템 접근 (`Context.GetSubsystemChecked<T>()`)

### 7.2 서브시스템 접근 방법 비교

**방법 A: `ProcessorRequirements.AddSubsystemRequirement` + `Context.GetSubsystemChecked`**

```cpp
// ConfigureQueries
ProcessorRequirements.AddSubsystemRequirement<UMySubsystem>(EMassFragmentAccess::ReadWrite);

// Execute
UMySubsystem& Sub = Context.GetMutableSubsystemChecked<UMySubsystem>();
```

- Mass 스케줄러가 의존성을 인식하여 같은 서브시스템에 ReadWrite 접근하는 Processor를 동시에 실행하지 않습니다.
- `ReadOnly`: 여러 Processor 동시 읽기 허용.
- `ReadWrite`: 배타적 접근, 직렬 실행.
- **워커 스레드 Processor에서는 이 방식이 필수입니다.**

**방법 B: `GetWorld()->GetSubsystem<T>()`**

```cpp
UMySubsystem* Sub = GetWorld()->GetSubsystem<UMySubsystem>();
```

- 스케줄러가 의존성을 모릅니다.
- `bRequiresGameThreadExecution = true`인 게임 스레드 전용 Processor에서는 안전합니다.
- 게임 스레드 전용이어도 가능하면 방법 A를 권장합니다 (일관성, 명시성).

**방법 C: `EntityManager.GetWorld()->GetSubsystem<T>()`**

- 방법 B와 같은 한계를 가집니다.
- `UObject::GetWorld()` 대신 EntityManager에서 월드를 얻는 것이 Mass-idiomatic합니다.

### 7.3 요약 원칙

| 상황 | 권장 방법 |
|---|---|
| 워커 스레드 Processor (default) | `AddSubsystemRequirement` + `Context.GetSubsystemChecked<T>()` |
| 게임 스레드 전용 (`bRequiresGameThreadExecution = true`) | 위 방법 동일 권장, 또는 `GetWorld()->GetSubsystem<T>()` |
| StateTree Task 내부 | `Context.GetWorld()->GetSubsystem<T>()` (`FStateTreeExecutionContext::GetWorld()`는 유효) |

---

## 8. 월드 접근 방법 (`GetWorld()` vs `EntityManager.GetWorld()`)

```cpp
// Execute(FMassEntityManager& EntityManager, ...) 시그니처 내
UWorld* World = GetWorld();              // UObject 상속 - Outer 체인 경유
UWorld* World = EntityManager.GetWorld(); // Mass-idiomatic, 더 명시적
```

두 방법 모두 같은 월드를 반환하지만 `EntityManager.GetWorld()`가 권장됩니다. 이유:
- Processor가 올바르게 등록된 경우 두 결과는 동일하지만, EntityManager가 데이터의 직접적인 출처입니다.
- `ConfigureQueries` 등 `GetWorld()`를 쓸 수 없는 컨텍스트에서는 EntityManager가 유일한 방법입니다.

---

## 9. 쿼리 시스템 심화 (Query System)

### 9.1 요구사항 종류

```cpp
// Fragment 접근
query.AddRequirement<FMyFragment>(EMassFragmentAccess::ReadOnly);   // const 뷰
query.AddRequirement<FMyFragment>(EMassFragmentAccess::ReadWrite);  // mutable 뷰

// Const Shared Fragment (읽기 전용만 가능)
query.AddConstSharedRequirement<FMySharedFragment>(EMassFragmentPresence::All);
query.AddConstSharedRequirement<FMySharedFragment>(EMassFragmentPresence::Optional);

// Tag 필터
query.AddTagRequirement<FMyTag>(EMassFragmentPresence::All);      // 있어야 함
query.AddTagRequirement<FMyTag>(EMassFragmentPresence::None);     // 없어야 함
query.AddTagRequirement<FMyTag>(EMassFragmentPresence::Optional); // 있든 없든

// 서브시스템 (쿼리 레벨 - 청크 처리 범위에서의 접근 선언)
query.AddSubsystemRequirement<UMySubsystem>(EMassFragmentAccess::ReadWrite);
```

### 9.2 쿼리 레벨 vs ProcessorRequirements 서브시스템 선언

- **`ProcessorRequirements.AddSubsystemRequirement`**: Execute 전체 범위에서 사용. 람다 바깥에서 `Context.GetSubsystemChecked<T>()`로 접근.
- **`SomeQuery.AddSubsystemRequirement`**: ForEachEntityChunk 람다 안에서 `Ctx.GetSubsystemChecked<T>()`로 접근.

두 곳 모두에 선언하면 중복이지만 오류는 아닙니다. Execute 레벨에서 한 번 꺼내 람다에 캡처하는 패턴이 일반적입니다.

### 9.3 복수 쿼리 패턴

한 Processor에서 여러 쿼리를 실행할 수 있습니다. 일반적으로 첫 번째 패스에서 데이터를 수집하고 두 번째 패스에서 처리합니다.

```cpp
// Pass 1: 플레이어 위치 수집
TArray<FVector> PlayerPositions;
PlayerQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
{
    const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
    for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
        PlayerPositions.Add(Transforms[i].GetTransform().GetLocation());
});

// Pass 2: 적 처리
EnemyQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
{
    // PlayerPositions 활용
});
```

---

## 10. 엔티티 생명주기 (Entity Lifecycle)

### 10.1 엔티티 생성

Mass에는 **Entity Template**과 **Entity Config** 두 가지 생성 방식이 있습니다.

```cpp
// EntityManager로 직접 생성 (C++ 코드에서)
FMassEntityHandle NewEntity = EntityManager.CreateEntity(EntityTemplate.GetEntityTemplate());

// 초기 Fragment 값 설정 (생성 후 즉시)
FMyFragment& Frag = EntityManager.GetFragmentDataChecked<FMyFragment>(NewEntity);
Frag.Health = 100.f;
```

Mass Spawner / Entity Config Asset을 통한 대량 스폰이 일반적이며, `FMassEntityTemplateRegistry`로 템플릿을 관리합니다.

### 10.2 엔티티 소멸

섹션 6에서 다룬 DeadTag 패턴이 권장됩니다. 단일 Processor에서만 호출되는 것이 보장된다면 직접 `DestroyEntities`도 사용할 수 있습니다.

---

## 11. 실전 문제 해결 (Troubleshooting)

### 11.1 Processor가 실행되지 않는 경우

- **Phase 미설정**: 기본값 PrePhysics에서 실행되고 있을 수 있습니다. 로그나 Mass Debugger로 확인합니다.
- **쿼리 조건 불일치**: 엔티티가 요구 Fragment/Tag를 모두 가지고 있는지 확인합니다.
- **ExecutionFlags 불일치**: 서버 전용 Processor가 클라이언트에서 실행되지 않거나 그 반대인 경우.
- **`bAutoRegisterWithProcessingPhases = false`**: 수동 등록이 필요한 경우 별도 설정 필요.

### 11.2 Phase 간 데이터 가시성 문제

**증상**: A Processor에서 AddTag를 했는데 같은 Phase의 B Processor에서 해당 Tag가 보이지 않음.

**원인**: 커맨드 버퍼는 Phase가 끝날 때 플러시됩니다. 같은 Phase 내에서는 구조 변경이 즉시 반영되지 않습니다.

**해결**: Tag를 붙이는 Processor와 그것을 읽는 Processor를 다른 Phase에 배치합니다.

### 11.3 이중 소멸 크래시

**증상**: 빠른 반복 조작(광클, 다수 동시 히트) 시 `EXCEPTION_ACCESS_VIOLATION (0xffffffffffffffff)` 크래시.

**원인**: 같은 Phase에서 여러 Processor가 동일 엔티티에 `DestroyEntities`를 호출.

**해결**: 섹션 6.3의 DeadTag + 전용 소멸 Processor 패턴 적용.

### 11.4 서브시스템 null 크래시 (워커 스레드)

**증상**: `World->GetSubsystem<T>()` 호출 시 null 또는 스레드 안전하지 않은 접근.

**원인**: 워커 스레드에서 `GetWorld()`는 안전하지 않을 수 있습니다.

**해결**: `ProcessorRequirements.AddSubsystemRequirement<T>()` + `Context.GetSubsystemChecked<T>()` 패턴 사용.

### 11.5 Phase 배치 오류로 인한 한 프레임 지연

**증상**: 히트 판정(StartPhysics) → 사망 처리가 항상 한 프레임 늦게 반영됨.

**원인**: 사망 처리 Processor가 PrePhysics (기본값)에서 실행되어 HitDetection 이전 프레임의 체력을 읽음.

**해결**: 사망 처리 Processor에 `ProcessingPhase = EMassProcessingPhase::PostPhysics` 명시.

### 11.6 LOD/Representation Processor가 엉뚱한 타이밍에 실행

**증상**: LOD나 Actor 표현 전환이 프레임 내 잘못된 시점에 발생.

**원인**: `ExecuteInGroup = ProcessorGroupNames::LOD`로 그룹을 지정했지만 `ProcessingPhase = PostPhysics`를 누락하여 PrePhysics에서 실행됨.

**해결**: 그룹 이름 지정만으로는 Phase가 결정되지 않습니다. `ProcessingPhase`를 항상 명시합니다.

---

## 12. 설계 시 고려사항 (Design Checklist)

새 Processor를 설계할 때 체크해야 할 항목:

**Phase 및 순서**
- [ ] 이 Processor는 어떤 데이터를 읽고 어떤 데이터를 쓰는가?
- [ ] 읽는 데이터가 어느 Phase에서 생산되는가? → 그 이후 Phase에 배치.
- [ ] 같은 Phase 내 다른 Processor와의 순서 의존성이 있는가? → `ExecuteAfter/Before` 명시.
- [ ] 그룹 이름을 지정했다면 Phase도 명시했는가?

**스레드 안전성**
- [ ] UObject, Niagara, Actor를 직접 조작하는가? → `bRequiresGameThreadExecution = true`.
- [ ] 서브시스템에 접근하는가? → `AddSubsystemRequirement` + `Context.GetSubsystemChecked`.
- [ ] 서브시스템 접근의 읽기/쓰기 권한이 실제 사용과 일치하는가?

**엔티티 소멸**
- [ ] 여러 Processor에서 같은 엔티티가 소멸 조건을 동시에 만족할 수 있는가? → DeadTag 패턴 사용.
- [ ] `DestroyEntities`를 호출하는 Processor가 해당 Phase에서 유일한가?

**데이터 접근**
- [ ] `RegisterWithProcessor(*this)`가 `ConfigureQueries` 마지막에 호출되는가?
- [ ] 소멸된 엔티티를 대상으로 포함시키지 않도록 DeadTag 필터가 있는가?

**Shared Fragment 해시 무결성**
- [ ] `FMassConstSharedFragment` 서브 구조체의 모든 필드에 `UPROPERTY()`가 붙어 있는가? — 없으면 `GetOrCreateConstSharedFragment()`의 CRC 키에서 제외되어 값이 달라도 같은 인스턴스로 중복 제거됨.
- [ ] 엔티티를 분리해야 하는 값(Damage, Type, Radius 등)이 `UPROPERTY()` 없는 원시 멤버로 선언되어 있지 않은가?

---

## 13. 전체 프레임 흐름 요약

```
[PrePhysics Phase]
  SyncWorldToMass 그룹: 월드 상태 → Mass 읽기
  Behavior 그룹:        StateTree 실행, AI 타게팅 결정 (전 프레임 스코어 사용)
  Movement 그룹:        이동 의도 → 실제 위치 적용

  ↓ [커맨드 버퍼 플러시 #1]

[StartPhysics Phase]
  발사체 이동 (PrePhysics)
  히트 판정 + 직접 데미지 적용 → AddTag<DeadTag>
  시각화 (게임 스레드, ExecuteAfter HitDetection)

  ↓ [커맨드 버퍼 플러시 #2] ← DeadTag가 여기서 적용됨

[DuringPhysics Phase]  ← 물리 시뮬레이션과 병렬

  ↓ [커맨드 버퍼 플러시 #3]

[PostPhysics Phase]
  DeadTag 엔티티 소멸 (StartPhysics 이후 적용된 DeadTag 조회 가능)
  체력 0 체크 → 사망 처리 (StartPhysics에서 적용된 데미지 반영)
  AI 스코어 계산 (이동 완료 후, 다음 프레임 Targeting에서 사용)
  LOD 그룹: LOD 계산 및 오버라이드
  Representation 그룹: Actor 스폰/디스폰 동기화
  ActorInitializer: 스폰된 Actor 초기화

  ↓ [커맨드 버퍼 플러시 #4]

[FrameEnd Phase]
  최종 클린업
```
