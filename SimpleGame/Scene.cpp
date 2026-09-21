#include "stdafx.h"
#include "Scene.h"
#include "Actor.h"
#include "Balance.h"
#include "Drop.h"
#include "FloatingText.h"
#include "Fog.h"
#include "GameConfig.h"
#include "Ground.h"
#include "Input.h"
#include "Monster.h"
#include "Player.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "World.h"
#include "ZoneGen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
	const float LIGHT_CORE_GAIN = 6.f;

	constexpr int32 DEBUG_VIEW_RADIUS = 24;

	ZoneLayout g_zone;
	bool g_player_ready = false;

	std::vector<Actor> g_decor;
	std::vector<size_t> g_light_indices;
	std::vector<size_t> g_nearby_lights;

	bool g_show_chunk_grid = false;
	bool g_use_lighting = true;
	float g_fog_scroll = 0.f;

	uint32 MakeRandomSeed()
	{
		const uint64 ticks = static_cast<uint64>(std::chrono::steady_clock::now().time_since_epoch().count());
		return static_cast<uint32>(ticks ^ (ticks >> 32));
	}

	int32 GetGlobalTileX(const WorldPos& pos)
	{
		return pos.chunk_x * CHUNK_TILE_COUNT + static_cast<int32>(pos.local_x);
	}

	int32 GetGlobalTileY(const WorldPos& pos)
	{
		return pos.chunk_y * CHUNK_TILE_COUNT + static_cast<int32>(pos.local_y);
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

	void AddDecor(int32 tile_x, int32 tile_y, int32 sprite, float shade, float jitter, bool casts_shadow)
	{
		// 칸 한가운데에 줄 세우면 격자가 읽힌다. 좌우 반전도 섞어 같은 그림 반복을 가린다
		const uint32 hash = HashZoneTile(g_zone.seed + 7u, tile_x, tile_y);

		Actor object;
		object.m_pos = ZoneTileToWorld(tile_x, tile_y, 0.5f + Jitter(hash, jitter), 0.5f + Jitter(hash >> 8, jitter));
		object.m_sprite = sprite;
		object.m_shade = shade;
		object.m_flip = HashToUnit(hash >> 16) > 0.5f;
		object.m_casts_shadow = casts_shadow;

		g_decor.push_back(object);
	}

	void BuildTileDecor(int32 x, int32 y)
	{
		const uint8 tile = GetZoneTile(g_zone, x, y);
		const float roll = HashToUnit(HashZoneTile(g_zone.seed + 13u, x, y));
		const float shade = 0.85f + HashToUnit(HashZoneTile(g_zone.seed + 29u, x, y)) * 0.25f;

		if (tile == TILE_PALM)
		{
			AddDecor(x, y, SPRITE_PALM, shade, 0.12f, true);
			return;
		}

		if (tile == TILE_ROCK)
		{
			AddDecor(x, y, SPRITE_ROCK, shade, 0.15f, true);
			return;
		}

		// 낭떠러지와 물은 바닥 셰이더가 그린다
		if (IsBlockingTile(tile))
		{
			return;
		}

		if (HasNeighborTile(x, y, TILE_WATER))
		{
			if (roll < 0.5f)
			{
				AddDecor(x, y, SPRITE_REEDS, shade, 0.2f, false);
			}

			return;
		}

		if (HasNeighborTile(x, y, TILE_CLIFF))
		{
			if (roll < 0.14f)
			{
				AddDecor(x, y, SPRITE_PEBBLE, shade, 0.2f, true);
			}

			return;
		}

		if (roll < 0.045f)
		{
			AddDecor(x, y, SPRITE_BUSH, shade, 0.25f, true);
		}
		else if (roll < 0.115f)
		{
			AddDecor(x, y, SPRITE_GRASS, shade, 0.3f, false);
		}
		else if (roll < 0.125f)
		{
			AddDecor(x, y, SPRITE_BONES, shade, 0.3f, false);
		}
		else if (roll < 0.145f)
		{
			AddDecor(x, y, SPRITE_PEBBLE, shade, 0.3f, true);
		}
	}

	void AddLight(const ZoneTile& tile, float radius, float r, float g, float b, float strength)
	{
		Actor light;
		light.m_pos = ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f);
		light.m_is_upright = false;
		light.m_light_radius = radius;
		light.m_light_r = r;
		light.m_light_g = g;
		light.m_light_b = b;
		light.m_light_strength = strength;

		g_light_indices.push_back(g_decor.size());
		g_decor.push_back(light);

		Actor brazier;
		brazier.m_pos = light.m_pos;
		brazier.m_sprite = SPRITE_BRAZIER;
		brazier.m_emissive = 1.8f;

		g_decor.push_back(brazier);
	}

	void BuildLights()
	{
		for (size_t index = 0; index < g_zone.lights.size(); ++index)
		{
			const ZoneTile& tile = g_zone.lights[index];

			// 스폰 모닥불
			if (index == 0)
			{
				AddLight(tile, 10.f, 1.00f, 0.74f, 0.34f, 1.10f);
				continue;
			}

			// 보스 화로. 붉은 빛이 멀리서도 목적지를 알린다
			if (index <= 2)
			{
				AddLight(tile, 7.f, 0.95f, 0.22f, 0.12f, 0.90f);
				continue;
			}

			AddLight(tile, 6.f, 0.95f, 0.55f, 0.22f, 0.60f);
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

	void BuildMonsters()
	{
		for (const ZoneTile& center : g_zone.packs)
		{
			const float kind_roll = HashToUnit(HashZoneTile(g_zone.seed + 31u, center.x, center.y));
			const int32 count = 3 + static_cast<int32>(HashToUnit(HashZoneTile(g_zone.seed + 37u, center.x, center.y)) * 2.f);

			for (int32 member = 0; member < count; ++member)
			{
				ZoneTile tile;
				if (FindOpenNear(center, 41u + static_cast<uint32>(member) * 97u, &tile) == false)
				{
					continue;
				}

				const WorldPos pos = ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f);

				// 좀비 무리 / 해골 무리 / 섞인 무리
				if (kind_roll < 0.4f)
				{
					AddZombie(pos);
				}
				else if (kind_roll < 0.75f)
				{
					AddSkeleton(pos);
				}
				else if (member % 2 == 0)
				{
					AddZombie(pos);
				}
				else
				{
					AddSkeleton(pos);
				}
			}
		}

		AddBoss(ZoneTileToWorld(g_zone.boss.x, g_zone.boss.y, 0.5f, 0.5f));
	}

	void BuildZone(uint32 seed)
	{
		GenerateZone(seed, &g_zone);

		ClearWorld();
		ClearMonsters();
		ClearDrops();
		ClearFloatingTexts();
		g_decor.clear();
		g_light_indices.clear();

		std::vector<uint8> codes(static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT), GROUND_CODE_OPEN);

		// 빈 칸도 전부 넣어야 청크 표가 생긴다. 표가 없는 청크는 막힌 걸로 친다
		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const uint8 tile = GetZoneTile(g_zone, x, y);
				SetTileBlocked(ZoneTileToWorld(x, y, 0.5f, 0.5f), IsBlockingTile(tile));

				// 바위와 야자수는 막히지만 바닥은 모래라 그림으로만 세운다
				if (tile == TILE_CLIFF)
				{
					codes[ToZoneIndex(x, y)] = GROUND_CODE_CLIFF;
				}
				else if (tile == TILE_WATER)
				{
					codes[ToZoneIndex(x, y)] = GROUND_CODE_WATER;
				}

				BuildTileDecor(x, y);
			}
		}

		const int32 origin_tile = -ZONE_CHUNK_RADIUS * CHUNK_TILE_COUNT;
		SetGroundZone(codes, ZONE_TILE_COUNT, origin_tile, origin_tile);

		BuildLights();
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

		// 해질녘이라 은은하게. 가장자리 어둠 속에서 발밑만 살짝 밝힌다
		FillGroundLight(&frame.lights[0], camera, GetPlayer().body.m_pos, 6.f, 0.70f, 0.58f, 0.40f, 0.22f);
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

	// G 키 디버그 - 막힌 칸을 붉게. 바위와 야자수 칸은 바닥색으로 안 드러나서 따로 그린다
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

		const PlayerState& player = GetPlayer();

		WorldPos marker = player.body.m_pos;
		MoveWorldPos(&marker, player.facing_x * 0.9f, player.facing_y * 0.9f);

		const ScreenPos screen = WorldToScreen(marker, camera);
		renderer->DrawSolidDiamond(screen.x, screen.y, 0.22f, 0.90f, 0.80f, 0.50f, 0.45f);
	}

	void AddDecorActors(const WorldPos& camera)
	{
		for (const Actor& actor : g_decor)
		{
			if (actor.m_is_upright == false)
			{
				continue;
			}

			// 키 큰 야자수는 발밑이 화면 밖이어도 잎이 걸치므로 여유를 크게 준다
			if (IsOffScreen(WorldToScreen(actor.m_pos, camera), 120.f))
			{
				continue;
			}

			AddSceneActor(&actor);
		}
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

			// 코어 하나만 1 을 훨씬 넘기게 찍고 번짐은 블룸에게 맡긴다. 화로 불꽃 높이에 둔다
			const float core = light.m_light_strength * LIGHT_CORE_GAIN;
			const float flame_y = screen.y + 7.f * static_cast<float>(PIXEL_SCALE);

			renderer->DrawSolidCircle(screen.x, flame_y, 5.f, light.m_light_r * core, light.m_light_g * core, light.m_light_b * core, 1.f);
		}
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

void UpdateScene(float delta_seconds)
{
	g_fog_scroll += delta_seconds * 60.f;

	if (IsPlayerDead() == false)
	{
		const PlayerState& player = GetPlayer();

		// 카메라가 플레이어라 커서 아래 바닥이 곧 조준점이다
		const WorldPos aim = ScreenToWorld(GetMouseScreenPos(), player.body.m_pos);
		AimPlayer(GetTileDistanceX(player.body.m_pos, aim), GetTileDistanceY(player.body.m_pos, aim));

		// 쿨이 돌았을 때만 꺼낸다. 쿨 중 클릭은 버리지 않고 대기시킨다
		if (player.swing_cooldown <= 0.f && ConsumeAttackRequest())
		{
			if (TryStartSwing())
			{
				HitMonstersInFan(player.body.m_pos, player.facing_x, player.facing_y, Balance::SWING_RANGE, Balance::SWING_HALF_COS, GetPlayerAttack());
			}
		}
	}

	UpdatePlayer(delta_seconds);
	UpdateMonsters(delta_seconds);
	UpdateDrops(delta_seconds);
	UpdateFloatingTexts(delta_seconds);
}

void DrawScene(Renderer* renderer)
{
	const WorldPos camera = GetCameraPos();

	DrawGroundLayer(MakeGroundFrame(camera));

	if (g_show_chunk_grid)
	{
		DrawBlockedDebug(renderer, camera);
	}

	DrawMonsterTelegraphs(renderer, camera);
	DrawAimMarker(renderer, camera);

	BeginSceneGraph();
	AddDecorActors(camera);
	CollectMonsterActors();
	CollectDropActors();

	if (IsPlayerDead() == false)
	{
		AddSceneActor(&GetPlayer().body);
	}

	DrawSceneGraph(renderer, camera);

	DrawLightGlows(renderer, camera);
	DrawFogLayer(g_fog_scroll);
}

void DrawSceneOverlay(Renderer* renderer)
{
	const WorldPos camera = GetCameraPos();

	DrawMonsterBars(renderer, camera);
	DrawDropLabels(renderer, camera);
	DrawFloatingTexts(renderer, camera);
}

WorldPos GetCameraPos()
{
	return GetPlayer().body.m_pos;
}

void MovePlayerByScreenAxis(float screen_right, float screen_up, float delta_seconds)
{
	const float length = std::sqrt(screen_right * screen_right + screen_up * screen_up);
	if (length < 0.0001f)
	{
		return;
	}

	// 화면에서 먼저 길이를 1 로 맞춰야 대각선이 더 빠르지 않다
	const float dir_right = screen_right / length;
	const float dir_up = screen_up / length;

	// 화면 오른쪽은 월드 (+1,-1), 화면 위는 월드 (-1,-1) 이라 되돌려서 넘긴다
	const float velocity_x = (dir_right - dir_up) * Balance::PLAYER_MOVE_SPEED;
	const float velocity_y = (-dir_right - dir_up) * Balance::PLAYER_MOVE_SPEED;

	MovePlayer(velocity_x, velocity_y, delta_seconds);
}

void MovePlayerByWorldAxis(float world_x, float world_y, float delta_seconds)
{
	const float length = std::sqrt(world_x * world_x + world_y * world_y);
	if (length < 0.0001f)
	{
		return;
	}

	const float velocity_x = world_x / length * Balance::PLAYER_MOVE_SPEED;
	const float velocity_y = world_y / length * Balance::PLAYER_MOVE_SPEED;

	MovePlayer(velocity_x, velocity_y, delta_seconds);
}

void PickupMaterials()
{
	PickupNearbyMaterials();
}

void ToggleChunkGrid()
{
	g_show_chunk_grid = !g_show_chunk_grid;
}

void ToggleLighting()
{
	g_use_lighting = !g_use_lighting;
}
