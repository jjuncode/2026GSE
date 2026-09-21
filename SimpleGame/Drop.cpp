#include "stdafx.h"
#include "Drop.h"
#include "Balance.h"
#include "FloatingText.h"
#include "Player.h"
#include "SceneGraph.h"
#include "Text.h"
#include "World.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	constexpr int32 DROP_EXP = 0;
	constexpr int32 DROP_GOLD = 1;
	constexpr int32 DROP_MATERIAL = 2;

	constexpr int32 EXP_PER_ORB = 5;
	constexpr int32 BOSS_GOLD_PILE_COUNT = 6;
	constexpr float SCATTER_MIN = 0.2f;
	constexpr float SCATTER_MAX = 0.7f;
	constexpr float MAGNET_DELAY_SEC = 0.35f;	// 떨어지자마자 빨려 들면 드랍이 안 보인다

	struct Drop
	{
		Actor body;
		int32 kind = DROP_EXP;
		int32 value = 0;	// 경험치량 / 돈 / 재료 등급
		float speed = 0.f;	// 끌려오는 속도
		float age = 0.f;
	};

	std::vector<Drop> g_drops;

	// 지형 씨앗과 갈라 둔다 ( 2.9 )
	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x5bd1e995u);
		return random;
	}

	float RollRange(float min_value, float max_value)
	{
		std::uniform_real_distribution<float> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	int32 RollInt(int32 min_value, int32 max_value)
	{
		std::uniform_int_distribution<int32> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	bool RollChance(float chance)
	{
		return RollRange(0.f, 1.f) < chance;
	}

	void ApplyLook(Drop* drop)
	{
		Actor& body = drop->body;

		// 작은 드랍까지 그림자를 깔면 바닥이 지저분하다
		body.m_casts_shadow = false;

		if (drop->kind == DROP_EXP)
		{
			body.m_sprite = SPRITE_EXP_ORB;
			body.m_emissive = 1.6f;
			return;
		}

		if (drop->kind == DROP_GOLD)
		{
			body.m_sprite = SPRITE_GOLD;
			body.m_emissive = 1.3f;
			return;
		}

		// 등급은 색과 밝기로 낸다 ( 2.9 ). 별핵이 화면에서 제일 밝아야 한다
		if (drop->value == MATERIAL_CORE)
		{
			body.m_sprite = SPRITE_CORE;
			body.m_emissive = 4.f;
		}
		else if (drop->value == MATERIAL_FRAGMENT)
		{
			body.m_sprite = SPRITE_FRAGMENT;
			body.m_emissive = 2.6f;
		}
		else
		{
			body.m_sprite = SPRITE_DUST;
			body.m_emissive = 1.6f;
		}
	}

	void AddDrop(const WorldPos& origin, int32 kind, int32 value)
	{
		Drop drop;
		drop.kind = kind;
		drop.value = value;
		drop.body.m_pos = origin;

		// 벽 안으로 튀면 못 줍는다
		const float angle = RollRange(0.f, 2.f * PI);
		const float distance = RollRange(SCATTER_MIN, SCATTER_MAX);
		MoveWithCollision(&drop.body.m_pos, 0.1f, std::cos(angle) * distance, std::sin(angle) * distance);

		ApplyLook(&drop);
		g_drops.push_back(drop);
	}

	void Collect(const Drop& drop)
	{
		if (drop.kind == DROP_EXP)
		{
			AddPlayerExp(drop.value);
			return;
		}

		if (drop.kind == DROP_GOLD)
		{
			MutablePlayer().gold += drop.value;
			AddFloatingText(drop.body.m_pos, 20.f, L"골드 +" + std::to_wstring(drop.value), 1.f, 0.82f, 0.30f);
		}
	}

	void RemoveAt(size_t index)
	{
		g_drops[index] = g_drops.back();
		g_drops.pop_back();
	}
}

void ClearDrops()
{
	g_drops.clear();
}

void SpawnMonsterDrops(const WorldPos& pos, int32 exp, int32 gold_min, int32 gold_max, bool is_boss)
{
	int32 remain_exp = exp;
	while (remain_exp > 0)
	{
		int32 orb = EXP_PER_ORB;
		if (remain_exp < orb)
		{
			orb = remain_exp;
		}

		AddDrop(pos, DROP_EXP, orb);
		remain_exp -= orb;
	}

	if (is_boss)
	{
		const int32 gold = RollInt(gold_min, gold_max);

		for (int32 pile = 0; pile < BOSS_GOLD_PILE_COUNT; ++pile)
		{
			AddDrop(pos, DROP_GOLD, gold / BOSS_GOLD_PILE_COUNT);
		}

		for (int32 count = 0; count < 5; ++count)
		{
			AddDrop(pos, DROP_MATERIAL, MATERIAL_DUST);
		}

		AddDrop(pos, DROP_MATERIAL, MATERIAL_FRAGMENT);
		AddDrop(pos, DROP_MATERIAL, MATERIAL_FRAGMENT);

		if (RollChance(0.4f))
		{
			AddDrop(pos, DROP_MATERIAL, MATERIAL_CORE);
		}

		return;
	}

	if (RollChance(0.7f))
	{
		AddDrop(pos, DROP_GOLD, RollInt(gold_min, gold_max));
	}

	if (RollChance(0.4f))
	{
		AddDrop(pos, DROP_MATERIAL, MATERIAL_DUST);
	}

	if (RollChance(0.05f))
	{
		AddDrop(pos, DROP_MATERIAL, MATERIAL_FRAGMENT);
	}

	if (RollChance(0.008f))
	{
		AddDrop(pos, DROP_MATERIAL, MATERIAL_CORE);
	}
}

void UpdateDrops(float delta_seconds)
{
	if (IsPlayerDead())
	{
		return;
	}

	const WorldPos player_pos = GetPlayer().body.m_pos;

	size_t index = 0;
	while (index < g_drops.size())
	{
		Drop& drop = g_drops[index];
		drop.age += delta_seconds;

		if (drop.kind == DROP_MATERIAL)
		{
			index += 1;
			continue;
		}

		const float to_x = GetTileDistanceX(drop.body.m_pos, player_pos);
		const float to_y = GetTileDistanceY(drop.body.m_pos, player_pos);
		const float distance = std::sqrt(to_x * to_x + to_y * to_y);

		if (distance <= Balance::PICKUP_RANGE)
		{
			Collect(drop);
			RemoveAt(index);
			continue;
		}

		if (drop.age < MAGNET_DELAY_SEC)
		{
			index += 1;
			continue;
		}

		// 한 번 끌리기 시작하면 범위를 벗어나도 끝까지 온다
		if (drop.speed <= 0.f && distance > Balance::MAGNET_RANGE)
		{
			index += 1;
			continue;
		}

		drop.speed += Balance::MAGNET_ACCEL * delta_seconds;
		if (drop.speed > Balance::MAGNET_MAX_SPEED)
		{
			drop.speed = Balance::MAGNET_MAX_SPEED;
		}

		float step = drop.speed * delta_seconds;
		if (step > distance)
		{
			step = distance;
		}

		MoveWorldPos(&drop.body.m_pos, to_x / distance * step, to_y / distance * step);
		index += 1;
	}
}

int32 PickupNearbyMaterials()
{
	if (IsPlayerDead())
	{
		return 0;
	}

	const WorldPos player_pos = GetPlayer().body.m_pos;
	PlayerState& player = MutablePlayer();

	int32 picked = 0;
	size_t index = 0;
	while (index < g_drops.size())
	{
		const Drop& drop = g_drops[index];

		if (drop.kind != DROP_MATERIAL)
		{
			index += 1;
			continue;
		}

		const float to_x = GetTileDistanceX(drop.body.m_pos, player_pos);
		const float to_y = GetTileDistanceY(drop.body.m_pos, player_pos);

		if (to_x * to_x + to_y * to_y > Balance::MATERIAL_PICKUP_RANGE * Balance::MATERIAL_PICKUP_RANGE)
		{
			index += 1;
			continue;
		}

		player.materials[static_cast<size_t>(drop.value)] += 1;
		picked += 1;
		AddFloatingText(drop.body.m_pos, 22.f, std::wstring(GetMaterialName(drop.value)) + L" 획득", 0.85f, 0.90f, 1.f);
		RemoveAt(index);
	}

	if (picked > 0)
	{
		std::cout << "[ Drop - PickupNearbyMaterials ] : picked " << picked << "\n";
	}

	return picked;
}

void CollectDropActors()
{
	for (const Drop& drop : g_drops)
	{
		AddSceneActor(&drop.body);
	}
}

void DrawDropLabels(Renderer* renderer, const WorldPos& camera)
{
	for (const Drop& drop : g_drops)
	{
		if (drop.kind != DROP_MATERIAL)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(drop.body.m_pos, camera);

		float r = 0.90f;
		float g = 0.92f;
		float b = 0.98f;

		if (drop.value == MATERIAL_CORE)
		{
			r = 1.00f;
			g = 0.60f;
			b = 0.20f;
		}
		else if (drop.value == MATERIAL_FRAGMENT)
		{
			r = 0.45f;
			g = 0.70f;
			b = 1.00f;
		}

		DrawStringShadow(renderer, GetMaterialName(drop.value), screen.x, screen.y + 34.f, TEXT_ALIGN_CENTER, r, g, b, 1.f);
	}
}
