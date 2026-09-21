#include "stdafx.h"
#include "Enhance.h"

#include <chrono>
#include <iostream>
#include <random>

namespace
{
	// 10 과 15 에서 크게 뛰어야 "일단 10 만 찍자" 가 생긴다
	const std::array<int32, MAX_ENHANCE_LEVEL + 1> ATTACK_BONUS = { 0, 2, 4, 6, 8, 10, 13, 16, 19, 22, 34, 38, 42, 46, 50, 72 };

	constexpr float RESET_SHARE_OF_FAIL = 0.25f;	// 도박 구간 실패 중 0단계로 가는 몫

	// 지형 씨앗과 갈라 둔다. 같이 쓰면 같은 자리 같은 결과가 반복된다
	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()));
		return random;
	}

	float RollUnit()
	{
		std::uniform_real_distribution<float> distribution(0.f, 1.f);
		return distribution(GetRandom());
	}
}

EnhanceCost GetEnhanceCost(int32 level)
{
	EnhanceCost cost;
	cost.gold = 15 + level * 10;

	if (level < 5)
	{
		cost.materials[static_cast<size_t>(MATERIAL_DUST)] = 1 + level / 2;
	}
	else if (level < 10)
	{
		cost.materials[static_cast<size_t>(MATERIAL_DUST)] = 2;
		cost.materials[static_cast<size_t>(MATERIAL_FRAGMENT)] = 1;
	}
	else
	{
		cost.materials[static_cast<size_t>(MATERIAL_FRAGMENT)] = 2;
		cost.materials[static_cast<size_t>(MATERIAL_CORE)] = 1;
	}

	return cost;
}

float GetEnhanceSuccessRate(int32 level)
{
	if (level < 5)
	{
		return 0.9f;
	}

	if (level < 10)
	{
		return 0.5f;
	}

	return 0.3f;
}

bool CanDropOnFail(int32 level)
{
	return level >= 5;
}

bool CanResetOnFail(int32 level)
{
	return level >= 10;
}

bool CanPayEnhance(const PlayerState& player, const EnhanceCost& cost)
{
	if (player.gold < cost.gold)
	{
		return false;
	}

	for (size_t index = 0; index < cost.materials.size(); ++index)
	{
		if (player.materials[index] < cost.materials[index])
		{
			return false;
		}
	}

	return true;
}

int32 GetEnhanceAttackBonus(int32 level)
{
	if (level < 0)
	{
		return 0;
	}

	if (level > MAX_ENHANCE_LEVEL)
	{
		return ATTACK_BONUS[static_cast<size_t>(MAX_ENHANCE_LEVEL)];
	}

	return ATTACK_BONUS[static_cast<size_t>(level)];
}

int32 TryEnhance()
{
	PlayerState& player = MutablePlayer();

	if (player.enhance_level >= MAX_ENHANCE_LEVEL)
	{
		return ENHANCE_RESULT_NONE;
	}

	const EnhanceCost cost = GetEnhanceCost(player.enhance_level);
	if (CanPayEnhance(player, cost) == false)
	{
		return ENHANCE_RESULT_NONE;
	}

	player.gold -= cost.gold;
	for (size_t index = 0; index < cost.materials.size(); ++index)
	{
		player.materials[index] -= cost.materials[index];
	}

	const int32 before = player.enhance_level;

	if (RollUnit() < GetEnhanceSuccessRate(before))
	{
		player.enhance_level = before + 1;
		std::cout << "[ Enhance - TryEnhance ] : " << before << " -> " << player.enhance_level << " success\n";
		return ENHANCE_RESULT_SUCCESS;
	}

	// 무기는 안 사라지고 쌓은 단계만 날린다
	if (CanResetOnFail(before) && RollUnit() < RESET_SHARE_OF_FAIL)
	{
		player.enhance_level = 0;
		std::cout << "[ Enhance - TryEnhance ] : " << before << " -> 0 reset\n";
		return ENHANCE_RESULT_RESET;
	}

	if (CanDropOnFail(before))
	{
		player.enhance_level = before - 1;
		std::cout << "[ Enhance - TryEnhance ] : " << before << " -> " << player.enhance_level << " down\n";
		return ENHANCE_RESULT_DOWN;
	}

	std::cout << "[ Enhance - TryEnhance ] : " << before << " keep\n";
	return ENHANCE_RESULT_KEEP;
}
