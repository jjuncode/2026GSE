#pragma once

#include "WorldPos.h"

#include <vector>

constexpr int32 ZONE_CHUNK_RADIUS = 5;	// 원점 청크 둘레 다섯 겹 ( 11 x 11 청크 )
constexpr int32 ZONE_TILE_COUNT = CHUNK_TILE_COUNT * (ZONE_CHUNK_RADIUS * 2 + 1);

// 성채 하나 + 둘레 개척촌 여섯. 0 번이 성채
constexpr int32 ZONE_VILLAGE_COUNT = 7;
constexpr int32 ZONE_CITADEL = 0;
constexpr uint8 ZONE_NO_VILLAGE = 255;

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

constexpr int32 SITE_TOWN = 0;	// 마을 광장. 시장 · 밥 · 대장간 · 게시판
constexpr int32 SITE_GATE = 1;
constexpr int32 SITE_OUTPOST = 2;	// 교역로 중간 개척대 초소
constexpr int32 SITE_HIDEOUT = 3;	// 약탈자 소굴
constexpr int32 SITE_VILLAGE = 4;	// 폐촌
constexpr int32 SITE_LAIR = 5;	// 성문파쇄자 둥지

struct ZoneSite
{
	ZoneTile tile;
	int32 kind = SITE_TOWN;
	int32 village = -1;	// 광장과 성문만. 그 마을 번호
	int32 walk_distance = 0;	// 성채 광장에서 걸어서
};

// 집 한 채. 주인은 NPC 쪽이 정한다
struct ZoneHouse
{
	int32 village = 0;
	ZoneTile lot;	// 가장 작은 x, y 칸
	int32 size_x = 2;
	int32 size_y = 2;
	ZoneTile door;	// 문 앞 빈 칸. 드나드는 자리
	bool door_on_x = false;	// 문이 +x 면에 있다. 아니면 +y 면
};

struct ZoneVillage
{
	ZoneTile center;
	ZoneTile board;	// 게시판
	int32 radius = 0;	// 이 안이 마을. 몬스터가 표적을 놓는다
	int32 site = -1;	// 광장 거점 번호
	bool is_citadel = false;
	std::vector<int32> neighbors;	// 길로 바로 이어진 마을
	std::vector<ZoneTile> entries;	// 길이 마을 경계를 넘는 칸. 새 사람이 들어오는 자리
};

// 마을 둘을 잇는 교역로. 칸 목록은 a 쪽에서 b 쪽으로
struct ZoneRoad
{
	int32 village_a = 0;
	int32 village_b = 0;
	std::vector<ZoneTile> path;
};

struct ZoneLayout
{
	uint32 seed = 0;
	std::vector<uint8> tiles;	// 한 변 ZONE_TILE_COUNT
	std::vector<int32> walk_distance;	// 성채 광장에서 걸어간 칸 수. 못 가면 -1
	std::vector<uint8> village_map;	// 칸마다 마을 번호. 밖이면 ZONE_NO_VILLAGE
	ZoneTile center;	// 성채 광장 한가운데
	ZoneTile spawn;
	ZoneTile boss;
	std::vector<ZonePack> packs;
	std::vector<ZoneSite> sites;
	std::vector<ZoneVillage> villages;
	std::vector<ZoneHouse> houses;
	std::vector<ZoneRoad> roads;
	std::vector<ZoneTile> herbs;	// 늪가 약초
	std::vector<ZoneTile> relics;	// 폐촌 유품과 유해
	std::vector<ZoneTile> lamps;	// 마을 등불
};

// 빈 칸은 전부 성채 광장에서 걸어서 닿는다
void GenerateZone(uint32 seed, ZoneLayout* layout);

bool IsBlockingTile(uint8 tile);
uint8 GetZoneTile(const ZoneLayout& layout, int32 x, int32 y);	// 구역 밖은 균열
int32 GetZoneWalkDistance(const ZoneLayout& layout, int32 x, int32 y);	// 못 가면 -1
bool IsInsideTown(const ZoneLayout& layout, int32 x, int32 y);	// 종벽 안쪽
int32 GetZoneVillageAt(const ZoneLayout& layout, int32 x, int32 y);	// 마을 밖이면 -1
WorldPos ZoneTileToWorld(int32 x, int32 y, float offset_x, float offset_y);
bool WorldToZoneTile(const WorldPos& pos, int32* x, int32* y);	// 구역 밖이면 false

uint32 HashZoneTile(uint32 seed, int32 x, int32 y);
float HashToUnit(uint32 hash);
