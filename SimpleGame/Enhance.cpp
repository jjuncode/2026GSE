#include "stdafx.h"
#include "Enhance.h"
#include "GameData.h"
#include "Player.h"

#include <chrono>
#include <iostream>
#include <random>

namespace
{
	// 지형 씨앗과 갈라 둔다. 같은 자리에서 다시 눌러도 결과가 달라야 한다 ( 설계 2.9 )
	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x2545f491u);
		return random;
	}

	int32 RollPercent()
	{
		std::uniform_int_distribution<int32> distribution(1, 100);
		return distribution(GetRandom());
	}

	int32 ReadTable(const std::vector<int32>& table, int32 index, int32 fallback)
	{
		if (table.empty())
		{
			return fallback;
		}

		if (index < 0)
		{
			return table.front();
		}

		if (index >= static_cast<int32>(table.size()))
		{
			return table.back();
		}

		return table[static_cast<size_t>(index)];
	}
}

int32 GetMaxEnhanceLevel()
{
	return GetConfig().enhance_max_level;
}

EnhanceCost GetEnhanceCost(int32 level)
{
	const GameConfig& config = GetConfig();

	EnhanceCost cost;
	cost.trace = ReadTable(config.enhance_trace_cost, level, 1);
	cost.gold = ReadTable(config.enhance_gold_cost, level, 0);

	return cost;
}

int32 GetEnhanceSuccessRate(int32 level)
{
	return ReadTable(GetConfig().enhance_success_rate, level, 50);
}

bool CanDropOnFail(int32 level)
{
	return level >= GetConfig().enhance_fail_down_from;
}

bool CanResetOnFail(int32 level)
{
	return level >= GetConfig().enhance_reset_from;
}

int32 GetEnhanceAttackBonus(int32 level)
{
	if (level <= 0)
	{
		return 0;
	}

	return ReadTable(GetConfig().enhance_attack_bonus, level, 0);
}

int32 TryEnhanceWeapon()
{
	Player& player = MutablePlayer();
	ItemSlot* weapon = player.m_inventory.MutableEquip(EQUIP_AREA_WEAPON);

	if (weapon == nullptr || player.m_inventory.IsEmptySlot(*weapon))
	{
		return ENHANCE_RESULT_NO_WEAPON;
	}

	const int32 level = weapon->enhance;
	if (level >= GetMaxEnhanceLevel())
	{
		return ENHANCE_RESULT_MAX_LEVEL;
	}

	const EnhanceCost cost = GetEnhanceCost(level);
	if (player.m_gold < cost.gold || player.m_inventory.CountItem(ITEM_ID_SPELL_TRACE) < cost.trace)
	{
		return ENHANCE_RESULT_NO_MATERIAL;
	}

	player.m_gold -= cost.gold;
	player.m_inventory.RemoveItem(ITEM_ID_SPELL_TRACE, cost.trace);

	int32 result = ENHANCE_RESULT_KEEP;

	if (RollPercent() <= GetEnhanceSuccessRate(level))
	{
		weapon->enhance = level + 1;
		result = ENHANCE_RESULT_SUCCESS;
	}
	else if (CanResetOnFail(level) && RollPercent() <= GetConfig().enhance_reset_share)
	{
		// 무기는 남기고 쌓은 단계만 날린다. 무기가 사라지면 게임이 멈춘다
		weapon->enhance = 0;
		result = ENHANCE_RESULT_RESET;
	}
	else if (CanDropOnFail(level))
	{
		weapon->enhance = level - 1;
		result = ENHANCE_RESULT_DOWN;
	}

	player.RecalcStat();

	std::cout << "[ Enhance - TryEnhanceWeapon ] : +" << level << " -> +" << weapon->enhance << " result " << result << "\n";

	return result;
}
