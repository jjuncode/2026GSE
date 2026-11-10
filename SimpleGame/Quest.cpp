#include "stdafx.h"
#include "Quest.h"
#include "Audio.h"
#include "Drop.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Inventory.h"
#include "Monster.h"
#include "NavGrid.h"
#include "Player.h"
#include "Rank.h"
#include "Renderer.h"
#include "Text.h"
#include "UiDraw.h"
#include "Village.h"
#include "ZoneGen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <random>

namespace
{
	const float PI = 3.14159265f;

	const float PATH_REFRESH_SEC = 0.4f;
	constexpr int32 PATH_DRAW_STEPS = 40;	// 화면 하나 반쯤. 더 그리면 바닥이 점으로 덮인다
	const float GUIDE_MARGIN = 34.f;
	const float TRACKER_WIDTH = 380.f;
	const float BANNER_STACK = 30.f;
	const float BANNER_STACK_SEC = 0.6f;
	const float TRACKER_ROW = 54.f;
	const float BANNER_LIFT = 120.f;

	struct BoardState
	{
		std::vector<Quest> offers;
		float refresh_timer = 0.f;
		uint32 epoch = 0;
	};

	std::vector<BoardState> g_boards;
	std::vector<Quest> g_active;
	std::vector<ZoneSite> g_sites;
	WorldPos g_boss_pos;
	uint32 g_seed = 0;
	int32 g_serial = 0;
	int32 g_tracked_serial = -1;
	float g_time = 0.f;

	// 한 번에 둘 이상 끝나면 알림 글자가 한 자리에 겹친다. 잠깐 사이에 낸 것은 위로 쌓는다
	float g_banner_time = -100.f;
	int32 g_banner_stack = 0;

	// 안내 길. 목적지가 바뀌거나 내가 움직이면 다시 찾는다
	std::vector<WorldPos> g_path;
	float g_path_timer = 0.f;
	int32 g_path_from_x = -9999;
	int32 g_path_from_y = -9999;
	int32 g_path_to_x = -9999;
	int32 g_path_to_y = -9999;

	const std::vector<Quest> EMPTY_QUESTS;

	// 승급 심사는 게시판마다 같은 것 하나를 맨 앞에 붙인다. 어느 게시판에서 받든 같은 의뢰다
	Quest g_exam;
	bool g_has_exam = false;

	float NextBannerLift()
	{
		if (g_time - g_banner_time > BANNER_STACK_SEC)
		{
			g_banner_stack = 0;
		}

		g_banner_time = g_time;
		const float lift = BANNER_LIFT + BANNER_STACK * static_cast<float>(g_banner_stack);
		g_banner_stack += 1;

		return lift;
	}

	float GetDistance(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);

		return std::sqrt(delta_x * delta_x + delta_y * delta_y);
	}

	// 화면 기준 여덟 방위. 지도도 화면 각도로 서 있어서 그쪽과 말이 맞는다
	std::wstring DescribeDirection(const WorldPos& from, const WorldPos& to)
	{
		const float delta_x = GetTileDistanceX(from, to);
		const float delta_y = GetTileDistanceY(from, to);
		const float screen_x = delta_x - delta_y;
		const float screen_up = -(delta_x + delta_y) * 0.5f;
		const float angle = std::atan2(screen_up, screen_x);

		const std::array<const wchar_t*, 8> names = { L"동", L"북동", L"북", L"북서", L"서", L"남서", L"남", L"남동" };
		int32 octant = static_cast<int32>(std::floor((angle + PI / 8.f) / (PI / 4.f)));
		octant = ((octant % 8) + 8) % 8;

		return names[static_cast<size_t>(octant)];
	}

	std::wstring DescribePlace(int32 village, const WorldPos& pos, const wchar_t* what)
	{
		const WorldPos center = GetVillageCenterPos(village);
		return std::wstring(GetVillageName(village)) + L" " + DescribeDirection(center, pos) + L"쪽 " + what;
	}

	const wchar_t* GetSiteKindName(int32 kind)
	{
		switch (kind)
		{
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
			case SITE_LAIR:
			{
				return L"둥지";
			}
			default:
			{
				return L"광장";
			}
		}
	}

	int32 RollCount(const QuestTemplate& source, std::mt19937* rng)
	{
		std::uniform_int_distribution<int32> roll(source.count_min, source.count_max);
		return roll(*rng);
	}

	// 마을에서 가까운 사냥터 셋 중 하나. 늘 제일 가까운 데만 주면 의뢰가 다 한 곳으로 몰린다
	bool PickHuntingGround(int32 village, int32 kind, bool elite_only, int32 min_level, std::mt19937* rng, WorldPos* place, int32* level)
	{
		struct Candidate
		{
			float distance = 0.f;
			WorldPos home;
			int32 level = 1;
		};

		std::vector<Candidate> candidates;
		const WorldPos center = GetVillageCenterPos(village);

		for (int32 index = 0; index < GetMonsterCount(); ++index)
		{
			MonsterReport report;
			if (GetMonsterReport(index, &report) == false)
			{
				continue;
			}

			if (elite_only && report.is_elite == false)
			{
				continue;
			}

			if (elite_only == false && report.kind != kind)
			{
				continue;
			}

			if (report.level < min_level || GetVillageAt(report.home) >= 0)
			{
				continue;
			}

			Candidate candidate;
			candidate.distance = GetDistance(center, report.home);
			candidate.home = report.home;
			candidate.level = report.level;
			candidates.push_back(candidate);
		}

		if (candidates.empty())
		{
			return false;
		}

		std::sort(candidates.begin(), candidates.end(),
			[](const Candidate& left, const Candidate& right)
			{
				return left.distance < right.distance;
			});

		const int32 pool = std::min(3, static_cast<int32>(candidates.size()));
		std::uniform_int_distribution<int32> roll(0, pool - 1);
		const Candidate& picked = candidates[static_cast<size_t>(roll(*rng))];

		*place = picked.home;
		*level = picked.level;
		return true;
	}

	// 정예 레벨 중 가장 낮은 것. 특급 시련은 여기서 몇 레벨 위만 센다
	int32 GetLowestEliteLevel()
	{
		int32 lowest = 9999;
		int32 highest = 0;

		for (int32 index = 0; index < GetMonsterCount(); ++index)
		{
			MonsterReport report;
			if (GetMonsterReport(index, &report) && report.is_elite)
			{
				lowest = std::min(lowest, report.level);
				highest = std::max(highest, report.level);
			}
		}

		if (highest == 0)
		{
			return 1;
		}

		return lowest;
	}

	int32 GetHighestEliteLevel()
	{
		int32 highest = 1;

		for (int32 index = 0; index < GetMonsterCount(); ++index)
		{
			MonsterReport report;
			if (GetMonsterReport(index, &report) && report.is_elite)
			{
				highest = std::max(highest, report.level);
			}
		}

		return highest;
	}

	bool PickSite(int32 village, int32 site_kind, std::mt19937* rng, WorldPos* place)
	{
		std::vector<std::pair<float, WorldPos>> candidates;
		const WorldPos center = GetVillageCenterPos(village);

		for (const ZoneSite& site : g_sites)
		{
			if (site.kind != site_kind)
			{
				continue;
			}

			const WorldPos pos = ZoneTileToWorld(site.tile.x, site.tile.y, 0.5f, 0.5f);
			candidates.push_back(std::make_pair(GetDistance(center, pos), pos));
		}

		if (site_kind == SITE_LAIR && candidates.empty())
		{
			candidates.push_back(std::make_pair(GetDistance(center, g_boss_pos), g_boss_pos));
		}

		if (candidates.empty())
		{
			return false;
		}

		std::sort(candidates.begin(), candidates.end(),
			[](const std::pair<float, WorldPos>& left, const std::pair<float, WorldPos>& right)
			{
				return left.first < right.first;
			});

		const int32 pool = std::min(3, static_cast<int32>(candidates.size()));
		std::uniform_int_distribution<int32> roll(0, pool - 1);
		*place = candidates[static_cast<size_t>(roll(*rng))].second;
		return true;
	}

	bool MakeQuest(const QuestTemplate& source, int32 village, std::mt19937* rng, Quest* quest)
	{
		quest->village = village;
		quest->tier = source.tier;
		quest->goal = source.goal;
		quest->title = source.title;
		quest->desc = source.desc;
		quest->reward = source.reward;
		quest->level = source.level;
		quest->need = RollCount(source, rng);

		switch (source.goal)
		{
			case QUEST_GOAL_HUNT:
			{
				quest->target = source.monster_type - 1;
				if (PickHuntingGround(village, quest->target, false, 0, rng, &quest->place, &quest->level) == false)
				{
					return false;
				}

				quest->has_place = true;
				quest->place_name = DescribePlace(village, quest->place, L"사냥터");
				break;
			}
			case QUEST_GOAL_ELITE:
			{
				// 쉽게 못 밀게 - 특급 시련은 가장 약한 정예보다 몇 레벨 위만 센다
				int32 min_level = 0;
				if (source.level_bonus > 0)
				{
					min_level = std::min(GetLowestEliteLevel() + source.level_bonus, GetHighestEliteLevel());
				}

				quest->min_level = min_level;
				if (PickHuntingGround(village, -1, true, min_level, rng, &quest->place, &quest->level) == false)
				{
					return false;
				}

				quest->has_place = true;
				quest->place_name = DescribePlace(village, quest->place, L"정예 출몰지");
				break;
			}
			case QUEST_GOAL_BOSS:
			{
				quest->target = source.monster_type - 1;
				quest->need = 1;
				quest->level = GetMonsterData(source.monster_type).level;
				quest->place = g_boss_pos;
				quest->has_place = true;
				quest->place_name = DescribePlace(village, quest->place, L"둥지");
				break;
			}
			case QUEST_GOAL_SCOUT:
			{
				quest->need = 1;
				quest->target = source.site_kind;
				if (PickSite(village, source.site_kind, rng, &quest->place) == false)
				{
					return false;
				}

				quest->has_place = true;
				quest->place_name = DescribePlace(village, quest->place, GetSiteKindName(source.site_kind));
				break;
			}
			default:
			{
				quest->target = source.item_id;
				break;
			}
		}

		g_serial += 1;
		quest->serial = g_serial;
		quest->state = QUEST_STATE_OFFERED;
		return true;
	}

	// 단계마다 정해진 칸 수만큼. 같은 단계 안에선 겹치지 않게 섞어서 앞에서부터 쓴다
	void FillBoard(int32 village)
	{
		BoardState& board = g_boards[static_cast<size_t>(village)];
		board.offers.clear();

		const QuestConfig& config = GetQuestConfig();
		std::mt19937 rng(g_seed ^ (static_cast<uint32>(village + 1) * 7919u) ^ (board.epoch * 104729u));

		for (int32 tier = 0; tier < QUEST_TIER_COUNT; ++tier)
		{
			std::vector<const QuestTemplate*> pool;
			for (const QuestTemplate& source : GetQuestTemplates())
			{
				if (source.tier == tier)
				{
					pool.push_back(&source);
				}
			}

			if (pool.empty())
			{
				continue;
			}

			std::shuffle(pool.begin(), pool.end(), rng);

			const int32 slots = config.tier_slots[static_cast<size_t>(tier)];
			for (int32 slot = 0; slot < slots; ++slot)
			{
				Quest quest;
				const QuestTemplate& source = *pool[static_cast<size_t>(slot) % pool.size()];
				if (MakeQuest(source, village, &rng, &quest))
				{
					board.offers.push_back(quest);
				}
			}
		}

		if (static_cast<int32>(board.offers.size()) > config.board_size)
		{
			board.offers.resize(static_cast<size_t>(config.board_size));
		}

		// 판을 갈아도 아직 안 받은 심사는 그대로 붙어 있다
		if (g_has_exam && g_exam.state != QUEST_STATE_REPORTED)
		{
			board.offers.insert(board.offers.begin(), g_exam);
		}

		board.refresh_timer = config.refresh_sec;
	}

	// 공헌이 차면 다음 등급 심사를 성채 기준으로 하나 만들어 모든 게시판 맨 앞에 붙인다
	void UpdateExamOffer()
	{
		if (g_has_exam && g_exam.state != QUEST_STATE_REPORTED)
		{
			return;
		}

		if (IsRankExamReady() == false)
		{
			return;
		}

		const RankData& next = GetRankData(GetPlayerRank() + 1);
		std::mt19937 rng(g_seed ^ static_cast<uint32>(GetPlayerRank() * 31337));

		Quest exam;
		if (MakeQuest(next.exam, ZONE_CITADEL, &rng, &exam) == false)
		{
			return;
		}

		exam.is_exam = true;
		exam.exam_rank = GetPlayerRank() + 1;
		g_exam = exam;
		g_has_exam = true;

		for (BoardState& board : g_boards)
		{
			board.offers.insert(board.offers.begin(), g_exam);
		}

		std::cout << "[ Quest - UpdateExamOffer ] : rank " << GetPlayerRank() + 1 << " serial " << exam.serial << "\n";
	}

	Quest* FindActive(int32 serial)
	{
		for (Quest& quest : g_active)
		{
			if (quest.serial == serial)
			{
				return &quest;
			}
		}

		return nullptr;
	}

	void MarkOffer(int32 serial, int32 state)
	{
		if (g_has_exam && g_exam.serial == serial)
		{
			g_exam.state = state;
		}

		for (BoardState& board : g_boards)
		{
			for (Quest& offer : board.offers)
			{
				if (offer.serial == serial)
				{
					offer.state = state;
				}
			}
		}
	}

	void CompleteGoal(Quest* quest)
	{
		if (quest->state == QUEST_STATE_DONE)
		{
			return;
		}

		quest->state = QUEST_STATE_DONE;
		MarkOffer(quest->serial, QUEST_STATE_DONE);

		AddDamageText(GetPlayer().m_pos, NextBannerLift(), L"의뢰 목표 달성  " + quest->title, DAMAGE_STYLE_BANNER_GOOD);
		PlaySfx(SFX_SKILL_UNLOCK, 0.8f);

		std::cout << "[ Quest - CompleteGoal ] : serial " << quest->serial << " goal " << quest->goal << "\n";
	}

	void UpdateLiveProgress()
	{
		const Player& player = GetPlayer();
		const float scout_radius = GetQuestConfig().scout_radius;

		for (Quest& quest : g_active)
		{
			if (quest.goal == QUEST_GOAL_DELIVER)
			{
				// 가방에 든 만큼. 팔거나 쓰면 다시 모자라다
				quest.progress = std::min(player.m_inventory.CountItem(quest.target), quest.need);
				if (quest.progress >= quest.need)
				{
					CompleteGoal(&quest);
				}
				else if (quest.state == QUEST_STATE_DONE)
				{
					quest.state = QUEST_STATE_ACTIVE;
					MarkOffer(quest.serial, QUEST_STATE_ACTIVE);
				}

				continue;
			}

			if (quest.goal == QUEST_GOAL_SCOUT && quest.state == QUEST_STATE_ACTIVE && player.IsDead() == false)
			{
				if (GetDistance(player.m_pos, quest.place) <= scout_radius)
				{
					quest.progress = 1;
					CompleteGoal(&quest);
				}
			}
		}
	}

	void UpdateBoards(float real_seconds)
	{
		for (size_t village = 0; village < g_boards.size(); ++village)
		{
			BoardState& board = g_boards[village];
			board.refresh_timer -= real_seconds;

			if (board.refresh_timer > 0.f)
			{
				continue;
			}

			// 받은 의뢰는 진행 목록에 따로 있어서 판을 갈아도 안 사라진다
			board.epoch += 1;
			FillBoard(static_cast<int32>(village));
		}
	}

	void GrantReward(const Quest& quest)
	{
		Player& player = MutablePlayer();
		const QuestReward& reward = quest.reward;

		if (reward.gold > 0)
		{
			AddPlayerGold(reward.gold);
		}

		if (reward.trace > 0 && GivePlayerItem(ITEM_ID_SPELL_TRACE, reward.trace) == false)
		{
			SpawnItemDrop(player.m_pos, ITEM_ID_SPELL_TRACE, reward.trace);
		}

		if (reward.item_id != ITEM_ID_NONE && GivePlayerItem(reward.item_id, 1) == false)
		{
			SpawnItemDrop(player.m_pos, reward.item_id, 1);
		}

		AddDamageText(player.m_pos, NextBannerLift(), L"의뢰 완료  " + quest.title + L"   +" + std::to_wstring(reward.gold) + L" 골드  +" + std::to_wstring(reward.exp) + L" 경험치", DAMAGE_STYLE_BANNER_GOOD);

		// 레벨업 연출이 의뢰 완료 글자 뒤에 오게 경험치를 마지막에 준다
		if (reward.exp > 0)
		{
			player.AddExp(reward.exp);
		}
	}

	void RefreshPath(float real_seconds)
	{
		g_path_timer -= real_seconds;

		const int32 tracked = GetTrackedQuest();
		WorldPos guide;
		if (tracked < 0 || GetQuestGuide(g_active[static_cast<size_t>(tracked)], &guide) == false || IsPlayerDead())
		{
			g_path.clear();
			g_path_to_x = -9999;
			return;
		}

		int32 from_x = 0;
		int32 from_y = 0;
		int32 to_x = 0;
		int32 to_y = 0;
		if (WorldToZoneTile(GetPlayer().m_pos, &from_x, &from_y) == false || WorldToZoneTile(guide, &to_x, &to_y) == false)
		{
			g_path.clear();
			return;
		}

		const bool moved = std::abs(from_x - g_path_from_x) + std::abs(from_y - g_path_from_y) >= 2;
		const bool new_goal = to_x != g_path_to_x || to_y != g_path_to_y;
		if ((moved == false || g_path_timer > 0.f) && new_goal == false)
		{
			return;
		}

		g_path_timer = PATH_REFRESH_SEC;
		g_path_from_x = from_x;
		g_path_from_y = from_y;
		g_path_to_x = to_x;
		g_path_to_y = to_y;

		ZoneTile start;
		start.x = from_x;
		start.y = from_y;
		ZoneTile goal;
		goal.x = to_x;
		goal.y = to_y;

		// 탐색 상한에 걸려도 가장 가까이 간 데까지는 돌려준다. 안내는 그걸로 충분하다
		std::vector<ZoneTile> tiles;
		FindNavPath(start, goal, &tiles);

		g_path.clear();
		for (size_t index = 0; index < tiles.size() && index < static_cast<size_t>(PATH_DRAW_STEPS); ++index)
		{
			g_path.push_back(ZoneTileToWorld(tiles[index].x, tiles[index].y, 0.5f, 0.5f));
		}
	}

	bool IsOffScreen(const ScreenPos& screen, float margin)
	{
		const float limit_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f + margin;
		const float limit_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f + margin;

		return std::fabs(screen.x) > limit_x || std::fabs(screen.y) > limit_y;
	}
}

void BuildQuests(const ZoneLayout& layout)
{
	g_sites = layout.sites;
	g_boss_pos = ZoneTileToWorld(layout.boss.x, layout.boss.y, 0.5f, 0.5f);
	g_seed = layout.seed;
	g_active.clear();
	g_tracked_serial = -1;
	g_path.clear();
	g_has_exam = false;

	g_boards.assign(static_cast<size_t>(GetVillageCount()), BoardState());
	for (int32 village = 0; village < GetVillageCount(); ++village)
	{
		FillBoard(village);
	}

	UpdateExamOffer();

	std::cout << "[ Quest - BuildQuests ] : boards " << g_boards.size() << " offers " << g_boards.front().offers.size() << "\n";
}

void UpdateQuests(float real_seconds)
{
	g_time += real_seconds;

	UpdateBoards(real_seconds);
	UpdateExamOffer();
	UpdateLiveProgress();
	RefreshPath(real_seconds);
}

void NotifyQuestKill(int32 kind, bool is_elite, bool is_boss, int32 level)
{
	for (Quest& quest : g_active)
	{
		if (quest.state != QUEST_STATE_ACTIVE)
		{
			continue;
		}

		bool counts = false;
		if (quest.goal == QUEST_GOAL_HUNT && quest.target == kind)
		{
			counts = true;
		}
		else if (quest.goal == QUEST_GOAL_ELITE && is_elite && level >= quest.min_level)
		{
			counts = true;
		}
		else if (quest.goal == QUEST_GOAL_BOSS && is_boss && quest.target == kind)
		{
			counts = true;
		}

		if (counts == false)
		{
			continue;
		}

		quest.progress = std::min(quest.progress + 1, quest.need);
		AddDamageText(GetPlayer().m_pos, NextBannerLift() - 30.f, DescribeQuestGoal(quest, true), DAMAGE_STYLE_INFO);

		if (quest.progress >= quest.need)
		{
			CompleteGoal(&quest);
		}
	}
}

const std::vector<Quest>& GetBoardQuests(int32 village)
{
	if (village < 0 || village >= static_cast<int32>(g_boards.size()))
	{
		return EMPTY_QUESTS;
	}

	return g_boards[static_cast<size_t>(village)].offers;
}

int32 AcceptBoardQuest(int32 village, int32 index)
{
	if (village < 0 || village >= static_cast<int32>(g_boards.size()))
	{
		return QUEST_ACCEPT_TAKEN;
	}

	std::vector<Quest>& offers = g_boards[static_cast<size_t>(village)].offers;
	if (index < 0 || index >= static_cast<int32>(offers.size()))
	{
		return QUEST_ACCEPT_TAKEN;
	}

	Quest& offer = offers[static_cast<size_t>(index)];
	if (offer.state != QUEST_STATE_OFFERED)
	{
		return QUEST_ACCEPT_TAKEN;
	}

	if (offer.is_exam == false && CanAcceptQuestTier(offer.tier) == false)
	{
		return QUEST_ACCEPT_RANK;
	}

	if (static_cast<int32>(g_active.size()) >= GetQuestConfig().max_active)
	{
		return QUEST_ACCEPT_FULL;
	}

	MarkOffer(offer.serial, QUEST_STATE_ACTIVE);

	Quest quest = offer;
	quest.progress = 0;
	g_active.push_back(quest);

	// 막 받은 의뢰로 길 안내를 옮긴다. 받자마자 어디로 가는지 보여야 한다.
	// 납품처럼 갈 곳이 없는 의뢰는 안내하던 곳을 빼앗지 않는다
	const int32 tracked = GetTrackedQuest();
	const bool tracked_has_place = tracked >= 0 && g_active[static_cast<size_t>(tracked)].has_place;
	if (quest.has_place || tracked_has_place == false)
	{
		g_tracked_serial = quest.serial;
	}
	UpdateLiveProgress();

	std::cout << "[ Quest - AcceptBoardQuest ] : village " << village << " serial " << quest.serial << " goal " << quest.goal << " need " << quest.need << "\n";
	return QUEST_ACCEPT_OK;
}

int32 ReportQuestsAt(int32 village)
{
	int32 reported = 0;

	for (size_t index = 0; index < g_active.size();)
	{
		Quest& quest = g_active[index];
		if (quest.state != QUEST_STATE_DONE)
		{
			++index;
			continue;
		}

		if (quest.goal == QUEST_GOAL_DELIVER)
		{
			// 맡기는 순간 가방에서 빠진다. 모자라면 아직 못 넘긴다
			if (MutablePlayer().m_inventory.RemoveItem(quest.target, quest.need) == false)
			{
				quest.state = QUEST_STATE_ACTIVE;
				++index;
				continue;
			}
		}

		GrantReward(quest);
		MarkOffer(quest.serial, QUEST_STATE_REPORTED);

		if (quest.is_exam)
		{
			PromotePlayerRank();
		}
		else
		{
			AddRankPoints(GetRankConfig().tier_points[static_cast<size_t>(quest.tier)]);
		}

		std::cout << "[ Quest - ReportQuestsAt ] : village " << village << " serial " << quest.serial << " gold " << quest.reward.gold << " exp " << quest.reward.exp << "\n";

		g_active.erase(g_active.begin() + static_cast<std::ptrdiff_t>(index));
		reported += 1;
	}

	if (reported > 0)
	{
		PlaySfx(SFX_ENHANCE_SUCCESS, 0.9f);
		UpdateExamOffer();
	}

	return reported;
}

float GetBoardRefreshLeft(int32 village)
{
	if (village < 0 || village >= static_cast<int32>(g_boards.size()))
	{
		return 0.f;
	}

	return g_boards[static_cast<size_t>(village)].refresh_timer;
}

const std::vector<Quest>& GetActiveQuests()
{
	return g_active;
}

int32 GetTrackedQuest()
{
	for (size_t index = 0; index < g_active.size(); ++index)
	{
		if (g_active[index].serial == g_tracked_serial)
		{
			return static_cast<int32>(index);
		}
	}

	if (g_active.empty())
	{
		return -1;
	}

	// 안내하던 의뢰를 넘겼으면 맨 앞 것으로
	g_tracked_serial = g_active.front().serial;
	return 0;
}

void CycleTrackedQuest()
{
	if (g_active.empty())
	{
		return;
	}

	const int32 now = GetTrackedQuest();
	const int32 next = (now + 1) % static_cast<int32>(g_active.size());
	g_tracked_serial = g_active[static_cast<size_t>(next)].serial;
	g_path_to_x = -9999;
	PlaySfx(SFX_UI_CLICK, 0.5f);
}

std::wstring DescribeQuestGoal(const Quest& quest, bool with_progress)
{
	std::wstring text;

	switch (quest.goal)
	{
		case QUEST_GOAL_HUNT:
		case QUEST_GOAL_BOSS:
		{
			text = std::wstring(GetMonsterKindName(quest.target)) + L" 처치";
			break;
		}
		case QUEST_GOAL_ELITE:
		{
			text = L"정예 처치";
			if (quest.min_level > 0)
			{
				text = L"Lv " + std::to_wstring(quest.min_level) + L" 이상 정예 처치";
			}

			break;
		}
		case QUEST_GOAL_DELIVER:
		{
			text = GetItemData(quest.target).name + L" 납품";
			break;
		}
		default:
		{
			return std::wstring(GetSiteKindName(quest.target)) + L" 확인";
		}
	}

	if (with_progress)
	{
		return text + L"  " + std::to_wstring(quest.progress) + L" / " + std::to_wstring(quest.need);
	}

	return text + L"  " + std::to_wstring(quest.need);
}

std::wstring GetQuestTagName(const Quest& quest)
{
	if (quest.is_exam)
	{
		return L"승급";
	}

	return GetQuestTierName(quest.tier);
}

std::wstring GetQuestTierName(int32 tier)
{
	if (tier < 0 || tier >= QUEST_TIER_COUNT)
	{
		return L"";
	}

	return GetQuestConfig().tier_names[static_cast<size_t>(tier)];
}

void GetQuestTierColor(int32 tier, float* r, float* g, float* b)
{
	switch (tier)
	{
		case QUEST_TIER_NORMAL:
		{
			*r = 0.5f;
			*g = 0.72f;
			*b = 1.f;
			return;
		}
		case QUEST_TIER_HARD:
		{
			*r = 1.f;
			*g = 0.62f;
			*b = 0.25f;
			return;
		}
		case QUEST_TIER_TRIAL:
		{
			*r = 1.f;
			*g = 0.32f;
			*b = 0.3f;
			return;
		}
		default:
		{
			*r = 0.62f;
			*g = 0.86f;
			*b = 0.58f;
			return;
		}
	}
}

bool GetQuestGuide(const Quest& quest, WorldPos* pos)
{
	if (quest.state == QUEST_STATE_DONE)
	{
		const int32 village = FindNearestVillage(GetPlayer().m_pos);
		if (village < 0)
		{
			return false;
		}

		*pos = GetBoardPos(village);
		return true;
	}

	if (quest.has_place == false)
	{
		return false;
	}

	*pos = quest.place;
	return true;
}

float GetPlayerDistanceTo(const WorldPos& pos)
{
	return GetDistance(GetPlayer().m_pos, pos);
}

void DrawQuestGround(Renderer* renderer, const WorldPos& camera)
{
	if (g_path.empty())
	{
		return;
	}

	// 발밑에서 멀어질수록 옅어지고, 빛 알갱이가 목적지 쪽으로 흘러간다
	const float flow = g_time * 7.f;
	const size_t count = g_path.size();

	for (size_t index = 1; index < count; index += 2)
	{
		const ScreenPos screen = WorldToScreen(g_path[index], camera);
		if (IsOffScreen(screen, 40.f))
		{
			continue;
		}

		// 밝은 광장 돌바닥에서도 읽히게 어두운 테를 먼저 깐다.
		// 씬은 3 분의 1 해상도라 0.2 칸 마름모는 도트 몇 개로 깎여 안 보인다
		const float fade = 1.f - static_cast<float>(index) / static_cast<float>(PATH_DRAW_STEPS);
		const float wave = std::fmod(static_cast<float>(index) - flow, 10.f);
		float pulse = 0.f;
		if (wave > -2.f && wave < 0.f)
		{
			pulse = 0.4f;
		}

		const float size = 0.42f + pulse * 0.16f;
		renderer->DrawSolidDiamond(screen.x, screen.y, size + 0.14f, 0.08f, 0.05f, 0.02f, 0.5f * fade);
		renderer->DrawSolidDiamond(screen.x, screen.y, size, 1.f + pulse, 0.82f + pulse * 0.5f, 0.38f, (0.85f + pulse) * fade);
	}

	const int32 tracked = GetTrackedQuest();
	WorldPos guide;
	if (tracked < 0 || GetQuestGuide(g_active[static_cast<size_t>(tracked)], &guide) == false)
	{
		return;
	}

	const ScreenPos target = WorldToScreen(guide, camera);
	if (IsOffScreen(target, 60.f))
	{
		return;
	}

	const float breath = 0.5f + 0.5f * std::sin(g_time * 4.f);
	renderer->DrawSolidDiamond(target.x, target.y, 1.4f + breath * 0.2f, 1.f, 0.78f, 0.3f, 0.12f + breath * 0.08f);
	renderer->DrawSolidDiamond(target.x, target.y, 0.5f, 1.f, 0.86f, 0.45f, 0.35f);
}

void DrawQuestOverlay(Renderer* renderer, const WorldPos& camera)
{
	const int32 tracked = GetTrackedQuest();
	WorldPos guide;
	if (tracked < 0 || IsPlayerDead() || GetQuestGuide(g_active[static_cast<size_t>(tracked)], &guide) == false)
	{
		return;
	}

	const ScreenPos screen = WorldToScreen(guide, camera);
	const int32 tiles = static_cast<int32>(GetPlayerDistanceTo(guide));

	// 화면 안이면 목적지 위에 거리만
	if (IsOffScreen(screen, -GUIDE_MARGIN) == false)
	{
		DrawStringShadow(renderer, std::to_wstring(tiles) + L"칸", screen.x, screen.y + 46.f, TEXT_ALIGN_CENTER, 1.f, 0.85f, 0.45f, 0.9f);
		return;
	}

	const float limit_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f - GUIDE_MARGIN;
	float limit_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - GUIDE_MARGIN;

	// 아래쪽은 체력 · 행동 칸 · 지갑이 차지한다. 그 위에서 멈춘다
	if (screen.y < 0.f)
	{
		limit_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - 150.f;
	}

	float scale = limit_y / std::max(std::fabs(screen.y), 1.f);
	if (std::fabs(screen.x) * limit_y > std::fabs(screen.y) * limit_x)
	{
		scale = limit_x / std::max(std::fabs(screen.x), 1.f);
	}

	const float marker_x = screen.x * scale;
	const float marker_y = screen.y * scale;
	const float length = std::sqrt(screen.x * screen.x + screen.y * screen.y);
	const float dir_x = screen.x / std::max(length, 1.f);
	const float dir_y = screen.y / std::max(length, 1.f);

	// 바깥쪽을 가리키는 꺾쇠
	const float tip_x = marker_x + dir_x * 16.f;
	const float tip_y = marker_y + dir_y * 16.f;
	const float side_x = -dir_y;
	const float side_y = dir_x;

	renderer->DrawSolidCircle(marker_x, marker_y, 11.f, 0.06f, 0.04f, 0.02f, 0.85f);
	renderer->DrawSolidCircle(marker_x, marker_y, 8.f, 1.f, 0.8f, 0.35f, 1.f);
	DrawLine(renderer, tip_x, tip_y, tip_x - dir_x * 9.f + side_x * 7.f, tip_y - dir_y * 9.f + side_y * 7.f, 3.f, 1.f, 0.8f, 0.35f, 1.f);
	DrawLine(renderer, tip_x, tip_y, tip_x - dir_x * 9.f - side_x * 7.f, tip_y - dir_y * 9.f - side_y * 7.f, 3.f, 1.f, 0.8f, 0.35f, 1.f);
	DrawStringShadow(renderer, std::to_wstring(tiles) + L"칸", marker_x - dir_x * 48.f, marker_y - dir_y * 26.f + 12.f, TEXT_ALIGN_CENTER, 1.f, 0.85f, 0.45f, 1.f);
}

void DrawQuestTracker(Renderer* renderer, bool hidden)
{
	if (hidden || g_active.empty())
	{
		return;
	}

	const float right = static_cast<float>(WINDOW_SIZE_X) * 0.5f - 16.f;
	const float left = right - TRACKER_WIDTH;
	float y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - 20.f;
	const int32 tracked = GetTrackedQuest();

	const float height = 34.f + TRACKER_ROW * static_cast<float>(g_active.size());
	renderer->DrawSolidBox(left + TRACKER_WIDTH * 0.5f, y - height * 0.5f + 6.f, TRACKER_WIDTH + 16.f, height, 0.02f, 0.018f, 0.015f, 0.55f);

	DrawStringShadow(renderer, L"의뢰  " + std::to_wstring(g_active.size()) + L" / " + std::to_wstring(GetQuestConfig().max_active) + L"     T  안내 바꾸기", left, y, TEXT_ALIGN_LEFT, 0.7f, 0.64f, 0.54f, 1.f);
	y -= 34.f;

	for (size_t index = 0; index < g_active.size(); ++index)
	{
		const Quest& quest = g_active[index];
		const bool done = quest.state == QUEST_STATE_DONE;

		float tier_r = 0.f;
		float tier_g = 0.f;
		float tier_b = 0.f;
		GetQuestTierColor(quest.tier, &tier_r, &tier_g, &tier_b);

		float title_r = 0.95f;
		float title_g = 0.9f;
		float title_b = 0.8f;

		// 끝낸 의뢰는 흑백으로 눌러 두고 줄을 긋는다. 남은 일과 한눈에 갈린다
		if (done)
		{
			tier_r = 0.5f;
			tier_g = 0.5f;
			tier_b = 0.5f;
			title_r = 0.55f;
			title_g = 0.55f;
			title_b = 0.55f;
		}

		const float text_left = left + 14.f;
		if (static_cast<int32>(index) == tracked)
		{
			const float breath = 0.6f + 0.4f * std::sin(g_time * 4.f);
			renderer->DrawSolidDiamond(left + 2.f, y - 12.f, 0.22f, 1.f, 0.82f * breath, 0.35f, 1.f);
		}

		const std::wstring tag = L"[" + GetQuestTagName(quest) + L"] ";
		DrawStringShadow(renderer, tag, text_left, y, TEXT_ALIGN_LEFT, tier_r, tier_g, tier_b, 1.f);

		const float title_x = text_left + MeasureString(tag);
		DrawStringShadow(renderer, quest.title, title_x, y, TEXT_ALIGN_LEFT, title_r, title_g, title_b, 1.f);

		std::wstring detail = DescribeQuestGoal(quest, true);
		float detail_r = 0.78f;
		float detail_g = 0.74f;
		float detail_b = 0.66f;

		if (done)
		{
			renderer->DrawSolidBox(title_x + MeasureString(quest.title) * 0.5f, y - 12.f, MeasureString(quest.title) + 6.f, 2.f, 0.62f, 0.62f, 0.62f, 1.f);
			detail = L"[완료]  게시판에 보고";
			detail_r = 0.6f;
			detail_g = 0.6f;
			detail_b = 0.6f;
		}

		WorldPos guide;
		if (GetQuestGuide(quest, &guide))
		{
			detail += L"  ·  " + std::to_wstring(static_cast<int32>(GetPlayerDistanceTo(guide))) + L"칸";
		}

		DrawStringShadow(renderer, detail, text_left + 10.f, y - 24.f, TEXT_ALIGN_LEFT, detail_r, detail_g, detail_b, 1.f);
		y -= TRACKER_ROW;
	}
}
