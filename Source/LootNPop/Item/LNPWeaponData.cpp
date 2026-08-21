// Copyright (c) 2026 LootNPop. All rights reserved.

#include "Item/LNPWeaponData.h"
#include "LootNPop.h"

const FLNPWeaponLevelRow* ULNPWeaponData::FindLevelRow(int32 Level) const
{
	if (LevelTable == nullptr || Level < 1)
		return nullptr;

	// 행 이름이 곧 레벨이다. 없는 행 조회는 GetMaxLevel의 탐색 수단이기도 하므로 경고를 끈다.
	const FName RowName(*FString::FromInt(Level));
	return LevelTable->FindRow<FLNPWeaponLevelRow>(
		RowName, TEXT("ULNPWeaponData::FindLevelRow"), /*bWarnIfRowMissing=*/false);
}

int32 ULNPWeaponData::GetMaxLevel() const
{
	if (LevelTable == nullptr)
		return 1;

	// "1"부터 끊기는 지점까지가 유효 구간이다 — 중간에 구멍이 나면 그 앞까지만 인정한다.
	// 행이 10개 남짓이라 매번 스캔해도 무시할 비용이고, 캐시하면 에디터에서 행을 고쳤을 때
	// 낡은 값이 남아 튜닝 반복을 방해한다.
	int32 Max = 0;
	while (FindLevelRow(Max + 1) != nullptr)
		++Max;

	return FMath::Max(1, Max);
}

TConstArrayView<FLNPStatModifier> ULNPWeaponData::GetStatModifiersForLevel(int32 Level) const
{
	if (LevelTable == nullptr)
		return StatModifiers;

	if (!bValidatedLevelTable)
	{
		bValidatedLevelTable = true;
		ValidateLevelTable();
	}

	// 범위를 벗어난 레벨은 잘라서 가장 가까운 행을 쓴다 — 빈 목록을 돌려주면 무기가 스텟을
	// 통째로 잃는다(테이블 행을 줄인 뒤 옛 레벨 인스턴스가 남은 경우).
	const FLNPWeaponLevelRow* Row = FindLevelRow(FMath::Clamp(Level, 1, GetMaxLevel()));
	return Row ? MakeArrayView(Row->StatModifiers) : TConstArrayView<FLNPStatModifier>();
}

float ULNPWeaponData::GetAbilityCoefScale(int32 Level) const
{
	const FLNPWeaponLevelRow* Row = FindLevelRow(FMath::Clamp(Level, 1, GetMaxLevel()));
	return Row ? Row->AbilityCoefScale : 1.0f;
}

void ULNPWeaponData::ValidateLevelTable() const
{
	if (!StatModifiers.IsEmpty())
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("[Weapon] %s has both LevelTable and base StatModifiers — the base list is ignored. Move those entries into the table rows."),
			*GetName());
	}

	if (FindLevelRow(1) == nullptr)
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("[Weapon] %s LevelTable '%s' has no row named \"1\" — rows must be named by level number (\"1\", \"2\", ...)."),
			*GetName(), *GetNameSafe(LevelTable));
		return;
	}

	// 연속 구간보다 행이 많으면 중간에 구멍이 있다는 뜻이고, 그 뒤 행은 영원히 도달할 수 없다.
	const int32 ContiguousMax = GetMaxLevel();
	const int32 RowCount = LevelTable->GetRowMap().Num();
	if (RowCount != ContiguousMax)
	{
		UE_LOG(LogLootNPop, Warning,
			TEXT("[Weapon] %s LevelTable '%s' has %d rows but only levels 1..%d are contiguous — rows past the gap are unreachable."),
			*GetName(), *GetNameSafe(LevelTable), RowCount, ContiguousMax);
	}
}
