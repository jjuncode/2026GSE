#pragma once

#include "WorldPos.h"

#include <vector>

constexpr int32 ZONE_CHUNK_RADIUS = 3;	// 원점 청크 둘레 세 겹 ( 7 x 7 청크 )
constexpr int32 ZONE_TILE_COUNT = CHUNK_TILE_COUNT * (ZONE_CHUNK_RADIUS * 2 + 1);

// 걸을 수 있는 칸
constexpr uint8 TILE_OPEN = 0;	// 잿빛 흙
constexpr uint8 TILE_ROAD = 1;	// 교역로와 성문 앞
constexpr uint8 TILE_PLAZA = 2;	// 성 안 돌길
constexpr uint8 TILE_FIELD = 3;	// 밭
constexpr uint8 TILE_BLIGHT = 4;	// 재에 물든 땅

// 막힌 칸은 10 부터
constexpr uint8 TILE_CLIFF = 10;	// 꺼진 균열
constexpr uint8 TILE_WATER = 11;	// 검은 늪
constexpr uint8 TILE_ROCK = 12;
constexpr uint8 TILE_TREE = 13;	// 죽은 나무
constexpr uint8 TILE_WALL = 14;	// 종벽
constexpr uint8 TILE_TOWER = 15;	// 성문 망루
constexpr uint8 TILE_HOUSE = 16;
constexpr uint8 TILE_RUIN = 17;	// 폐촌 벽 조각

struct ZoneTile
{
	int32 x = 0;
	int32 y = 0;
};

// 성문에서 걸어간 거리로 가르는 난이도 띠
constexpr int32 BAND_INTRO = 0;	// 성문 앞 - 타락자뿐
constexpr int32 BAND_FIELD = 1;	// 들판 - 재늑대와 가끔 거식귀
constexpr int32 BAND_OUTER = 2;	// 깊은 곳 - 거식귀 짝과 정예

struct ZonePack
{
	ZoneTile tile;
	int32 band = BAND_INTRO;
};

constexpr int32 SITE_TOWN = 0;	// 성채 광장 시장
constexpr int32 SITE_GATE = 1;
constexpr int32 SITE_OUTPOST = 2;	// 개척대 초소
constexpr int32 SITE_HIDEOUT = 3;	// 약탈자 소굴
constexpr int32 SITE_VILLAGE = 4;	// 폐촌
constexpr int32 SITE_LAIR = 5;	// 성문파쇄자 둥지

struct ZoneSite
{
	ZoneTile tile;
	int32 kind = SITE_TOWN;
	int32 walk_distance = 0;	// 광장에서 걸어서
};

struct ZoneLayout
{
	uint32 seed = 0;
	std::vector<uint8> tiles;	// 한 변 ZONE_TILE_COUNT
	std::vector<int32> walk_distance;	// 광장에서 걸어간 칸 수. 못 가면 -1
	ZoneTile center;	// 성채 광장 한가운데
	ZoneTile spawn;
	ZoneTile boss;
	std::vector<ZonePack> packs;
	std::vector<ZoneSite> sites;
	std::vector<ZoneTile> herbs;	// 늪가 약초
	std::vector<ZoneTile> relics;	// 폐촌 유품과 유해
	std::vector<ZoneTile> lamps;	// 성 안 등불
};

// 빈 칸은 전부 광장에서 걸어서 닿는다
void GenerateZone(uint32 seed, ZoneLayout* layout);

bool IsBlockingTile(uint8 tile);
uint8 GetZoneTile(const ZoneLayout& layout, int32 x, int32 y);	// 구역 밖은 균열
int32 GetZoneWalkDistance(const ZoneLayout& layout, int32 x, int32 y);	// 못 가면 -1
bool IsInsideTown(const ZoneLayout& layout, int32 x, int32 y);	// 종벽 안쪽
WorldPos ZoneTileToWorld(int32 x, int32 y, float offset_x, float offset_y);
bool WorldToZoneTile(const WorldPos& pos, int32* x, int32* y);	// 구역 밖이면 false

uint32 HashZoneTile(uint32 seed, int32 x, int32 y);
float HashToUnit(uint32 hash);
