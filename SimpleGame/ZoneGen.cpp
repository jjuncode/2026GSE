#include "stdafx.h"
#include "ZoneGen.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>

namespace
{
	constexpr int32 BORDER = 2;
	constexpr float INITIAL_FILL = 0.40f;
	constexpr int32 SMOOTH_PASS_COUNT = 4;
	constexpr int32 MIN_REGION_SIZE = 24;	// 이보다 작은 빈 터는 뚫지 않고 메운다
	constexpr int32 MAX_CONNECT_PASS = 256;
	constexpr int32 MAX_PICK_TRY = 4000;

	// 성채
	constexpr float WALL_INNER_RADIUS = 20.f;
	constexpr float WALL_OUTER_RADIUS = 22.f;
	constexpr float TOWN_CLEAR_RADIUS = 25.f;	// 성벽 밖 빈 고리. 어느 성문으로 나가도 돌아 들어올 수 있다
	constexpr float PLAZA_RADIUS = 5.f;
	constexpr float HOUSE_MIN_RADIUS = 7.5f;
	constexpr float HOUSE_MAX_RADIUS = 16.5f;	// 성벽 안쪽에 한 바퀴 도는 길을 남긴다
	constexpr int32 HOUSE_LATTICE = 4;	// 집은 최대 3 칸이라 사이에 한 칸 골목이 꼭 남는다
	constexpr int32 GATE_HALF_WIDTH = 1;
	constexpr int32 GATE_ROAD_LENGTH = 34;	// 성문 밖 곧은 길 끝 ( 광장 기준 )

	// 거점
	constexpr int32 OUTPOST_COUNT = 5;
	constexpr int32 HIDEOUT_COUNT = 2;
	constexpr int32 VILLAGE_COUNT = 3;
	constexpr float OUTPOST_SPACING = 30.f;
	constexpr float HIDEOUT_SPACING = 45.f;
	constexpr float VILLAGE_SPACING = 24.f;
	constexpr int32 OUTPOST_CLEAR_RADIUS = 3;
	constexpr int32 HIDEOUT_CLEAR_RADIUS = 4;
	constexpr int32 VILLAGE_CLEAR_RADIUS = 6;
	constexpr int32 LAIR_CLEAR_RADIUS = 5;
	constexpr float LAIR_WALK_RATIO = 0.78f;	// 제일 먼 칸에 두면 구역 경계에 붙는다
	constexpr int32 LAIR_EDGE_MARGIN = 24;

	// 몬스터 무리
	constexpr int32 PACK_MIN_WALK = 30;	// 성문 앞 몇 걸음은 비워 둔다
	constexpr int32 BAND_FIELD_WALK = 55;
	constexpr int32 BAND_OUTER_WALK = 105;
	constexpr int32 PACK_INTRO_COUNT = 6;
	constexpr int32 PACK_FIELD_COUNT = 10;
	constexpr int32 PACK_OUTER_COUNT = 12;
	constexpr int32 PACK_COUNT = PACK_INTRO_COUNT + PACK_FIELD_COUNT + PACK_OUTER_COUNT;
	constexpr float PACK_SPACING = 10.f;
	constexpr float PACK_INTRO_SPACING = 8.f;
	constexpr float PACK_SITE_SPACING = 20.f;	// 배회 3 + 어그로 8 + 여유. 이보다 가까우면 거점이 늘 싸움판이다
	constexpr float PACK_LAIR_SPACING = 14.f;

	// 지형
	constexpr int32 FOREST_MAX_SIZE = 250;
	constexpr float SCATTER_TREE_CHANCE = 0.025f;
	constexpr float SCATTER_ROCK_CHANCE = 0.010f;
	constexpr float BLIGHT_START_RATIO = 0.6f;	// 가장 먼 거리의 이 비율부터 재에 물든다
	constexpr int32 HERB_MAX_COUNT = 50;
	constexpr int32 RELIC_PER_VILLAGE = 4;
	constexpr int32 RELIC_WILD_COUNT = 18;

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
		const int32 limit = ZONE_TILE_COUNT - BORDER;

		if (x < BORDER || y < BORDER)
		{
			return true;
		}

		if (x >= limit || y >= limit)
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

	float CenterDistance(int32 x, int32 y, const ZoneTile& center)
	{
		ZoneTile tile;
		tile.x = x;
		tile.y = y;

		return TileDistance(tile, center);
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

	// 칸 해시를 몇 칸 단위로 부드럽게 이은 값. 얼룩 크기가 주기만큼 커진다
	float SmoothNoise(uint32 seed, int32 x, int32 y, int32 period)
	{
		const int32 cell_x = x / period;
		const int32 cell_y = y / period;
		const float local_x = static_cast<float>(x - cell_x * period) / static_cast<float>(period);
		const float local_y = static_cast<float>(y - cell_y * period) / static_cast<float>(period);

		const float corner_00 = HashToUnit(HashZoneTile(seed, cell_x, cell_y));
		const float corner_10 = HashToUnit(HashZoneTile(seed, cell_x + 1, cell_y));
		const float corner_01 = HashToUnit(HashZoneTile(seed, cell_x, cell_y + 1));
		const float corner_11 = HashToUnit(HashZoneTile(seed, cell_x + 1, cell_y + 1));

		const float lower = corner_00 + (corner_10 - corner_00) * local_x;
		const float upper = corner_01 + (corner_11 - corner_01) * local_x;

		return lower + (upper - lower) * local_y;
	}

	// 난수를 차례로 뽑지 않고 좌표로 해시해야 같은 씨앗에서 같은 지형이 나온다
	void FillNoise(uint32 seed, std::vector<uint8>* tiles)
	{
		// 테두리를 조건식으로 가려내면 Release 빌드에서 판정이 무너졌다 ( 원인 미확정 ).
		// 전체를 균열로 덮고 안쪽만 훑으면 그 비교 자체가 없어진다
		tiles->assign(tiles->size(), TILE_CLIFF);

		const int32 limit = ZONE_TILE_COUNT - BORDER;

		for (int32 y = BORDER; y < limit; ++y)
		{
			for (int32 x = BORDER; x < limit; ++x)
			{
				if (HashToUnit(HashZoneTile(seed, x, y)) < INITIAL_FILL)
				{
					continue;
				}

				(*tiles)[ToIndex(x, y)] = TILE_OPEN;
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

	void Smooth(std::vector<uint8>* tiles)
	{
		const std::vector<uint8> source = *tiles;
		const int32 limit = ZONE_TILE_COUNT - BORDER;

		for (int32 y = BORDER; y < limit; ++y)
		{
			for (int32 x = BORDER; x < limit; ++x)
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

	// 성채 둘레를 비우는 원이 성벽을 파고들면 안 된다
	void CarveCircle(std::vector<uint8>* tiles, const ZoneTile& center, int32 radius, const ZoneTile& town)
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

				if (CenterDistance(x, y, town) < TOWN_CLEAR_RADIUS + 1.f)
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

	int32 CountReachedTiles(const std::vector<int32>& distances)
	{
		int32 count = 0;

		for (int32 distance : distances)
		{
			if (distance >= 0)
			{
				count += 1;
			}
		}

		return count;
	}

	int32 FindMaxDistance(const std::vector<int32>& distances)
	{
		int32 best = 0;

		for (int32 distance : distances)
		{
			if (distance > best)
			{
				best = distance;
			}
		}

		return best;
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

	// 광장과 이어진 칸에 닿을 때까지 파 들어간다
	void CarveCorridor(std::vector<uint8>* tiles, const std::vector<int32>& reached, const ZoneTile& from, const ZoneTile& to)
	{
		ZoneTile current = from;
		bool moving_x = true;

		for (int32 guard = 0; guard < ZONE_TILE_COUNT * 4; ++guard)
		{
			const size_t index = ToIndex(current.x, current.y);
			if (reached[index] >= 0)
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

	void ConnectRegions(std::vector<uint8>* tiles, const ZoneTile& start)
	{
		std::vector<int32> reached;
		std::vector<int32> region_distances;

		for (int32 pass = 0; pass < MAX_CONNECT_PASS; ++pass)
		{
			MeasureDistance(*tiles, start, &reached);

			ZoneTile orphan;
			if (FindOrphan(*tiles, reached, &orphan) == false)
			{
				return;
			}

			MeasureDistance(*tiles, orphan, &region_distances);

			const int32 region_size = CountReachedTiles(region_distances);

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

			CarveCorridor(tiles, reached, orphan, start);
		}
	}

	// 연결 패스 횟수가 모자라도 갇힌 빈 칸이 절대 안 남게 마지막에 메운다
	void FillUnreached(std::vector<uint8>* tiles, const ZoneTile& start, uint8 fill)
	{
		std::vector<int32> distances;
		MeasureDistance(*tiles, start, &distances);

		for (size_t index = 0; index < tiles->size(); ++index)
		{
			if (IsBlockingTile((*tiles)[index]) == false && distances[index] < 0)
			{
				(*tiles)[index] = fill;
			}
		}
	}

	void GetGateAxis(size_t dir, int32* along_x, int32* along_y, int32* side_x, int32* side_y)
	{
		*along_x = STEP_X[dir];
		*along_y = STEP_Y[dir];
		*side_x = std::abs(STEP_Y[dir]);
		*side_y = std::abs(STEP_X[dir]);
	}

	// 성벽을 세우기 전에 성채 자리와 성문 밖 곧은 길을 비운다. 연결 보장이 이 빈터를 기준으로 돈다
	void ClearTownArea(ZoneLayout* layout)
	{
		const ZoneTile& center = layout->center;
		const int32 reach = static_cast<int32>(TOWN_CLEAR_RADIUS) + 1;

		for (int32 y = center.y - reach; y <= center.y + reach; ++y)
		{
			for (int32 x = center.x - reach; x <= center.x + reach; ++x)
			{
				if (IsInside(x, y) == false)
				{
					continue;
				}

				if (CenterDistance(x, y, center) < TOWN_CLEAR_RADIUS)
				{
					layout->tiles[ToIndex(x, y)] = TILE_OPEN;
				}
			}
		}

		for (size_t dir = 0; dir < STEP_X.size(); ++dir)
		{
			int32 along_x = 0;
			int32 along_y = 0;
			int32 side_x = 0;
			int32 side_y = 0;
			GetGateAxis(dir, &along_x, &along_y, &side_x, &side_y);

			for (int32 along = 0; along <= GATE_ROAD_LENGTH; ++along)
			{
				for (int32 side = -GATE_HALF_WIDTH; side <= GATE_HALF_WIDTH; ++side)
				{
					const int32 x = center.x + along_x * along + side_x * side;
					const int32 y = center.y + along_y * along + side_y * side;

					if (IsInside(x, y) && IsBorder(x, y) == false)
					{
						layout->tiles[ToIndex(x, y)] = TILE_OPEN;
					}
				}
			}
		}
	}

	void BuildWallRing(ZoneLayout* layout)
	{
		const ZoneTile& center = layout->center;
		const int32 reach = static_cast<int32>(WALL_OUTER_RADIUS) + 1;

		for (int32 y = center.y - reach; y <= center.y + reach; ++y)
		{
			for (int32 x = center.x - reach; x <= center.x + reach; ++x)
			{
				const float distance = CenterDistance(x, y, center);

				if (distance >= WALL_INNER_RADIUS && distance < WALL_OUTER_RADIUS)
				{
					layout->tiles[ToIndex(x, y)] = TILE_WALL;
				}
				else if (distance < PLAZA_RADIUS)
				{
					layout->tiles[ToIndex(x, y)] = TILE_PLAZA;
				}
			}
		}
	}

	// 성문 넷. 성 안은 돌길, 성문과 그 밖은 흙길, 성문 양옆 벽은 망루
	void BuildGates(ZoneLayout* layout)
	{
		const ZoneTile& center = layout->center;

		for (size_t dir = 0; dir < STEP_X.size(); ++dir)
		{
			int32 along_x = 0;
			int32 along_y = 0;
			int32 side_x = 0;
			int32 side_y = 0;
			GetGateAxis(dir, &along_x, &along_y, &side_x, &side_y);

			for (int32 along = 0; along <= GATE_ROAD_LENGTH; ++along)
			{
				for (int32 side = -3; side <= 3; ++side)
				{
					const int32 x = center.x + along_x * along + side_x * side;
					const int32 y = center.y + along_y * along + side_y * side;

					if (IsInside(x, y) == false || IsBorder(x, y))
					{
						continue;
					}

					const size_t index = ToIndex(x, y);
					const float distance = CenterDistance(x, y, center);

					if (std::abs(side) > GATE_HALF_WIDTH)
					{
						if (layout->tiles[index] == TILE_WALL)
						{
							layout->tiles[index] = TILE_TOWER;
						}

						continue;
					}

					if (distance < WALL_INNER_RADIUS)
					{
						layout->tiles[index] = TILE_PLAZA;
					}
					else
					{
						layout->tiles[index] = TILE_ROAD;
					}
				}
			}

			ZoneSite gate;
			gate.kind = SITE_GATE;
			gate.tile.x = center.x + along_x * static_cast<int32>(WALL_OUTER_RADIUS + 2.f);
			gate.tile.y = center.y + along_y * static_cast<int32>(WALL_OUTER_RADIUS + 2.f);
			layout->sites.push_back(gate);
		}
	}

	bool CanPlaceTownLot(const ZoneLayout& layout, int32 base_x, int32 base_y, int32 size_x, int32 size_y)
	{
		const ZoneTile& center = layout.center;

		for (int32 y = base_y; y < base_y + size_y; ++y)
		{
			for (int32 x = base_x; x < base_x + size_x; ++x)
			{
				if (IsInside(x, y) == false)
				{
					return false;
				}

				const float distance = CenterDistance(x, y, center);
				if (distance < HOUSE_MIN_RADIUS || distance > HOUSE_MAX_RADIUS)
				{
					return false;
				}

				// 성문으로 가는 큰길에서 한 칸 띄운다
				if (std::abs(x - center.x) < 3 || std::abs(y - center.y) < 3)
				{
					return false;
				}

				if (layout.tiles[ToIndex(x, y)] != TILE_OPEN)
				{
					return false;
				}
			}
		}

		return true;
	}

	void FillLot(ZoneLayout* layout, int32 base_x, int32 base_y, int32 size_x, int32 size_y, uint8 tile)
	{
		for (int32 y = base_y; y < base_y + size_y; ++y)
		{
			for (int32 x = base_x; x < base_x + size_x; ++x)
			{
				layout->tiles[ToIndex(x, y)] = tile;
			}
		}
	}

	// 격자 칸마다 집 / 밭 / 빈 마당 중 하나. 칸 간격이 4 라 골목이 끊기지 않는다
	void BuildHouses(uint32 seed, ZoneLayout* layout)
	{
		const ZoneTile& center = layout->center;
		const int32 reach = static_cast<int32>(HOUSE_MAX_RADIUS) + 1;

		for (int32 base_y = center.y - reach; base_y <= center.y + reach; base_y += HOUSE_LATTICE)
		{
			for (int32 base_x = center.x - reach; base_x <= center.x + reach; base_x += HOUSE_LATTICE)
			{
				const uint32 hash = HashZoneTile(seed + 606u, base_x, base_y);
				const float roll = HashToUnit(hash);

				if (roll < 0.18f)
				{
					if (CanPlaceTownLot(*layout, base_x, base_y, 3, 3))
					{
						FillLot(layout, base_x, base_y, 3, 3, TILE_FIELD);
					}

					continue;
				}

				if (roll > 0.82f)
				{
					continue;
				}

				const int32 size_x = 2 + static_cast<int32>((hash >> 17) & 1u);
				const int32 size_y = 2 + static_cast<int32>((hash >> 19) & 1u);

				if (CanPlaceTownLot(*layout, base_x, base_y, size_x, size_y))
				{
					FillLot(layout, base_x, base_y, size_x, size_y, TILE_HOUSE);
				}
			}
		}
	}

	void AddLamp(ZoneLayout* layout, int32 x, int32 y)
	{
		if (IsOpenAt(layout->tiles, x, y) == false)
		{
			return;
		}

		ZoneTile lamp;
		lamp.x = x;
		lamp.y = y;
		layout->lamps.push_back(lamp);
	}

	void PlaceLamps(ZoneLayout* layout)
	{
		const ZoneTile& center = layout->center;

		AddLamp(layout, center.x + 3, center.y + 3);
		AddLamp(layout, center.x - 3, center.y - 3);
		AddLamp(layout, center.x + 3, center.y - 3);
		AddLamp(layout, center.x - 3, center.y + 3);

		for (size_t dir = 0; dir < STEP_X.size(); ++dir)
		{
			int32 along_x = 0;
			int32 along_y = 0;
			int32 side_x = 0;
			int32 side_y = 0;
			GetGateAxis(dir, &along_x, &along_y, &side_x, &side_y);

			const int32 along = 12;
			AddLamp(layout, center.x + along_x * along + side_x * 2, center.y + along_y * along + side_y * 2);
		}
	}

	bool IsFarFromSites(const ZoneTile& tile, const std::vector<ZoneSite>& sites, int32 kind, float spacing)
	{
		for (const ZoneSite& site : sites)
		{
			if (kind >= 0 && site.kind != kind)
			{
				continue;
			}

			if (TileDistance(tile, site.tile) < spacing)
			{
				return false;
			}
		}

		return true;
	}

	// 걸어서 제일 먼 칸은 거의 언제나 구역 가장자리다. 최대 거리의 일정 비율 지점을 고른다
	ZoneTile FindWalkRatioTile(const std::vector<int32>& distances, const ZoneTile& fallback, float ratio)
	{
		const int32 target = static_cast<int32>(static_cast<float>(FindMaxDistance(distances)) * ratio);

		ZoneTile found = fallback;
		int32 closest_gap = ZONE_TILE_COUNT * ZONE_TILE_COUNT;
		const int32 edge_limit = ZONE_TILE_COUNT - LAIR_EDGE_MARGIN;

		for (int32 y = LAIR_EDGE_MARGIN; y < edge_limit; ++y)
		{
			for (int32 x = LAIR_EDGE_MARGIN; x < edge_limit; ++x)
			{
				const int32 distance = distances[ToIndex(x, y)];

				if (distance < 0)
				{
					continue;
				}

				const int32 gap = std::abs(distance - target);

				if (gap < closest_gap)
				{
					closest_gap = gap;
					found.x = x;
					found.y = y;
				}
			}
		}

		return found;
	}

	void PickSites(uint32* random_state, const std::vector<int32>& distances, int32 kind, int32 count, int32 min_walk, int32 max_walk, float spacing, ZoneLayout* layout)
	{
		int32 placed = 0;

		for (int32 attempt = 0; attempt < MAX_PICK_TRY && placed < count; ++attempt)
		{
			const ZoneTile tile = PickRandomTile(random_state);

			if (IsOpenAt(layout->tiles, tile.x, tile.y) == false)
			{
				continue;
			}

			const int32 walk = distances[ToIndex(tile.x, tile.y)];
			if (walk < min_walk || walk > max_walk)
			{
				continue;
			}

			if (IsFarFromSites(tile, layout->sites, kind, spacing) == false)
			{
				continue;
			}

			// 거점끼리는 종류가 달라도 붙지 않게
			if (IsFarFromSites(tile, layout->sites, -1, 20.f) == false)
			{
				continue;
			}

			ZoneSite site;
			site.tile = tile;
			site.kind = kind;
			site.walk_distance = walk;
			layout->sites.push_back(site);

			placed += 1;
		}
	}

	int32 GetSiteClearRadius(int32 kind)
	{
		switch (kind)
		{
			case SITE_OUTPOST:
			{
				return OUTPOST_CLEAR_RADIUS;
			}
			case SITE_HIDEOUT:
			{
				return HIDEOUT_CLEAR_RADIUS;
			}
			case SITE_VILLAGE:
			{
				return VILLAGE_CLEAR_RADIUS;
			}
			case SITE_LAIR:
			{
				return LAIR_CLEAR_RADIUS;
			}
			default:
			{
				return 0;
			}
		}
	}

	// 테두리에 붙은 막힌 덩어리는 균열, 가운데 떠 있으면 크기에 따라 바위 / 죽은 숲 / 늪
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

				const int32 size = static_cast<int32>(region.size());
				const float roll = HashToUnit(HashZoneTile(seed + 404u, start.x, start.y));

				uint8 kind = TILE_CLIFF;

				if (size <= 3)
				{
					kind = TILE_ROCK;
				}
				else if (size <= FOREST_MAX_SIZE)
				{
					if (roll < 0.5f)
					{
						kind = TILE_TREE;
					}
					else if (roll < 0.8f)
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

	// 벽 조각 둘레 한 칸이 전부 비어 있으면 조각은 섬이라 길을 못 끊는다
	bool CanPlaceSegment(const std::vector<uint8>& tiles, const std::vector<ZoneTile>& segment)
	{
		for (const ZoneTile& tile : segment)
		{
			for (int32 offset_y = -1; offset_y <= 1; ++offset_y)
			{
				for (int32 offset_x = -1; offset_x <= 1; ++offset_x)
				{
					if (IsOpenAt(tiles, tile.x + offset_x, tile.y + offset_y) == false)
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	void PlaceRuins(uint32* random_state, ZoneLayout* layout)
	{
		for (const ZoneSite& site : layout->sites)
		{
			if (site.kind != SITE_VILLAGE)
			{
				continue;
			}

			int32 placed = 0;

			for (int32 attempt = 0; attempt < 40 && placed < 7; ++attempt)
			{
				ZoneTile start;
				start.x = site.tile.x + static_cast<int32>(NextRandom(random_state) % 15u) - 7;
				start.y = site.tile.y + static_cast<int32>(NextRandom(random_state) % 15u) - 7;

				const int32 length = 2 + static_cast<int32>(NextRandom(random_state) % 3u);
				const bool along_x = (NextRandom(random_state) & 1u) == 0;
				const bool bend = (NextRandom(random_state) % 3u) == 0;

				std::vector<ZoneTile> segment;

				for (int32 step = 0; step < length; ++step)
				{
					ZoneTile tile = start;
					if (along_x)
					{
						tile.x += step;
					}
					else
					{
						tile.y += step;
					}

					segment.push_back(tile);
				}

				// ㄱ 자로 꺾인 담
				if (bend)
				{
					ZoneTile corner = segment.back();
					if (along_x)
					{
						corner.y += 1;
					}
					else
					{
						corner.x += 1;
					}

					segment.push_back(corner);
				}

				if (CanPlaceSegment(layout->tiles, segment) == false)
				{
					continue;
				}

				for (const ZoneTile& tile : segment)
				{
					layout->tiles[ToIndex(tile.x, tile.y)] = TILE_RUIN;
				}

				placed += 1;
			}
		}
	}

	void ScatterObstacles(uint32 seed, ZoneLayout* layout)
	{
		for (int32 y = BORDER; y < ZONE_TILE_COUNT - BORDER; ++y)
		{
			for (int32 x = BORDER; x < ZONE_TILE_COUNT - BORDER; ++x)
			{
				if (layout->tiles[ToIndex(x, y)] != TILE_OPEN)
				{
					continue;
				}

				if (CenterDistance(x, y, layout->center) < TOWN_CLEAR_RADIUS + 3.f)
				{
					continue;
				}

				ZoneTile tile;
				tile.x = x;
				tile.y = y;

				if (IsFarFromSites(tile, layout->sites, -1, 7.f) == false)
				{
					continue;
				}

				const float roll = HashToUnit(HashZoneTile(seed + 505u, x, y));

				if (roll >= SCATTER_TREE_CHANCE + SCATTER_ROCK_CHANCE)
				{
					continue;
				}

				if (IsIsolatedSpot(layout->tiles, x, y) == false)
				{
					continue;
				}

				if (roll < SCATTER_TREE_CHANCE)
				{
					layout->tiles[ToIndex(x, y)] = TILE_TREE;
				}
				else
				{
					layout->tiles[ToIndex(x, y)] = TILE_ROCK;
				}
			}
		}
	}

	void PaintRoadTile(ZoneLayout* layout, int32 x, int32 y)
	{
		if (IsInside(x, y) == false)
		{
			return;
		}

		const size_t index = ToIndex(x, y);
		const uint8 tile = layout->tiles[index];

		if (tile == TILE_OPEN || tile == TILE_BLIGHT)
		{
			layout->tiles[index] = TILE_ROAD;
		}
	}

	// 거점에서 광장 쪽으로 거리가 줄어드는 칸만 밟아 내려간다. 그게 곧 가장 짧은 교역로다
	void PaintRoadFrom(const ZoneTile& from, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		ZoneTile current = from;
		size_t last_dir = 0;

		for (int32 guard = 0; guard < ZONE_TILE_COUNT * 4; ++guard)
		{
			const int32 distance = distances[ToIndex(current.x, current.y)];
			if (distance <= 0)
			{
				return;
			}

			if (CenterDistance(current.x, current.y, layout->center) < WALL_INNER_RADIUS)
			{
				return;
			}

			PaintRoadTile(layout, current.x, current.y);

			// 가던 방향을 먼저 본다. 안 그러면 계단처럼 지그재그로 꺾인다
			bool moved = false;

			for (size_t attempt = 0; attempt < STEP_X.size(); ++attempt)
			{
				const size_t dir = (last_dir + attempt) % STEP_X.size();
				const int32 next_x = current.x + STEP_X[dir];
				const int32 next_y = current.y + STEP_Y[dir];

				if (IsInside(next_x, next_y) == false)
				{
					continue;
				}

				if (distances[ToIndex(next_x, next_y)] != distance - 1)
				{
					continue;
				}

				// 폭 2
				PaintRoadTile(layout, current.x + std::abs(STEP_Y[dir]), current.y + std::abs(STEP_X[dir]));

				current.x = next_x;
				current.y = next_y;
				last_dir = dir;
				moved = true;
				break;
			}

			if (moved == false)
			{
				return;
			}
		}
	}

	// 멀어질수록 바닥이 재에 물든다. 거리가 곧 위험이라는 걸 바닥색이 먼저 말한다
	void PaintBlight(uint32 seed, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		const float max_walk = static_cast<float>(FindMaxDistance(distances));
		if (max_walk <= 0.f)
		{
			return;
		}

		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const size_t index = ToIndex(x, y);
				if (layout->tiles[index] != TILE_OPEN || distances[index] < 0)
				{
					continue;
				}

				const float ratio = static_cast<float>(distances[index]) / max_walk;
				if (ratio < BLIGHT_START_RATIO)
				{
					continue;
				}

				const float noise = SmoothNoise(seed + 707u, x, y, 7);
				const float threshold = 0.62f - (ratio - BLIGHT_START_RATIO) * 0.9f;

				if (noise > threshold)
				{
					layout->tiles[index] = TILE_BLIGHT;
				}
			}
		}

		// 둥지 둘레는 짙게
		for (int32 offset_y = -9; offset_y <= 9; ++offset_y)
		{
			for (int32 offset_x = -9; offset_x <= 9; ++offset_x)
			{
				const int32 x = layout->boss.x + offset_x;
				const int32 y = layout->boss.y + offset_y;

				if (IsInside(x, y) == false || offset_x * offset_x + offset_y * offset_y > 81)
				{
					continue;
				}

				const size_t index = ToIndex(x, y);
				if (layout->tiles[index] == TILE_OPEN)
				{
					layout->tiles[index] = TILE_BLIGHT;
				}
			}
		}
	}

	// 폐촌 둘레 말라 죽은 밭
	void PaintVillageFields(uint32 seed, ZoneLayout* layout)
	{
		for (const ZoneSite& site : layout->sites)
		{
			if (site.kind != SITE_VILLAGE)
			{
				continue;
			}

			for (int32 offset_y = -9; offset_y <= 9; ++offset_y)
			{
				for (int32 offset_x = -9; offset_x <= 9; ++offset_x)
				{
					const int32 x = site.tile.x + offset_x;
					const int32 y = site.tile.y + offset_y;

					if (IsInside(x, y) == false || layout->tiles[ToIndex(x, y)] != TILE_OPEN)
					{
						continue;
					}

					if (SmoothNoise(seed + 808u, x, y, 4) > 0.6f)
					{
						layout->tiles[ToIndex(x, y)] = TILE_FIELD;
					}
				}
			}
		}
	}

	int32 GetBandOf(int32 walk_distance)
	{
		if (walk_distance < BAND_FIELD_WALK)
		{
			return BAND_INTRO;
		}

		if (walk_distance < BAND_OUTER_WALK)
		{
			return BAND_FIELD;
		}

		return BAND_OUTER;
	}

	int32 GetBandPackCount(int32 band)
	{
		if (band == BAND_INTRO)
		{
			return PACK_INTRO_COUNT;
		}

		if (band == BAND_FIELD)
		{
			return PACK_FIELD_COUNT;
		}

		return PACK_OUTER_COUNT;
	}

	float GetBandPackSpacing(int32 band)
	{
		if (band == BAND_INTRO)
		{
			return PACK_INTRO_SPACING;
		}

		return PACK_SPACING;
	}

	int32 CountPacksInBand(const std::vector<ZonePack>& packs, int32 band)
	{
		int32 count = 0;

		for (const ZonePack& pack : packs)
		{
			if (pack.band == band)
			{
				count += 1;
			}
		}

		return count;
	}

	bool IsFarFromPacks(const ZoneTile& tile, const std::vector<ZonePack>& packs, float spacing)
	{
		for (const ZonePack& pack : packs)
		{
			if (TileDistance(tile, pack.tile) < spacing)
			{
				return false;
			}
		}

		return true;
	}

	void PickPacks(uint32* random_state, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		for (int32 attempt = 0; attempt < MAX_PICK_TRY * 2; ++attempt)
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

			const int32 walk_distance = distances[ToIndex(tile.x, tile.y)];
			if (walk_distance < PACK_MIN_WALK)
			{
				continue;
			}

			if (TileDistance(tile, layout->boss) < PACK_LAIR_SPACING)
			{
				continue;
			}

			if (IsFarFromSites(tile, layout->sites, SITE_OUTPOST, PACK_SITE_SPACING) == false)
			{
				continue;
			}

			if (IsFarFromSites(tile, layout->sites, SITE_HIDEOUT, PACK_SITE_SPACING) == false)
			{
				continue;
			}

			// 띠마다 정원이 따로다. 안 나누면 넓은 바깥 띠가 무리를 다 가져간다
			const int32 band = GetBandOf(walk_distance);
			if (CountPacksInBand(layout->packs, band) >= GetBandPackCount(band))
			{
				continue;
			}

			if (IsFarFromPacks(tile, layout->packs, GetBandPackSpacing(band)) == false)
			{
				continue;
			}

			ZonePack pack;
			pack.tile = tile;
			pack.band = band;

			layout->packs.push_back(pack);
		}
	}

	bool HasNeighborTile(const std::vector<uint8>& tiles, int32 x, int32 y, uint8 kind)
	{
		for (size_t dir = 0; dir < STEP_X.size(); ++dir)
		{
			const int32 near_x = x + STEP_X[dir];
			const int32 near_y = y + STEP_Y[dir];

			if (IsInside(near_x, near_y) && tiles[ToIndex(near_x, near_y)] == kind)
			{
				return true;
			}
		}

		return false;
	}

	bool IsFarFromTiles(const ZoneTile& tile, const std::vector<ZoneTile>& others, float spacing)
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

	// 줄 순서로 훑다 정원을 채우면 약초가 지도 위쪽에만 몰린다. 후보를 다 모아 섞고 고른다
	void PickHerbs(uint32* random_state, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		std::vector<ZoneTile> candidates;

		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const size_t index = ToIndex(x, y);
				if (layout->tiles[index] != TILE_OPEN || distances[index] < PACK_MIN_WALK)
				{
					continue;
				}

				if (HasNeighborTile(layout->tiles, x, y, TILE_WATER) == false)
				{
					continue;
				}

				ZoneTile tile;
				tile.x = x;
				tile.y = y;
				candidates.push_back(tile);
			}
		}

		for (size_t index = candidates.size(); index > 1; --index)
		{
			const size_t swap_index = static_cast<size_t>(NextRandom(random_state) % static_cast<uint32>(index));
			std::swap(candidates[index - 1], candidates[swap_index]);
		}

		for (const ZoneTile& tile : candidates)
		{
			if (static_cast<int32>(layout->herbs.size()) >= HERB_MAX_COUNT)
			{
				return;
			}

			if (IsFarFromTiles(tile, layout->herbs, 5.f))
			{
				layout->herbs.push_back(tile);
			}
		}
	}

	bool IsNextToRuin(const std::vector<uint8>& tiles, int32 x, int32 y)
	{
		for (int32 offset_y = -1; offset_y <= 1; ++offset_y)
		{
			for (int32 offset_x = -1; offset_x <= 1; ++offset_x)
			{
				const int32 near_x = x + offset_x;
				const int32 near_y = y + offset_y;

				if (IsInside(near_x, near_y) && tiles[ToIndex(near_x, near_y)] == TILE_RUIN)
				{
					return true;
				}
			}
		}

		return false;
	}

	void PickRelics(uint32 seed, uint32* random_state, const std::vector<int32>& distances, ZoneLayout* layout)
	{
		for (const ZoneSite& site : layout->sites)
		{
			if (site.kind != SITE_VILLAGE)
			{
				continue;
			}

			int32 placed = 0;

			for (int32 offset_y = -8; offset_y <= 8 && placed < RELIC_PER_VILLAGE; ++offset_y)
			{
				for (int32 offset_x = -8; offset_x <= 8 && placed < RELIC_PER_VILLAGE; ++offset_x)
				{
					const int32 x = site.tile.x + offset_x;
					const int32 y = site.tile.y + offset_y;

					if (IsOpenAt(layout->tiles, x, y) == false || IsNextToRuin(layout->tiles, x, y) == false)
					{
						continue;
					}

					if (HashToUnit(HashZoneTile(seed + 1111u, x, y)) > 0.25f)
					{
						continue;
					}

					ZoneTile tile;
					tile.x = x;
					tile.y = y;

					if (IsFarFromTiles(tile, layout->relics, 3.f))
					{
						layout->relics.push_back(tile);
						placed += 1;
					}
				}
			}
		}

		// 들판에 흩어진 이전 원정대의 유해
		int32 wild = 0;

		for (int32 attempt = 0; attempt < MAX_PICK_TRY && wild < RELIC_WILD_COUNT; ++attempt)
		{
			const ZoneTile tile = PickRandomTile(random_state);

			if (IsOpenAt(layout->tiles, tile.x, tile.y) == false)
			{
				continue;
			}

			if (distances[ToIndex(tile.x, tile.y)] < 40)
			{
				continue;
			}

			if (IsFarFromTiles(tile, layout->relics, 12.f) == false)
			{
				continue;
			}

			layout->relics.push_back(tile);
			wild += 1;
		}
	}
}

void GenerateZone(uint32 seed, ZoneLayout* layout)
{
	layout->seed = seed;
	layout->tiles.assign(static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT), TILE_OPEN);
	layout->packs.clear();
	layout->sites.clear();
	layout->herbs.clear();
	layout->relics.clear();
	layout->lamps.clear();

	layout->center.x = ZONE_TILE_COUNT / 2;
	layout->center.y = ZONE_TILE_COUNT / 2;
	layout->spawn.x = layout->center.x + 2;
	layout->spawn.y = layout->center.y + 2;

	FillNoise(seed, &layout->tiles);

	for (int32 pass = 0; pass < SMOOTH_PASS_COUNT; ++pass)
	{
		Smooth(&layout->tiles);
	}

	// 성벽이 없을 때 연결을 보장하고 나서 성벽을 세운다. 성벽 밖 빈 고리가 모든 바깥 땅을 성문으로 잇는다
	ClearTownArea(layout);
	ConnectRegions(&layout->tiles, layout->center);
	FillUnreached(&layout->tiles, layout->center, TILE_CLIFF);

	BuildWallRing(layout);
	BuildGates(layout);
	BuildHouses(seed, layout);
	PlaceLamps(layout);

	ZoneSite town;
	town.tile = layout->center;
	town.kind = SITE_TOWN;
	layout->sites.push_back(town);

	std::vector<int32> distances;
	MeasureDistance(layout->tiles, layout->center, &distances);

	// 걸을 수 있는 땅이 갑자기 줄면 지형 생성이 깨진 것이다. 한 번 크게 당한 자리라 남겨 둔다
	std::cout << "[ ZoneGen - GenerateZone ] : zone " << ZONE_TILE_COUNT
		<< " walkable " << CountReachedTiles(distances)
		<< " max_walk " << FindMaxDistance(distances) << "\n";

	uint32 random_state = seed * 747796405u + 2891336453u;
	if (random_state == 0)
	{
		random_state = 1;
	}

	const int32 max_walk = FindMaxDistance(distances);

	layout->boss = FindWalkRatioTile(distances, layout->spawn, LAIR_WALK_RATIO);

	ZoneSite lair;
	lair.tile = layout->boss;
	lair.kind = SITE_LAIR;
	lair.walk_distance = distances[ToIndex(layout->boss.x, layout->boss.y)];
	layout->sites.push_back(lair);

	PickSites(&random_state, distances, SITE_HIDEOUT, HIDEOUT_COUNT, max_walk * 45 / 100, max_walk * 90 / 100, HIDEOUT_SPACING, layout);
	PickSites(&random_state, distances, SITE_OUTPOST, OUTPOST_COUNT, 40, max_walk * 85 / 100, OUTPOST_SPACING, layout);
	PickSites(&random_state, distances, SITE_VILLAGE, VILLAGE_COUNT, 30, max_walk * 80 / 100, VILLAGE_SPACING, layout);

	for (const ZoneSite& site : layout->sites)
	{
		const int32 radius = GetSiteClearRadius(site.kind);
		if (radius > 0)
		{
			CarveCircle(&layout->tiles, site.tile, radius, layout->center);
		}
	}

	ClassifyBlockedRegions(seed, &layout->tiles);
	PlaceRuins(&random_state, layout);
	ScatterObstacles(seed, layout);

	MeasureDistance(layout->tiles, layout->center, &distances);

	for (const ZoneSite& site : layout->sites)
	{
		if (site.kind == SITE_OUTPOST || site.kind == SITE_VILLAGE)
		{
			PaintRoadFrom(site.tile, distances, layout);
		}
	}

	PaintBlight(seed, distances, layout);
	PaintVillageFields(seed, layout);

	// 칠하기만 했지 막힘은 안 바꿨다. 그래도 보장을 한 번 더 건다 ( 성 안 막힌 틈은 바위로 메운다 )
	FillUnreached(&layout->tiles, layout->center, TILE_ROCK);
	MeasureDistance(layout->tiles, layout->center, &distances);

	for (ZoneSite& site : layout->sites)
	{
		site.walk_distance = distances[ToIndex(site.tile.x, site.tile.y)];
	}

	PickPacks(&random_state, distances, layout);
	PickHerbs(&random_state, distances, layout);
	PickRelics(seed, &random_state, distances, layout);

	layout->walk_distance = distances;

	std::cout << "[ ZoneGen - GenerateZone ] : sites " << layout->sites.size()
		<< " packs " << layout->packs.size()
		<< " herbs " << layout->herbs.size()
		<< " relics " << layout->relics.size() << "\n";
}

bool IsBlockingTile(uint8 tile)
{
	return tile >= TILE_CLIFF;
}

uint8 GetZoneTile(const ZoneLayout& layout, int32 x, int32 y)
{
	if (IsInside(x, y) == false)
	{
		return TILE_CLIFF;
	}

	return layout.tiles[ToIndex(x, y)];
}

int32 GetZoneWalkDistance(const ZoneLayout& layout, int32 x, int32 y)
{
	if (IsInside(x, y) == false || layout.walk_distance.empty())
	{
		return -1;
	}

	return layout.walk_distance[ToIndex(x, y)];
}

bool IsInsideTown(const ZoneLayout& layout, int32 x, int32 y)
{
	return CenterDistance(x, y, layout.center) < WALL_INNER_RADIUS;
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
