#include "stdafx.h"
#include "Scene.h"
#include "Actor.h"
#include "Audio.h"
#include "Balance.h"
#include "Board.h"
#include "Drop.h"
#include "Feel.h"
#include "FloatingText.h"
#include "Fog.h"
#include "GameConfig.h"
#include "Ground.h"
#include "HouseArt.h"
#include "Input.h"
#include "Minimap.h"
#include "Monitor.h"
#include "Monster.h"
#include "Npc.h"
#include "NpcHud.h"
#include "Player.h"
#include "PostProcess.h"
#include "Profiler.h"
#include "Quest.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "SpriteAtlas.h"
#include "Text.h"
#include "UiWindows.h"
#include "Village.h"
#include "World.h"
#include "ZoneGen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	const float LIGHT_CORE_GAIN = 6.f;

	// 화면 밖 보스를 가리키는 표식
	const float BOSS_MARKER_MARGIN = 30.f;
	const float BOSS_MARKER_RADIUS = 7.f;

	// 보스 화로 세기. 꺼진 상태로 두면 목적지 자체가 안 보인다
	const float BOSS_LIGHT_DIM = 0.16f;
	const float BOSS_LIGHT_LIT = 0.90f;

	// 입체 블록은 발밑이 칸 가운데가 아니라 칸 앞 꼭짓점이다
	const float BLOCK_FOOT_OFFSET = -TILE_PIXEL_HEIGHT * 0.5f;

	constexpr int32 DEBUG_VIEW_RADIUS = 24;

	const float ANVIL_RANGE = 2.4f;	// 이 안에서 F 나 U 로 강화 창이 열린다
	const float BOARD_RANGE = 2.4f;	// 이 안에서 F 로 게시판이 열린다
	const float SHOP_RANGE = 2.2f;	// 이 안에서 F 로 상점이 열린다
	const float SHOP_LABEL_LIFT = 92.f;	// 간판 34 도트 위
	const float BOARD_LABEL_LIFT = 100.f;	// 게시판 그림 28 도트 x 3 위
	const float PICKUP_REPEAT_SEC = 0.14f;
	const float VILLAGE_BANNER_SEC = 3.2f;

	ZoneLayout g_zone;
	bool g_player_ready = false;
	std::vector<uint8> g_door_tiles;	// 문이 난 집 칸. 집마다 하나

	int32 g_player_village = -1;
	float g_village_banner_timer = 0.f;
	int32 g_village_banner = -1;

	std::vector<Actor> g_decor;

	// 한 채로 구운 집의 칸 조각과 게시판. 그림이 집 그림판에 있어 그리기만 다르다
	class HousePieceActor : public Actor
	{
	public:
		void Draw(Renderer* renderer, const WorldPos& camera) const override
		{
			const ScreenPos screen = WorldToScreen(m_pos, camera);
			const float glow = m_shade * m_emissive;

			if (m_prop >= 0)
			{
				DrawHouseProp(renderer, m_prop, screen.x, screen.y + m_foot_offset_y, glow, glow, glow);
				return;
			}

			DrawHousePiece(renderer, m_piece, screen.x, screen.y + m_foot_offset_y, glow, glow, glow);
		}

		int32 m_piece = -1;
		int32 m_prop = -1;
	};

	std::vector<HousePieceActor> g_house_pieces;
	std::vector<uint8> g_house_piece_tiles;	// 조각으로 세운 집 칸. 여기는 칸 블록을 안 세운다
	std::vector<size_t> g_light_indices;
	std::vector<size_t> g_boss_light_indices;
	std::vector<size_t> g_nearby_lights;

	WorldPos g_anvil_pos;
	bool g_has_anvil = false;

	// 광장 노점마다 상점 하나. 성채는 넷, 개척촌은 둘
	struct ShopSpot
	{
		int32 kind = SHOP_GENERAL;
		WorldPos pos;
	};

	std::vector<ShopSpot> g_shop_spots;
	bool g_interact_held = false;
	float g_interact_timer = 0.f;

	bool g_show_chunk_grid = false;
	bool g_use_lighting = true;
	bool g_player_hidden = false;
	float g_fog_scroll = 0.f;

	uint32 MakeRandomSeed()
	{
		const uint64 ticks = static_cast<uint64>(std::chrono::steady_clock::now().time_since_epoch().count());
		return static_cast<uint32>(ticks ^ (ticks >> 32));
	}

	int32 GetGlobalTileX(const WorldPos& pos)
	{
		return pos.chunk_x * CHUNK_TILE_COUNT + static_cast<int32>(std::floor(pos.local_x));
	}

	int32 GetGlobalTileY(const WorldPos& pos)
	{
		return pos.chunk_y * CHUNK_TILE_COUNT + static_cast<int32>(std::floor(pos.local_y));
	}

	bool IsOffScreen(const ScreenPos& screen, float margin)
	{
		const float limit_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f + margin;
		const float limit_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f + margin;

		if (std::fabs(screen.x) > limit_x)
		{
			return true;
		}

		if (std::fabs(screen.y) > limit_y)
		{
			return true;
		}

		return false;
	}

	float GetDistanceSq(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return delta_x * delta_x + delta_y * delta_y;
	}

	float Jitter(uint32 hash, float amount)
	{
		return (HashToUnit(hash) - 0.5f) * 2.f * amount;
	}

	bool HasNeighborTile(int32 x, int32 y, uint8 kind)
	{
		if (GetZoneTile(g_zone, x + 1, y) == kind || GetZoneTile(g_zone, x - 1, y) == kind)
		{
			return true;
		}

		if (GetZoneTile(g_zone, x, y + 1) == kind || GetZoneTile(g_zone, x, y - 1) == kind)
		{
			return true;
		}

		return false;
	}

	size_t ToZoneIndex(int32 x, int32 y)
	{
		return static_cast<size_t>(y) * static_cast<size_t>(ZONE_TILE_COUNT) + static_cast<size_t>(x);
	}

	bool IsOccluder(const Actor& actor)
	{
		switch (actor.m_sprite)
		{
			case SPRITE_BLOCK_WALL:
			case SPRITE_BLOCK_TOWER:
			case SPRITE_BLOCK_HOUSE:
			case SPRITE_BLOCK_RUIN:
			case SPRITE_DEAD_TREE:
			{
				return true;
			}
			default:
			{
				return false;
			}
		}
	}

	Actor& AddDecorAt(const WorldPos& pos, int32 sprite, int32 frame, float shade, bool casts_shadow)
	{
		Actor object;
		object.m_pos = pos;
		object.m_sprite = sprite;
		object.m_frame = frame;
		object.m_shade = shade;
		object.m_casts_shadow = casts_shadow;

		g_decor.push_back(object);

		return g_decor.back();
	}

	void AddDecor(int32 tile_x, int32 tile_y, int32 sprite, int32 frame, float shade, float jitter, bool casts_shadow)
	{
		// 칸 한가운데에 줄 세우면 격자가 읽힌다. 좌우 반전도 섞어 같은 그림 반복을 가린다
		const uint32 hash = HashZoneTile(g_zone.seed + 7u, tile_x, tile_y);
		const WorldPos pos = ZoneTileToWorld(tile_x, tile_y, 0.5f + Jitter(hash, jitter), 0.5f + Jitter(hash >> 8, jitter));

		Actor& object = AddDecorAt(pos, sprite, frame, shade, casts_shadow);
		object.m_flip = HashToUnit(hash >> 16) > 0.5f;
	}

	// 칸 하나짜리 입체. 그림자 대신 옆면 음영이 있고, 좌우로 뒤집으면 빛 방향이 뒤집힌다
	void AddBlock(int32 tile_x, int32 tile_y, int32 sprite, int32 frame, float shade)
	{
		Actor& block = AddDecorAt(ZoneTileToWorld(tile_x, tile_y, 0.5f, 0.5f), sprite, frame, shade, false);
		block.m_foot_offset_y = BLOCK_FOOT_OFFSET;
	}

	// 성채든 개척촌이든 마을 안이면 마당 흙과 푸른 밭
	bool IsTownTile(int32 x, int32 y)
	{
		return GetZoneVillageAt(g_zone, x, y) >= 0;
	}

	// 문 앞 칸 바로 뒤 집 칸이 문 난 칸이다. 그 칸만 문 그림, 나머지는 창 그림
	void MarkDoorTiles()
	{
		g_door_tiles.assign(static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT), 0);

		for (const ZoneHouse& house : g_zone.houses)
		{
			const int32 x = house.door.x;
			const int32 y = house.door.y - 1;

			if (x >= 0 && y >= 0 && x < ZONE_TILE_COUNT && y < ZONE_TILE_COUNT)
			{
				g_door_tiles[ToZoneIndex(x, y)] = 1;
			}
		}
	}

	// 집마다 지붕 색 하나, 밝기 하나. 칸마다 다르게 주면 한 채 안에 이음매가 보인다
	void BuildHousePieces()
	{
		g_house_piece_tiles.assign(static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT), 0);

		if (HasHouseArt() == false)
		{
			return;
		}

		const int32 roof_count = std::max(1, GetHouseRoofCount());

		for (const ZoneHouse& house : g_zone.houses)
		{
			const uint32 hash = HashZoneTile(g_zone.seed + 919u, house.lot.x, house.lot.y);
			const int32 roof = static_cast<int32>(hash % static_cast<uint32>(roof_count));
			const int32 door_col = house.door.x - house.lot.x;
			const float shade = 0.92f + HashToUnit(hash >> 8) * 0.1f;

			std::vector<int32> pieces;
			bool complete = true;

			for (int32 tile_y = 0; tile_y < house.size_y; ++tile_y)
			{
				for (int32 tile_x = 0; tile_x < house.size_x; ++tile_x)
				{
					const int32 piece = FindHousePiece(house.size_x, house.size_y, door_col, roof, tile_x, tile_y);
					if (piece < 0)
					{
						complete = false;
					}

					pieces.push_back(piece);
				}
			}

			// 구운 적 없는 모양이면 그 집만 옛 칸 블록으로 남긴다
			if (complete == false)
			{
				continue;
			}

			for (int32 tile_y = 0; tile_y < house.size_y; ++tile_y)
			{
				for (int32 tile_x = 0; tile_x < house.size_x; ++tile_x)
				{
					const int32 x = house.lot.x + tile_x;
					const int32 y = house.lot.y + tile_y;

					HousePieceActor actor;
					actor.m_pos = ZoneTileToWorld(x, y, 0.5f, 0.5f);
					actor.m_sprite = SPRITE_BLOCK_HOUSE;	// 그리기엔 안 쓴다. 벽 투시 판정이 블록 크기로 본다
					actor.m_piece = pieces[static_cast<size_t>(tile_y * house.size_x + tile_x)];
					actor.m_shade = shade;
					actor.m_casts_shadow = false;
					actor.m_foot_offset_y = BLOCK_FOOT_OFFSET;
					g_house_pieces.push_back(actor);

					g_house_piece_tiles[ToZoneIndex(x, y)] = 1;
				}
			}
		}
	}

	bool IsHousePieceTile(int32 x, int32 y)
	{
		if (g_house_piece_tiles.empty())
		{
			return false;
		}

		return g_house_piece_tiles[ToZoneIndex(x, y)] == 1;
	}

	bool IsDoorTile(int32 x, int32 y)
	{
		if (x < 0 || y < 0 || x >= ZONE_TILE_COUNT || y >= ZONE_TILE_COUNT || g_door_tiles.empty())
		{
			return false;
		}

		return g_door_tiles[ToZoneIndex(x, y)] == 1;
	}

	uint8 GetGroundCode(int32 x, int32 y, uint8 tile)
	{
		const bool in_town = IsTownTile(x, y);

		switch (tile)
		{
			case TILE_CLIFF:
			{
				return GROUND_CODE_CLIFF;
			}
			case TILE_WATER:
			{
				return GROUND_CODE_WATER;
			}
			case TILE_ROAD:
			case TILE_WALL:
			case TILE_TOWER:
			{
				return GROUND_CODE_ROAD;
			}
			case TILE_PLAZA:
			{
				return GROUND_CODE_PLAZA;
			}
			case TILE_FIELD:
			{
				if (in_town)
				{
					return GROUND_CODE_FIELD;
				}

				return GROUND_CODE_FIELD_DEAD;
			}
			case TILE_BLIGHT:
			{
				return GROUND_CODE_BLIGHT;
			}
			default:
			{
				if (in_town)
				{
					return GROUND_CODE_YARD;
				}

				return GROUND_CODE_OPEN;
			}
		}
	}

	void BuildOpenDecor(int32 x, int32 y, uint8 tile, float roll, float shade)
	{
		const bool in_town = IsTownTile(x, y);

		if (tile == TILE_FIELD)
		{
			if (roll < 0.6f)
			{
				int32 frame = 1;
				if (in_town)
				{
					frame = 0;
				}

				AddDecor(x, y, SPRITE_CROPS, frame, shade, 0.2f, false);
			}

			return;
		}

		// 성 안 길은 깨끗하게
		if (tile == TILE_PLAZA || tile == TILE_ROAD)
		{
			return;
		}

		if (HasNeighborTile(x, y, TILE_WATER))
		{
			if (roll < 0.5f)
			{
				AddDecor(x, y, SPRITE_REEDS, 0, shade, 0.2f, false);
			}

			return;
		}

		if (HasNeighborTile(x, y, TILE_CLIFF))
		{
			if (roll < 0.12f)
			{
				AddDecor(x, y, SPRITE_PEBBLE, 0, shade, 0.2f, true);
			}

			return;
		}

		if (in_town)
		{
			if (roll < 0.05f)
			{
				AddDecor(x, y, SPRITE_GRASS, 0, shade, 0.3f, false);
			}

			return;
		}

		if (tile == TILE_BLIGHT)
		{
			if (roll < 0.05f)
			{
				AddDecor(x, y, SPRITE_THORN, 0, shade * 0.8f, 0.25f, true);
			}
			else if (roll < 0.08f)
			{
				AddDecor(x, y, SPRITE_BONES, 0, shade, 0.3f, false);
			}

			return;
		}

		if (roll < 0.04f)
		{
			AddDecor(x, y, SPRITE_THORN, 0, shade, 0.25f, true);
		}
		else if (roll < 0.11f)
		{
			AddDecor(x, y, SPRITE_GRASS, 0, shade, 0.3f, false);
		}
		else if (roll < 0.12f)
		{
			AddDecor(x, y, SPRITE_BONES, 0, shade, 0.3f, false);
		}
		else if (roll < 0.14f)
		{
			AddDecor(x, y, SPRITE_PEBBLE, 0, shade, 0.3f, true);
		}
	}

	void BuildTileDecor(int32 x, int32 y)
	{
		const uint8 tile = GetZoneTile(g_zone, x, y);
		const uint32 hash = HashZoneTile(g_zone.seed + 13u, x, y);
		const float roll = HashToUnit(hash);
		const float shade = 0.85f + HashToUnit(HashZoneTile(g_zone.seed + 29u, x, y)) * 0.2f;
		const int32 variant = static_cast<int32>((hash >> 20) & 1u);

		switch (tile)
		{
			case TILE_WALL:
			{
				AddBlock(x, y, SPRITE_BLOCK_WALL, 0, 0.94f + roll * 0.08f);
				return;
			}
			case TILE_TOWER:
			{
				AddBlock(x, y, SPRITE_BLOCK_TOWER, 0, 1.f);
				return;
			}
			case TILE_HOUSE:
			{
				if (IsHousePieceTile(x, y))
				{
					return;
				}

				// 프레임 0 이 +y 면에 문, 1 이 창뿐
				int32 frame = 1;
				if (IsDoorTile(x, y))
				{
					frame = 0;
				}

				AddBlock(x, y, SPRITE_BLOCK_HOUSE, frame, 0.92f + roll * 0.1f);
				return;
			}
			case TILE_RUIN:
			{
				AddBlock(x, y, SPRITE_BLOCK_RUIN, variant, shade);
				return;
			}
			case TILE_TREE:
			{
				AddDecor(x, y, SPRITE_DEAD_TREE, variant, shade, 0.12f, true);
				return;
			}
			case TILE_ROCK:
			{
				AddDecor(x, y, SPRITE_ROCK, 0, shade, 0.15f, true);
				return;
			}
			case TILE_CLIFF:
			case TILE_WATER:
			{
				// 바닥 셰이더가 그린다
				return;
			}
			default:
			{
				BuildOpenDecor(x, y, tile, roll, shade);
				return;
			}
		}
	}

	size_t AddLightAt(const WorldPos& pos, float radius, float r, float g, float b, float strength, int32 sprite)
	{
		Actor light;
		light.m_pos = pos;
		light.m_is_upright = false;
		light.m_light_radius = radius;
		light.m_light_r = r;
		light.m_light_g = g;
		light.m_light_b = b;
		light.m_light_strength = strength;

		const size_t index = g_decor.size();
		g_light_indices.push_back(index);
		g_decor.push_back(light);

		if (sprite != SPRITE_NONE)
		{
			Actor& holder = AddDecorAt(pos, sprite, 0, 1.f, true);
			holder.m_emissive = 1.8f;
		}

		return index;
	}

	WorldPos TileOffset(const ZoneTile& tile, float offset_x, float offset_y)
	{
		return ZoneTileToWorld(tile.x, tile.y, 0.5f + offset_x, 0.5f + offset_y);
	}

	// 노점 하나에 상점 하나. 간판은 노점 오른쪽 뒤에 세워 무엇을 파는지 멀리서도 보이게 한다
	void AddShopStall(const ZoneSite& town, float offset_x, float offset_y, float shade, int32 kind)
	{
		const WorldPos pos = TileOffset(town.tile, offset_x, offset_y);
		AddDecorAt(pos, SPRITE_STALL, 0, shade, true);

		ShopSpot spot;
		spot.kind = kind;
		spot.pos = pos;
		g_shop_spots.push_back(spot);

		const int32 sign = FindSpriteByName(GetShopData(kind).sign);
		if (sign != SPRITE_NONE)
		{
			AddDecorAt(TileOffset(town.tile, offset_x + 0.75f, offset_y - 0.75f), sign, 0, shade, true);
		}
	}

	void BuildTownDecor(const ZoneSite& town)
	{
		// 광장 한가운데 큰 화로. 성채의 심장
		AddLightAt(TileOffset(town.tile, 0.f, 0.f), 9.f, 1.00f, 0.72f, 0.36f, 0.95f, SPRITE_BRAZIER);

		AddShopStall(town, -2.6f, -1.2f, 1.f, SHOP_WEAPON);
		AddShopStall(town, 1.4f, -2.8f, 0.95f, SHOP_ARMOR);
		AddShopStall(town, -1.2f, 2.6f, 0.9f, SHOP_APOTHECARY);

		// 광장 모루. 강화는 여기서만 된다 - 사냥을 나갔다 돌아올 이유
		g_anvil_pos = TileOffset(town.tile, 2.4f, 1.6f);
		g_has_anvil = true;
		AddDecorAt(g_anvil_pos, SPRITE_ANVIL, 0, 1.f, true);
		AddLightAt(TileOffset(town.tile, 2.9f, 2.1f), 4.f, 1.00f, 0.55f, 0.25f, 0.55f, SPRITE_BRAZIER);
	}

	// 개척촌 광장. 화로 하나에 노점 둘, 성채보다 수수하다
	void BuildHamletDecor(const ZoneSite& town)
	{
		AddLightAt(TileOffset(town.tile, 0.f, 0.f), 8.f, 1.00f, 0.70f, 0.34f, 0.85f, SPRITE_BRAZIER);

		AddShopStall(town, -2.6f, -1.2f, 0.95f, SHOP_GENERAL);
		AddShopStall(town, -1.2f, 2.6f, 0.9f, SHOP_APOTHECARY);
	}

	// 게시판은 집 그림판에 함께 굽는다. 그림판이 없으면 깃대로 자리만 표시한다
	void BuildBoardDecor()
	{
		const int32 board_prop = FindHouseProp("board");

		for (const ZoneVillage& village : g_zone.villages)
		{
			const WorldPos pos = ZoneTileToWorld(village.board.x, village.board.y, 0.5f, 0.5f);

			if (board_prop < 0)
			{
				AddDecorAt(pos, SPRITE_BANNER, 0, 1.f, true);
				continue;
			}

			HousePieceActor board;
			board.m_pos = pos;
			board.m_prop = board_prop;
			board.m_casts_shadow = false;
			g_house_pieces.push_back(board);
		}
	}

	void BuildGateDecor(const ZoneSite& gate)
	{
		// 성문 축에 수직인 쪽으로 양옆을 벌린다
		const int32 axis_x = gate.tile.x - g_zone.center.x;
		const int32 axis_y = gate.tile.y - g_zone.center.y;

		float side_x = 0.f;
		float side_y = 0.f;
		if (std::abs(axis_x) > std::abs(axis_y))
		{
			side_y = 2.4f;
		}
		else
		{
			side_x = 2.4f;
		}

		AddLightAt(TileOffset(gate.tile, side_x, side_y), 6.f, 1.00f, 0.66f, 0.30f, 0.60f, SPRITE_BRAZIER);
		AddDecorAt(TileOffset(gate.tile, -side_x, -side_y), SPRITE_BANNER, 0, 1.f, true);
	}

	void BuildOutpostDecor(const ZoneSite& site)
	{
		AddLightAt(TileOffset(site.tile, 0.f, 0.f), 7.f, 0.95f, 0.58f, 0.24f, 0.70f, SPRITE_BRAZIER);
		AddDecorAt(TileOffset(site.tile, -2.2f, 0.8f), SPRITE_TENT, 0, 1.f, true);
		AddDecorAt(TileOffset(site.tile, 0.9f, -2.3f), SPRITE_TENT, 0, 0.92f, true);
		AddDecorAt(TileOffset(site.tile, 1.8f, 1.6f), SPRITE_BANNER, 0, 1.f, true);
	}

	void BuildHideoutDecor(const ZoneSite& site)
	{
		AddLightAt(TileOffset(site.tile, 0.f, 0.f), 7.f, 0.95f, 0.24f, 0.12f, 0.75f, SPRITE_BRAZIER);
		AddDecorAt(TileOffset(site.tile, -2.4f, 1.0f), SPRITE_TENT, 1, 1.f, true);
		AddDecorAt(TileOffset(site.tile, 1.2f, -2.6f), SPRITE_TENT, 1, 0.9f, true);
		AddDecorAt(TileOffset(site.tile, 2.2f, 2.0f), SPRITE_TOTEM, 0, 1.f, true);
		AddDecorAt(TileOffset(site.tile, -1.5f, -1.8f), SPRITE_BONES, 0, 1.f, false);
	}

	void BuildVillageDecor(const ZoneSite& site)
	{
		// 이전 원정대가 남긴 무덤 셋
		AddDecorAt(TileOffset(site.tile, -1.0f, 1.2f), SPRITE_GRAVE, 0, 0.9f, true);
		AddDecorAt(TileOffset(site.tile, 0.2f, 1.6f), SPRITE_GRAVE, 0, 0.85f, true);
		AddDecorAt(TileOffset(site.tile, 1.4f, 1.1f), SPRITE_GRAVE, 0, 0.9f, true);
	}

	void BuildLairDecor(const ZoneSite& site)
	{
		// 해금 전엔 사위어 있다가 성문파쇄자가 깨어날 때 붉게 살아난다
		g_boss_light_indices.push_back(AddLightAt(TileOffset(site.tile, -3.f, 0.f), 7.f, 0.95f, 0.22f, 0.12f, BOSS_LIGHT_DIM, SPRITE_BRAZIER));
		g_boss_light_indices.push_back(AddLightAt(TileOffset(site.tile, 3.f, 0.f), 7.f, 0.95f, 0.22f, 0.12f, BOSS_LIGHT_DIM, SPRITE_BRAZIER));

		AddDecorAt(TileOffset(site.tile, -1.5f, 2.0f), SPRITE_BONES, 0, 1.f, false);
		AddDecorAt(TileOffset(site.tile, 2.0f, -1.6f), SPRITE_BONES, 0, 1.f, false);
		AddDecorAt(TileOffset(site.tile, 0.5f, 2.8f), SPRITE_BONES, 0, 1.f, false);
	}

	void BuildSites()
	{
		for (const ZoneTile& lamp : g_zone.lamps)
		{
			AddLightAt(ZoneTileToWorld(lamp.x, lamp.y, 0.5f, 0.5f), 5.f, 1.00f, 0.78f, 0.45f, 0.45f, SPRITE_LAMP);
		}

		BuildBoardDecor();

		for (const ZoneSite& site : g_zone.sites)
		{
			switch (site.kind)
			{
				case SITE_TOWN:
				{
					if (site.village == ZONE_CITADEL)
					{
						BuildTownDecor(site);
					}
					else
					{
						BuildHamletDecor(site);
					}

					break;
				}
				case SITE_GATE:
				{
					BuildGateDecor(site);
					break;
				}
				case SITE_OUTPOST:
				{
					BuildOutpostDecor(site);
					break;
				}
				case SITE_HIDEOUT:
				{
					BuildHideoutDecor(site);
					break;
				}
				case SITE_VILLAGE:
				{
					BuildVillageDecor(site);
					break;
				}
				case SITE_LAIR:
				{
					BuildLairDecor(site);
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}

	bool FindOpenNear(const ZoneTile& center, uint32 salt, ZoneTile* found)
	{
		for (int32 attempt = 0; attempt < 12; ++attempt)
		{
			const uint32 hash = HashZoneTile(g_zone.seed + salt, center.x + attempt * 17, center.y);
			const int32 x = center.x + static_cast<int32>(HashToUnit(hash) * 5.f) - 2;
			const int32 y = center.y + static_cast<int32>(HashToUnit(hash >> 16) * 5.f) - 2;

			if (IsBlockingTile(GetZoneTile(g_zone, x, y)) == false)
			{
				found->x = x;
				found->y = y;
				return true;
			}
		}

		return false;
	}

	// 띠마다 무리 구성이 다르다. 성문 앞은 느린 타락자뿐이라 첫 전투에서 거리를 배운다
	int32 PickMemberKind(int32 band, float kind_roll, int32 member, int32 count)
	{
		if (band == BAND_INTRO)
		{
			return MONSTER_BLIGHTED;
		}

		if (band == BAND_FIELD)
		{
			if (kind_roll < 0.45f)
			{
				return MONSTER_HOUND;
			}

			if (kind_roll < 0.8f || member < count - 1)
			{
				return MONSTER_BLIGHTED;
			}

			return MONSTER_DEVOURER;
		}

		if (kind_roll < 0.35f)
		{
			return MONSTER_DEVOURER;
		}

		if (kind_roll < 0.7f)
		{
			return MONSTER_HOUND;
		}

		if (member == count - 1)
		{
			return MONSTER_DEVOURER;
		}

		return MONSTER_BLIGHTED;
	}

	int32 GetPackSize(int32 band, float kind_roll, float size_roll)
	{
		if (band == BAND_OUTER && kind_roll < 0.35f)
		{
			// 거식귀는 짝으로 다닌다
			return 2;
		}

		int32 count = 3 + static_cast<int32>(size_roll * 2.f);

		if (kind_roll >= 0.35f && kind_roll < 0.7f && band == BAND_OUTER)
		{
			count += 1;
		}

		if (kind_roll < 0.45f && band == BAND_FIELD)
		{
			count += 1;
		}

		return count;
	}

	float GetEliteChance(int32 band)
	{
		if (band == BAND_OUTER)
		{
			return Balance::ELITE_CHANCE_OUTER;
		}

		if (band == BAND_FIELD)
		{
			return Balance::ELITE_CHANCE_FIELD;
		}

		return 0.f;
	}

	void BuildMonsters()
	{
		for (const ZonePack& pack : g_zone.packs)
		{
			const ZoneTile& center = pack.tile;
			const float kind_roll = HashToUnit(HashZoneTile(g_zone.seed + 31u, center.x, center.y));
			const float size_roll = HashToUnit(HashZoneTile(g_zone.seed + 37u, center.x, center.y));
			const int32 count = GetPackSize(pack.band, kind_roll, size_roll);

			for (int32 member = 0; member < count; ++member)
			{
				ZoneTile tile;
				if (FindOpenNear(center, 41u + static_cast<uint32>(member) * 97u, &tile) == false)
				{
					continue;
				}

				const int32 kind = PickMemberKind(pack.band, kind_roll, member, count);
				const float elite_roll = HashToUnit(HashZoneTile(g_zone.seed + 53u + static_cast<uint32>(member), center.x, center.y));
				const bool is_elite = elite_roll < GetEliteChance(pack.band);

				AddMonster(kind, ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f), pack.band, is_elite);
			}
		}

		SetBossSpawnPoint(ZoneTileToWorld(g_zone.boss.x, g_zone.boss.y, 0.5f, 0.5f));
	}

	void BuildZone(uint32 seed)
	{
		GenerateZone(seed, &g_zone);

		ClearWorld();
		ClearMonsters();
		ClearDrops();
		ClearFloatingTexts();
		ClearFeel();
		g_has_anvil = false;
		g_shop_spots.clear();
		g_decor.clear();
		g_house_pieces.clear();
		g_light_indices.clear();
		g_boss_light_indices.clear();
		g_player_village = -1;
		g_village_banner = -1;
		g_village_banner_timer = 0.f;

		MarkDoorTiles();
		BuildHousePieces();

		std::vector<uint8> codes(static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT), GROUND_CODE_OPEN);

		// 빈 칸도 전부 넣어야 청크 표가 생긴다. 표가 없는 청크는 막힌 걸로 친다
		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const uint8 tile = GetZoneTile(g_zone, x, y);
				SetTileBlocked(ZoneTileToWorld(x, y, 0.5f, 0.5f), IsBlockingTile(tile));

				codes[ToZoneIndex(x, y)] = GetGroundCode(x, y, tile);

				BuildTileDecor(x, y);
			}
		}

		const int32 origin_tile = -ZONE_CHUNK_RADIUS * CHUNK_TILE_COUNT;
		SetGroundZone(codes, ZONE_TILE_COUNT, origin_tile, origin_tile);

		BuildSites();
		BuildMonsters();

		const WorldPos spawn = ZoneTileToWorld(g_zone.spawn.x, g_zone.spawn.y, 0.5f, 0.5f);

		if (g_player_ready)
		{
			PlacePlayer(spawn);
		}
		else
		{
			InitPlayer(spawn);
			g_player_ready = true;
		}

		// 마을 살림과 게시판이 먼저 서야 주민이 집을 받는다
		BuildVillages(g_zone);
		BuildBoards();
		BuildNpcs(g_zone);
		BuildMinimap(g_zone);
		BuildMonitorZone(g_zone);

		// 사냥터를 몬스터 자리에서 고르니 몬스터 · 마을 뒤
		BuildQuests(g_zone);

		std::cout << "[ Scene - BuildZone ] : seed " << seed << " packs " << g_zone.packs.size() << " decor " << g_decor.size() << "\n";
	}
}

namespace
{
	void FillGroundLight(GroundLight* target, const WorldPos& camera, const WorldPos& pos, float radius, float r, float g, float b, float strength)
	{
		target->tile_offset_x = GetTileDistanceX(camera, pos);
		target->tile_offset_y = GetTileDistanceY(camera, pos);
		target->radius = radius;
		target->r = r;
		target->g = g;
		target->b = b;
		target->strength = strength;
	}

	// 바닥 셰이더가 광원 여덟 개만 받는다. 카메라에 가까운 것부터 채운다
	GroundFrame MakeGroundFrame(const WorldPos& camera)
	{
		GroundFrame frame;

		frame.camera_tile_x = GetGlobalTileX(camera);
		frame.camera_tile_y = GetGlobalTileY(camera);
		frame.camera_frac_x = camera.local_x - std::floor(camera.local_x);
		frame.camera_frac_y = camera.local_y - std::floor(camera.local_y);
		frame.show_chunk_grid = g_show_chunk_grid;

		if (g_use_lighting == false)
		{
			return frame;
		}

		// 등불 하나 든 정도. 가장자리 어둠 속에서 발밑만 살짝 밝힌다
		FillGroundLight(&frame.lights[0], camera, GetPlayer().m_pos, 6.f, 0.62f, 0.58f, 0.50f, 0.24f);
		frame.light_count = 1;

		g_nearby_lights = g_light_indices;
		std::sort(g_nearby_lights.begin(), g_nearby_lights.end(),
			[&camera](size_t left, size_t right)
			{
				return GetDistanceSq(g_decor[left].m_pos, camera) < GetDistanceSq(g_decor[right].m_pos, camera);
			});

		for (size_t index : g_nearby_lights)
		{
			if (frame.light_count >= GROUND_MAX_LIGHT_COUNT)
			{
				break;
			}

			const Actor& light = g_decor[index];
			GroundLight& target = frame.lights[static_cast<size_t>(frame.light_count)];
			FillGroundLight(&target, camera, light.m_pos, light.m_light_radius, light.m_light_r, light.m_light_g, light.m_light_b, light.m_light_strength);

			frame.light_count += 1;
		}

		return frame;
	}

	// G 키 디버그 - 막힌 칸을 붉게. 바위와 나무 칸은 바닥색으로 안 드러나서 따로 그린다
	void DrawBlockedDebug(Renderer* renderer, const WorldPos& camera)
	{
		int32 camera_x = 0;
		int32 camera_y = 0;
		WorldToZoneTile(camera, &camera_x, &camera_y);

		for (int32 y = camera_y - DEBUG_VIEW_RADIUS; y <= camera_y + DEBUG_VIEW_RADIUS; ++y)
		{
			for (int32 x = camera_x - DEBUG_VIEW_RADIUS; x <= camera_x + DEBUG_VIEW_RADIUS; ++x)
			{
				if (IsBlockingTile(GetZoneTile(g_zone, x, y)) == false)
				{
					continue;
				}

				const ScreenPos screen = WorldToScreen(ZoneTileToWorld(x, y, 0.5f, 0.5f), camera);
				if (IsOffScreen(screen, 40.f))
				{
					continue;
				}

				renderer->DrawSolidDiamond(screen.x, screen.y, 1.f, 0.60f, 0.05f, 0.05f, 0.45f);
			}
		}
	}

	void DrawAimMarker(Renderer* renderer, const WorldPos& camera)
	{
		if (IsPlayerDead())
		{
			return;
		}

		const Player& player = GetPlayer();

		WorldPos marker = player.m_pos;
		MoveWorldPos(&marker, player.m_aim_x * 0.9f, player.m_aim_y * 0.9f);

		const ScreenPos screen = WorldToScreen(marker, camera);
		renderer->DrawSolidDiamond(screen.x, screen.y, 0.18f, 0.90f, 0.80f, 0.50f, 0.35f);
	}

	struct ScreenBox
	{
		float left = 0.f;
		float right = 0.f;
		float bottom = 0.f;
		float top = 0.f;
	};

	ScreenBox GetActorBox(const Actor& actor, const WorldPos& camera)
	{
		const ScreenPos foot = WorldToScreen(actor.m_pos, camera);
		const float unit = static_cast<float>(PIXEL_SCALE);
		const float half_width = actor.GetArtWidth() * unit * 0.5f;

		ScreenBox box;
		box.left = foot.x - half_width;
		box.right = foot.x + half_width;
		box.bottom = foot.y + actor.m_foot_offset_y;
		box.top = box.bottom + actor.GetArtHeight() * unit;

		return box;
	}

	bool IsOverlapping(const ScreenBox& left, const ScreenBox& right)
	{
		if (left.right < right.left || right.right < left.left)
		{
			return false;
		}

		return left.top >= right.bottom && right.top >= left.bottom;
	}

	void AddDecorActor(const Actor& actor, const WorldPos& camera, const ScreenBox& player_box, float player_depth)
	{
		if (actor.m_is_upright == false)
		{
			return;
		}

		// 망루와 죽은 나무는 발밑이 화면 밖이어도 윗부분이 걸치므로 여유를 크게 준다
		if (IsOffScreen(WorldToScreen(actor.m_pos, camera), 140.f))
		{
			return;
		}

		AddSceneActor(&actor);

		// 플레이어 앞에 서서 플레이어를 덮는 벽이 있으면 투시로 다시 그린다 ( 디아블로식 )
		if (g_player_hidden == false && IsOccluder(actor) && GetDepthKey(actor.m_pos, camera) > player_depth)
		{
			if (IsOverlapping(player_box, GetActorBox(actor, camera)))
			{
				g_player_hidden = true;
			}
		}
	}

	void AddDecorActors(const WorldPos& camera)
	{
		const Actor& player = GetPlayer();
		const ScreenBox player_box = GetActorBox(player, camera);
		const float player_depth = GetDepthKey(player.m_pos, camera);

		g_player_hidden = false;

		for (const Actor& actor : g_decor)
		{
			AddDecorActor(actor, camera, player_box, player_depth);
		}

		for (const HousePieceActor& piece : g_house_pieces)
		{
			AddDecorActor(piece, camera, player_box, player_depth);
		}
	}

	void DrawPlayerSilhouette(Renderer* renderer, const WorldPos& camera)
	{
		if (g_player_hidden == false || IsPlayerDead())
		{
			return;
		}

		const Actor& body = GetPlayer();
		const ScreenPos screen = WorldToScreen(body.m_pos, camera);

		DrawSheetFrame(renderer, body.m_sheet, body.m_frame, screen.x, screen.y, 0.45f, 1.1f, 1.0f, 0.45f, 0.f);
	}

	// 깨어난 보스가 어디 있는지 모르면 넓은 구역에서 길을 잃는다
	void DrawBossMarker(Renderer* renderer, const WorldPos& camera)
	{
		WorldPos boss_pos;

		if (GetBossPos(&boss_pos) == false)
		{
			return;
		}

		const ScreenPos screen = WorldToScreen(boss_pos, camera);

		// 화면에 이미 보이면 표식이 오히려 방해다
		if (IsOffScreen(screen, -BOSS_MARKER_MARGIN) == false)
		{
			return;
		}

		const float limit_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f - BOSS_MARKER_MARGIN;
		const float limit_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - BOSS_MARKER_MARGIN;

		// 화면 한가운데에서 보스 쪽으로 그은 선이 테두리와 만나는 자리
		float scale = 1.f;
		const float span_x = std::fabs(screen.x) * limit_y;
		const float span_y = std::fabs(screen.y) * limit_x;

		if (span_x > span_y)
		{
			scale = limit_x / std::fabs(screen.x);
		}
		else
		{
			scale = limit_y / std::fabs(screen.y);
		}

		const float marker_x = screen.x * scale;
		const float marker_y = screen.y * scale;

		renderer->DrawSolidCircle(marker_x, marker_y, BOSS_MARKER_RADIUS + 2.f, 0.10f, 0.02f, 0.02f, 0.75f);
		renderer->DrawSolidCircle(marker_x, marker_y, BOSS_MARKER_RADIUS, 0.95f, 0.25f, 0.15f, 1.f);

		// 몇 칸 남았는지 있어야 방향만 보고 헤매지 않는다
		const float tile_x = GetTileDistanceX(camera, boss_pos);
		const float tile_y = GetTileDistanceY(camera, boss_pos);
		const int32 tiles = static_cast<int32>(std::sqrt(tile_x * tile_x + tile_y * tile_y));

		DrawStringShadow(renderer, std::to_wstring(tiles), marker_x, marker_y - 18.f, TEXT_ALIGN_CENTER, 1.f, 0.72f, 0.55f, 1.f);
	}

	void LightBossBraziers()
	{
		for (size_t index : g_boss_light_indices)
		{
			g_decor[index].m_light_strength = BOSS_LIGHT_LIT;
		}
	}

	// 1 레벨이 걸어가서 두들겨 맞고 끝나면 보스가 목표가 아니라 지형지물이 된다
	void UpdateBossUnlock()
	{
		if (IsBossSummoned())
		{
			return;
		}

		if (GetPlayer().m_level < Balance::BOSS_UNLOCK_LEVEL)
		{
			return;
		}

		SummonBoss();
		LightBossBraziers();

		const WorldPos lair = ZoneTileToWorld(g_zone.boss.x, g_zone.boss.y, 0.5f, 0.5f);
		PostNewsAt(lair, NEWS_BOSS, std::wstring(GetVillageName(FindNearestVillage(lair))) + L" 너머 둥지에서 " + GetBossName() + L"가 깨어났다");

		AddDamageText(GetPlayer().m_pos, 150.f, std::wstring(GetBossName()) + L"가 깨어났다", DAMAGE_STYLE_BANNER_BAD);
		AddShake(0.4f);
		PlaySfx(SFX_DANGER, 1.f);
	}

	void DrawLightGlows(Renderer* renderer, const WorldPos& camera)
	{
		for (size_t index : g_light_indices)
		{
			const Actor& light = g_decor[index];
			const ScreenPos screen = WorldToScreen(light.m_pos, camera);

			if (IsOffScreen(screen, 120.f))
			{
				continue;
			}

			// 코어 하나만 1 을 훨씬 넘기게 찍고 번짐은 블룸에게 맡긴다. 불꽃 높이에 둔다
			const float core = light.m_light_strength * LIGHT_CORE_GAIN;
			const float flame_y = screen.y + 7.f * static_cast<float>(PIXEL_SCALE);

			renderer->DrawSolidCircle(screen.x, flame_y, 5.f, light.m_light_r * core, light.m_light_g * core, light.m_light_b * core, 1.f);
		}
	}

	float DistanceToAnvil()
	{
		if (g_has_anvil == false)
		{
			return 1.0e9f;
		}

		const float delta_x = GetTileDistanceX(GetPlayer().m_pos, g_anvil_pos);
		const float delta_y = GetTileDistanceY(GetPlayer().m_pos, g_anvil_pos);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	// 모루 앞이면 머리 위에 안내를 띄운다
	void DrawAnvilPrompt(Renderer* renderer, const WorldPos& camera)
	{
		if (g_has_anvil == false || IsPlayerDead())
		{
			return;
		}

		const ScreenPos screen = WorldToScreen(g_anvil_pos, camera);
		const bool near = DistanceToAnvil() <= ANVIL_RANGE;

		if (near)
		{
			DrawStringShadow(renderer, L"F  강화", screen.x, screen.y + 58.f, TEXT_ALIGN_CENTER, 1.f, 0.85f, 0.45f, 1.f);
			return;
		}

		// 멀리서도 어디 있는지는 보이게
		if (DistanceToAnvil() < 10.f)
		{
			DrawStringShadow(renderer, L"모루", screen.x, screen.y + 58.f, TEXT_ALIGN_CENTER, 0.75f, 0.65f, 0.5f, 0.8f);
		}
	}

	int32 FindShopSpotInRange(float range)
	{
		const WorldPos& player = GetPlayer().m_pos;
		int32 best = -1;
		float best_distance = range * range;

		for (size_t index = 0; index < g_shop_spots.size(); ++index)
		{
			const float distance = GetDistanceSq(player, g_shop_spots[index].pos);
			if (distance <= best_distance)
			{
				best = static_cast<int32>(index);
				best_distance = distance;
			}
		}

		return best;
	}

	void DrawShopPrompts(Renderer* renderer, const WorldPos& camera)
	{
		if (IsPlayerDead())
		{
			return;
		}

		const int32 near_spot = FindShopSpotInRange(SHOP_RANGE);

		for (size_t index = 0; index < g_shop_spots.size(); ++index)
		{
			const ShopSpot& spot = g_shop_spots[index];
			if (GetDistanceSq(GetPlayer().m_pos, spot.pos) > 100.f)
			{
				continue;
			}

			const ScreenPos screen = WorldToScreen(spot.pos, camera);
			const std::wstring& name = GetShopData(spot.kind).name;

			if (static_cast<int32>(index) == near_spot)
			{
				DrawStringShadow(renderer, L"F  " + name, screen.x, screen.y + SHOP_LABEL_LIFT, TEXT_ALIGN_CENTER, 1.f, 0.85f, 0.45f, 1.f);
				continue;
			}

			DrawStringShadow(renderer, name, screen.x, screen.y + SHOP_LABEL_LIFT, TEXT_ALIGN_CENTER, 0.75f, 0.65f, 0.5f, 0.8f);
		}
	}

	int32 FindBoardInRange(float range)
	{
		const WorldPos& player = GetPlayer().m_pos;

		for (int32 village = 0; village < GetVillageCount(); ++village)
		{
			const WorldPos board = GetBoardPos(village);
			const float delta_x = GetTileDistanceX(player, board);
			const float delta_y = GetTileDistanceY(player, board);

			if (delta_x * delta_x + delta_y * delta_y <= range * range)
			{
				return village;
			}
		}

		return -1;
	}

	void DrawBoardPrompt(Renderer* renderer, const WorldPos& camera)
	{
		if (IsPlayerDead())
		{
			return;
		}

		const int32 near_board = FindBoardInRange(BOARD_RANGE);
		const int32 seen_board = FindBoardInRange(10.f);

		if (seen_board < 0)
		{
			return;
		}

		const ScreenPos screen = WorldToScreen(GetBoardPos(seen_board), camera);

		if (near_board >= 0)
		{
			DrawStringShadow(renderer, L"F  게시판", screen.x, screen.y + BOARD_LABEL_LIFT, TEXT_ALIGN_CENTER, 1.f, 0.85f, 0.45f, 1.f);
			return;
		}

		DrawStringShadow(renderer, std::wstring(GetVillageName(seen_board)) + L" 게시판", screen.x, screen.y + BOARD_LABEL_LIFT, TEXT_ALIGN_CENTER, 0.75f, 0.65f, 0.5f, 0.8f);
	}

	// 마을 경계를 넘는 순간 이름을 크게 띄운다. 일곱 마을이 다 다른 곳이라는 걸 먼저 알린다
	void UpdateVillageBanner(float real_seconds)
	{
		g_village_banner_timer -= real_seconds;

		const int32 village = GetVillageAt(GetPlayer().m_pos);
		if (village == g_player_village)
		{
			return;
		}

		g_player_village = village;

		if (village >= 0)
		{
			g_village_banner = village;
			g_village_banner_timer = VILLAGE_BANNER_SEC;
		}
	}

	void DrawVillageBanner(Renderer* renderer)
	{
		if (g_village_banner < 0 || g_village_banner_timer <= 0.f)
		{
			return;
		}

		// 처음 0.4 초 차오르고 마지막 0.8 초 사위어 간다
		float alpha = 1.f;
		const float elapsed = VILLAGE_BANNER_SEC - g_village_banner_timer;
		if (elapsed < 0.4f)
		{
			alpha = elapsed / 0.4f;
		}
		else if (g_village_banner_timer < 0.8f)
		{
			alpha = g_village_banner_timer / 0.8f;
		}

		const float top = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - 96.f;
		const std::wstring detail = L"주민 " + std::to_wstring(CountResidents(g_village_banner)) + L"   빈집 " + std::to_wstring(CountFreeHouses(g_village_banner)) + L"   금고 " + std::to_wstring(GetVillageTreasury(g_village_banner));

		// 밝은 광장 돌바닥 위에서도 숫자 줄이 읽히게 어두운 띠를 깐다
		renderer->DrawSolidBox(0.f, top - 36.f, 460.f, 92.f, 0.02f, 0.018f, 0.015f, 0.55f * alpha);
		DrawStringOutline(renderer, GetVillageName(g_village_banner), 0.f, top, TEXT_ALIGN_CENTER, 2, 0.96f, 0.86f, 0.62f, alpha);
		DrawStringShadow(renderer, detail, 0.f, top - 44.f, TEXT_ALIGN_CENTER, 0.78f, 0.74f, 0.66f, alpha);
	}

	void UpdateInteractHold(float real_seconds)
	{
		if (g_interact_held == false)
		{
			return;
		}

		g_interact_timer -= real_seconds;
		if (g_interact_timer > 0.f)
		{
			return;
		}

		g_interact_timer = PICKUP_REPEAT_SEC;
		PickupNearestDrop();
	}
}

void InitScene()
{
	BuildZone(MakeRandomSeed());
}

void RegenerateZone()
{
	BuildZone(MakeRandomSeed());
}

void UpdateScene(float real_seconds)
{
	// 연출은 실제 시간, 판정은 히트스톱이 곱해진 시간. 그래야 세상이 멈춰도 불꽃은 튄다
	UpdateFeel(real_seconds);
	const float world_seconds = real_seconds * GetWorldTimeScale();

	g_fog_scroll += world_seconds * 60.f;

	// 커서 아래 바닥이 조준점. 카메라가 NPC 를 따라가도 커서 기준이 맞게 카메라로 되돌린다
	SetPlayerAim(GetCursorWorldPos());

	UpdateBossUnlock();
	UpdateInteractHold(real_seconds);
	UpdateVillageBanner(real_seconds);

	UpdatePlayer(world_seconds, real_seconds);
	UpdateMonsters(world_seconds);
	UpdateNpcs(world_seconds);
	UpdateDrops(world_seconds);
	UpdateQuests(real_seconds);
	UpdateFloatingTexts(real_seconds);

	float flash_r = 0.f;
	float flash_g = 0.f;
	float flash_b = 0.f;
	float flash_strength = 0.f;
	GetScreenFlash(&flash_r, &flash_g, &flash_b, &flash_strength);
	SetPostEffects(GetZoomPunch(), flash_r, flash_g, flash_b, flash_strength);
}

void DrawScene(Renderer* renderer)
{
	const WorldPos camera = GetCameraPos();

	BeginProfileStage(PROFILE_STAGE_GROUND);
	const GroundFrame ground_frame = MakeGroundFrame(camera);
	EndProfileStage();

	DrawGroundLayer(ground_frame);

	BeginProfileStage(PROFILE_STAGE_SCENE);

	if (g_show_chunk_grid)
	{
		DrawBlockedDebug(renderer, camera);
	}

	DrawNpcGroundMarks(renderer, camera);
	DrawMonsterTelegraphs(renderer, camera);
	DrawFeelGround(renderer, camera);
	DrawAimMarker(renderer, camera);
	DrawQuestGround(renderer, camera);

	BeginSceneGraph();
	AddDecorActors(camera);
	CollectMonsterActors();
	CollectDropActors();
	CollectNpcActors();
	AddSceneActor(&GetPlayer());

	DrawSceneGraph(renderer, camera);

	DrawFeelWorld(renderer, camera);
	DrawPlayerSilhouette(renderer, camera);
	DrawNpcArrows(renderer, camera);
	DrawLightGlows(renderer, camera);

	EndProfileStage();

	DrawFogLayer(g_fog_scroll);
}

void DrawSceneOverlay(Renderer* renderer)
{
	const WorldPos camera = GetCameraPos();

	BeginProfileStage(PROFILE_STAGE_OVERLAY);

	DrawMonsterBars(renderer, camera);
	DrawNpcOverlay(renderer, camera);
	DrawDropLabels(renderer, camera);
	DrawAnvilPrompt(renderer, camera);
	DrawBoardPrompt(renderer, camera);
	DrawShopPrompts(renderer, camera);
	DrawBossMarker(renderer, camera);
	DrawQuestOverlay(renderer, camera);
	DrawFloatingTexts(renderer, camera);
	DrawVillageBanner(renderer);

	EndProfileStage();
}

WorldPos GetCameraPos()
{
	WorldPos camera = GetPlayer().m_pos;

	WorldPos follow;
	if (GetFollowPos(&follow))
	{
		camera = follow;
	}

	// 흔들림은 카메라만 민다. 판정 좌표는 그대로다
	const ScreenPos shake = GetShakeOffset();
	if (std::fabs(shake.x) + std::fabs(shake.y) > 0.01f)
	{
		camera = ScreenToWorld(shake, camera);
	}

	return camera;
}

WorldPos GetCursorWorldPos()
{
	return ScreenToWorld(GetMouseScreenPos(), GetCameraPos());
}

void Interact()
{
	if (PickupNearestDrop())
	{
		g_interact_timer = PICKUP_REPEAT_SEC;
		return;
	}

	if (DistanceToAnvil() <= ANVIL_RANGE)
	{
		ToggleEnhanceWindow();
		return;
	}

	const int32 spot = FindShopSpotInRange(SHOP_RANGE);
	if (spot >= 0)
	{
		ToggleShopWindow(g_shop_spots[static_cast<size_t>(spot)].kind, spot);
		return;
	}

	const int32 board = FindBoardInRange(BOARD_RANGE);
	if (board >= 0)
	{
		// 끝낸 의뢰는 게시판 앞에 서는 순간 넘긴다. 보고 단추를 따로 누르게 하면 한 번 더 헤맨다
		ReportQuestsAt(board);
		ToggleBoardWindow(board);
	}
}

bool IsPlayerNearBoard(int32 village, float range)
{
	if (village < 0 || village >= GetVillageCount())
	{
		return false;
	}

	const WorldPos board = GetBoardPos(village);
	const float delta_x = GetTileDistanceX(GetPlayer().m_pos, board);
	const float delta_y = GetTileDistanceY(GetPlayer().m_pos, board);

	return delta_x * delta_x + delta_y * delta_y <= range * range;
}

void SetInteractHeld(bool held)
{
	g_interact_held = held;

	if (held)
	{
		g_interact_timer = PICKUP_REPEAT_SEC * 2.f;
	}
}

bool IsPlayerNearAnvil()
{
	return DistanceToAnvil() <= ANVIL_RANGE;
}

bool IsPlayerNearShop(int32 spot, float range)
{
	if (spot < 0 || spot >= static_cast<int32>(g_shop_spots.size()))
	{
		return false;
	}

	return GetDistanceSq(GetPlayer().m_pos, g_shop_spots[static_cast<size_t>(spot)].pos) <= range * range;
}

void SelectNpcAtCursor()
{
	const WorldPos cursor = ScreenToWorld(GetMouseScreenPos(), GetCameraPos());

	SelectNpc(PickNpcAt(cursor, 1.5f));
}

void ToggleChunkGrid()
{
	g_show_chunk_grid = !g_show_chunk_grid;
}

bool IsChunkGridShown()
{
	return g_show_chunk_grid;
}

void ToggleLighting()
{
	g_use_lighting = !g_use_lighting;
}
