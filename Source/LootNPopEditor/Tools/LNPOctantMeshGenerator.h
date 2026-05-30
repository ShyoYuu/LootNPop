#pragma once

#include "CoreMinimal.h"
#include "GeometryActors/GeneratedDynamicMeshActor.h"
#include "IDetailCustomization.h"
#include "LNPOctantMeshGenerator.generated.h"

UCLASS(Abstract)
class LOOTNPOPEDITOR_API ALNPOctantMeshGeneratorBase : public AGeneratedDynamicMeshActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Asset")
	FString AssetPathAndName;

	UFUNCTION(BlueprintImplementableEvent, Category = "Save Asset")
	void ReceiveSaveMesh();
};

class FLNPOctantMeshGeneratorCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
