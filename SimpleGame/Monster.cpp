#include "stdafx.h"
#include "Monster.h"
#include "Balance.h"
#include "Drop.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Player.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "SpriteAtlas.h"
#include "World.h"

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

	struct Monster
	{
		Actor body;
		WorldPos home_pos;

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

		bool is_alive = true;
		bool is_aggro = false;
		bool is_returning = false;
		float respawn_timer = 0.f;
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
		monster->body.m_flip = (moved_x - moved_y) < 0.f;
	}

	Monster MakeBase(const WorldPos& pos)
	{
		Monster monster;
		monster.body.m_pos = pos;
		monster.home_pos = pos;
		monster.wander_timer = RollRange(0.f, 1.5f);

		return monster;
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

	void ResolveAttack(const Monster& monster)
	{
		if (IsPlayerDead())
		{
			return;
		}

		float to_x = 0.f;
		float to_y = 0.f;
		const float distance = DistanceTo(monster.body.m_pos, GetPlayer().body.m_pos, &to_x, &to_y);

		// 내려치는 사이 빠져나가면 피한다
		if (distance <= monster.attack_hit_range + Balance::BODY_RADIUS)
		{
			DamagePlayer(monster.attack);
		}
	}

	void Kill(Monster* monster)
	{
		monster->hp = 0;
		monster->is_alive = false;
		monster->windup_timer = 0.f;
		monster->respawn_timer = Balance::MONSTER_RESPAWN_SEC;

		SpawnMonsterDrops(monster->body.m_pos, monster->exp, monster->gold_min, monster->gold_max, monster->is_boss);

		if (monster->is_boss)
		{
			AddFloatingText(monster->body.m_pos, 80.f, std::wstring(GetBossName()) + L" 처치!", 1.f, 0.80f, 0.30f);
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
		monster->hp = monster->max_hp;
		monster->body.m_pos = monster->home_pos;
		monster->attack_cooldown = 0.f;
		monster->windup_timer = 0.f;
	}

	void UpdateMonster(Monster* monster, float delta_seconds)
	{
		if (monster->is_alive == false)
		{
			UpdateRespawn(monster, delta_seconds);
			return;
		}

		monster->idle_timer += delta_seconds;

		// 걸은 거리로 넘겨야 느린 좀비와 빠른 해골의 발이 둘 다 안 미끄러진다
		if (monster->idle_timer < 0.15f)
		{
			monster->body.m_frame = static_cast<int32>(monster->walk_distance * 3.f) % 2;
		}
		else
		{
			monster->body.m_frame = 0;
		}

		monster->attack_cooldown = CountDown(monster->attack_cooldown, delta_seconds);
		monster->hit_flash = CountDown(monster->hit_flash, delta_seconds);
		monster->body.m_emissive = monster->base_emissive + monster->hit_flash / Balance::HIT_FLASH_SEC * 3.f;

		// 내려치는 동안은 제자리. 이 틈이 피할 시간이다
		if (monster->windup_timer > 0.f)
		{
			monster->windup_timer -= delta_seconds;

			if (monster->windup_timer <= 0.f)
			{
				monster->windup_timer = 0.f;
				monster->attack_cooldown = monster->attack_interval;
				ResolveAttack(*monster);
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

		float to_x = 0.f;
		float to_y = 0.f;
		const float player_distance = DistanceTo(monster->body.m_pos, GetPlayer().body.m_pos, &to_x, &to_y);

		if (IsPlayerDead())
		{
			monster->is_aggro = false;
		}
		else if (player_distance <= monster->aggro_range)
		{
			monster->is_aggro = true;
		}

		if (monster->is_aggro && home_distance > Balance::MONSTER_LEASH)
		{
			monster->is_aggro = false;
			monster->is_returning = true;
			return;
		}

		if (monster->is_aggro == false)
		{
			Wander(monster, delta_seconds);
			return;
		}

		// 멈춰서 내려칠 때도 플레이어 쪽을 봐야 공격이 읽힌다
		monster->body.m_flip = (to_x - to_y) < 0.f;

		if (player_distance <= monster->attack_trigger_range)
		{
			if (monster->attack_cooldown <= 0.f)
			{
				monster->windup_timer = monster->windup_sec;
			}

			return;
		}

		MoveToward(monster, to_x, to_y, player_distance, monster->move_speed, delta_seconds);
	}
}

void ClearMonsters()
{
	g_monsters.clear();
}

void AddZombie(const WorldPos& pos)
{
	Monster monster = MakeBase(pos);
	monster.max_hp = 38;
	monster.attack = 7;
	monster.exp = 6;
	monster.gold_min = 2;
	monster.gold_max = 6;
	monster.move_speed = 1.4f;
	monster.aggro_range = 6.f;
	monster.attack_trigger_range = 0.9f;
	monster.attack_hit_range = 1.1f;
	monster.attack_interval = 1.3f;
	monster.windup_sec = 0.4f;

	monster.body.m_sprite = SPRITE_ZOMBIE;

	monster.hp = monster.max_hp;
	g_monsters.push_back(monster);
}

void AddSkeleton(const WorldPos& pos)
{
	Monster monster = MakeBase(pos);
	monster.max_hp = 26;
	monster.attack = 9;
	monster.exp = 7;
	monster.gold_min = 3;
	monster.gold_max = 7;
	monster.move_speed = 2.2f;
	monster.aggro_range = 7.f;
	monster.attack_trigger_range = 0.9f;
	monster.attack_hit_range = 1.1f;
	monster.attack_interval = 1.1f;
	monster.windup_sec = 0.3f;

	monster.body.m_sprite = SPRITE_SKELETON;

	monster.hp = monster.max_hp;
	g_monsters.push_back(monster);
}

void AddBoss(const WorldPos& pos)
{
	Monster monster = MakeBase(pos);
	monster.is_boss = true;
	monster.max_hp = 600;
	monster.attack = 26;
	monster.exp = 120;
	monster.gold_min = 80;
	monster.gold_max = 120;
	monster.body_radius = 0.45f;
	monster.move_speed = 1.8f;
	monster.aggro_range = 9.f;
	monster.attack_trigger_range = 1.6f;
	monster.attack_hit_range = 2.3f;	// 내려찍기 범위
	monster.attack_interval = 2.6f;
	monster.windup_sec = 0.9f;

	monster.body.m_sprite = SPRITE_BOSS;

	// 조금 빛나야 무리 속에서도 보스가 갈린다
	monster.base_emissive = 1.35f;
	monster.body.m_emissive = monster.base_emissive;

	monster.hp = monster.max_hp;
	g_monsters.push_back(monster);
}

void UpdateMonsters(float delta_seconds)
{
	for (Monster& monster : g_monsters)
	{
		UpdateMonster(&monster, delta_seconds);
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

		hit_count += 1;
		monster.hp -= damage;

		const float head = static_cast<float>(GetSpriteHeight(monster.body.m_sprite) * PIXEL_SCALE);
		AddFloatingText(monster.body.m_pos, head + 4.f, std::to_wstring(damage), 1.f, 0.92f, 0.75f);
		monster.hit_flash = Balance::HIT_FLASH_SEC;
		monster.is_aggro = true;
		monster.is_returning = false;

		// 보스는 안 밀린다
		if (monster.is_boss == false)
		{
			MoveWithCollision(&monster.body.m_pos, monster.body_radius, dir_x * Balance::SWING_KNOCKBACK, dir_y * Balance::SWING_KNOCKBACK);
		}

		if (monster.hp <= 0)
		{
			Kill(&monster);
		}
	}

	return hit_count;
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
	const float bar_width = 22.f;

	for (const Monster& monster : g_monsters)
	{
		if (monster.is_alive == false || monster.is_boss)
		{
			continue;
		}

		if (monster.hp >= monster.max_hp)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(monster.body.m_pos, camera);
		const float bar_y = screen.y + static_cast<float>(GetSpriteHeight(monster.body.m_sprite) * PIXEL_SCALE) + 6.f;
		const float ratio = static_cast<float>(monster.hp) / static_cast<float>(monster.max_hp);
		const float fill_width = bar_width * ratio;

		renderer->DrawSolidBox(screen.x, bar_y, bar_width + 2.f, 5.f, 0.02f, 0.02f, 0.02f, 0.9f);
		renderer->DrawSolidBox(screen.x - (bar_width - fill_width) * 0.5f, bar_y, fill_width, 3.f, 0.75f, 0.08f, 0.06f, 1.f);
	}
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
	return L"모래무덤의 군주";
}
