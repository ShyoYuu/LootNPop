// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimSequence;

#include "LNPWeaponVisualSet.generated.h"

/**
 * 무기의 **표현 세트** — 손에 무엇이 들리고 어떻게 움직여 보이는가.
 *
 * `ULNPWeaponData`에서 떼어낸 이유는 **공유를 선언으로 만들기 위해서**다. 런처는 샷건의 메시·애님
 * 레이어·몽타주를 재활용하는데, 이전에는 그것이 두 DataAsset에 **같은 값을 세 번 적어 넣은 우연한
 * 일치**로만 표현됐다. 그중 `WeaponTag` 하나가 "무기의 정체성"이라는 이름을 달고 있어서,
 * 쿨다운처럼 무기를 갈라야 하는 규칙이 그 태그를 키로 삼으면 **런처와 샷건이 한 몸이 되는** 함정이
 * 있었다. 이제 공유는 `DA_Launcher.VisualSet = VS_Shotgun` 한 줄이고, 그 이상의 의미는 없다.
 *
 * ⚠️ **여기 있는 것은 전부 표현이다.** 무기의 정체성은 `ULNPWeaponData` 에셋 그 자체이고,
 * 규칙이 무기를 구분해야 할 때는 그 포인터를 키로 쓴다(쿨다운이 그 예 —
 * `TechDesign_Ability.md` §5.2). 표현 세트를 규칙의 키로 쓰면 공유하는 무기들이 함께 묶인다.
 *
 * 반대로 **조준 모드·발사 간격·발사체·스탯은 여기 오지 않는다** — 같은 메시를 든 두 무기가
 * 서로 다르게 싸울 수 있어야 한다.
 */
UCLASS(BlueprintType)
class LOOTNPOP_API ULNPWeaponVisualSet : public UDataAsset
{
	GENERATED_BODY()
public:
	/**
	 * 몽타주 Chooser(`CHT_Montage`)의 입력 키. 이 세트를 쓰는 무기들이 같은 공격 몽타주를 탄다.
	 *
	 * ⚠️ **무기 타입이 아니라 표현 세트 이름이다** (`LNP.VisualSet.*`). `VS_Shotgun`을 가리키는 샷건과 런처는
	 * 같은 값을 갖는 것이 **정상이다.** 무기를 갈라야 하는 규칙의 키로 쓰면 안 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet", meta = (Categories = "LNP.VisualSet"))
	FGameplayTag AnimSetTag;

	/** 장착 시 `LinkAnimClassLayers()`에 전달할 서브 AnimBP 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet")
	TSubclassOf<UAnimInstance> AnimLayerClass;

	/** 캐릭터 메시에 어태치할 무기 스켈레탈 메시. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet|Mesh")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	/** 무기 메시를 어태치할 캐릭터 소켓 이름. 비어 있으면 메시를 붙이지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet|Mesh")
	FName AttachSocketName = NAME_None;

	/** 장착 시 무기 메시의 상대 위치 오프셋. 피벗이 그립 위치에서 벗어난 경우 미세 보정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet|Mesh")
	FVector WeaponMeshRelativeLocation = FVector::ZeroVector;

	/** 장착 시 무기 메시의 상대 회전 오프셋. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet|Mesh")
	FRotator WeaponMeshRelativeRotation = FRotator(0.0f, 90.0f, -4.0f);

	/**
	 * 재장전 동안 무기 메시에 재생할 시퀀스 (`WeaponMesh`와 같은 스켈레톤). 비어 있으면 무기 메시는 움직이지 않는다.
	 * 재생은 GameplayCue.LNP.Weapon.Reload가 하고 배속은 재장전 시간에 맞춘다 (ULNPGameplayCueNotify_Reload).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet|Mesh")
	TObjectPtr<UAnimSequence> WeaponReloadAnim;

	/**
	 * 발사 순간 무기 메시에 재생할 시퀀스 (슬라이드·볼트 등 파츠 모션). 비어 있으면 재생하지 않는다.
	 * 캐릭터 발사 몽타주와 같은 1배속이다 — Lyra 원본은 두 시퀀스 길이가 같다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VisualSet|Mesh")
	TObjectPtr<UAnimSequence> WeaponFireAnim;
};
