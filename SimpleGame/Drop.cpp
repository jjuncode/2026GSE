#include "stdafx.h"
#include "Drop.h"
#include "Audio.h"
#include "Feel.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Player.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Text.h"
#include "World.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	const float SCATTER_MIN = 0.25f;
	const float SCATTER_MAX = 0.8f;
	const float HOP_SEC = 0.4f;	// 떨어질 때 한 번 튀어 오른다
	const float HOP_HEIGHT = 9.f * static_cast<float>(PIXEL_SCALE);
	const float MAGNET_DELAY_SEC = 0.45f;	// 떨어지자마자 빨려 들면 드랍이 안 보인다
	const float MAGNET_ACCEL = 18.f;
	const float MAGNET_MAX_SPEED = 12.f;
	const float GOLD_PICKUP_RANGE = 0.35f;
	const float LABEL_RANGE = 6.f;
	const float BLINK_SEC = 6.f;	// 사라지기 전 깜빡이는 구간
	constexpr int32 GOLD_PER_PILE = 25;

	struct WorldDrop
	{
		Actor body;
		int32 item_id = ITEM_ID_NONE;
		int32 count = 0;
		int32 gold = 0;	// 0 보다 크면 골드 더미
		float age = 0.f;
		float speed = 0.f;
		float base_emissive = 1.f;
	};

	std::vector<WorldDrop> g_drops;

	// 지형 씨앗과 갈라 둔다 ( 설계 2.9 )
	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x7f4a7c15u);
		return random;
	}

	float RollRange(float min_value, float max_value)
	{
		std::uniform_real_distribution<float> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	int32 RollInt(int32 min_value, int32 max_value)
	{
		if (max_value < min_value)
		{
			return min_value;
		}

		std::uniform_int_distribution<int32> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	float DistanceBetween(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	void Scatter(WorldDrop* drop, const WorldPos& origin)
	{
		drop->body.m_pos = origin;

		// 벽 안으로 튀면 못 줍는다
		const float angle = RollRange(0.f, 2.f * PI);
		const float distance = RollRange(SCATTER_MIN, SCATTER_MAX);
		MoveWithCollision(&drop->body.m_pos, 0.1f, std::cos(angle) * distance, std::sin(angle) * distance);
	}

	void PushDrop(const WorldDrop& drop)
	{
		g_drops.push_back(drop);
	}

	std::wstring GetDropLabel(const WorldDrop& drop)
	{
		const ItemData& item = GetItemData(drop.item_id);

		if (drop.count > 1)
		{
			return item.name + L" x" + std::to_wstring(drop.count);
		}

		return item.name;
	}

	void GetLabelColor(int32 item_id, float* r, float* g, float* b)
	{
		if (item_id == ITEM_ID_SPELL_TRACE)
		{
			*r = 0.72f;
			*g = 0.62f;
			*b = 1.f;
			return;
		}

		if (item_id == ITEM_ID_HP_POTION)
		{
			*r = 1.f;
			*g = 0.55f;
			*b = 0.5f;
			return;
		}

		*r = 1.f;
		*g = 0.88f;
		*b = 0.6f;
	}

	int32 FindNearestItem(float range)
	{
		const WorldPos player_pos = GetPlayer().m_pos;
		int32 best = -1;
		float best_distance = range;

		for (size_t index = 0; index < g_drops.size(); ++index)
		{
			const WorldDrop& drop = g_drops[index];
			if (drop.gold > 0)
			{
				continue;
			}

			const float distance = DistanceBetween(player_pos, drop.body.m_pos);
			if (distance <= best_distance)
			{
				best_distance = distance;
				best = static_cast<int32>(index);
			}
		}

		return best;
	}
}

void ClearDrops()
{
	g_drops.clear();
}

void SpawnItemDrop(const WorldPos& pos, int32 item_id, int32 count)
{
	const ItemData& item = GetItemData(item_id);
	if (item.item_id == ITEM_ID_NONE || count <= 0)
	{
		return;
	}

	WorldDrop drop;
	drop.item_id = item_id;
	drop.count = count;
	drop.body.m_sprite = item.drop_sprite;
	drop.body.m_casts_shadow = false;

	// 주문의 흔적이 바닥에서 제일 눈에 띈다. 어두운 바닥에 밝은 점이 떨어지면 화면이 알아서 강조한다
	drop.base_emissive = 1.6f;
	if (item_id == ITEM_ID_SPELL_TRACE)
	{
		drop.base_emissive = 2.6f;
	}

	drop.body.m_emissive = drop.base_emissive;

	Scatter(&drop, pos);
	PushDrop(drop);
}

void SpawnGoldDrop(const WorldPos& pos, int32 gold)
{
	int32 remain = gold;

	// 한 무더기가 크면 몇 개로 나눠 흩는다. 쏟아지는 맛
	while (remain > 0)
	{
		const int32 pile = std::min(remain, GOLD_PER_PILE);
		remain -= pile;

		WorldDrop drop;
		drop.gold = pile;
		drop.body.m_sprite = SPRITE_GOLD;
		drop.body.m_casts_shadow = false;
		drop.base_emissive = 1.4f;
		drop.body.m_emissive = drop.base_emissive;

		Scatter(&drop, pos);
		PushDrop(drop);
	}
}

void SpawnRewardDrops(const WorldPos& pos, const MonsterRewardData& reward, int32 gold, int32 roll_count)
{
	if (gold > 0)
	{
		SpawnGoldDrop(pos, gold);
	}

	for (int32 roll = 0; roll < std::max(1, roll_count); ++roll)
	{
		// 줄마다 따로 굴린다. 100 이 나와도 100 퍼센트는 떨어지게 > 로 비교
		for (const DropInfo& info : reward.drops)
		{
			if (RollRange(0.f, 100.f) > info.rate)
			{
				continue;
			}

			SpawnItemDrop(pos, info.item_id, RollInt(info.min_count, info.max_count));
		}
	}
}

void SpawnMonsterDrops(const WorldPos& pos, int32 exp, int32 gold_min, int32 gold_max, int32 tier)
{
	GrantKillReward(exp, GetPlayerLevel());

	const int32 gold = RollInt(gold_min, gold_max);
	if (gold > 0)
	{
		SpawnGoldDrop(pos, gold);
	}

	SpawnItemDrop(pos, ITEM_ID_SPELL_TRACE, 1 + tier);
}

void UpdateDrops(float world_seconds)
{
	const GameConfig& config = GetConfig();
	const Player& player = GetPlayer();
	const bool player_alive = IsPlayerDead() == false;

	int32 gold_gained = 0;

	for (WorldDrop& drop : g_drops)
	{
		drop.age += world_seconds;

		// 떨어질 때 한 번 톡 튄다
		if (drop.age < HOP_SEC)
		{
			const float t = drop.age / HOP_SEC;
			drop.body.m_foot_offset_y = std::sin(t * PI) * HOP_HEIGHT * (1.f - t * 0.5f);
		}
		else
		{
			// 바닥에서 숨 쉬듯 빛이 오르내린다
			drop.body.m_foot_offset_y = 0.f;
			drop.body.m_emissive = drop.base_emissive * (0.85f + 0.15f * std::sin(drop.age * 3.f));
		}

		if (drop.gold <= 0 || player_alive == false || drop.age < MAGNET_DELAY_SEC)
		{
			continue;
		}

		const float delta_x = GetTileDistanceX(drop.body.m_pos, player.m_pos);
		const float delta_y = GetTileDistanceY(drop.body.m_pos, player.m_pos);
		const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);

		if (distance > config.gold_magnet_range && drop.speed <= 0.f)
		{
			continue;
		}

		if (distance <= GOLD_PICKUP_RANGE)
		{
			gold_gained += drop.gold;
			drop.gold = -1;
			continue;
		}

		// 끌려오기 시작하면 벽도 무시한다. 벽에 걸려 안 오면 그게 더 거슬린다
		drop.speed = std::min(drop.speed + MAGNET_ACCEL * world_seconds, MAGNET_MAX_SPEED);
		const float step = std::min(drop.speed * world_seconds, distance);
		MoveWorldPos(&drop.body.m_pos, delta_x / distance * step, delta_y / distance * step);
	}

	if (gold_gained > 0)
	{
		AddPlayerGold(gold_gained);
		AddDamageText(player.m_pos, 46.f, L"+" + std::to_wstring(gold_gained) + L" 골드", DAMAGE_STYLE_INFO);
		PlaySfx(SFX_GOLD, 0.55f);
	}

	g_drops.erase(std::remove_if(g_drops.begin(), g_drops.end(),
		[&config](const WorldDrop& drop)
		{
			if (drop.gold < 0)
			{
				return true;
			}

			return drop.gold == 0 && drop.age >= config.drop_lifetime_sec;
		}), g_drops.end());
}

bool PickupNearestDrop()
{
	if (IsPlayerDead())
	{
		return false;
	}

	const int32 index = FindNearestItem(GetConfig().pickup_range);
	if (index < 0)
	{
		return false;
	}

	WorldDrop& drop = g_drops[static_cast<size_t>(index)];

	// 가방이 차면 드랍은 그대로 남긴다 ( 서버 PickupDrop )
	if (GivePlayerItem(drop.item_id, drop.count) == false)
	{
		AddDamageText(GetPlayer().m_pos, 52.f, L"가방이 가득 찼다", DAMAGE_STYLE_BANNER_BAD);
		PlaySfx(SFX_UI_ERROR, 0.6f);
		return false;
	}

	AddDamageText(GetPlayer().m_pos, 46.f, GetDropLabel(drop) + L" 획득", DAMAGE_STYLE_INFO);
	SpawnSparks(drop.body.m_pos, 4.f, 0.f, 1.f, 8, 80.f, 1.2f, 1.6f, 1.4f, 2.2f, 0.4f);
	PlaySfx(SFX_PICKUP, 0.7f);

	g_drops.erase(g_drops.begin() + index);
	return true;
}

bool HasPickupNear()
{
	return FindNearestItem(GetConfig().pickup_range) >= 0;
}

void CollectDropActors()
{
	for (WorldDrop& drop : g_drops)
	{
		// 사라지기 전엔 깜빡인다
		const float remain = GetConfig().drop_lifetime_sec - drop.age;
		if (drop.gold == 0 && remain < BLINK_SEC && std::fmod(drop.age, 0.4f) < 0.15f)
		{
			continue;
		}

		AddSceneActor(&drop.body);
	}
}

void DrawDropLabels(Renderer* renderer, const WorldPos& camera)
{
	const WorldPos player_pos = GetPlayer().m_pos;
	const int32 nearest = FindNearestItem(GetConfig().pickup_range);

	for (size_t index = 0; index < g_drops.size(); ++index)
	{
		const WorldDrop& drop = g_drops[index];
		if (drop.gold != 0 || drop.age < HOP_SEC)
		{
			continue;
		}

		const float distance = DistanceBetween(player_pos, drop.body.m_pos);
		if (distance > LABEL_RANGE)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(drop.body.m_pos, camera);
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
		GetLabelColor(drop.item_id, &r, &g, &b);

		// 멀수록 흐리게. 화면이 이름표로 뒤덮이지 않게
		const float alpha = std::min(1.f, (LABEL_RANGE - distance) / 2.f + 0.3f);
		DrawStringShadow(renderer, GetDropLabel(drop), screen.x, screen.y + 14.f * static_cast<float>(PIXEL_SCALE), TEXT_ALIGN_CENTER, r, g, b, alpha);

		if (static_cast<int32>(index) == nearest)
		{
			DrawStringShadow(renderer, L"F 줍기", screen.x, screen.y + 14.f * static_cast<float>(PIXEL_SCALE) + 24.f, TEXT_ALIGN_CENTER, 1.f, 0.95f, 0.7f, 1.f);
		}
	}
}
