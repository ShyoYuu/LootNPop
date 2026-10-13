// Copyright LootNPop. All Rights Reserved.

#include "Movement/LNPLayeredMove_LinearVelocity.h"

FLayeredMoveBase* FLNPLayeredMove_LinearVelocity::Clone() const
{
	// 부모 Clone을 쓰면 롤백 복사(CopyLayeredMoveArray)에서 부모 타입으로 잘려 이 우회가 사라진다.
	return new FLNPLayeredMove_LinearVelocity(*this);
}

void FLNPLayeredMove_LinearVelocity::NetSerialize(FArchive& Ar)
{
	// 부모 경로가 FinishVelocitySettings 자리에 "설정 없음" 비트 하나만 쓰고 읽는다. 송수신이 대칭이라 그대로 두고 뒤에 덧붙인다.
	Super::NetSerialize(Ar);

	uint8 ModeAsU8 = static_cast<uint8>(FinishVelocitySettings.FinishVelocityMode);
	Ar << ModeAsU8;
	FinishVelocitySettings.FinishVelocityMode = static_cast<ELayeredMoveFinishVelocityMode>(ModeAsU8);

	if (FinishVelocitySettings.FinishVelocityMode == ELayeredMoveFinishVelocityMode::SetVelocity)
	{
		Ar << FinishVelocitySettings.SetVelocity;
	}
	else if (FinishVelocitySettings.FinishVelocityMode == ELayeredMoveFinishVelocityMode::ClampVelocity)
	{
		Ar << FinishVelocitySettings.ClampVelocity;
	}
}

UScriptStruct* FLNPLayeredMove_LinearVelocity::GetScriptStruct() const
{
	return FLNPLayeredMove_LinearVelocity::StaticStruct();
}

FString FLNPLayeredMove_LinearVelocity::ToSimpleString() const
{
	return TEXT("LNP LinearVelocity");
}
