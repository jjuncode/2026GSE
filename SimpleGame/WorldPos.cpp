#include "stdafx.h"
#include "WorldPos.h"

#include <cmath>

void NormalizeWorldPos(WorldPos* pos)
{
	// 음수 방향 floor 나눗셈
	const float chunk_step_x = std::floor(pos->local_x / static_cast<float>(CHUNK_TILE_COUNT));
	const float chunk_step_y = std::floor(pos->local_y / static_cast<float>(CHUNK_TILE_COUNT));

	pos->chunk_x += static_cast<int32>(chunk_step_x);
	pos->chunk_y += static_cast<int32>(chunk_step_y);

	pos->local_x -= chunk_step_x * static_cast<float>(CHUNK_TILE_COUNT);
	pos->local_y -= chunk_step_y * static_cast<float>(CHUNK_TILE_COUNT);
}

void MoveWorldPos(WorldPos* pos, float delta_x, float delta_y)
{
	pos->local_x += delta_x;
	pos->local_y += delta_y;

	NormalizeWorldPos(pos);
}

float GetTileDistanceX(const WorldPos& from, const WorldPos& to)
{
	const int32 chunk_diff = to.chunk_x - from.chunk_x;
	const float chunk_part = static_cast<float>(chunk_diff * CHUNK_TILE_COUNT);
	const float local_part = to.local_x - from.local_x;

	return chunk_part + local_part;
}

float GetTileDistanceY(const WorldPos& from, const WorldPos& to)
{
	const int32 chunk_diff = to.chunk_y - from.chunk_y;
	const float chunk_part = static_cast<float>(chunk_diff * CHUNK_TILE_COUNT);
	const float local_part = to.local_y - from.local_y;

	return chunk_part + local_part;
}

float GetDepthKey(const WorldPos& target, const WorldPos& camera)
{
	return GetTileDistanceX(camera, target) + GetTileDistanceY(camera, target);
}

ScreenPos WorldToScreen(const WorldPos& target, const WorldPos& camera)
{
	const float tile_x = GetTileDistanceX(camera, target);
	const float tile_y = GetTileDistanceY(camera, target);

	ScreenPos screen;
	screen.x = (tile_x - tile_y) * (TILE_PIXEL_WIDTH * 0.5f);
	screen.y = -(tile_x + tile_y) * (TILE_PIXEL_HEIGHT * 0.5f);

	return screen;
}

WorldPos ScreenToWorld(const ScreenPos& screen, const WorldPos& camera)
{
	// 정변환 두 식을 더하고 빼서 푼 결과
	const float sum_axis = -screen.y / (TILE_PIXEL_HEIGHT * 0.5f);
	const float diff_axis = screen.x / (TILE_PIXEL_WIDTH * 0.5f);

	const float tile_x = (sum_axis + diff_axis) * 0.5f;
	const float tile_y = (sum_axis - diff_axis) * 0.5f;

	WorldPos world = camera;
	MoveWorldPos(&world, tile_x, tile_y);

	return world;
}
