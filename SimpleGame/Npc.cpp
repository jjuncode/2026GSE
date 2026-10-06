#include "stdafx.h"
#include "Npc.h"
#include "Balance.h"
#include "Drop.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Monster.h"
#include "NavGrid.h"
#include "NpcBrain.h"
#include "NpcData.h"
#include "Player.h"
#include "Renderer.h"
#include "Scene.h"
#include "SceneGraph.h"
#include "SpriteAtlas.h"
#include "World.h"
#include "ZoneGen.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	constexpr int32 WARDEN_COUNT = 10;
	constexpr int32 HUNTER_COUNT = 9;
	constexpr int32 GATHERER_COUNT = 11;
	constexpr int32 MERCHANT_COUNT = 6;
	constexpr int32 RAIDER_COUNT = 9;

	constexpr float HUNGER_FULL_SEC = 170.f;
	constexpr float STARVE_DAMAGE_SEC = 3.f;
	constexpr float NATURAL_REGEN_DELAY = 6.f;
	constexpr float NATURAL_REGEN_PER_SEC = 0.3f;
	constexpr float THREAT_MEMORY_SEC = 6.f;
	constexpr float OUTLAW_SEC = 90.f;
	constexpr float ARRIVAL_MIN_SEC = 40.f;
	constexpr float ARRIVAL_MAX_SEC = 70.f;
	constexpr float CORPSE_LIFE_SEC = 240.f;
	constexpr size_t GRAVE_MAX_COUNT = 48;
	constexpr float ARROW_FLIGHT_SEC = 0.22f;
	constexpr float EVENT_LIFE_SEC = 18.f;
	constexpr size_t EVENT_MAX_COUNT = 7;
	constexpr float CENSUS_SEC = 15.f;
	constexpr float DIRECT_STEER_RANGE = 5.f;
	constexpr float ENGAGE_GIVE_UP = 22.f;
	constexpr float SEPARATION_DISTANCE = 0.5f;
	constexpr float SEPARATION_SPEED = 0.9f;
	constexpr int32 TREASURY_START = 600;
	constexpr float TREASURY_INCOME_PER_SEC = 2.f;	// 왕관벽 안 세금. 없으면 벽 밖 짐값에 금고가 몇 분 만에 마른다
	constexpr float TEXT_NEAR_RANGE = 22.f;	// 이보다 먼 NPC 의 떠오르는 글자는 안 띄운다
	constexpr int32 OUTLAW_BOUNTY = 20;
	constexpr float WAYPOINT_REACH = 0.35f;
	constexpr float PROGRESS_CHECK_SEC = 1.2f;
	constexpr int32 PATH_FAIL_LIMIT = 4;
	constexpr float DEVOUR_HP_RATIO = 0.3f;	// 거식귀에게 이 아래로 몰리면 먹힌다

	struct Arrival
	{
		int32 job = NPC_JOB_HUNTER;
		float timer = 0.f;
	};

	const std::array<const wchar_t*, 56> NAMES =
	{
		L"로안", L"카심", L"도르", L"에바", L"미렐", L"하겐", L"브란", L"시그",
		L"올라프", L"레아", L"토린", L"가렛", L"유리엔", L"세라", L"마르코", L"이델",
		L"칼렌", L"노아", L"베른", L"휴고", L"오린", L"다리온", L"펠릭스", L"리사",
		L"크누트", L"에린", L"멜", L"보리스", L"잔", L"테오", L"루카", L"한스",
		L"요르크", L"일사", L"마야", L"프리다", L"오스카", L"벨", L"리안", L"그레타",
		L"아스트", L"빌름", L"코라", L"데인", L"니브", L"사울", L"타리크", L"엘로",
		L"무라", L"키란", L"페드로", L"야나", L"로크", L"수엔", L"이반", L"헬가",
	};

	const char* GetTaskTag(int32 task);

	std::vector<Npc> g_npcs;
	std::vector<ZoneSite> g_sites;
	std::vector<NpcResource> g_resources;
	std::vector<NpcCorpse> g_corpses;
	std::vector<Actor> g_graves;
	std::vector<NpcArrow> g_arrows;
	std::vector<NpcEvent> g_events;
	std::vector<Arrival> g_arrivals;
	const ZoneLayout* g_layout = nullptr;

	int32 g_treasury = 0;
	int32 g_next_corpse_id = 1;
	float g_treasury_carry = 0.f;
	bool g_treasury_warned = false;

	int32 g_selected = -1;
	bool g_follow = false;

	float g_census_timer = 0.f;
	int32 g_death_count = 0;
	int32 g_robbery_count = 0;
	float g_tick_accum_us = 0.f;
	int32 g_tick_samples = 0;
	float g_tick_average_us = 0.f;

	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x7f4a7c15u);
		return random;
	}

	float RollRange(float min_value, float max_value)
	{
		std::uniform_real_distribution<float> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	int32 RollInt(int32 min_value, int32 max_value)
	{
		std::uniform_int_distribution<int32> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	float CountDown(float value, float delta_seconds)
	{
		const float next = value - delta_seconds;

		if (next < 0.f)
		{
			return 0.f;
		}

		return next;
	}

	float DistanceBetween(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	bool IsValidNpc(int32 index)
	{
		if (index < 0 || index >= static_cast<int32>(g_npcs.size()))
		{
			return false;
		}

		return g_npcs[static_cast<size_t>(index)].is_alive;
	}

	int32 GetSpriteOfJob(int32 job)
	{
		switch (job)
		{
			case NPC_JOB_RAIDER:
			{
				return SPRITE_NPC_RAIDER;
			}
			case NPC_JOB_MERCHANT:
			{
				return SPRITE_NPC_MERCHANT;
			}
			case NPC_JOB_GATHERER:
			{
				return SPRITE_NPC_GATHERER;
			}
			case NPC_JOB_WARDEN:
			{
				return SPRITE_NPC_WARDEN;
			}
			default:
			{
				return SPRITE_NPC_HUNTER;
			}
		}
	}

	void ApplyJobStats(Npc* npc)
	{
		switch (npc->job)
		{
			case NPC_JOB_RAIDER:
			{
				npc->max_hp = 60;
				npc->attack = 8;
				npc->move_speed = 3.4f;
				npc->attack_range = 1.1f;
				npc->attack_interval = 1.0f;
				npc->goods_capacity = 80;
				npc->gold = RollInt(5, 20);
				break;
			}
			case NPC_JOB_MERCHANT:
			{
				npc->max_hp = 50;
				npc->attack = 3;
				npc->move_speed = 2.6f;
				npc->attack_range = 1.1f;
				npc->attack_interval = 1.3f;
				npc->goods_capacity = 400;
				npc->gold = RollInt(120, 200);
				break;
			}
			case NPC_JOB_GATHERER:
			{
				npc->max_hp = 45;
				npc->attack = 3;
				npc->move_speed = 2.9f;
				npc->attack_range = 1.1f;
				npc->attack_interval = 1.2f;
				npc->goods_capacity = 50;
				npc->gold = RollInt(10, 25);
				break;
			}
			case NPC_JOB_WARDEN:
			{
				npc->max_hp = 95;
				npc->attack = 10;
				npc->move_speed = 3.0f;
				npc->attack_range = 1.4f;
				npc->attack_interval = 1.0f;
				npc->goods_capacity = 40;
				npc->gold = RollInt(30, 50);
				break;
			}
			default:
			{
				// 사냥꾼은 활이라 멀리서 쏜다
				npc->max_hp = 70;
				npc->attack = 7;
				npc->move_speed = 3.1f;
				npc->attack_range = 4.2f;
				npc->attack_interval = 1.2f;
				npc->goods_capacity = 60;
				npc->gold = RollInt(20, 40);
				break;
			}
		}
	}

	std::wstring PickName()
	{
		const size_t start = static_cast<size_t>(RollInt(0, static_cast<int32>(NAMES.size()) - 1));

		for (size_t step = 0; step < NAMES.size(); ++step)
		{
			const wchar_t* candidate = NAMES[(start + step) % NAMES.size()];
			bool used = false;

			for (const Npc& npc : g_npcs)
			{
				if (npc.is_alive && npc.name == candidate)
				{
					used = true;
					break;
				}
			}

			if (used == false)
			{
				return candidate;
			}
		}

		return std::wstring(NAMES[start]) + std::to_wstring(RollInt(2, 9));
	}

	WorldPos FindSpawnNear(int32 site)
	{
		const ZoneTile center = g_sites[static_cast<size_t>(site)].tile;

		for (int32 attempt = 0; attempt < 24; ++attempt)
		{
			const int32 x = center.x + RollInt(-4, 4);
			const int32 y = center.y + RollInt(-4, 4);

			if (IsNavBlocked(x, y) == false)
			{
				return ZoneTileToWorld(x, y, RollRange(0.3f, 0.7f), RollRange(0.3f, 0.7f));
			}
		}

		return ZoneTileToWorld(center.x, center.y, 0.5f, 0.5f);
	}

	int32 FindSiteOfKind(int32 kind, int32 order)
	{
		int32 seen = 0;
		int32 first = -1;

		for (size_t index = 0; index < g_sites.size(); ++index)
		{
			if (g_sites[index].kind != kind)
			{
				continue;
			}

			if (first < 0)
			{
				first = static_cast<int32>(index);
			}

			if (seen == order)
			{
				return static_cast<int32>(index);
			}

			seen += 1;
		}

		// 순번이 넘치면 처음 것으로 돌린다
		return first;
	}

	int32 CountSitesOfKind(int32 kind)
	{
		int32 count = 0;

		for (const ZoneSite& site : g_sites)
		{
			if (site.kind == kind)
			{
				count += 1;
			}
		}

		return count;
	}

	int32 PickHomeSite(int32 job, int32 order)
	{
		const int32 town = FindSiteOfKind(SITE_TOWN, 0);

		if (job == NPC_JOB_RAIDER)
		{
			const int32 hideouts = CountSitesOfKind(SITE_HIDEOUT);
			if (hideouts > 0)
			{
				return FindSiteOfKind(SITE_HIDEOUT, order % hideouts);
			}

			const int32 villages = CountSitesOfKind(SITE_VILLAGE);
			if (villages > 0)
			{
				return FindSiteOfKind(SITE_VILLAGE, order % villages);
			}

			return town;
		}

		if (job == NPC_JOB_MERCHANT)
		{
			return town;
		}

		// 절반쯤은 성채, 나머지는 초소에 기대 산다
		const int32 outposts = CountSitesOfKind(SITE_OUTPOST);
		if (outposts > 0 && order % 5 >= 2)
		{
			return FindSiteOfKind(SITE_OUTPOST, order % outposts);
		}

		return town;
	}

	void InitNpc(Npc* npc, int32 job, int32 home_site, const WorldPos& pos)
	{
		*npc = Npc();
		npc->job = job;
		npc->home_site = home_site;
		npc->is_alive = true;
		npc->name = PickName();

		ApplyJobStats(npc);
		npc->hp = npc->max_hp;

		npc->greed = RollRange(0.5f, 1.5f);
		npc->bravery = RollRange(0.5f, 1.5f);
		npc->cruelty = RollRange(0.5f, 1.5f);
		npc->diligence = RollRange(0.5f, 1.5f);

		// 들개단은 잔혹한 쪽으로 기운다
		if (job == NPC_JOB_RAIDER)
		{
			npc->cruelty = RollRange(0.9f, 1.5f);
		}

		// 모두 같은 순간에 배고프면 정오에 일제히 밥 먹으러 가는 시계 장치가 된다
		npc->hunger = RollRange(0.f, 0.55f);

		npc->body.m_pos = pos;
		npc->body.m_sprite = GetSpriteOfJob(job);
		npc->body.m_flip = RollRange(0.f, 1.f) > 0.5f;
		npc->body.m_shade = RollRange(0.88f, 1.05f);
		npc->spot = pos;
		npc->progress_pos = pos;
		npc->sense_timer = RollRange(0.f, 0.4f);

		InitNpcBrain(npc, static_cast<uint32>(GetRandom()()));
	}

	void SpawnInitial(int32 job, int32 count)
	{
		for (int32 order = 0; order < count; ++order)
		{
			const int32 home = PickHomeSite(job, order);

			Npc npc;
			g_npcs.push_back(npc);
			InitNpc(&g_npcs.back(), job, home, FindSpawnNear(home));
		}
	}

	void BuildResources(const ZoneLayout& layout)
	{
		for (const ZoneTile& tile : layout.herbs)
		{
			NpcResource resource;
			resource.tile = tile;
			resource.kind = RESOURCE_HERB;
			resource.value = 5;
			resource.body.m_pos = ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f);
			resource.body.m_sprite = SPRITE_HERB;
			resource.body.m_emissive = 1.5f;
			resource.body.m_casts_shadow = false;
			g_resources.push_back(resource);
		}

		for (const ZoneTile& tile : layout.relics)
		{
			NpcResource resource;
			resource.tile = tile;
			resource.kind = RESOURCE_RELIC;
			resource.value = 9;
			resource.body.m_pos = ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f);
			resource.body.m_sprite = SPRITE_BONES;
			resource.body.m_emissive = 1.25f;
			resource.body.m_casts_shadow = false;
			g_resources.push_back(resource);
		}
	}

	bool IsNearCamera(const WorldPos& pos)
	{
		return DistanceBetween(pos, GetCameraPos()) < TEXT_NEAR_RANGE;
	}

	void ClearReferencesTo(int32 index)
	{
		ForgetMonsterTargetNpc(index);

		for (Npc& npc : g_npcs)
		{
			if (npc.target_kind == NPC_TARGET_NPC && npc.target_index == index)
			{
				npc.target_kind = NPC_TARGET_NONE;
				npc.target_index = -1;
			}

			if (npc.threat_kind == NPC_TARGET_NPC && npc.threat_index == index)
			{
				npc.threat_kind = NPC_TARGET_NONE;
				npc.threat_index = -1;
				npc.threat_timer = 0.f;
			}

			if (npc.enemy_kind == NPC_TARGET_NPC && npc.enemy_index == index)
			{
				npc.enemy_kind = NPC_TARGET_NONE;
				npc.enemy_index = -1;
			}
		}
	}

	void AddGrave(const WorldPos& pos)
	{
		Actor grave;
		grave.m_pos = pos;
		grave.m_sprite = SPRITE_GRAVE;
		grave.m_shade = 0.9f;

		if (g_graves.size() >= GRAVE_MAX_COUNT)
		{
			g_graves.erase(g_graves.begin());
		}

		g_graves.push_back(grave);
	}

	void KillNpc(int32 index, const std::wstring& cause, bool drop_gold_to_player)
	{
		Npc& npc = g_npcs[static_cast<size_t>(index)];

		npc.is_alive = false;
		npc.hp = 0;
		npc.stall_heartbeat = 0.f;
		ResetNpcBrain(&npc);

		// 주머니는 남는다. 약탈자와 채집꾼이 뒤지러 온다
		NpcCorpse corpse;
		corpse.body.m_pos = npc.body.m_pos;
		corpse.body.m_sprite = SPRITE_POUCH;
		corpse.body.m_casts_shadow = false;
		corpse.id = g_next_corpse_id;
		corpse.goods = npc.goods;
		corpse.gold = npc.gold;
		g_next_corpse_id += 1;

		if (drop_gold_to_player)
		{
			SpawnMonsterDrops(npc.body.m_pos, 10 + npc.level * 4, 0, 0, DROP_TIER_NORMAL);
			SpawnGoldDrop(npc.body.m_pos, npc.gold);
			corpse.gold = 0;
		}

		if (corpse.gold > 0 || corpse.goods > 0)
		{
			g_corpses.push_back(corpse);
		}

		AddGrave(npc.body.m_pos);

		PushNpcEvent(WithSubject(GetNpcTitle(npc)) + cause, 0.95f, 0.40f, 0.35f);

		ClearReferencesTo(index);

		if (g_selected == index)
		{
			g_follow = false;
		}

		Arrival arrival;
		arrival.job = npc.job;
		arrival.timer = RollRange(ARRIVAL_MIN_SEC, ARRIVAL_MAX_SEC);
		g_arrivals.push_back(arrival);

		g_death_count += 1;
	}

	const char* GetJobTag(int32 job)
	{
		switch (job)
		{
			case NPC_JOB_RAIDER:
			{
				return "raider";
			}
			case NPC_JOB_MERCHANT:
			{
				return "merchant";
			}
			case NPC_JOB_GATHERER:
			{
				return "gatherer";
			}
			case NPC_JOB_WARDEN:
			{
				return "warden";
			}
			default:
			{
				return "hunter";
			}
		}
	}

	// 사망률이 곧 밸런스다. 누가 무엇을 하다 무엇에 죽었나를 남긴다
	void LogDeath(const Npc& npc, int32 attacker_kind, int32 attacker_index)
	{
		std::cout << "[ Npc - LogDeath ] : " << GetJobTag(npc.job) << " lv " << npc.level << " during " << GetTaskTag(npc.task) << " by ";

		if (attacker_kind == NPC_TARGET_MONSTER)
		{
			std::cout << "monster kind " << GetMonsterKind(attacker_index) << "\n";
		}
		else if (attacker_kind == NPC_TARGET_NPC && attacker_index >= 0 && attacker_index < static_cast<int32>(g_npcs.size()))
		{
			std::cout << GetJobTag(g_npcs[static_cast<size_t>(attacker_index)].job) << "\n";
		}
		else if (attacker_kind == NPC_TARGET_PLAYER)
		{
			std::cout << "player\n";
		}
		else
		{
			std::cout << "hunger\n";
		}
	}

	std::wstring DescribeKiller(int32 attacker_kind, int32 attacker_index, bool devoured)
	{
		if (attacker_kind == NPC_TARGET_MONSTER)
		{
			const std::wstring name = GetMonsterKindName(GetMonsterKind(attacker_index));

			if (devoured)
			{
				return L" " + name + L"에게 먹혔다";
			}

			return L" " + name + L"에게 쓰러졌다";
		}

		if (attacker_kind == NPC_TARGET_NPC && attacker_index >= 0 && attacker_index < static_cast<int32>(g_npcs.size()))
		{
			return L" " + GetNpcTitle(g_npcs[static_cast<size_t>(attacker_index)]) + L"에게 베였다";
		}

		if (attacker_kind == NPC_TARGET_PLAYER)
		{
			return L" 개척대원에게 처단당했다";
		}

		return L" 굶어 죽었다";
	}

	void HurtNpc(int32 index, int32 damage, int32 attacker_kind, int32 attacker_index)
	{
		if (IsValidNpc(index) == false)
		{
			return;
		}

		Npc& npc = g_npcs[static_cast<size_t>(index)];

		npc.hp -= damage;
		npc.hit_flash = Balance::HIT_FLASH_SEC;
		npc.since_hit = 0.f;

		if (attacker_kind != NPC_TARGET_NONE)
		{
			npc.threat_kind = attacker_kind;
			npc.threat_index = attacker_index;
			npc.threat_timer = THREAT_MEMORY_SEC;
		}

		if (IsNearCamera(npc.body.m_pos))
		{
			NpcShowText(npc, std::to_wstring(damage), 1.f, 0.70f, 0.60f);
		}

		// 거인은 사람을 끝까지 두들기지 않는다. 몰리면 집어 삼킨다
		bool devoured = false;
		if (attacker_kind == NPC_TARGET_MONSTER && npc.hp > 0)
		{
			const int32 kind = GetMonsterKind(attacker_index);
			const bool is_giant = kind == MONSTER_DEVOURER || kind == MONSTER_GATEBREAKER;
			const float ratio = static_cast<float>(npc.hp) / static_cast<float>(npc.max_hp);

			if (is_giant && ratio < DEVOUR_HP_RATIO)
			{
				npc.hp = 0;
				devoured = true;
			}
		}

		if (npc.hp > 0)
		{
			return;
		}

		if (attacker_kind == NPC_TARGET_MONSTER)
		{
			const int32 kind = GetMonsterKind(attacker_index);
			if (kind == MONSTER_DEVOURER || kind == MONSTER_GATEBREAKER)
			{
				devoured = true;
			}
		}

		LogDeath(npc, attacker_kind, attacker_index);
		KillNpc(index, DescribeKiller(attacker_kind, attacker_index, devoured), attacker_kind == NPC_TARGET_PLAYER);
	}

	void SpawnArrival(const Arrival& arrival)
	{
		int32 slot = -1;

		for (size_t index = 0; index < g_npcs.size(); ++index)
		{
			if (g_npcs[index].is_alive == false)
			{
				slot = static_cast<int32>(index);
				break;
			}
		}

		if (slot < 0)
		{
			return;
		}

		const int32 home = PickHomeSite(arrival.job, RollInt(0, 9));
		int32 entry = home;

		// 벽 안 사람은 성문을 나서서 온다
		if (arrival.job != NPC_JOB_RAIDER)
		{
			const int32 gates = CountSitesOfKind(SITE_GATE);
			if (gates > 0)
			{
				entry = FindSiteOfKind(SITE_GATE, RollInt(0, gates - 1));
			}
		}

		// 슬롯을 다시 쓴다. 벡터가 커지면 몬스터와 NPC 가 들고 있는 번호가 어긋난다
		Npc& npc = g_npcs[static_cast<size_t>(slot)];
		InitNpc(&npc, arrival.job, home, FindSpawnNear(entry));

		if (arrival.job == NPC_JOB_RAIDER)
		{
			PushNpcEvent(L"들개단에 새 얼굴 " + WithSubject(npc.name) + L" 합류했다", 0.85f, 0.45f, 0.40f);
		}
		else
		{
			PushNpcEvent(L"새 " + WithSubject(GetNpcTitle(npc)) + L" 성문을 나섰다", 0.45f, 0.85f, 0.80f);
		}
	}

	void UpdateArrivals(float delta_seconds)
	{
		for (size_t index = 0; index < g_arrivals.size();)
		{
			g_arrivals[index].timer -= delta_seconds;

			if (g_arrivals[index].timer > 0.f)
			{
				++index;
				continue;
			}

			const Arrival arrival = g_arrivals[index];
			g_arrivals.erase(g_arrivals.begin() + static_cast<std::ptrdiff_t>(index));
			SpawnArrival(arrival);
		}
	}

	void UpdateWorldObjects(float delta_seconds)
	{
		for (NpcResource& resource : g_resources)
		{
			resource.regrow_timer = CountDown(resource.regrow_timer, delta_seconds);
		}

		for (size_t index = 0; index < g_corpses.size();)
		{
			g_corpses[index].age += delta_seconds;

			const bool empty = g_corpses[index].gold <= 0 && g_corpses[index].goods <= 0;
			if (empty || g_corpses[index].age > CORPSE_LIFE_SEC)
			{
				g_corpses.erase(g_corpses.begin() + static_cast<std::ptrdiff_t>(index));
				continue;
			}

			++index;
		}

		for (size_t index = 0; index < g_arrows.size();)
		{
			g_arrows[index].age += delta_seconds;

			if (g_arrows[index].age > ARROW_FLIGHT_SEC)
			{
				g_arrows.erase(g_arrows.begin() + static_cast<std::ptrdiff_t>(index));
				continue;
			}

			++index;
		}

		for (NpcEvent& event : g_events)
		{
			event.age += delta_seconds;
		}

		while (g_events.empty() == false && g_events.front().age > EVENT_LIFE_SEC)
		{
			g_events.erase(g_events.begin());
		}
	}

	void UpdateTreasury(float delta_seconds)
	{
		g_treasury_carry += TREASURY_INCOME_PER_SEC * delta_seconds;

		const int32 income = static_cast<int32>(g_treasury_carry);
		g_treasury_carry -= static_cast<float>(income);
		g_treasury += income;

		if (g_treasury > 40)
		{
			g_treasury_warned = false;
		}
	}

	void UpdateNeeds(Npc* npc, int32 index, float delta_seconds)
	{
		npc->hunger += delta_seconds / HUNGER_FULL_SEC;
		if (npc->hunger > 1.f)
		{
			npc->hunger = 1.f;
		}

		// 굶으면 천천히 죽는다
		if (npc->hunger >= 1.f)
		{
			npc->starve_carry += delta_seconds;

			if (npc->starve_carry >= STARVE_DAMAGE_SEC)
			{
				npc->starve_carry = 0.f;
				npc->hp -= 1;

				if (npc->hp <= 0)
				{
					LogDeath(*npc, NPC_TARGET_NONE, -1);
					KillNpc(index, DescribeKiller(NPC_TARGET_NONE, -1, false), false);
					return;
				}
			}

			return;
		}

		if (npc->since_hit >= NATURAL_REGEN_DELAY)
		{
			NpcHeal(npc, NATURAL_REGEN_PER_SEC, delta_seconds);
		}
	}

	void UpdateLook(Npc* npc)
	{
		if (npc->idle_timer < 0.12f)
		{
			npc->body.m_frame = static_cast<int32>(npc->walk_distance * 2.6f) % 2;
		}
		else
		{
			npc->body.m_frame = 0;
		}

		npc->body.m_emissive = 1.f + npc->hit_flash / Balance::HIT_FLASH_SEC * 2.5f;
	}

	void UpdateNpc(Npc* npc, int32 index, float delta_seconds)
	{
		npc->attack_cooldown = CountDown(npc->attack_cooldown, delta_seconds);
		npc->hit_flash = CountDown(npc->hit_flash, delta_seconds);
		npc->threat_timer = CountDown(npc->threat_timer, delta_seconds);
		npc->outlaw_timer = CountDown(npc->outlaw_timer, delta_seconds);
		npc->stall_heartbeat = CountDown(npc->stall_heartbeat, delta_seconds);
		npc->since_hit += delta_seconds;
		npc->idle_timer += delta_seconds;

		UpdateNeeds(npc, index, delta_seconds);

		if (npc->is_alive == false)
		{
			return;
		}

		SenseNpc(npc, delta_seconds);
		ThinkNpc(npc, delta_seconds);

		UpdateLook(npc);
	}

	// 광장에서 한 점에 몰려 겹치면 한 사람으로 보인다
	void SeparateNpcs(float delta_seconds)
	{
		const float push = SEPARATION_SPEED * delta_seconds;

		for (size_t first = 0; first < g_npcs.size(); ++first)
		{
			if (g_npcs[first].is_alive == false)
			{
				continue;
			}

			for (size_t second = first + 1; second < g_npcs.size(); ++second)
			{
				if (g_npcs[second].is_alive == false)
				{
					continue;
				}

				Npc& left = g_npcs[first];
				Npc& right = g_npcs[second];

				const float delta_x = GetTileDistanceX(left.body.m_pos, right.body.m_pos);
				const float delta_y = GetTileDistanceY(left.body.m_pos, right.body.m_pos);

				if (std::fabs(delta_x) > SEPARATION_DISTANCE || std::fabs(delta_y) > SEPARATION_DISTANCE)
				{
					continue;
				}

				const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
				if (distance >= SEPARATION_DISTANCE)
				{
					continue;
				}

				float dir_x = 1.f;
				float dir_y = 0.f;
				if (distance > 0.001f)
				{
					dir_x = delta_x / distance;
					dir_y = delta_y / distance;
				}

				MoveWithCollision(&left.body.m_pos, NPC_BODY_RADIUS, -dir_x * push, -dir_y * push);
				MoveWithCollision(&right.body.m_pos, NPC_BODY_RADIUS, dir_x * push, dir_y * push);
			}
		}
	}

	const char* GetTaskTag(int32 task)
	{
		switch (task)
		{
			case NPC_TASK_HUNT:
			{
				return "hunt";
			}
			case NPC_TASK_RAID:
			{
				return "raid";
			}
			case NPC_TASK_SELL:
			{
				return "sell";
			}
			case NPC_TASK_GATHER:
			{
				return "gather";
			}
			case NPC_TASK_PEDDLE:
			{
				return "peddle";
			}
			case NPC_TASK_PATROL:
			{
				return "patrol";
			}
			case NPC_TASK_EAT:
			{
				return "eat";
			}
			case NPC_TASK_REST:
			{
				return "rest";
			}
			case NPC_TASK_UPGRADE:
			{
				return "upgrade";
			}
			case NPC_TASK_SCAVENGE:
			{
				return "scavenge";
			}
			case NPC_TASK_WANDER:
			{
				return "wander";
			}
			case NPC_TASK_FIGHT:
			{
				return "fight";
			}
			case NPC_TASK_FLEE:
			{
				return "flee";
			}
			case NPC_TASK_ENGAGE:
			{
				return "engage";
			}
			default:
			{
				return "avoid";
			}
		}
	}

	// 조건 없는 숫자는 나중에 못 쓴다. 인원·금고·일과 분포·갱신 비용을 같이 남긴다
	void LogCensus()
	{
		std::array<int32, NPC_JOB_COUNT> jobs = {};
		std::array<int32, NPC_TASK_COUNT> tasks = {};
		int32 alive = 0;
		int32 outlaws = 0;
		int64 total_gold = 0;

		for (const Npc& npc : g_npcs)
		{
			if (npc.is_alive == false)
			{
				continue;
			}

			alive += 1;
			jobs[static_cast<size_t>(npc.job)] += 1;
			tasks[static_cast<size_t>(npc.task)] += 1;
			total_gold += npc.gold;

			if (IsNpcOutlaw(npc) && npc.job != NPC_JOB_RAIDER)
			{
				outlaws += 1;
			}
		}

		std::cout << "[ Npc - Census ] : alive " << alive
			<< " hunter " << jobs[NPC_JOB_HUNTER]
			<< " raider " << jobs[NPC_JOB_RAIDER]
			<< " merchant " << jobs[NPC_JOB_MERCHANT]
			<< " gatherer " << jobs[NPC_JOB_GATHERER]
			<< " warden " << jobs[NPC_JOB_WARDEN]
			<< " outlaw " << outlaws
			<< " deaths " << g_death_count
			<< " robberies " << g_robbery_count
			<< " treasury " << g_treasury
			<< " npc_gold " << total_gold
			<< " tick_us " << static_cast<int32>(g_tick_average_us)
			<< " paths " << PopNavSearchCount() << "\n";

		std::cout << "[ Npc - Census ] : tasks";
		for (int32 task = 0; task < NPC_TASK_COUNT; ++task)
		{
			if (tasks[static_cast<size_t>(task)] > 0)
			{
				std::cout << " " << GetTaskTag(task) << " " << tasks[static_cast<size_t>(task)];
			}
		}
		std::cout << "\n";
	}

	void StepToward(Npc* npc, const WorldPos& target, float delta_seconds)
	{
		const float delta_x = GetTileDistanceX(npc->body.m_pos, target);
		const float delta_y = GetTileDistanceY(npc->body.m_pos, target);
		const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);

		if (distance < 0.0001f)
		{
			return;
		}

		float step = npc->move_speed * delta_seconds;
		if (step > distance)
		{
			step = distance;
		}

		const WorldPos before = npc->body.m_pos;
		MoveWithCollision(&npc->body.m_pos, NPC_BODY_RADIUS, delta_x / distance * step, delta_y / distance * step);

		const float moved_x = GetTileDistanceX(before, npc->body.m_pos);
		const float moved_y = GetTileDistanceY(before, npc->body.m_pos);

		if (std::fabs(moved_x) + std::fabs(moved_y) < 0.0001f)
		{
			return;
		}

		npc->walk_distance += std::sqrt(moved_x * moved_x + moved_y * moved_y);
		npc->idle_timer = 0.f;
		npc->body.m_flip = (moved_x - moved_y) < 0.f;
	}

	void FaceToward(Npc* npc, const WorldPos& target)
	{
		const float delta_x = GetTileDistanceX(npc->body.m_pos, target);
		const float delta_y = GetTileDistanceY(npc->body.m_pos, target);

		if (std::fabs(delta_x - delta_y) > 0.01f)
		{
			npc->body.m_flip = (delta_x - delta_y) < 0.f;
		}
	}

	int32 TileGap(const ZoneTile& left, const ZoneTile& right)
	{
		return std::max(std::abs(left.x - right.x), std::abs(left.y - right.y));
	}

	// 멈춰 있으면 길을 다시 찾는다. 모서리에 끼면 거기서 영원히 걷는 시늉만 한다
	void CheckProgress(Npc* npc, float delta_seconds)
	{
		npc->progress_timer += delta_seconds;

		if (npc->progress_timer < PROGRESS_CHECK_SEC)
		{
			return;
		}

		npc->progress_timer = 0.f;

		if (DistanceBetween(npc->progress_pos, npc->body.m_pos) < 0.3f)
		{
			npc->path.clear();
			npc->path_cursor = 0;
			npc->path_fail_count += 1;
		}

		npc->progress_pos = npc->body.m_pos;
	}

	void LaunchArrow(const WorldPos& from, const WorldPos& to)
	{
		if (IsNearCamera(from) == false)
		{
			return;
		}

		NpcArrow arrow;
		arrow.from = from;
		arrow.to = to;
		g_arrows.push_back(arrow);
	}

	void StealFrom(Npc* robber, int32 kind, int32 index)
	{
		const int32 want = 5 + robber->attack / 2;

		if (kind == NPC_TARGET_PLAYER)
		{
			const int32 taken = TakePlayerGold(want / 2 + RollInt(0, 3));
			if (taken > 0)
			{
				robber->gold += taken;
				robber->robbed_total += taken;
				AddFloatingText(GetPlayer().body.m_pos, 60.f, L"-" + std::to_wstring(taken) + L" 골드", 1.f, 0.55f, 0.25f);
			}

			return;
		}

		if (IsValidNpc(index) == false)
		{
			return;
		}

		Npc& victim = g_npcs[static_cast<size_t>(index)];

		int32 gold = want + victim.gold / 5;
		if (gold > victim.gold)
		{
			gold = victim.gold;
		}

		victim.gold -= gold;
		robber->gold += gold;

		int32 goods = 10;
		if (goods > victim.goods)
		{
			goods = victim.goods;
		}

		if (goods > robber->goods_capacity - robber->goods)
		{
			goods = robber->goods_capacity - robber->goods;
		}

		if (goods > 0)
		{
			victim.goods -= goods;
			robber->goods += goods;
		}

		robber->robbed_total += gold + goods;
	}

	void MarkOutlaw(Npc* npc)
	{
		if (npc->job == NPC_JOB_RAIDER)
		{
			return;
		}

		if (npc->outlaw_timer <= 0.f)
		{
			PushNpcEvent(WithSubject(GetNpcTitle(*npc)) + L" 약탈에 손을 대 무법자가 되었다", 1.f, 0.55f, 0.30f);
		}

		npc->outlaw_timer = OUTLAW_SEC;
	}

	void OnMonsterKilled(Npc* npc, int32 monster_index, const MonsterReward& reward)
	{
		int32 trophy = reward.trophy;
		if (trophy > npc->goods_capacity - npc->goods)
		{
			trophy = npc->goods_capacity - npc->goods;
		}

		npc->goods += trophy;
		npc->gold += reward.gold;
		npc->kills += 1;
		NpcGainExp(npc, reward.exp);

		const int32 kind = GetMonsterKind(monster_index);
		if (kind == MONSTER_DEVOURER)
		{
			PushNpcEvent(WithSubject(GetNpcTitle(*npc)) + L" 소거식귀를 쓰러뜨렸다", 0.95f, 0.85f, 0.45f);
		}
	}

	void OnNpcKilledByNpc(Npc* killer, int32 victim_level, bool victim_outlaw)
	{
		killer->kills += 1;
		NpcGainExp(killer, 10 + victim_level * 4);

		// 무법자를 벤 쪽은 성채 금고에서 현상금을 받는다
		if (victim_outlaw && IsNpcOutlaw(*killer) == false)
		{
			killer->gold += TakeTreasury(OUTLAW_BOUNTY);
		}
	}

	void Strike(Npc* npc, int32 target_kind, int32 target_index, bool rob, const WorldPos& target_pos)
	{
		const int32 self_index = GetNpcIndex(*npc);
		const int32 damage = npc->attack + RollInt(-1, 1);

		if (npc->attack_range > 2.f)
		{
			LaunchArrow(npc->body.m_pos, target_pos);
		}

		if (target_kind == NPC_TARGET_MONSTER)
		{
			MonsterReward reward;
			if (DamageMonsterFromNpc(target_index, damage, self_index, npc->body.m_pos, &reward))
			{
				OnMonsterKilled(npc, target_index, reward);
			}

			return;
		}

		if (target_kind == NPC_TARGET_PLAYER)
		{
			DamagePlayer(damage);

			if (rob)
			{
				StealFrom(npc, target_kind, target_index);
			}

			return;
		}

		if (IsValidNpc(target_index) == false)
		{
			return;
		}

		if (rob)
		{
			MarkOutlaw(npc);
			StealFrom(npc, target_kind, target_index);
		}

		const int32 victim_level = g_npcs[static_cast<size_t>(target_index)].level;
		const bool victim_outlaw = IsNpcOutlaw(g_npcs[static_cast<size_t>(target_index)]);
		HurtNpc(target_index, damage, NPC_TARGET_NPC, self_index);

		if (IsValidNpc(target_index) == false)
		{
			OnNpcKilledByNpc(npc, victim_level, victim_outlaw);
		}
	}
}

// ------------------------------------------------------------ 세계

std::vector<Npc>& GetNpcList()
{
	return g_npcs;
}

const std::vector<ZoneSite>& GetNpcSites()
{
	return g_sites;
}

std::vector<NpcResource>& GetNpcResources()
{
	return g_resources;
}

std::vector<NpcCorpse>& GetNpcCorpses()
{
	return g_corpses;
}

const std::vector<NpcEvent>& GetNpcEvents()
{
	return g_events;
}

int32 GetTreasury()
{
	return g_treasury;
}

void AddTreasury(int32 amount)
{
	g_treasury += amount;
}

int32 TakeTreasury(int32 amount)
{
	int32 paid = amount;
	if (paid > g_treasury)
	{
		paid = g_treasury;
	}

	g_treasury -= paid;

	if (g_treasury <= 0 && g_treasury_warned == false)
	{
		g_treasury_warned = true;
		PushNpcEvent(L"성채 금고가 바닥났다. 급료와 짐값이 끊긴다", 1.f, 0.35f, 0.25f);
	}

	return paid;
}

void PushNpcEvent(const std::wstring& text, float r, float g, float b)
{
	NpcEvent event;
	event.text = text;
	event.r = r;
	event.g = g;
	event.b = b;

	g_events.push_back(event);

	if (g_events.size() > EVENT_MAX_COUNT)
	{
		g_events.erase(g_events.begin());
	}
}

bool IsNpcOutlaw(const Npc& npc)
{
	if (npc.job == NPC_JOB_RAIDER)
	{
		return true;
	}

	return npc.outlaw_timer > 0.f;
}

bool IsWorldPosInTown(const WorldPos& pos)
{
	if (g_layout == nullptr)
	{
		return false;
	}

	int32 x = 0;
	int32 y = 0;
	WorldToZoneTile(pos, &x, &y);

	return IsInsideTown(*g_layout, x, y);
}

WorldPos GetSitePos(int32 site)
{
	const ZoneTile tile = g_sites[static_cast<size_t>(site)].tile;

	return ZoneTileToWorld(tile.x, tile.y, 0.5f, 0.5f);
}

bool IsSiteInTown(int32 site)
{
	if (g_layout == nullptr)
	{
		return false;
	}

	const ZoneTile tile = g_sites[static_cast<size_t>(site)].tile;

	return IsInsideTown(*g_layout, tile.x, tile.y);
}

int32 GetNpcIndex(const Npc& npc)
{
	return static_cast<int32>(&npc - g_npcs.data());
}

float GetNpcDistance(const Npc& npc, const WorldPos& pos)
{
	return DistanceBetween(npc.body.m_pos, pos);
}

bool GetNpcTargetPos(int32 kind, int32 index, WorldPos* pos)
{
	switch (kind)
	{
		case NPC_TARGET_MONSTER:
		{
			if (IsMonsterAlive(index) == false)
			{
				return false;
			}

			*pos = GetMonsterPos(index);
			return true;
		}
		case NPC_TARGET_NPC:
		{
			return GetNpcPos(index, pos);
		}
		case NPC_TARGET_PLAYER:
		{
			if (IsPlayerDead())
			{
				return false;
			}

			*pos = GetPlayer().body.m_pos;
			return true;
		}
		case NPC_TARGET_SITE:
		{
			if (index < 0 || index >= static_cast<int32>(g_sites.size()))
			{
				return false;
			}

			*pos = GetSitePos(index);
			return true;
		}
		case NPC_TARGET_RESOURCE:
		{
			if (index < 0 || index >= static_cast<int32>(g_resources.size()))
			{
				return false;
			}

			*pos = g_resources[static_cast<size_t>(index)].body.m_pos;
			return true;
		}
		case NPC_TARGET_CORPSE:
		{
			for (const NpcCorpse& corpse : g_corpses)
			{
				if (corpse.id == index)
				{
					*pos = corpse.body.m_pos;
					return true;
				}
			}

			return false;
		}
		default:
		{
			return false;
		}
	}
}

const wchar_t* GetNpcJobName(int32 job)
{
	switch (job)
	{
		case NPC_JOB_RAIDER:
		{
			return L"약탈자";
		}
		case NPC_JOB_MERCHANT:
		{
			return L"행상";
		}
		case NPC_JOB_GATHERER:
		{
			return L"채집꾼";
		}
		case NPC_JOB_WARDEN:
		{
			return L"순찰병";
		}
		default:
		{
			return L"사냥꾼";
		}
	}
}

std::wstring WithSubject(const std::wstring& word)
{
	if (word.empty())
	{
		return word;
	}

	// 한글 음절은 (코드 - 0xAC00) 를 28 로 나눈 나머지가 받침 번호다. 0 이면 받침 없음
	const wchar_t last = word.back();
	if (last >= 0xAC00 && last <= 0xD7A3 && (last - 0xAC00) % 28 != 0)
	{
		return word + L"이";
	}

	return word + L"가";
}

std::wstring WithObject(const std::wstring& word)
{
	if (word.empty())
	{
		return word;
	}

	const wchar_t last = word.back();
	if (last >= 0xAC00 && last <= 0xD7A3 && (last - 0xAC00) % 28 != 0)
	{
		return word + L"을";
	}

	return word + L"를";
}

std::wstring GetNpcTitle(const Npc& npc)
{
	return std::wstring(GetNpcJobName(npc.job)) + L" " + npc.name;
}

int32 GetNpcUpgradeCost(const Npc& npc)
{
	return 40 + npc.gear * 30;
}

float GetNpcTickMicroseconds()
{
	return g_tick_average_us;
}

// ------------------------------------------------------------ 몸

void ResetNpcPath(Npc* npc)
{
	npc->path.clear();
	npc->path_cursor = 0;
	npc->path_fail_count = 0;
	npc->progress_timer = 0.f;
	npc->progress_pos = npc->body.m_pos;
}

int32 NpcMoveTo(Npc* npc, const WorldPos& goal, float arrive_radius, float delta_seconds)
{
	const float distance = DistanceBetween(npc->body.m_pos, goal);

	if (distance <= arrive_radius)
	{
		npc->path.clear();
		npc->path_cursor = 0;
		return BT_SUCCESS;
	}

	if (npc->path_fail_count >= PATH_FAIL_LIMIT)
	{
		return BT_FAILURE;
	}

	CheckProgress(npc, delta_seconds);

	// 가깝고 막힌 게 없으면 길찾기 없이 곧장
	if (distance < DIRECT_STEER_RANGE && HasNavLine(npc->body.m_pos, goal, NPC_BODY_RADIUS))
	{
		npc->path.clear();
		npc->path_cursor = 0;
		StepToward(npc, goal, delta_seconds);
		return BT_RUNNING;
	}

	ZoneTile goal_tile;
	WorldToZoneTile(goal, &goal_tile.x, &goal_tile.y);

	bool need_path = npc->path_cursor >= npc->path.size();
	if (need_path == false && TileGap(goal_tile, npc->path_goal) > 2)
	{
		need_path = true;
	}

	if (need_path)
	{
		// 이번 틱 예산이 없으면 제자리에서 한 틱 기다린다
		if (TakeNavBudget() == false)
		{
			return BT_RUNNING;
		}

		ZoneTile start_tile;
		WorldToZoneTile(npc->body.m_pos, &start_tile.x, &start_tile.y);

		const bool complete = FindNavPath(start_tile, goal_tile, &npc->path);
		npc->path_cursor = 0;
		npc->path_goal = goal_tile;

		if (complete == false)
		{
			npc->path_fail_count += 1;
		}

		if (npc->path.empty())
		{
			return BT_RUNNING;
		}
	}

	// 앞 칸이 곧장 보이면 건너뛴다. 칸 중심만 밟으면 계단처럼 꺾여 걷는다
	for (int32 skip = 0; skip < 2; ++skip)
	{
		if (npc->path_cursor + 1 >= npc->path.size())
		{
			break;
		}

		const ZoneTile& ahead = npc->path[npc->path_cursor + 1];
		const WorldPos ahead_pos = ZoneTileToWorld(ahead.x, ahead.y, 0.5f, 0.5f);

		if (HasNavLine(npc->body.m_pos, ahead_pos, NPC_BODY_RADIUS) == false)
		{
			break;
		}

		npc->path_cursor += 1;
	}

	const ZoneTile& waypoint = npc->path[npc->path_cursor];
	const WorldPos waypoint_pos = ZoneTileToWorld(waypoint.x, waypoint.y, 0.5f, 0.5f);

	if (DistanceBetween(npc->body.m_pos, waypoint_pos) < WAYPOINT_REACH)
	{
		npc->path_cursor += 1;
		return BT_RUNNING;
	}

	StepToward(npc, waypoint_pos, delta_seconds);

	return BT_RUNNING;
}

int32 NpcEngage(Npc* npc, int32 target_kind, int32 target_index, bool rob, float delta_seconds)
{
	WorldPos target_pos;
	if (GetNpcTargetPos(target_kind, target_index, &target_pos) == false)
	{
		return BT_SUCCESS;
	}

	const float distance = DistanceBetween(npc->body.m_pos, target_pos);

	if (distance > ENGAGE_GIVE_UP)
	{
		return BT_FAILURE;
	}

	const float reach = npc->attack_range + 0.3f;

	if (distance > reach)
	{
		if (NpcMoveTo(npc, target_pos, reach * 0.85f, delta_seconds) == BT_FAILURE)
		{
			return BT_FAILURE;
		}

		return BT_RUNNING;
	}

	FaceToward(npc, target_pos);

	if (npc->attack_cooldown <= 0.f)
	{
		npc->attack_cooldown = npc->attack_interval * RollRange(0.9f, 1.1f);
		Strike(npc, target_kind, target_index, rob, target_pos);
	}

	return BT_RUNNING;
}

void NpcMoveAway(Npc* npc, const WorldPos& from, float delta_seconds)
{
	float delta_x = GetTileDistanceX(from, npc->body.m_pos);
	float delta_y = GetTileDistanceY(from, npc->body.m_pos);
	const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);

	if (length < 0.001f)
	{
		delta_x = 1.f;
		delta_y = 0.f;
	}
	else
	{
		delta_x /= length;
		delta_y /= length;
	}

	// 곧장 등을 돌리면 나무나 균열에 막혀 제자리에서 잡힌다. 막혔으면 옆으로 틀어 본다
	const std::array<float, 7> turns = { 0.f, 0.785f, -0.785f, 1.571f, -1.571f, 2.356f, -2.356f };

	for (float turn : turns)
	{
		const float cos_turn = std::cos(turn);
		const float sin_turn = std::sin(turn);
		const float dir_x = delta_x * cos_turn - delta_y * sin_turn;
		const float dir_y = delta_x * sin_turn + delta_y * cos_turn;

		WorldPos away = npc->body.m_pos;
		MoveWorldPos(&away, dir_x * 1.5f, dir_y * 1.5f);

		if (HasNavLine(npc->body.m_pos, away, NPC_BODY_RADIUS))
		{
			StepToward(npc, away, delta_seconds);
			return;
		}
	}

	WorldPos away = npc->body.m_pos;
	MoveWorldPos(&away, delta_x * 2.f, delta_y * 2.f);
	StepToward(npc, away, delta_seconds);
}

void NpcHeal(Npc* npc, float per_second, float delta_seconds)
{
	if (npc->hp >= npc->max_hp)
	{
		npc->regen_carry = 0.f;
		return;
	}

	npc->regen_carry += per_second * delta_seconds;

	const int32 gained = static_cast<int32>(npc->regen_carry);
	npc->regen_carry -= static_cast<float>(gained);
	npc->hp += gained;

	if (npc->hp > npc->max_hp)
	{
		npc->hp = npc->max_hp;
	}
}

void NpcGainExp(Npc* npc, int32 exp)
{
	npc->exp += exp;

	while (npc->exp >= npc->level * 25)
	{
		npc->exp -= npc->level * 25;
		npc->level += 1;
		npc->max_hp += 8;
		npc->attack += 1;
		npc->hp += 8;
	}
}

void NpcShowText(const Npc& npc, const std::wstring& text, float r, float g, float b)
{
	if (IsNearCamera(npc.body.m_pos) == false)
	{
		return;
	}

	AddFloatingText(npc.body.m_pos, 50.f, text, r, g, b);
}

// ------------------------------------------------------------ 바깥 창구

void BuildNpcs(const ZoneLayout& layout)
{
	g_layout = &layout;

	g_npcs.clear();
	g_resources.clear();
	g_corpses.clear();
	g_graves.clear();
	g_arrows.clear();
	g_events.clear();
	g_arrivals.clear();

	g_sites = layout.sites;
	g_treasury = TREASURY_START;
	g_treasury_carry = 0.f;
	g_treasury_warned = false;
	g_selected = -1;
	g_follow = false;
	g_census_timer = 0.f;
	g_death_count = 0;
	g_robbery_count = 0;

	BuildNavGrid(layout.tiles);
	BuildNpcBrain();
	BuildResources(layout);

	// 번호가 몬스터와 다른 NPC 표적으로 쓰이니 한 번 잡은 크기는 안 바꾼다
	g_npcs.reserve(static_cast<size_t>(WARDEN_COUNT + HUNTER_COUNT + GATHERER_COUNT + MERCHANT_COUNT + RAIDER_COUNT));

	SpawnInitial(NPC_JOB_WARDEN, WARDEN_COUNT);
	SpawnInitial(NPC_JOB_HUNTER, HUNTER_COUNT);
	SpawnInitial(NPC_JOB_GATHERER, GATHERER_COUNT);
	SpawnInitial(NPC_JOB_MERCHANT, MERCHANT_COUNT);
	SpawnInitial(NPC_JOB_RAIDER, RAIDER_COUNT);

	PushNpcEvent(L"성문의 종이 울린다. 벽 밖의 하루가 시작된다", 0.85f, 0.80f, 0.65f);

	std::cout << "[ Npc - BuildNpcs ] : npcs " << g_npcs.size()
		<< " resources " << g_resources.size()
		<< " sites " << g_sites.size() << "\n";
}

void UpdateNpcs(float delta_seconds)
{
	const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	ResetNavBudget();
	UpdateTreasury(delta_seconds);
	UpdateWorldObjects(delta_seconds);

	for (size_t index = 0; index < g_npcs.size(); ++index)
	{
		if (g_npcs[index].is_alive == false)
		{
			continue;
		}

		UpdateNpc(&g_npcs[index], static_cast<int32>(index), delta_seconds);
	}

	SeparateNpcs(delta_seconds);
	UpdateArrivals(delta_seconds);

	const std::chrono::duration<float, std::micro> elapsed = std::chrono::steady_clock::now() - begin;
	g_tick_accum_us += elapsed.count();
	g_tick_samples += 1;

	g_census_timer += delta_seconds;
	if (g_census_timer >= CENSUS_SEC)
	{
		g_census_timer = 0.f;
		g_tick_average_us = g_tick_accum_us / static_cast<float>(g_tick_samples);
		g_tick_accum_us = 0.f;
		g_tick_samples = 0;

		LogCensus();
	}
	else if (g_tick_samples >= 60)
	{
		// 관찰창 숫자는 1 초마다 갱신
		g_tick_average_us = g_tick_accum_us / static_cast<float>(g_tick_samples);
	}
}

void CollectNpcActors()
{
	for (const Npc& npc : g_npcs)
	{
		if (npc.is_alive)
		{
			AddSceneActor(&npc.body);
		}
	}

	for (const NpcResource& resource : g_resources)
	{
		if (resource.regrow_timer <= 0.f)
		{
			AddSceneActor(&resource.body);
		}
	}

	for (const NpcCorpse& corpse : g_corpses)
	{
		AddSceneActor(&corpse.body);
	}

	for (const Actor& grave : g_graves)
	{
		AddSceneActor(&grave);
	}
}

void DrawNpcGroundMarks(Renderer* renderer, const WorldPos& camera)
{
	if (IsValidNpc(g_selected) == false)
	{
		return;
	}

	const Npc& npc = g_npcs[static_cast<size_t>(g_selected)];
	const ScreenPos screen = WorldToScreen(npc.body.m_pos, camera);

	renderer->DrawSolidDiamond(screen.x, screen.y, 1.3f, 0.30f, 0.85f, 0.78f, 0.35f);
	renderer->DrawSolidDiamond(screen.x, screen.y, 0.9f, 0.05f, 0.10f, 0.10f, 0.35f);
}

void DrawNpcArrows(Renderer* renderer, const WorldPos& camera)
{
	const float unit = static_cast<float>(PIXEL_SCALE);
	const float length = static_cast<float>(GetSpriteHeight(SPRITE_ARROW)) * unit;
	const float width = static_cast<float>(GetSpriteWidth(SPRITE_ARROW)) * unit;
	const float lift = 11.f * unit;

	for (const NpcArrow& arrow : g_arrows)
	{
		const ScreenPos from = WorldToScreen(arrow.from, camera);
		const ScreenPos to = WorldToScreen(arrow.to, camera);
		const float delta_x = to.x - from.x;
		const float delta_y = to.y - from.y;
		const float span = std::sqrt(delta_x * delta_x + delta_y * delta_y);

		if (span < 1.f)
		{
			continue;
		}

		const float progress = arrow.age / ARROW_FLIGHT_SEC;
		const float dir_x = delta_x / span;
		const float dir_y = delta_y / span;
		const float center_x = from.x + delta_x * progress;
		const float center_y = from.y + delta_y * progress + lift;

		// 세로축이 화살 머리 방향
		AffineQuad quad;
		quad.axis_y_x = dir_x * length;
		quad.axis_y_y = dir_y * length;
		quad.axis_x_x = dir_y * width;
		quad.axis_x_y = -dir_x * width;
		quad.origin_x = center_x - quad.axis_x_x * 0.5f - quad.axis_y_x * 0.5f;
		quad.origin_y = center_y - quad.axis_x_y * 0.5f - quad.axis_y_y * 0.5f;

		DrawSpriteQuad(renderer, SPRITE_ARROW, 0, quad, false, 1.f, 1.f, 1.f, 1.f);
	}
}

bool FindNpcForMonster(const WorldPos& pos, float range, int32* npc_index, float* distance)
{
	int32 best = -1;
	float best_distance = range;

	for (size_t index = 0; index < g_npcs.size(); ++index)
	{
		const Npc& npc = g_npcs[index];

		if (npc.is_alive == false)
		{
			continue;
		}

		const float npc_distance = DistanceBetween(pos, npc.body.m_pos);

		if (npc_distance < best_distance)
		{
			best_distance = npc_distance;
			best = static_cast<int32>(index);
		}
	}

	if (best < 0)
	{
		return false;
	}

	*npc_index = best;
	*distance = best_distance;

	return true;
}

bool GetNpcPos(int32 npc_index, WorldPos* pos)
{
	if (IsValidNpc(npc_index) == false)
	{
		return false;
	}

	*pos = g_npcs[static_cast<size_t>(npc_index)].body.m_pos;

	return true;
}

void DamageNpcFromMonster(int32 npc_index, int32 damage, int32 monster_index)
{
	HurtNpc(npc_index, damage, NPC_TARGET_MONSTER, monster_index);
}

int32 HitHostileNpcsInFan(const WorldPos& origin, float dir_x, float dir_y, float range, float half_cos, int32 damage)
{
	int32 hit_count = 0;

	for (size_t index = 0; index < g_npcs.size(); ++index)
	{
		Npc& npc = g_npcs[index];

		if (npc.is_alive == false || IsNpcOutlaw(npc) == false)
		{
			continue;
		}

		const float to_x = GetTileDistanceX(origin, npc.body.m_pos);
		const float to_y = GetTileDistanceY(origin, npc.body.m_pos);
		const float distance = std::sqrt(to_x * to_x + to_y * to_y);

		if (distance > range + NPC_BODY_RADIUS)
		{
			continue;
		}

		if (distance > 0.05f && (to_x * dir_x + to_y * dir_y) / distance < half_cos)
		{
			continue;
		}

		hit_count += 1;
		MoveWithCollision(&npc.body.m_pos, NPC_BODY_RADIUS, dir_x * Balance::SWING_KNOCKBACK, dir_y * Balance::SWING_KNOCKBACK);
		HurtNpc(static_cast<int32>(index), damage, NPC_TARGET_PLAYER, -1);
	}

	return hit_count;
}

int32 PickNpcAt(const WorldPos& pos, float radius)
{
	int32 best = -1;
	float best_distance = radius;

	for (size_t index = 0; index < g_npcs.size(); ++index)
	{
		if (g_npcs[index].is_alive == false)
		{
			continue;
		}

		const float distance = DistanceBetween(pos, g_npcs[index].body.m_pos);

		if (distance < best_distance)
		{
			best_distance = distance;
			best = static_cast<int32>(index);
		}
	}

	return best;
}

void SelectNpc(int32 npc_index)
{
	g_selected = npc_index;

	if (npc_index < 0)
	{
		g_follow = false;
	}
}

int32 GetSelectedNpc()
{
	if (IsValidNpc(g_selected) == false)
	{
		return -1;
	}

	return g_selected;
}

void SelectNextNpc()
{
	const int32 count = static_cast<int32>(g_npcs.size());

	for (int32 step = 1; step <= count; ++step)
	{
		const int32 candidate = (g_selected + step + count) % count;

		if (IsValidNpc(candidate))
		{
			g_selected = candidate;
			return;
		}
	}
}

void ToggleFollowNpc()
{
	if (IsValidNpc(g_selected) == false)
	{
		g_follow = false;
		return;
	}

	g_follow = !g_follow;
}

bool GetFollowPos(WorldPos* pos)
{
	if (g_follow == false)
	{
		return false;
	}

	return GetNpcPos(g_selected, pos);
}

void ForEachLiveNpc(void (*visit)(const WorldPos& pos, int32 job, bool is_outlaw, bool is_selected, void* context), void* context)
{
	for (size_t index = 0; index < g_npcs.size(); ++index)
	{
		const Npc& npc = g_npcs[index];

		if (npc.is_alive)
		{
			visit(npc.body.m_pos, npc.job, IsNpcOutlaw(npc), static_cast<int32>(index) == g_selected, context);
		}
	}
}

void NoteNpcRobbery()
{
	g_robbery_count += 1;
}
