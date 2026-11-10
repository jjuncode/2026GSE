#include "stdafx.h"
#include "Instance.h"
#include "Audio.h"
#include "Drop.h"
#include "Feel.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Monster.h"
#include "Player.h"
#include "Quest.h"
#include "Rank.h"
#include "Renderer.h"
#include "Text.h"
#include "Village.h"
#include "World.h"
#include "ZoneGen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	constexpr int32 STATE_IDLE = 0;	// 아무도 없다. 몬스터는 꺼져 있다
	constexpr int32 STATE_RUNNING = 1;
	constexpr int32 STATE_CLEARED = 2;	// 다 비웠다. 출구가 열려 있다

	constexpr int32 WALL = 2;	// 방 사이 벽 두께
	constexpr int32 CORNER_MARGIN = 4;
	const float ENTRANCE_RANGE = 2.2f;
	const float RIFT_SPARK_SEC = 0.12f;

	struct Room
	{
		int32 x0 = 0;	// 바닥 칸 ( 포함 )
		int32 y0 = 0;
		int32 x1 = 0;	// 바닥 칸 ( 미포함 )
		int32 y1 = 0;
		std::vector<int32> monsters;
		std::vector<ZoneTile> gate;	// 다음 방으로 가는 문 칸. 이 방을 비우면 걷힌다
	};

	struct Instance
	{
		const InstanceData* data = nullptr;
		bool carved = false;
		int32 x0 = 0;
		int32 y0 = 0;
		int32 width = 0;
		int32 height = 0;
		std::vector<Room> rooms;
		WorldPos entrance;
		WorldPos start;	// 들어가면 서는 자리
		WorldPos exit;	// 다 비우면 열리는 출구
		int32 state = STATE_IDLE;
		int32 room = 0;	// 지금 싸우는 방
		bool waiting_next = false;	// 결계는 걷혔고 다음 방에 아직 안 들어섰다
	};

	std::vector<Instance> g_instances;
	float g_time = 0.f;
	float g_spark_timer = 0.f;

	size_t ToIndex(int32 x, int32 y)
	{
		return static_cast<size_t>(y) * static_cast<size_t>(ZONE_TILE_COUNT) + static_cast<size_t>(x);
	}

	float GetDistance(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	bool IsInsideRect(int32 x, int32 y, int32 x0, int32 y0, int32 width, int32 height, int32 margin)
	{
		return x >= x0 - margin && y >= y0 - margin && x < x0 + width + margin && y < y0 + height + margin;
	}

	// 마을 · 보스 둥지 · 거점에서 가장 먼 구석. 사람 사는 곳 옆에 낭떠러지 상자가 서면 안 된다
	bool PickCorner(const ZoneLayout& layout, int32 width, int32 height, const std::vector<std::array<int32, 2>>& used, int32* out_x, int32* out_y)
	{
		const std::array<std::array<int32, 2>, 4> corners =
		{
			std::array<int32, 2>{ CORNER_MARGIN, CORNER_MARGIN },
			std::array<int32, 2>{ ZONE_TILE_COUNT - CORNER_MARGIN - width, CORNER_MARGIN },
			std::array<int32, 2>{ CORNER_MARGIN, ZONE_TILE_COUNT - CORNER_MARGIN - height },
			std::array<int32, 2>{ ZONE_TILE_COUNT - CORNER_MARGIN - width, ZONE_TILE_COUNT - CORNER_MARGIN - height },
		};

		float best_score = -1.f;
		bool found = false;

		for (const std::array<int32, 2>& corner : corners)
		{
			bool taken = false;
			for (const std::array<int32, 2>& other : used)
			{
				if (other[0] == corner[0] && other[1] == corner[1])
				{
					taken = true;
				}
			}

			if (taken)
			{
				continue;
			}

			const float center_x = static_cast<float>(corner[0] + width / 2);
			const float center_y = static_cast<float>(corner[1] + height / 2);
			float score = 1.0e9f;

			const auto consider = [&](const ZoneTile& tile)
			{
				const float delta_x = static_cast<float>(tile.x) - center_x;
				const float delta_y = static_cast<float>(tile.y) - center_y;
				score = std::min(score, std::sqrt(delta_x * delta_x + delta_y * delta_y));
			};

			for (const ZoneVillage& village : layout.villages)
			{
				consider(village.center);
			}

			for (const ZoneSite& site : layout.sites)
			{
				consider(site.tile);
			}

			consider(layout.boss);

			if (score > best_score)
			{
				best_score = score;
				*out_x = corner[0];
				*out_y = corner[1];
				found = true;
			}
		}

		return found;
	}

	template <typename T>
	void RemoveInside(std::vector<T>* items, const Instance& instance, ZoneTile (*tile_of)(const T&))
	{
		items->erase(std::remove_if(items->begin(), items->end(),
			[&instance, tile_of](const T& item)
			{
				const ZoneTile tile = tile_of(item);
				return IsInsideRect(tile.x, tile.y, instance.x0, instance.y0, instance.width, instance.height, 3);
			}), items->end());
	}

	ZoneTile TileOfPack(const ZonePack& pack)
	{
		return pack.tile;
	}

	ZoneTile TileOfSite(const ZoneSite& site)
	{
		return site.tile;
	}

	ZoneTile TileOfTile(const ZoneTile& tile)
	{
		return tile;
	}

	// 바깥 한 겹 낭떠러지, 안쪽 한 겹 폐허 벽, 그 안을 방으로 나눈다. 방 사이 벽 가운데 세 칸이 문이다
	void Carve(ZoneLayout* layout, Instance* instance)
	{
		for (int32 y = instance->y0; y < instance->y0 + instance->height; ++y)
		{
			for (int32 x = instance->x0; x < instance->x0 + instance->width; ++x)
			{
				const bool edge = x == instance->x0 || y == instance->y0 || x == instance->x0 + instance->width - 1 || y == instance->y0 + instance->height - 1;
				uint8 tile = TILE_RUIN;
				if (edge)
				{
					tile = TILE_CLIFF;
				}

				layout->tiles[ToIndex(x, y)] = tile;
				layout->village_map[ToIndex(x, y)] = ZONE_NO_VILLAGE;
			}
		}

		const InstanceData& data = *instance->data;
		const int32 floor_y0 = instance->y0 + 2;
		const int32 floor_y1 = floor_y0 + data.room_height;
		const int32 door_y = (floor_y0 + floor_y1) / 2;

		for (size_t index = 0; index < data.rooms.size(); ++index)
		{
			Room room;
			room.x0 = instance->x0 + 2 + static_cast<int32>(index) * (data.room_width + WALL);
			room.x1 = room.x0 + data.room_width;
			room.y0 = floor_y0;
			room.y1 = floor_y1;

			for (int32 y = room.y0; y < room.y1; ++y)
			{
				for (int32 x = room.x0; x < room.x1; ++x)
				{
					layout->tiles[ToIndex(x, y)] = TILE_PLAZA;
				}
			}

			// 다음 방으로 가는 문. 바닥으로 파 두고 결계로 막는다
			if (index + 1 < data.rooms.size())
			{
				for (int32 x = room.x1; x < room.x1 + WALL; ++x)
				{
					for (int32 y = door_y - 1; y <= door_y + 1; ++y)
					{
						layout->tiles[ToIndex(x, y)] = TILE_PLAZA;

						ZoneTile gate;
						gate.x = x;
						gate.y = y;
						room.gate.push_back(gate);
					}
				}
			}

			instance->rooms.push_back(room);
		}

		const Room& first = instance->rooms.front();
		const Room& last = instance->rooms.back();
		instance->start = ZoneTileToWorld(first.x0 + 2, door_y, 0.5f, 0.5f);
		instance->exit = ZoneTileToWorld((last.x0 + last.x1) / 2, door_y, 0.5f, 0.5f);

		// 판 자리에 걸린 것들을 걷어 낸다. 무리 · 거점이 낭떠러지 속에 서면 안 된다
		RemoveInside(&layout->packs, *instance, TileOfPack);
		RemoveInside(&layout->sites, *instance, TileOfSite);
		RemoveInside(&layout->herbs, *instance, TileOfTile);
		RemoveInside(&layout->relics, *instance, TileOfTile);
		RemoveInside(&layout->lamps, *instance, TileOfTile);

		instance->carved = true;
	}

	// 입구는 그 종류 거점 중 성채에서 가장 가까운 곳 곁. 처음 찾아가기 쉬워야 한다
	WorldPos PickEntrance(const ZoneLayout& layout, int32 site_kind)
	{
		const ZoneSite* best = nullptr;
		for (const ZoneSite& site : layout.sites)
		{
			if (site.kind != site_kind)
			{
				continue;
			}

			if (best == nullptr || site.walk_distance < best->walk_distance)
			{
				best = &site;
			}
		}

		ZoneTile base = layout.center;
		if (best != nullptr)
		{
			base = best->tile;
		}

		// 거점 한가운데는 천막 · 화로가 선다. 몇 칸 비켜 빈 칸을 찾는다
		for (int32 radius = 3; radius < 9; ++radius)
		{
			for (int32 step = 0; step < 8; ++step)
			{
				const float angle = static_cast<float>(step) * PI * 0.25f;
				const int32 x = base.x + static_cast<int32>(std::round(std::cos(angle) * static_cast<float>(radius)));
				const int32 y = base.y + static_cast<int32>(std::round(std::sin(angle) * static_cast<float>(radius)));

				if (IsBlockingTile(GetZoneTile(layout, x, y)) == false && GetZoneTile(layout, x, y) != TILE_ROAD)
				{
					return ZoneTileToWorld(x, y, 0.5f, 0.5f);
				}
			}
		}

		return ZoneTileToWorld(base.x + 3, base.y, 0.5f, 0.5f);
	}

	void SetGate(const Room& room, bool blocked)
	{
		for (const ZoneTile& tile : room.gate)
		{
			SetTileBlocked(ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f), blocked);
		}
	}

	void ActivateRoom(Instance* instance, int32 room_index)
	{
		instance->room = room_index;

		for (int32 monster : instance->rooms[static_cast<size_t>(room_index)].monsters)
		{
			SetMonsterInstanced(monster, true);
		}
	}

	// 처음 상태로. 몬스터는 끄고 결계는 다시 친다
	void ResetInstance(Instance* instance)
	{
		for (const Room& room : instance->rooms)
		{
			for (int32 monster : room.monsters)
			{
				SetMonsterInstanced(monster, false);
			}

			SetGate(room, true);
		}

		instance->state = STATE_IDLE;
		instance->room = 0;
		instance->waiting_next = false;
	}

	// 다음 방 문 한가운데. 결계가 걷힌 뒤 어디로 가는지 바닥에 보여 준다
	WorldPos GetDoorPos(const Room& room)
	{
		if (room.gate.empty())
		{
			return ZoneTileToWorld((room.x0 + room.x1) / 2, (room.y0 + room.y1) / 2, 0.5f, 0.5f);
		}

		// 문 칸들의 한가운데. 칸 하나를 고르면 문 모서리를 겨눠 벽에 걸린다
		float sum_x = 0.f;
		float sum_y = 0.f;
		for (const ZoneTile& tile : room.gate)
		{
			sum_x += static_cast<float>(tile.x);
			sum_y += static_cast<float>(tile.y);
		}

		const float count = static_cast<float>(room.gate.size());
		const int32 base_x = room.gate.front().x;
		const int32 base_y = room.gate.front().y;
		return ZoneTileToWorld(base_x, base_y, sum_x / count - static_cast<float>(base_x) + 0.5f, sum_y / count - static_cast<float>(base_y) + 0.5f);
	}

	bool IsRoomCleared(const Room& room)
	{
		for (int32 monster : room.monsters)
		{
			if (IsMonsterAlive(monster))
			{
				return false;
			}
		}

		return true;
	}

	bool IsInside(const Instance& instance, const WorldPos& pos)
	{
		int32 x = 0;
		int32 y = 0;
		if (WorldToZoneTile(pos, &x, &y) == false)
		{
			return false;
		}

		return IsInsideRect(x, y, instance.x0, instance.y0, instance.width, instance.height, 0);
	}

	void FinishInstance(Instance* instance, int32 index)
	{
		instance->state = STATE_CLEARED;

		const InstanceData& data = *instance->data;
		const int32 gold = data.reward.gold_min + static_cast<int32>((g_time * 997.f)) % std::max(1, data.reward.gold_max - data.reward.gold_min + 1);
		SpawnRewardDrops(instance->exit, data.reward, gold, 1);

		if (data.reward.exp > 0)
		{
			MutablePlayer().AddExp(data.reward.exp);
		}

		AddRankPoints(data.rank_points);
		NotifyQuestInstanceClear(index);

		AddDamageText(GetPlayer().m_pos, 170.f, data.name + L" 정화", DAMAGE_STYLE_BANNER_GOOD);
		AddScreenFlash(0.8f, 0.7f, 1.f, 0.4f);
		PlaySfx(SFX_LEVEL_UP, 1.f);

		std::cout << "[ Instance - FinishInstance ] : " << data.id << " cleared\n";
	}

	void UpdateInstance(Instance* instance, int32 index)
	{
		if (instance->state == STATE_IDLE)
		{
			return;
		}

		// 죽으면 성채에서 일어선다. 던전은 처음으로
		if (IsPlayerDead())
		{
			ResetInstance(instance);
			std::cout << "[ Instance - UpdateInstance ] : " << instance->data->id << " reset ( player died )\n";
			return;
		}

		if (instance->state != STATE_RUNNING)
		{
			return;
		}

		// 다음 방 몬스터는 들어서야 깨운다. 결계가 걷히자마자 깨우면 길을 못 찾고 벽에 붙어 버린다
		if (instance->waiting_next)
		{
			const Room& next = instance->rooms[static_cast<size_t>(instance->room + 1)];
			int32 x = 0;
			int32 y = 0;
			if (WorldToZoneTile(GetPlayer().m_pos, &x, &y) && x >= next.x0)
			{
				instance->waiting_next = false;
				ActivateRoom(instance, instance->room + 1);
			}

			return;
		}

		Room& room = instance->rooms[static_cast<size_t>(instance->room)];
		if (IsRoomCleared(room) == false)
		{
			return;
		}

		if (instance->room + 1 >= static_cast<int32>(instance->rooms.size()))
		{
			FinishInstance(instance, index);
			return;
		}

		SetGate(room, false);
		AddShake(0.25f);
		PlaySfx(SFX_ENHANCE_SUCCESS, 0.8f);
		AddDamageText(GetPlayer().m_pos, 150.f, L"결계가 걷혔다", DAMAGE_STYLE_BANNER_GOOD);
		instance->waiting_next = true;
	}

	void DrawRift(Renderer* renderer, const WorldPos& camera, const WorldPos& pos, float r, float g, float b, float size)
	{
		const ScreenPos screen = WorldToScreen(pos, camera);
		const float breath = 0.5f + 0.5f * std::sin(g_time * 3.f);

		renderer->DrawSolidDiamond(screen.x, screen.y, size * (1.5f + breath * 0.15f), r * 0.4f, g * 0.4f, b * 0.4f, 0.35f);
		renderer->DrawSolidDiamond(screen.x, screen.y, size * 1.1f, 0.03f, 0.01f, 0.05f, 0.95f);
		renderer->DrawSolidDiamond(screen.x, screen.y, size * 0.75f, r, g, b, 0.55f + breath * 0.3f);
		renderer->DrawSolidDiamond(screen.x, screen.y, size * 0.45f, 0.02f, 0.f, 0.04f, 1.f);
	}

	void DrawPrompt(Renderer* renderer, const WorldPos& camera, const WorldPos& pos, const std::wstring& near_text, const std::wstring& far_text)
	{
		const float distance = GetDistance(GetPlayer().m_pos, pos);
		if (distance > 12.f)
		{
			return;
		}

		const ScreenPos screen = WorldToScreen(pos, camera);
		if (distance <= ENTRANCE_RANGE)
		{
			DrawStringShadow(renderer, near_text, screen.x, screen.y + 70.f, TEXT_ALIGN_CENTER, 1.f, 0.85f, 0.45f, 1.f);
			return;
		}

		DrawStringShadow(renderer, far_text, screen.x, screen.y + 70.f, TEXT_ALIGN_CENTER, 0.78f, 0.6f, 0.95f, 0.85f);
	}
}

void CarveInstances(ZoneLayout* layout)
{
	g_instances.clear();

	std::vector<std::array<int32, 2>> used;
	for (const InstanceData& data : GetInstanceDatas())
	{
		Instance instance;
		instance.data = &data;
		instance.width = static_cast<int32>(data.rooms.size()) * (data.room_width + WALL) - WALL + 4;
		instance.height = data.room_height + 4;

		if (data.rooms.empty() || PickCorner(*layout, instance.width, instance.height, used, &instance.x0, &instance.y0) == false)
		{
			std::cout << "[ Instance - CarveInstances ] : no room for " << data.id << "\n";
			continue;
		}

		used.push_back({ instance.x0, instance.y0 });
		Carve(layout, &instance);
		instance.entrance = PickEntrance(*layout, data.entrance_site);
		g_instances.push_back(instance);

		std::cout << "[ Instance - CarveInstances ] : " << data.id << " at " << instance.x0 << "," << instance.y0 << " size " << instance.width << "x" << instance.height << "\n";
	}
}

void BuildInstances()
{
	for (Instance& instance : g_instances)
	{
		const InstanceData& data = *instance.data;

		for (size_t room_index = 0; room_index < instance.rooms.size(); ++room_index)
		{
			Room& room = instance.rooms[room_index];
			room.monsters.clear();

			// 방 안쪽 격자에 고르게 세운다. 문 앞 두 칸은 비운다
			std::mt19937 rng(static_cast<uint32>(room_index * 7919 + room.x0));
			std::uniform_int_distribution<int32> roll_x(room.x0 + 3, room.x1 - 3);
			std::uniform_int_distribution<int32> roll_y(room.y0 + 2, room.y1 - 3);

			for (const InstanceSpawn& spawn : data.rooms[room_index].spawns)
			{
				for (int32 count = 0; count < spawn.count; ++count)
				{
					WorldPos pos = ZoneTileToWorld((room.x0 + room.x1) / 2, (room.y0 + room.y1) / 2, 0.5f, 0.5f);
					if (count > 0 || spawn.count > 1)
					{
						pos = ZoneTileToWorld(roll_x(rng), roll_y(rng), 0.5f, 0.5f);
					}

					const int32 monster = AddMonster(spawn.monster_type - 1, pos, BAND_FIELD, false);
					SetMonsterInstanced(monster, false);
					room.monsters.push_back(monster);
				}
			}
		}

		ResetInstance(&instance);
	}
}

void UpdateInstances(float real_seconds)
{
	g_time += real_seconds;

	for (size_t index = 0; index < g_instances.size(); ++index)
	{
		UpdateInstance(&g_instances[index], static_cast<int32>(index));
	}

	// 입구 균열에서 보랏빛 불티가 피어오른다. 멀리서도 저기가 다르다는 게 보여야 한다
	g_spark_timer -= real_seconds;
	if (g_spark_timer <= 0.f)
	{
		g_spark_timer = RIFT_SPARK_SEC;

		for (const Instance& instance : g_instances)
		{
			if (GetDistance(GetPlayer().m_pos, instance.entrance) < 16.f)
			{
				SpawnSparks(instance.entrance, 8.f, 0.f, 1.f, 1, 40.f, 0.6f, 0.9f, 0.4f, 1.6f, 1.2f);
			}
		}
	}
}

bool InteractInstance()
{
	const Player& player = GetPlayer();

	for (size_t index = 0; index < g_instances.size(); ++index)
	{
		Instance& instance = g_instances[index];
		const InstanceData& data = *instance.data;

		if (instance.state == STATE_CLEARED && GetDistance(player.m_pos, instance.exit) <= ENTRANCE_RANGE)
		{
			ResetInstance(&instance);
			WorldPos out = instance.entrance;
			MoveWorldPos(&out, 1.2f, 1.2f);
			TeleportPlayer(out);
			PlaySfx(SFX_DODGE, 0.8f);
			return true;
		}

		if (GetDistance(player.m_pos, instance.entrance) > ENTRANCE_RANGE)
		{
			continue;
		}

		if (GetPlayerRank() < data.min_rank)
		{
			AddDamageText(player.m_pos, 120.f, GetRankName(data.min_rank) + L" 등급부터 들어갈 수 있다", DAMAGE_STYLE_INFO);
			PlaySfx(SFX_UI_ERROR, 0.6f);
			return true;
		}

		ResetInstance(&instance);
		instance.state = STATE_RUNNING;
		ActivateRoom(&instance, 0);
		TeleportPlayer(instance.start);

		AddScreenFlash(0.4f, 0.1f, 0.6f, 0.5f);
		AddShake(0.3f);
		PlaySfx(SFX_DANGER, 0.9f);
		AddDamageText(player.m_pos, 170.f, data.name, DAMAGE_STYLE_BANNER_BAD);

		std::cout << "[ Instance - InteractInstance ] : enter " << data.id << "\n";
		return true;
	}

	return false;
}

bool GetInstanceGuide(WorldPos* pos)
{
	for (const Instance& instance : g_instances)
	{
		if (IsInside(instance, GetPlayer().m_pos) == false)
		{
			continue;
		}

		if (instance.state == STATE_RUNNING && instance.waiting_next)
		{
			*pos = GetDoorPos(instance.rooms[static_cast<size_t>(instance.room)]);
			return true;
		}

		if (instance.state == STATE_CLEARED)
		{
			*pos = instance.exit;
			return true;
		}
	}

	return false;
}

bool IsPlayerInInstance()
{
	for (const Instance& instance : g_instances)
	{
		if (IsInside(instance, GetPlayer().m_pos))
		{
			return true;
		}
	}

	return false;
}

int32 GetInstanceCount()
{
	return static_cast<int32>(g_instances.size());
}

bool GetInstanceEntrance(int32 index, WorldPos* pos, std::wstring* name)
{
	if (index < 0 || index >= static_cast<int32>(g_instances.size()))
	{
		return false;
	}

	*pos = g_instances[static_cast<size_t>(index)].entrance;
	*name = g_instances[static_cast<size_t>(index)].data->name;
	return true;
}

void DrawInstanceGround(Renderer* renderer, const WorldPos& camera)
{
	for (const Instance& instance : g_instances)
	{
		DrawRift(renderer, camera, instance.entrance, 0.75f, 0.3f, 1.4f, 1.5f);

		if (instance.state == STATE_CLEARED)
		{
			DrawRift(renderer, camera, instance.exit, 1.3f, 1.1f, 0.6f, 1.f);
		}

		// 아직 안 걷힌 결계. 칸마다 보랏빛 마름모가 깜빡인다
		for (size_t room_index = 0; room_index < instance.rooms.size(); ++room_index)
		{
			const bool open = instance.state != STATE_IDLE && static_cast<int32>(room_index) < instance.room;
			if (open || instance.state == STATE_CLEARED)
			{
				continue;
			}

			for (const ZoneTile& tile : instance.rooms[room_index].gate)
			{
				const ScreenPos screen = WorldToScreen(ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f), camera);
				const float flicker = 0.6f + 0.4f * std::sin(g_time * 7.f + static_cast<float>(tile.x + tile.y));
				renderer->DrawSolidDiamond(screen.x, screen.y, 1.f, 0.6f * flicker, 0.2f * flicker, 1.2f * flicker, 0.55f);
			}
		}

		// 걷힌 문은 금빛으로 숨 쉰다. 다음 방이 어디인지
		if (instance.state == STATE_RUNNING && instance.waiting_next)
		{
			const ScreenPos door = WorldToScreen(GetDoorPos(instance.rooms[static_cast<size_t>(instance.room)]), camera);
			const float breath = 0.5f + 0.5f * std::sin(g_time * 4.f);
			renderer->DrawSolidDiamond(door.x, door.y, 1.4f + breath * 0.3f, 1.f, 0.8f, 0.35f, 0.18f + breath * 0.12f);
		}
	}
}

void DrawInstanceOverlay(Renderer* renderer, const WorldPos& camera)
{
	for (const Instance& instance : g_instances)
	{
		const InstanceData& data = *instance.data;

		std::wstring rank_note;
		if (GetPlayerRank() < data.min_rank)
		{
			rank_note = L"  ( " + GetRankName(data.min_rank) + L"부터 )";
		}

		DrawPrompt(renderer, camera, instance.entrance, L"F  " + data.name + L" 들어가기" + rank_note, data.name);

		if (instance.state == STATE_CLEARED)
		{
			DrawPrompt(renderer, camera, instance.exit, L"F  나가기", L"출구");
		}

		if (instance.state != STATE_RUNNING || IsInside(instance, GetPlayer().m_pos) == false)
		{
			continue;
		}

		// 방 진행. 남은 적 수가 보여야 결계가 왜 안 걷히는지 안다
		const Room& room = instance.rooms[static_cast<size_t>(instance.room)];
		int32 left = 0;
		for (int32 monster : room.monsters)
		{
			if (IsMonsterAlive(monster))
			{
				left += 1;
			}
		}

		const float y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - 96.f;
		std::wstring line = data.name + L"   방 " + std::to_wstring(instance.room + 1) + L" / " + std::to_wstring(instance.rooms.size()) + L"   남은 적 " + std::to_wstring(left);
		if (instance.waiting_next)
		{
			line = data.name + L"   방 " + std::to_wstring(instance.room + 1) + L" 비움   금빛 문으로";
		}
		renderer->DrawSolidBox(0.f, y - 12.f, MeasureString(line) + 40.f, 34.f, 0.02f, 0.01f, 0.04f, 0.6f);
		DrawStringShadow(renderer, line, 0.f, y, TEXT_ALIGN_CENTER, 0.85f, 0.7f, 1.f, 1.f);
	}
}
