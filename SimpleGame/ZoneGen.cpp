#include "stdafx.h"
#include "ZoneGen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>

namespace
{
	const float PI = 3.14159265f;

	constexpr int32 BORDER = 2;
	constexpr float INITIAL_FILL = 0.40f;
	constexpr int32 SMOOTH_PASS_COUNT = 4;
	constexpr int32 MIN_REGION_SIZE = 24;	// 이보다 작은 빈 터는 뚫지 않고 메운다
	constexpr int32 MAX_PICK_TRY = 6000;

	// 성채
	constexpr float WALL_INNER_RADIUS = 22.f;
	constexpr float WALL_OUTER_RADIUS = 24.f;
	constexpr float TOWN_CLEAR_RADIUS = 27.f;	// 성벽 밖 빈 고리. 어느 성문으로 나가도 돌아 들어올 수 있다
	constexpr float PLAZA_RADIUS = 5.f;
	constexpr float CITADEL_HOUSE_MIN = 7.5f;
	constexpr float CITADEL_HOUSE_MAX = 19.5f;	// 성벽 안쪽에 한 바퀴 도는 길을 남긴다
	constexpr int32 CITADEL_RADIUS = 24;
	constexpr int32 GATE_HALF_WIDTH = 1;
	constexpr int32 GATE_ROAD_LENGTH = 36;	// 성문 밖 곧은 길 끝 ( 광장 기준 )

	// 개척촌. 이웃 마을 경계에서 경계까지 초속 4 칸으로 20 초 ( 80 칸 ) 안팎
	constexpr float HAMLET_RING_RADIUS = 124.f;	// 118 일 때 경계 사이 길이 62 ~ 86 칸 ( 평균 72 ) 이라 늘렸다
	constexpr float HAMLET_RING_JITTER = 5.f;
	constexpr float HAMLET_ANGLE_JITTER = 0.09f;
	constexpr float HAMLET_CLEAR_RADIUS = 24.f;
	constexpr float HAMLET_PLAZA_RADIUS = 4.5f;
	constexpr float HAMLET_HOUSE_MIN = 6.5f;
	constexpr float HAMLET_HOUSE_MAX = 19.5f;
	constexpr int32 HAMLET_RADIUS = 22;
	constexpr int32 HOUSE_LATTICE = 4;	// 집은 최대 3 칸이라 사이에 한 칸 골목이 꼭 남는다
	constexpr int32 HOUSES_PER_VILLAGE = 36;	// 주민 30 + 이사 올 빈집
	constexpr int32 STREET_KEEP_CLEAR = 2;	// 길 칸 둘레 이만큼은 집을 안 짓는다

	// 교역로
	constexpr float ROAD_CARVE_RADIUS = 1.7f;
	constexpr float ROAD_BEND = 9.f;
	constexpr float ROAD_WIGGLE = 2.5f;
	constexpr float ROAD_SAMPLE_STEP = 0.4f;
	constexpr float OUTPOST_ROAD_OFFSET = 5.f;	// 길 바로 위가 아니라 길가에 친다

	// 거점
	constexpr int32 HIDEOUT_COUNT = 3;
	constexpr int32 RUIN_COUNT = 4;
	constexpr float HIDEOUT_SPACING = 60.f;
	constexpr float RUIN_SPACING = 40.f;
	constexpr float HIDEOUT_VILLAGE_CLEARANCE = 50.f;
	constexpr float RUIN_VILLAGE_CLEARANCE = 44.f;
	constexpr float HIDEOUT_ROAD_CLEARANCE = 14.f;
	constexpr float RUIN_ROAD_CLEARANCE = 8.f;
	constexpr int32 OUTPOST_CLEAR_RADIUS = 3;
	constexpr int32 HIDEOUT_CLEAR_RADIUS = 4;
	constexpr int32 RUIN_CLEAR_RADIUS = 6;
	constexpr int32 LAIR_CLEAR_RADIUS = 5;
	constexpr float LAIR_WALK_RATIO = 0.8f;	// 제일 먼 칸에 두면 구역 경계에 붙는다
	constexpr float LAIR_VILLAGE_CLEARANCE = 60.f;
	constexpr int32 LAIR_EDGE_MARGIN = 24;

	// 몬스터 무리. 마을 경계 가까이는 성문 앞 띠, 마을 사이는 들판, 고리 바깥 먼 곳은 깊은 곳.
	// 성채에서 걸은 거리로만 가르면 개척촌 여섯이 통째로 들판 한복판이라 주민이 209 초에 109 명 죽었다
	constexpr int32 PACK_MIN_WALK = 36;	// 성문 앞 몇 걸음은 비워 둔다
	constexpr float BAND_INTRO_EDGE = 40.f;	// 마을 경계에서 이 안은 타락자뿐
	constexpr int32 BAND_OUTER_WALK = 170;	// 개척촌 고리 ( 124 + 22 ) 바깥
	constexpr int32 PACK_INTRO_COUNT = 16;
	constexpr int32 PACK_FIELD_COUNT = 20;
	constexpr int32 PACK_OUTER_COUNT = 16;
	constexpr int32 PACK_COUNT = PACK_INTRO_COUNT + PACK_FIELD_COUNT + PACK_OUTER_COUNT;
	constexpr float PACK_SPACING = 10.f;
	constexpr float PACK_INTRO_SPACING = 8.f;
	constexpr float PACK_SITE_SPACING = 20.f;	// 배회 3 + 어그로 8 + 여유. 이보다 가까우면 거점이 늘 싸움판이다
	constexpr float PACK_VILLAGE_SPACING = 18.f;	// 마을 경계에서
	constexpr float PACK_LAIR_SPACING = 14.f;

	// 지형
	constexpr int32 FOREST_MAX_SIZE = 250;
	constexpr float SCATTER_TREE_CHANCE = 0.025f;
	constexpr float SCATTER_ROCK_CHANCE = 0.010f;
	constexpr float BLIGHT_START_RATIO = 0.6f;	// 가장 먼 거리의 이 비율부터 재에 물든다
	constexpr int32 HERB_MAX_COUNT = 100;
	constexpr int32 RELIC_PER_RUIN = 4;
	constexpr int32 RELIC_WILD_COUNT = 30;

	const std::array<int32, 4> STEP_X = { 1, -1, 0, 0 };
	const std::array<int32, 4> STEP_Y = { 0, 0, 1, -1 };

	struct RoadPoint
	{
		float x = 0.f;
		float y = 0.f;
	};

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

	void ClearDisk(std::vector<uint8>* tiles, float center_x, float center_y, float radius)
	{
		const int32 min_x = static_cast<int32>(std::floor(center_x - radius));
		const int32 max_x = static_cast<int32>(std::ceil(center_x + radius));
		const int32 min_y = static_cast<int32>(std::floor(center_y - radius));
		const int32 max_y = static_cast<int32>(std::ceil(center_y + radius));

		for (int32 y = min_y; y <= max_y; ++y)
		{
			for (int32 x = min_x; x <= max_x; ++x)
			{
				if (IsInside(x, y) == false || IsBorder(x, y))
				{
					continue;
				}

				const float delta_x = static_cast<float>(x) + 0.5f - center_x;
				const float delta_y = static_cast<float>(y) + 0.5f - center_y;

				if (delta_x * delta_x + delta_y * delta_y > radius * radius)
				{
					continue;
				}

				(*tiles)[ToIndex(x, y)] = TILE_OPEN;
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

	// 닿은 표시를 넓힌다. 거리는 안 재고 이어졌는지만 본다. 넘긴 목록이 있으면 새로 닿은 칸을 담는다
	int32 FloodMark(const std::vector<uint8>& tiles, const ZoneTile& start, uint8 mark, std::vector<uint8>* marks, std::vector<ZoneTile>* region)
	{
		if (IsOpenAt(tiles, start.x, start.y) == false || (*marks)[ToIndex(start.x, start.y)] == mark)
		{
			return 0;
		}

		std::deque<ZoneTile> queue;
		(*marks)[ToIndex(start.x, start.y)] = mark;
		queue.push_back(start);
		int32 count = 0;

		while (queue.empty() == false)
		{
			const ZoneTile current = queue.front();
			queue.pop_front();
			count += 1;

			if (region != nullptr)
			{
				region->push_back(current);
			}

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
				if ((*marks)[next_index] == mark)
				{
					continue;
				}

				(*marks)[next_index] = mark;
				queue.push_back(next);
			}
		}

		return count;
	}

	// 광장과 이어진 칸에 닿을 때까지 파 들어간다
	void CarveCorridor(std::vector<uint8>* tiles, const std::vector<uint8>& reached, const ZoneTile& from, const ZoneTile& to)
	{
		ZoneTile current = from;
		bool moving_x = true;

		for (int32 guard = 0; guard < ZONE_TILE_COUNT * 4; ++guard)
		{
			const size_t index = ToIndex(current.x, current.y);
			if (reached[index] == 1)
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

	// 갇힌 빈 터마다 광장 쪽으로 통로를 판다.
	// 매번 처음부터 다시 재면 구역이 커진 만큼 수백 번 전체를 훑는다. 판 통로에서 닿은 표시만 넓힌다
	void ConnectRegions(std::vector<uint8>* tiles, const ZoneTile& start)
	{
		std::vector<uint8> reached(tiles->size(), 0);
		FloodMark(*tiles, start, 1, &reached, nullptr);

		std::vector<uint8> scratch(tiles->size(), 0);
		std::vector<ZoneTile> region;

		for (int32 y = 0; y < ZONE_TILE_COUNT; ++y)
		{
			for (int32 x = 0; x < ZONE_TILE_COUNT; ++x)
			{
				const size_t index = ToIndex(x, y);
				if (IsBlockingTile((*tiles)[index]) || reached[index] == 1 || scratch[index] == 1)
				{
					continue;
				}

				ZoneTile orphan;
				orphan.x = x;
				orphan.y = y;

				region.clear();
				const int32 region_size = FloodMark(*tiles, orphan, 1, &scratch, &region);

				if (region_size < MIN_REGION_SIZE)
				{
					for (const ZoneTile& tile : region)
					{
						(*tiles)[ToIndex(tile.x, tile.y)] = TILE_CLIFF;
					}

					continue;
				}

				CarveCorridor(tiles, reached, orphan, start);
				FloodMark(*tiles, orphan, 1, &reached, nullptr);
			}
		}
	}

	// 연결이 끝난 뒤에도 갇힌 빈 칸이 절대 안 남게 마지막에 메운다
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

	// ------------------------------------------------------------ 마을 자리

	// 성채가 가운데, 개척촌 여섯이 둘레 육각형. 육각형이라 이웃끼리도 성채와도 같은 거리다
	void PlaceVillages(uint32 seed, ZoneLayout* layout)
	{
		layout->villages.clear();

		ZoneVillage citadel;
		citadel.center = layout->center;
		citadel.radius = CITADEL_RADIUS;
		citadel.is_citadel = true;
		layout->villages.push_back(citadel);

		for (int32 index = 1; index < ZONE_VILLAGE_COUNT; ++index)
		{
			const float jitter_angle = (HashToUnit(HashZoneTile(seed + 1201u, index, 0)) - 0.5f) * 2.f * HAMLET_ANGLE_JITTER;
			const float jitter_radius = (HashToUnit(HashZoneTile(seed + 1203u, index, 0)) - 0.5f) * 2.f * HAMLET_RING_JITTER;
			const float angle = static_cast<float>(index - 1) * PI / 3.f + jitter_angle;
			const float radius = HAMLET_RING_RADIUS + jitter_radius;

			ZoneVillage hamlet;
			hamlet.center.x = layout->center.x + static_cast<int32>(std::lround(std::cos(angle) * radius));
			hamlet.center.y = layout->center.y + static_cast<int32>(std::lround(std::sin(angle) * radius));
			hamlet.radius = HAMLET_RADIUS;
			layout->villages.push_back(hamlet);
		}
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

	void ClearHamlets(ZoneLayout* layout)
	{
		for (const ZoneVillage& village : layout->villages)
		{
			if (village.is_citadel)
			{
				continue;
			}

			ClearDisk(&layout->tiles, static_cast<float>(village.center.x) + 0.5f, static_cast<float>(village.center.y) + 0.5f, HAMLET_CLEAR_RADIUS);
		}
	}

	// ------------------------------------------------------------ 교역로

	// 성채 쪽 끝은 개척촌 방향에 가장 가까운 성문 길 끝
	RoadPoint GetCitadelRoadEnd(const ZoneLayout& layout, const ZoneTile& toward)
	{
		const ZoneTile& center = layout.center;
		const float delta_x = static_cast<float>(toward.x - center.x);
		const float delta_y = static_cast<float>(toward.y - center.y);

		size_t best_dir = 0;
		float best_dot = -1.0e9f;

		for (size_t dir = 0; dir < STEP_X.size(); ++dir)
		{
			const float dot = delta_x * static_cast<float>(STEP_X[dir]) + delta_y * static_cast<float>(STEP_Y[dir]);
			if (dot > best_dot)
			{
				best_dot = dot;
				best_dir = dir;
			}
		}

		RoadPoint point;
		point.x = static_cast<float>(center.x + STEP_X[best_dir] * GATE_ROAD_LENGTH) + 0.5f;
		point.y = static_cast<float>(center.y + STEP_Y[best_dir] * GATE_ROAD_LENGTH) + 0.5f;

		return point;
	}

	RoadPoint GetVillageRoadEnd(const ZoneLayout& layout, int32 village, int32 other)
	{
		if (layout.villages[static_cast<size_t>(village)].is_citadel)
		{
			return GetCitadelRoadEnd(layout, layout.villages[static_cast<size_t>(other)].center);
		}

		const ZoneTile& center = layout.villages[static_cast<size_t>(village)].center;

		RoadPoint point;
		point.x = static_cast<float>(center.x) + 0.5f;
		point.y = static_cast<float>(center.y) + 0.5f;

		return point;
	}

	// 곧게 그으면 자로 그은 길이라 굽이를 준다. 양 끝은 마을 광장으로 곧게 들어간다
	void TraceRoad(uint32 seed, int32 road_index, const RoadPoint& from, const RoadPoint& to, std::vector<RoadPoint>* points)
	{
		points->clear();

		const float delta_x = to.x - from.x;
		const float delta_y = to.y - from.y;
		const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);

		if (length < 1.f)
		{
			return;
		}

		const float normal_x = -delta_y / length;
		const float normal_y = delta_x / length;
		const float bend = (HashToUnit(HashZoneTile(seed + 1301u, road_index, 0)) - 0.5f) * 2.f * ROAD_BEND;
		const float phase = HashToUnit(HashZoneTile(seed + 1303u, road_index, 0)) * PI * 2.f;
		const int32 steps = static_cast<int32>(length / ROAD_SAMPLE_STEP) + 1;

		for (int32 step = 0; step <= steps; ++step)
		{
			const float t = static_cast<float>(step) / static_cast<float>(steps);
			const float envelope = std::sin(PI * t);
			const float offset = bend * envelope + ROAD_WIGGLE * std::sin(PI * 3.f * t + phase) * envelope;

			RoadPoint point;
			point.x = from.x + delta_x * t + normal_x * offset;
			point.y = from.y + delta_y * t + normal_y * offset;
			points->push_back(point);
		}
	}

	// 점을 칸으로. 대각선으로 건너뛴 곳은 한 칸 끼워 4 방향으로 잇는다 ( 칠할 때 끊기지 않게 )
	void RasterizeRoad(const std::vector<RoadPoint>& points, std::vector<ZoneTile>* path)
	{
		path->clear();

		for (const RoadPoint& point : points)
		{
			ZoneTile tile;
			tile.x = static_cast<int32>(std::floor(point.x));
			tile.y = static_cast<int32>(std::floor(point.y));

			if (IsInside(tile.x, tile.y) == false)
			{
				continue;
			}

			if (path->empty() == false)
			{
				const ZoneTile last = path->back();
				if (last.x == tile.x && last.y == tile.y)
				{
					continue;
				}

				if (last.x != tile.x && last.y != tile.y)
				{
					ZoneTile bridge;
					bridge.x = tile.x;
					bridge.y = last.y;
					path->push_back(bridge);
				}
			}

			path->push_back(tile);
		}
	}

	float MeasureRoadOutside(const ZoneLayout& layout, const ZoneRoad& road, const std::vector<RoadPoint>& points)
	{
		const ZoneVillage& village_a = layout.villages[static_cast<size_t>(road.village_a)];
		const ZoneVillage& village_b = layout.villages[static_cast<size_t>(road.village_b)];
		float length = 0.f;

		for (size_t index = 1; index < points.size(); ++index)
		{
			const RoadPoint& point = points[index];
			const ZoneTile tile = { static_cast<int32>(point.x), static_cast<int32>(point.y) };

			if (TileDistance(tile, village_a.center) < static_cast<float>(village_a.radius))
			{
				continue;
			}

			if (TileDistance(tile, village_b.center) < static_cast<float>(village_b.radius))
			{
				continue;
			}

			const float delta_x = point.x - points[index - 1].x;
			const float delta_y = point.y - points[index - 1].y;
			length += std::sqrt(delta_x * delta_x + delta_y * delta_y);
		}

		return length;
	}

	void AddRoad(uint32 seed, int32 village_a, int32 village_b, ZoneLayout* layout)
	{
		ZoneRoad road;
		road.village_a = village_a;
		road.village_b = village_b;

		std::vector<RoadPoint> points;
		const int32 road_index = static_cast<int32>(layout->roads.size());
		TraceRoad(seed, road_index, GetVillageRoadEnd(*layout, village_a, village_b), GetVillageRoadEnd(*layout, village_b, village_a), &points);

		for (const RoadPoint& point : points)
		{
			ClearDisk(&layout->tiles, point.x, point.y, ROAD_CARVE_RADIUS);
		}

		RasterizeRoad(points, &road.path);

		std::cout << "[ ZoneGen - AddRoad ] : road " << village_a << "-" << village_b
			<< " outside " << static_cast<int32>(MeasureRoadOutside(*layout, road, points)) << " tiles\n";

		layout->villages[static_cast<size_t>(village_a)].neighbors.push_back(village_b);
		layout->villages[static_cast<size_t>(village_b)].neighbors.push_back(village_a);
		layout->roads.push_back(road);
	}

	// 성채에서 개척촌마다 하나, 이웃 개척촌끼리 하나씩. 열두 갈래
	void CarveRoads(uint32 seed, ZoneLayout* layout)
	{
		layout->roads.clear();

		for (int32 index = 1; index < ZONE_VILLAGE_COUNT; ++index)
		{
			AddRoad(seed, ZONE_CITADEL, index, layout);
		}

		for (int32 index = 1; index < ZONE_VILLAGE_COUNT; ++index)
		{
			int32 next = index + 1;
			if (next >= ZONE_VILLAGE_COUNT)
			{
				next = 1;
			}

			AddRoad(seed, index, next, layout);
		}
	}

	// 길이 마을 경계를 처음 넘는 칸. 새 사람이 이 길로 걸어 들어온다
	void FindRoadEntries(ZoneLayout* layout)
	{
		for (const ZoneRoad& road : layout->roads)
		{
			const std::array<int32, 2> ends = { road.village_a, road.village_b };

			for (size_t end = 0; end < ends.size(); ++end)
			{
				ZoneVillage& village = layout->villages[static_cast<size_t>(ends[end])];
				const float radius = static_cast<float>(village.radius) + 1.f;

				for (size_t step = 0; step < road.path.size(); ++step)
				{
					size_t index = step;
					if (end == 1)
					{
						index = road.path.size() - 1 - step;
					}

					const ZoneTile& tile = road.path[index];
					if (TileDistance(tile, village.center) >= radius)
					{
						village.entries.push_back(tile);
						break;
					}
				}
			}
		}
	}

	// 마을 안 길 둘레는 집을 안 짓는다. 성채는 성문 축이 큰길이다
	void MarkStreets(const ZoneLayout& layout, std::vector<uint8>* keep_clear)
	{
		keep_clear->assign(layout.tiles.size(), 0);

		for (const ZoneRoad& road : layout.roads)
		{
			for (const ZoneTile& tile : road.path)
			{
				for (int32 offset_y = -STREET_KEEP_CLEAR; offset_y <= STREET_KEEP_CLEAR; ++offset_y)
				{
					for (int32 offset_x = -STREET_KEEP_CLEAR; offset_x <= STREET_KEEP_CLEAR; ++offset_x)
					{
						const int32 x = tile.x + offset_x;
						const int32 y = tile.y + offset_y;

						if (IsInside(x, y))
						{
							(*keep_clear)[ToIndex(x, y)] = 1;
						}
					}
				}
			}
		}

		const ZoneTile& center = layout.center;
		const int32 reach = static_cast<int32>(WALL_OUTER_RADIUS) + 1;

		for (int32 y = center.y - reach; y <= center.y + reach; ++y)
		{
			for (int32 x = center.x - reach; x <= center.x + reach; ++x)
			{
				if (IsInside(x, y) == false)
				{
					continue;
				}

				if (std::abs(x - center.x) < 3 || std::abs(y - center.y) < 3)
				{
					(*keep_clear)[ToIndex(x, y)] = 1;
				}
			}
		}
	}

	// ------------------------------------------------------------ 성채

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
			gate.village = ZONE_CITADEL;
			gate.tile.x = center.x + along_x * static_cast<int32>(WALL_OUTER_RADIUS + 2.f);
			gate.tile.y = center.y + along_y * static_cast<int32>(WALL_OUTER_RADIUS + 2.f);
			layout->sites.push_back(gate);
		}
	}

	void BuildHamletPlazas(ZoneLayout* layout)
	{
		for (const ZoneVillage& village : layout->villages)
		{
			if (village.is_citadel)
			{
				continue;
			}

			const int32 reach = static_cast<int32>(HAMLET_PLAZA_RADIUS) + 1;

			for (int32 y = village.center.y - reach; y <= village.center.y + reach; ++y)
			{
				for (int32 x = village.center.x - reach; x <= village.center.x + reach; ++x)
				{
					if (CenterDistance(x, y, village.center) < HAMLET_PLAZA_RADIUS)
					{
						layout->tiles[ToIndex(x, y)] = TILE_PLAZA;
					}
				}
			}
		}
	}

	// ------------------------------------------------------------ 집

	struct LotCandidate
	{
		ZoneTile base;
		float order = 0.f;	// 작을수록 광장에 가깝다
	};

	bool CanPlaceLot(const ZoneLayout& layout, const std::vector<uint8>& keep_clear, const ZoneVillage& village, int32 base_x, int32 base_y, int32 size_x, int32 size_y)
	{
		float min_radius = HAMLET_HOUSE_MIN;
		float max_radius = HAMLET_HOUSE_MAX;
		if (village.is_citadel)
		{
			min_radius = CITADEL_HOUSE_MIN;
			max_radius = CITADEL_HOUSE_MAX;
		}

		for (int32 y = base_y; y < base_y + size_y; ++y)
		{
			for (int32 x = base_x; x < base_x + size_x; ++x)
			{
				if (IsInside(x, y) == false)
				{
					return false;
				}

				const float distance = CenterDistance(x, y, village.center);
				if (distance < min_radius || distance > max_radius)
				{
					return false;
				}

				const size_t index = ToIndex(x, y);
				if (keep_clear[index] == 1 || layout.tiles[index] != TILE_OPEN)
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

	void PickHouseSize(uint32 hash, int32* size_x, int32* size_y)
	{
		const float roll = HashToUnit(hash);

		*size_x = 2;
		*size_y = 2;

		if (roll < 0.15f)
		{
			*size_x = 3;
		}
		else if (roll < 0.30f)
		{
			*size_y = 3;
		}
		else if (roll < 0.40f)
		{
			*size_x = 3;
			*size_y = 3;
		}
	}

	// 문은 카메라 쪽 면 ( +y ) 에 낸다. 문 앞은 격자 골목이라 늘 비어 있다
	void AddHouse(int32 village_index, int32 base_x, int32 base_y, int32 size_x, int32 size_y, uint32 hash, ZoneLayout* layout)
	{
		ZoneHouse house;
		house.village = village_index;
		house.lot.x = base_x;
		house.lot.y = base_y;
		house.size_x = size_x;
		house.size_y = size_y;
		house.door_on_x = false;
		house.door.x = base_x + static_cast<int32>((hash >> 9) % static_cast<uint32>(size_x));
		house.door.y = base_y + size_y;

		FillLot(layout, base_x, base_y, size_x, size_y, TILE_HOUSE);
		layout->houses.push_back(house);
	}

	// 광장에 가까운 칸부터 집을 채우고, 바깥 칸은 밭과 마당으로 남긴다
	void BuildVillageHouses(uint32 seed, int32 village_index, const std::vector<uint8>& keep_clear, ZoneLayout* layout)
	{
		const ZoneVillage village = layout->villages[static_cast<size_t>(village_index)];
		const int32 reach = static_cast<int32>(HAMLET_HOUSE_MAX) + 2;
		std::vector<LotCandidate> candidates;

		for (int32 base_y = village.center.y - reach; base_y <= village.center.y + reach; base_y += HOUSE_LATTICE)
		{
			for (int32 base_x = village.center.x - reach; base_x <= village.center.x + reach; base_x += HOUSE_LATTICE)
			{
				if (CanPlaceLot(*layout, keep_clear, village, base_x, base_y, 2, 2) == false)
				{
					continue;
				}

				LotCandidate candidate;
				candidate.base.x = base_x;
				candidate.base.y = base_y;
				candidate.order = CenterDistance(base_x + 1, base_y + 1, village.center) + HashToUnit(HashZoneTile(seed + 611u, base_x, base_y)) * 4.f;
				candidates.push_back(candidate);
			}
		}

		std::sort(candidates.begin(), candidates.end(),
			[](const LotCandidate& left, const LotCandidate& right)
			{
				return left.order < right.order;
			});

		int32 built = 0;

		for (const LotCandidate& candidate : candidates)
		{
			const int32 base_x = candidate.base.x;
			const int32 base_y = candidate.base.y;
			const uint32 hash = HashZoneTile(seed + 606u, base_x, base_y);

			if (built < HOUSES_PER_VILLAGE)
			{
				int32 size_x = 2;
				int32 size_y = 2;
				PickHouseSize(hash, &size_x, &size_y);

				if (CanPlaceLot(*layout, keep_clear, village, base_x, base_y, size_x, size_y) == false)
				{
					size_x = 2;
					size_y = 2;
				}

				AddHouse(village_index, base_x, base_y, size_x, size_y, hash, layout);
				built += 1;
				continue;
			}

			if (HashToUnit(hash >> 3) < 0.6f && CanPlaceLot(*layout, keep_clear, village, base_x, base_y, 3, 3))
			{
				FillLot(layout, base_x, base_y, 3, 3, TILE_FIELD);
			}
		}

		std::cout << "[ ZoneGen - BuildVillageHouses ] : village " << village_index << " houses " << built << " lots " << candidates.size() << "\n";
	}

	// ------------------------------------------------------------ 등불 · 게시판

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
		for (const ZoneVillage& village : layout->villages)
		{
			const ZoneTile& center = village.center;

			AddLamp(layout, center.x + 3, center.y + 3);
			AddLamp(layout, center.x - 3, center.y - 3);
			AddLamp(layout, center.x + 3, center.y - 3);
			AddLamp(layout, center.x - 3, center.y + 3);

			if (village.is_citadel)
			{
				for (size_t dir = 0; dir < STEP_X.size(); ++dir)
				{
					int32 along_x = 0;
					int32 along_y = 0;
					int32 side_x = 0;
					int32 side_y = 0;
					GetGateAxis(dir, &along_x, &along_y, &side_x, &side_y);

					const int32 along = 13;
					AddLamp(layout, center.x + along_x * along + side_x * 2, center.y + along_y * along + side_y * 2);
				}

				continue;
			}

			// 길이 마을로 들어서는 자리마다 등불. 밤길에 마을 입구가 보여야 한다
			for (const ZoneTile& entry : village.entries)
			{
				const float delta_x = static_cast<float>(entry.x - center.x);
				const float delta_y = static_cast<float>(entry.y - center.y);
				const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);

				if (length < 1.f)
				{
					continue;
				}

				const int32 inward_x = entry.x - static_cast<int32>(std::lround(delta_x / length * 3.f));
				const int32 inward_y = entry.y - static_cast<int32>(std::lround(delta_y / length * 3.f));
				AddLamp(layout, inward_x + static_cast<int32>(std::lround(-delta_y / length * 3.f)), inward_y + static_cast<int32>(std::lround(delta_x / length * 3.f)));
			}
		}
	}

	// 광장 가장자리 빈 칸. 길 위는 피한다
	void PlaceBoards(const std::vector<uint8>& keep_clear, ZoneLayout* layout)
	{
		const std::array<ZoneTile, 8> offsets =
		{ {
			{ 3, -2 }, { -2, 3 }, { 3, 2 }, { -3, -2 }, { 2, -3 }, { -3, 2 }, { 2, 3 }, { -2, -3 },
		} };

		for (ZoneVillage& village : layout->villages)
		{
			village.board = village.center;

			for (const ZoneTile& offset : offsets)
			{
				const int32 x = village.center.x + offset.x;
				const int32 y = village.center.y + offset.y;

				if (IsOpenAt(layout->tiles, x, y) == false)
				{
					continue;
				}

				if (village.is_citadel == false && keep_clear[ToIndex(x, y)] == 1)
				{
					continue;
				}

				village.board.x = x;
				village.board.y = y;
				break;
			}
		}
	}

	// ------------------------------------------------------------ 거점

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

	bool IsFarFromVillages(const ZoneTile& tile, const std::vector<ZoneVillage>& villages, float spacing)
	{
		for (const ZoneVillage& village : villages)
		{
			if (TileDistance(tile, village.center) < spacing)
			{
				return false;
			}
		}

		return true;
	}

	// 마을 경계에서 잰다
	bool IsFarFromVillageEdges(const ZoneTile& tile, const std::vector<ZoneVillage>& villages, float spacing)
	{
		for (const ZoneVillage& village : villages)
		{
			if (TileDistance(tile, village.center) < static_cast<float>(village.radius) + spacing)
			{
				return false;
			}
		}

		return true;
	}

	bool IsFarFromRoads(const ZoneTile& tile, const std::vector<ZoneRoad>& roads, float spacing)
	{
		const float spacing_sq = spacing * spacing;

		for (const ZoneRoad& road : roads)
		{
			for (const ZoneTile& point : road.path)
			{
				const float delta_x = static_cast<float>(point.x - tile.x);
				const float delta_y = static_cast<float>(point.y - tile.y);

				if (delta_x * delta_x + delta_y * delta_y < spacing_sq)
				{
					return false;
				}
			}
		}

		return true;
	}

	// 걸어서 제일 먼 칸은 거의 언제나 구역 가장자리다. 최대 거리의 일정 비율 지점을 고른다
	ZoneTile FindLairTile(const ZoneLayout& layout, const std::vector<int32>& distances, const ZoneTile& fallback)
	{
		const int32 target = static_cast<int32>(static_cast<float>(FindMaxDistance(distances)) * LAIR_WALK_RATIO);

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
				if (gap >= closest_gap)
				{
					continue;
				}

				ZoneTile tile;
				tile.x = x;
				tile.y = y;

				if (IsFarFromVillages(tile, layout.villages, LAIR_VILLAGE_CLEARANCE) == false)
				{
					continue;
				}

				closest_gap = gap;
				found = tile;
			}
		}

		return found;
	}

	void PickSites(uint32* random_state, const std::vector<int32>& distances, int32 kind, int32 count, int32 min_walk, int32 max_walk, float spacing, float village_clearance, float road_clearance, ZoneLayout* layout)
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

			if (IsFarFromVillages(tile, layout->villages, village_clearance) == false)
			{
				continue;
			}

			if (IsFarFromRoads(tile, layout->roads, road_clearance) == false)
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

	// 고리 길 한가운데 길가에 초소. 마을 사이 20 초 길에서 쉬어 갈 자리
	void PlaceOutposts(ZoneLayout* layout)
	{
		for (const ZoneRoad& road : layout->roads)
		{
			if (road.village_a == ZONE_CITADEL || road.village_b == ZONE_CITADEL || road.path.size() < 8)
			{
				continue;
			}

			const size_t middle = road.path.size() / 2;
			const ZoneTile& before = road.path[middle - 4];
			const ZoneTile& after = road.path[middle + 4];
			const float delta_x = static_cast<float>(after.x - before.x);
			const float delta_y = static_cast<float>(after.y - before.y);
			const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);

			if (length < 0.5f)
			{
				continue;
			}

			// 바깥쪽 ( 성채 반대편 ) 길가에 친다
			float normal_x = -delta_y / length;
			float normal_y = delta_x / length;
			const ZoneTile& mid = road.path[middle];
			const float out_x = static_cast<float>(mid.x - layout->center.x);
			const float out_y = static_cast<float>(mid.y - layout->center.y);

			if (normal_x * out_x + normal_y * out_y < 0.f)
			{
				normal_x = -normal_x;
				normal_y = -normal_y;
			}

			ZoneSite outpost;
			outpost.kind = SITE_OUTPOST;
			outpost.tile.x = mid.x + static_cast<int32>(std::lround(normal_x * OUTPOST_ROAD_OFFSET));
			outpost.tile.y = mid.y + static_cast<int32>(std::lround(normal_y * OUTPOST_ROAD_OFFSET));

			if (IsInside(outpost.tile.x, outpost.tile.y) == false || IsBorder(outpost.tile.x, outpost.tile.y))
			{
				continue;
			}

			layout->sites.push_back(outpost);
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
				return RUIN_CLEAR_RADIUS;
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

	// ------------------------------------------------------------ 지형 꾸미기

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

	void ScatterObstacles(uint32 seed, const std::vector<uint8>& keep_clear, ZoneLayout* layout)
	{
		for (int32 y = BORDER; y < ZONE_TILE_COUNT - BORDER; ++y)
		{
			for (int32 x = BORDER; x < ZONE_TILE_COUNT - BORDER; ++x)
			{
				const size_t index = ToIndex(x, y);
				if (layout->tiles[index] != TILE_OPEN || keep_clear[index] == 1)
				{
					continue;
				}

				ZoneTile tile;
				tile.x = x;
				tile.y = y;

				if (IsFarFromVillageEdges(tile, layout->villages, 3.f) == false)
				{
					continue;
				}

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
					layout->tiles[index] = TILE_TREE;
				}
				else
				{
					layout->tiles[index] = TILE_ROCK;
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

	// 길 칸과 그 +x · +y 칸. 대각선 길에서도 띠가 끊기지 않는다
	void PaintRoads(ZoneLayout* layout)
	{
		for (const ZoneRoad& road : layout->roads)
		{
			for (const ZoneTile& tile : road.path)
			{
				if (IsInsideTown(*layout, tile.x, tile.y))
				{
					continue;
				}

				PaintRoadTile(layout, tile.x, tile.y);
				PaintRoadTile(layout, tile.x + 1, tile.y);
				PaintRoadTile(layout, tile.x, tile.y + 1);
			}
		}
	}

	void BuildVillageMap(ZoneLayout* layout)
	{
		layout->village_map.assign(layout->tiles.size(), ZONE_NO_VILLAGE);

		for (size_t village_index = 0; village_index < layout->villages.size(); ++village_index)
		{
			const ZoneVillage& village = layout->villages[village_index];
			const int32 radius = village.radius;

			for (int32 y = village.center.y - radius; y <= village.center.y + radius; ++y)
			{
				for (int32 x = village.center.x - radius; x <= village.center.x + radius; ++x)
				{
					if (IsInside(x, y) == false)
					{
						continue;
					}

					if (CenterDistance(x, y, village.center) <= static_cast<float>(radius))
					{
						layout->village_map[ToIndex(x, y)] = static_cast<uint8>(village_index);
					}
				}
			}
		}
	}

	bool IsInVillage(const ZoneLayout& layout, int32 x, int32 y)
	{
		return layout.village_map[ToIndex(x, y)] != ZONE_NO_VILLAGE;
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
				if (layout->tiles[index] != TILE_OPEN || distances[index] < 0 || IsInVillage(*layout, x, y))
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
	void PaintRuinFields(uint32 seed, ZoneLayout* layout)
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

	// ------------------------------------------------------------ 몬스터 · 자원

	float GetVillageEdgeDistance(const ZoneTile& tile, const std::vector<ZoneVillage>& villages)
	{
		float nearest = 1.0e9f;

		for (const ZoneVillage& village : villages)
		{
			const float edge = TileDistance(tile, village.center) - static_cast<float>(village.radius);
			if (edge < nearest)
			{
				nearest = edge;
			}
		}

		return nearest;
	}

	int32 GetBandOf(const ZoneTile& tile, int32 walk_distance, const std::vector<ZoneVillage>& villages)
	{
		if (GetVillageEdgeDistance(tile, villages) < BAND_INTRO_EDGE)
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
		for (int32 attempt = 0; attempt < MAX_PICK_TRY * 3; ++attempt)
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

			// 마을 안은 몬스터가 못 들어오는 곳이다. 경계에서 배회 · 추격 거리만큼 띄운다
			if (IsFarFromVillageEdges(tile, layout->villages, PACK_VILLAGE_SPACING) == false)
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
			const int32 band = GetBandOf(tile, walk_distance, layout->villages);
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
				if (layout->tiles[index] != TILE_OPEN || distances[index] < PACK_MIN_WALK || IsInVillage(*layout, x, y))
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

			for (int32 offset_y = -8; offset_y <= 8 && placed < RELIC_PER_RUIN; ++offset_y)
			{
				for (int32 offset_x = -8; offset_x <= 8 && placed < RELIC_PER_RUIN; ++offset_x)
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

			if (IsOpenAt(layout->tiles, tile.x, tile.y) == false || IsInVillage(*layout, tile.x, tile.y))
			{
				continue;
			}

			if (distances[ToIndex(tile.x, tile.y)] < 50)
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
	layout->houses.clear();
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

	// 마을 터와 교역로를 먼저 비운다. 연결 보장이 이 빈터를 기준으로 돌아 길이 끊기지 않는다
	PlaceVillages(seed, layout);
	ClearTownArea(layout);
	ClearHamlets(layout);
	CarveRoads(seed, layout);

	// 성벽이 없을 때 연결을 보장하고 나서 성벽을 세운다. 성벽 밖 빈 고리가 모든 바깥 땅을 성문으로 잇는다
	ConnectRegions(&layout->tiles, layout->center);
	FillUnreached(&layout->tiles, layout->center, TILE_CLIFF);

	BuildWallRing(layout);
	BuildGates(layout);
	BuildHamletPlazas(layout);
	FindRoadEntries(layout);

	std::vector<uint8> keep_clear;
	MarkStreets(*layout, &keep_clear);

	for (int32 village = 0; village < ZONE_VILLAGE_COUNT; ++village)
	{
		BuildVillageHouses(seed, village, keep_clear, layout);
	}

	PlaceLamps(layout);
	PlaceBoards(keep_clear, layout);

	for (size_t index = 0; index < layout->villages.size(); ++index)
	{
		ZoneSite town;
		town.tile = layout->villages[index].center;
		town.kind = SITE_TOWN;
		town.village = static_cast<int32>(index);
		layout->villages[index].site = static_cast<int32>(layout->sites.size());
		layout->sites.push_back(town);
	}

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

	layout->boss = FindLairTile(*layout, distances, layout->spawn);

	ZoneSite lair;
	lair.tile = layout->boss;
	lair.kind = SITE_LAIR;
	lair.walk_distance = distances[ToIndex(layout->boss.x, layout->boss.y)];
	layout->sites.push_back(lair);

	PlaceOutposts(layout);
	PickSites(&random_state, distances, SITE_HIDEOUT, HIDEOUT_COUNT, 70, max_walk * 90 / 100, HIDEOUT_SPACING, HIDEOUT_VILLAGE_CLEARANCE, HIDEOUT_ROAD_CLEARANCE, layout);
	PickSites(&random_state, distances, SITE_VILLAGE, RUIN_COUNT, 50, max_walk * 85 / 100, RUIN_SPACING, RUIN_VILLAGE_CLEARANCE, RUIN_ROAD_CLEARANCE, layout);

	for (const ZoneSite& site : layout->sites)
	{
		const int32 radius = GetSiteClearRadius(site.kind);
		if (radius > 0)
		{
			CarveCircle(&layout->tiles, site.tile, radius, layout->center);
		}
	}

	BuildVillageMap(layout);
	ClassifyBlockedRegions(seed, &layout->tiles);
	PlaceRuins(&random_state, layout);
	ScatterObstacles(seed, keep_clear, layout);

	MeasureDistance(layout->tiles, layout->center, &distances);

	PaintRoads(layout);
	PaintBlight(seed, distances, layout);
	PaintRuinFields(seed, layout);

	// 칠하기만 했지 막힘은 안 바꿨다. 그래도 보장을 한 번 더 건다 ( 막힌 틈은 바위로 메운다 )
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
		<< " villages " << layout->villages.size()
		<< " houses " << layout->houses.size()
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

int32 GetZoneVillageAt(const ZoneLayout& layout, int32 x, int32 y)
{
	if (IsInside(x, y) == false || layout.village_map.empty())
	{
		return -1;
	}

	const uint8 village = layout.village_map[ToIndex(x, y)];
	if (village == ZONE_NO_VILLAGE)
	{
		return -1;
	}

	return static_cast<int32>(village);
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
