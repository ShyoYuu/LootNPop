// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "Core/CameraNode.h"
#include "LNPMovementBaseDampingCameraNode.generated.h"

class UDampenPositionCameraNode;

/**
 * 이동패널의 운반 좌표계에서 기존 위치 감쇠를 실행한다.
 * 패널 이동·회전은 즉시 따르고, 패널에 대한 캐릭터의 이동만 감쇠한다.
 * CR_ThirdPerson의 기존 DampenPosition을 자식으로 연결해 감쇠 파라미터를 보존한다.
 * 자식의 DampenSpace는 CameraPose여야 한다.
 */
UCLASS(meta=(CameraNodeCategories="LNP"))
class LOOTNPOP_API ULNPMovementBaseDampingCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, Category="Damping")
	TObjectPtr<UDampenPositionCameraNode> DampingNode;

protected:
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};
