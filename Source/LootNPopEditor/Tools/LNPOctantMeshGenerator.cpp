#include "Tools/LNPOctantMeshGenerator.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UObjectIterator.h"

TSharedRef<IDetailCustomization> FLNPOctantMeshGeneratorCustomization::MakeInstance()
{
	return MakeShareable(new FLNPOctantMeshGeneratorCustomization);
}

void FLNPOctantMeshGeneratorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Save Asset");

	Category.AddCustomRow(FText::FromString("Save Mesh"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(FText::FromString("Save Mesh"))
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([SelectedObjects]() -> FReply
		{
			for (const TWeakObjectPtr<UObject>& WeakObj : SelectedObjects)
			{
				UObject* Object = WeakObj.Get();
				if (!Object)
					continue;

				ALNPOctantMeshGeneratorBase* Target = nullptr;

				if (Object->HasAnyFlags(RF_ClassDefaultObject))
				{
					UClass* ActorClass = Object->GetClass();
					for (TObjectIterator<ALNPOctantMeshGeneratorBase> It; It; ++It)
					{
						if (IsValid(*It) && !It->HasAnyFlags(RF_ClassDefaultObject) && It->GetClass() == ActorClass)
						{
							Target = *It;
							break;
						}
					}
				}
				else
				{
					Target = Cast<ALNPOctantMeshGeneratorBase>(Object);
				}

				if (Target)
				{
					FEditorScriptExecutionGuard Guard;
					Target->ReceiveSaveMesh();
				}
			}
			return FReply::Handled();
		})
	];
}
