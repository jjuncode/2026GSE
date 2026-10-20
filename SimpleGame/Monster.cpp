#include "stdafx.h"
#include "Monster.h"
#include "Audio.h"
#include "Balance.h"
#include "CombatDraw.h"
#include "Drop.h"
#include "Feel.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Npc.h"
#include "Player.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "SpriteAtlas.h"
#include "Text.h"
#include "World.h"
#include "ZoneGen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;
	const float DEG = PI / 180.f;

	constexpr int32 AI_PATROL = 0;
	constexpr int32 AI_CHASE = 1;
	constexpr int32 AI_ATTACK = 2;
	constexpr int32 AI_RETURN = 3;

	constexpr int32 TARGET_NONE = 0;
	constexpr int32 TARGET_PLAYER = 1;
	constexpr int32 TARGET_NPC = 2;

	const float RETARGET_SEC = 0.3f;	// 표적 훑기 간격. 매 틱 NPC 전원을 훑을 필요는 없다
	const float LOSE_TARGET_RATE = 1.6f;	// 탐색 거리의 이 배를 넘으면 놓친다
	const float RETURN_ARRIVE = 0.7f;
	const float PATROL_WAIT_MIN = 1.f;
	const float PATROL_WAIT_MAX = 3.5f;
	const float GLINT_LEAD_SEC = 0.22f;
	const float DEATH_ANIM_SEC = 1.f;
	const float CORPSE_FADE_SEC = 1.f;	// 시체가 사그라드는 마지막 구간	// 창이 열리기 이만큼 전에 번쩍인다. 패링 판정 0.25 초 안쪽
	const float FOCUS_SEC = 6.f;
	const float MAX_COLLISION_RADIUS = 0.45f;	// 벽 충돌은 한 칸보다 좁아야 모서리 넷으로 잡힌다
	const float SEPARATION_RANGE = 22.f;	// 플레이어에서 이 안의 몬스터만 서로 민다
	const float NPC_TARGET_RADIUS = 0.25f;

	class Monster;

	std::vector<std::unique_ptr<Monster>> g_monsters;
	WorldPos g_boss_spawn_pos;
	bool g_boss_summoned = false;
	int32 g_focus_index = -1;
	float g_focus_timer = 0.f;

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
		if (max_value < min_value)
		{
			return min_value;
		}

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

	int32 GetMonsterSheet(int32 kind)
	{
		switch (kind)
		{
			case MONSTER_HOUND:
			{
				return SHEET_HOUND;
			}
			case MONSTER_DEVOURER:
			{
				return SHEET_DEVOURER;
			}
			case MONSTER_GATEBREAKER:
			{
				return SHEET_GATEBREAKER;
			}
			default:
			{
				return SHEET_BLIGHTED;
			}
		}
	}

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

	int32 GetBandLevel(int32 band)
	{
		if (band == BAND_INTRO)
		{
			return Balance::BAND_LEVEL_INTRO;
		}

		if (band == BAND_OUTER)
		{
			return Balance::BAND_LEVEL_OUTER;
		}

		return Balance::BAND_LEVEL_FIELD;
	}

	int32 ScaleByPower(int32 value, float power)
	{
		const int32 result = static_cast<int32>(static_cast<float>(value) * power + 0.5f);

		if (result < 1 && value > 0)
		{
			return 1;
		}

		return result;
	}

	float TurnToward(float current, float target, float max_step)
	{
		float delta = target - current;

		while (delta > PI)
		{
			delta -= PI * 2.f;
		}

		while (delta < -PI)
		{
			delta += PI * 2.f;
		}

		if (delta > max_step)
		{
			delta = max_step;
		}
		else if (delta < -max_step)
		{
			delta = -max_step;
		}

		return current + delta;
	}

	class Monster : public Creature
	{
	public:
		void Draw(Renderer* renderer, const WorldPos& camera) const override;
		void ApplyHitReact(const HitInfo& hit) override;
		void OnDamaged(const HitInfo& hit, int32 taken) override;
		void OnDeath(Creature* killer) override;
		void OnParried(const HitInfo& hit) override;
		void OnWindowCheck(const HitInfo& hit) override;

		void Setup(int32 kind, const WorldPos& pos, int32 band, bool is_elite);
		void Revive();
		void Update(float delta_seconds, int32 self_index);
		bool IsInWorld() const;
		float GetChestLift() const;
		void Aggro(int32 target_kind, int32 npc_index);

	private:
		void UpdateDead(float delta_seconds);
		void UpdatePatrol(float delta_seconds);
		void UpdateChase(float delta_seconds);
		void UpdateAttack(float delta_seconds);
		void UpdateReturn(float delta_seconds);
		void UpdateCues();
		void UpdatePose();

		bool SearchTarget();
		bool GetTargetPos(WorldPos* pos, float* radius) const;
		void SelectSkill();
		void StartAttack();
		void WalkToward(const WorldPos& dest, float speed, float delta_seconds);
		bool IsOverLeash() const;
		float GetHitReactCool() const;
		void AlertPack(Creature* attacker);

	public:
		const MonsterData* m_data = nullptr;
		int32 m_kind = MONSTER_BLIGHTED;
		int32 m_index = -1;
		WorldPos m_home;
		int32 m_band = BAND_FIELD;
		bool m_is_elite = false;
		int32 m_grade = GRADE_NORMAL;
		int32 m_exp = 0;
		int32 m_gold_min = 0;
		int32 m_gold_max = 0;
		float m_drop_rolls = 1.f;
		float m_base_glow = 1.f;

		int32 m_ai = AI_PATROL;
		int32 m_target = TARGET_NONE;
		int32 m_target_npc = -1;
		float m_retarget_timer = 0.f;
		WorldPos m_patrol_dest;
		bool m_has_patrol_dest = false;
		float m_patrol_wait = 0.f;
		const SkillData* m_selected_skill = nullptr;
		float m_next_attack_timer = 0.f;	// 공격 자체의 쿨
		int32 m_combo_next_skill = SKILL_ID_NONE;
		float m_strafe_sign = 1.f;
		float m_heal_carry = 0.f;
		float m_corpse_timer = 0.f;
		float m_respawn_timer = 0.f;
		bool m_in_world = true;	// 시체까지 치운 뒤면 false
		int32 m_glint_serial = -1;	// 번쩍임을 이미 낸 시전과 창
		int32 m_glint_window = -1;
		int32 m_sound_window = -1;
		int32 m_npc_hit_serial = -1;
		int32 m_npc_hit_window = -1;
		float m_walk_distance = 0.f;
		float m_idle_timer = 0.f;
		bool m_killed_by_player = false;
	};

	Monster* GetMonsterAt(int32 index)
	{
		if (index < 0 || index >= static_cast<int32>(g_monsters.size()))
		{
			return nullptr;
		}

		return g_monsters[static_cast<size_t>(index)].get();
	}

	void Monster::Setup(int32 kind, const WorldPos& pos, int32 band, bool is_elite)
	{
		m_kind = kind;
		m_data = &GetMonsterData(kind + 1);
		m_home = pos;
		m_band = band;
		m_is_elite = is_elite;
		m_team = TEAM_MONSTER;

		const MonsterData& data = *m_data;
		const float power = GetBandPower(band);

		m_sprite = data.sprite;
		m_sheet = GetMonsterSheet(kind);
		m_grade = data.grade;
		m_body_radius = std::min(data.body_radius, MAX_COLLISION_RADIUS);
		m_hit_radius = data.hit_radius;
		m_weak_point_rate = data.weak_point_rate;
		m_level = std::max(1, data.level + GetBandLevel(band));

		m_stat = data.stat;
		m_stat.max_hp = ScaleByPower(data.stat.max_hp, power);
		m_stat.attack = ScaleByPower(data.stat.attack, power);
		m_exp = ScaleByPower(data.reward.exp, power);
		m_gold_min = ScaleByPower(data.reward.gold_min, power);
		m_gold_max = ScaleByPower(data.reward.gold_max, power);
		m_drop_rolls = 1.f;

		// 띠가 눈에도 읽혀야 한다. 깊은 곳 몹이 조금 더 밝다
		m_base_glow = 0.9f + 0.1f * static_cast<float>(band);

		// 재불 정예. 화면에서 제일 밝은 적이라 멀리서도 "저건 다르다" 가 읽힌다
		if (is_elite)
		{
			m_stat.max_hp = ScaleByPower(m_stat.max_hp, Balance::ELITE_HP_RATE);
			m_stat.attack = ScaleByPower(m_stat.attack, Balance::ELITE_ATTACK_RATE);
			m_exp = ScaleByPower(m_exp, Balance::ELITE_EXP_RATE);
			m_gold_min *= 2;
			m_gold_max *= 2;
			m_drop_rolls = 2.f;
			m_level += 1;
			m_base_glow = 1.35f;

			if (m_grade == GRADE_NORMAL)
			{
				m_grade = GRADE_ELITE;
			}
		}

		InitSkills(data.skill_ids);
		Revive();
	}

	void Monster::Revive()
	{
		m_pos = m_home;
		m_hp = m_stat.max_hp;
		m_action = ACTION_NORMAL;
		m_action_time = 0.f;
		m_state_timer = 0.f;
		m_freeze_timer = 0.f;
		m_stun_timer = 0.f;
		m_super_armor_timer = 0.f;
		m_hit_react_cool = 0.f;
		m_flash = 0.f;
		CancelCast();
		StopMotion();

		m_ai = AI_PATROL;
		m_target = TARGET_NONE;
		m_target_npc = -1;
		m_selected_skill = nullptr;
		m_next_attack_timer = RollRange(0.f, 1.f);
		m_combo_next_skill = SKILL_ID_NONE;
		m_has_patrol_dest = false;
		m_patrol_wait = RollRange(0.f, PATROL_WAIT_MAX);
		m_in_world = true;
		m_killed_by_player = false;

		Face(RollRange(-1.f, 1.f), RollRange(-1.f, 1.f));
	}

	bool Monster::IsInWorld() const
	{
		return m_in_world;
	}

	float Monster::GetChestLift() const
	{
		return GetSheetBodyHeight(m_sheet) * static_cast<float>(PIXEL_SCALE) * 0.55f;
	}

	float Monster::GetHitReactCool() const
	{
		if (m_grade == GRADE_BOSS)
		{
			return GetConfig().boss_hit_react_cool_sec;
		}

		if (m_grade == GRADE_ELITE)
		{
			return GetConfig().elite_hit_react_cool_sec;
		}

		// 잡몹은 피격 쿨이 없다. 맞을 때마다 새로 경직 ( 기획서 잡몹 피격 로직 1 )
		return 0.f;
	}

	bool Monster::IsOverLeash() const
	{
		const float leash = m_data->patrol_radius + m_data->search_range * GetConfig().monster_chase_margin;

		return DistanceBetween(m_home, m_pos) > leash;
	}

	void Monster::Aggro(int32 target_kind, int32 npc_index)
	{
		if (m_ai == AI_RETURN || IsDead())
		{
			return;
		}

		m_target = target_kind;
		m_target_npc = npc_index;

		if (m_ai == AI_PATROL)
		{
			m_ai = AI_CHASE;
		}
	}

	void Monster::AlertPack(Creature* attacker)
	{
		if (attacker == nullptr || attacker->m_team != TEAM_PLAYER)
		{
			return;
		}

		// 한 마리를 치면 무리가 같이 돌아본다
		const float range = GetConfig().pack_alert_range;
		for (const std::unique_ptr<Monster>& other : g_monsters)
		{
			if (other.get() == this || other->IsDead() || other->m_in_world == false)
			{
				continue;
			}

			if (other->m_ai != AI_PATROL)
			{
				continue;
			}

			if (DistanceBetween(other->m_pos, m_pos) <= range)
			{
				other->Aggro(TARGET_PLAYER, -1);
			}
		}
	}

	void Monster::ApplyHitReact(const HitInfo& hit)
	{
		const GameConfig& config = GetConfig();
		const SkillData& skill = *hit.skill;

		const bool stunned = m_stun_timer > 0.f;
		const bool cooling = m_hit_react_cool > 0.f && GetHitReactCool() > 0.f;

		// 정예와 보스는 피격 쿨 안이면 데미지만 받는다. 스턴은 쿨과 상관없이 먹는다
		if (cooling && stunned == false)
		{
			return;
		}

		float lock = config.monster_hit_recovery_sec;
		if (stunned)
		{
			lock = std::max(lock, m_stun_timer);
		}

		const float to_x = GetTileDistanceX(m_pos, hit.origin);
		const float to_y = GetTileDistanceY(m_pos, hit.origin);

		// 넉백은 잡몹만 밀린다. 덩치 큰 놈이 밀려나면 위치 싸움이 깨진다
		const bool knockback = skill.knockback.movements.empty() == false && m_grade == GRADE_NORMAL;

		CancelCast();
		StopMotion();

		if (knockback)
		{
			StartMotion(skill.knockback, to_x, to_y);
			lock = std::max(lock, skill.knockback.length + 0.15f);
			Face(to_x, to_y);
		}
		else if (skill.hit_movement.movements.empty() == false && m_grade != GRADE_BOSS)
		{
			StartMotion(skill.hit_movement, to_x, to_y);
		}

		// 패링 브레이크 · 앞선 피격 잠금은 줄이지 않는다
		if ((m_action == ACTION_ATTACK_INTERRUPTED || m_action == ACTION_DAMAGED) && m_state_timer > lock)
		{
			lock = m_state_timer;
		}

		SetAction(ACTION_DAMAGED, lock);
		m_hit_react_cool = GetHitReactCool();

		// 콤보 끊김
		m_combo_next_skill = SKILL_ID_NONE;
		m_next_attack_timer = 0.f;
		m_selected_skill = nullptr;
		m_ai = AI_CHASE;
	}

	void Monster::OnDamaged(const HitInfo& hit, int32 taken)
	{
		Creature::OnDamaged(hit, taken);

		if (hit.attacker != nullptr && hit.attacker->m_team == TEAM_PLAYER)
		{
			g_focus_index = m_index;
			g_focus_timer = FOCUS_SEC;

			if (IsDead() == false)
			{
				Aggro(TARGET_PLAYER, -1);
				AlertPack(hit.attacker);
			}
		}
	}

	void Monster::OnDeath(Creature* killer)
	{
		m_corpse_timer = GetConfig().corpse_sec;
		m_respawn_timer = m_data->respawn_sec;
		m_super_armor_timer = 0.f;

		const float lift = GetChestLift();

		// 재로 흩어진다
		SpawnDust(m_pos, 10, 0.25f, 0.24f, 0.24f);
		SpawnSparks(m_pos, lift, 0.f, 1.f, 12, 120.f, 2.4f, 1.6f, 0.6f, 0.3f, 0.6f);
		PlaySfxAt(SFX_MONSTER_DIE, m_pos, 0.8f);

		if (killer == nullptr || killer->m_team != TEAM_PLAYER)
		{
			return;
		}

		m_killed_by_player = true;

		GrantKillReward(m_exp, m_level);

		const int32 rolls = static_cast<int32>(m_drop_rolls);
		SpawnRewardDrops(m_pos, m_data->reward, RollInt(m_gold_min, m_gold_max), rolls);

		if (m_grade == GRADE_BOSS)
		{
			AddSlowMotion(1.2f, 0.25f);
			AddScreenFlash(1.f, 0.8f, 0.5f, 0.5f);
			AddShake(0.8f);
			SpawnRing(m_pos, 4.f, 2.4f, 1.6f, 0.8f, 1.f);
			AddDamageText(m_pos, lift + 40.f, std::wstring(m_data->name) + L" 쓰러지다", DAMAGE_STYLE_BANNER_GOOD);
		}
	}

	void Monster::OnParried(const HitInfo& hit)
	{
		// 패링당하면 하던 공격은 버린다. 끝나면 상황을 다시 본다 ( 기획서 13 )
		m_combo_next_skill = SKILL_ID_NONE;
		m_next_attack_timer = 0.f;
		m_selected_skill = nullptr;
		m_super_armor_timer = 0.f;
		m_ai = AI_CHASE;

		m_hurt_shake = 0.25f;
	}

	void Monster::OnWindowCheck(const HitInfo& hit)
	{
		if (m_target != TARGET_NPC)
		{
			return;
		}

		if (m_npc_hit_serial == m_cast_serial && m_npc_hit_window == hit.window_index)
		{
			return;
		}

		WorldPos npc_pos;
		if (GetNpcPos(m_target_npc, &npc_pos) == false)
		{
			return;
		}

		if (IsInAttackFan(hit.origin, hit.dir_x, hit.dir_y, *hit.skill, npc_pos, NPC_TARGET_RADIUS) == false)
		{
			return;
		}

		m_npc_hit_serial = m_cast_serial;
		m_npc_hit_window = hit.window_index;

		DamageNpcFromMonster(m_target_npc, hit.damage, m_index);
	}

	bool Monster::GetTargetPos(WorldPos* pos, float* radius) const
	{
		if (m_target == TARGET_PLAYER)
		{
			if (IsPlayerDead())
			{
				return false;
			}

			*pos = GetPlayer().m_pos;
			*radius = GetPlayer().m_hit_radius;
			return true;
		}

		if (m_target == TARGET_NPC)
		{
			*radius = NPC_TARGET_RADIUS;
			return GetNpcPos(m_target_npc, pos);
		}

		return false;
	}

	bool Monster::SearchTarget()
	{
		if (m_data->chases == false)
		{
			return false;
		}

		const float range = m_data->search_range;
		float best_distance = range;
		int32 best_kind = TARGET_NONE;
		int32 best_npc = -1;

		if (IsPlayerDead() == false)
		{
			const float distance = DistanceBetween(m_pos, GetPlayer().m_pos);
			if (distance <= best_distance)
			{
				best_distance = distance;
				best_kind = TARGET_PLAYER;
			}
		}

		int32 npc_index = -1;
		float npc_distance = 0.f;
		if (FindNpcForMonster(m_pos, range, &npc_index, &npc_distance) && npc_distance < best_distance)
		{
			best_kind = TARGET_NPC;
			best_npc = npc_index;
		}

		if (best_kind == TARGET_NONE)
		{
			return false;
		}

		m_target = best_kind;
		m_target_npc = best_npc;

		return true;
	}

	void Monster::SelectSkill()
	{
		m_selected_skill = nullptr;

		// 콤보 예약은 한 번만
		if (m_combo_next_skill != SKILL_ID_NONE)
		{
			const int32 combo = m_combo_next_skill;
			m_combo_next_skill = SKILL_ID_NONE;

			if (IsSkillReady(combo))
			{
				m_selected_skill = &GetSkillData(combo);
				return;
			}
		}

		if (m_data->attacks == false)
		{
			return;
		}

		// 쿨이 돈 것 중 무작위 ( 서버 RANDOM 정책 )
		std::vector<int32> ready;
		for (const OwnedSkill& owned : m_skills)
		{
			if (owned.cooldown <= 0.f)
			{
				ready.push_back(owned.skill_id);
			}
		}

		if (ready.empty())
		{
			return;
		}

		m_selected_skill = &GetSkillData(ready[static_cast<size_t>(RollInt(0, static_cast<int32>(ready.size()) - 1))]);
	}

	void Monster::WalkToward(const WorldPos& dest, float speed, float delta_seconds)
	{
		float delta_x = GetTileDistanceX(m_pos, dest);
		float delta_y = GetTileDistanceY(m_pos, dest);
		const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);

		if (distance < 0.05f || speed <= 0.f)
		{
			m_idle_timer += delta_seconds;
			return;
		}

		float step = speed * delta_seconds;
		if (step > distance)
		{
			step = distance;
		}

		const WorldPos before = m_pos;
		MoveWithCollision(&m_pos, m_body_radius, delta_x / distance * step, delta_y / distance * step);

		const float moved = DistanceBetween(before, m_pos);
		if (moved > 0.0001f)
		{
			m_walk_distance += moved;
			m_idle_timer = 0.f;
		}
		else
		{
			// 막히면 반대로 돈다
			m_strafe_sign = -m_strafe_sign;
			m_has_patrol_dest = false;
		}
	}

	void Monster::StartAttack()
	{
		const SkillData& skill = *m_selected_skill;
		m_selected_skill = nullptr;

		WorldPos target_pos;
		float target_radius = 0.f;
		if (GetTargetPos(&target_pos, &target_radius))
		{
			FaceToward(target_pos);
		}

		CastAttack(skill);
		m_ai = AI_ATTACK;

		// 보스는 휘두르는 동안 맞아도 안 끊긴다. 패링만 끊는다
		if (m_grade == GRADE_BOSS)
		{
			m_super_armor_timer = skill.anim.length;
		}

		// 콤보면 쿨 없이 바로 이어 친다
		float rest = m_data->attack_cooldown_sec * RollRange(0.8f, 1.2f);
		m_combo_next_skill = skill.combo_next_skill_id;
		if (m_combo_next_skill != SKILL_ID_NONE)
		{
			rest = 0.f;
		}

		m_next_attack_timer = skill.anim.length + rest;
		m_glint_serial = -1;
		m_sound_window = -1;
	}

	void Monster::UpdatePatrol(float delta_seconds)
	{
		m_retarget_timer -= delta_seconds;
		if (m_retarget_timer <= 0.f)
		{
			m_retarget_timer = RETARGET_SEC;

			if (SearchTarget())
			{
				m_ai = AI_CHASE;
				return;
			}
		}

		if (m_data->patrols == false)
		{
			m_idle_timer += delta_seconds;
			return;
		}

		if (m_patrol_wait > 0.f)
		{
			m_patrol_wait -= delta_seconds;
			m_idle_timer += delta_seconds;
			return;
		}

		if (m_has_patrol_dest == false)
		{
			const float angle = RollRange(0.f, PI * 2.f);
			const float radius = RollRange(0.3f, 1.f) * m_data->patrol_radius;

			m_patrol_dest = m_home;
			MoveWorldPos(&m_patrol_dest, std::cos(angle) * radius, std::sin(angle) * radius);
			m_has_patrol_dest = true;
		}

		if (DistanceBetween(m_pos, m_patrol_dest) < 0.15f)
		{
			m_has_patrol_dest = false;
			m_patrol_wait = RollRange(PATROL_WAIT_MIN, PATROL_WAIT_MAX);
			return;
		}

		FaceToward(m_patrol_dest);
		WalkToward(m_patrol_dest, m_data->patrol_speed, delta_seconds);
	}

	void Monster::UpdateChase(float delta_seconds)
	{
		// 리쉬 - 이동보다 먼저 봐야 넘긴 틱에 한 걸음 더 가지 않는다
		if (IsOverLeash())
		{
			m_ai = AI_RETURN;
			m_target = TARGET_NONE;
			m_heal_carry = 0.f;
			m_super_armor_timer = 30.f;	// 상한. 해제를 놓쳐도 스스로 풀린다
			return;
		}

		WorldPos target_pos;
		float target_radius = 0.f;

		if (GetTargetPos(&target_pos, &target_radius) == false || DistanceBetween(m_pos, target_pos) > m_data->search_range * LOSE_TARGET_RATE)
		{
			if (SearchTarget() == false)
			{
				m_ai = AI_PATROL;
				m_target = TARGET_NONE;
				m_has_patrol_dest = false;
				return;
			}

			if (GetTargetPos(&target_pos, &target_radius) == false)
			{
				return;
			}
		}

		// 사거리가 스킬마다 달라서 다가가기 전에 고른다
		if (m_selected_skill == nullptr)
		{
			SelectSkill();
		}

		const float distance = DistanceBetween(m_pos, target_pos);
		const bool ready = m_next_attack_timer <= 0.f && m_selected_skill != nullptr && CanAct();

		if (ready)
		{
			const float reach = m_selected_skill->range + target_radius;

			// 찌르기처럼 앞으로 나가며 치는 기술은 사거리 밖에서도 시작한다
			float lunge = 0.f;
			for (const MoveSegment& segment : m_selected_skill->anim.movements)
			{
				lunge += std::max(segment.forward, 0.f);
			}

			if (distance <= reach * 0.9f + lunge * 0.7f)
			{
				StartAttack();
				return;
			}

			FaceToward(target_pos);
			WalkToward(target_pos, m_data->move_speed, delta_seconds);
			return;
		}

		// 공격 쿨이 도는 동안은 일정 거리를 두고 둘레를 돈다 ( 서버 standoff )
		if (m_data->keep_distance > 0.f)
		{
			float away_x = GetTileDistanceX(target_pos, m_pos);
			float away_y = GetTileDistanceY(target_pos, m_pos);
			const float away_length = std::sqrt(away_x * away_x + away_y * away_y);

			if (away_length > 0.001f)
			{
				away_x /= away_length;
				away_y /= away_length;
			}
			else
			{
				away_x = 1.f;
				away_y = 0.f;
			}

			const float ring_angle = std::atan2(away_y, away_x) + m_strafe_sign * GetConfig().monster_standoff_step_deg * DEG;

			WorldPos ring = target_pos;
			MoveWorldPos(&ring, std::cos(ring_angle) * m_data->keep_distance, std::sin(ring_angle) * m_data->keep_distance);

			FaceToward(target_pos);
			WalkToward(ring, m_data->standoff_speed, delta_seconds);

			// 걷는 쪽이 아니라 표적을 본다
			FaceToward(target_pos);
			return;
		}

		FaceToward(target_pos);
		WalkToward(target_pos, m_data->move_speed, delta_seconds);
	}

	void Monster::UpdateAttack(float delta_seconds)
	{
		if (IsAttacking() == false)
		{
			m_ai = AI_CHASE;
			return;
		}

		// 예비 동작 앞부분까지만 표적을 따라 돈다. 그 뒤로 굳혀야 옆으로 빠지는 게 의미가 있다
		if (m_cast_skill == nullptr || m_cast_skill->hit_windows.empty())
		{
			return;
		}

		const float lock_time = m_cast_skill->hit_windows.front().start_sec - GetConfig().track_lock_lead_sec;
		if (m_action_time >= lock_time)
		{
			return;
		}

		WorldPos target_pos;
		float target_radius = 0.f;
		if (GetTargetPos(&target_pos, &target_radius) == false)
		{
			return;
		}

		const float current = GetWorldAngle(m_face_x, m_face_y);
		const float desired = std::atan2(GetTileDistanceY(m_pos, target_pos), GetTileDistanceX(m_pos, target_pos));
		const float turned = TurnToward(current, desired, GetConfig().track_turn_deg_per_sec * DEG * delta_seconds);

		Face(std::cos(turned), std::sin(turned));

		// 루트모션도 바뀐 방향을 따른다
		if (m_motion != nullptr)
		{
			m_motion_dir_x = m_face_x;
			m_motion_dir_y = m_face_y;
		}
	}

	void Monster::UpdateReturn(float delta_seconds)
	{
		if (DistanceBetween(m_home, m_pos) <= RETURN_ARRIVE)
		{
			m_hp = m_stat.max_hp;
			m_super_armor_timer = 0.f;
			m_ai = AI_PATROL;
			m_has_patrol_dest = false;
			return;
		}

		// 복귀 중엔 초당 최대 체력의 일정 비율씩 찬다
		m_heal_carry += static_cast<float>(m_stat.max_hp) * GetConfig().monster_return_heal_per_sec * delta_seconds;
		const int32 heal = static_cast<int32>(m_heal_carry);
		if (heal > 0)
		{
			m_heal_carry -= static_cast<float>(heal);
			Heal(heal);
		}

		FaceToward(m_home);
		WalkToward(m_home, m_data->move_speed, delta_seconds);
	}

	// 패링 타이밍 신호와 휘두르는 소리. 창마다 한 번
	void Monster::UpdateCues()
	{
		if (IsAttacking() == false || m_cast_skill == nullptr)
		{
			return;
		}

		const std::vector<HitWindow>& windows = m_cast_skill->hit_windows;

		for (int32 index = 0; index < static_cast<int32>(windows.size()); ++index)
		{
			const HitWindow& window = windows[static_cast<size_t>(index)];

			if (m_glint_serial != m_cast_serial || m_glint_window < index)
			{
				if (m_action_time >= window.start_sec - GLINT_LEAD_SEC && m_action_time < window.end_sec)
				{
					m_glint_serial = m_cast_serial;
					m_glint_window = index;

					const float lift = GetChestLift() + 6.f;

					// 못 막는 공격은 붉게 번쩍인다. 이건 패링 말고 빠져야 한다
					if (m_cast_skill->pierce_guard)
					{
						SpawnGlint(m_pos, lift, 4.f, 0.4f, 0.3f, 1.5f);
						PlaySfxAt(SFX_DANGER, m_pos, 0.85f);
					}
					else
					{
						SpawnGlint(m_pos, lift, 3.6f, 3.4f, 2.6f, 1.f);
						PlaySfxAt(SFX_GLINT, m_pos, 0.7f);
					}
				}
			}

			if (m_sound_window < index && m_action_time >= window.start_sec)
			{
				m_sound_window = index;
				PlaySfxAt(SFX_MONSTER_SWING, m_pos, 0.6f);
			}
		}
	}

	void Monster::UpdatePose()
	{
		const int32 dir = GetDir8(m_face_x, m_face_y);

		float ratio = 0.f;
		if (m_state_length > 0.f)
		{
			ratio = m_action_time / m_state_length;
		}

		float seconds = m_idle_timer;
		int32 anim = GetSheetAnim(m_sheet, ANIM_IDLE);

		if (IsDead())
		{
			anim = GetSheetAnim(m_sheet, ANIM_DEATH);
			ratio = m_action_time / DEATH_ANIM_SEC;
		}
		else if (m_action == ACTION_ATTACK_INTERRUPTED)
		{
			anim = GetSheetAnim(m_sheet, ANIM_PARRIED);
		}
		else if (m_action == ACTION_DAMAGED)
		{
			if (m_stun_timer > 0.f)
			{
				anim = GetSheetAnim(m_sheet, ANIM_STUN);
				seconds = m_action_time;
			}
			else
			{
				// 연달아 맞으면 같은 상태라 흐른 시간이 안 돌아간다. 남은 잠금으로 잰다
				anim = GetSheetAnim(m_sheet, ANIM_HIT);
				ratio = 1.f - m_state_timer / std::max(m_state_length, 0.01f);
			}
		}
		else if (IsAttacking() && m_cast_skill != nullptr)
		{
			const int32 skill_anim = GetSheetSkillAnim(m_sheet, m_cast_skill->skill_id);
			if (skill_anim >= 0)
			{
				anim = skill_anim;
				ratio = m_action_time / std::max(m_cast_skill->anim.length, 0.01f);
			}
		}
		else if (m_idle_timer < 0.15f)
		{
			anim = GetSheetAnim(m_sheet, ANIM_RUN);
		}

		m_frame = PickSheetFrame(m_sheet, anim, dir, ratio, seconds, m_walk_distance);
	}

	void Monster::UpdateDead(float delta_seconds)
	{
		if (m_corpse_timer > 0.f)
		{
			const float before = m_corpse_timer;
			m_corpse_timer -= delta_seconds;

			// 사그라들기 시작하는 순간 재로 흩어진다
			if (before > CORPSE_FADE_SEC && m_corpse_timer <= CORPSE_FADE_SEC)
			{
				SpawnVfx(VFX_ASH, m_pos, GetChestLift() * 0.6f, 0.f, 1.2f);
			}

			if (m_corpse_timer <= 0.f)
			{
				m_in_world = false;
			}

			return;
		}

		// 보스는 안 살아난다
		if (m_data->respawn_sec <= 0.f)
		{
			return;
		}

		m_respawn_timer -= delta_seconds;
		if (m_respawn_timer > 0.f)
		{
			return;
		}

		// 눈앞에서는 안 살아난다
		if (IsPlayerDead() == false && DistanceBetween(GetPlayer().m_pos, m_home) < GetConfig().respawn_min_distance)
		{
			m_respawn_timer = 3.f;
			return;
		}

		Revive();
	}

	void Monster::Update(float delta_seconds, int32 self_index)
	{
		m_index = self_index;

		if (IsDead())
		{
			TickCombat(delta_seconds);
			UpdateDead(delta_seconds);
			UpdatePose();
			return;
		}

		m_next_attack_timer = CountDown(m_next_attack_timer, delta_seconds);

		TickCombat(delta_seconds);

		if (IsDead())
		{
			UpdatePose();
			return;
		}

		UpdateCues();

		// 경직 · 스턴 · 패링 브레이크 중엔 아무 판단도 안 한다
		if (CanAct() == false || m_action == ACTION_DAMAGED || m_action == ACTION_ATTACK_INTERRUPTED)
		{
			m_idle_timer += delta_seconds;
			UpdatePose();
			return;
		}

		switch (m_ai)
		{
			case AI_PATROL:
			{
				UpdatePatrol(delta_seconds);
				break;
			}
			case AI_CHASE:
			{
				UpdateChase(delta_seconds);
				break;
			}
			case AI_ATTACK:
			{
				UpdateAttack(delta_seconds);
				break;
			}
			case AI_RETURN:
			{
				UpdateReturn(delta_seconds);
				break;
			}
			default:
			{
				m_ai = AI_PATROL;
				break;
			}
		}

		UpdatePose();
	}

	void Monster::Draw(Renderer* renderer, const WorldPos& camera) const
	{
		if (m_in_world == false)
		{
			return;
		}

		const ScreenPos foot = WorldToScreen(m_pos, camera);
		const float unit = static_cast<float>(PIXEL_SCALE);

		float body_x = foot.x;
		if (m_hurt_shake > 0.f)
		{
			body_x += std::sin(m_hurt_shake * 110.f) * unit;
		}

		float alpha = 1.f;
		if (IsDead())
		{
			// 시체는 마지막 1 초에 재로 사그라든다
			alpha = std::min(m_corpse_timer / CORPSE_FADE_SEC, 1.f);
		}

		float glow = m_shade * m_base_glow;
		float r = glow;
		float g = glow;
		float b = glow;

		if (m_is_elite)
		{
			r = glow * 1.25f;
			g = glow * 0.85f;
			b = glow * 0.55f;
		}

		if (m_action == ACTION_ATTACK_INTERRUPTED)
		{
			r *= 0.8f;
			g *= 0.8f;
			b *= 1.1f;
		}

		float flash = 0.f;
		if (m_flash > 0.f)
		{
			flash = std::min(m_flash / 0.12f, 1.f);
		}

		DrawSheetFrame(renderer, m_sheet, m_frame, body_x, foot.y, r, g, b, alpha, flash);

		if (IsDead())
		{
			return;
		}

		// 공격 궤적. 판정과 같은 부채꼴
		float trail_r = 2.2f;
		float trail_g = 0.55f;
		float trail_b = 0.35f;
		if (m_cast_skill != nullptr && m_cast_skill->pierce_guard)
		{
			trail_r = 2.4f;
			trail_g = 0.25f;
			trail_b = 0.9f;
		}

		renderer->SetPixelTriangles(true);
		DrawSwingTrail(renderer, camera, *this, GetChestLift() * 0.6f, 0.85f, trail_r, trail_g, trail_b);
		renderer->SetPixelTriangles(false);
	}

	// 그로기 표시. 머리 위에 도는 별 셋
	void DrawGroggy(Renderer* renderer, float x, float y, float time)
	{
		for (int32 index = 0; index < 3; ++index)
		{
			const float angle = time * 6.f + static_cast<float>(index) * PI * 2.f / 3.f;
			const float star_x = x + std::cos(angle) * 9.f;
			const float star_y = y + std::sin(angle) * 3.f;

			renderer->DrawSolidBox(star_x, star_y, 4.f, 4.f, 1.f, 0.9f, 0.35f, 1.f);
		}
	}

	void SeparateCreatures()
	{
		const WorldPos player_pos = GetPlayer().m_pos;
		const bool player_alive = IsPlayerDead() == false;

		for (size_t first = 0; first < g_monsters.size(); ++first)
		{
			Monster& one = *g_monsters[first];
			if (one.IsDead() || one.m_in_world == false || DistanceBetween(one.m_pos, player_pos) > SEPARATION_RANGE)
			{
				continue;
			}

			// 플레이어 몸 안으로 파고들지 않게 몬스터만 밀어낸다
			if (player_alive)
			{
				const float delta_x = GetTileDistanceX(player_pos, one.m_pos);
				const float delta_y = GetTileDistanceY(player_pos, one.m_pos);
				const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
				const float min_distance = (one.m_hit_radius + GetPlayer().m_hit_radius) * 1.1f;

				if (distance < min_distance && distance > 0.001f)
				{
					const float push = (min_distance - distance) * 0.5f;
					MoveWithCollision(&one.m_pos, one.m_body_radius, delta_x / distance * push, delta_y / distance * push);
				}
			}

			for (size_t second = first + 1; second < g_monsters.size(); ++second)
			{
				Monster& two = *g_monsters[second];
				if (two.IsDead() || two.m_in_world == false)
				{
					continue;
				}

				const float delta_x = GetTileDistanceX(one.m_pos, two.m_pos);
				const float delta_y = GetTileDistanceY(one.m_pos, two.m_pos);
				const float min_distance = (one.m_hit_radius + two.m_hit_radius) * 1.2f;

				if (std::fabs(delta_x) > min_distance || std::fabs(delta_y) > min_distance)
				{
					continue;
				}

				float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
				float dir_x = delta_x;
				float dir_y = delta_y;

				if (distance < 0.001f)
				{
					dir_x = 1.f;
					dir_y = 0.f;
					distance = 0.001f;
				}
				else
				{
					dir_x /= distance;
					dir_y /= distance;
				}

				if (distance >= min_distance)
				{
					continue;
				}

				const float push = (min_distance - distance) * 0.25f;
				MoveWithCollision(&one.m_pos, one.m_body_radius, -dir_x * push, -dir_y * push);
				MoveWithCollision(&two.m_pos, two.m_body_radius, dir_x * push, dir_y * push);
			}
		}
	}

	float g_groggy_time = 0.f;
}

void ClearMonsters()
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		UnregisterCreature(monster.get());
	}

	g_monsters.clear();
	g_boss_summoned = false;
	g_focus_index = -1;
	g_focus_timer = 0.f;
}

void AddMonster(int32 kind, const WorldPos& pos, int32 band, bool is_elite)
{
	std::unique_ptr<Monster> monster = std::make_unique<Monster>();
	monster->m_index = static_cast<int32>(g_monsters.size());
	monster->Setup(kind, pos, band, is_elite);

	RegisterCreature(monster.get());
	g_monsters.push_back(std::move(monster));
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

	AddMonster(MONSTER_GATEBREAKER, g_boss_spawn_pos, BAND_OUTER, false);
	g_boss_summoned = true;

	std::cout << "[ Monster - SummonBoss ] : boss awakened\n";
}

void UpdateMonsters(float world_seconds)
{
	g_focus_timer = CountDown(g_focus_timer, world_seconds);
	g_groggy_time += world_seconds;

	for (size_t index = 0; index < g_monsters.size(); ++index)
	{
		g_monsters[index]->Update(world_seconds, static_cast<int32>(index));
	}

	SeparateCreatures();
}

void CollectMonsterActors()
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->IsInWorld())
		{
			AddSceneActor(monster.get());
		}
	}
}

void DrawMonsterTelegraphs(Renderer* renderer, const WorldPos& camera)
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->IsDead() || monster->IsAttacking() == false || monster->m_cast_skill == nullptr)
		{
			continue;
		}

		const SkillData& skill = *monster->m_cast_skill;
		const std::vector<HitWindow>& windows = skill.hit_windows;
		if (windows.empty())
		{
			continue;
		}

		// 창 사이엔 다시 차오른다. 마지막 창이 끝나면 지운다
		int32 window_index = -1;
		const float progress = GetWindowProgress(*monster, &window_index);
		if (window_index < 0 || progress >= 1.f)
		{
			continue;
		}

		const HitWindow& window = windows[static_cast<size_t>(window_index)];
		float charge_from = 0.f;
		if (window_index > 0)
		{
			charge_from = windows[static_cast<size_t>(window_index - 1)].end_sec;
		}

		const float time = monster->m_action_time;
		float fill = (time - charge_from) / std::max(window.start_sec - charge_from, 0.01f);
		float alpha = 0.7f;
		float r = 1.2f;
		float g = 0.16f;
		float b = 0.08f;

		if (skill.pierce_guard)
		{
			r = 1.1f;
			g = 0.08f;
			b = 0.8f;
		}

		// 판정이 살아 있는 동안은 밝게
		if (time >= window.start_sec)
		{
			fill = 1.f;
			alpha = 0.95f;
			r = 2.2f;
			g = 0.7f;
			b = 0.25f;
		}

		const float facing = GetWorldAngle(monster->m_face_x, monster->m_face_y);
		DrawGroundSector(renderer, camera, monster->m_pos, facing, skill, fill, r, g, b, alpha);
	}
}

void DrawMonsterBars(Renderer* renderer, const WorldPos& camera)
{
	for (const std::unique_ptr<Monster>& owned : g_monsters)
	{
		const Monster& monster = *owned;
		if (monster.IsDead() || monster.m_in_world == false || monster.m_grade == GRADE_BOSS)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(monster.m_pos, camera);
		const float head_y = screen.y + monster.GetArtHeight() * static_cast<float>(PIXEL_SCALE) + 6.f;

		const bool groggy = monster.m_action == ACTION_ATTACK_INTERRUPTED || monster.m_stun_timer > 0.f;
		if (groggy)
		{
			DrawGroggy(renderer, screen.x, head_y + 10.f, g_groggy_time);
		}

		// 정예는 다 찬 막대도 보여 준다. 멀리서 "저건 세다" 가 읽혀야 한다
		if (monster.m_hp >= monster.m_stat.max_hp && monster.m_is_elite == false && monster.m_grade == GRADE_NORMAL)
		{
			continue;
		}

		float bar_width = 22.f;
		if (monster.m_grade != GRADE_NORMAL)
		{
			bar_width = 36.f;
		}

		const float ratio = monster.GetHpRatio();
		const float fill_width = bar_width * ratio;

		renderer->DrawSolidBox(screen.x, head_y, bar_width + 2.f, 5.f, 0.02f, 0.02f, 0.02f, 0.9f);

		if (monster.m_is_elite || monster.m_grade == GRADE_ELITE)
		{
			renderer->DrawSolidBox(screen.x - (bar_width - fill_width) * 0.5f, head_y, fill_width, 3.f, 1.f, 0.55f, 0.12f, 1.f);
		}
		else
		{
			renderer->DrawSolidBox(screen.x - (bar_width - fill_width) * 0.5f, head_y, fill_width, 3.f, 0.8f, 0.1f, 0.08f, 1.f);
		}
	}
}

bool GetBossPos(WorldPos* pos)
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->m_grade == GRADE_BOSS && monster->IsDead() == false)
		{
			*pos = monster->m_pos;
			return true;
		}
	}

	return false;
}

bool GetBossHpRatio(float* ratio)
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->m_grade != GRADE_BOSS || monster->IsDead())
		{
			continue;
		}

		// 싸우는 중일 때만 막대를 띄운다
		if (monster->m_ai == AI_PATROL)
		{
			return false;
		}

		*ratio = monster->GetHpRatio();
		return true;
	}

	return false;
}

bool IsBossDead()
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->m_grade == GRADE_BOSS)
		{
			return monster->IsDead();
		}
	}

	return false;
}

const wchar_t* GetBossName()
{
	return GetMonsterKindName(MONSTER_GATEBREAKER);
}

bool GetFocusMonster(MonsterFocus* focus)
{
	if (g_focus_timer <= 0.f)
	{
		return false;
	}

	Monster* monster = GetMonsterAt(g_focus_index);
	if (monster == nullptr || monster->m_grade == GRADE_BOSS)
	{
		return false;
	}

	focus->name = monster->m_data->name;
	if (monster->m_is_elite)
	{
		focus->name = L"재불 " + focus->name;
	}

	focus->level = monster->m_level;
	focus->hp_ratio = monster->GetHpRatio();
	focus->grade = monster->m_grade;
	focus->is_elite = monster->m_is_elite;
	focus->is_groggy = monster->m_action == ACTION_ATTACK_INTERRUPTED || monster->m_stun_timer > 0.f;

	return true;
}

int32 GetMonsterCount()
{
	return static_cast<int32>(g_monsters.size());
}

bool IsMonsterAlive(int32 index)
{
	const Monster* monster = GetMonsterAt(index);
	if (monster == nullptr)
	{
		return false;
	}

	return monster->IsDead() == false && monster->m_in_world;
}

WorldPos GetMonsterPos(int32 index)
{
	const Monster* monster = GetMonsterAt(index);
	if (monster == nullptr)
	{
		return WorldPos();
	}

	return monster->m_pos;
}

int32 GetMonsterKind(int32 index)
{
	const Monster* monster = GetMonsterAt(index);
	if (monster == nullptr)
	{
		return MONSTER_BLIGHTED;
	}

	return monster->m_kind;
}

int32 GetMonsterThreat(int32 index)
{
	if (IsMonsterAlive(index) == false)
	{
		return 0;
	}

	const Monster* monster = GetMonsterAt(index);
	return monster->m_stat.attack * monster->m_hp;
}

int32 FindNearestMonster(const WorldPos& pos, float range, float* distance)
{
	int32 best = -1;
	float best_distance = range;

	for (size_t index = 0; index < g_monsters.size(); ++index)
	{
		const Monster& monster = *g_monsters[index];
		if (monster.IsDead() || monster.m_in_world == false)
		{
			continue;
		}

		const float current = DistanceBetween(pos, monster.m_pos);
		if (current < best_distance)
		{
			best_distance = current;
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

	Monster& monster = *GetMonsterAt(index);

	int32 taken = damage;
	if (monster.m_weak_point_rate > 1.f && monster.IsBehind(attacker_pos))
	{
		taken = static_cast<int32>(static_cast<float>(taken) * monster.m_weak_point_rate);
	}

	taken = std::max(1, taken * 100 / (100 + monster.m_stat.defence));

	monster.m_hp -= taken;
	monster.m_flash = 0.12f;

	// 플레이어와 싸우는 중이 아니면 때린 NPC 를 돌아본다
	if (monster.m_target != TARGET_PLAYER)
	{
		monster.Aggro(TARGET_NPC, npc_index);
	}

	if (monster.m_hp > 0)
	{
		return false;
	}

	monster.m_hp = 0;
	monster.SetAction(ACTION_DEATH, 0.f);
	monster.CancelCast();
	monster.StopMotion();
	monster.OnDeath(nullptr);

	reward->exp = monster.m_exp;
	reward->gold = RollInt(monster.m_gold_min, monster.m_gold_max);
	reward->trophy = monster.m_exp / 2 + 3;

	return true;
}

void ForgetMonsterTargetNpc(int32 npc_index)
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->m_target == TARGET_NPC && monster->m_target_npc == npc_index)
		{
			monster->m_target = TARGET_NONE;
			monster->m_target_npc = -1;
		}
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

void ForEachLiveMonster(void (*visit)(const WorldPos& pos, int32 kind, bool is_elite, void* context), void* context)
{
	for (const std::unique_ptr<Monster>& monster : g_monsters)
	{
		if (monster->IsDead() || monster->m_in_world == false)
		{
			continue;
		}

		visit(monster->m_pos, monster->m_kind, monster->m_is_elite, context);
	}
}

bool GetMonsterReport(int32 index, MonsterReport* report)
{
	const Monster* monster = GetMonsterAt(index);
	if (monster == nullptr)
	{
		return false;
	}

	report->pos = monster->m_pos;
	report->home = monster->m_home;
	report->kind = monster->m_kind;
	report->is_elite = monster->m_is_elite;
	report->is_alive = monster->IsDead() == false && monster->m_in_world;
	report->is_fighting = monster->m_ai == AI_CHASE || monster->m_ai == AI_ATTACK;

	return true;
}
