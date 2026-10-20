#pragma once

// 무기 강화. 주문의 흔적과 골드를 태워 낀 무기 단계를 올린다 ( 설계 2.10 의 세 구간 )
constexpr int32 ENHANCE_RESULT_NONE = 0;
constexpr int32 ENHANCE_RESULT_SUCCESS = 1;
constexpr int32 ENHANCE_RESULT_KEEP = 2;	// 실패, 단계 유지
constexpr int32 ENHANCE_RESULT_DOWN = 3;	// 실패, 한 단계 하락
constexpr int32 ENHANCE_RESULT_RESET = 4;	// 실패, 0 단계로
constexpr int32 ENHANCE_RESULT_NO_WEAPON = 5;
constexpr int32 ENHANCE_RESULT_NO_MATERIAL = 6;
constexpr int32 ENHANCE_RESULT_MAX_LEVEL = 7;

struct EnhanceCost
{
	int32 trace = 0;	// 주문의 흔적
	int32 gold = 0;
};

int32 GetMaxEnhanceLevel();
EnhanceCost GetEnhanceCost(int32 level);	// level 에서 level + 1 로
int32 GetEnhanceSuccessRate(int32 level);	// %
bool CanDropOnFail(int32 level);
bool CanResetOnFail(int32 level);
int32 GetEnhanceAttackBonus(int32 level);

// 재료는 시도할 때 태운다. 성공할 때만 태우면 실패에 대가가 없어 그냥 시간 지연이다
int32 TryEnhanceWeapon();
