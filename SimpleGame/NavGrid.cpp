#include "stdafx.h"
#include "NavGrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <queue>
#include <utility>

namespace
{
	constexpr int32 MAX_EXPAND = 16000;	// 구역 3 분의 1 쯤. 이걸 넘기면 거의 막힌 목표다
	constexpr int32 BUDGET_PER_TICK = 4;
	constexpr int32 GOAL_SEARCH_RADIUS = 3;
	const float DIAGONAL_COST = 1.41421356f;
	const float LINE_STEP = 0.25f;

	const std::array<int32, 8> STEP_X = { 1, -1, 0, 0, 1, 1, -1, -1 };
	const std::array<int32, 8> STEP_Y = { 0, 0, 1, -1, 1, -1, 1, -1 };

	std::vector<uint8> g_blocked;

	// 탐색마다 배열을 지우지 않으려고 도장을 찍는다. 도장이 이번 번호가 아니면 안 가 본 칸이다
	std::vector<float> g_cost;
	std::vector<int32> g_parent;
	std::vector<uint32> g_open_stamp;
	std::vector<uint32> g_closed_stamp;
	uint32 g_stamp = 0;

	int32 g_budget = BUDGET_PER_TICK;
	int32 g_search_count = 0;

	int32 ToIndex(int32 x, int32 y)
	{
		return y * ZONE_TILE_COUNT + x;
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

	float Heuristic(int32 x, int32 y, const ZoneTile& goal)
	{
		const float delta_x = std::fabs(static_cast<float>(x - goal.x));
		const float delta_y = std::fabs(static_cast<float>(y - goal.y));

		if (delta_x > delta_y)
		{
			return delta_x + (DIAGONAL_COST - 1.f) * delta_y;
		}

		return delta_y + (DIAGONAL_COST - 1.f) * delta_x;
	}

	bool FindOpenNear(const ZoneTile& goal, ZoneTile* found)
	{
		for (int32 radius = 1; radius <= GOAL_SEARCH_RADIUS; ++radius)
		{
			for (int32 offset_y = -radius; offset_y <= radius; ++offset_y)
			{
				for (int32 offset_x = -radius; offset_x <= radius; ++offset_x)
				{
					if (std::abs(offset_x) != radius && std::abs(offset_y) != radius)
					{
						continue;
					}

					const int32 x = goal.x + offset_x;
					const int32 y = goal.y + offset_y;

					if (IsNavBlocked(x, y) == false)
					{
						found->x = x;
						found->y = y;
						return true;
					}
				}
			}
		}

		return false;
	}

	void BuildPath(int32 end_index, int32 start_index, std::vector<ZoneTile>* path)
	{
		path->clear();

		int32 cursor = end_index;
		while (cursor != start_index && cursor >= 0)
		{
			ZoneTile tile;
			tile.x = cursor % ZONE_TILE_COUNT;
			tile.y = cursor / ZONE_TILE_COUNT;
			path->push_back(tile);

			cursor = g_parent[static_cast<size_t>(cursor)];
		}

		std::reverse(path->begin(), path->end());
	}

	void WorldToZoneFloat(const WorldPos& pos, float* x, float* y)
	{
		*x = static_cast<float>((pos.chunk_x + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_x;
		*y = static_cast<float>((pos.chunk_y + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_y;
	}

	bool IsBoxBlocked(float x, float y, float radius)
	{
		const std::array<float, 2> offsets = { -radius, radius };

		for (float offset_y : offsets)
		{
			for (float offset_x : offsets)
			{
				const int32 tile_x = static_cast<int32>(std::floor(x + offset_x));
				const int32 tile_y = static_cast<int32>(std::floor(y + offset_y));

				if (IsNavBlocked(tile_x, tile_y))
				{
					return true;
				}
			}
		}

		return false;
	}
}

void BuildNavGrid(const std::vector<uint8>& tiles)
{
	const size_t count = static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT);

	g_blocked.assign(count, 0);
	for (size_t index = 0; index < count && index < tiles.size(); ++index)
	{
		if (IsBlockingTile(tiles[index]))
		{
			g_blocked[index] = 1;
		}
	}

	g_cost.assign(count, 0.f);
	g_parent.assign(count, -1);
	g_open_stamp.assign(count, 0);
	g_closed_stamp.assign(count, 0);
	g_stamp = 0;
}

bool IsNavBlocked(int32 x, int32 y)
{
	if (IsInside(x, y) == false || g_blocked.empty())
	{
		return true;
	}

	return g_blocked[static_cast<size_t>(ToIndex(x, y))] != 0;
}

bool FindNavPath(const ZoneTile& start, const ZoneTile& goal_request, std::vector<ZoneTile>* path)
{
	path->clear();
	g_search_count += 1;

	if (IsNavBlocked(start.x, start.y))
	{
		return false;
	}

	ZoneTile goal = goal_request;
	if (IsNavBlocked(goal.x, goal.y))
	{
		if (FindOpenNear(goal_request, &goal) == false)
		{
			return false;
		}
	}

	g_stamp += 1;

	// 도장이 한 바퀴 돌면 옛 도장과 섞이니 그때만 싹 지운다
	if (g_stamp == 0)
	{
		std::fill(g_open_stamp.begin(), g_open_stamp.end(), 0u);
		std::fill(g_closed_stamp.begin(), g_closed_stamp.end(), 0u);
		g_stamp = 1;
	}

	typedef std::pair<float, int32> OpenEntry;
	std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> open;

	const int32 start_index = ToIndex(start.x, start.y);
	const int32 goal_index = ToIndex(goal.x, goal.y);

	g_cost[static_cast<size_t>(start_index)] = 0.f;
	g_parent[static_cast<size_t>(start_index)] = -1;
	g_open_stamp[static_cast<size_t>(start_index)] = g_stamp;
	open.push(OpenEntry(Heuristic(start.x, start.y, goal), start_index));

	int32 best_index = start_index;
	float best_heuristic = Heuristic(start.x, start.y, goal);
	int32 expanded = 0;

	while (open.empty() == false)
	{
		const OpenEntry entry = open.top();
		open.pop();

		const int32 current = entry.second;
		const size_t current_slot = static_cast<size_t>(current);

		// 같은 칸이 더 싼 값으로 다시 들어온 뒤라 옛 항목은 버린다
		if (g_closed_stamp[current_slot] == g_stamp)
		{
			continue;
		}

		g_closed_stamp[current_slot] = g_stamp;

		if (current == goal_index)
		{
			BuildPath(goal_index, start_index, path);
			return true;
		}

		expanded += 1;
		if (expanded > MAX_EXPAND)
		{
			break;
		}

		const int32 current_x = current % ZONE_TILE_COUNT;
		const int32 current_y = current / ZONE_TILE_COUNT;

		for (size_t dir = 0; dir < STEP_X.size(); ++dir)
		{
			const int32 next_x = current_x + STEP_X[dir];
			const int32 next_y = current_y + STEP_Y[dir];

			if (IsNavBlocked(next_x, next_y))
			{
				continue;
			}

			float step_cost = 1.f;

			// 대각선은 옆 두 칸이 다 비어야 한다. 모서리를 스치면 사각형 몸이 걸린다
			if (STEP_X[dir] != 0 && STEP_Y[dir] != 0)
			{
				if (IsNavBlocked(current_x + STEP_X[dir], current_y) || IsNavBlocked(current_x, current_y + STEP_Y[dir]))
				{
					continue;
				}

				step_cost = DIAGONAL_COST;
			}

			const int32 next = ToIndex(next_x, next_y);
			const size_t next_slot = static_cast<size_t>(next);

			if (g_closed_stamp[next_slot] == g_stamp)
			{
				continue;
			}

			const float cost = g_cost[current_slot] + step_cost;

			if (g_open_stamp[next_slot] == g_stamp && cost >= g_cost[next_slot])
			{
				continue;
			}

			g_open_stamp[next_slot] = g_stamp;
			g_cost[next_slot] = cost;
			g_parent[next_slot] = current;

			const float heuristic = Heuristic(next_x, next_y, goal);
			if (heuristic < best_heuristic)
			{
				best_heuristic = heuristic;
				best_index = next;
			}

			open.push(OpenEntry(cost + heuristic, next));
		}
	}

	// 다 못 찾았어도 가까이 간 데까지는 걷게 한다. 다음에 거기서 다시 찾는다
	if (best_index != start_index)
	{
		BuildPath(best_index, start_index, path);
	}

	return false;
}

bool HasNavLine(const WorldPos& from, const WorldPos& to, float radius)
{
	float from_x = 0.f;
	float from_y = 0.f;
	float to_x = 0.f;
	float to_y = 0.f;
	WorldToZoneFloat(from, &from_x, &from_y);
	WorldToZoneFloat(to, &to_x, &to_y);

	const float delta_x = to_x - from_x;
	const float delta_y = to_y - from_y;
	const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
	const int32 steps = static_cast<int32>(length / LINE_STEP) + 1;

	for (int32 step = 0; step <= steps; ++step)
	{
		const float ratio = static_cast<float>(step) / static_cast<float>(steps);

		if (IsBoxBlocked(from_x + delta_x * ratio, from_y + delta_y * ratio, radius))
		{
			return false;
		}
	}

	return true;
}

void ResetNavBudget()
{
	g_budget = BUDGET_PER_TICK;
}

bool TakeNavBudget()
{
	if (g_budget <= 0)
	{
		return false;
	}

	g_budget -= 1;

	return true;
}

int32 PopNavSearchCount()
{
	const int32 count = g_search_count;
	g_search_count = 0;

	return count;
}
