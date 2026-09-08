#include "stdafx.h"
#include "Scene.h"
#include "Renderer.h"
#include "Ground.h"
#include "Fog.h"
#include "GameConfig.h"

#include <vector>
#include <algorithm>
#include <cmath>

namespace
{
	const float PI = 3.14159265f;

	const float PLAYER_SPEED_TILES = 4.5f;
	const size_t PLAYER_INDEX = 0;

	const float LIGHT_CORE_GAIN = 6.f;

	std::vector<GameObject> g_objects;
	std::vector<size_t> g_light_indices;
	std::vector<size_t> g_draw_order;

	WorldPos g_picked_tile;
	bool g_has_picked_tile = false;
	bool g_show_chunk_grid = false;
	bool g_use_lighting = true;

	float g_fog_scroll = 0.f;
	unsigned int g_wander_seed = 12345u;

	// 같은 칸이면 늘 같은 값이 나와야 걸을 때 바닥이 안 깜빡인다
	unsigned int HashInt(int x, int y)
	{
		unsigned int hash = static_cast<unsigned int>(x) * 374761393u;
		hash += static_cast<unsigned int>(y) * 668265263u;
		hash = (hash ^ (hash >> 13)) * 1274126177u;

		return hash ^ (hash >> 16);
	}

	float HashToUnit(unsigned int hash)
	{
		return static_cast<float>(hash & 0xFFFFu) / 65535.f;
	}

	int GetGlobalTileX(const WorldPos& pos)
	{
		return pos.m_chunk_x * CHUNK_TILE_COUNT + static_cast<int>(pos.m_local_x);
	}

	int GetGlobalTileY(const WorldPos& pos)
	{
		return pos.m_chunk_y * CHUNK_TILE_COUNT + static_cast<int>(pos.m_local_y);
	}

	bool IsOffScreen(const ScreenPos& screen, float margin)
	{
		const float limit_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f + margin;
		const float limit_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f + margin;

		if (std::fabs(screen.m_x) > limit_x)
		{
			return true;
		}

		if (std::fabs(screen.m_y) > limit_y)
		{
			return true;
		}

		return false;
	}

	// 바닥색과 조명은 셰이더가 픽셀마다 푼다. 여기선 카메라와 광원만 챙겨 넘긴다
	GroundFrame MakeGroundFrame(const WorldPos& camera)
	{
		GroundFrame frame;

		frame.m_camera_tile_x = GetGlobalTileX(camera);
		frame.m_camera_tile_y = GetGlobalTileY(camera);
		frame.m_camera_frac_x = camera.m_local_x - std::floor(camera.m_local_x);
		frame.m_camera_frac_y = camera.m_local_y - std::floor(camera.m_local_y);

		if (g_use_lighting)
		{
			for (size_t order = 0; order < g_light_indices.size(); ++order)
			{
				if (frame.m_light_count >= GROUND_MAX_LIGHT_COUNT)
				{
					break;
				}

				const GameObject& light = g_objects[g_light_indices[order]];
				GroundLight& target = frame.m_lights[static_cast<size_t>(frame.m_light_count)];

				target.m_tile_offset_x = GetTileDistanceX(camera, light.m_pos);
				target.m_tile_offset_y = GetTileDistanceY(camera, light.m_pos);
				target.m_radius = light.m_light_radius;
				target.m_r = light.m_light_r;
				target.m_g = light.m_light_g;
				target.m_b = light.m_light_b;
				target.m_strength = light.m_light_strength;

				frame.m_light_count += 1;
			}
		}

		frame.m_show_chunk_grid = g_show_chunk_grid;

		return frame;
	}
}

namespace
{
	// 배치는 전부 시작 지점에서 몇 칸 떨어졌는지로 적는다
	WorldPos MakeScenePos(float tile_x, float tile_y)
	{
		WorldPos pos;
		pos.m_local_x = 16.f;
		pos.m_local_y = 16.f;

		MoveWorldPos(&pos, tile_x, tile_y);

		return pos;
	}

	void AddUpright(float tile_x, float tile_y, float width, float height, float r, float g, float b)
	{
		GameObject object;
		object.m_pos = MakeScenePos(tile_x, tile_y);
		object.m_width = width;
		object.m_height = height;
		object.m_r = r;
		object.m_g = g;
		object.m_b = b;

		g_objects.push_back(object);
	}

	void AddWanderer(float tile_x, float tile_y, float width, float height, float r, float g, float b, float speed)
	{
		GameObject object;
		object.m_pos = MakeScenePos(tile_x, tile_y);
		object.m_home_pos = object.m_pos;
		object.m_width = width;
		object.m_height = height;
		object.m_r = r;
		object.m_g = g;
		object.m_b = b;
		object.m_move_speed = speed;

		g_objects.push_back(object);
	}

	void AddLight(float tile_x, float tile_y, float radius, float r, float g, float b, float strength)
	{
		GameObject object;
		object.m_pos = MakeScenePos(tile_x, tile_y);
		object.m_is_upright = false;
		object.m_width = 0.f;
		object.m_height = 0.f;
		object.m_light_radius = radius;
		object.m_light_r = r;
		object.m_light_g = g;
		object.m_light_b = b;
		object.m_light_strength = strength;

		g_objects.push_back(object);
	}

	void AddEmissive(float tile_x, float tile_y, float size, float r, float g, float b, float emissive)
	{
		GameObject object;
		object.m_pos = MakeScenePos(tile_x, tile_y);
		object.m_width = size;
		object.m_height = size;
		object.m_r = r;
		object.m_g = g;
		object.m_b = b;
		object.m_emissive = emissive;

		g_objects.push_back(object);
	}

	// 화면에서 수평으로 뻗는 울타리. 월드 두 축을 반대로 움직여야 화면에서 가로선이 된다
	void AddFenceRow(float center_x, float center_y, float half_span, float step, float gate_half_width)
	{
		for (float offset = -half_span; offset <= half_span; offset += step)
		{
			if (std::fabs(offset) < gate_half_width)
			{
				continue;
			}

			AddUpright(center_x + offset, center_y - offset, 3.f, 44.f, 0.050f, 0.050f, 0.062f);
		}
	}

	// 화면에서 세로로 내려오는 울타리. 두 축을 같이 늘리면 화면에서 아래로 간다
	void AddFenceColumn(float start_x, float start_y, float length, float step)
	{
		for (float offset = 0.f; offset <= length; offset += step)
		{
			AddUpright(start_x + offset, start_y + offset, 3.f, 44.f, 0.050f, 0.050f, 0.062f);
		}
	}

	void AddScatteredGrass()
	{
		for (int tile_y = -24; tile_y <= 24; ++tile_y)
		{
			for (int tile_x = -24; tile_x <= 24; ++tile_x)
			{
				const unsigned int hash = HashInt(tile_x * 7 + 13, tile_y * 11 + 29);
				const float roll = HashToUnit(hash);

				if (roll > 0.11f)
				{
					continue;
				}

				const float size_roll = HashToUnit(hash >> 7);
				const float width = 3.f + size_roll * 4.f;
				const float height = 5.f + size_roll * 7.f;

				AddUpright(static_cast<float>(tile_x), static_cast<float>(tile_y), width, height, 0.075f, 0.095f, 0.058f);
			}
		}
	}

	void BuildGraveyard()
	{
		// 화면 위쪽을 가로지르는 철제 울타리. 가운데는 열린 문이라 비운다
		AddFenceRow(-6.f, -6.f, 8.f, 0.5f, 1.2f);
		AddFenceColumn(2.f, -14.f, 7.f, 0.5f);
		AddFenceColumn(-14.f, 2.f, 5.f, 0.5f);

		// 문기둥. 울타리 창살보다 굵고 높아야 문이라고 읽힌다
		AddUpright(-7.2f, -4.8f, 9.f, 62.f, 0.070f, 0.068f, 0.075f);
		AddUpright(-4.8f, -7.2f, 9.f, 62.f, 0.070f, 0.068f, 0.075f);

		// 오른쪽 위 멀리 선 탑. 멀리 큰 게 하나 있으면 공간이 넓어 보인다
		AddUpright(2.f, -16.f, 30.f, 132.f, 0.042f, 0.042f, 0.052f);
		AddUpright(2.f, -16.f, 14.f, 168.f, 0.038f, 0.038f, 0.048f);

		// 왼쪽 부서진 석벽
		AddUpright(-11.f, -3.f, 34.f, 26.f, 0.105f, 0.100f, 0.092f);
		AddUpright(-12.5f, -1.f, 28.f, 20.f, 0.098f, 0.094f, 0.086f);
		AddUpright(-10.f, 0.5f, 22.f, 32.f, 0.110f, 0.105f, 0.096f);
		AddUpright(-13.f, 4.f, 30.f, 22.f, 0.100f, 0.096f, 0.088f);

		// 묘비. 밀도가 고르면 절차 생성 티가 나므로 뭉치고 비운다
		AddUpright(-4.f, 3.f, 13.f, 22.f, 0.145f, 0.145f, 0.155f);
		AddUpright(-2.6f, 4.6f, 11.f, 19.f, 0.135f, 0.135f, 0.145f);
		AddUpright(-3.2f, 6.2f, 15.f, 25.f, 0.150f, 0.148f, 0.158f);
		AddUpright(1.f, 6.f, 12.f, 20.f, 0.140f, 0.140f, 0.150f);
		AddUpright(2.4f, 7.4f, 14.f, 24.f, 0.148f, 0.146f, 0.156f);
		AddUpright(4.f, 4.f, 11.f, 18.f, 0.132f, 0.132f, 0.142f);
		AddUpright(-6.f, 1.f, 13.f, 23.f, 0.142f, 0.140f, 0.150f);
		AddUpright(6.f, 2.f, 12.f, 21.f, 0.138f, 0.138f, 0.148f);
		AddUpright(5.2f, 8.6f, 14.f, 26.f, 0.150f, 0.148f, 0.158f);
		AddUpright(-1.f, 9.f, 12.f, 20.f, 0.136f, 0.136f, 0.146f);

		// 나무. 화면 가장자리에 어두운 실루엣으로만 둔다
		AddUpright(8.5f, 4.0f, 13.f, 23.f, 0.142f, 0.140f, 0.150f);
		AddUpright(10.0f, 6.5f, 15.f, 26.f, 0.148f, 0.146f, 0.156f);
		AddUpright(12.0f, 3.0f, 12.f, 20.f, 0.134f, 0.134f, 0.144f);
		AddUpright(12.5f, 6.0f, 26.f, 22.f, 0.100f, 0.096f, 0.088f);
		AddUpright(3.0f, 11.0f, 14.f, 24.f, 0.146f, 0.144f, 0.154f);
		AddUpright(1.5f, 13.0f, 12.f, 21.f, 0.138f, 0.138f, 0.148f);
		AddUpright(-2.0f, 12.0f, 13.f, 22.f, 0.140f, 0.138f, 0.148f);
		AddUpright(6.5f, 9.5f, 11.f, 18.f, 0.130f, 0.130f, 0.140f);

		AddUpright(-13.f, -6.f, 15.f, 78.f, 0.035f, 0.042f, 0.032f);
		AddUpright(-15.f, 3.f, 13.f, 66.f, 0.032f, 0.038f, 0.030f);
		AddUpright(9.f, -9.f, 14.f, 72.f, 0.034f, 0.040f, 0.031f);
		AddUpright(12.f, 2.f, 16.f, 84.f, 0.033f, 0.039f, 0.030f);
		AddUpright(7.f, 11.f, 13.f, 62.f, 0.032f, 0.038f, 0.029f);
		AddUpright(5.f, 14.f, 15.f, 68.f, 0.033f, 0.039f, 0.030f);
		AddUpright(14.f, 8.f, 14.f, 64.f, 0.032f, 0.038f, 0.029f);
	}

	void BuildMonsters()
	{
		AddWanderer(0.5f, 3.5f, 12.f, 26.f, 0.130f, 0.150f, 0.125f, 0.7f);
		AddWanderer(2.2f, 3.0f, 12.f, 25.f, 0.125f, 0.145f, 0.120f, 0.8f);
		AddWanderer(-1.2f, 4.4f, 12.f, 27.f, 0.135f, 0.155f, 0.130f, 0.6f);
		AddWanderer(3.4f, 5.2f, 12.f, 24.f, 0.128f, 0.148f, 0.122f, 0.9f);
		AddWanderer(1.4f, 5.6f, 12.f, 26.f, 0.132f, 0.152f, 0.127f, 0.7f);
		AddWanderer(-3.4f, 5.0f, 12.f, 25.f, 0.126f, 0.146f, 0.121f, 0.8f);

		// 보스. 화면에서 제일 붉은 점이라 눈이 여기로 간다
		AddWanderer(-2.4f, 0.6f, 15.f, 32.f, 0.480f, 0.105f, 0.095f, 0.45f);

		// 보스만 약하게 빛나게 둔다. 잡몹과 섮여 있어도 어느 것인지 바로 갈린다
		g_objects.back().m_emissive = 2.6f;
	}

	// 2.9 의 재료 드랍. 어두운 바닥에 밝은 점이 떨어지면
	// 그게 화면에서 제일 밝은 지점이 되고, 등급을 색만으로 낼 수 있다
	void BuildDrops()
	{
		AddEmissive(1.8f, 2.2f, 5.f, 0.90f, 0.92f, 0.98f, 3.0f);
		AddEmissive(-1.6f, 3.4f, 5.f, 0.90f, 0.92f, 0.98f, 3.0f);
		AddEmissive(4.2f, 6.4f, 5.f, 0.90f, 0.92f, 0.98f, 3.0f);

		AddEmissive(-3.0f, 2.0f, 6.f, 0.35f, 0.62f, 1.00f, 5.0f);

		// 별핵. 여기가 화면에서 가장 밝아야 한다
		AddEmissive(2.6f, 1.2f, 7.f, 1.00f, 0.52f, 0.12f, 8.0f);
	}
}

namespace
{
	void UpdateWanderer(GameObject* object, float delta_seconds)
	{
		object->m_move_timer -= delta_seconds;

		if (object->m_move_timer <= 0.f)
		{
			// 방향을 주기적으로 새로 뽑아야 한 방향으로 계속 흘러가지 않는다.
			// 지형과 달리 배회는 다시 물어도 같을 이유가 없어 전역 난수를 쓴다
			g_wander_seed += 2654435761u;

			const unsigned int hash = HashInt(static_cast<int>(g_wander_seed), static_cast<int>(g_wander_seed >> 16));
			const float angle = HashToUnit(hash) * 2.f * PI;

			object->m_move_dir_x = std::cos(angle);
			object->m_move_dir_y = std::sin(angle);
			object->m_move_timer = 0.7f + HashToUnit(hash >> 8) * 1.8f;
		}

		// 안 묶어 두면 무리가 서서히 흩어져 화면 밖으로 사라진다
		const float home_x = GetTileDistanceX(object->m_pos, object->m_home_pos);
		const float home_y = GetTileDistanceY(object->m_pos, object->m_home_pos);
		const float home_distance = std::sqrt(home_x * home_x + home_y * home_y);

		if (home_distance > 3.5f)
		{
			object->m_move_dir_x = home_x / home_distance;
			object->m_move_dir_y = home_y / home_distance;
		}

		const float step_x = object->m_move_dir_x * object->m_move_speed * delta_seconds;
		const float step_y = object->m_move_dir_y * object->m_move_speed * delta_seconds;

		MoveWorldPos(&object->m_pos, step_x, step_y);
	}

	void DrawGround(const WorldPos& camera)
	{
		DrawGroundLayer(MakeGroundFrame(camera));
	}

	void DrawPickedTile(Renderer* renderer, const WorldPos& camera)
	{
		if (!g_has_picked_tile)
		{
			return;
		}

		const ScreenPos screen = WorldToScreen(g_picked_tile, camera);

		renderer->DrawSolidDiamond(screen.m_x, screen.m_y, 1.f, 0.95f, 0.62f, 0.18f, 0.40f);
	}

	void DrawObject(Renderer* renderer, const GameObject& object, const WorldPos& camera)
	{
		if (!object.m_is_upright)
		{
			return;
		}

		const ScreenPos screen = WorldToScreen(object.m_pos, camera);

		// 발밑 그림자가 없으면 세운 물체가 공중에 뜬 것처럼 보인다
		const float shadow_scale = object.m_width / TILE_PIXEL_WIDTH * 1.7f;
		renderer->DrawSolidDiamond(screen.m_x, screen.m_y, shadow_scale, 0.f, 0.f, 0.f, 0.45f);

		// 세운 그림은 발밑이 월드 위치라 높이 절반을 얹는다
		const float foot_offset = object.m_height * 0.5f;
		const float glow = object.m_emissive;
		renderer->DrawSolidBox(screen.m_x, screen.m_y + foot_offset, object.m_width, object.m_height, object.m_r * glow, object.m_g * glow, object.m_b * glow, object.m_a);
	}
}

namespace
{
	void DrawSortedObjects(Renderer* renderer, const WorldPos& camera)
	{
		g_draw_order.clear();

		for (size_t index = 0; index < g_objects.size(); ++index)
		{
			if (!g_objects[index].m_is_upright)
			{
				continue;
			}

			const ScreenPos screen = WorldToScreen(g_objects[index].m_pos, camera);

			// 키 큰 물체는 발밑이 화면 밖이어도 머리가 걸치므로 여유를 크게 준다
			if (IsOffScreen(screen, 260.f))
			{
				continue;
			}

			g_draw_order.push_back(index);
		}

		// 뒤에 있는 것부터 그려야 앞엣것이 덮는다. 깊이 버퍼는 안 쓴다
		std::sort(g_draw_order.begin(), g_draw_order.end(),
			[&camera](size_t left, size_t right)
			{
				return GetDepthKey(g_objects[left].m_pos, camera) < GetDepthKey(g_objects[right].m_pos, camera);
			});

		for (size_t order = 0; order < g_draw_order.size(); ++order)
		{
			DrawObject(renderer, g_objects[g_draw_order[order]], camera);
		}
	}

	void DrawLightGlows(Renderer* renderer, const WorldPos& camera)
	{
		for (size_t order = 0; order < g_light_indices.size(); ++order)
		{
			const GameObject& light = g_objects[g_light_indices[order]];
			const ScreenPos screen = WorldToScreen(light.m_pos, camera);

			if (IsOffScreen(screen, 120.f))
			{
				continue;
			}

			// 고리를 겹쳐 번짐을 훉내 내면 알파 단계가 그대로 테두리로 보인다.
			// 코어 하나만 1 을 훨씬 넘기게 찍고 번짐은 블뢸에게 맡긴다
			const float core = light.m_light_strength * LIGHT_CORE_GAIN;

			renderer->DrawSolidCircle(screen.m_x, screen.m_y, 7.f, light.m_light_r * core, light.m_light_g * core, light.m_light_b * core, 1.f);
		}
	}

	void DrawFog()
	{
		DrawFogLayer(g_fog_scroll);
	}
}

void InitScene()
{
	g_objects.clear();
	g_light_indices.clear();

	// 인덱스 0 은 플레이어. 정렬은 그리는 차례만 바꾸므로 인덱스는 안 흔들린다
	GameObject player;
	player.m_pos = MakeScenePos(0.f, 0.f);
	player.m_width = 15.f;
	player.m_height = 33.f;
	player.m_r = 0.80f;
	player.m_g = 0.72f;
	player.m_b = 0.52f;
	g_objects.push_back(player);

	BuildGraveyard();
	BuildMonsters();
	AddScatteredGrass();
	BuildDrops();

	AddLight(0.f, -2.f, 10.f, 1.00f, 0.74f, 0.34f, 1.40f);
	AddLight(9.f, -10.f, 6.f, 0.95f, 0.55f, 0.22f, 0.75f);
	AddLight(-9.f, 4.f, 6.f, 0.30f, 0.48f, 0.95f, 0.60f);

	for (size_t index = 0; index < g_objects.size(); ++index)
	{
		if (g_objects[index].m_light_radius > 0.f)
		{
			g_light_indices.push_back(index);
		}
	}
}

void UpdateScene(float delta_seconds)
{
	g_fog_scroll += delta_seconds * 60.f;

	for (size_t index = 0; index < g_objects.size(); ++index)
	{
		if (g_objects[index].m_move_speed <= 0.f)
		{
			continue;
		}

		UpdateWanderer(&g_objects[index], delta_seconds);
	}
}

void DrawScene(Renderer* renderer)
{
	const WorldPos camera = GetCameraPos();

	DrawGround(camera);
	DrawPickedTile(renderer, camera);
	DrawSortedObjects(renderer, camera);
	DrawLightGlows(renderer, camera);
	DrawFog();
}

WorldPos GetCameraPos()
{
	if (g_objects.empty())
	{
		WorldPos fallback;
		return fallback;
	}

	return g_objects[PLAYER_INDEX].m_pos;
}

void MovePlayerByScreenAxis(float screen_right, float screen_up, float delta_seconds)
{
	if (g_objects.empty())
	{
		return;
	}

	const float length = std::sqrt(screen_right * screen_right + screen_up * screen_up);
	if (length < 0.0001f)
	{
		return;
	}

	// 화면에서 먼저 길이를 1 로 맞춰야 대각선이 더 빠르지 않다
	const float dir_right = screen_right / length;
	const float dir_up = screen_up / length;

	// 화면 오른쪽은 월드 (+1,-1), 화면 위는 월드 (-1,-1) 이라 되돌려서 넘긴다
	const float step = PLAYER_SPEED_TILES * delta_seconds;
	const float delta_x = (dir_right - dir_up) * step;
	const float delta_y = (-dir_right - dir_up) * step;

	MoveWorldPos(&g_objects[PLAYER_INDEX].m_pos, delta_x, delta_y);
}

void MovePlayerByWorldAxis(float world_x, float world_y, float delta_seconds)
{
	if (g_objects.empty())
	{
		return;
	}

	const float length = std::sqrt(world_x * world_x + world_y * world_y);
	if (length < 0.0001f)
	{
		return;
	}

	const float step = PLAYER_SPEED_TILES * delta_seconds;
	const float delta_x = world_x / length * step;
	const float delta_y = world_y / length * step;

	MoveWorldPos(&g_objects[PLAYER_INDEX].m_pos, delta_x, delta_y);
}

void SetPickedTile(const WorldPos& tile)
{
	g_picked_tile = tile;
	g_has_picked_tile = true;
}

void ToggleChunkGrid()
{
	g_show_chunk_grid = !g_show_chunk_grid;
}

void ToggleLighting()
{
	g_use_lighting = !g_use_lighting;
}
