#include "stdafx.h"
#include "Village.h"
#include "Board.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
	constexpr int32 CITADEL_TREASURY_START = 900;
	constexpr int32 HAMLET_TREASURY_START = 500;
	constexpr float CITADEL_INCOME_PER_SEC = 2.f;	// 왕관벽 안 세금. 없으면 벽 밖 짐값에 금고가 몇 분 만에 마른다
	constexpr float HAMLET_INCOME_PER_SEC = 1.4f;	// 주민 30 명 몫. 성채 45 명에 2 였던 비율
	constexpr float RECENT_FORGET_SEC = 240.f;	// 최근 일이 반쯤 잊히는 시간
	constexpr float REPORT_INTERVAL_SEC = 75.f;

	// 성채 이름은 세계관에 박혀 있고, 개척촌 이름은 씨앗마다 섞는다
	const wchar_t* CITADEL_NAME = L"하르벤";
	const std::array<const wchar_t*, 10> HAMLET_NAMES =
	{
		L"잿골", L"갈대여울", L"돌우물", L"까마귀재", L"회색목",
		L"등불언덕", L"버들목", L"서리내", L"검은둑", L"붉은여울",
	};

	std::vector<Village> g_villages;
	std::vector<House> g_houses;
	const ZoneLayout* g_layout = nullptr;

	bool IsValidVillage(int32 village)
	{
		return village >= 0 && village < static_cast<int32>(g_villages.size());
	}

	bool IsValidHouse(int32 house)
	{
		return house >= 0 && house < static_cast<int32>(g_houses.size());
	}

	float DistanceBetween(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	// 반감기 RECENT_FORGET_SEC 로 줄어든다
	float Forget(float value, float delta_seconds)
	{
		return value * std::exp(-delta_seconds * 0.693f / RECENT_FORGET_SEC);
	}

	void UpdateTreasury(Village* village, float delta_seconds)
	{
		float income = HAMLET_INCOME_PER_SEC;
		if (village->is_citadel)
		{
			income = CITADEL_INCOME_PER_SEC;
		}

		village->treasury_carry += income * delta_seconds;

		const int32 gained = static_cast<int32>(village->treasury_carry);
		village->treasury_carry -= static_cast<float>(gained);
		village->treasury += gained;

		if (village->treasury > 40)
		{
			village->treasury_warned = false;
		}
	}

	// 마을마다 시차를 두고 소식지를 낸다. 숫자가 편지에 실려 다른 마을 평판이 된다
	void UpdateReport(int32 index, float delta_seconds)
	{
		Village& village = g_villages[static_cast<size_t>(index)];
		village.report_timer -= delta_seconds;

		if (village.report_timer > 0.f)
		{
			return;
		}

		village.report_timer = REPORT_INTERVAL_SEC;
		PostVillageReport(index, CountResidents(index), village.treasury, static_cast<int32>(std::lround(village.recent_deaths)));
	}
}

void BuildVillages(const ZoneLayout& layout)
{
	g_layout = &layout;
	g_villages.clear();
	g_houses.clear();

	// 이름 섞기 - 씨앗으로 고정해서 같은 구역은 같은 이름
	std::array<int32, 10> order = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	for (size_t index = order.size(); index > 1; --index)
	{
		const size_t swap_index = static_cast<size_t>(HashZoneTile(layout.seed + 1777u, static_cast<int32>(index), 0) % static_cast<uint32>(index));
		std::swap(order[index - 1], order[swap_index]);
	}

	for (size_t index = 0; index < layout.villages.size(); ++index)
	{
		const ZoneVillage& source = layout.villages[index];

		Village village;
		village.center = source.center;
		village.board = source.board;
		village.radius = source.radius;
		village.site = source.site;
		village.is_citadel = source.is_citadel;
		village.neighbors = source.neighbors;
		village.entries = source.entries;

		if (village.is_citadel)
		{
			village.name = CITADEL_NAME;
			village.treasury = CITADEL_TREASURY_START;
		}
		else
		{
			village.name = HAMLET_NAMES[static_cast<size_t>(order[(index - 1) % order.size()])];
			village.treasury = HAMLET_TREASURY_START;
		}

		// 소식지가 한꺼번에 나오지 않게 시차
		village.report_timer = 8.f + static_cast<float>(index) * (REPORT_INTERVAL_SEC / static_cast<float>(layout.villages.size()));

		g_villages.push_back(village);
	}

	for (const ZoneHouse& source : layout.houses)
	{
		House house;
		house.village = source.village;
		house.lot = source.lot;
		house.size_x = source.size_x;
		house.size_y = source.size_y;
		house.door = source.door;

		if (IsValidVillage(house.village))
		{
			g_villages[static_cast<size_t>(house.village)].houses.push_back(static_cast<int32>(g_houses.size()));
		}

		g_houses.push_back(house);
	}

	for (const Village& village : g_villages)
	{
		std::cout << "[ Village - BuildVillages ] : houses " << village.houses.size() << " neighbors " << village.neighbors.size() << " entries " << village.entries.size() << "\n";
	}
}

void UpdateVillages(float delta_seconds)
{
	for (size_t index = 0; index < g_villages.size(); ++index)
	{
		Village& village = g_villages[index];

		UpdateTreasury(&village, delta_seconds);

		village.recent_deaths = Forget(village.recent_deaths, delta_seconds);
		village.recent_robberies = Forget(village.recent_robberies, delta_seconds);
		village.recent_trade = Forget(village.recent_trade, delta_seconds);

		UpdateReport(static_cast<int32>(index), delta_seconds);
	}
}

int32 GetVillageCount()
{
	return static_cast<int32>(g_villages.size());
}

Village& GetVillage(int32 village)
{
	return g_villages[static_cast<size_t>(village)];
}

const wchar_t* GetVillageName(int32 village)
{
	if (IsValidVillage(village) == false)
	{
		return L"벽 밖";
	}

	return g_villages[static_cast<size_t>(village)].name.c_str();
}

int32 GetVillageAt(const WorldPos& pos)
{
	if (g_layout == nullptr)
	{
		return -1;
	}

	int32 x = 0;
	int32 y = 0;
	if (WorldToZoneTile(pos, &x, &y) == false)
	{
		return -1;
	}

	return GetZoneVillageAt(*g_layout, x, y);
}

int32 GetVillageOfSite(int32 site)
{
	if (g_layout == nullptr || site < 0 || site >= static_cast<int32>(g_layout->sites.size()))
	{
		return -1;
	}

	return g_layout->sites[static_cast<size_t>(site)].village;
}

int32 FindNearestVillage(const WorldPos& pos)
{
	int32 best = -1;
	float best_distance = 1.0e9f;

	for (size_t index = 0; index < g_villages.size(); ++index)
	{
		const float distance = DistanceBetween(pos, GetVillageCenterPos(static_cast<int32>(index)));

		if (distance < best_distance)
		{
			best_distance = distance;
			best = static_cast<int32>(index);
		}
	}

	return best;
}

float GetVillageDistance(int32 village, const WorldPos& pos)
{
	if (IsValidVillage(village) == false)
	{
		return 1.0e9f;
	}

	return DistanceBetween(pos, GetVillageCenterPos(village));
}

WorldPos GetVillageCenterPos(int32 village)
{
	const ZoneTile& center = g_villages[static_cast<size_t>(village)].center;

	return ZoneTileToWorld(center.x, center.y, 0.5f, 0.5f);
}

WorldPos GetBoardPos(int32 village)
{
	const ZoneTile& board = g_villages[static_cast<size_t>(village)].board;

	return ZoneTileToWorld(board.x, board.y, 0.5f, 0.5f);
}

WorldPos GetVillageEntryPos(int32 village, int32 order)
{
	const Village& target = g_villages[static_cast<size_t>(village)];

	if (target.entries.empty())
	{
		return GetVillageCenterPos(village);
	}

	const ZoneTile& entry = target.entries[static_cast<size_t>(order) % target.entries.size()];

	return ZoneTileToWorld(entry.x, entry.y, 0.5f, 0.5f);
}

int32 GetHouseCount()
{
	return static_cast<int32>(g_houses.size());
}

House& GetHouse(int32 house)
{
	return g_houses[static_cast<size_t>(house)];
}

WorldPos GetHouseDoorPos(int32 house)
{
	const ZoneTile& door = g_houses[static_cast<size_t>(house)].door;

	// 문 앞 칸의 문 쪽 가장자리. 칸 한가운데면 문에서 한 발 떨어져 선다
	return ZoneTileToWorld(door.x, door.y, 0.5f, 0.3f);
}

int32 ClaimFreeHouse(int32 village, int32 npc)
{
	if (IsValidVillage(village) == false)
	{
		return -1;
	}

	for (int32 house : g_villages[static_cast<size_t>(village)].houses)
	{
		House& target = g_houses[static_cast<size_t>(house)];

		if (target.owner < 0 && target.reserved_by < 0)
		{
			target.owner = npc;
			return house;
		}
	}

	return -1;
}

int32 ReserveFreeHouse(int32 village, int32 npc)
{
	if (IsValidVillage(village) == false)
	{
		return -1;
	}

	for (int32 house : g_villages[static_cast<size_t>(village)].houses)
	{
		House& target = g_houses[static_cast<size_t>(house)];

		if (target.owner < 0 && target.reserved_by < 0)
		{
			target.reserved_by = npc;
			return house;
		}
	}

	return -1;
}

bool MoveIntoHouse(int32 house, int32 npc)
{
	if (IsValidHouse(house) == false)
	{
		return false;
	}

	House& target = g_houses[static_cast<size_t>(house)];

	if (target.owner >= 0 && target.owner != npc)
	{
		return false;
	}

	if (target.reserved_by >= 0 && target.reserved_by != npc)
	{
		return false;
	}

	target.owner = npc;
	target.reserved_by = -1;

	return true;
}

void ReleaseHouse(int32 house, int32 npc)
{
	if (IsValidHouse(house) == false)
	{
		return;
	}

	House& target = g_houses[static_cast<size_t>(house)];

	if (target.owner == npc)
	{
		target.owner = -1;
	}

	if (target.reserved_by == npc)
	{
		target.reserved_by = -1;
	}
}

int32 CountFreeHouses(int32 village)
{
	if (IsValidVillage(village) == false)
	{
		return 0;
	}

	int32 count = 0;

	for (int32 house : g_villages[static_cast<size_t>(village)].houses)
	{
		const House& target = g_houses[static_cast<size_t>(house)];

		if (target.owner < 0 && target.reserved_by < 0)
		{
			count += 1;
		}
	}

	return count;
}

int32 CountResidents(int32 village)
{
	if (IsValidVillage(village) == false)
	{
		return 0;
	}

	int32 count = 0;

	for (int32 house : g_villages[static_cast<size_t>(village)].houses)
	{
		if (g_houses[static_cast<size_t>(house)].owner >= 0)
		{
			count += 1;
		}
	}

	return count;
}

int32 GetVillageTreasury(int32 village)
{
	if (IsValidVillage(village) == false)
	{
		return 0;
	}

	return g_villages[static_cast<size_t>(village)].treasury;
}

void AddVillageTreasury(int32 village, int32 amount)
{
	if (IsValidVillage(village) == false)
	{
		return;
	}

	g_villages[static_cast<size_t>(village)].treasury += amount;
}

int32 TakeVillageTreasury(int32 village, int32 amount)
{
	if (IsValidVillage(village) == false)
	{
		return 0;
	}

	Village& target = g_villages[static_cast<size_t>(village)];

	int32 paid = amount;
	if (paid > target.treasury)
	{
		paid = target.treasury;
	}

	target.treasury -= paid;

	if (target.treasury <= 0 && target.treasury_warned == false)
	{
		target.treasury_warned = true;
		PostNews(village, NEWS_TREASURY, target.name + L" 금고가 바닥났다. 급료와 짐값이 끊긴다");
	}

	return paid;
}

int32 GetTotalTreasury()
{
	int32 total = 0;

	for (const Village& village : g_villages)
	{
		total += village.treasury;
	}

	return total;
}

void NoteVillageDeath(const WorldPos& pos)
{
	const int32 village = FindNearestVillage(pos);
	if (IsValidVillage(village) == false)
	{
		return;
	}

	g_villages[static_cast<size_t>(village)].recent_deaths += 1.f;
	g_villages[static_cast<size_t>(village)].total_deaths += 1;
}

void NoteVillageRobbery(const WorldPos& pos)
{
	const int32 village = FindNearestVillage(pos);
	if (IsValidVillage(village) == false)
	{
		return;
	}

	g_villages[static_cast<size_t>(village)].recent_robberies += 1.f;
}

void NoteVillageTrade(int32 village, int32 amount)
{
	if (IsValidVillage(village) == false)
	{
		return;
	}

	g_villages[static_cast<size_t>(village)].recent_trade += static_cast<float>(amount);
}

void NoteVillageMove(int32 from_village, int32 to_village)
{
	if (IsValidVillage(from_village))
	{
		g_villages[static_cast<size_t>(from_village)].moved_out += 1;
	}

	if (IsValidVillage(to_village))
	{
		g_villages[static_cast<size_t>(to_village)].moved_in += 1;
	}
}
