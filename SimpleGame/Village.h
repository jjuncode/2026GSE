#pragma once

#include "WorldPos.h"
#include "ZoneGen.h"

#include <string>
#include <vector>

// 마을 하나의 살림. 자리는 구역 생성이 정하고, 금고와 최근 일은 여기서 굴린다
struct Village
{
	std::wstring name;
	ZoneTile center;
	ZoneTile board;
	int32 radius = 0;
	int32 site = -1;	// 광장 거점
	bool is_citadel = false;
	std::vector<int32> neighbors;
	std::vector<ZoneTile> entries;
	std::vector<int32> houses;

	int32 treasury = 0;
	float treasury_carry = 0.f;
	bool treasury_warned = false;

	// 천천히 잊는 최근 일. 소식 보고서와 평판 숫자가 여기서 나온다
	float recent_deaths = 0.f;
	float recent_robberies = 0.f;
	float recent_trade = 0.f;
	int32 total_deaths = 0;
	int32 moved_in = 0;
	int32 moved_out = 0;
	float report_timer = 0.f;
};

struct House
{
	int32 village = 0;
	ZoneTile lot;
	int32 size_x = 2;
	int32 size_y = 2;
	ZoneTile door;
	int32 owner = -1;	// NPC 번호
	int32 reserved_by = -1;	// 이사 오기로 한 NPC. 오는 사이 다른 사람이 못 든다
};

void BuildVillages(const ZoneLayout& layout);
void UpdateVillages(float delta_seconds);

int32 GetVillageCount();
Village& GetVillage(int32 village);
const wchar_t* GetVillageName(int32 village);	// 없으면 "벽 밖"
int32 GetVillageAt(const WorldPos& pos);	// 마을 밖이면 -1
int32 GetVillageOfSite(int32 site);	// 광장 · 성문이 아니면 -1
int32 FindNearestVillage(const WorldPos& pos);
float GetVillageDistance(int32 village, const WorldPos& pos);	// 광장에서
WorldPos GetVillageCenterPos(int32 village);
WorldPos GetBoardPos(int32 village);
WorldPos GetVillageEntryPos(int32 village, int32 order);	// 길이 마을로 들어서는 자리

// 집
int32 GetHouseCount();
House& GetHouse(int32 house);
WorldPos GetHouseDoorPos(int32 house);
int32 ClaimFreeHouse(int32 village, int32 npc);	// 빈집이 없으면 -1
int32 ReserveFreeHouse(int32 village, int32 npc);
bool MoveIntoHouse(int32 house, int32 npc);	// 예약한 집에 든다
void ReleaseHouse(int32 house, int32 npc);	// 주인 · 예약 둘 다 푼다
int32 CountFreeHouses(int32 village);
int32 CountResidents(int32 village);

// 금고. 마을마다 따로 돈다
int32 GetVillageTreasury(int32 village);
void AddVillageTreasury(int32 village, int32 amount);
int32 TakeVillageTreasury(int32 village, int32 amount);	// 실제로 내준 양
int32 GetTotalTreasury();

// 최근 일 ( 가장 가까운 마을 몫으로 센다 )
void NoteVillageDeath(const WorldPos& pos);
void NoteVillageRobbery(const WorldPos& pos);
void NoteVillageTrade(int32 village, int32 amount);
void NoteVillageMove(int32 from_village, int32 to_village);
