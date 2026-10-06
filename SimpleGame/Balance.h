#pragma once

// 레벨1 프로토타입 수치. 돌려 보고 고친다
namespace Balance
{
	constexpr float BODY_RADIUS = 0.3f;	// 몸 반경 ( 칸 )

	constexpr float PLAYER_MOVE_SPEED = 4.5f;	// 초당 칸
	constexpr int32 PLAYER_BASE_HP = 100;
	constexpr int32 PLAYER_HP_PER_POINT = 10;
	constexpr int32 PLAYER_BASE_ATTACK = 10;
	constexpr int32 PLAYER_ATTACK_PER_POINT = 2;
	constexpr int32 PLAYER_DEFENCE_PER_POINT = 1;
	constexpr int32 LEVEL_UP_STAT_POINT = 4;
	constexpr float PLAYER_RESPAWN_SEC = 2.f;
	constexpr float PLAYER_REGEN_DELAY_SEC = 3.f;	// 마지막 피격 뒤 이만큼 지나야 빨리 찬다
	constexpr float PLAYER_REGEN_PER_SEC = 4.f;
	constexpr float PLAYER_COMBAT_REGEN_PER_SEC = 1.f;

	constexpr float SWING_INTERVAL_SEC = 0.45f;
	constexpr float SWING_SHOW_SEC = 0.18f;
	constexpr float SWING_RANGE = 1.7f;	// 칸
	constexpr float SWING_HALF_COS = 0.5f;	// 좌우 60도
	constexpr float SWING_KNOCKBACK = 0.25f;

	constexpr float HIT_FLASH_SEC = 0.12f;
	constexpr float MONSTER_RESPAWN_SEC = 25.f;
	constexpr float MONSTER_RESPAWN_MIN_DISTANCE = 12.f;	// 눈앞에서는 안 살아난다
	constexpr float MONSTER_LEASH = 14.f;	// 스폰 자리에서 이만큼 끌려가면 돌아간다

	constexpr int32 BOSS_UNLOCK_LEVEL = 5;	// 이 레벨이 돼야 성문파쇄자가 깨어난다

	constexpr float WEAK_POINT_MULTIPLIER = 2.5f;	// 거식귀 등 뒤에서 벤 배수
	constexpr float ELITE_HP_RATE = 2.2f;
	constexpr float ELITE_ATTACK_RATE = 1.35f;
	constexpr float ELITE_EXP_RATE = 2.5f;
	constexpr float ELITE_CHANCE_FIELD = 0.10f;
	constexpr float ELITE_CHANCE_OUTER = 0.18f;

	// 띠마다 몬스터 수치에 곱한다. 멀리 갈수록 세고 많이 준다
	constexpr float BAND_POWER_INTRO = 0.6f;
	constexpr float BAND_POWER_FIELD = 1.f;
	constexpr float BAND_POWER_OUTER = 1.6f;

	constexpr float MAGNET_RANGE = 2.5f;
	constexpr float MAGNET_ACCEL = 14.f;
	constexpr float MAGNET_MAX_SPEED = 10.f;
	constexpr float PICKUP_RANGE = 0.35f;
	constexpr float MATERIAL_PICKUP_RANGE = 1.3f;
}
