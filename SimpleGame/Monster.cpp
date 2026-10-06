#include "stdafx.h"
#include "Monster.h"
#include "Balance.h"
#include "Drop.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Npc.h"
#include "Player.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "SpriteAtlas.h"
#include "World.h"
#include "ZoneGen.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	constexpr float WANDER_SPEED_RATE = 0.4f;	// 배회는 추적보다 느리게
	constexpr float WANDER_HOME_RADIUS = 3.f;
	constexpr float RESPAWN_RETRY_SEC = 3.f;
	constexpr float RETARGET_SEC = 0.35f;	// 표적 훑기 간격. 매 틱 NPC 전원을 훑을 필요는 없다
	constexpr float BEHIND_COS = -0.25f;	// 몸이 보는 쪽과 이 이상 어긋나면 등 뒤

	struct Monster
	{
		Actor body;
		WorldPos home_pos;
		int32 kind = MONSTER_BLIGHTED;

		int32 hp = 0;
		int32 max_hp = 0;
		int32 attack = 0;
		int32 exp = 0;
		int32 gold_min = 0;
		int32 gold_max = 0;

		float body_radius = 0.3f;
		float move_speed = 0.f;
		float aggro_range = 0.f;
		float attack_trigger_range = 0.f;	// 이 안이면 내려치기 시작
		float attack_hit_range = 0.f;	// 내려칠 때 이 안이면 맞는다
		float attack_interval = 0.f;
		float windup_sec = 0.f;
		float base_emissive = 1.f;
		bool is_boss = false;
		bool is_elite = false;
		bool has_weak_point = false;

		bool is_alive = true;
		bool is_aggro = false;
		bool is_returning = false;
		int32 target_npc = -1;	// -1 이면 플레이어
		float facing_x = 1.f;
		float facing_y = 0.f;
		float respawn_timer = 0.f;
		float retarget_timer = 0.f;
		float attack_cooldown = 0.f;
		float windup_timer = 0.f;	// 0 보다 크면 내려치기 직전
		float hit_flash = 0.f;
		float wander_dir_x = 0.f;
		float wander_dir_y = 0.f;
		float wander_timer = 0.f;
		float walk_distance = 0.f;
		float idle_timer = 0.f;
	};

	std::vector<Monster> g_monsters;

	WorldPos g_boss_spawn_pos;
	bool g_boss_summoned = false;

	float GetBandPower(int32 band)
	{
		if (band == BAND_INTRO)
		{
			return Balance::BAND_POWER_INTRO;
		}

		if (band == BAND_OUTER)
		{
			return Balance::BAND_POWER_OUTER;
		}

		return Balance::BAND_POWER_FIELD;
	}

	// 띠가 눈에도 읽혀야 한다. 깊은 곳 몹이 조금 더 밝다
	float GetBandEmissive(int32 band)
	{
		if (band == BAND_INTRO)
		{
			return 0.9f;
		}

		if (band == BAND_OUTER)
		{
			return 1.15f;
		}

		return 1.f;
	}

	int32 ScaleByPower(int32 value, float power)
	{
		const float scaled = static_cast<float>(value) * power;

		int32 result = static_cast<int32>(scaled + 0.5f);
		if (result < 1)
		{
			result = 1;
		}

		return result;
	}

	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x9e3779b9u);
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

	float DistanceTo(const WorldPos& from, const WorldPos& to, float* delta_x, float* delta_y)
	{
		*delta_x = GetTileDistanceX(from, to);
		*delta_y = GetTileDistanceY(from, to);

		return std::sqrt(*delta_x * *delta_x + *delta_y * *delta_y);
	}

	void FaceToward(Monster* monster, float delta_x, float delta_y)
	{
		const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
		if (length < 0.0001f)
		{
			return;
		}

		monster->facing_x = delta_x / length;
		monster->facing_y = delta_y / length;

		// 화면 가로 성분이 x - y 라서 이 부호가 곧 좌우
		monster->body.m_flip = (delta_x - delta_y) < 0.f;
	}

	// 걸은 거리를 세고 가는 쪽을 바라본다
	void MoveMonster(Monster* monster, float delta_x, float delta_y)
	{
		const WorldPos before = monster->body.m_pos;
		MoveWithCollision(&monster->body.m_pos, monster->body_radius, delta_x, delta_y);

		const float moved_x = GetTileDistanceX(before, monster->body.m_pos);
		const float moved_y = GetTileDistanceY(before, monster->body.m_pos);
		const float moved = std::sqrt(moved_x * moved_x + moved_y * moved_y);

		if (moved <= 0.0001f)
		{
			return;
		}

		monster->walk_distance += moved;
		monster->idle_timer = 0.f;
		FaceToward(monster, moved_x, moved_y);
	}

	void MoveToward(Monster* monster, float delta_x, float delta_y, float distance, float speed, float delta_seconds)
	{
		if (distance < 0.0001f)
		{
			return;
		}

		float step = speed * delta_seconds;
		if (step > distance)
		{
			step = distance;
		}

		MoveMonster(monster, delta_x / distance * step, delta_y / distance * step);
	}

	void Wander(Monster* monster, float delta_seconds)
	{
		monster->wander_timer -= delta_seconds;

		if (monster->wander_timer <= 0.f)
		{
			const float angle = RollRange(0.f, 2.f * PI);
			monster->wander_dir_x = std::cos(angle);
			monster->wander_dir_y = std::sin(angle);
			monster->wander_timer = RollRange(0.7f, 2.5f);
		}

		// 안 묶어 두면 무리가 서서히 흩어진다
		float home_x = 0.f;
		float home_y = 0.f;
		const float home_distance = DistanceTo(monster->body.m_pos, monster->home_pos, &home_x, &home_y);

		if (home_distance > WANDER_HOME_RADIUS)
		{
			monster->wander_dir_x = home_x / home_distance;
			monster->wander_dir_y = home_y / home_distance;
		}

		const float step = monster->move_speed * WANDER_SPEED_RATE * delta_seconds;
		MoveMonster(monster, monster->wander_dir_x * step, monster->wander_dir_y * step);
	}

	bool GetTargetPos(const Monster& monster, WorldPos* pos)
	{
		if (monster.target_npc >= 0)
		{
			return GetNpcPos(monster.target_npc, pos);
		}

		if (IsPlayerDead())
		{
			return false;
		}

		*pos = GetPlayer().body.m_pos;
		return true;
	}

	void ResolveAttack(const Monster& monster, int32 monster_index)
	{
		WorldPos target_pos;
		if (GetTargetPos(monster, &target_pos) == false)
		{
			return;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float distance = DistanceTo(monster.body.m_pos, target_pos, &to_x, &to_y);

		// 내려치는 사이 빠져나가면 피한다
		if (distance > monster.attack_hit_range + Balance::BODY_RADIUS)
		{
			return;
		}

		if (monster.target_npc >= 0)
		{
			DamageNpcFromMonster(monster.target_npc, monster.attack, monster_index);
			return;
		}

		DamagePlayer(monster.attack);
	}

	int32 GetDropTier(const Monster& monster)
	{
		if (monster.is_boss)
		{
			return DROP_TIER_BOSS;
		}

		// 심장석은 거식귀에게서 나온다는 소문이 맞도록
		if (monster.is_elite || monster.kind == MONSTER_DEVOURER)
		{
			return DROP_TIER_ELITE;
		}

		return DROP_TIER_NORMAL;
	}

	void Kill(Monster* monster, bool by_player)
	{
		monster->hp = 0;
		monster->is_alive = false;
		monster->windup_timer = 0.f;
		monster->respawn_timer = Balance::MONSTER_RESPAWN_SEC;
		monster->target_npc = -1;

		// NPC 가 잡은 것은 그 NPC 의 몫이다. 바닥에 떨구면 플레이어가 공짜로 줍는다
		if (by_player)
		{
			SpawnMonsterDrops(monster->body.m_pos, monster->exp, monster->gold_min, monster->gold_max, GetDropTier(*monster));
		}

		if (monster->is_boss)
		{
			AddFloatingText(monster->body.m_pos, 110.f, std::wstring(GetBossName()) + L" 쓰러지다!", 1.f, 0.80f, 0.30f);
			std::cout << "[ Monster - Kill ] : boss down\n";
		}
	}

	// 사냥터를 다시 채워야 파밍이 이어진다. 보스는 한 번뿐
	void UpdateRespawn(Monster* monster, float delta_seconds)
	{
		if (monster->is_boss)
		{
			return;
		}

		monster->respawn_timer -= delta_seconds;
		if (monster->respawn_timer > 0.f)
		{
			return;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float distance = DistanceTo(monster->home_pos, GetPlayer().body.m_pos, &to_x, &to_y);

		if (distance < Balance::MONSTER_RESPAWN_MIN_DISTANCE)
		{
			monster->respawn_timer = RESPAWN_RETRY_SEC;
			return;
		}

		monster->is_alive = true;
		monster->is_aggro = false;
		monster->is_returning = false;
		monster->target_npc = -1;
		monster->hp = monster->max_hp;
		monster->body.m_pos = monster->home_pos;
		monster->attack_cooldown = 0.f;
		monster->windup_timer = 0.f;
	}

	// 가까운 쪽을 문다. 거식귀는 사람만 먹으니 NPC 와 플레이어를 가리지 않는다
	void PickTarget(Monster* monster)
	{
		float best_distance = monster->aggro_range + 1.f;
		int32 best_npc = -2;

		if (IsPlayerDead() == false)
		{
			float to_x = 0.f;
			float to_y = 0.f;
			const float player_distance = DistanceTo(monster->body.m_pos, GetPlayer().body.m_pos, &to_x, &to_y);

			if (player_distance <= monster->aggro_range)
			{
				best_distance = player_distance;
				best_npc = -1;
			}
		}

		int32 npc_index = -1;
		float npc_distance = 0.f;

		if (FindNpcForMonster(monster->body.m_pos, monster->aggro_range, &npc_index, &npc_distance))
		{
			if (npc_distance < best_distance)
			{
				best_distance = npc_distance;
				best_npc = npc_index;
			}
		}

		if (best_npc == -2)
		{
			return;
		}

		monster->is_aggro = true;
		monster->target_npc = best_npc;
	}

	void UpdateMonster(Monster* monster, int32 monster_index, float delta_seconds)
	{
		if (monster->is_alive == false)
		{
			UpdateRespawn(monster, delta_seconds);
			return;
		}

		monster->idle_timer += delta_seconds;

		// 걸은 거리로 넘겨야 느린 거인과 빠른 늑대의 발이 둘 다 안 미끄러진다
		if (monster->idle_timer < 0.15f)
		{
			float stride = 3.f;
			if (monster->kind == MONSTER_DEVOURER || monster->kind == MONSTER_GATEBREAKER)
			{
				stride = 1.4f;
			}

			monster->body.m_frame = static_cast<int32>(monster->walk_distance * stride) % 2;
		}
		else
		{
			monster->body.m_frame = 0;
		}

		monster->attack_cooldown = CountDown(monster->attack_cooldown, delta_seconds);
		monster->hit_flash = CountDown(monster->hit_flash, delta_seconds);
		monster->body.m_emissive = monster->base_emissive + monster->hit_flash / Balance::HIT_FLASH_SEC * 3.f;

		// 내려치는 동안은 제자리. 이 틈이 피할 시간이자 등 뒤로 돌아 들어갈 시간이다
		if (monster->windup_timer > 0.f)
		{
			monster->windup_timer -= delta_seconds;

			if (monster->windup_timer <= 0.f)
			{
				monster->windup_timer = 0.f;
				monster->attack_cooldown = monster->attack_interval;
				ResolveAttack(*monster, monster_index);
			}

			return;
		}

		float home_x = 0.f;
		float home_y = 0.f;
		const float home_distance = DistanceTo(monster->body.m_pos, monster->home_pos, &home_x, &home_y);

		// 리쉬 복귀. 도착하면 체력을 채운다
		if (monster->is_returning)
		{
			if (home_distance < 0.5f)
			{
				monster->is_returning = false;
				monster->hp = monster->max_hp;
				return;
			}

			MoveToward(monster, home_x, home_y, home_distance, monster->move_speed, delta_seconds);
			return;
		}

		if (monster->is_aggro == false)
		{
			monster->retarget_timer -= delta_seconds;

			if (monster->retarget_timer <= 0.f)
			{
				monster->retarget_timer = RETARGET_SEC;
				PickTarget(monster);
			}
		}

		if (monster->is_aggro == false)
		{
			Wander(monster, delta_seconds);
			return;
		}

		WorldPos target_pos;
		if (GetTargetPos(*monster, &target_pos) == false)
		{
			monster->is_aggro = false;
			monster->target_npc = -1;
			return;
		}

		if (home_distance > Balance::MONSTER_LEASH)
		{
			monster->is_aggro = false;
			monster->target_npc = -1;
			monster->is_returning = true;
			return;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float target_distance = DistanceTo(monster->body.m_pos, target_pos, &to_x, &to_y);

		// 멈춰서 내려칠 때도 표적 쪽을 봐야 공격이 읽힌다
		FaceToward(monster, to_x, to_y);

		if (target_distance <= monster->attack_trigger_range)
		{
			if (monster->attack_cooldown <= 0.f)
			{
				monster->windup_timer = monster->windup_sec;
			}

			return;
		}

		MoveToward(monster, to_x, to_y, target_distance, monster->move_speed, delta_seconds);
	}

	// 거식귀는 등허리가 급소다. 몸이 보는 방향 반대편에서 들어온 칼만 크게 박힌다
	int32 ApplyWeakPoint(const Monster& monster, const WorldPos& attacker_pos, int32 damage, bool* is_weak)
	{
		*is_weak = false;

		if (monster.has_weak_point == false)
		{
			return damage;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float distance = DistanceTo(monster.body.m_pos, attacker_pos, &to_x, &to_y);

		if (distance < 0.05f)
		{
			return damage;
		}

		const float facing_cos = (to_x * monster.facing_x + to_y * monster.facing_y) / distance;

		if (facing_cos > BEHIND_COS)
		{
			return damage;
		}

		*is_weak = true;

		return static_cast<int32>(static_cast<float>(damage) * Balance::WEAK_POINT_MULTIPLIER + 0.5f);
	}

	void ShowDamage(const Monster& monster, int32 damage, bool is_weak)
	{
		const float head = static_cast<float>(GetSpriteHeight(monster.body.m_sprite) * PIXEL_SCALE);

		if (is_weak)
		{
			AddFloatingText(monster.body.m_pos, head + 8.f, L"급소 " + std::to_wstring(damage), 1.f, 0.55f, 0.15f);
			return;
		}

		AddFloatingText(monster.body.m_pos, head + 4.f, std::to_wstring(damage), 1.f, 0.92f, 0.75f);
	}

	void SetupKind(Monster* monster, int32 kind, float power)
	{
		monster->kind = kind;

		switch (kind)
		{
			case MONSTER_HOUND:
			{
				monster->max_hp = ScaleByPower(24, power);
				monster->attack = ScaleByPower(6, power);
				monster->exp = ScaleByPower(6, power);
				monster->gold_min = ScaleByPower(1, power);
				monster->gold_max = ScaleByPower(4, power);
				monster->move_speed = 2.7f;
				monster->aggro_range = 8.f;
				monster->attack_trigger_range = 0.9f;
				monster->attack_hit_range = 1.1f;
				monster->attack_interval = 0.9f;
				monster->windup_sec = 0.25f;
				monster->body.m_sprite = SPRITE_HOUND;
				break;
			}
			case MONSTER_DEVOURER:
			{
				monster->max_hp = ScaleByPower(170, power);
				monster->attack = ScaleByPower(20, power);
				monster->exp = ScaleByPower(28, power);
				monster->gold_min = ScaleByPower(8, power);
				monster->gold_max = ScaleByPower(16, power);
				monster->body_radius = 0.45f;
				monster->move_speed = 1.1f;
				monster->aggro_range = 8.f;
				monster->attack_trigger_range = 1.4f;
				monster->attack_hit_range = 1.9f;
				monster->attack_interval = 2.4f;
				monster->windup_sec = 0.9f;
				monster->has_weak_point = true;
				monster->body.m_sprite = SPRITE_DEVOURER;
				break;
			}
			default:
			{
				monster->max_hp = ScaleByPower(40, power);
				monster->attack = ScaleByPower(7, power);
				monster->exp = ScaleByPower(6, power);
				monster->gold_min = ScaleByPower(2, power);
				monster->gold_max = ScaleByPower(6, power);
				monster->move_speed = 1.3f;
				monster->aggro_range = 6.f;
				monster->attack_trigger_range = 0.9f;
				monster->attack_hit_range = 1.1f;
				monster->attack_interval = 1.3f;
				monster->windup_sec = 0.45f;
				monster->body.m_sprite = SPRITE_BLIGHTED;
				break;
			}
		}
	}

	// 재불 정예. 화면에서 제일 밝은 적이라 멀리서도 다르다는 게 읽힌다
	void MakeElite(Monster* monster)
	{
		monster->is_elite = true;
		monster->max_hp = ScaleByPower(monster->max_hp, Balance::ELITE_HP_RATE);
		monster->attack = ScaleByPower(monster->attack, Balance::ELITE_ATTACK_RATE);
		monster->exp = ScaleByPower(monster->exp, Balance::ELITE_EXP_RATE);
		monster->gold_min = ScaleByPower(monster->gold_min, 2.f);
		monster->gold_max = ScaleByPower(monster->gold_max, 2.f);
		monster->base_emissive = 1.9f;
	}
}

void ClearMonsters()
{
	g_monsters.clear();
	g_boss_summoned = false;
}

void AddMonster(int32 kind, const WorldPos& pos, int32 band, bool is_elite)
{
	Monster monster;
	monster.body.m_pos = pos;
	monster.home_pos = pos;
	monster.wander_timer = RollRange(0.f, 1.5f);
	monster.retarget_timer = RollRange(0.f, RETARGET_SEC);

	SetupKind(&monster, kind, GetBandPower(band));
	monster.base_emissive = GetBandEmissive(band);

	if (is_elite)
	{
		MakeElite(&monster);
	}

	monster.body.m_emissive = monster.base_emissive;
	monster.hp = monster.max_hp;

	g_monsters.push_back(monster);
}

void SetBossSpawnPoint(const WorldPos& pos)
{
	g_boss_spawn_pos = pos;
	g_boss_summoned = false;
}

bool IsBossSummoned()
{
	return g_boss_summoned;
}

void SummonBoss()
{
	if (g_boss_summoned)
	{
		return;
	}

	g_boss_summoned = true;

	Monster monster;
	monster.body.m_pos = g_boss_spawn_pos;
	monster.home_pos = g_boss_spawn_pos;
	monster.kind = MONSTER_GATEBREAKER;
	monster.is_boss = true;
	monster.has_weak_point = true;
	monster.max_hp = 900;
	monster.attack = 30;
	monster.exp = 150;
	monster.gold_min = 90;
	monster.gold_max = 140;
	monster.body_radius = 0.6f;
	monster.move_speed = 1.5f;
	monster.aggro_range = 10.f;
	monster.attack_trigger_range = 1.9f;
	monster.attack_hit_range = 2.7f;	// 내려찍기 범위
	monster.attack_interval = 2.8f;
	monster.windup_sec = 1.1f;

	monster.body.m_sprite = SPRITE_GATEBREAKER;

	// 조금 빛나야 무리 속에서도 보스가 갈린다
	monster.base_emissive = 1.3f;
	monster.body.m_emissive = monster.base_emissive;

	monster.hp = monster.max_hp;
	g_monsters.push_back(monster);

	std::cout << "[ Monster - SummonBoss ] : boss awake\n";
}

void UpdateMonsters(float delta_seconds)
{
	for (size_t index = 0; index < g_monsters.size(); ++index)
	{
		UpdateMonster(&g_monsters[index], static_cast<int32>(index), delta_seconds);
	}
}

int32 HitMonstersInFan(const WorldPos& origin, float dir_x, float dir_y, float range, float half_cos, int32 damage)
{
	int32 hit_count = 0;

	for (Monster& monster : g_monsters)
	{
		if (monster.is_alive == false)
		{
			continue;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float distance = DistanceTo(origin, monster.body.m_pos, &to_x, &to_y);

		// 중심만 보면 코앞의 큰 몹을 헛친다. 몸 반경만큼 너그럽게
		if (distance > range + monster.body_radius)
		{
			continue;
		}

		// 겹쳐 선 몹은 방향이 안 나온다. 맞은 걸로 친다
		if (distance > 0.05f)
		{
			const float facing_cos = (to_x * dir_x + to_y * dir_y) / distance;

			if (facing_cos < half_cos)
			{
				continue;
			}
		}

		bool is_weak = false;
		const int32 dealt = ApplyWeakPoint(monster, origin, damage, &is_weak);

		hit_count += 1;
		monster.hp -= dealt;

		ShowDamage(monster, dealt, is_weak);
		monster.hit_flash = Balance::HIT_FLASH_SEC;
		monster.is_aggro = true;
		monster.is_returning = false;
		monster.target_npc = -1;

		// 거인은 안 밀린다
		if (monster.kind == MONSTER_BLIGHTED || monster.kind == MONSTER_HOUND)
		{
			MoveWithCollision(&monster.body.m_pos, monster.body_radius, dir_x * Balance::SWING_KNOCKBACK, dir_y * Balance::SWING_KNOCKBACK);
		}

		if (monster.hp <= 0)
		{
			Kill(&monster, true);
		}
	}

	return hit_count;
}

int32 GetMonsterCount()
{
	return static_cast<int32>(g_monsters.size());
}

bool IsMonsterAlive(int32 index)
{
	if (index < 0 || index >= static_cast<int32>(g_monsters.size()))
	{
		return false;
	}

	return g_monsters[static_cast<size_t>(index)].is_alive;
}

WorldPos GetMonsterPos(int32 index)
{
	if (index < 0 || index >= static_cast<int32>(g_monsters.size()))
	{
		return WorldPos();
	}

	return g_monsters[static_cast<size_t>(index)].body.m_pos;
}

int32 GetMonsterKind(int32 index)
{
	if (index < 0 || index >= static_cast<int32>(g_monsters.size()))
	{
		return MONSTER_BLIGHTED;
	}

	return g_monsters[static_cast<size_t>(index)].kind;
}

int32 GetMonsterThreat(int32 index)
{
	if (IsMonsterAlive(index) == false)
	{
		return 0;
	}

	const Monster& monster = g_monsters[static_cast<size_t>(index)];

	return monster.attack * monster.hp;
}

int32 FindNearestMonster(const WorldPos& pos, float range, float* distance)
{
	int32 best = -1;
	float best_distance = range;

	for (size_t index = 0; index < g_monsters.size(); ++index)
	{
		const Monster& monster = g_monsters[index];

		if (monster.is_alive == false || monster.is_boss)
		{
			continue;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float monster_distance = DistanceTo(pos, monster.body.m_pos, &to_x, &to_y);

		if (monster_distance < best_distance)
		{
			best_distance = monster_distance;
			best = static_cast<int32>(index);
		}
	}

	*distance = best_distance;
	return best;
}

bool DamageMonsterFromNpc(int32 index, int32 damage, int32 npc_index, const WorldPos& attacker_pos, MonsterReward* reward)
{
	if (IsMonsterAlive(index) == false)
	{
		return false;
	}

	Monster& monster = g_monsters[static_cast<size_t>(index)];

	bool is_weak = false;
	const int32 dealt = ApplyWeakPoint(monster, attacker_pos, damage, &is_weak);

	monster.hp -= dealt;
	monster.hit_flash = Balance::HIT_FLASH_SEC;
	monster.is_returning = false;

	// 플레이어와 싸우는 중이면 표적을 안 바꾼다. 안 그러면 NPC 가 끼어들 때마다 등을 보인다
	if (monster.is_aggro == false || monster.target_npc >= 0)
	{
		monster.is_aggro = true;
		monster.target_npc = npc_index;
	}

	if (is_weak)
	{
		ShowDamage(monster, dealt, true);
	}

	if (monster.hp > 0)
	{
		return false;
	}

	reward->exp = monster.exp;
	reward->gold = RollInt(monster.gold_min, monster.gold_max);
	reward->trophy = monster.exp * 2;

	Kill(&monster, false);

	return true;
}

void ForgetMonsterTargetNpc(int32 npc_index)
{
	for (Monster& monster : g_monsters)
	{
		if (monster.target_npc != npc_index)
		{
			continue;
		}

		monster.target_npc = -1;
		monster.is_aggro = false;
		monster.windup_timer = 0.f;
	}
}

const wchar_t* GetMonsterKindName(int32 kind)
{
	switch (kind)
	{
		case MONSTER_HOUND:
		{
			return L"재늑대";
		}
		case MONSTER_DEVOURER:
		{
			return L"소거식귀";
		}
		case MONSTER_GATEBREAKER:
		{
			return L"성문파쇄자";
		}
		default:
		{
			return L"타락자";
		}
	}
}

void CollectMonsterActors()
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_alive)
		{
			AddSceneActor(&monster.body);
		}
	}
}

void DrawMonsterTelegraphs(Renderer* renderer, const WorldPos& camera)
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_alive == false || monster.windup_timer <= 0.f)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(monster.body.m_pos, camera);
		const float progress = 1.f - monster.windup_timer / monster.windup_sec;

		// 판정 반경을 그대로 그려야 어디까지 빠지면 되는지 읽힌다
		const float full_scale = (monster.attack_hit_range + Balance::BODY_RADIUS) * 2.f;

		renderer->DrawSolidDiamond(screen.x, screen.y, full_scale, 0.35f, 0.02f, 0.02f, 0.18f);
		renderer->DrawSolidDiamond(screen.x, screen.y, full_scale * progress, 0.90f, 0.08f, 0.05f, 0.35f);
	}
}

void DrawMonsterBars(Renderer* renderer, const WorldPos& camera)
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_alive == false || monster.is_boss)
		{
			continue;
		}

		// 정예는 다 찬 막대도 보여 준다. 멀리서 "저건 세다"가 읽혀야 한다
		if (monster.hp >= monster.max_hp && monster.is_elite == false)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(monster.body.m_pos, camera);
		const float bar_y = screen.y + static_cast<float>(GetSpriteHeight(monster.body.m_sprite) * PIXEL_SCALE) + 6.f;
		const float ratio = static_cast<float>(monster.hp) / static_cast<float>(monster.max_hp);

		float bar_width = 22.f;
		if (monster.kind == MONSTER_DEVOURER)
		{
			bar_width = 36.f;
		}

		const float fill_width = bar_width * ratio;

		renderer->DrawSolidBox(screen.x, bar_y, bar_width + 2.f, 5.f, 0.02f, 0.02f, 0.02f, 0.9f);

		if (monster.is_elite)
		{
			renderer->DrawSolidBox(screen.x - (bar_width - fill_width) * 0.5f, bar_y, fill_width, 3.f, 1.f, 0.55f, 0.12f, 1.f);
		}
		else
		{
			renderer->DrawSolidBox(screen.x - (bar_width - fill_width) * 0.5f, bar_y, fill_width, 3.f, 0.75f, 0.08f, 0.06f, 1.f);
		}
	}
}

bool GetBossPos(WorldPos* pos)
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_boss == false)
		{
			continue;
		}

		if (monster.is_alive == false)
		{
			return false;
		}

		*pos = monster.body.m_pos;
		return true;
	}

	return false;
}

bool GetBossHpRatio(float* ratio)
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_boss == false)
		{
			continue;
		}

		if (monster.is_alive == false || monster.is_aggro == false)
		{
			return false;
		}

		*ratio = static_cast<float>(monster.hp) / static_cast<float>(monster.max_hp);
		return true;
	}

	return false;
}

bool IsBossDead()
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_boss)
		{
			return monster.is_alive == false;
		}
	}

	return false;
}

const wchar_t* GetBossName()
{
	return L"성문파쇄자";
}

void ForEachLiveMonster(void (*visit)(const WorldPos& pos, int32 kind, bool is_elite, void* context), void* context)
{
	for (const Monster& monster : g_monsters)
	{
		if (monster.is_alive)
		{
			visit(monster.body.m_pos, monster.kind, monster.is_elite, context);
		}
	}
}
