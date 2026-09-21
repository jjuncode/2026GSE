#include "stdafx.h"
#include "ZoneGen.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>

namespace
{
	constexpr int32 BORDER = 2;
	constexpr float INITIAL_FILL = 0.43f;
	constexpr int32 SMOOTH_PASS_COUNT = 4;
	constexpr int32 SPAWN_CLEAR_RADIUS = 5;
	constexpr int32 BOSS_ARENA_RADIUS = 4;
	constexpr int32 MIN_REGION_SIZE = 24;	// 이보다 작은 빈 터는 뚫지 않고 메운다
	constexpr int32 MAX_CONNECT_PASS = 256;
	constexpr int32 PACK_COUNT = 10;
	constexpr int32 PACK_MIN_SPAWN_DISTANCE = 14;
	constexpr float PACK_SPACING = 11.f;
	constexpr float PACK_BOSS_SPACING = 10.f;
	constexpr int32 LIGHT_COUNT = 12;
	constexpr float LIGHT_SPACING = 13.f;
	constexpr int32 BOULDER_FIELD_COUNT = 7;
	constexpr int32 BOULDER_FIELD_RADIUS = 4;
	constexpr int32 WATER_MIN_SIZE = 12;
	constexpr int32 WATER_MAX_SIZE = 180;
	constexpr float WATER_REGION_CHANCE = 0.3f;	// 전부 물이면 씨앗마다 웅덩이가 40개라 사막이 늪이 된다
	constexpr int32 BOULDER_MAX_SIZE = 3;
	constexpr float PALM_NEAR_WATER_CHANCE = 0.12f;
	constexpr int32 MAX_PICK_TRY = 3000;

	const std::array<int32, 4> STEP_X = { 1, -1, 0, 0 };
	const std::array<int32, 4> STEP_Y = { 0, 0, 1, -1 };

	size_t ToIndex(int32 x, int32 y)
	{
		return static_cast<size_t>(y) * static_cast<size_t>(ZONE_TILE_COUNT) + static_cast<size_t>(x);
	}

	bool IsInside(int32 x, int32 y)
	{
		if (x < 0 || y < 0)
		{
			return false;
		}

		if (x >= ZONE_TILE_COUNT || y >= ZONE_TILE_COUNT)
		{
			return false;
		}

		return true;
	}

	bool IsBorder(int32 x, int32 y)
	{
		if (x < BORDER || y < BORDER)
		{
			return true;
		}

		if (x >= ZONE_TILE_COUNT - BORDER || y >= ZONE_TILE_COUNT - BORDER)
		{
			return true;
		}

		return false;
	}

	bool IsOpenAt(const std::vector<uint8>& tiles, int32 x, int32 y)
	{
		if (IsInside(x, y) == false)
		{
			return false;
		}

		return IsBlockingTile(tiles[ToIndex(x, y)]) == false;
	}

	int32 SignOf(int32 value)
	{
		if (value > 0)
		{
			return 1;
		}

		if (value < 0)
		{
			return -1;
		}

		return 0;
	}

	float TileDistance(const ZoneTile& from, const ZoneTile& to)
	{
		const float delta_x = static_cast<float>(to.x - from.x);
		const float delta_y = static_cast<float>(to.y - from.y);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	uint32 NextRandom(uint32* state)
	{
		uint32 value = *state;
		value ^= value << 13;
		value ^= value >> 17;
		value ^= value << 5;
		*state = value;

		return value;
	}

	ZoneTile PickRandomTile(uint32* state)
	{
		ZoneTile tile;
		tile.x = static_cast<int32>(NextRandom(state) % static_cast<uint32>(ZONE_TILE_COUNT));
		tile.y = static_cast<int32>(NextRandom(state) % static_cast<uint32>(ZONE_TILE_COUNT));

		return tile;
	}

	// 난수를 차례로 뽑지 않고 좌표로 해시해야 같은 씨앗에서 같은 지형이 나온다
	void FillNoise(uint32 seed, std::vector<uint8>* tiles)
	{
		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				uint8 tile = TILE_OPEN;

				if (IsBorder(x, y))
				{
					tile = TILE_CLIFF;
				}
				else if (HashToUnit(HashZoneTile(seed, x, y)) < INITIAL_FILL)
				{
					tile = TILE_CLIFF;
				}

				(*tiles)[ToIndex(x, y)] = tile;
			}
		}
	}

	int32 CountBlockedAround(const std::vector<uint8>& tiles, int32 x, int32 y)
	{
		int32 count = 0;

		for (int32 offset_y = -1; offset_y <= 1; ++offset_y)
		{
			for (int32 offset_x = -1; offset_x <= 1; ++offset_x)
			{
				if (offset_x == 0 && offset_y == 0)
				{
					continue;
				}

				if (IsOpenAt(tiles, x + offset_x, y + offset_y) == false)
				{
					count += 1;
				}
			}
		}

		return count;
	}

	// 셀룰러 오토마타. 흩뿌린 점이 숲 덩어리와 빈 터로 뭉친다
	void Smooth(std::vector<uint8>* tiles)
	{
		const std::vector<uint8> source = *tiles;

		for (int32 y = BORDER; y < ZONE_TILE_COUNT - BORDER; ++y)
		{
			for (int32 x = BORDER; x < ZONE_TILE_COUNT - BORDER; ++x)
			{
				const int32 blocked = CountBlockedAround(source, x, y);

				if (blocked >= 5)
				{
					(*tiles)[ToIndex(x, y)] = TILE_CLIFF;
				}
				else if (blocked <= 3)
				{
					(*tiles)[ToIndex(x, y)] = TILE_OPEN;
				}
			}
		}
	}

	void CarveCircle(std::vector<uint8>* tiles, const ZoneTile& center, int32 radius)
	{
		for (int32 offset_y = -radius; offset_y <= radius; ++offset_y)
		{
			for (int32 offset_x = -radius; offset_x <= radius; ++offset_x)
			{
				if (offset_x * offset_x + offset_y * offset_y > radius * radius)
				{
					continue;
				}

				const int32 x = center.x + offset_x;
				const int32 y = center.y + offset_y;

				if (IsInside(x, y) == false || IsBorder(x, y))
				{
					continue;
				}

				(*tiles)[ToIndex(x, y)] = TILE_OPEN;
			}
		}
	}

	// 4방향으로만 잇는다. 모서리로만 닿은 빈 칸은 사각형 몸이 못 지나간다
	void MeasureDistance(const std::vector<uint8>& tiles, const ZoneTile& start, std::vector<int32>* distances)
	{
		distances->assign(tiles.size(), -1);

		if (IsOpenAt(tiles, start.x, start.y) == false)
		{
			return;
		}

		std::deque<ZoneTile> queue;
		(*distances)[ToIndex(start.x, start.y)] = 0;
		queue.push_back(start);

		while (queue.empty() == false)
		{
			const ZoneTile current = queue.front();
			queue.pop_front();

			const int32 current_distance = (*distances)[ToIndex(current.x, current.y)];

			for (size_t dir = 0; dir < STEP_X.size(); ++dir)
			{
				ZoneTile next;
				next.x = current.x + STEP_X[dir];
				next.y = current.y + STEP_Y[dir];

				if (IsOpenAt(tiles, next.x, next.y) == false)
				{
					continue;
				}

				const size_t next_index = ToIndex(next.x, next.y);
				if ((*distances)[next_index] >= 0)
				{
					continue;
				}

				(*distances)[next_index] = current_distance + 1;
				queue.push_back(next);
			}
		}
	}

	bool FindOrphan(const std::vector<uint8>& tiles, const std::vector<int32>& distances, ZoneTile* orphan)
	{
		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				if (IsOpenAt(tiles, x, y) && distances[ToIndex(x, y)] < 0)
				{
					orphan->x = x;
					orphan->y = y;
					return true;
				}
			}
		}

		return false;
	}

	// 스폰과 이어진 칸에 닿을 때까지 파 들어간다
	void CarveCorridor(std::vector<uint8>* tiles, const std::vector<int32>& spawn_distances, const ZoneTile& from, const ZoneTile& to)
	{
		ZoneTile current = from;
		bool moving_x = true;

		for (int32 guard = 0; guard < ZONE_TILE_COUNT * 4; ++guard)
		{
			const size_t index = ToIndex(current.x, current.y);
			if (spawn_distances[index] >= 0)
			{
				return;
			}

			(*tiles)[index] = TILE_OPEN;

			// 폭 2 ( 한 칸 통로는 몸이 겨우 끼어 답답하다 )
			ZoneTile side = current;
			if (moving_x)
			{
				side.y += 1;
			}
			else
			{
				side.x += 1;
			}

			if (IsBorder(side.x, side.y) == false)
			{
				(*tiles)[ToIndex(side.x, side.y)] = TILE_OPEN;
			}

			const int32 delta_x = to.x - current.x;
			const int32 delta_y = to.y - current.y;

			if (delta_x == 0 && delta_y == 0)
			{
				return;
			}

			if (std::abs(delta_x) >= std::abs(delta_y))
			{
				current.x += SignOf(delta_x);
				moving_x = true;
			}
			else
			{
				current.y += SignOf(delta_y);
				moving_x = false;
			}
		}
	}

	void ConnectRegions(std::vector<uint8>* tiles, const ZoneTile& spawn)
	{
		std::vector<int32> spawn_distances;
		std::vector<int32> region_distances;

		for (int32 pass = 0; pass < MAX_CONNECT_PASS; ++pass)
		{
			MeasureDistance(*tiles, spawn, &spawn_distances);

			ZoneTile orphan;
			if (FindOrphan(*tiles, spawn_distances, &orphan) == false)
			{
				return;
			}

			MeasureDistance(*tiles, orphan, &region_distances);

			int32 region_size = 0;
			for (int32 distance : region_distances)
			{
				if (distance >= 0)
				{
					region_size += 1;
				}
			}

			if (region_size < MIN_REGION_SIZE)
			{
				for (size_t index = 0; index < region_distances.size(); ++index)
				{
					if (region_distances[index] >= 0)
					{
						(*tiles)[index] = TILE_CLIFF;
					}
				}

				continue;
			}

			CarveCorridor(tiles, spawn_distances, orphan, spawn);
		}
	}

	// 연결 패스 횟수가 모자라도 갇힌 빈 칸이 절대 안 남게 마지막에 메운다
	void FillUnreached(std::vector<uint8>* tiles, const ZoneTile& spawn)
	{
		std::vector<int32> distances;
		MeasureDistance(*tiles, spawn, &distances);

		for (size_t index = 0; index < tiles->size(); ++index)
		{
			if (IsBlockingTile((*tiles)[index]) == false && distances[index] < 0)
			{
				(*tiles)[index] = TILE_CLIFF;
			}
		}
	}

	ZoneTile FindFarthestTile(const std::vector<int32>& distances, const ZoneTile& fallback)
	{
		ZoneTile farthest = fallback;
		int32 best = -1;

		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const int32 distance = distances[ToIndex(x, y)];
				if (distance > best)
				{
					best = distance;
					farthest.x = x;
					farthest.y = y;
				}
			}
		}

		return farthest;
	}

	// 테두리에 붙은 막힌 덩어리는 낭떠러지, 가운데 떠 있으면 물웅덩이나 바위. 막힘 여부는 안 바꾼다
	void ClassifyBlockedRegions(uint32 seed, std::vector<uint8>* tiles)
	{
		std::vector<bool> visited(tiles->size(), false);
		std::vector<ZoneTile> region;
		std::deque<ZoneTile> queue;

		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const size_t start_index = ToIndex(x, y);
				if (visited[start_index] || (*tiles)[start_index] != TILE_CLIFF)
				{
					continue;
				}

				ZoneTile start;
				start.x = x;
				start.y = y;

				region.clear();
				bool touches_border = false;
				visited[start_index] = true;
				queue.push_back(start);

				while (queue.empty() == false)
				{
					const ZoneTile current = queue.front();
					queue.pop_front();
					region.push_back(current);

					if (IsBorder(current.x, current.y))
					{
						touches_border = true;
					}

					for (int32 offset_y = -1; offset_y <= 1; ++offset_y)
					{
						for (int32 offset_x = -1; offset_x <= 1; ++offset_x)
						{
							ZoneTile next;
							next.x = current.x + offset_x;
							next.y = current.y + offset_y;

							if (IsInside(next.x, next.y) == false)
							{
								continue;
							}

							const size_t next_index = ToIndex(next.x, next.y);
							if (visited[next_index] || (*tiles)[next_index] != TILE_CLIFF)
							{
								continue;
							}

							visited[next_index] = true;
							queue.push_back(next);
						}
					}
				}

				if (touches_border)
				{
					continue;
				}

				uint8 kind = TILE_CLIFF;
				const int32 size = static_cast<int32>(region.size());

				if (size <= BOULDER_MAX_SIZE)
				{
					kind = TILE_ROCK;
				}
				else if (size >= WATER_MIN_SIZE && size <= WATER_MAX_SIZE)
				{
					if (HashToUnit(HashZoneTile(seed + 404u, start.x, start.y)) < WATER_REGION_CHANCE)
					{
						kind = TILE_WATER;
					}
				}

				for (const ZoneTile& tile : region)
				{
					(*tiles)[ToIndex(tile.x, tile.y)] = kind;
				}
			}
		}
	}

	// 여덟 이웃이 전부 비어야 박는다. 외딴 막힌 칸은 둘레가 고리로 이어져 길을 못 끊는다
	bool IsIsolatedSpot(const std::vector<uint8>& tiles, int32 x, int32 y)
	{
		for (int32 offset_y = -1; offset_y <= 1; ++offset_y)
		{
			for (int32 offset_x = -1; offset_x <= 1; ++offset_x)
			{
				if (IsOpenAt(tiles, x + offset_x, y + offset_y) == false)
				{
					return false;
				}
			}
		}

		return true;
	}

	void PlaceBoulderFields(uint32 seed, uint32* random_state, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		int32 placed = 0;

		for (int32 attempt = 0; attempt < MAX_PICK_TRY && placed < BOULDER_FIELD_COUNT; ++attempt)
		{
			const ZoneTile center = PickRandomTile(random_state);

			if (IsOpenAt(layout->tiles, center.x, center.y) == false)
			{
				continue;
			}

			if (distances[ToIndex(center.x, center.y)] < SPAWN_CLEAR_RADIUS + 4)
			{
				continue;
			}

			if (TileDistance(center, layout->boss) < static_cast<float>(BOSS_ARENA_RADIUS + 4))
			{
				continue;
			}

			for (int32 offset_y = -BOULDER_FIELD_RADIUS; offset_y <= BOULDER_FIELD_RADIUS; ++offset_y)
			{
				for (int32 offset_x = -BOULDER_FIELD_RADIUS; offset_x <= BOULDER_FIELD_RADIUS; ++offset_x)
				{
					const int32 x = center.x + offset_x;
					const int32 y = center.y + offset_y;

					if (IsInside(x, y) == false || IsIsolatedSpot(layout->tiles, x, y) == false)
					{
						continue;
					}

					const float roll = HashToUnit(HashZoneTile(seed + 202u, x, y));

					if (roll < 0.05f)
					{
						layout->tiles[ToIndex(x, y)] = TILE_PALM;
					}
					else if (roll < 0.20f)
					{
						layout->tiles[ToIndex(x, y)] = TILE_ROCK;
					}
				}
			}

			placed += 1;
		}
	}

	bool HasWaterNear(const std::vector<uint8>& tiles, int32 x, int32 y, int32 radius)
	{
		for (int32 offset_y = -radius; offset_y <= radius; ++offset_y)
		{
			for (int32 offset_x = -radius; offset_x <= radius; ++offset_x)
			{
				const int32 near_x = x + offset_x;
				const int32 near_y = y + offset_y;

				if (IsInside(near_x, near_y) && tiles[ToIndex(near_x, near_y)] == TILE_WATER)
				{
					return true;
				}
			}
		}

		return false;
	}

	// 물가 한 칸 건너에 야자수. 외딴 자리 규칙을 그대로 써서 길을 못 막는다
	void PlacePalmsByWater(uint32 seed, std::vector<uint8>* tiles)
	{
		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				if (IsIsolatedSpot(*tiles, x, y) == false || HasWaterNear(*tiles, x, y, 2) == false)
				{
					continue;
				}

				if (HashToUnit(HashZoneTile(seed + 303u, x, y)) >= PALM_NEAR_WATER_CHANCE)
				{
					continue;
				}

				(*tiles)[ToIndex(x, y)] = TILE_PALM;
			}
		}
	}

	bool IsFarFromAll(const ZoneTile& tile, const std::vector<ZoneTile>& others, float spacing)
	{
		for (const ZoneTile& other : others)
		{
			if (TileDistance(tile, other) < spacing)
			{
				return false;
			}
		}

		return true;
	}

	void PickPacks(uint32* random_state, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		for (int32 attempt = 0; attempt < MAX_PICK_TRY; ++attempt)
		{
			if (static_cast<int32>(layout->packs.size()) >= PACK_COUNT)
			{
				return;
			}

			const ZoneTile tile = PickRandomTile(random_state);

			if (IsOpenAt(layout->tiles, tile.x, tile.y) == false)
			{
				continue;
			}

			if (distances[ToIndex(tile.x, tile.y)] < PACK_MIN_SPAWN_DISTANCE)
			{
				continue;
			}

			if (TileDistance(tile, layout->boss) < PACK_BOSS_SPACING)
			{
				continue;
			}

			if (IsFarFromAll(tile, layout->packs, PACK_SPACING) == false)
			{
				continue;
			}

			layout->packs.push_back(tile);
		}
	}

	void PickLights(uint32* random_state, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		ZoneTile campfire = layout->spawn;
		campfire.x += 2;
		campfire.y -= 1;
		layout->lights.push_back(campfire);

		ZoneTile boss_left = layout->boss;
		boss_left.x -= 3;
		layout->lights.push_back(boss_left);

		ZoneTile boss_right = layout->boss;
		boss_right.x += 3;
		layout->lights.push_back(boss_right);

		for (int32 attempt = 0; attempt < MAX_PICK_TRY; ++attempt)
		{
			if (static_cast<int32>(layout->lights.size()) >= LIGHT_COUNT)
			{
				return;
			}

			const ZoneTile tile = PickRandomTile(random_state);

			if (IsOpenAt(layout->tiles, tile.x, tile.y) == false)
			{
				continue;
			}

			if (distances[ToIndex(tile.x, tile.y)] < 8)
			{
				continue;
			}

			if (IsFarFromAll(tile, layout->lights, LIGHT_SPACING) == false)
			{
				continue;
			}

			layout->lights.push_back(tile);
		}
	}
}

void GenerateZone(uint32 seed, ZoneLayout* layout)
{
	layout->seed = seed;
	layout->tiles.assign(static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT), TILE_OPEN);
	layout->packs.clear();
	layout->lights.clear();

	layout->spawn.x = ZONE_TILE_COUNT / 2;
	layout->spawn.y = ZONE_TILE_COUNT / 2;

	FillNoise(seed, &layout->tiles);

	for (int32 pass = 0; pass < SMOOTH_PASS_COUNT; ++pass)
	{
		Smooth(&layout->tiles);
	}

	CarveCircle(&layout->tiles, layout->spawn, SPAWN_CLEAR_RADIUS);
	ConnectRegions(&layout->tiles, layout->spawn);
	FillUnreached(&layout->tiles, layout->spawn);

	std::vector<int32> distances;
	MeasureDistance(layout->tiles, layout->spawn, &distances);

	// 걸어서 제일 먼 곳. 직선 거리로 고르면 벽 너머 코앞일 수 있다
	layout->boss = FindFarthestTile(distances, layout->spawn);
	CarveCircle(&layout->tiles, layout->boss, BOSS_ARENA_RADIUS);

	ClassifyBlockedRegions(seed, &layout->tiles);
	MeasureDistance(layout->tiles, layout->spawn, &distances);

	uint32 random_state = seed * 747796405u + 2891336453u;
	if (random_state == 0)
	{
		random_state = 1;
	}

	PlaceBoulderFields(seed, &random_state, distances, layout);
	PlacePalmsByWater(seed, &layout->tiles);
	MeasureDistance(layout->tiles, layout->spawn, &distances);

	PickPacks(&random_state, distances, layout);
	PickLights(&random_state, distances, layout);
}

bool IsBlockingTile(uint8 tile)
{
	return tile != TILE_OPEN;
}

uint8 GetZoneTile(const ZoneLayout& layout, int32 x, int32 y)
{
	if (IsInside(x, y) == false)
	{
		return TILE_CLIFF;
	}

	return layout.tiles[ToIndex(x, y)];
}

WorldPos ZoneTileToWorld(int32 x, int32 y, float offset_x, float offset_y)
{
	WorldPos pos;
	pos.chunk_x = -ZONE_CHUNK_RADIUS;
	pos.chunk_y = -ZONE_CHUNK_RADIUS;

	MoveWorldPos(&pos, static_cast<float>(x) + offset_x, static_cast<float>(y) + offset_y);

	return pos;
}

bool WorldToZoneTile(const WorldPos& pos, int32* x, int32* y)
{
	*x = (pos.chunk_x + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT + static_cast<int32>(std::floor(pos.local_x));
	*y = (pos.chunk_y + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT + static_cast<int32>(std::floor(pos.local_y));

	return IsInside(*x, *y);
}

uint32 HashZoneTile(uint32 seed, int32 x, int32 y)
{
	uint32 hash = seed * 2654435761u;
	hash ^= static_cast<uint32>(x) * 374761393u;
	hash = (hash ^ (hash >> 13)) * 1274126177u;
	hash ^= static_cast<uint32>(y) * 668265263u;
	hash = (hash ^ (hash >> 16)) * 2246822519u;

	return hash ^ (hash >> 15);
}

float HashToUnit(uint32 hash)
{
	return static_cast<float>(hash & 0xFFFFu) / 65535.f;
}
