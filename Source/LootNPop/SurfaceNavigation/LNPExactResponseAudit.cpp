// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPCollisionChannels.h"
#include "LootNPop.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

#if !UE_BUILD_SHIPPING

namespace
{
	/**
	 * Gate -1(D-036) 판정 분류. 선언 순서가 출력 순서다.
	 * Pawn을 막는 충돌은 지금 캐릭터가 부딪히는 월드이므로 LNPWorldExact도 막아야 한다.
	 */
	enum class EExactAuditStatus : uint8
	{
		/** Pawn은 막지만 LNPWorldExact는 무시 — exact 경로 전환 시 관통한다. 마이그레이션 대상. */
		Missing,
		/** LNPWorldExact만 막음 — 의도(투사체 전용 blocker 등)인지 검토 대상. */
		ExactOnly,
		/** Pawn과 LNPWorldExact를 모두 막음. */
		Ok,
		/** 둘 다 막지 않음 — trigger·파편 등. exact world에서 제외. */
		NonBlocking,
	};

	const TCHAR* ToString(EExactAuditStatus Status)
	{
		switch (Status)
		{
		case EExactAuditStatus::Ok:          return TEXT("Ok");
		case EExactAuditStatus::Missing:     return TEXT("MISSING");
		case EExactAuditStatus::ExactOnly:   return TEXT("ExactOnly");
		case EExactAuditStatus::NonBlocking: return TEXT("NonBlocking");
		}
		return TEXT("?");
	}

	const TCHAR* ToString(ECollisionResponse Response)
	{
		switch (Response)
		{
		case ECR_Ignore:  return TEXT("Ignore");
		case ECR_Overlap: return TEXT("Overlap");
		case ECR_Block:   return TEXT("Block");
		default:          return TEXT("?");
		}
	}

	/** 같은 판정을 받는 component를 한 행으로 묶는 키. */
	struct FExactAuditRow
	{
		EExactAuditStatus Status = EExactAuditStatus::Ok;
		FString Source;
		FString OwnerClass;
		FString ComponentClass;
		FName Profile;
		FString ObjectType;
		ECollisionResponse PawnResponse = ECR_Ignore;
		ECollisionResponse ExactResponse = ECR_Ignore;
		FString Mesh;

		int32 ComponentCount = 0;
		int32 InstanceCount = 0;

		FString MakeKey() const
		{
			return FString::Printf(TEXT("%d|%s|%s|%s|%s|%s|%d|%d|%s"),
				static_cast<int32>(Status), *Source, *OwnerClass, *ComponentClass, *Profile.ToString(),
				*ObjectType, static_cast<int32>(PawnResponse), static_cast<int32>(ExactResponse), *Mesh);
		}
	};

	/** Level Instance로 로드된 레벨은 패키지 이름 끝에 원본 LVI 이름과 인스턴스 접미사가 붙는다. */
	FString DescribeSource(const UPrimitiveComponent& Component, const UWorld& World)
	{
		const ULevel* Level = Component.GetComponentLevel();
		if (Level == nullptr || Level == World.PersistentLevel)
		{
			return TEXT("Persistent");
		}
		return FPackageName::GetShortName(Level->GetOutermost()->GetName());
	}

	/** ISM·HISM은 component BodyInstance가 아니라 instance별 body를 쓰므로 따로 판정한다. */
	bool HasQueryShape(const UPrimitiveComponent& Component)
	{
		if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(&Component))
		{
			const UStaticMesh* Mesh = ISM->GetStaticMesh();
			return ISM->GetInstanceCount() > 0 && Mesh != nullptr && Mesh->GetBodySetup() != nullptr;
		}
		const FBodyInstance* BodyInstance = Component.GetBodyInstance();
		return BodyInstance != nullptr && BodyInstance->IsValidBodyInstance();
	}

	EExactAuditStatus Classify(ECollisionResponse PawnResponse, ECollisionResponse ExactResponse)
	{
		const bool bBlocksPawn = PawnResponse == ECR_Block;
		const bool bBlocksExact = ExactResponse == ECR_Block;
		if (bBlocksPawn && bBlocksExact)
		{
			return EExactAuditStatus::Ok;
		}
		if (bBlocksPawn)
		{
			return EExactAuditStatus::Missing;
		}
		return bBlocksExact ? EExactAuditStatus::ExactOnly : EExactAuditStatus::NonBlocking;
	}

	/**
	 * LNP.SurfaceNav.AuditExactResponse [all]
	 * 현재 월드의 query 충돌 component를 Pawn 응답과 LNPWorldExact 응답으로 분류해 로그에 표로 남긴다.
	 * 기본은 NonBlocking 행을 생략하고, all을 주면 모두 출력한다.
	 */
	FAutoConsoleCommandWithWorldAndArgs GLNPAuditExactResponse(
		TEXT("LNP.SurfaceNav.AuditExactResponse"),
		TEXT("Audit query-collision components in the current world: Pawn response vs LNPWorldExact response. ")
		TEXT("Components that block Pawn but ignore LNPWorldExact are reported as MISSING. Args: [all] to include NonBlocking rows."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				return;
			}
			const bool bIncludeNonBlocking = Args.Contains(TEXT("all"));

			TMap<FString, FExactAuditRow> Rows;
			int32 StatusComponentCounts[4] = {};
			int32 NoBodyCount = 0;

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const AActor* Actor = *It;
				// 캐릭터 캡슐·메시는 world가 아니라 판정 대상이다. LNPWorldExact 기본 응답 Ignore가 의도다.
				if (Actor->IsA<APawn>())
				{
					continue;
				}

				TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
				for (const UPrimitiveComponent* Component : Components)
				{
					if (Component == nullptr || !Component->IsQueryCollisionEnabled())
					{
						continue;
					}
					// 디버그 렌더링 component·빈 builder brush는 BlockAll이어도 query할 shape가 없다.
					if (!HasQueryShape(*Component))
					{
						++NoBodyCount;
						continue;
					}

					FExactAuditRow Row;
					Row.PawnResponse = Component->GetCollisionResponseToChannel(ECC_Pawn);
					Row.ExactResponse = Component->GetCollisionResponseToChannel(LNPCollisionChannels::WorldExact);
					Row.Status = Classify(Row.PawnResponse, Row.ExactResponse);
					++StatusComponentCounts[static_cast<int32>(Row.Status)];
					if (Row.Status == EExactAuditStatus::NonBlocking && !bIncludeNonBlocking)
					{
						continue;
					}

					Row.Source = DescribeSource(*Component, *World);
					Row.OwnerClass = Actor->GetClass()->GetName();
					Row.ComponentClass = Component->GetClass()->GetName();
					Row.Profile = Component->GetCollisionProfileName();
					Row.ObjectType = StaticEnum<ECollisionChannel>()->GetNameStringByValue(Component->GetCollisionObjectType());
					if (const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
					{
						Row.Mesh = GetNameSafe(MeshComponent->GetStaticMesh());
					}

					FExactAuditRow& Entry = Rows.FindOrAdd(Row.MakeKey(), Row);
					++Entry.ComponentCount;
					if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Component))
					{
						Entry.InstanceCount += ISM->GetInstanceCount();
					}
				}
			}

			TArray<FExactAuditRow> Sorted;
			Rows.GenerateValueArray(Sorted);
			Sorted.Sort([](const FExactAuditRow& A, const FExactAuditRow& B)
			{
				// 조치가 필요한 MISSING·ExactOnly가 먼저 오도록 enum 순서로 정렬한다.
				if (A.Status != B.Status)
				{
					return A.Status < B.Status;
				}
				return A.Source != B.Source ? A.Source < B.Source : A.OwnerClass < B.OwnerClass;
			});

			UE_LOG(LogLootNPop, Display, TEXT("[ExactAudit] World=%s NetMode=%d Rows=%d"),
				*World->GetName(), static_cast<int32>(World->GetNetMode()), Sorted.Num());
			UE_LOG(LogLootNPop, Display,
				TEXT("[ExactAudit] Status | Source | Owner | Component | Profile | ObjectType | Pawn | Exact | Mesh | Components | Instances"));
			for (const FExactAuditRow& Row : Sorted)
			{
				UE_LOG(LogLootNPop, Display, TEXT("[ExactAudit] %s | %s | %s | %s | %s | %s | %s | %s | %s | %d | %d"),
					ToString(Row.Status), *Row.Source, *Row.OwnerClass, *Row.ComponentClass, *Row.Profile.ToString(),
					*Row.ObjectType, ToString(Row.PawnResponse), ToString(Row.ExactResponse), *Row.Mesh,
					Row.ComponentCount, Row.InstanceCount);
			}
			UE_LOG(LogLootNPop, Display, TEXT("[ExactAudit] Summary components: Ok=%d MISSING=%d ExactOnly=%d NonBlocking=%d NoBody=%d"),
				StatusComponentCounts[static_cast<int32>(EExactAuditStatus::Ok)],
				StatusComponentCounts[static_cast<int32>(EExactAuditStatus::Missing)],
				StatusComponentCounts[static_cast<int32>(EExactAuditStatus::ExactOnly)],
				StatusComponentCounts[static_cast<int32>(EExactAuditStatus::NonBlocking)],
				NoBodyCount);
		}));
}

#endif // !UE_BUILD_SHIPPING
