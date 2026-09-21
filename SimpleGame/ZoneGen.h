#pragma once

#include "WorldPos.h"

#include <vector>

constexpr int32 ZONE_CHUNK_RADIUS = 1;	// 원점 청크 둘레 한 겹 ( 3 x 3 청크 )
constexpr int32 ZONE_TILE_COUNT = CHUNK_TILE_COUNT * (ZONE_CHUNK_RADIUS * 2 + 1);

constexpr uint8 TILE_OPEN = 0;
constexpr uint8 TILE_CLIFF = 1;	// 아래로 꺼진 낭떠러지
constexpr uint8 TILE_WATER = 2;
constexpr uint8 TILE_ROCK = 3;	// 외딴 바위
constexpr uint8 TILE_PALM = 4;	// 외딴 야자수

struct ZoneTile
{
	int32 x = 0;
	int32 y = 0;
};

struct ZoneLayout
{
	uint32 seed = 0;
	std::vector<uint8> tiles;	// 한 변 ZONE_TILE_COUNT
	ZoneTile spawn;
	ZoneTile boss;
	std::vector<ZoneTile> packs;
	std::vector<ZoneTile> lights;	// 0 = 스폰 모닥불, 1 ~ 2 = 보스 화로
};

// 빈 칸은 전부 스폰에서 걸어서 닿는다
void GenerateZone(uint32 seed, ZoneLayout* layout);

bool IsBlockingTile(uint8 tile);
uint8 GetZoneTile(const ZoneLayout& layout, int32 x, int32 y);	// 구역 밖은 낭떠러지
WorldPos ZoneTileToWorld(int32 x, int32 y, float offset_x, float offset_y);
bool WorldToZoneTile(const WorldPos& pos, int32* x, int32* y);	// 구역 밖이면 false

uint32 HashZoneTile(uint32 seed, int32 x, int32 y);
float HashToUnit(uint32 hash);
