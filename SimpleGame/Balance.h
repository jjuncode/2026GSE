#pragma once

// 데이터 파일( Data/*.json )로 안 뺀 수치. 구역 배치와 NPC 쪽이 쓴다
namespace Balance
{
	constexpr float BODY_RADIUS = 0.3f;	// 몸 반경 ( 칸 )

	constexpr float HIT_FLASH_SEC = 0.12f;
	constexpr float SWING_KNOCKBACK = 0.25f;	// NPC 끼리 칠 때 밀림

	constexpr int32 BOSS_UNLOCK_LEVEL = 5;	// 이 레벨이 돼야 성문파쇄자가 깨어난다

	constexpr float ELITE_HP_RATE = 2.2f;
	constexpr float ELITE_ATTACK_RATE = 1.35f;
	constexpr float ELITE_EXP_RATE = 2.5f;
	constexpr float ELITE_CHANCE_FIELD = 0.10f;
	constexpr float ELITE_CHANCE_OUTER = 0.18f;

	// 띠마다 몬스터 수치에 곱한다. 멀리 갈수록 세고 많이 준다
	constexpr float BAND_POWER_INTRO = 0.6f;
	constexpr float BAND_POWER_FIELD = 1.f;
	constexpr float BAND_POWER_OUTER = 1.6f;

	// 띠마다 레벨을 민다. 레벨 차이 경험치 보정이 이걸 본다
	constexpr int32 BAND_LEVEL_INTRO = -1;
	constexpr int32 BAND_LEVEL_FIELD = 0;
	constexpr int32 BAND_LEVEL_OUTER = 3;
}
