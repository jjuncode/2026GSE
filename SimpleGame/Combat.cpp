#include "stdafx.h"
#include "Combat.h"
#include "Feel.h"
#include "World.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>

namespace
{
	const float PI = 3.14159265f;
	const float BEHIND_COS = -0.25f;	// 몸이 보는 쪽과 이만큼 어긋나면 등 뒤
	const float FLASH_SEC = 0.12f;
	const float HURT_SHAKE_SEC = 0.18f;

	std::vector<Creature*> g_creatures;

	struct HitCandidate
	{
		Creature* creature = nullptr;
		float sort_key = 0.f;
	};

	std::vector<HitCandidate> g_candidates;

	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x5bd1e995u);
		return random;
	}

	int32 RollPercent()
	{
		std::uniform_int_distribution<int32> distribution(1, 100);
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

	float Cross(float ax, float ay, float bx, float by)
	{
		return ax * by - ay * bx;
	}

	bool Normalize(float* x, float* y)
	{
		const float length = std::sqrt(*x * *x + *y * *y);
		if (length < 0.0001f)
		{
			return false;
		}

		*x /= length;
		*y /= length;

		return true;
	}

	// 월드 외적이 양수면 화면에서 오른쪽이다 ( 아이소 변환이 좌우를 한 번 뒤집는다 )
	float GetSideCos(const SkillData& skill, float cross)
	{
		if (cross > 0.f)
		{
			return std::cos(skill.angle_right * PI / 180.f);
		}

		return std::cos(skill.angle_left * PI / 180.f);
	}

	float ApplyToughness(float seconds, int32 toughness)
	{
		int32 resist = toughness;
		if (resist > GetConfig().toughness_max)
		{
			resist = GetConfig().toughness_max;
		}

		if (resist < 0)
		{
			resist = 0;
		}

		return seconds * (1.f - static_cast<float>(resist) / 100.f);
	}

	void ProcessParry(Creature* victim, Creature* attacker, const HitInfo& hit)
	{
		victim->EndGuard();
		victim->SetAction(ACTION_PARRY_SUCCESS, GetConfig().player_parry_success_sec);
		victim->m_parry_timer = 0.f;
		victim->OnParry(hit);

		// 공격은 그 자리에서 끊긴다. 남은 창은 판정하지 않는다
		attacker->CancelCast();
		attacker->StopMotion();
		attacker->SetAction(ACTION_ATTACK_INTERRUPTED, GetConfig().player_interrupted_sec);
		attacker->OnParried(hit);

		NotifyCombatHit(hit, victim, HIT_RESULT_PARRY, 0);
	}

	void ProcessGuard(Creature* victim, Creature* attacker, const HitInfo& hit)
	{
		const GameConfig& config = GetConfig();

		// 게이지는 가드 전 데미지로 깎는다 ( 서버 ProcessHitGuarded )
		const int32 full_taken = CalcTakenDamage(*victim, hit.damage, hit.penetration);
		victim->m_guard.gage -= static_cast<float>(full_taken) * config.guard_gage_cost_rate;

		bool is_break = false;
		if (victim->m_guard.gage <= 0.f)
		{
			is_break = true;
			victim->m_guard.gage = 0.f;
			victim->EndGuard();

			const float stun = ApplyToughness(config.guard_break_sec, victim->m_stat.toughness);
			victim->m_stun_timer = std::max(victim->m_stun_timer, stun);
			victim->SetAction(ACTION_DAMAGED, stun);
		}

		const int32 guarded_damage = static_cast<int32>(static_cast<float>(hit.damage) * config.guard_damage_rate);
		const int32 taken = CalcTakenDamage(*victim, guarded_damage, hit.penetration);

		victim->OnGuarded(hit, taken, is_break);

		int32 result = HIT_RESULT_GUARD;
		if (is_break)
		{
			result = HIT_RESULT_GUARD_BREAK;
		}

		attacker->OnHitLanded(hit, victim, result);
		NotifyCombatHit(hit, victim, result, taken);
	}

	void ProcessHit(Creature* victim, Creature* attacker, const HitInfo& hit)
	{
		// 슈퍼아머는 반응 대신 정화
		if (victim->IsSuperArmor())
		{
			victim->m_freeze_timer = 0.f;
		}
		else
		{
			// 스턴이 붙은 스킬은 경직을 대신한다
			if (hit.skill->stun_sec > 0.f)
			{
				const float stun = ApplyToughness(hit.skill->stun_sec, victim->m_stat.toughness);
				victim->m_stun_timer = std::max(victim->m_stun_timer, stun);
			}

			victim->ApplyHitReact(hit);
		}

		const int32 taken = CalcTakenDamage(*victim, hit.damage, hit.penetration);
		victim->OnDamaged(hit, taken);

		attacker->OnHitLanded(hit, victim, HIT_RESULT_HIT);
		NotifyCombatHit(hit, victim, HIT_RESULT_HIT, taken);
	}
}

Creature::~Creature()
{
}

bool Creature::IsParryable() const
{
	return false;
}

void Creature::ApplyHitReact(const HitInfo& hit)
{
}

void Creature::OnDamaged(const HitInfo& hit, int32 taken)
{
	if (IsDead())
	{
		return;
	}

	m_hp -= taken;
	m_flash = FLASH_SEC;
	m_hurt_shake = HURT_SHAKE_SEC;

	if (m_hp <= 0)
	{
		m_hp = 0;
		SetAction(ACTION_DEATH, 0.f);
		CancelCast();
		StopMotion();
		OnDeath(hit.attacker);
	}
}

void Creature::OnGuarded(const HitInfo& hit, int32 taken, bool is_break)
{
	OnDamaged(hit, taken);
}

void Creature::OnParry(const HitInfo& hit)
{
}

void Creature::OnDeath(Creature* killer)
{
}

void Creature::OnParried(const HitInfo& hit)
{
}

void Creature::OnHitLanded(const HitInfo& hit, Creature* victim, int32 result)
{
}

void Creature::OnWindowCheck(const HitInfo& hit)
{
}

bool Creature::IsDead() const
{
	return m_action == ACTION_DEATH;
}

bool Creature::CanAct() const
{
	if (IsDead())
	{
		return false;
	}

	if (m_freeze_timer > 0.f || m_stun_timer > 0.f)
	{
		return false;
	}

	return true;
}

bool Creature::CanMove() const
{
	return CanAct();
}

bool Creature::IsLocked() const
{
	if (m_action == ACTION_NORMAL)
	{
		return false;
	}

	if (IsGuarding())
	{
		return true;
	}

	return m_state_timer > 0.f;
}

bool Creature::IsGuarding() const
{
	return m_action == ACTION_GUARD && m_guard.gage > 0.f;
}

bool Creature::IsAttacking() const
{
	return m_action == ACTION_ATTACK_TRY || m_action == ACTION_ATTACK_SUCCESS;
}

bool Creature::IsSuperArmor() const
{
	return m_super_armor_timer > 0.f;
}

bool Creature::IsGuardDirValid(const WorldPos& attacker_pos) const
{
	float to_x = GetTileDistanceX(m_pos, attacker_pos);
	float to_y = GetTileDistanceY(m_pos, attacker_pos);

	// 겹쳐 서 있으면 정면으로 친다
	if (Normalize(&to_x, &to_y) == false)
	{
		return true;
	}

	return to_x * m_face_x + to_y * m_face_y >= GetConfig().guard_angle_half_cos;
}

bool Creature::IsBehind(const WorldPos& attacker_pos) const
{
	float to_x = GetTileDistanceX(m_pos, attacker_pos);
	float to_y = GetTileDistanceY(m_pos, attacker_pos);

	if (Normalize(&to_x, &to_y) == false)
	{
		return false;
	}

	return to_x * m_face_x + to_y * m_face_y < BEHIND_COS;
}

void Creature::SetAction(int32 action, float lock_sec)
{
	// 죽으면 아무 상태도 못 덮는다
	if (IsDead() && action != ACTION_DEATH)
	{
		return;
	}

	if (m_action != action)
	{
		m_action_time = 0.f;
	}

	m_action = action;

	if (lock_sec > 0.f)
	{
		m_state_timer = lock_sec;
		m_state_length = lock_sec;
	}
}

void Creature::Face(float dir_x, float dir_y)
{
	if (Normalize(&dir_x, &dir_y) == false)
	{
		return;
	}

	m_face_x = dir_x;
	m_face_y = dir_y;

	// 그림은 오른쪽을 본다. 화면 가로 성분이 x - y 라 이 부호가 곧 좌우
	m_flip = (dir_x - dir_y) < 0.f;
}

void Creature::FaceToward(const WorldPos& target)
{
	Face(GetTileDistanceX(m_pos, target), GetTileDistanceY(m_pos, target));
}

void Creature::InitSkills(const std::vector<int32>& skill_ids)
{
	m_skills.clear();

	for (int32 skill_id : skill_ids)
	{
		AddSkill(skill_id);
	}

	for (const OwnedSkill& owned : m_skills)
	{
		const SkillData& skill = GetSkillData(owned.skill_id);
		if (skill.skill_type == SKILL_TYPE_PARRY && skill.guard_gage_max > 0.f)
		{
			InitGuard(skill.guard_gage_max);
		}
	}
}

void Creature::AddSkill(int32 skill_id)
{
	if (HasSkill(skill_id) || GetSkillData(skill_id).skill_id == SKILL_ID_NONE)
	{
		return;
	}

	OwnedSkill owned;
	owned.skill_id = skill_id;
	m_skills.push_back(owned);
}

bool Creature::HasSkill(int32 skill_id) const
{
	for (const OwnedSkill& owned : m_skills)
	{
		if (owned.skill_id == skill_id)
		{
			return true;
		}
	}

	return false;
}

bool Creature::IsSkillReady(int32 skill_id) const
{
	for (const OwnedSkill& owned : m_skills)
	{
		if (owned.skill_id == skill_id)
		{
			return owned.cooldown <= 0.f;
		}
	}

	return false;
}

float Creature::GetSkillCooldown(int32 skill_id) const
{
	for (const OwnedSkill& owned : m_skills)
	{
		if (owned.skill_id == skill_id)
		{
			return owned.cooldown;
		}
	}

	return 0.f;
}

void Creature::StartCooldown(const SkillData& skill)
{
	if (skill.cooldown_sec <= 0.f)
	{
		return;
	}

	for (OwnedSkill& owned : m_skills)
	{
		if (owned.skill_id == skill.skill_id)
		{
			owned.cooldown = skill.cooldown_sec;
			return;
		}
	}
}

void Creature::ResetCooldownOfType(int32 skill_type)
{
	for (OwnedSkill& owned : m_skills)
	{
		if (GetSkillData(owned.skill_id).skill_type == skill_type)
		{
			owned.cooldown = 0.f;
		}
	}
}

void Creature::CastAttack(const SkillData& skill)
{
	SetAction(ACTION_ATTACK_TRY, skill.anim.length);
	m_action_time = 0.f;

	m_cast_skill = &skill;
	m_cast_serial += 1;
	m_window_index = 0;
	m_window_open = false;
	m_window_victims.clear();
	m_cast_power = m_stat.attack;

	StartMotion(skill.anim, m_face_x, m_face_y);
	StartCooldown(skill);
}

void Creature::CancelCast()
{
	m_cast_skill = nullptr;
	m_window_open = false;
	m_window_victims.clear();
	m_cast_serial += 1;
}

void Creature::StartMotion(const AnimationData& motion, float dir_x, float dir_y)
{
	if (motion.movements.empty())
	{
		m_motion = nullptr;
		return;
	}

	if (Normalize(&dir_x, &dir_y) == false)
	{
		dir_x = m_face_x;
		dir_y = m_face_y;
	}

	m_motion = &motion;
	m_motion_time = 0.f;
	m_motion_dir_x = dir_x;
	m_motion_dir_y = dir_y;
}

void Creature::StopMotion()
{
	m_motion = nullptr;
	m_motion_time = 0.f;
}

void Creature::InitGuard(float gage_max)
{
	m_guard.gage_max = gage_max;
	m_guard.gage = gage_max;
	m_guard.recover_delay = 0.f;
}

void Creature::StartGuard(float parry_sec)
{
	SetAction(ACTION_GUARD, 0.f);
	m_action_time = 0.f;
	m_state_timer = 0.f;
	m_parry_timer = parry_sec;
}

void Creature::EndGuard()
{
	m_parry_timer = 0.f;

	if (m_action != ACTION_GUARD)
	{
		return;
	}

	m_action = ACTION_NORMAL;
	m_action_time = 0.f;

	// 가드를 내린 순간부터 회복 대기가 다시 돈다
	m_guard.recover_delay = GetConfig().guard_gage_recover_delay_sec;
}

float Creature::GetGuardRatio() const
{
	if (m_guard.gage_max <= 0.f)
	{
		return 0.f;
	}

	return m_guard.gage / m_guard.gage_max;
}

void Creature::Heal(int32 amount)
{
	if (IsDead() || amount <= 0)
	{
		return;
	}

	m_hp += amount;
	if (m_hp > m_stat.max_hp)
	{
		m_hp = m_stat.max_hp;
	}
}

float Creature::GetHpRatio() const
{
	if (m_stat.max_hp <= 0)
	{
		return 0.f;
	}

	return static_cast<float>(m_hp) / static_cast<float>(m_stat.max_hp);
}

float Creature::GetLockElapsed() const
{
	return m_state_length - m_state_timer;
}

void Creature::TickTimers(float delta_seconds)
{
	m_state_timer = CountDown(m_state_timer, delta_seconds);
	m_parry_timer = CountDown(m_parry_timer, delta_seconds);
	m_freeze_timer = CountDown(m_freeze_timer, delta_seconds);
	m_stun_timer = CountDown(m_stun_timer, delta_seconds);
	m_super_armor_timer = CountDown(m_super_armor_timer, delta_seconds);
	m_hit_react_cool = CountDown(m_hit_react_cool, delta_seconds);
	m_flash = CountDown(m_flash, delta_seconds);
	m_hurt_shake = CountDown(m_hurt_shake, delta_seconds);

	for (OwnedSkill& owned : m_skills)
	{
		owned.cooldown = CountDown(owned.cooldown, delta_seconds);
	}
}

void Creature::TickGuardGage(float delta_seconds)
{
	if (m_guard.gage_max <= 0.f)
	{
		return;
	}

	const GameConfig& config = GetConfig();

	if (m_action == ACTION_GUARD)
	{
		m_guard.gage -= m_guard.gage_max * config.guard_gage_usage_per_sec * delta_seconds;

		// 들고 있는 것만으로 바닥나면 그냥 내려간다. 브레이크는 맞아서 바닥날 때만
		if (m_guard.gage <= 0.f)
		{
			m_guard.gage = 0.f;
			EndGuard();
		}

		return;
	}

	if (m_guard.recover_delay > 0.f)
	{
		m_guard.recover_delay = CountDown(m_guard.recover_delay, delta_seconds);
		return;
	}

	m_guard.gage += m_guard.gage_max * config.guard_gage_recover_per_sec * delta_seconds;
	if (m_guard.gage > m_guard.gage_max)
	{
		m_guard.gage = m_guard.gage_max;
	}
}

void Creature::TickMotion(float delta_seconds)
{
	if (m_motion == nullptr)
	{
		return;
	}

	const float from = m_motion_time;
	const float to = m_motion_time + delta_seconds;
	m_motion_time = to;

	float forward = 0.f;
	float right = 0.f;

	for (const MoveSegment& segment : m_motion->movements)
	{
		const float begin = std::max(from, segment.start_sec);
		const float end = std::min(to, segment.end_sec);
		if (end <= begin)
		{
			continue;
		}

		const float ratio = (end - begin) / (segment.end_sec - segment.start_sec);
		forward += segment.forward * ratio;
		right += segment.right * ratio;
	}

	// 화면 오른쪽이 외적 양수 쪽이다
	const float right_x = -m_motion_dir_y;
	const float right_y = m_motion_dir_x;
	const float move_x = m_motion_dir_x * forward + right_x * right;
	const float move_y = m_motion_dir_y * forward + right_y * right;

	if (std::fabs(move_x) + std::fabs(move_y) > 0.f)
	{
		MoveWithCollision(&m_pos, m_body_radius, move_x, move_y);
	}

	float last_end = 0.f;
	if (m_motion->movements.empty() == false)
	{
		last_end = m_motion->movements.back().end_sec;
	}

	if (m_motion_time >= last_end)
	{
		StopMotion();
	}
}

void Creature::RollWindowDamage(const HitWindow& window)
{
	float damage = static_cast<float>(m_cast_power) * window.damage_rate;

	// 창마다 한 번 굴린다. 이 창에 맞은 대상은 같이 치명타
	m_window_critical = RollPercent() <= m_stat.crit_rate;
	if (m_window_critical)
	{
		damage = damage * static_cast<float>(m_stat.crit_damage) / 100.f;
	}

	m_window_damage = static_cast<int32>(std::lround(damage));
}

bool Creature::CheckWindowHits()
{
	const SkillData& skill = *m_cast_skill;

	HitInfo base;
	base.attacker = this;
	base.skill = &skill;
	base.window_index = m_window_index;
	base.damage = m_window_damage;
	base.penetration = m_stat.penetration;
	base.is_critical = m_window_critical;
	base.origin = m_pos;
	base.dir_x = m_face_x;
	base.dir_y = m_face_y;

	// 판정 목록 밖 대상( NPC )은 자기가 알아서 본다
	OnWindowCheck(base);

	g_candidates.clear();

	for (Creature* creature : g_creatures)
	{
		if (creature == this || creature->IsDead() || creature->m_team == m_team)
		{
			continue;
		}

		if (std::find(m_window_victims.begin(), m_window_victims.end(), creature) != m_window_victims.end())
		{
			continue;
		}

		if (IsInAttackFan(m_pos, m_face_x, m_face_y, skill, creature->m_pos, creature->m_hit_radius) == false)
		{
			continue;
		}

		HitCandidate candidate;
		candidate.creature = creature;

		const float delta_x = GetTileDistanceX(m_pos, creature->m_pos);
		const float delta_y = GetTileDistanceY(m_pos, creature->m_pos);
		candidate.sort_key = delta_x * delta_x + delta_y * delta_y;

		// 휘두르는 방향이 있으면 칼이 먼저 닿는 쪽부터 맞는다
		if (skill.attack_dir == ATTACK_DIR_LEFT_TO_RIGHT || skill.attack_dir == ATTACK_DIR_RIGHT_TO_LEFT)
		{
			float sweep_x = delta_x;
			float sweep_y = delta_y;
			Normalize(&sweep_x, &sweep_y);

			candidate.sort_key = Cross(m_face_x, m_face_y, sweep_x, sweep_y);
			if (skill.attack_dir == ATTACK_DIR_RIGHT_TO_LEFT)
			{
				candidate.sort_key = -candidate.sort_key;
			}
		}

		g_candidates.push_back(candidate);
	}

	if (g_candidates.empty())
	{
		return false;
	}

	std::sort(g_candidates.begin(), g_candidates.end(),
		[](const HitCandidate& left, const HitCandidate& right)
		{
			return left.sort_key < right.sort_key;
		});

	const bool first_attack = m_action != ACTION_ATTACK_SUCCESS;
	bool landed = false;

	// 처리하다 목록이 바뀌어도 안전하게 복사본을 돈다
	const std::vector<HitCandidate> candidates = g_candidates;

	for (const HitCandidate& candidate : candidates)
	{
		Creature* victim = candidate.creature;
		if (victim->IsDead())
		{
			continue;
		}

		HitInfo hit = base;
		if (victim->m_weak_point_rate > 1.f && victim->IsBehind(m_pos))
		{
			hit.damage = static_cast<int32>(std::lround(static_cast<float>(hit.damage) * victim->m_weak_point_rate));
			hit.is_weak_point = true;
		}

		m_window_victims.push_back(victim);

		// 패링당하면 공격이 거기서 끝난다 ( 가드를 뚫는 공격은 패링도 안 된다 )
		if (skill.pierce_guard == false && victim->IsParryable() && victim->m_parry_timer > 0.f)
		{
			ProcessParry(victim, this, hit);
			return true;
		}

		if (victim->IsGuarding())
		{
			if (skill.pierce_guard == false && victim->IsGuardDirValid(m_pos))
			{
				ProcessGuard(victim, this, hit);
				landed = true;
				continue;
			}

			// 등 뒤나 뚫는 공격이면 가드가 풀리고 그대로 맞는다
			victim->EndGuard();
		}

		ProcessHit(victim, this, hit);
		landed = true;
	}

	if (landed && first_attack && IsAttacking())
	{
		m_action = ACTION_ATTACK_SUCCESS;
	}

	return false;
}

void Creature::ProcessHitWindows()
{
	if (m_cast_skill == nullptr || IsAttacking() == false)
	{
		return;
	}

	const std::vector<HitWindow>& windows = m_cast_skill->hit_windows;

	while (m_window_index < static_cast<int32>(windows.size()))
	{
		const HitWindow& window = windows[static_cast<size_t>(m_window_index)];

		if (m_action_time < window.start_sec)
		{
			return;
		}

		// 창이 바뀌면 다시 맞는다
		if (m_window_open == false)
		{
			m_window_open = true;
			m_window_victims.clear();
			RollWindowDamage(window);
			NotifyWindowOpen(*this, m_window_index);
		}

		const int32 serial = m_cast_serial;
		if (CheckWindowHits())
		{
			return;
		}

		// 판정 중에 죽었거나 다른 공격으로 바뀌었다
		if (serial != m_cast_serial || m_cast_skill == nullptr)
		{
			return;
		}

		if (m_action_time <= window.end_sec)
		{
			return;
		}

		m_window_open = false;
		m_window_index += 1;
	}
}

void Creature::TickCombat(float delta_seconds)
{
	TickTimers(delta_seconds);

	if (IsDead())
	{
		m_action_time += delta_seconds;
		return;
	}

	TickGuardGage(delta_seconds);

	m_action_time += delta_seconds;

	TickMotion(delta_seconds);
	ProcessHitWindows();

	if (IsDead())
	{
		return;
	}

	// 가드는 손을 뗄 때까지, 나머지는 잠금이 끝나면 풀린다
	if (m_action != ACTION_NORMAL && m_action != ACTION_GUARD && m_state_timer <= 0.f)
	{
		if (IsAttacking() || m_action == ACTION_ATTACK_INTERRUPTED)
		{
			CancelCast();
		}

		m_action = ACTION_NORMAL;
		m_action_time = 0.f;
	}
}

void RegisterCreature(Creature* creature)
{
	if (std::find(g_creatures.begin(), g_creatures.end(), creature) != g_creatures.end())
	{
		return;
	}

	g_creatures.push_back(creature);
}

void UnregisterCreature(Creature* creature)
{
	g_creatures.erase(std::remove(g_creatures.begin(), g_creatures.end(), creature), g_creatures.end());

	// 지운 대상을 기억하는 창이 있으면 같은 주소가 재사용될 때 엉뚱하게 걸러진다
	for (Creature* other : g_creatures)
	{
		other->m_window_victims.erase(std::remove(other->m_window_victims.begin(), other->m_window_victims.end(), creature), other->m_window_victims.end());
	}
}

void ClearCreatures()
{
	g_creatures.clear();
}

const std::vector<Creature*>& GetCreatures()
{
	return g_creatures;
}

int32 CalcTakenDamage(const Creature& victim, int32 damage, int32 penetration)
{
	if (damage <= 0)
	{
		return 0;
	}

	int32 defence = victim.m_stat.defence - penetration;
	if (defence < 0)
	{
		defence = 0;
	}

	// 감소율이 방어력 / ( 상수 + 방어력 ) 이라 남는 비율은 상수 / ( 상수 + 방어력 )
	const float constant = GetConfig().defence_constant;
	const float remain_rate = constant / (constant + static_cast<float>(defence));
	const int32 taken = static_cast<int32>(std::lround(static_cast<float>(damage) * remain_rate));

	if (taken < 1)
	{
		return 1;
	}

	return taken;
}

bool IsInAttackFan(const WorldPos& origin, float dir_x, float dir_y, const SkillData& skill, const WorldPos& target, float target_radius)
{
	const float delta_x = GetTileDistanceX(origin, target);
	const float delta_y = GetTileDistanceY(origin, target);
	const float distance_sq = delta_x * delta_x + delta_y * delta_y;
	const float reach = skill.range + target_radius;

	if (distance_sq > reach * reach)
	{
		return false;
	}

	// 붙어 있으면 각도가 안 나온다. 몸이 겹칠 만큼 가까우면 맞는다
	if (distance_sq < target_radius * target_radius)
	{
		return true;
	}

	float target_x = delta_x;
	float target_y = delta_y;
	Normalize(&target_x, &target_y);

	// 대상 반경만큼 공격 축 쪽으로 당긴 점 ( 몸 가장자리가 걸치면 맞는다 )
	float pull_x = dir_x - (dir_x * target_x + dir_y * target_y) * target_x;
	float pull_y = dir_y - (dir_x * target_x + dir_y * target_y) * target_y;
	float check_x = target_x;
	float check_y = target_y;

	if (Normalize(&pull_x, &pull_y))
	{
		check_x = delta_x + pull_x * target_radius;
		check_y = delta_y + pull_y * target_radius;
		Normalize(&check_x, &check_y);
	}

	const float cos_check = GetSideCos(skill, Cross(dir_x, dir_y, check_x, check_y));
	if (dir_x * check_x + dir_y * check_y >= cos_check)
	{
		return true;
	}

	const float cos_target = GetSideCos(skill, Cross(dir_x, dir_y, target_x, target_y));
	if (dir_x * target_x + dir_y * target_y >= cos_target)
	{
		return true;
	}

	float half_x = check_x + target_x;
	float half_y = check_y + target_y;
	if (Normalize(&half_x, &half_y))
	{
		const float cos_half = GetSideCos(skill, Cross(dir_x, dir_y, half_x, half_y));
		if (dir_x * half_x + dir_y * half_y >= cos_half)
		{
			return true;
		}
	}

	return false;
}

float GetWindowProgress(const Creature& creature, int32* window_index)
{
	*window_index = -1;

	if (creature.m_cast_skill == nullptr)
	{
		return 0.f;
	}

	const std::vector<HitWindow>& windows = creature.m_cast_skill->hit_windows;
	const float time = creature.m_action_time;

	for (size_t index = 0; index < windows.size(); ++index)
	{
		const HitWindow& window = windows[index];
		*window_index = static_cast<int32>(index);

		if (time < window.start_sec)
		{
			return 0.f;
		}

		if (time <= window.end_sec)
		{
			const float span = std::max(window.end_sec - window.start_sec, 0.01f);
			return (time - window.start_sec) / span;
		}
	}

	return 1.f;
}
