#include "stdafx.h"
#include "World.h"

#include <array>
#include <cmath>
#include <unordered_map>

namespace
{
	struct ChunkTiles
	{
		std::array<bool, CHUNK_TILE_COUNT * CHUNK_TILE_COUNT> blocked = {};
	};

	// 청크 번호가 음수로도 끝없이 가서 큰 배열을 미리 못 잡는다
	std::unordered_map<uint64, ChunkTiles> g_chunks;

	uint64 MakeChunkKey(int32 chunk_x, int32 chunk_y)
	{
		const uint64 high = static_cast<uint64>(static_cast<uint32>(chunk_x));
		const uint64 low = static_cast<uint64>(static_cast<uint32>(chunk_y));

		return (high << 32) | low;
	}

	// 정규화 오차로 local 이 32.0 에 붙으면 표 밖을 읽는다
	int32 ToTileIndex(float local)
	{
		const int32 index = static_cast<int32>(std::floor(local));

		if (index < 0)
		{
			return 0;
		}

		if (index >= CHUNK_TILE_COUNT)
		{
			return CHUNK_TILE_COUNT - 1;
		}

		return index;
	}

	size_t ToSlot(const WorldPos& pos)
	{
		const int32 tile_x = ToTileIndex(pos.local_x);
		const int32 tile_y = ToTileIndex(pos.local_y);

		return static_cast<size_t>(tile_y * CHUNK_TILE_COUNT + tile_x);
	}
}

void ClearWorld()
{
	g_chunks.clear();
}

void SetTileBlocked(const WorldPos& pos, bool blocked)
{
	ChunkTiles& chunk = g_chunks[MakeChunkKey(pos.chunk_x, pos.chunk_y)];
	chunk.blocked[ToSlot(pos)] = blocked;
}

bool IsTileBlocked(const WorldPos& pos)
{
	const auto iter = g_chunks.find(MakeChunkKey(pos.chunk_x, pos.chunk_y));

	// 시작 구역 밖 ( 7단계 청크 로드 전까지 )
	if (iter == g_chunks.end())
	{
		return true;
	}

	return iter->second.blocked[ToSlot(pos)];
}

bool IsBodyBlocked(const WorldPos& pos, float radius)
{
	// 몸 폭이 한 칸보다 좁아서 걸친 칸은 모서리 넷 중 하나가 반드시 품는다
	const std::array<float, 2> signs = { -1.f, 1.f };

	for (float sign_x : signs)
	{
		for (float sign_y : signs)
		{
			WorldPos corner = pos;
			MoveWorldPos(&corner, radius * sign_x, radius * sign_y);

			if (IsTileBlocked(corner))
			{
				return true;
			}
		}
	}

	return false;
}

void MoveWithCollision(WorldPos* pos, float radius, float delta_x, float delta_y)
{
	// 축을 나눠야 벽에 비스듬히 닿아도 나란한 쪽으로 미끄러진다
	WorldPos next_x = *pos;
	MoveWorldPos(&next_x, delta_x, 0.f);

	if (IsBodyBlocked(next_x, radius) == false)
	{
		*pos = next_x;
	}

	WorldPos next_y = *pos;
	MoveWorldPos(&next_y, 0.f, delta_y);

	if (IsBodyBlocked(next_y, radius) == false)
	{
		*pos = next_y;
	}
}
