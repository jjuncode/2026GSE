#include "stdafx.h"
#include "NpcBrain.h"
#include "Monster.h"
#include "NavGrid.h"
#include "Player.h"
#include "ZoneGen.h"

#include <array>
#include <cmath>
#include <string>

namespace
{
	constexpr float SENSE_INTERVAL = 0.3f;
	constexpr float SENSE_RANGE = 8.f;
	constexpr float STANCE_REROLL_SEC = 2.5f;
	constexpr float THREAT_RANGE = 10.f;
	constexpr float FLEE_SAFE_DISTANCE = 14.f;
	constexpr float FLEE_LIMIT_SEC = 12.f;
	constexpr float AVOID_LIMIT_SEC = 2.5f;
	constexpr float FIGHT_GIVE_UP_HEALTH = 0.22f;	// 이 아래로 떨어지면 맞서기를 접고 다시 굴린다
	constexpr float HUNT_SEARCH_RANGE = 45.f;
	constexpr float RAID_SEARCH_RANGE = 55.f;
	constexpr float PLAYER_RAID_RANGE = 30.f;
	constexpr float RESOURCE_SEARCH_RANGE = 60.f;
	constexpr float RESOURCE_MONSTER_CLEARANCE = 9.f;
	constexpr float CORPSE_SEARCH_RANGE = 50.f;
	constexpr float STALL_SEARCH_RANGE = 45.f;
	constexpr float RAID_LOSE_DISTANCE = 14.f;
	constexpr float SITE_ARRIVE = 2.2f;
	constexpr float NPC_ARRIVE = 1.4f;
	constexpr float ITEM_ARRIVE = 0.7f;
	constexpr float SPOT_ARRIVE = 0.5f;
	constexpr float TASK_TIMEOUT_SEC = 70.f;	// 한 일과가 이보다 길면 막힌 것으로 치고 놓는다
	constexpr float MERCHANT_RATE = 0.8f;	// 행상은 시장보다 짐값을 더 쳐 준다
	constexpr float MARKET_RATE = 0.6f;
	constexpr float FENCE_RATE = 0.5f;	// 장물아비
	constexpr int32 HIDEOUT_FOOD_PRICE = 3;
	constexpr int32 OUTPOST_FOOD_PRICE = 6;	// 벽 밖으로 실어 나른 값
	constexpr int32 WARDEN_WAGE = 8;

	BehaviorTree<Npc> g_tree;
	int32 g_daily_node = -1;
	bool g_built = false;

	// 직업 x 일과 기본 가중치. 직업은 할 수 있는 일의 윤곽만 정한다.
	// 순서: 사냥 약탈 거래 채집 행상 순찰 식사 휴식 정비 뒤지기 배회
	const std::array<std::array<float, NPC_DAILY_TASK_COUNT>, NPC_JOB_COUNT> JOB_BASE =
	{ {
		{ { 1.2f, 0.f, 1.f, 0.25f, 0.f, 0.f, 1.f, 1.f, 0.6f, 0.15f, 0.3f } },
		{ { 0.25f, 1.3f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.6f, 0.9f, 0.35f } },
		{ { 0.f, 0.f, 0.f, 0.f, 1.5f, 0.f, 1.f, 1.f, 0.3f, 0.f, 0.2f } },
		{ { 0.f, 0.f, 1.f, 1.3f, 0.f, 0.f, 1.f, 1.f, 0.4f, 0.35f, 0.3f } },
		{ { 0.5f, 0.f, 0.5f, 0.f, 0.f, 1.3f, 1.f, 1.f, 0.6f, 0.f, 0.25f } },
	} };

	float Clamp01(float value)
	{
		if (value < 0.f)
		{
			return 0.f;
		}

		if (value > 1.f)
		{
			return 1.f;
		}

		return value;
	}

	float HealthRatio(const Npc& npc)
	{
		return Clamp01(static_cast<float>(npc.hp) / static_cast<float>(npc.max_hp));
	}

	float LoadRatio(const Npc& npc)
	{
		if (npc.goods_capacity <= 0)
		{
			return 1.f;
		}

		return Clamp01(static_cast<float>(npc.goods) / static_cast<float>(npc.goods_capacity));
	}

	float Base(const Npc& npc, int32 task)
	{
		return JOB_BASE[static_cast<size_t>(npc.job)][static_cast<size_t>(task)];
	}

	float Roll(Npc* npc)
	{
		return BehaviorTree<Npc>::NextUnit(&npc->brain);
	}

	float RollRange(Npc* npc, float min_value, float max_value)
	{
		return min_value + (max_value - min_value) * Roll(npc);
	}

	std::vector<Npc>& Npcs()
	{
		return GetNpcList();
	}

	bool IsLiveNpc(int32 index)
	{
		if (index < 0 || index >= static_cast<int32>(Npcs().size()))
		{
			return false;
		}

		return Npcs()[static_cast<size_t>(index)].is_alive;
	}

	Npc& NpcAt(int32 index)
	{
		return Npcs()[static_cast<size_t>(index)];
	}

	int32 FindNearestSite(const Npc& npc, int32 kind, int32 other_kind)
	{
		const std::vector<ZoneSite>& sites = GetNpcSites();
		int32 best = -1;
		float best_distance = 1.0e9f;

		for (size_t index = 0; index < sites.size(); ++index)
		{
			if (sites[index].kind != kind && sites[index].kind != other_kind)
			{
				continue;
			}

			const float distance = GetNpcDistance(npc, GetSitePos(static_cast<int32>(index)));

			if (distance < best_distance)
			{
				best_distance = distance;
				best = static_cast<int32>(index);
			}
		}

		return best;
	}

	// 무법자는 벽 안이나 초소로 못 간다. 소굴이 곧 집이다
	int32 FindShelter(const Npc& npc)
	{
		if (IsNpcOutlaw(npc))
		{
			const int32 hideout = FindNearestSite(npc, SITE_HIDEOUT, -1);
			if (hideout >= 0)
			{
				return hideout;
			}
		}

		return FindNearestSite(npc, SITE_TOWN, SITE_OUTPOST);
	}

	bool IsStallOpen(const Npc& merchant)
	{
		return merchant.is_alive && merchant.job == NPC_JOB_MERCHANT && merchant.stall_heartbeat > 0.f;
	}

	int32 FindOpenStall(const Npc& npc, int32 min_gold, float* distance)
	{
		int32 best = -1;
		float best_distance = STALL_SEARCH_RANGE;

		for (size_t index = 0; index < Npcs().size(); ++index)
		{
			const Npc& merchant = Npcs()[index];

			if (&merchant == &npc || IsStallOpen(merchant) == false || merchant.gold < min_gold)
			{
				continue;
			}

			const float merchant_distance = GetNpcDistance(npc, merchant.body.m_pos);

			if (merchant_distance < best_distance)
			{
				best_distance = merchant_distance;
				best = static_cast<int32>(index);
			}
		}

		*distance = best_distance;

		return best;
	}

	// 채집 자리나 주머니를 이미 누가 가지러 가는 중이면 양보한다
	bool IsClaimedByOther(const Npc& self, int32 kind, int32 index)
	{
		for (const Npc& other : Npcs())
		{
			if (&other == &self || other.is_alive == false)
			{
				continue;
			}

			if (other.target_kind == kind && other.target_index == index)
			{
				if (other.task == NPC_TASK_GATHER || other.task == NPC_TASK_SCAVENGE)
				{
					return true;
				}
			}
		}

		return false;
	}

	void SetTarget(Npc* npc, int32 kind, int32 index)
	{
		npc->target_kind = kind;
		npc->target_index = index;
	}

	bool GetGoalPos(const Npc& npc, WorldPos* pos)
	{
		if (npc.target_kind == NPC_TARGET_SPOT)
		{
			*pos = npc.spot;
			return true;
		}

		return GetNpcTargetPos(npc.target_kind, npc.target_index, pos);
	}

	float GetArriveRadius(int32 kind)
	{
		switch (kind)
		{
			case NPC_TARGET_SITE:
			{
				return SITE_ARRIVE;
			}
			case NPC_TARGET_NPC:
			case NPC_TARGET_PLAYER:
			{
				return NPC_ARRIVE;
			}
			case NPC_TARGET_SPOT:
			{
				return SPOT_ARRIVE;
			}
			default:
			{
				return ITEM_ARRIVE;
			}
		}
	}

	int32 GetSiteKind(int32 site)
	{
		return GetNpcSites()[static_cast<size_t>(site)].kind;
	}

	int32 GetFoodPriceAt(int32 site_kind)
	{
		if (site_kind == SITE_HIDEOUT)
		{
			return HIDEOUT_FOOD_PRICE;
		}

		if (site_kind == SITE_OUTPOST)
		{
			return OUTPOST_FOOD_PRICE;
		}

		return NPC_FOOD_PRICE;
	}

	int32 GetCheapestFood(const Npc& npc)
	{
		if (IsNpcOutlaw(npc))
		{
			return HIDEOUT_FOOD_PRICE;
		}

		return NPC_FOOD_PRICE;
	}

	int32 GetPower(const Npc& npc)
	{
		return npc.attack * npc.hp;
	}

	int32 GetTargetPower(int32 kind, int32 index)
	{
		if (kind == NPC_TARGET_MONSTER)
		{
			return GetMonsterThreat(index);
		}

		if (kind == NPC_TARGET_NPC && IsLiveNpc(index))
		{
			return GetPower(NpcAt(index));
		}

		if (kind == NPC_TARGET_PLAYER)
		{
			return GetPlayerAttack() * GetPlayer().m_hp;
		}

		return 0;
	}

	// 내 힘 / ( 내 힘 + 상대 힘 ). 이게 없으면 사냥꾼이 소거식귀에게 활로 맞선다
	float GetOdds(const Npc& npc, int32 kind, int32 index)
	{
		const float mine = static_cast<float>(GetPower(npc));
		const float theirs = static_cast<float>(GetTargetPower(kind, index));

		return mine / (mine + theirs + 1.f);
	}

	// ------------------------------------------------------------ 일과 가중치
	// 가중치 = 직업 기본값 x 성향 x 지금 형편. 표적이 있는지는 여기서 안 본다 ( 뽑은 뒤 한 번만 찾는다 )

	// 굶는데 돈이 없으면 약탈자가 아니어도 약탈 가중치가 생긴다
	float Desperation(const Npc& npc)
	{
		if (npc.job == NPC_JOB_RAIDER)
		{
			return 0.f;
		}

		if (npc.hunger < 0.7f || npc.gold >= NPC_FOOD_PRICE)
		{
			return 0.f;
		}

		float amount = (npc.hunger - 0.6f) * 2.25f;

		// 행상은 칼을 못 쓰고, 순찰병은 제복이 붙잡는다
		if (npc.job == NPC_JOB_WARDEN)
		{
			amount *= 0.3f;
		}
		else if (npc.job == NPC_JOB_MERCHANT)
		{
			amount *= 0.1f;
		}

		return amount;
	}

	float WeightHunt(const Npc& npc)
	{
		const float health = HealthRatio(npc);

		return Base(npc, NPC_TASK_HUNT) * npc.bravery * health * health * (1.f - LoadRatio(npc));
	}

	float WeightRaid(const Npc& npc)
	{
		const float health = HealthRatio(npc);
		const float base = Base(npc, NPC_TASK_RAID) + Desperation(npc) * npc.cruelty;

		return base * npc.cruelty * npc.greed * health * health * (1.f - LoadRatio(npc) * 0.5f);
	}

	float WeightSell(const Npc& npc)
	{
		if (npc.goods <= 0)
		{
			return 0.f;
		}

		const float load = LoadRatio(npc);
		float weight = Base(npc, NPC_TASK_SELL) * (load * load * 3.f + 0.05f);

		// 짐이 다 차면 다른 일이 전부 헛걸음이다
		if (load >= 0.95f)
		{
			weight *= 2.f;
		}

		return weight;
	}

	float WeightGather(const Npc& npc)
	{
		return Base(npc, NPC_TASK_GATHER) * npc.diligence * (1.f - LoadRatio(npc));
	}

	float WeightPeddle(const Npc& npc)
	{
		return Base(npc, NPC_TASK_PEDDLE) * npc.diligence;
	}

	float WeightPatrol(const Npc& npc)
	{
		return Base(npc, NPC_TASK_PATROL) * npc.diligence * (0.4f + HealthRatio(npc));
	}

	float WeightEat(const Npc& npc)
	{
		float weight = Base(npc, NPC_TASK_EAT) * npc.hunger * npc.hunger * 4.f;

		if (npc.gold < GetCheapestFood(npc))
		{
			weight *= 0.05f;
		}

		return weight;
	}

	float WeightRest(const Npc& npc)
	{
		const float missing = 1.f - HealthRatio(npc);

		return Base(npc, NPC_TASK_REST) * (std::pow(missing, 1.5f) * 3.f + 0.02f);
	}

	float WeightUpgrade(const Npc& npc)
	{
		const float surplus = static_cast<float>(npc.gold) / static_cast<float>(GetNpcUpgradeCost(npc)) - 1.2f;

		if (surplus <= 0.f)
		{
			return 0.f;
		}

		float weight = Base(npc, NPC_TASK_UPGRADE) * surplus * 0.5f;
		if (weight > 1.5f)
		{
			weight = 1.5f;
		}

		return weight;
	}

	float WeightScavenge(const Npc& npc)
	{
		if (GetNpcCorpses().empty())
		{
			return 0.f;
		}

		return Base(npc, NPC_TASK_SCAVENGE) * npc.greed;
	}

	float WeightWander(const Npc& npc)
	{
		return Base(npc, NPC_TASK_WANDER) * (2.f - npc.diligence);
	}

	// ------------------------------------------------------------ 반응 가중치

	float GetJobFight(int32 job)
	{
		switch (job)
		{
			case NPC_JOB_WARDEN:
			{
				return 1.6f;
			}
			case NPC_JOB_RAIDER:
			{
				return 1.3f;
			}
			case NPC_JOB_HUNTER:
			{
				return 1.2f;
			}
			case NPC_JOB_MERCHANT:
			{
				return 0.25f;
			}
			default:
			{
				return 0.3f;
			}
		}
	}

	float GetJobFlee(int32 job)
	{
		if (job == NPC_JOB_MERCHANT || job == NPC_JOB_GATHERER)
		{
			return 1.5f;
		}

		return 0.7f;
	}

	// 반반이면 1 배. 상대가 셀수록 맞서기는 줄고 달아나기는 는다
	float WeightFightBack(const Npc& npc)
	{
		const float odds = GetOdds(npc, npc.threat_kind, npc.threat_index);

		return npc.bravery * HealthRatio(npc) * GetJobFight(npc.job) * odds * 2.f;
	}

	float WeightFleeThreat(const Npc& npc)
	{
		const float odds = GetOdds(npc, npc.threat_kind, npc.threat_index);

		return (2.f - npc.bravery) * (1.2f - HealthRatio(npc)) * GetJobFlee(npc.job) * (1.f - odds) * 2.f;
	}

	// ------------------------------------------------------------ 조건

	bool HasThreat(const Npc& npc)
	{
		if (npc.threat_timer <= 0.f)
		{
			return false;
		}

		WorldPos pos;
		if (GetNpcTargetPos(npc.threat_kind, npc.threat_index, &pos) == false)
		{
			return false;
		}

		return GetNpcDistance(npc, pos) < THREAT_RANGE;
	}

	bool IsEnemyInSight(const Npc& npc)
	{
		WorldPos pos;
		if (GetNpcTargetPos(npc.enemy_kind, npc.enemy_index, &pos) == false)
		{
			return false;
		}

		return GetNpcDistance(npc, pos) <= SENSE_RANGE + 1.f;
	}

	bool WantsEngage(const Npc& npc)
	{
		return npc.stance == NPC_STANCE_ENGAGE && IsEnemyInSight(npc);
	}

	bool WantsAvoid(const Npc& npc)
	{
		return npc.stance == NPC_STANCE_AVOID && IsEnemyInSight(npc);
	}

	// ------------------------------------------------------------ 반응 잎

	int32 ActFightBack(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			npc->task = NPC_TASK_FIGHT;
			ResetNpcPath(npc);
		}

		// 버티다 몰리면 접는다. 가중 노드가 남은 선택지 ( 도주 ) 로 다시 굴린다
		if (HealthRatio(*npc) < FIGHT_GIVE_UP_HEALTH && npc->job != NPC_JOB_WARDEN)
		{
			return BT_FAILURE;
		}

		const int32 result = NpcEngage(npc, npc->threat_kind, npc->threat_index, false, delta_seconds);

		if (result != BT_RUNNING)
		{
			npc->threat_timer = 0.f;
		}

		return result;
	}

	int32 ActFleeThreat(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			npc->task = NPC_TASK_FLEE;
			npc->task_timer = 0.f;
			ResetNpcPath(npc);
			NpcShowText(*npc, L"살려줘!", 1.f, 0.85f, 0.55f);
		}

		npc->task_timer += delta_seconds;

		WorldPos threat_pos;
		if (GetNpcTargetPos(npc->threat_kind, npc->threat_index, &threat_pos) == false)
		{
			npc->threat_timer = 0.f;
			return BT_SUCCESS;
		}

		if (GetNpcDistance(*npc, threat_pos) > FLEE_SAFE_DISTANCE || npc->task_timer > FLEE_LIMIT_SEC)
		{
			npc->threat_timer = 0.f;
			return BT_SUCCESS;
		}

		// 쉴 곳이 위협 반대편이면 그리로, 아니면 그냥 등을 돌려 뛴다. 거점으로 가는 길이 적을 지나면 안 된다
		const int32 shelter = FindShelter(*npc);
		if (shelter >= 0)
		{
			const WorldPos shelter_pos = GetSitePos(shelter);
			const float to_shelter_x = GetTileDistanceX(npc->body.m_pos, shelter_pos);
			const float to_shelter_y = GetTileDistanceY(npc->body.m_pos, shelter_pos);
			const float to_threat_x = GetTileDistanceX(npc->body.m_pos, threat_pos);
			const float to_threat_y = GetTileDistanceY(npc->body.m_pos, threat_pos);
			const float dot = to_shelter_x * to_threat_x + to_shelter_y * to_threat_y;

			if (dot < 0.f)
			{
				if (NpcMoveTo(npc, shelter_pos, SITE_ARRIVE, delta_seconds) != BT_FAILURE)
				{
					return BT_RUNNING;
				}
			}
		}

		NpcMoveAway(npc, threat_pos, delta_seconds);

		return BT_RUNNING;
	}

	int32 ActEngageEnemy(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			npc->task = NPC_TASK_ENGAGE;
			ResetNpcPath(npc);
		}

		const int32 result = NpcEngage(npc, npc->enemy_kind, npc->enemy_index, false, delta_seconds);

		if (result != BT_RUNNING)
		{
			npc->stance = NPC_STANCE_IGNORE;
		}

		return result;
	}

	int32 ActAvoidEnemy(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			npc->task = NPC_TASK_AVOID;
			npc->task_timer = 0.f;
		}

		npc->task_timer += delta_seconds;

		WorldPos enemy_pos;
		if (GetNpcTargetPos(npc->enemy_kind, npc->enemy_index, &enemy_pos) == false || npc->task_timer > AVOID_LIMIT_SEC)
		{
			npc->stance = NPC_STANCE_IGNORE;
			return BT_SUCCESS;
		}

		NpcMoveAway(npc, enemy_pos, delta_seconds);

		return BT_RUNNING;
	}

	// ------------------------------------------------------------ 일과 공용 잎

	// 앞 잎이 적어 둔 표적으로 걸어간다. 표적이 움직이면 ( 행상 ) 따라간다
	int32 ActGoToTarget(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			ResetNpcPath(npc);
			npc->task_timer = 0.f;
		}

		npc->task_timer += delta_seconds;
		if (npc->task_timer > TASK_TIMEOUT_SEC)
		{
			return BT_FAILURE;
		}

		WorldPos goal;
		if (GetGoalPos(*npc, &goal) == false)
		{
			return BT_FAILURE;
		}

		return NpcMoveTo(npc, goal, GetArriveRadius(npc->target_kind), delta_seconds);
	}

	// 정해 둔 시간만큼 머문다
	bool Linger(Npc* npc, bool is_enter, float delta_seconds, float min_sec, float max_sec)
	{
		if (is_enter)
		{
			npc->task_timer = 0.f;
			npc->task_limit = RollRange(npc, min_sec, max_sec);
		}

		npc->task_timer += delta_seconds;

		return npc->task_timer >= npc->task_limit;
	}

	void SetHomeIfSite(Npc* npc)
	{
		if (npc->target_kind == NPC_TARGET_SITE && GetSiteKind(npc->target_index) != SITE_GATE)
		{
			npc->home_site = npc->target_index;
		}
	}

	// ------------------------------------------------------------ 사냥

	int32 ActPickPrey(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_HUNT;

		const int32 my_power = GetPower(*npc);
		int32 best = -1;
		float best_score = 1.0e9f;

		for (int32 index = 0; index < GetMonsterCount(); ++index)
		{
			if (IsMonsterAlive(index) == false || GetMonsterKind(index) == MONSTER_GATEBREAKER)
			{
				continue;
			}

			const float distance = GetNpcDistance(*npc, GetMonsterPos(index));
			if (distance > HUNT_SEARCH_RANGE)
			{
				continue;
			}

			// 용감할수록 센 놈도 노린다
			const float risk = static_cast<float>(GetMonsterThreat(index)) / static_cast<float>(my_power + 1);
			if (risk > 2.5f * npc->bravery)
			{
				continue;
			}

			const float score = distance + risk * 8.f;

			if (score < best_score)
			{
				best_score = score;
				best = index;
			}
		}

		if (best < 0)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, NPC_TARGET_MONSTER, best);

		return BT_SUCCESS;
	}

	int32 ActHunt(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			npc->task_timer = 0.f;
			ResetNpcPath(npc);
		}

		npc->task_timer += delta_seconds;
		if (npc->task_timer > TASK_TIMEOUT_SEC)
		{
			return BT_FAILURE;
		}

		return NpcEngage(npc, NPC_TARGET_MONSTER, npc->target_index, false, delta_seconds);
	}

	// ------------------------------------------------------------ 약탈

	int32 CountRaidersOn(const Npc& self, int32 kind, int32 index)
	{
		int32 count = 0;

		for (const Npc& other : Npcs())
		{
			if (&other == &self || other.is_alive == false || other.task != NPC_TASK_RAID)
			{
				continue;
			}

			if (other.target_kind == kind && other.target_index == index)
			{
				count += 1;
			}
		}

		return count;
	}

	int32 ActPickVictim(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_RAID;
		npc->robbed_total = 0;

		const bool desperate = Desperation(*npc) > 0.f;
		const float my_power = static_cast<float>(GetPower(*npc));
		int32 best_kind = NPC_TARGET_NONE;
		int32 best_index = -1;
		float best_score = 2.f;

		for (size_t index = 0; index < Npcs().size(); ++index)
		{
			const Npc& victim = Npcs()[index];

			if (&victim == npc || victim.is_alive == false || IsNpcOutlaw(victim))
			{
				continue;
			}

			// 순찰병은 굶어 눈이 뒤집힌 게 아니면 안 건드린다
			if (victim.job == NPC_JOB_WARDEN && desperate == false)
			{
				continue;
			}

			// 벽 안은 수비대가 지킨다
			if (IsWorldPosInTown(victim.body.m_pos))
			{
				continue;
			}

			const float distance = GetNpcDistance(*npc, victim.body.m_pos);
			if (distance > RAID_SEARCH_RANGE)
			{
				continue;
			}

			const float loot = static_cast<float>(victim.gold) + static_cast<float>(victim.goods) * 0.5f;
			const float weakness = my_power / (my_power + static_cast<float>(GetPower(victim)) + 1.f);
			const float crowd = static_cast<float>(CountRaidersOn(*npc, NPC_TARGET_NPC, static_cast<int32>(index)));

			// 셋이 한 행상을 같이 털면 몇 대 만에 죽는다. 이미 노리는 자가 있으면 다른 먹잇감을 찾는다
			const float score = loot / (1.f + distance * 0.08f) * weakness / (1.f + crowd * 2.f);

			if (score > best_score)
			{
				best_score = score;
				best_kind = NPC_TARGET_NPC;
				best_index = static_cast<int32>(index);
			}
		}

		// 개척대원 주머니도 노린다. 세니까 점수를 깎는다
		const Player& player = GetPlayer();
		if (IsPlayerDead() == false && player.m_gold >= 15 && IsWorldPosInTown(player.m_pos) == false)
		{
			const float distance = GetNpcDistance(*npc, player.m_pos);

			if (distance < PLAYER_RAID_RANGE)
			{
				const float crowd = static_cast<float>(CountRaidersOn(*npc, NPC_TARGET_PLAYER, -1));
				const float score = static_cast<float>(player.m_gold) * 0.6f / (1.f + distance * 0.08f) * 0.4f / (1.f + crowd);

				if (score > best_score)
				{
					best_score = score;
					best_kind = NPC_TARGET_PLAYER;
					best_index = -1;
				}
			}
		}

		if (best_kind == NPC_TARGET_NONE)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, best_kind, best_index);

		if (best_kind == NPC_TARGET_PLAYER)
		{
			npc->prey_title = L"개척대원";
		}
		else
		{
			npc->prey_title = GetNpcTitle(NpcAt(best_index));
		}

		return BT_SUCCESS;
	}

	std::wstring DescribeVictim(const Npc& npc)
	{
		return WithObject(npc.prey_title);
	}

	void ReportRobbery(const Npc& npc)
	{
		if (npc.robbed_total <= 0)
		{
			return;
		}

		NoteNpcRobbery();
		PushNpcEvent(WithSubject(GetNpcTitle(npc)) + L" " + DescribeVictim(npc) + L" 덮쳐 " + std::to_wstring(npc.robbed_total) + L"골드어치를 빼앗았다", 1.f, 0.62f, 0.30f);
	}

	int32 ActAmbush(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			npc->task_timer = 0.f;
			ResetNpcPath(npc);
		}

		npc->task_timer += delta_seconds;

		WorldPos victim_pos;
		if (GetNpcTargetPos(npc->target_kind, npc->target_index, &victim_pos) == false)
		{
			ReportRobbery(*npc);
			return BT_SUCCESS;
		}

		// 챙길 만큼 챙겼으면 뜬다. 끝까지 두들기면 순찰병이 온다
		const float enough = 30.f + npc->greed * 30.f;
		if (static_cast<float>(npc->robbed_total) >= enough)
		{
			ReportRobbery(*npc);
			return BT_SUCCESS;
		}

		// 놓쳤다. 성벽 안으로 도망친 먹잇감은 수비대가 있어 못 쫓는다
		const bool escaped = IsWorldPosInTown(victim_pos);
		if (escaped || GetNpcDistance(*npc, victim_pos) > RAID_LOSE_DISTANCE || npc->task_timer > TASK_TIMEOUT_SEC)
		{
			if (npc->robbed_total > 0)
			{
				ReportRobbery(*npc);
				return BT_SUCCESS;
			}

			return BT_FAILURE;
		}

		const int32 result = NpcEngage(npc, npc->target_kind, npc->target_index, true, delta_seconds);

		if (result == BT_SUCCESS)
		{
			ReportRobbery(*npc);
		}

		return result;
	}

	int32 ActEscape(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			ResetNpcPath(npc);
			npc->task_timer = 0.f;

			// 들개단이 아닌 무법자는 갈 소굴이 없다
			if (npc->job != NPC_JOB_RAIDER)
			{
				return BT_SUCCESS;
			}

			const int32 hideout = FindNearestSite(*npc, SITE_HIDEOUT, -1);
			if (hideout < 0)
			{
				return BT_SUCCESS;
			}

			SetTarget(npc, NPC_TARGET_SITE, hideout);
		}

		return ActGoToTarget(npc, false, delta_seconds);
	}

	// 소굴은 멀어서 먹잇감이 시야에 잘 안 든다. 초소나 폐촌 둘레 길목으로 나가 기다린다
	int32 ActPickAmbushSpot(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_RAID;

		const std::vector<ZoneSite>& sites = GetNpcSites();
		std::vector<int32> candidates;

		for (size_t index = 0; index < sites.size(); ++index)
		{
			if (sites[index].kind == SITE_OUTPOST || sites[index].kind == SITE_VILLAGE)
			{
				candidates.push_back(static_cast<int32>(index));
			}
		}

		if (candidates.empty())
		{
			return BT_FAILURE;
		}

		for (int32 attempt = 0; attempt < 10; ++attempt)
		{
			const size_t pick = static_cast<size_t>(Roll(npc) * static_cast<float>(candidates.size())) % candidates.size();
			const float angle = Roll(npc) * 6.2832f;
			const float radius = RollRange(npc, 9.f, 15.f);

			WorldPos spot = GetSitePos(candidates[pick]);
			MoveWorldPos(&spot, std::cos(angle) * radius, std::sin(angle) * radius);

			int32 tile_x = 0;
			int32 tile_y = 0;
			WorldToZoneTile(spot, &tile_x, &tile_y);

			if (IsNavBlocked(tile_x, tile_y) || IsWorldPosInTown(spot))
			{
				continue;
			}

			npc->spot = spot;
			SetTarget(npc, NPC_TARGET_SPOT, -1);
			return BT_SUCCESS;
		}

		return BT_FAILURE;
	}

	int32 ActLurk(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (Linger(npc, is_enter, delta_seconds, 6.f, 12.f) == false)
		{
			return BT_RUNNING;
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 거래

	int32 ActPickBuyer(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_SELL;

		if (npc->goods <= 0)
		{
			return BT_FAILURE;
		}

		if (IsNpcOutlaw(*npc))
		{
			const int32 fence = FindNearestSite(*npc, SITE_HIDEOUT, -1);
			if (fence < 0)
			{
				return BT_FAILURE;
			}

			SetTarget(npc, NPC_TARGET_SITE, fence);
			return BT_SUCCESS;
		}

		float stall_distance = 0.f;
		const int32 stall = FindOpenStall(*npc, 10, &stall_distance);
		const int32 market = FindNearestSite(*npc, SITE_TOWN, -1);

		float market_distance = 1.0e9f;
		if (market >= 0 && GetTreasury() > 0)
		{
			market_distance = GetNpcDistance(*npc, GetSitePos(market));
		}

		// 행상이 값을 더 쳐 주니 조금 더 멀어도 행상에게 간다
		if (stall >= 0 && stall_distance < market_distance + 15.f)
		{
			SetTarget(npc, NPC_TARGET_NPC, stall);
			return BT_SUCCESS;
		}

		if (market_distance < 1.0e9f)
		{
			SetTarget(npc, NPC_TARGET_SITE, market);
			return BT_SUCCESS;
		}

		return BT_FAILURE;
	}

	int32 SellToMerchant(Npc* npc, Npc* merchant)
	{
		int32 amount = npc->goods;

		if (amount > merchant->goods_capacity - merchant->goods)
		{
			amount = merchant->goods_capacity - merchant->goods;
		}

		int32 pay = static_cast<int32>(static_cast<float>(amount) * MERCHANT_RATE);
		if (pay > merchant->gold)
		{
			pay = merchant->gold;
			amount = static_cast<int32>(static_cast<float>(pay) / MERCHANT_RATE);
		}

		if (amount <= 0 || pay <= 0)
		{
			return 0;
		}

		npc->goods -= amount;
		npc->gold += pay;
		merchant->goods += amount;
		merchant->gold -= pay;

		return pay;
	}

	int32 ActTrade(Npc* npc, bool is_enter, float delta_seconds)
	{
		int32 pay = 0;
		std::wstring where;

		if (npc->target_kind == NPC_TARGET_NPC)
		{
			if (IsLiveNpc(npc->target_index) == false)
			{
				return BT_FAILURE;
			}

			Npc& merchant = NpcAt(npc->target_index);

			// 오는 사이 노점을 걷었으면 헛걸음
			if (IsStallOpen(merchant) == false || GetNpcDistance(*npc, merchant.body.m_pos) > NPC_ARRIVE + 1.f)
			{
				return BT_FAILURE;
			}

			pay = SellToMerchant(npc, &merchant);
			where = GetNpcTitle(merchant) + L"에게";
		}
		else if (GetSiteKind(npc->target_index) == SITE_HIDEOUT)
		{
			pay = static_cast<int32>(static_cast<float>(npc->goods) * FENCE_RATE);
			npc->goods = 0;
			npc->gold += pay;
			where = L"장물아비에게";
		}
		else
		{
			const int32 want = static_cast<int32>(static_cast<float>(npc->goods) * MARKET_RATE);
			pay = TakeTreasury(want);

			// 금고가 모자라면 받은 만큼만 넘긴다
			int32 sold = npc->goods;
			if (pay < want && want > 0)
			{
				sold = npc->goods * pay / want;
			}

			npc->goods -= sold;
			npc->gold += pay;
			where = L"성채 시장에";
		}

		if (pay <= 0)
		{
			return BT_FAILURE;
		}

		NpcShowText(*npc, L"+" + std::to_wstring(pay) + L" 골드", 1.f, 0.82f, 0.30f);

		if (pay >= 25)
		{
			PushNpcEvent(WithSubject(GetNpcTitle(*npc)) + L" 짐을 " + where + L" " + std::to_wstring(pay) + L"골드에 넘겼다", 0.95f, 0.82f, 0.40f);
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 채집

	int32 ActPickResource(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_GATHER;

		if (npc->goods >= npc->goods_capacity)
		{
			return BT_FAILURE;
		}

		const std::vector<NpcResource>& resources = GetNpcResources();
		int32 best = -1;
		float best_distance = RESOURCE_SEARCH_RANGE;

		for (size_t index = 0; index < resources.size(); ++index)
		{
			if (resources[index].regrow_timer > 0.f)
			{
				continue;
			}

			const float distance = GetNpcDistance(*npc, resources[index].body.m_pos);
			if (distance >= best_distance)
			{
				continue;
			}

			if (IsClaimedByOther(*npc, NPC_TARGET_RESOURCE, static_cast<int32>(index)))
			{
				continue;
			}

			// 거식귀 한 방이면 채집꾼은 끝이다. 다가가기 전에 거른다
			float monster_distance = 0.f;
			if (FindNearestMonster(resources[index].body.m_pos, RESOURCE_MONSTER_CLEARANCE, &monster_distance) >= 0)
			{
				continue;
			}

			best_distance = distance;
			best = static_cast<int32>(index);
		}

		if (best < 0)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, NPC_TARGET_RESOURCE, best);

		return BT_SUCCESS;
	}

	int32 ActHarvest(Npc* npc, bool is_enter, float delta_seconds)
	{
		std::vector<NpcResource>& resources = GetNpcResources();
		NpcResource& resource = resources[static_cast<size_t>(npc->target_index)];

		// 먼저 온 사람이 캐 갔다
		if (resource.regrow_timer > 0.f)
		{
			return BT_FAILURE;
		}

		float work_sec = 2.5f;
		if (resource.kind == RESOURCE_RELIC)
		{
			work_sec = 3.5f;
		}

		if (Linger(npc, is_enter, delta_seconds, work_sec, work_sec) == false)
		{
			npc->work_anim = 0.2f;
			return BT_RUNNING;
		}

		int32 gained = resource.value;
		if (gained > npc->goods_capacity - npc->goods)
		{
			gained = npc->goods_capacity - npc->goods;
		}

		npc->goods += gained;
		resource.regrow_timer = RollRange(npc, 70.f, 110.f);

		if (resource.kind == RESOURCE_RELIC)
		{
			NpcShowText(*npc, L"+유품", 0.85f, 0.85f, 0.75f);
		}
		else
		{
			NpcShowText(*npc, L"+약초", 0.55f, 0.95f, 0.60f);
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 행상

	int32 ActPickRoute(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_PEDDLE;

		const int32 town = FindNearestSite(*npc, SITE_TOWN, -1);

		// 짐이 쌓이면 성채 시장에 풀러 간다. 그게 행상의 차익이다
		if (town >= 0 && npc->goods >= npc->goods_capacity * 2 / 5)
		{
			SetTarget(npc, NPC_TARGET_SITE, town);
			return BT_SUCCESS;
		}

		const std::vector<ZoneSite>& sites = GetNpcSites();
		std::vector<int32> candidates;

		for (size_t index = 0; index < sites.size(); ++index)
		{
			const int32 kind = sites[index].kind;
			if (kind != SITE_OUTPOST && kind != SITE_TOWN)
			{
				continue;
			}

			// 지금 서 있는 거점은 빼고
			if (GetNpcDistance(*npc, GetSitePos(static_cast<int32>(index))) < 6.f)
			{
				continue;
			}

			candidates.push_back(static_cast<int32>(index));
		}

		if (candidates.empty())
		{
			return BT_FAILURE;
		}

		const size_t pick = static_cast<size_t>(Roll(npc) * static_cast<float>(candidates.size())) % candidates.size();
		SetTarget(npc, NPC_TARGET_SITE, candidates[pick]);

		return BT_SUCCESS;
	}

	int32 ActOpenStall(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			SetHomeIfSite(npc);

			if (GetSiteKind(npc->target_index) == SITE_TOWN && npc->goods > 0)
			{
				const int32 pay = TakeTreasury(npc->goods);
				npc->gold += pay;
				npc->goods -= pay;

				if (pay >= 40)
				{
					PushNpcEvent(WithSubject(GetNpcTitle(*npc)) + L" 성채 시장에 물건을 풀어 " + std::to_wstring(pay) + L"골드를 벌었다", 0.95f, 0.82f, 0.40f);
				}
			}
		}

		// 매 틱 다시 켠다. 도주로 끊기면 저절로 꺼진다
		npc->stall_heartbeat = 0.3f;

		if (Linger(npc, is_enter, delta_seconds, 22.f, 34.f) == false)
		{
			return BT_RUNNING;
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 순찰

	int32 ActPickPost(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_PATROL;

		const std::vector<ZoneSite>& sites = GetNpcSites();
		std::vector<int32> candidates;

		for (size_t index = 0; index < sites.size(); ++index)
		{
			const int32 kind = sites[index].kind;
			if (kind != SITE_OUTPOST && kind != SITE_GATE && kind != SITE_TOWN)
			{
				continue;
			}

			if (GetNpcDistance(*npc, GetSitePos(static_cast<int32>(index))) < 6.f)
			{
				continue;
			}

			candidates.push_back(static_cast<int32>(index));
		}

		if (candidates.empty())
		{
			return BT_FAILURE;
		}

		const size_t pick = static_cast<size_t>(Roll(npc) * static_cast<float>(candidates.size())) % candidates.size();
		SetTarget(npc, NPC_TARGET_SITE, candidates[pick]);

		return BT_SUCCESS;
	}

	int32 ActWatch(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			SetHomeIfSite(npc);

			// 성채에 들르면 급료. 금고가 마르면 끊긴다
			if (GetSiteKind(npc->target_index) == SITE_TOWN)
			{
				const int32 wage = TakeTreasury(WARDEN_WAGE);
				npc->gold += wage;
			}
		}

		if (Linger(npc, is_enter, delta_seconds, 6.f, 10.f) == false)
		{
			return BT_RUNNING;
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 식사

	int32 ActPickFood(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_EAT;

		if (IsNpcOutlaw(*npc))
		{
			const int32 hideout = FindNearestSite(*npc, SITE_HIDEOUT, -1);
			if (hideout < 0 || npc->gold < HIDEOUT_FOOD_PRICE)
			{
				return BT_FAILURE;
			}

			SetTarget(npc, NPC_TARGET_SITE, hideout);
			return BT_SUCCESS;
		}

		int32 best_kind = NPC_TARGET_NONE;
		int32 best_index = -1;
		float best_distance = 1.0e9f;

		const std::vector<ZoneSite>& sites = GetNpcSites();
		for (size_t index = 0; index < sites.size(); ++index)
		{
			const int32 kind = sites[index].kind;
			if (kind != SITE_TOWN && kind != SITE_OUTPOST)
			{
				continue;
			}

			if (npc->gold < GetFoodPriceAt(kind))
			{
				continue;
			}

			const float distance = GetNpcDistance(*npc, GetSitePos(static_cast<int32>(index)));
			if (distance < best_distance)
			{
				best_distance = distance;
				best_kind = NPC_TARGET_SITE;
				best_index = static_cast<int32>(index);
			}
		}

		if (npc->gold >= NPC_FOOD_PRICE)
		{
			float stall_distance = 0.f;
			const int32 stall = FindOpenStall(*npc, 0, &stall_distance);

			if (stall >= 0 && stall_distance < best_distance)
			{
				best_kind = NPC_TARGET_NPC;
				best_index = stall;
			}
		}

		if (best_kind == NPC_TARGET_NONE)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, best_kind, best_index);

		return BT_SUCCESS;
	}

	int32 ActEat(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (Linger(npc, is_enter, delta_seconds, 1.5f, 1.5f) == false)
		{
			return BT_RUNNING;
		}

		int32 price = NPC_FOOD_PRICE;
		int32 site_kind = SITE_TOWN;

		if (npc->target_kind == NPC_TARGET_NPC)
		{
			if (IsLiveNpc(npc->target_index) == false || IsStallOpen(NpcAt(npc->target_index)) == false)
			{
				return BT_FAILURE;
			}
		}
		else
		{
			site_kind = GetSiteKind(npc->target_index);
			price = GetFoodPriceAt(site_kind);
		}

		if (npc->gold < price)
		{
			return BT_FAILURE;
		}

		npc->gold -= price;

		if (npc->target_kind == NPC_TARGET_NPC)
		{
			NpcAt(npc->target_index).gold += price;
		}
		else if (site_kind != SITE_HIDEOUT)
		{
			// 소굴 밥값은 벽 안으로 안 돌아온다
			AddTreasury(price);
		}
		npc->hunger = 0.f;
		npc->hp += 15;
		if (npc->hp > npc->max_hp)
		{
			npc->hp = npc->max_hp;
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 휴식

	int32 ActPickShelter(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_REST;

		const int32 shelter = FindShelter(*npc);
		if (shelter < 0)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, NPC_TARGET_SITE, shelter);

		return BT_SUCCESS;
	}

	int32 ActRest(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (is_enter)
		{
			SetHomeIfSite(npc);
		}

		NpcHeal(npc, 4.f, delta_seconds);

		const bool timed_out = Linger(npc, is_enter, delta_seconds, 30.f, 30.f);

		if (npc->hp >= npc->max_hp || timed_out)
		{
			return BT_SUCCESS;
		}

		return BT_RUNNING;
	}

	// ------------------------------------------------------------ 정비

	int32 ActPickSmith(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_UPGRADE;

		if (npc->gold < GetNpcUpgradeCost(*npc))
		{
			return BT_FAILURE;
		}

		int32 smith = -1;
		if (IsNpcOutlaw(*npc))
		{
			smith = FindNearestSite(*npc, SITE_HIDEOUT, -1);
		}
		else
		{
			smith = FindNearestSite(*npc, SITE_TOWN, -1);
		}

		if (smith < 0)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, NPC_TARGET_SITE, smith);

		return BT_SUCCESS;
	}

	int32 ActBuyGear(Npc* npc, bool is_enter, float delta_seconds)
	{
		const int32 cost = GetNpcUpgradeCost(*npc);

		if (npc->gold < cost)
		{
			return BT_FAILURE;
		}

		npc->gold -= cost;

		if (IsNpcOutlaw(*npc) == false)
		{
			AddTreasury(cost);
		}

		npc->gear += 1;
		npc->max_hp += 12;
		npc->attack += 2;
		npc->hp = npc->max_hp;

		NpcShowText(*npc, L"장비 +" + std::to_wstring(npc->gear), 0.55f, 0.75f, 1.f);
		PushNpcEvent(WithSubject(GetNpcTitle(*npc)) + L" 장비를 맞췄다 (" + std::to_wstring(npc->gear) + L"단계)", 0.55f, 0.72f, 1.f);

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 시체 뒤지기

	int32 ActPickCorpse(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_SCAVENGE;

		const std::vector<NpcCorpse>& corpses = GetNpcCorpses();
		int32 best = -1;
		float best_distance = CORPSE_SEARCH_RANGE;

		for (const NpcCorpse& corpse : corpses)
		{
			const float distance = GetNpcDistance(*npc, corpse.body.m_pos);
			if (distance >= best_distance)
			{
				continue;
			}

			if (IsClaimedByOther(*npc, NPC_TARGET_CORPSE, corpse.id))
			{
				continue;
			}

			best_distance = distance;
			best = corpse.id;
		}

		if (best < 0)
		{
			return BT_FAILURE;
		}

		SetTarget(npc, NPC_TARGET_CORPSE, best);

		return BT_SUCCESS;
	}

	int32 ActLoot(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (Linger(npc, is_enter, delta_seconds, 2.f, 2.f) == false)
		{
			return BT_RUNNING;
		}

		for (NpcCorpse& corpse : GetNpcCorpses())
		{
			if (corpse.id != npc->target_index)
			{
				continue;
			}

			const int32 gold = corpse.gold;

			int32 goods = corpse.goods;
			if (goods > npc->goods_capacity - npc->goods)
			{
				goods = npc->goods_capacity - npc->goods;
			}

			npc->gold += gold;
			npc->goods += goods;
			corpse.gold = 0;
			corpse.goods -= goods;

			if (gold >= 10)
			{
				PushNpcEvent(WithSubject(GetNpcTitle(*npc)) + L" 죽은 자의 주머니에서 " + std::to_wstring(gold) + L"골드를 챙겼다", 0.75f, 0.70f, 0.60f);
			}

			return BT_SUCCESS;
		}

		return BT_FAILURE;
	}

	// ------------------------------------------------------------ 배회

	int32 ActPickSpot(Npc* npc, bool is_enter, float delta_seconds)
	{
		npc->task = NPC_TASK_WANDER;

		// 집에서 너무 멀면 집 근처로 돌아가 서성인다
		WorldPos anchor = npc->body.m_pos;
		if (npc->home_site >= 0 && GetNpcDistance(*npc, GetSitePos(npc->home_site)) > 25.f)
		{
			anchor = GetSitePos(npc->home_site);
		}

		for (int32 attempt = 0; attempt < 8; ++attempt)
		{
			WorldPos spot = anchor;
			MoveWorldPos(&spot, RollRange(npc, -6.f, 6.f), RollRange(npc, -6.f, 6.f));

			int32 tile_x = 0;
			int32 tile_y = 0;
			WorldToZoneTile(spot, &tile_x, &tile_y);

			if (IsNavBlocked(tile_x, tile_y))
			{
				continue;
			}

			npc->spot = spot;
			SetTarget(npc, NPC_TARGET_SPOT, -1);
			return BT_SUCCESS;
		}

		return BT_FAILURE;
	}

	int32 ActLinger(Npc* npc, bool is_enter, float delta_seconds)
	{
		if (Linger(npc, is_enter, delta_seconds, 2.f, 5.f) == false)
		{
			return BT_RUNNING;
		}

		return BT_SUCCESS;
	}

	// ------------------------------------------------------------ 감지

	float GetJobEngage(const Npc& npc, int32 enemy_kind, int32 enemy_index)
	{
		if (enemy_kind == NPC_TARGET_MONSTER)
		{
			float job = 0.05f;

			switch (npc.job)
			{
				case NPC_JOB_WARDEN:
				{
					job = 1.6f;
					break;
				}
				case NPC_JOB_HUNTER:
				{
					job = 1.2f;
					break;
				}
				case NPC_JOB_RAIDER:
				{
					job = 0.4f;
					break;
				}
				case NPC_JOB_MERCHANT:
				{
					job = 0.03f;
					break;
				}
				default:
				{
					break;
				}
			}

			// 거인은 누구에게나 무섭다
			const int32 kind = GetMonsterKind(enemy_index);
			if (kind == MONSTER_DEVOURER)
			{
				job *= 0.35f;
			}
			else if (kind == MONSTER_GATEBREAKER)
			{
				job *= 0.05f;
			}

			return job;
		}

		// 무법자가 본 순찰병
		if (IsNpcOutlaw(npc))
		{
			if (npc.job == NPC_JOB_RAIDER)
			{
				return 0.25f;
			}

			return 0.1f;
		}

		// 선량한 쪽이 본 무법자
		switch (npc.job)
		{
			case NPC_JOB_WARDEN:
			{
				return 2.f;
			}
			case NPC_JOB_HUNTER:
			{
				return 0.5f;
			}
			default:
			{
				return 0.03f;
			}
		}
	}

	float GetJobAvoid(const Npc& npc, int32 enemy_kind, int32 enemy_index)
	{
		float job = 0.5f;

		switch (npc.job)
		{
			case NPC_JOB_MERCHANT:
			case NPC_JOB_GATHERER:
			{
				job = 1.6f;
				break;
			}
			case NPC_JOB_HUNTER:
			{
				job = 0.4f;
				break;
			}
			case NPC_JOB_WARDEN:
			{
				job = 0.15f;
				break;
			}
			default:
			{
				break;
			}
		}

		if (enemy_kind == NPC_TARGET_MONSTER)
		{
			const int32 kind = GetMonsterKind(enemy_index);
			if (kind == MONSTER_DEVOURER || kind == MONSTER_GATEBREAKER)
			{
				job *= 2.f;
			}
		}
		else if (IsNpcOutlaw(npc))
		{
			job *= 2.5f;
		}

		return job;
	}

	// 선량한 쪽에게 적은 무법자와 몬스터, 무법자에게 적은 순찰병과 몬스터
	bool IsEnemyNpc(const Npc& self, const Npc& other)
	{
		if (IsNpcOutlaw(self))
		{
			return other.job == NPC_JOB_WARDEN && IsNpcOutlaw(other) == false;
		}

		return IsNpcOutlaw(other);
	}

	void FindEnemy(const Npc& npc, int32* kind, int32* index, float* distance)
	{
		*kind = NPC_TARGET_NONE;
		*index = -1;
		*distance = SENSE_RANGE;

		float monster_distance = 0.f;
		const int32 monster = FindNearestMonster(npc.body.m_pos, SENSE_RANGE, &monster_distance);

		if (monster >= 0)
		{
			*kind = NPC_TARGET_MONSTER;
			*index = monster;
			*distance = monster_distance;
		}

		for (size_t other_index = 0; other_index < Npcs().size(); ++other_index)
		{
			const Npc& other = Npcs()[other_index];

			if (&other == &npc || other.is_alive == false || IsEnemyNpc(npc, other) == false)
			{
				continue;
			}

			const float other_distance = GetNpcDistance(npc, other.body.m_pos);

			if (other_distance < *distance)
			{
				*kind = NPC_TARGET_NPC;
				*index = static_cast<int32>(other_index);
				*distance = other_distance;
			}
		}
	}

	// 맞설지 / 피할지 / 못 본 척할지를 가중치로 굴린다. 같은 적 앞에서도 사람마다, 순간마다 갈린다
	void RollStance(Npc* npc)
	{
		const float health = HealthRatio(*npc);

		const float odds = GetOdds(*npc, npc->enemy_kind, npc->enemy_index);

		float engage = npc->bravery * health * GetJobEngage(*npc, npc->enemy_kind, npc->enemy_index) * odds * 2.f;
		float avoid = (2.f - npc->bravery) * (1.3f - health) * GetJobAvoid(*npc, npc->enemy_kind, npc->enemy_index) * (1.f - odds) * 2.f;
		const float ignore = 0.6f;

		// 사냥하러 간 사냥꾼이 사냥감을 보고 도망가면 안 된다
		if (npc->task == NPC_TASK_HUNT && npc->enemy_kind == NPC_TARGET_MONSTER)
		{
			avoid = 0.f;
		}

		// 약탈 중인 들개단은 먹잇감만 본다
		if (npc->task == NPC_TASK_RAID)
		{
			engage = 0.f;
		}

		const float total = engage + avoid + ignore;
		const float roll = Roll(npc) * total;

		if (roll < engage)
		{
			npc->stance = NPC_STANCE_ENGAGE;
		}
		else if (roll < engage + avoid)
		{
			npc->stance = NPC_STANCE_AVOID;
		}
		else
		{
			npc->stance = NPC_STANCE_IGNORE;
		}

		npc->stance_timer = STANCE_REROLL_SEC;
	}
}

void BuildNpcBrain()
{
	if (g_built)
	{
		return;
	}

	g_built = true;

	BehaviorTree<Npc>& tree = g_tree;

	const int32 threat = tree.AddSequence(L"위협 대응",
		{
			tree.AddCondition(L"나를 때린 자가 있나", HasThreat),
			tree.AddWeighted(L"맞설까 달아날까",
				{
					tree.AddAction(L"반격", ActFightBack),
					tree.AddAction(L"도주", ActFleeThreat),
				},
				{ WeightFightBack, WeightFleeThreat }),
		});

	const int32 engage = tree.AddSequence(L"교전",
		{
			tree.AddCondition(L"덤빌 적이 보이나", WantsEngage),
			tree.AddAction(L"덤비기", ActEngageEnemy),
		});

	const int32 avoid = tree.AddSequence(L"회피",
		{
			tree.AddCondition(L"피할 적이 보이나", WantsAvoid),
			tree.AddAction(L"물러나기", ActAvoidEnemy),
		});

	// 자식 순서가 NPC_TASK_ 번호와 같아야 관찰창 가중치가 맞는다
	const int32 hunt = tree.AddSequence(L"사냥",
		{
			tree.AddAction(L"사냥감 고르기", ActPickPrey),
			tree.AddAction(L"쫓아가 쏘기", ActHunt),
		});

	// 먹잇감이 보이면 덮치고, 없으면 길목에 나가 기다린다
	const int32 raid = tree.AddSelector(L"약탈",
		{
			tree.AddSequence(L"습격",
				{
					tree.AddAction(L"먹잇감 고르기", ActPickVictim),
					tree.AddAction(L"덮치기", ActAmbush),
					tree.AddAction(L"소굴로 튀기", ActEscape),
				}),
			tree.AddSequence(L"매복",
				{
					tree.AddAction(L"길목 고르기", ActPickAmbushSpot),
					tree.AddAction(L"길목으로", ActGoToTarget),
					tree.AddAction(L"숨어 기다리기", ActLurk),
				}),
		});

	const int32 sell = tree.AddSequence(L"거래",
		{
			tree.AddAction(L"살 사람 찾기", ActPickBuyer),
			tree.AddAction(L"찾아가기", ActGoToTarget),
			tree.AddAction(L"넘기기", ActTrade),
		});

	const int32 gather = tree.AddSequence(L"채집",
		{
			tree.AddAction(L"자원 고르기", ActPickResource),
			tree.AddAction(L"찾아가기", ActGoToTarget),
			tree.AddAction(L"캐기", ActHarvest),
		});

	const int32 peddle = tree.AddSequence(L"행상",
		{
			tree.AddAction(L"행선지 고르기", ActPickRoute),
			tree.AddAction(L"짐 지고 가기", ActGoToTarget),
			tree.AddAction(L"노점 펴기", ActOpenStall),
		});

	const int32 patrol = tree.AddSequence(L"순찰",
		{
			tree.AddAction(L"초소 고르기", ActPickPost),
			tree.AddAction(L"걸어가기", ActGoToTarget),
			tree.AddAction(L"둘러보기", ActWatch),
		});

	const int32 eat = tree.AddSequence(L"식사",
		{
			tree.AddAction(L"먹을 곳 찾기", ActPickFood),
			tree.AddAction(L"찾아가기", ActGoToTarget),
			tree.AddAction(L"먹기", ActEat),
		});

	const int32 rest = tree.AddSequence(L"휴식",
		{
			tree.AddAction(L"쉴 곳 찾기", ActPickShelter),
			tree.AddAction(L"돌아가기", ActGoToTarget),
			tree.AddAction(L"쉬기", ActRest),
		});

	const int32 upgrade = tree.AddSequence(L"정비",
		{
			tree.AddAction(L"대장간 찾기", ActPickSmith),
			tree.AddAction(L"찾아가기", ActGoToTarget),
			tree.AddAction(L"장비 사기", ActBuyGear),
		});

	const int32 scavenge = tree.AddSequence(L"시체 뒤지기",
		{
			tree.AddAction(L"주머니 찾기", ActPickCorpse),
			tree.AddAction(L"찾아가기", ActGoToTarget),
			tree.AddAction(L"뒤지기", ActLoot),
		});

	const int32 wander = tree.AddSequence(L"배회",
		{
			tree.AddAction(L"갈 곳 고르기", ActPickSpot),
			tree.AddAction(L"걷기", ActGoToTarget),
			tree.AddAction(L"서성이기", ActLinger),
		});

	g_daily_node = tree.AddWeighted(L"일과",
		{ hunt, raid, sell, gather, peddle, patrol, eat, rest, upgrade, scavenge, wander },
		{ WeightHunt, WeightRaid, WeightSell, WeightGather, WeightPeddle, WeightPatrol, WeightEat, WeightRest, WeightUpgrade, WeightScavenge, WeightWander });

	const int32 root = tree.AddSelector(L"뿌리", { threat, engage, avoid, g_daily_node });
	tree.SetRoot(root);
}

void InitNpcBrain(Npc* npc, uint32 seed)
{
	g_tree.InitMemory(&npc->brain, seed);
}

void ResetNpcBrain(Npc* npc)
{
	g_tree.Reset(&npc->brain);
}

void SenseNpc(Npc* npc, float delta_seconds)
{
	npc->sense_timer -= delta_seconds;
	npc->stance_timer -= delta_seconds;

	if (npc->sense_timer > 0.f)
	{
		return;
	}

	npc->sense_timer = SENSE_INTERVAL;

	const int32 previous_kind = npc->enemy_kind;
	const int32 previous_index = npc->enemy_index;

	FindEnemy(*npc, &npc->enemy_kind, &npc->enemy_index, &npc->enemy_distance);

	if (npc->enemy_kind == NPC_TARGET_NONE)
	{
		npc->stance = NPC_STANCE_IGNORE;
		return;
	}

	const bool is_new = npc->enemy_kind != previous_kind || npc->enemy_index != previous_index;

	if (is_new || npc->stance_timer <= 0.f)
	{
		RollStance(npc);
	}
}

void ThinkNpc(Npc* npc, float delta_seconds)
{
	g_tree.Tick(npc, &npc->brain, delta_seconds);
}

const BehaviorTree<Npc>& GetNpcBrainTree()
{
	return g_tree;
}

int32 GetNpcDailyNode()
{
	return g_daily_node;
}

float GetNpcTaskWeight(const Npc& npc, int32 task)
{
	if (g_daily_node < 0)
	{
		return 0.f;
	}

	return g_tree.EvaluateWeight(g_daily_node, static_cast<size_t>(task), npc);
}

const wchar_t* GetNpcTaskName(int32 task)
{
	switch (task)
	{
		case NPC_TASK_HUNT:
		{
			return L"사냥";
		}
		case NPC_TASK_RAID:
		{
			return L"약탈";
		}
		case NPC_TASK_SELL:
		{
			return L"거래";
		}
		case NPC_TASK_GATHER:
		{
			return L"채집";
		}
		case NPC_TASK_PEDDLE:
		{
			return L"행상";
		}
		case NPC_TASK_PATROL:
		{
			return L"순찰";
		}
		case NPC_TASK_EAT:
		{
			return L"식사";
		}
		case NPC_TASK_REST:
		{
			return L"휴식";
		}
		case NPC_TASK_UPGRADE:
		{
			return L"정비";
		}
		case NPC_TASK_SCAVENGE:
		{
			return L"뒤지기";
		}
		case NPC_TASK_WANDER:
		{
			return L"배회";
		}
		case NPC_TASK_FIGHT:
		{
			return L"반격";
		}
		case NPC_TASK_FLEE:
		{
			return L"도주";
		}
		case NPC_TASK_ENGAGE:
		{
			return L"교전";
		}
		default:
		{
			return L"회피";
		}
	}
}

const wchar_t* GetNpcStanceName(int32 stance)
{
	if (stance == NPC_STANCE_ENGAGE)
	{
		return L"덤빈다";
	}

	if (stance == NPC_STANCE_AVOID)
	{
		return L"피한다";
	}

	return L"무시";
}
