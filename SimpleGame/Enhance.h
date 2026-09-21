#pragma once

#include "Player.h"

#include <array>

constexpr int32 MAX_ENHANCE_LEVEL = 15;

constexpr int32 ENHANCE_RESULT_NONE = 0;	// 못 눌렀다 ( 재료 부족 / 최대 단계 )
constexpr int32 ENHANCE_RESULT_SUCCESS = 1;
constexpr int32 ENHANCE_RESULT_KEEP = 2;
constexpr int32 ENHANCE_RESULT_DOWN = 3;
constexpr int32 ENHANCE_RESULT_RESET = 4;

struct EnhanceCost
{
	int32 gold = 0;
	std::array<int32, MATERIAL_COUNT> materials = {};
};

EnhanceCost GetEnhanceCost(int32 level);	// level 에서 level + 1 로
float GetEnhanceSuccessRate(int32 level);
bool CanDropOnFail(int32 level);
bool CanResetOnFail(int32 level);
bool CanPayEnhance(const PlayerState& player, const EnhanceCost& cost);
int32 GetEnhanceAttackBonus(int32 level);

// 재료는 시도할 때 태운다
int32 TryEnhance();
