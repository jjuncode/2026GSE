#include "stdafx.h"
#include "Monitor.h"
#include "Monster.h"
#include "NpcBrain.h"
#include "NpcData.h"
#include "Player.h"
#include "ZoneGen.h"

#include <nlohmann/json.hpp>

// winsock2 가 windows.h 보다 먼저. 거꾸로면 옛 winsock.h 와 정의가 부딪힌다
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
	using Json = nlohmann::json;
	using Clock = std::chrono::steady_clock;

	constexpr uint16 MONITOR_PORT = 7788;
	constexpr int32 PORT_TRY_COUNT = 8;	// 막혀 있으면 다음 번호로
	constexpr float WATCHED_INTERVAL_SEC = 0.25f;	// 보는 페이지가 있을 때
	constexpr float IDLE_INTERVAL_SEC = 1.f;	// /state 한 번 묻기용
	constexpr float THREAT_RADIUS = 12.f;	// 거점 둘레 위협 ( 칸 )
	constexpr int32 SELECT_TIMEOUT_MS = 50;
	constexpr DWORD SEND_TIMEOUT_MS = 1000;
	constexpr int64 REQUEST_TIMEOUT_MS = 3000;	// 크롬 예비 연결은 요청 없이 붙어만 있다
	constexpr size_t REQUEST_MAX_BYTES = 8192;
	constexpr size_t PENDING_MAX_COUNT = 16;
	constexpr size_t STREAM_MAX_COUNT = 8;	// select 상한 64 안에서
	constexpr size_t SITE_KIND_COUNT = static_cast<size_t>(SITE_LAIR) + 1;
	constexpr size_t MONSTER_KIND_COUNT = static_cast<size_t>(MONSTER_GATEBREAKER) + 1;
	const char* PAGE_PATH = "./Monitor/index.html";

	struct TilePoint
	{
		float x = 0.f;
		float y = 0.f;
	};

	// 구역마다 한 번. 다 채운 뒤엔 안 바뀌어서 두 스레드가 잠금 없이 같이 읽는다
	struct ZoneInfo
	{
		uint32 seed = 0;
		std::string tiles;	// 'A' + 칸 번호
		ZoneTile center;
		std::vector<ZoneSite> sites;
		std::vector<std::wstring> site_names;
	};

	// 이름 포인터는 전부 문자열 리터럴이라 스레드를 넘어가도 안 사라진다
	struct NpcRow
	{
		int32 id = 0;
		bool is_alive = false;
		std::wstring name;
		int32 job = 0;
		int32 level = 0;
		int32 hp = 0;
		int32 max_hp = 0;
		int32 gold = 0;
		int32 goods = 0;
		int32 goods_capacity = 0;
		int32 kills = 0;
		int32 gear = 0;
		int32 task = 0;
		int32 home = -1;
		int32 region = -1;
		float hunger = 0.f;
		float outlaw_sec = 0.f;	// 약탈자가 아닌데 약탈에 손댄 남은 시간
		float greed = 0.f;
		float bravery = 0.f;
		float cruelty = 0.f;
		float diligence = 0.f;
		TilePoint point;
		const wchar_t* task_name = L"";
		const wchar_t* stance_name = L"";
		std::vector<const wchar_t*> brain;	// 행동 트리 노드 이름. 뿌리는 뺀다
	};

	struct MonsterRow
	{
		int32 id = 0;
		int32 kind = 0;
		bool is_elite = false;
		bool is_fighting = false;
		int32 region = -1;
		TilePoint point;
	};

	struct SiteTally
	{
		int32 npc_here = 0;
		int32 npc_fighting = 0;
		int32 npc_fleeing = 0;
		int32 outlaw_here = 0;
		std::array<int32, NPC_JOB_COUNT> jobs = {};
		int32 resident_alive = 0;
		int32 resident_slots = 0;
		int32 monster_here = 0;
		int32 monster_elite = 0;
		int32 monster_fighting = 0;
		int32 monster_near = 0;	// 거점 둘레 THREAT_RADIUS 안
		int32 monster_respawning = 0;
		std::array<int32, MONSTER_KIND_COUNT> monster_kinds = {};
	};

	struct EventRow
	{
		int32 serial = 0;
		std::wstring text;
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
	};

	struct Snapshot
	{
		std::shared_ptr<const ZoneInfo> zone;
		uint64 sequence = 0;
		float time = 0.f;
		float interval = 0.f;
		int32 gather_us = 0;	// 게임 스레드가 이걸 채우는 데 쓴 시간

		std::vector<NpcRow> npcs;
		std::vector<MonsterRow> monsters;
		std::vector<SiteTally> tallies;
		std::vector<EventRow> events;

		std::array<int32, NPC_JOB_COUNT> job_alive = {};
		int32 npc_alive = 0;
		int64 npc_gold = 0;
		int32 outlaws = 0;
		int32 deaths = 0;
		int32 robberies = 0;
		int32 treasury = 0;
		float npc_tick_us = 0.f;
		int32 monster_alive = 0;
		int32 monster_slots = 0;
		int32 monster_fighting = 0;
		int32 elite_alive = 0;

		TilePoint player_point;
		int32 player_hp = 0;
		int32 player_max_hp = 0;
		int32 player_level = 0;
		int32 player_gold = 0;
		bool is_player_dead = false;
		int32 player_region = -1;

		bool is_boss_summoned = false;
		bool is_boss_dead = false;
		bool has_boss_pos = false;
		TilePoint boss_point;
	};

	// 게임 스레드 -> 모니터 스레드. 넘기는 건 다 채운 const 값뿐이다
	std::mutex g_publish_lock;
	std::shared_ptr<const ZoneInfo> g_published_zone;
	std::shared_ptr<const Snapshot> g_published_snapshot;
	uint64 g_zone_serial = 0;
	uint64 g_snapshot_serial = 0;

	std::atomic<bool> g_running(false);
	std::atomic<int32> g_stream_count(0);

	// 전역 std::thread 는 join 없이 소멸하면 terminate. 콘솔 창을 닫아 끝날 때 대비
	std::thread* g_thread = nullptr;
	SOCKET g_listen_socket = INVALID_SOCKET;
	uint16 g_port = 0;
	bool g_wsa_ready = false;

	// 게임 스레드 전용
	std::shared_ptr<const ZoneInfo> g_game_zone;
	float g_publish_timer = 0.f;
	float g_zone_seconds = 0.f;
	uint64 g_sequence = 0;

	// 모니터 스레드 전용
	struct PendingClient
	{
		SOCKET socket = INVALID_SOCKET;
		std::string request;
		Clock::time_point since;
	};

	struct StreamClient
	{
		SOCKET socket = INVALID_SOCKET;
		uint64 zone_serial = 0;
		uint64 state_serial = 0;
	};

	std::vector<PendingClient> g_pending;
	std::vector<StreamClient> g_streams;
	std::string g_zone_json;
	std::string g_state_json;
	uint64 g_zone_json_serial = 0;
	uint64 g_state_json_serial = 0;

	// ------------------------------------------------------------ 날 값 모으기 ( 게임 스레드 )

	// 미니맵과 같은 칸 좌표. 0 ~ ZONE_TILE_COUNT
	TilePoint ToTilePoint(const WorldPos& pos)
	{
		TilePoint point;
		point.x = static_cast<float>((pos.chunk_x + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_x;
		point.y = static_cast<float>((pos.chunk_y + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_y;

		return point;
	}

	float GetDistance(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	bool IsFightTask(int32 task)
	{
		return task == NPC_TASK_FIGHT || task == NPC_TASK_ENGAGE;
	}

	bool IsFleeTask(int32 task)
	{
		return task == NPC_TASK_FLEE || task == NPC_TASK_AVOID;
	}

	const wchar_t* GetSiteKindName(int32 kind)
	{
		switch (kind)
		{
			case SITE_TOWN:
			{
				return L"성채";
			}
			case SITE_GATE:
			{
				return L"성문";
			}
			case SITE_OUTPOST:
			{
				return L"초소";
			}
			case SITE_HIDEOUT:
			{
				return L"약탈자 소굴";
			}
			case SITE_VILLAGE:
			{
				return L"폐촌";
			}
			default:
			{
				return L"성문파쇄자 둥지";
			}
		}
	}

	// 같은 종류가 둘 이상이면 번호를 붙인다. "초소 2"
	std::vector<std::wstring> BuildSiteNames(const std::vector<ZoneSite>& sites)
	{
		std::array<int32, SITE_KIND_COUNT> kind_total = {};
		for (const ZoneSite& site : sites)
		{
			kind_total[static_cast<size_t>(site.kind)] += 1;
		}

		std::array<int32, SITE_KIND_COUNT> kind_order = {};
		std::vector<std::wstring> names;

		for (const ZoneSite& site : sites)
		{
			const size_t kind = static_cast<size_t>(site.kind);
			kind_order[kind] += 1;

			std::wstring name = GetSiteKindName(site.kind);
			if (kind_total[kind] > 1)
			{
				name += L" " + std::to_wstring(kind_order[kind]);
			}

			names.push_back(name);
		}

		return names;
	}

	// 성벽 안은 성채, 밖은 가장 가까운 바깥 거점. 어느 자리든 정확히 한 곳에 들어가서 거점별 합이 전체와 같다
	int32 FindRegion(const std::vector<WorldPos>& spots, const std::vector<ZoneSite>& sites, int32 town, const WorldPos& pos)
	{
		if (town >= 0 && IsWorldPosInTown(pos))
		{
			return town;
		}

		int32 best = town;
		float best_distance = 0.f;
		bool found = false;

		for (size_t index = 0; index < spots.size(); ++index)
		{
			if (sites[index].kind == SITE_TOWN)
			{
				continue;
			}

			const float distance = GetDistance(spots[index], pos);
			if (found && distance >= best_distance)
			{
				continue;
			}

			found = true;
			best = static_cast<int32>(index);
			best_distance = distance;
		}

		return best;
	}

	void FillNpcRow(const Npc& npc, int32 index, int32 region, NpcRow* row)
	{
		row->id = index;
		row->is_alive = npc.is_alive;
		row->name = npc.name;
		row->job = npc.job;
		row->level = npc.level;
		row->hp = npc.hp;
		row->max_hp = npc.max_hp;
		row->gold = npc.gold;
		row->goods = npc.goods;
		row->goods_capacity = npc.goods_capacity;
		row->kills = npc.kills;
		row->gear = npc.gear;
		row->task = npc.task;
		row->home = npc.home_site;
		row->region = region;
		row->hunger = npc.hunger;
		row->outlaw_sec = npc.outlaw_timer;
		row->greed = npc.greed;
		row->bravery = npc.bravery;
		row->cruelty = npc.cruelty;
		row->diligence = npc.diligence;
		row->point = ToTilePoint(npc.body.m_pos);
		row->task_name = GetNpcTaskName(npc.task);
		row->stance_name = GetNpcStanceName(npc.stance);

		const BehaviorTree<Npc>& tree = GetNpcBrainTree();
		for (size_t order = 1; order < npc.brain.active_path.size(); ++order)
		{
			row->brain.push_back(tree.GetNodeName(npc.brain.active_path[order]));
		}
	}

	void GatherNpcs(const std::vector<WorldPos>& spots, int32 town, Snapshot* snapshot)
	{
		const std::vector<ZoneSite>& sites = snapshot->zone->sites;
		const std::vector<Npc>& npc_list = GetNpcList();
		snapshot->npcs.resize(npc_list.size());

		for (size_t index = 0; index < npc_list.size(); ++index)
		{
			const Npc& npc = npc_list[index];
			int32 region = -1;

			if (npc.home_site >= 0 && npc.home_site < static_cast<int32>(snapshot->tallies.size()))
			{
				SiteTally& home = snapshot->tallies[static_cast<size_t>(npc.home_site)];
				home.resident_slots += 1;

				if (npc.is_alive)
				{
					home.resident_alive += 1;
				}
			}

			if (npc.is_alive)
			{
				snapshot->npc_alive += 1;
				snapshot->npc_gold += npc.gold;
				snapshot->job_alive[static_cast<size_t>(npc.job)] += 1;

				if (npc.outlaw_timer > 0.f)
				{
					snapshot->outlaws += 1;
				}

				region = FindRegion(spots, sites, town, npc.body.m_pos);
				if (region >= 0)
				{
					SiteTally& tally = snapshot->tallies[static_cast<size_t>(region)];
					tally.npc_here += 1;
					tally.jobs[static_cast<size_t>(npc.job)] += 1;

					if (IsFightTask(npc.task))
					{
						tally.npc_fighting += 1;
					}

					if (IsFleeTask(npc.task))
					{
						tally.npc_fleeing += 1;
					}

					if (IsNpcOutlaw(npc))
					{
						tally.outlaw_here += 1;
					}
				}
			}

			FillNpcRow(npc, static_cast<int32>(index), region, &snapshot->npcs[index]);
		}
	}

	void GatherMonsters(const std::vector<WorldPos>& spots, int32 town, Snapshot* snapshot)
	{
		const std::vector<ZoneSite>& sites = snapshot->zone->sites;
		snapshot->monster_slots = GetMonsterCount();
		snapshot->monsters.reserve(static_cast<size_t>(snapshot->monster_slots));

		for (int32 index = 0; index < snapshot->monster_slots; ++index)
		{
			MonsterReport report;
			if (GetMonsterReport(index, &report) == false)
			{
				continue;
			}

			// 다시 나오길 기다리는 몬스터는 처음 선 자리 거점 몫
			if (report.is_alive == false)
			{
				const int32 home_region = FindRegion(spots, sites, town, report.home);
				if (home_region >= 0)
				{
					snapshot->tallies[static_cast<size_t>(home_region)].monster_respawning += 1;
				}

				continue;
			}

			snapshot->monster_alive += 1;
			if (report.is_fighting)
			{
				snapshot->monster_fighting += 1;
			}

			if (report.is_elite)
			{
				snapshot->elite_alive += 1;
			}

			const int32 region = FindRegion(spots, sites, town, report.pos);
			if (region >= 0)
			{
				SiteTally& tally = snapshot->tallies[static_cast<size_t>(region)];
				tally.monster_here += 1;
				tally.monster_kinds[static_cast<size_t>(report.kind)] += 1;

				if (report.is_elite)
				{
					tally.monster_elite += 1;
				}

				if (report.is_fighting)
				{
					tally.monster_fighting += 1;
				}
			}

			for (size_t site = 0; site < spots.size(); ++site)
			{
				if (GetDistance(spots[site], report.pos) <= THREAT_RADIUS)
				{
					snapshot->tallies[site].monster_near += 1;
				}
			}

			MonsterRow row;
			row.id = index;
			row.kind = report.kind;
			row.is_elite = report.is_elite;
			row.is_fighting = report.is_fighting;
			row.region = region;
			row.point = ToTilePoint(report.pos);

			snapshot->monsters.push_back(row);
		}
	}

	std::shared_ptr<const Snapshot> GatherSnapshot(float interval)
	{
		const Clock::time_point begin = Clock::now();

		std::shared_ptr<Snapshot> snapshot = std::make_shared<Snapshot>();
		snapshot->zone = g_game_zone;

		const std::vector<ZoneSite>& sites = g_game_zone->sites;
		std::vector<WorldPos> spots(sites.size());
		int32 town = -1;

		for (size_t index = 0; index < sites.size(); ++index)
		{
			spots[index] = ZoneTileToWorld(sites[index].tile.x, sites[index].tile.y, 0.5f, 0.5f);

			if (sites[index].kind == SITE_TOWN && town < 0)
			{
				town = static_cast<int32>(index);
			}
		}

		snapshot->tallies.resize(sites.size());

		GatherNpcs(spots, town, snapshot.get());
		GatherMonsters(spots, town, snapshot.get());

		const Player& player = GetPlayer();
		snapshot->player_point = ToTilePoint(player.m_pos);
		snapshot->player_hp = player.m_hp;
		snapshot->player_max_hp = player.m_stat.max_hp;
		snapshot->player_level = player.m_level;
		snapshot->player_gold = GetPlayerGold();
		snapshot->is_player_dead = IsPlayerDead();
		snapshot->player_region = FindRegion(spots, sites, town, player.m_pos);

		snapshot->is_boss_summoned = IsBossSummoned();
		snapshot->is_boss_dead = IsBossDead();

		WorldPos boss_pos;
		if (GetBossPos(&boss_pos))
		{
			snapshot->has_boss_pos = true;
			snapshot->boss_point = ToTilePoint(boss_pos);
		}

		for (const NpcEvent& event : GetNpcEvents())
		{
			EventRow row;
			row.serial = event.serial;
			row.text = event.text;
			row.r = event.r;
			row.g = event.g;
			row.b = event.b;

			snapshot->events.push_back(row);
		}

		snapshot->deaths = GetNpcDeathCount();
		snapshot->robberies = GetNpcRobberyCount();
		snapshot->treasury = GetTreasury();
		snapshot->npc_tick_us = GetNpcTickMicroseconds();

		g_sequence += 1;
		snapshot->sequence = g_sequence;
		snapshot->time = g_zone_seconds;
		snapshot->interval = interval;

		const std::chrono::duration<float, std::micro> elapsed = Clock::now() - begin;
		snapshot->gather_us = static_cast<int32>(elapsed.count());

		return snapshot;
	}

	// ------------------------------------------------------------ JSON 굽기 ( 모니터 스레드 )

	std::string ToUtf8(const std::wstring& text)
	{
		if (text.empty())
		{
			return std::string();
		}

		const int32 length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int32>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string utf8(static_cast<size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int32>(text.size()), &utf8[0], length, nullptr, nullptr);

		return utf8;
	}

	// float 를 그대로 넣으면 102.30000305 로 찍힌다
	double RoundHundredths(float value)
	{
		return std::round(static_cast<double>(value) * 100.0) / 100.0;
	}

	std::string JoinBrain(const std::vector<const wchar_t*>& names)
	{
		std::wstring path;

		for (const wchar_t* name : names)
		{
			if (path.empty() == false)
			{
				path += L" > ";
			}

			path += name;
		}

		return ToUtf8(path);
	}

	Json BuildNpcJson(const NpcRow& row)
	{
		Json traits;
		traits["greed"] = RoundHundredths(row.greed);
		traits["bravery"] = RoundHundredths(row.bravery);
		traits["cruelty"] = RoundHundredths(row.cruelty);
		traits["diligence"] = RoundHundredths(row.diligence);

		Json entry;
		entry["id"] = row.id;
		entry["alive"] = row.is_alive;
		entry["name"] = ToUtf8(row.name);
		entry["job"] = row.job;
		entry["job_name"] = ToUtf8(GetNpcJobName(row.job));
		entry["level"] = row.level;
		entry["hp"] = row.hp;
		entry["max_hp"] = row.max_hp;
		entry["hunger"] = RoundHundredths(row.hunger);
		entry["gold"] = row.gold;
		entry["goods"] = row.goods;
		entry["goods_capacity"] = row.goods_capacity;
		entry["kills"] = row.kills;
		entry["gear"] = row.gear;
		entry["task"] = row.task;
		entry["task_name"] = ToUtf8(row.task_name);
		entry["stance_name"] = ToUtf8(row.stance_name);
		entry["outlaw_sec"] = RoundHundredths(row.outlaw_sec);
		entry["home"] = row.home;
		entry["region"] = row.region;
		entry["x"] = RoundHundredths(row.point.x);
		entry["y"] = RoundHundredths(row.point.y);
		entry["brain"] = JoinBrain(row.brain);
		entry["traits"] = traits;

		return entry;
	}

	Json BuildSiteJson(const ZoneInfo& zone, size_t index, const SiteTally& tally)
	{
		Json npc;
		npc["here"] = tally.npc_here;
		npc["fighting"] = tally.npc_fighting;
		npc["fleeing"] = tally.npc_fleeing;
		npc["outlaws"] = tally.outlaw_here;
		npc["jobs"] = tally.jobs;
		npc["residents"] = tally.resident_alive;
		npc["resident_slots"] = tally.resident_slots;

		Json monster;
		monster["here"] = tally.monster_here;
		monster["elite"] = tally.monster_elite;
		monster["fighting"] = tally.monster_fighting;
		monster["near"] = tally.monster_near;
		monster["respawning"] = tally.monster_respawning;
		monster["kinds"] = tally.monster_kinds;

		Json entry;
		entry["id"] = static_cast<int32>(index);
		entry["kind"] = zone.sites[index].kind;
		entry["name"] = ToUtf8(zone.site_names[index]);
		entry["walk"] = zone.sites[index].walk_distance;
		entry["npc"] = npc;
		entry["monster"] = monster;

		return entry;
	}

	std::string BuildStateJson(const Snapshot& snapshot)
	{
		const Clock::time_point begin = Clock::now();

		Json npcs = Json::array();
		for (const NpcRow& row : snapshot.npcs)
		{
			npcs.push_back(BuildNpcJson(row));
		}

		Json monsters = Json::array();
		for (const MonsterRow& row : snapshot.monsters)
		{
			Json entry;
			entry["id"] = row.id;
			entry["kind"] = row.kind;
			entry["elite"] = row.is_elite;
			entry["fighting"] = row.is_fighting;
			entry["region"] = row.region;
			entry["x"] = RoundHundredths(row.point.x);
			entry["y"] = RoundHundredths(row.point.y);

			monsters.push_back(entry);
		}

		Json sites = Json::array();
		for (size_t index = 0; index < snapshot.tallies.size(); ++index)
		{
			sites.push_back(BuildSiteJson(*snapshot.zone, index, snapshot.tallies[index]));
		}

		// 일지. 페이지가 serial 로 이어 붙인다
		Json events = Json::array();
		for (const EventRow& row : snapshot.events)
		{
			Json entry;
			entry["serial"] = row.serial;
			entry["text"] = ToUtf8(row.text);
			entry["color"] = { RoundHundredths(row.r), RoundHundredths(row.g), RoundHundredths(row.b) };

			events.push_back(entry);
		}

		Json player;
		player["x"] = RoundHundredths(snapshot.player_point.x);
		player["y"] = RoundHundredths(snapshot.player_point.y);
		player["hp"] = snapshot.player_hp;
		player["max_hp"] = snapshot.player_max_hp;
		player["level"] = snapshot.player_level;
		player["gold"] = snapshot.player_gold;
		player["dead"] = snapshot.is_player_dead;
		player["region"] = snapshot.player_region;

		Json boss;
		boss["summoned"] = snapshot.is_boss_summoned;
		boss["dead"] = snapshot.is_boss_dead;
		if (snapshot.has_boss_pos)
		{
			boss["x"] = RoundHundredths(snapshot.boss_point.x);
			boss["y"] = RoundHundredths(snapshot.boss_point.y);
		}

		Json summary;
		summary["npc_alive"] = snapshot.npc_alive;
		summary["npc_slots"] = static_cast<int32>(snapshot.npcs.size());
		summary["npc_jobs"] = snapshot.job_alive;
		summary["npc_gold"] = snapshot.npc_gold;
		summary["outlaws"] = snapshot.outlaws;
		summary["deaths"] = snapshot.deaths;
		summary["robberies"] = snapshot.robberies;
		summary["treasury"] = snapshot.treasury;
		summary["npc_tick_us"] = RoundHundredths(snapshot.npc_tick_us);
		summary["monster_alive"] = snapshot.monster_alive;
		summary["monster_slots"] = snapshot.monster_slots;
		summary["monster_fighting"] = snapshot.monster_fighting;
		summary["elite_alive"] = snapshot.elite_alive;

		Json state;
		state["type"] = "state";
		state["seq"] = snapshot.sequence;
		state["seed"] = snapshot.zone->seed;
		state["time"] = RoundHundredths(snapshot.time);
		state["interval"] = RoundHundredths(snapshot.interval);
		state["gather_us"] = snapshot.gather_us;
		state["summary"] = summary;
		state["player"] = player;
		state["boss"] = boss;
		state["sites"] = sites;
		state["npcs"] = npcs;
		state["monsters"] = monsters;
		state["events"] = events;

		// 직렬화는 빼고 JSON 트리를 짓기까지
		const std::chrono::duration<float, std::micro> elapsed = Clock::now() - begin;
		state["json_us"] = static_cast<int32>(elapsed.count());

		return state.dump(-1, ' ', false, Json::error_handler_t::replace);
	}

	std::string BuildZoneJson(const ZoneInfo& zone)
	{
		Json sites = Json::array();
		for (size_t index = 0; index < zone.sites.size(); ++index)
		{
			const ZoneSite& site = zone.sites[index];

			Json entry;
			entry["id"] = static_cast<int32>(index);
			entry["kind"] = site.kind;
			entry["kind_name"] = ToUtf8(GetSiteKindName(site.kind));
			entry["name"] = ToUtf8(zone.site_names[index]);
			entry["x"] = site.tile.x;
			entry["y"] = site.tile.y;
			entry["walk"] = site.walk_distance;

			sites.push_back(entry);
		}

		Json center;
		center["x"] = zone.center.x;
		center["y"] = zone.center.y;

		Json message;
		message["type"] = "zone";
		message["seed"] = zone.seed;
		message["size"] = ZONE_TILE_COUNT;
		message["tiles"] = zone.tiles;
		message["center"] = center;
		message["sites"] = sites;
		message["threat_radius"] = THREAT_RADIUS;

		return message.dump(-1, ' ', false, Json::error_handler_t::replace);
	}

	// 게임 스레드가 새로 넘긴 게 있으면 한 번만 굽는다. 스트림이 여럿이어도 굽기는 한 번
	void RefreshMessages()
	{
		std::shared_ptr<const ZoneInfo> zone;
		std::shared_ptr<const Snapshot> snapshot;
		uint64 zone_serial = 0;
		uint64 snapshot_serial = 0;

		{
			std::lock_guard<std::mutex> guard(g_publish_lock);
			zone = g_published_zone;
			snapshot = g_published_snapshot;
			zone_serial = g_zone_serial;
			snapshot_serial = g_snapshot_serial;
		}

		if (zone != nullptr && zone_serial != g_zone_json_serial)
		{
			g_zone_json = BuildZoneJson(*zone);
			g_zone_json_serial = zone_serial;

			// 옛 구역 상태가 새 지도 위에 찍히지 않게
			g_state_json.clear();
		}

		if (snapshot != nullptr && snapshot_serial != g_state_json_serial && snapshot->zone == zone)
		{
			g_state_json = BuildStateJson(*snapshot);
			g_state_json_serial = snapshot_serial;
		}
	}

	// ------------------------------------------------------------ 소켓 ( 모니터 스레드 )

	bool SendAll(SOCKET socket, const std::string& text)
	{
		size_t sent = 0;

		while (sent < text.size())
		{
			const int32 result = send(socket, text.data() + sent, static_cast<int32>(text.size() - sent), 0);
			if (result == SOCKET_ERROR || result == 0)
			{
				return false;
			}

			sent += static_cast<size_t>(result);
		}

		return true;
	}

	void CloseClient(SOCKET socket)
	{
		shutdown(socket, SD_SEND);
		closesocket(socket);
	}

	void SendResponse(SOCKET socket, const char* status, const char* content_type, const std::string& body)
	{
		std::string header = std::string("HTTP/1.1 ") + status + "\r\n";
		header += std::string("Content-Type: ") + content_type + "\r\n";
		header += "Content-Length: " + std::to_string(body.size()) + "\r\n";
		header += "Cache-Control: no-store\r\n";
		header += "Access-Control-Allow-Origin: *\r\n";
		header += "Connection: close\r\n\r\n";

		if (SendAll(socket, header))
		{
			SendAll(socket, body);
		}
	}

	bool ReadPage(std::string* body)
	{
		std::ifstream file(PAGE_PATH, std::ios::binary);
		if (file.is_open() == false)
		{
			return false;
		}

		body->assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

		return true;
	}

	// "GET /events?x=1 HTTP/1.1" -> "/events"
	std::string GetRequestPath(const std::string& request)
	{
		const size_t first_space = request.find(' ');
		if (first_space == std::string::npos)
		{
			return std::string();
		}

		const size_t second_space = request.find(' ', first_space + 1);
		if (second_space == std::string::npos)
		{
			return std::string();
		}

		std::string path = request.substr(first_space + 1, second_space - first_space - 1);

		const size_t query = path.find('?');
		if (query != std::string::npos)
		{
			path.resize(query);
		}

		return path;
	}

	void UpdateStreamCount()
	{
		g_stream_count.store(static_cast<int32>(g_streams.size()));
	}

	void OpenStream(SOCKET socket)
	{
		if (g_streams.size() >= STREAM_MAX_COUNT)
		{
			SendResponse(socket, "503 Service Unavailable", "text/plain; charset=utf-8", "too many monitor pages");
			CloseClient(socket);
			return;
		}

		// 끊기면 브라우저가 1 초 뒤 다시 붙는다
		std::string header = "HTTP/1.1 200 OK\r\n";
		header += "Content-Type: text/event-stream; charset=utf-8\r\n";
		header += "Cache-Control: no-store\r\n";
		header += "Access-Control-Allow-Origin: *\r\n";
		header += "Connection: keep-alive\r\n\r\n";
		header += "retry: 1000\n\n";

		if (SendAll(socket, header) == false)
		{
			closesocket(socket);
			return;
		}

		StreamClient stream;
		stream.socket = socket;
		g_streams.push_back(stream);
		UpdateStreamCount();

		std::cout << "[ Monitor - OpenStream ] : viewers " << g_streams.size() << "\n";
	}

	void SendCached(SOCKET socket, const std::string& message)
	{
		if (message.empty())
		{
			SendResponse(socket, "503 Service Unavailable", "application/json; charset=utf-8", "{\"type\":\"error\",\"message\":\"not ready\"}");
			return;
		}

		SendResponse(socket, "200 OK", "application/json; charset=utf-8", message);
	}

	void HandleRequest(SOCKET socket, const std::string& request)
	{
		if (request.compare(0, 4, "GET ") != 0)
		{
			SendResponse(socket, "405 Method Not Allowed", "text/plain; charset=utf-8", "GET only");
			CloseClient(socket);
			return;
		}

		const std::string path = GetRequestPath(request);

		if (path == "/events")
		{
			OpenStream(socket);
			return;
		}

		if (path == "/state")
		{
			SendCached(socket, g_state_json);
		}
		else if (path == "/zone")
		{
			SendCached(socket, g_zone_json);
		}
		else if (path == "/" || path == "/index.html")
		{
			// 매번 디스크에서 읽는다. 페이지만 고치면 새로고침으로 끝난다
			std::string page;
			if (ReadPage(&page))
			{
				SendResponse(socket, "200 OK", "text/html; charset=utf-8", page);
			}
			else
			{
				SendResponse(socket, "404 Not Found", "text/plain; charset=utf-8", "Monitor/index.html missing - run the game from SimpleGame/");
			}
		}
		else
		{
			SendResponse(socket, "404 Not Found", "text/plain; charset=utf-8", "not found");
		}

		CloseClient(socket);
	}

	void AcceptClient()
	{
		const SOCKET socket = accept(g_listen_socket, nullptr, nullptr);
		if (socket == INVALID_SOCKET)
		{
			return;
		}

		if (g_pending.size() >= PENDING_MAX_COUNT)
		{
			closesocket(socket);
			return;
		}

		// 멈춘 페이지 하나가 모니터 스레드를 붙잡지 않게
		const DWORD send_timeout = SEND_TIMEOUT_MS;
		setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&send_timeout), sizeof(send_timeout));

		PendingClient client;
		client.socket = socket;
		client.since = Clock::now();
		g_pending.push_back(client);
	}

	// select 가 읽을 게 있다고 한 소켓만 recv 한다. 소켓이 블로킹이라 그냥 부르면 멈춘다
	void ReadPending(fd_set* read_set)
	{
		for (size_t index = 0; index < g_pending.size();)
		{
			PendingClient& client = g_pending[index];

			if (FD_ISSET(client.socket, read_set) == 0)
			{
				++index;
				continue;
			}

			std::array<char, 2048> buffer = {};
			const int32 received = recv(client.socket, buffer.data(), static_cast<int32>(buffer.size()), 0);

			if (received <= 0)
			{
				closesocket(client.socket);
				g_pending.erase(g_pending.begin() + static_cast<std::ptrdiff_t>(index));
				continue;
			}

			client.request.append(buffer.data(), static_cast<size_t>(received));

			if (client.request.find("\r\n\r\n") == std::string::npos)
			{
				if (client.request.size() > REQUEST_MAX_BYTES)
				{
					closesocket(client.socket);
					g_pending.erase(g_pending.begin() + static_cast<std::ptrdiff_t>(index));
					continue;
				}

				++index;
				continue;
			}

			const SOCKET socket = client.socket;
			const std::string request = client.request;
			g_pending.erase(g_pending.begin() + static_cast<std::ptrdiff_t>(index));

			HandleRequest(socket, request);
		}
	}

	void DropStream(size_t index)
	{
		closesocket(g_streams[index].socket);
		g_streams.erase(g_streams.begin() + static_cast<std::ptrdiff_t>(index));
		UpdateStreamCount();

		std::cout << "[ Monitor - DropStream ] : viewers " << g_streams.size() << "\n";
	}

	// 스트림은 페이지가 보낼 게 없다. 읽을 게 생겼으면 닫힌 것
	void ReadStreams(fd_set* read_set)
	{
		for (size_t index = 0; index < g_streams.size();)
		{
			if (FD_ISSET(g_streams[index].socket, read_set) == 0)
			{
				++index;
				continue;
			}

			std::array<char, 512> buffer = {};
			const int32 received = recv(g_streams[index].socket, buffer.data(), static_cast<int32>(buffer.size()), 0);

			if (received <= 0)
			{
				DropStream(index);
				continue;
			}

			++index;
		}
	}

	void ExpirePending()
	{
		const Clock::time_point now = Clock::now();

		for (size_t index = 0; index < g_pending.size();)
		{
			const int64 waited_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_pending[index].since).count();

			if (waited_ms < REQUEST_TIMEOUT_MS)
			{
				++index;
				continue;
			}

			closesocket(g_pending[index].socket);
			g_pending.erase(g_pending.begin() + static_cast<std::ptrdiff_t>(index));
		}
	}

	void PushStreams()
	{
		std::string frame;

		for (size_t index = 0; index < g_streams.size();)
		{
			StreamClient& stream = g_streams[index];
			bool is_sent = true;

			// 지도가 상태보다 먼저 가야 페이지가 거점 이름을 안다
			if (g_zone_json.empty() == false && stream.zone_serial != g_zone_json_serial)
			{
				frame = "event: zone\ndata: " + g_zone_json + "\n\n";
				is_sent = SendAll(stream.socket, frame);
				stream.zone_serial = g_zone_json_serial;
			}

			if (is_sent && g_state_json.empty() == false && stream.state_serial != g_state_json_serial)
			{
				frame = "data: " + g_state_json + "\n\n";
				is_sent = SendAll(stream.socket, frame);
				stream.state_serial = g_state_json_serial;
			}

			if (is_sent == false)
			{
				DropStream(index);
				continue;
			}

			++index;
		}
	}

	void RunServer()
	{
		while (g_running.load())
		{
			fd_set read_set;
			FD_ZERO(&read_set);
			FD_SET(g_listen_socket, &read_set);

			for (const PendingClient& client : g_pending)
			{
				FD_SET(client.socket, &read_set);
			}

			for (const StreamClient& stream : g_streams)
			{
				FD_SET(stream.socket, &read_set);
			}

			timeval timeout = {};
			timeout.tv_usec = SELECT_TIMEOUT_MS * 1000;

			const int32 ready = select(0, &read_set, nullptr, nullptr, &timeout);
			if (ready == SOCKET_ERROR)
			{
				std::cout << "[ Monitor - RunServer ] : select failed " << WSAGetLastError() << "\n";
				break;
			}

			// 요청에 답하기 전에 구워 둬야 /state 가 최신을 준다
			RefreshMessages();

			if (ready > 0)
			{
				// 스트림 먼저. 대기 소켓이 이번에 스트림으로 옮겨 가면 같은 읽기 표시로 한 번 더 recv 해 멈춘다
				ReadStreams(&read_set);
				ReadPending(&read_set);

				if (FD_ISSET(g_listen_socket, &read_set) != 0)
				{
					AcceptClient();
				}
			}

			ExpirePending();
			PushStreams();
		}

		for (const PendingClient& client : g_pending)
		{
			closesocket(client.socket);
		}

		for (const StreamClient& stream : g_streams)
		{
			closesocket(stream.socket);
		}

		g_pending.clear();
		g_streams.clear();
		UpdateStreamCount();
	}

	SOCKET OpenListenSocket(uint16 port)
	{
		const SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_socket == INVALID_SOCKET)
		{
			return INVALID_SOCKET;
		}

		// 루프백만 연다. 바깥 주소를 열면 방화벽 허용 창이 뜬다
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(port);

		if (bind(listen_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
		{
			closesocket(listen_socket);
			return INVALID_SOCKET;
		}

		if (listen(listen_socket, SOMAXCONN) != 0)
		{
			closesocket(listen_socket);
			return INVALID_SOCKET;
		}

		return listen_socket;
	}
}

bool InitMonitor()
{
	WSADATA wsa_data = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
	{
		std::cout << "[ Monitor - InitMonitor ] : WSAStartup failed\n";
		return false;
	}

	g_wsa_ready = true;

	for (int32 attempt = 0; attempt < PORT_TRY_COUNT; ++attempt)
	{
		const uint16 port = static_cast<uint16>(MONITOR_PORT + attempt);

		g_listen_socket = OpenListenSocket(port);
		if (g_listen_socket != INVALID_SOCKET)
		{
			g_port = port;
			break;
		}
	}

	if (g_listen_socket == INVALID_SOCKET)
	{
		std::cout << "[ Monitor - InitMonitor ] : no free port from " << MONITOR_PORT << "\n";

		WSACleanup();
		g_wsa_ready = false;

		return false;
	}

	g_running.store(true);
	g_thread = new std::thread(RunServer);

	std::cout << "[ Monitor - InitMonitor ] : http://127.0.0.1:" << g_port << "/  ( B key opens it )\n";

	return true;
}

void ShutdownMonitor()
{
	g_running.store(false);

	// select 가 SELECT_TIMEOUT_MS 안에 돌아와 루프를 빠져나온다
	if (g_thread != nullptr)
	{
		g_thread->join();

		delete g_thread;
		g_thread = nullptr;
	}

	if (g_listen_socket != INVALID_SOCKET)
	{
		closesocket(g_listen_socket);
		g_listen_socket = INVALID_SOCKET;
	}

	if (g_wsa_ready)
	{
		WSACleanup();
		g_wsa_ready = false;
	}
}

void BuildMonitorZone(const ZoneLayout& layout)
{
	std::shared_ptr<ZoneInfo> zone = std::make_shared<ZoneInfo>();
	zone->seed = layout.seed;
	zone->center = layout.center;
	zone->sites = layout.sites;
	zone->site_names = BuildSiteNames(layout.sites);

	// 칸 하나 = 글자 하나. 224 x 224 를 숫자 배열로 보내면 네 배로 붓는다
	zone->tiles.assign(layout.tiles.size(), 'A');
	for (size_t index = 0; index < layout.tiles.size(); ++index)
	{
		zone->tiles[index] = static_cast<char>('A' + layout.tiles[index]);
	}

	g_game_zone = zone;
	g_zone_seconds = 0.f;

	// 다음 갱신에서 바로 새 구역 상태를 낸다
	g_publish_timer = IDLE_INTERVAL_SEC;

	std::lock_guard<std::mutex> guard(g_publish_lock);
	g_published_zone = zone;
	g_published_snapshot.reset();
	g_zone_serial += 1;
}

void UpdateMonitor(float real_seconds)
{
	if (g_running.load() == false || g_game_zone == nullptr)
	{
		return;
	}

	g_zone_seconds += real_seconds;
	g_publish_timer += real_seconds;

	float interval = IDLE_INTERVAL_SEC;
	if (g_stream_count.load() > 0)
	{
		interval = WATCHED_INTERVAL_SEC;
	}

	if (g_publish_timer < interval)
	{
		return;
	}

	g_publish_timer = 0.f;

	const std::shared_ptr<const Snapshot> snapshot = GatherSnapshot(interval);

	std::lock_guard<std::mutex> guard(g_publish_lock);
	g_published_snapshot = snapshot;
	g_snapshot_serial += 1;
}

bool OpenMonitorPage()
{
	if (g_running.load() == false)
	{
		return false;
	}

	const std::wstring url = L"http://127.0.0.1:" + std::to_wstring(g_port) + L"/";
	const HINSTANCE result = ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	// ShellExecute 는 32 이하를 실패로 돌려준다
	return reinterpret_cast<INT_PTR>(result) > 32;
}
