// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPOctantTriangleExtractor.h"

#include "Chaos/TriangleMeshImplicitObject.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
const FName SupportTag(TEXT("LNP.Surface.Support"));
const FName CoarseSupportTag(TEXT("LNP.Surface.CoarseSupport"));

template <typename IndexType>
void AppendTriangles(
	const TArray<Chaos::TVec3<IndexType>>& Elements,
	bool bFlipWinding,
	TArray<FIntVector3>& OutTriangles)
{
	for (const Chaos::TVec3<IndexType>& Element : Elements)
	{
		const int32 A = static_cast<int32>(Element[0]);
		const int32 B = static_cast<int32>(Element[1]);
		const int32 C = static_cast<int32>(Element[2]);
		OutTriangles.Emplace(A, bFlipWinding ? C : B, bFlipWinding ? B : C);
	}
}
}

FTransform FLNPOctantTriangleExtractor::GetSourceTransform(const USceneComponent& Component)
{
	if (Component.IsRegistered())
	{
		return Component.GetComponentTransform();
	}

	// 로드만 한 source World는 component를 등록하지 않아 ComponentToWorld가 갱신되지 않을 수 있다.
	FTransform Transform = Component.GetRelativeTransform();
	for (const USceneComponent* Parent = Component.GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
	{
		Transform = Transform * Parent->GetRelativeTransform();
	}
	return Transform;
}

bool FLNPOctantTriangleExtractor::ExtractComponent(
	const UStaticMeshComponent& Component,
	FLNPBakeTriangleMesh& OutMesh,
	FString& OutError)
{
	OutMesh = FLNPBakeTriangleMesh();
	const UStaticMesh* StaticMesh = Component.GetStaticMesh();
	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	if (!BodySetup)
	{
		OutError = FString::Printf(TEXT("Component '%s' has no Static Mesh body setup."), *Component.GetPathName());
		return false;
	}
	if (BodySetup->GetCollisionTraceFlag() != CTF_UseComplexAsSimple)
	{
		OutError = FString::Printf(
			TEXT("Support component '%s' mesh '%s' must use complex collision as simple; exact queries do not trace complex collision."),
			*Component.GetPathName(), *StaticMesh->GetPathName());
		return false;
	}

	// 물리 state를 만든 적 없는 에디터 로드 mesh는 trimesh가 아직 없다. 쿠킹은 DDC를 거치며 결정론적이다.
	if (BodySetup->TriMeshGeometries.IsEmpty())
	{
		BodySetup->CreatePhysicsMeshes();
	}
	if (BodySetup->TriMeshGeometries.IsEmpty())
	{
		OutError = FString::Printf(
			TEXT("Support component '%s' mesh '%s' has no cooked Chaos triangle mesh."),
			*Component.GetPathName(), *StaticMesh->GetPathName());
		return false;
	}

	// exact hit의 FaceIndex는 trimesh 하나 안의 원본 삼각형 번호라서, trimesh가 여럿이면 어느 것의 번호인지 알 수 없다.
	const Chaos::FTriangleMeshImplicitObject* TriMesh = nullptr;
	for (const Chaos::FTriangleMeshImplicitObjectPtr& Candidate : BodySetup->TriMeshGeometries)
	{
		if (!Candidate.IsValid())
		{
			continue;
		}
		if (TriMesh)
		{
			OutError = FString::Printf(
				TEXT("Support component '%s' mesh '%s' has more than one Chaos triangle mesh; exact hit face indices would be ambiguous."),
				*Component.GetPathName(), *StaticMesh->GetPathName());
			return false;
		}
		TriMesh = Candidate.GetReference();
	}

	const FTransform Transform = GetSourceTransform(Component);
	const bool bFlipWinding = Transform.GetDeterminant() < 0.0;
	if (TriMesh)
	{
		const Chaos::FTriangleMeshImplicitObject::ParticlesType& Particles = TriMesh->Particles();
		const int32 ParticleCount = static_cast<int32>(Particles.Size());
		OutMesh.Vertices.Reserve(ParticleCount);
		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			const Chaos::FVec3f& Local = Particles.GetX(Index);
			OutMesh.Vertices.Add(Transform.TransformPosition(FVector3d(Local.X, Local.Y, Local.Z)));
		}

		const Chaos::FTrimeshIndexBuffer& Elements = TriMesh->Elements();
		if (Elements.RequiresLargeIndices())
		{
			AppendTriangles(Elements.GetLargeIndexBuffer(), bFlipWinding, OutMesh.Triangles);
		}
		else
		{
			AppendTriangles(Elements.GetSmallIndexBuffer(), bFlipWinding, OutMesh.Triangles);
		}

		// 엔진은 hit의 내부 face 번호를 이 표로 바꿔 FaceIndex로 돌려준다(CollisionConversions.cpp).
		OutMesh.ExternalFaceIndices.SetNumUninitialized(OutMesh.Triangles.Num());
		for (int32 Internal = 0; Internal < OutMesh.Triangles.Num(); ++Internal)
		{
			const int32 External = TriMesh->GetExternalFaceIndexFromInternal(Internal);
			if (External < 0)
			{
				OutError = FString::Printf(
					TEXT("Support component '%s' mesh '%s' has no external face index for triangle %d; exact hits cannot be mapped to Layers."),
					*Component.GetPathName(), *StaticMesh->GetPathName(), Internal);
				return false;
			}
			OutMesh.ExternalFaceIndices[Internal] = External;
		}
	}

	if (OutMesh.Triangles.IsEmpty())
	{
		OutError = FString::Printf(
			TEXT("Support component '%s' mesh '%s' has an empty Chaos triangle mesh."),
			*Component.GetPathName(), *StaticMesh->GetPathName());
		return false;
	}
	return true;
}

bool FLNPOctantTriangleExtractor::ExtractSupportSources(
	const UWorld& SourceWorld,
	TArray<FLNPBakeSupportSource>& OutSources,
	FString& OutError)
{
	OutSources.Reset();
	OutError.Reset();
	if (!SourceWorld.PersistentLevel)
	{
		OutError = TEXT("Source World has no persistent Level.");
		return false;
	}

	for (const AActor* Actor : SourceWorld.PersistentLevel->Actors)
	{
		if (!IsValid(Actor) || Actor->HasAnyFlags(RF_Transient))
		{
			continue;
		}

		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (!IsValid(Component) || !Component->ComponentTags.Contains(SupportTag))
			{
				continue;
			}
			// instance별 transform과 Layer 대응(D-037)은 4b 다층 Atlas에서 다룬다.
			if (Component->IsA<UInstancedStaticMeshComponent>())
			{
				OutError = FString::Printf(
					TEXT("Instanced Support component '%s' is not supported yet."), *Component->GetPathName());
				return false;
			}

			FLNPBakeSupportSource& Source = OutSources.AddDefaulted_GetRef();
			Source.Name = Component->GetPathName();
			Source.Key = FString::Printf(TEXT("%s.%s"), *Actor->GetFName().ToString(), *Component->GetFName().ToString());
			Source.bCoarseSupport = Component->ComponentTags.Contains(CoarseSupportTag);
			if (!ExtractComponent(*Component, Source.Mesh, OutError))
			{
				OutSources.Reset();
				return false;
			}
		}
	}

	OutSources.Sort([](const FLNPBakeSupportSource& A, const FLNPBakeSupportSource& B)
	{
		return A.Name.Compare(B.Name, ESearchCase::CaseSensitive) < 0;
	});
	for (int32 Index = 1; Index < OutSources.Num(); ++Index)
	{
		for (int32 Other = 0; Other < Index; ++Other)
		{
			if (OutSources[Index].Key.Equals(OutSources[Other].Key, ESearchCase::CaseSensitive))
			{
				OutError = FString::Printf(TEXT("Support components '%s' and '%s' share the source key '%s'."),
					*OutSources[Other].Name, *OutSources[Index].Name, *OutSources[Index].Key);
				OutSources.Reset();
				return false;
			}
		}
	}
	return true;
}
