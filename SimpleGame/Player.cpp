#include "stdafx.h"
#include "Player.h"
#include "Audio.h"
#include "CombatDraw.h"
#include "Feel.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Npc.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
	const float PI = 3.14159265f;

	const float ATTACK_CANCEL_PAD = 0.04f;	// 마지막 창이 닫힌 뒤 이만큼 지나면 다음 행동을 받는다
	const float GUARD_BUFFER_SEC = 0.15f;
	const float SKILL_BUFFER_SEC = 0.45f;
	const float DODGE_BUFFER_SEC = 0.2f;
	const float NOTICE_SEC = 2.f;
	const float LEVEL_UP_GLOW_SEC = 1.4f;
	const float DASH_TRAIL_STEP = 0.03f;
	const float PLAYER_CHEST_LIFT = 11.f * static_cast<float>(PIXEL_SCALE);	// 칼 궤적 높이 ( 창 픽셀 ). 시트 그림의 손 높이
	const float DEATH_ANIM_SEC = 0.9f;
	const float PLAYER_SWORD_REACH = 1.05f;	// 칼 궤적 반지름 ( 칸 ). 판정 사거리 끝에 그리면 칼에서 떨어진 점선처럼 보였다
	const float SPIN_TRAIL_SCALE = 0.6f;
	const float SPIN_SWORD_ANGLE = 95.f * 3.14159265f / 180.f;	// 회전 그림은 칼을 몸 오른쪽으로 뻗고 돈다 ( tools/art/player.py spin_pose )
	const float SILHOUETTE_FLASH = 2.f;	// 그림 셰이더 실루엣 모드
	const float LEVEL_UP_VFX_LIFT = 24.f * static_cast<float>(PIXEL_SCALE);	// 빛기둥 밑동이 발에 오게 ( 그림 높이 절반 - 밑동 여백 )
	const float HEAL_VFX_LIFT = 16.f * static_cast<float>(PIXEL_SCALE);
	const float SKILL_RANGE_PAD = 0.3f;	// NPC 판정은 몸 반경만큼 넉넉히

	Player g_player;

	float CountDown(float value, float delta_seconds)
	{
		const float next = value - delta_seconds;

		if (next < 0.f)
		{
			return 0.f;
		}

		return next;
	}

	float GetCancelPoint(const SkillData& skill)
	{
		if (skill.hit_windows.empty())
		{
			return skill.anim.length;
		}

		return skill.hit_windows.back().end_sec + ATTACK_CANCEL_PAD;
	}

	bool IsPastCancelPoint()
	{
		if (g_player.IsAttacking() == false || g_player.m_cast_skill == nullptr)
		{
			return false;
		}

		return g_player.m_action_time >= GetCancelPoint(*g_player.m_cast_skill);
	}

	bool IsParrySuccessFree()
	{
		if (g_player.m_action != ACTION_PARRY_SUCCESS)
		{
			return false;
		}

		return g_player.GetLockElapsed() >= GetConfig().player_parry_success_lock_sec;
	}

	// 지금 다른 행동을 새로 시작해도 되나. 공격 중엔 창이 닫힌 뒤부터 ( 서버 IsStateProcessing )
	bool CanStartAction()
	{
		if (g_player.CanAct() == false)
		{
			return false;
		}

		if (g_player.m_action == ACTION_NORMAL)
		{
			return true;
		}

		if (IsPastCancelPoint())
		{
			return true;
		}

		return IsParrySuccessFree();
	}

	int32 GetComboStepOf(int32 skill_id)
	{
		const std::vector<int32>& combo = GetComboSkillIds();

		for (size_t index = 0; index < combo.size(); ++index)
		{
			if (combo[index] == skill_id)
			{
				return static_cast<int32>(index);
			}
		}

		return -1;
	}

	void ShowNotice(const std::wstring& text)
	{
		AddDamageText(g_player.m_pos, PLAYER_CHEST_LIFT + 34.f, text, DAMAGE_STYLE_INFO);
		PlaySfx(SFX_UI_ERROR, 0.6f);
	}

	void FaceAim()
	{
		g_player.Face(g_player.m_aim_x, g_player.m_aim_y);
	}

	void StartAttackSkill(const SkillData& skill)
	{
		g_player.EndGuard();
		g_player.CancelCast();
		g_player.StopMotion();
		FaceAim();

		g_player.CastAttack(skill);
		g_player.m_attack_buffer = 0.f;

		const int32 step = GetComboStepOf(skill.skill_id);
		g_player.m_combo_step = step;
		g_player.m_combo_reset_timer = skill.anim.length + GetConfig().combo_reset_sec;

		if (step == 3 || skill.skill_type == SKILL_TYPE_PARRY_ATTACK || step < 0)
		{
			PlaySfx(SFX_SWING_HEAVY, 0.75f);
		}
		else
		{
			PlaySfx(SFX_SWING_LIGHT, 0.7f);
		}
	}

	void StartComboStep(int32 step)
	{
		const std::vector<int32>& combo = GetComboSkillIds();
		if (combo.empty())
		{
			return;
		}

		if (step < 0 || step >= static_cast<int32>(combo.size()) || g_player.HasSkill(combo[static_cast<size_t>(step)]) == false)
		{
			step = 0;
		}

		StartAttackSkill(GetSkillData(combo[static_cast<size_t>(step)]));
	}

	int32 GetNextComboStep()
	{
		const std::vector<int32>& combo = GetComboSkillIds();

		if (g_player.m_combo_reset_timer <= 0.f || g_player.m_combo_step < 0)
		{
			return 0;
		}

		const int32 next = g_player.m_combo_step + 1;
		if (next >= static_cast<int32>(combo.size()))
		{
			return 0;
		}

		return next;
	}

	bool TryStartAttack()
	{
		if (g_player.CanAct() == false)
		{
			return false;
		}

		if (g_player.HasWeapon() == false)
		{
			g_player.m_no_weapon_notice = NOTICE_SEC;
			ShowNotice(L"무기를 장착하세요");
			return true;
		}

		// 패링 성공 창 안의 좌클릭은 반격이다
		if (g_player.m_action == ACTION_PARRY_SUCCESS)
		{
			const SkillData& counter = GetSkillData(109);

			if (counter.skill_id != SKILL_ID_NONE && g_player.HasSkill(counter.skill_id) && g_player.IsSkillReady(counter.skill_id))
			{
				StartAttackSkill(counter);
				return true;
			}

			if (IsParrySuccessFree())
			{
				StartComboStep(0);
				return true;
			}

			return false;
		}

		if (g_player.m_action == ACTION_NORMAL)
		{
			StartComboStep(GetNextComboStep());
			return true;
		}

		if (IsPastCancelPoint())
		{
			StartComboStep(GetNextComboStep());
			return true;
		}

		return false;
	}

	bool TryStartParry()
	{
		const SkillData& parry = GetSkillData(103);
		if (parry.skill_id == SKILL_ID_NONE || g_player.HasSkill(parry.skill_id) == false)
		{
			return true;
		}

		// 연속 패링 - 성공 창 안에서는 잠금과 상관없이 다시 든다
		const bool re_parry = g_player.m_action == ACTION_PARRY_SUCCESS && g_player.CanAct();
		if (re_parry == false && CanStartAction() == false)
		{
			return false;
		}

		if (g_player.IsSkillReady(parry.skill_id) == false)
		{
			return false;
		}

		if (g_player.m_guard.gage <= 0.f)
		{
			return true;
		}

		g_player.CancelCast();
		g_player.StopMotion();
		FaceAim();
		g_player.StartGuard(parry.parry_sec);
		g_player.StartCooldown(parry);
		g_player.m_combo_step = -1;
		g_player.m_attack_buffer = 0.f;

		return true;
	}

	void UpdateGuard()
	{
		if (g_player.m_action != ACTION_GUARD)
		{
			return;
		}

		FaceAim();

		// 톡 치고 떼도 패링 판정은 끝까지 산다. 패링은 타이밍이지 버티기가 아니다
		if (g_player.m_guard_held == false && g_player.m_parry_timer <= 0.f)
		{
			g_player.EndGuard();
		}
	}

	bool TryStartSkill(int32 skill_id)
	{
		const SkillData& skill = GetSkillData(skill_id);
		if (skill.skill_id == SKILL_ID_NONE || g_player.HasSkill(skill_id) == false)
		{
			return true;
		}

		if (g_player.IsSkillReady(skill_id) == false || CanStartAction() == false)
		{
			return false;
		}

		StartAttackSkill(skill);
		return true;
	}

	bool TryStartDodge(int32 skill_id)
	{
		const SkillData& skill = GetSkillData(skill_id);
		if (skill.skill_id == SKILL_ID_NONE || g_player.HasSkill(skill_id) == false)
		{
			return true;
		}

		// 대쉬는 진행 중인 스킬을 끊고 들어간다. 경직 · 스턴 중엔 못 쓴다
		if (g_player.CanAct() == false || g_player.IsSkillReady(skill_id) == false)
		{
			return false;
		}

		float dir_x = g_player.m_move_input_x;
		float dir_y = g_player.m_move_input_y;
		if (std::fabs(dir_x) + std::fabs(dir_y) < 0.001f)
		{
			dir_x = g_player.m_aim_x;
			dir_y = g_player.m_aim_y;
		}

		g_player.EndGuard();
		g_player.CancelCast();
		g_player.StopMotion();
		g_player.Face(dir_x, dir_y);
		g_player.SetAction(ACTION_DASH, skill.anim.length);
		g_player.m_action_time = 0.f;
		g_player.m_dash_skill_id = skill_id;
		g_player.StartMotion(skill.anim, dir_x, dir_y);
		g_player.StartCooldown(skill);
		g_player.m_combo_step = -1;
		g_player.m_attack_buffer = 0.f;
		g_player.m_skill_buffer = 0.f;
		g_player.m_dash_trail.fill(g_player.m_pos);
		g_player.m_dash_trail_timer = 0.f;

		SpawnDust(g_player.m_pos, 8, 0.35f, 0.32f, 0.3f);
		SpawnVfx(VFX_DUST, g_player.m_pos, 6.f, 0.f, 1.f);
		PlaySfx(SFX_DODGE, 0.8f);

		return true;
	}

	void UpdateBuffers()
	{
		if (g_player.m_dodge_buffer > 0.f)
		{
			if (TryStartDodge(g_player.m_dodge_buffer_id))
			{
				g_player.m_dodge_buffer = 0.f;
			}
		}

		if (g_player.m_skill_buffer > 0.f)
		{
			if (TryStartSkill(g_player.m_skill_buffer_id))
			{
				g_player.m_skill_buffer = 0.f;
			}
		}

		if (g_player.m_guard_buffer > 0.f)
		{
			if (TryStartParry())
			{
				g_player.m_guard_buffer = 0.f;
			}
		}

		if (g_player.m_attack_buffer > 0.f && g_player.m_action != ACTION_GUARD)
		{
			if (TryStartAttack())
			{
				g_player.m_attack_buffer = 0.f;
			}
		}
	}

	void UpdateMovement(float delta_seconds)
	{
		const bool has_input = std::fabs(g_player.m_move_input_x) + std::fabs(g_player.m_move_input_y) > 0.001f;

		// 창이 닫힌 공격과 패링 성공 뒤는 걸어서 끊을 수 있다
		if (has_input && g_player.CanAct())
		{
			if (IsPastCancelPoint())
			{
				g_player.CancelCast();
				g_player.StopMotion();
				g_player.SetAction(ACTION_NORMAL, 0.f);
			}
			else if (IsParrySuccessFree())
			{
				g_player.SetAction(ACTION_NORMAL, 0.f);
			}
		}

		const bool can_walk = g_player.CanMove() && (g_player.m_action == ACTION_NORMAL || g_player.m_action == ACTION_GUARD);
		if (can_walk)
		{
			FaceAim();
		}

		if (can_walk == false || has_input == false)
		{
			g_player.m_idle_timer += delta_seconds;
			return;
		}

		float speed = GetConfig().player_move_speed;
		if (g_player.m_action == ACTION_GUARD)
		{
			speed *= GetConfig().guard_move_speed_rate;
		}

		const WorldPos before = g_player.m_pos;
		MoveWithCollision(&g_player.m_pos, g_player.m_body_radius, g_player.m_move_input_x * speed * delta_seconds, g_player.m_move_input_y * speed * delta_seconds);

		const float moved_x = GetTileDistanceX(before, g_player.m_pos);
		const float moved_y = GetTileDistanceY(before, g_player.m_pos);
		const float moved = std::sqrt(moved_x * moved_x + moved_y * moved_y);

		if (moved > 0.0001f)
		{
			g_player.m_walk_distance += moved;
			g_player.m_idle_timer = 0.f;
		}
	}

	void UpdateDashTrail(float real_seconds)
	{
		if (g_player.m_action != ACTION_DASH)
		{
			return;
		}

		g_player.m_dash_trail_timer -= real_seconds;
		if (g_player.m_dash_trail_timer > 0.f)
		{
			return;
		}

		g_player.m_dash_trail_timer = DASH_TRAIL_STEP;

		for (size_t index = g_player.m_dash_trail.size() - 1; index > 0; --index)
		{
			g_player.m_dash_trail[index] = g_player.m_dash_trail[index - 1];
		}

		g_player.m_dash_trail[0] = g_player.m_pos;
	}

	// 지금 상태에 맞는 시트 동작을 고르고 그 안에서 몇 번째 장인지 정한다
	void UpdatePose()
	{
		const int32 sheet = g_player.m_sheet;
		const int32 dir = GetDir8(g_player.m_face_x, g_player.m_face_y);
		const float time = g_player.m_action_time;

		float ratio = 0.f;
		if (g_player.m_state_length > 0.f)
		{
			ratio = time / g_player.m_state_length;
		}

		float seconds = g_player.m_idle_timer;
		int32 anim = GetSheetAnim(sheet, ANIM_IDLE);
		const bool is_moving = g_player.m_idle_timer < 0.1f;

		switch (g_player.m_action)
		{
			case ACTION_DEATH:
			{
				anim = GetSheetAnim(sheet, ANIM_DEATH);
				ratio = time / DEATH_ANIM_SEC;
				break;
			}
			case ACTION_DAMAGED:
			{
				if (g_player.m_stun_timer > 0.f)
				{
					anim = GetSheetAnim(sheet, ANIM_STUN);
					seconds = time;
				}
				else
				{
					// 연달아 맞으면 같은 상태라 흐른 시간이 안 돌아간다. 남은 잠금으로 잰다
					anim = GetSheetAnim(sheet, ANIM_HIT);
					ratio = 1.f - g_player.m_state_timer / std::max(g_player.m_state_length, 0.01f);
				}

				break;
			}
			case ACTION_DASH:
			{
				const int32 dash = GetSheetSkillAnim(sheet, g_player.m_dash_skill_id);
				if (dash >= 0)
				{
					anim = dash;
				}

				break;
			}
			case ACTION_GUARD:
			{
				anim = GetSheetAnim(sheet, ANIM_GUARD);

				if (g_player.m_parry_timer > 0.f)
				{
					anim = GetSheetAnim(sheet, ANIM_PARRY);
				}
				else if (is_moving)
				{
					anim = GetSheetAnim(sheet, ANIM_GUARD_WALK);
				}

				break;
			}
			case ACTION_PARRY_SUCCESS:
			{
				anim = GetSheetAnim(sheet, ANIM_PARRY_SUCCESS);
				break;
			}
			case ACTION_ATTACK_INTERRUPTED:
			{
				anim = GetSheetAnim(sheet, ANIM_PARRIED);
				break;
			}
			case ACTION_ATTACK_TRY:
			case ACTION_ATTACK_SUCCESS:
			{
				if (g_player.m_cast_skill != nullptr)
				{
					const int32 skill_anim = GetSheetSkillAnim(sheet, g_player.m_cast_skill->skill_id);
					if (skill_anim >= 0)
					{
						anim = skill_anim;
						ratio = time / std::max(g_player.m_cast_skill->anim.length, 0.01f);
						seconds = time;
					}
				}

				break;
			}
			default:
			{
				// 걸은 거리로 넘겨야 속도가 달라도 발이 안 미끄러진다
				if (is_moving)
				{
					anim = GetSheetAnim(sheet, ANIM_RUN);
				}

				break;
			}
		}

		g_player.m_frame = PickSheetFrame(sheet, anim, dir, ratio, seconds, g_player.m_walk_distance);
	}

	void Respawn()
	{
		g_player.m_pos = g_player.m_spawn_pos;
		g_player.m_action = ACTION_NORMAL;
		g_player.m_action_time = 0.f;
		g_player.m_state_timer = 0.f;
		g_player.m_freeze_timer = 0.f;
		g_player.m_stun_timer = 0.f;
		g_player.m_parry_timer = 0.f;
		g_player.m_hit_react_cool = 0.f;
		g_player.m_respawn_timer = 0.f;
		g_player.CancelCast();
		g_player.StopMotion();
		g_player.RecalcStat();
		g_player.m_hp = g_player.m_stat.max_hp;
		g_player.m_guard.gage = g_player.m_guard.gage_max;
		g_player.m_guard.recover_delay = 0.f;
		g_player.m_combo_step = -1;

		// 쿨은 일부러 안 건드린다 ( 서버 Revive - 죽어도 유지 )
		std::cout << "[ Player - Respawn ] : back to town\n";
	}

	void ShowLevelUp(int32 gained_hp)
	{
		g_player.m_level_up_glow = LEVEL_UP_GLOW_SEC;

		AddDamageText(g_player.m_pos, PLAYER_CHEST_LIFT + 40.f, L"레벨 업!", DAMAGE_STYLE_BANNER_GOOD);
		SpawnRing(g_player.m_pos, 2.2f, 1.6f, 1.4f, 0.7f, 0.6f);
		SpawnVfx(VFX_LEVEL_UP, g_player.m_pos, LEVEL_UP_VFX_LIFT, 0.f, 1.5f);
		SpawnSparks(g_player.m_pos, 10.f, 0.f, 1.f, 30, 220.f, 1.2f, 1.8f, 1.6f, 0.9f, 0.8f);
		AddScreenFlash(1.f, 0.9f, 0.6f, 0.18f);
		PlaySfx(SFX_LEVEL_UP, 0.9f);

		if (gained_hp > 0)
		{
			g_player.Heal(gained_hp);
		}
	}

	ScreenPos GetHandPos(const ScreenPos& foot, float side)
	{
		const float unit = static_cast<float>(PIXEL_SCALE);

		ScreenPos hand;
		hand.x = foot.x + side * 3.f * unit;
		hand.y = foot.y + 10.5f * unit;

		return hand;
	}

	float GetFacingScreenAngle(const Creature& creature)
	{
		return WorldAngleToScreenAngle(GetWorldAngle(creature.m_face_x, creature.m_face_y));
	}
}

bool Player::IsParryable() const
{
	return true;
}

bool Player::HasWeapon() const
{
	return m_inventory.IsEmptySlot(m_inventory.GetEquip(EQUIP_AREA_WEAPON)) == false;
}

void Player::ApplyHitReact(const HitInfo& hit)
{
	const GameConfig& config = GetConfig();
	const SkillData& skill = *hit.skill;

	// 스턴이 없는 공격만 경직. 경직 쿨 안이면 데미지만 받고 계속 움직인다
	if (skill.stun_sec <= 0.f && m_hit_react_cool <= 0.f)
	{
		m_freeze_timer = config.hit_freeze_sec;
		m_hit_react_cool = config.player_hit_freeze_cool_sec;
	}

	float lock = std::max(m_freeze_timer, m_stun_timer);

	// 밀리는 방향은 공격자 반대쪽. 이동 기준 앞을 공격자 쪽으로 잡고 데이터의 음수 앞으로 민다
	const float to_x = GetTileDistanceX(m_pos, hit.origin);
	const float to_y = GetTileDistanceY(m_pos, hit.origin);

	if (skill.knockback.movements.empty() == false)
	{
		StartMotion(skill.knockback, to_x, to_y);
		lock = std::max(lock, skill.knockback.length);
	}
	else if (skill.hit_movement.movements.empty() == false)
	{
		StartMotion(skill.hit_movement, to_x, to_y);
	}

	if (lock > 0.f)
	{
		CancelCast();
		EndGuard();
		SetAction(ACTION_DAMAGED, lock);
		m_combo_step = -1;
		m_attack_buffer = 0.f;
	}
}

void Player::OnDamaged(const HitInfo& hit, int32 taken)
{
	Creature::OnDamaged(hit, taken);
}

void Player::OnParry(const HitInfo& hit)
{
	// 성공하면 패링 쿨이 돈다. 연속 패링이 여기서 나온다
	ResetCooldownOfType(SKILL_TYPE_PARRY);

	if (hit.attacker != nullptr)
	{
		FaceToward(hit.attacker->m_pos);
	}

	StopMotion();
	m_combo_step = -1;
}

void Player::OnDeath(Creature* killer)
{
	m_respawn_timer = GetConfig().player_respawn_sec;
	m_guard_held = false;

	AddShake(0.6f);
	AddScreenFlash(0.6f, 0.f, 0.f, 0.5f);
	AddSlowMotion(0.6f, 0.3f);
	PlaySfx(SFX_PLAYER_DIE, 1.f);

	std::cout << "[ Player - OnDeath ] : player died\n";
}

void Player::OnWindowCheck(const HitInfo& hit)
{
	// 창 하나에 한 번만 NPC 를 본다
	if (m_npc_hit_serial == m_cast_serial && m_npc_hit_window == hit.window_index)
	{
		return;
	}

	m_npc_hit_serial = m_cast_serial;
	m_npc_hit_window = hit.window_index;

	const SkillData& skill = *hit.skill;
	const float half_angle = std::max(skill.angle_left, skill.angle_right);
	const float half_cos = std::cos(std::min(half_angle, 180.f) * PI / 180.f);

	HitHostileNpcsInFan(hit.origin, hit.dir_x, hit.dir_y, skill.range + SKILL_RANGE_PAD, half_cos, hit.damage);
}

void Player::RecalcStat()
{
	const GameConfig& config = GetConfig();
	const int32 level_up_count = m_level - 1;

	CombatStat stat;
	stat.max_hp = config.base_max_hp + config.level_max_hp * level_up_count + config.point_max_hp * m_stat_invest[static_cast<size_t>(STAT_MAX_HP)];
	stat.attack = config.base_attack + config.level_attack * level_up_count + config.point_attack * m_stat_invest[static_cast<size_t>(STAT_ATTACK)];
	stat.defence = config.base_defence + config.level_defence * level_up_count + config.point_defence * m_stat_invest[static_cast<size_t>(STAT_DEFENCE)];

	// 치명타 · 관통력 · 강인함은 레벨과 포인트로 안 오른다
	stat.crit_rate = config.base_crit_rate;
	stat.crit_damage = config.base_crit_damage;
	stat.penetration = config.base_penetration;
	stat.toughness = config.base_toughness;

	m_inventory.AddEquipStat(&stat);
	m_stat = stat;

	if (m_hp > m_stat.max_hp)
	{
		m_hp = m_stat.max_hp;
	}
}

bool Player::AddExp(int32 exp)
{
	m_exp += exp;

	bool level_up = false;
	const int32 before_max_hp = m_stat.max_hp;

	// 한 번에 여러 레벨 오를 수 있다
	while (true)
	{
		const int32 need_exp = GetNeedExp(m_level);

		if (need_exp <= 0)
		{
			m_exp = 0;
			break;
		}

		if (m_exp < need_exp)
		{
			break;
		}

		m_exp -= need_exp;
		m_level += 1;
		m_stat_point += GetConfig().level_up_stat_point;
		m_skill_point += GetConfig().level_up_skill_point;
		level_up = true;
	}

	if (level_up)
	{
		RecalcStat();

		// 레벨업으로 오른 최대 체력만큼 현재 체력도 오른다 ( 기획서 스탯 )
		ShowLevelUp(m_stat.max_hp - before_max_hp);

		std::cout << "[ Player - AddExp ] : level up " << m_level << "\n";
	}

	return level_up;
}

bool Player::TryStatUp(int32 stat)
{
	if (m_stat_point <= 0 || stat < 0 || stat >= STAT_COUNT)
	{
		return false;
	}

	m_stat_invest[static_cast<size_t>(stat)] += 1;
	m_stat_point -= 1;

	const int32 before_max_hp = m_stat.max_hp;
	RecalcStat();

	// 찍어서 올린 최대치만큼 현재 값도 같이 올린다 ( 서버 TryStatUp )
	if (stat == STAT_MAX_HP)
	{
		Heal(m_stat.max_hp - before_max_hp);
	}

	return true;
}

int32 Player::CheckUnlockSkill(int32 skill_id) const
{
	if (GetSkillData(skill_id).skill_id == SKILL_ID_NONE)
	{
		return UNLOCK_UNKNOWN;
	}

	if (HasSkill(skill_id))
	{
		return UNLOCK_ALREADY_OWNED;
	}

	// 이어진 스킬 중 하나라도 가졌어야 찍을 수 있다
	bool is_linked = false;
	for (int32 link_id : GetSkillLinks(skill_id))
	{
		if (HasSkill(link_id))
		{
			is_linked = true;
			break;
		}
	}

	if (is_linked == false)
	{
		return UNLOCK_NO_LINK;
	}

	if (m_skill_point <= 0)
	{
		return UNLOCK_NO_POINT;
	}

	return UNLOCK_OK;
}

bool Player::TryUnlockSkill(int32 skill_id)
{
	if (CheckUnlockSkill(skill_id) != UNLOCK_OK)
	{
		return false;
	}

	AddSkill(skill_id);
	m_skill_point -= 1;

	std::cout << "[ Player - TryUnlockSkill ] : skill " << skill_id << " unlocked\n";

	return true;
}

namespace
{
	// 대쉬 속도선. 몸 뒤로 가늘고 긴 줄 넷이 끌린다
	void DrawDashStreaks(Renderer* renderer, const WorldPos& camera, const Player& player)
	{
		const ScreenPos now = WorldToScreen(player.m_pos, camera);
		const ScreenPos tail = WorldToScreen(player.m_dash_trail.back(), camera);

		float dir_x = now.x - tail.x;
		float dir_y = now.y - tail.y;
		const float length = std::sqrt(dir_x * dir_x + dir_y * dir_y);
		if (length < 1.f)
		{
			return;
		}

		dir_x /= length;
		dir_y /= length;

		const float unit = static_cast<float>(PIXEL_SCALE);
		const float side_x = -dir_y;
		const float side_y = dir_x;
		const float progress = std::min(player.m_action_time / std::max(player.m_state_length, 0.01f), 1.f);
		const float strength = 1.f - progress * progress;

		const std::array<float, 4> offsets = { -7.f, -2.f, 3.f, 8.f };
		const std::array<float, 4> heights = { 6.f, 15.f, 10.f, 19.f };
		const std::array<float, 4> lengths = { 1.1f, 1.5f, 0.9f, 1.3f };

		renderer->SetPixelTriangles(true);

		for (size_t index = 0; index < offsets.size(); ++index)
		{
			const float base_x = now.x + side_x * offsets[index] * unit - dir_x * 4.f * unit;
			const float base_y = now.y + side_y * offsets[index] * unit - dir_y * 4.f * unit + heights[index] * unit;
			const float reach = (length + 10.f * unit) * lengths[index];
			const float half = unit * 0.6f;

			ColorVertex head_a;
			head_a.x = base_x + side_x * half;
			head_a.y = base_y + side_y * half;
			ColorVertex head_b;
			head_b.x = base_x - side_x * half;
			head_b.y = base_y - side_y * half;
			ColorVertex tail_v;
			tail_v.x = base_x - dir_x * reach;
			tail_v.y = base_y - dir_y * reach;

			for (ColorVertex* vertex : { &head_a, &head_b, &tail_v })
			{
				vertex->r = 1.4f;
				vertex->g = 2.2f;
				vertex->b = 2.6f;
			}

			head_a.a = 0.95f * strength;
			head_b.a = 0.95f * strength;
			tail_v.a = 0.f;

			renderer->DrawTriangle(head_a, head_b, tail_v);
		}

		renderer->SetPixelTriangles(false);
	}

	// 회전베기 궤적. 머리를 그림 속 칼 각도에 붙인다 - 따로 놀면 칼과 궤적이 엇갈려 보인다
	void DrawSpinTrail(Renderer* renderer, const WorldPos& camera, const Player& player)
	{
		const SkillData* skill = player.m_cast_skill;
		if (skill == nullptr || player.IsAttacking() == false || skill->attack_dir != ATTACK_DIR_NONE || skill->hit_windows.empty())
		{
			return;
		}

		const float facing = GetWorldAngle(player.m_face_x, player.m_face_y);
		const float radius = skill->range * SPIN_TRAIL_SCALE;
		const float time = player.m_action_time;
		const HitWindow& first = skill->hit_windows.front();
		const HitWindow& last = skill->hit_windows.back();

		const int32 anim = GetSheetSkillAnim(player.m_sheet, skill->skill_id);
		const float period = GetSheetAnimPeriod(player.m_sheet, anim);

		if (period > 0.f)
		{
			// 계속 도는 회전베기 - 두 갈래 소용돌이
			const float fade_in = std::min(std::max((time - first.start_sec + 0.08f) / 0.12f, 0.f), 1.f);
			const float fade_out = std::min(std::max((last.end_sec + 0.15f - time) / 0.15f, 0.f), 1.f);
			const float strength = std::min(fade_in, fade_out);
			if (strength <= 0.f)
			{
				return;
			}

			const float turns = time / period;
			const float head = facing + SPIN_SWORD_ANGLE + (turns - std::floor(turns)) * 2.f * PI;
			const float tail_span = 2.6f;

			DrawSlashCrescent(renderer, camera, player.m_pos, PLAYER_CHEST_LIFT, head - tail_span, head, radius, radius * 0.4f, 3.f, 1.8f, 0.7f, strength, 0.f, 1.f);
			DrawSlashCrescent(renderer, camera, player.m_pos, PLAYER_CHEST_LIFT, head + PI - tail_span, head + PI, radius * 0.86f, radius * 0.32f, 2.6f, 1.3f, 0.5f, strength * 0.8f, 0.f, 1.f);
			DrawBladeGlint(renderer, camera, player.m_pos, PLAYER_CHEST_LIFT, head, radius, strength);
			return;
		}

		// 한 바퀴 회전 - 판정 창 동안 그림이 한 바퀴 돈다
		if (time < first.start_sec)
		{
			return;
		}

		const float span = std::max(last.end_sec - first.start_sec, 0.01f);
		const float progress = std::min((time - first.start_sec) / span, 1.f);
		float strength = 1.f;
		if (time > last.end_sec)
		{
			strength = 1.f - std::min((time - last.end_sec) / 0.16f, 1.f);
		}

		if (strength <= 0.f)
		{
			return;
		}

		const float head = facing + SPIN_SWORD_ANGLE + progress * 2.f * PI;
		const float tail_span = std::min(progress * 2.f, 1.f) * 4.f;

		DrawSlashCrescent(renderer, camera, player.m_pos, PLAYER_CHEST_LIFT, head - tail_span, head, radius, radius * 0.5f, 0.8f, 1.6f, 3.f, strength, 0.f, 1.f);
		DrawBladeGlint(renderer, camera, player.m_pos, PLAYER_CHEST_LIFT, head, radius, strength);
	}
}

void Player::Draw(Renderer* renderer, const WorldPos& camera) const
{
	const ScreenPos foot = WorldToScreen(m_pos, camera);
	const float unit = static_cast<float>(PIXEL_SCALE);

	// 대쉬 잔상. 반투명 대신 청록 단색 실루엣을 점으로 솎는다 - 멀수록 성기다
	if (m_action == ACTION_DASH)
	{
		DrawDashStreaks(renderer, camera, *this);

		// 셋만. 가까운 것은 흰 청록으로 꽉 차고, 멀수록 짙은 파랑으로 식으며 성기다
		const std::array<std::array<float, 4>, 3> ghost_colors = { {
			{ 1.6f, 2.5f, 2.9f, 0.95f },
			{ 0.6f, 1.6f, 2.5f, 0.7f },
			{ 0.25f, 0.7f, 1.9f, 0.45f },
		} };

		for (size_t index = ghost_colors.size(); index > 0; --index)
		{
			const size_t trail_index = std::min(index - 1, m_dash_trail.size() - 1);
			const ScreenPos ghost = WorldToScreen(m_dash_trail[trail_index], camera);
			const std::array<float, 4>& color = ghost_colors[index - 1];
			DrawSheetFrame(renderer, m_sheet, m_frame, ghost.x, ghost.y, color[0], color[1], color[2], color[3], SILHOUETTE_FLASH);
		}
	}

	// 맞은 직후 몸이 잘게 떨린다
	float body_x = foot.x;
	if (m_hurt_shake > 0.f)
	{
		body_x += std::sin(m_hurt_shake * 120.f) * unit;
	}

	float glow = m_shade;
	float r = glow;
	float g = glow;
	float b = glow;

	if (m_action == ACTION_PARRY_SUCCESS)
	{
		r = 1.5f;
		g = 1.35f;
		b = 0.9f;
	}

	if (m_level_up_glow > 0.f)
	{
		const float ratio = m_level_up_glow / LEVEL_UP_GLOW_SEC;
		r += ratio * 0.8f;
		g += ratio * 0.7f;
		b += ratio * 0.3f;
	}

	float flash = 0.f;
	if (m_flash > 0.f)
	{
		flash = std::min(m_flash / 0.12f, 1.f);
	}

	DrawSheetFrame(renderer, m_sheet, m_frame, body_x, foot.y, r, g, b, 1.f, flash);

	if (IsDead() || HasWeapon() == false)
	{
		return;
	}

	// 궤적과 막는 호도 도트로 끊어 그린다
	renderer->SetPixelTriangles(true);

	// 막는 쪽을 보여 준다. 판정 각도( guard angle_half )와 같은 호
	if (m_action == ACTION_GUARD)
	{
		const float facing = GetWorldAngle(m_face_x, m_face_y);
		const float half = std::acos(GetConfig().guard_angle_half_cos);

		float strength = 0.35f;
		float arc_r = 0.5f;
		float arc_g = 0.8f;
		float arc_b = 1.4f;
		if (m_parry_timer > 0.f)
		{
			strength = 0.9f;
			arc_r = 1.6f;
			arc_g = 1.9f;
			arc_b = 2.4f;
		}

		DrawWorldArc(renderer, camera, m_pos, PLAYER_CHEST_LIFT * 0.4f, facing - half, facing + half, 0.45f, 0.7f, arc_r, arc_g, arc_b, strength, strength);
	}

	// 칼 궤적. 각도는 판정 부채꼴, 반지름은 칼끝. 푸른 흰빛 ( 반격은 금빛 )
	float trail_r = 0.8f;
	float trail_g = 1.6f;
	float trail_b = 3.f;
	float reach_scale = 1.f;
	if (m_cast_skill != nullptr)
	{
		reach_scale = PLAYER_SWORD_REACH / std::max(m_cast_skill->range, 0.1f);

		if (m_cast_skill->skill_type == SKILL_TYPE_PARRY_ATTACK)
		{
			trail_r = 3.2f;
			trail_g = 2.4f;
			trail_b = 0.9f;
		}
	}

	DrawSwingTrail(renderer, camera, *this, PLAYER_CHEST_LIFT, reach_scale, trail_r, trail_g, trail_b);
	DrawSpinTrail(renderer, camera, *this);

	renderer->SetPixelTriangles(false);
}

void InitPlayer(const WorldPos& spawn_pos)
{
	g_player = Player();
	g_player.m_team = TEAM_PLAYER;
	g_player.m_sprite = SPRITE_PLAYER;
	g_player.m_sheet = SHEET_PLAYER;
	g_player.m_body_radius = std::min(GetConfig().player_body_radius, 0.45f);
	g_player.m_hit_radius = GetConfig().player_hit_radius;

	const PlayerPreset& preset = GetPlayerPreset();
	g_player.m_level = std::max(1, preset.level);
	g_player.m_exp = preset.exp;
	g_player.m_gold = preset.gold;

	g_player.m_inventory.Init(GetConfig().inventory_slot_count);
	for (const PresetItem& item : preset.items)
	{
		g_player.m_inventory.AddItem(item.item_id, item.count);
	}

	// 기본 스킬은 SkillTree.json 에 한 번만 적고 여기서 붙인다
	g_player.InitSkills(GetBasicSkillIds());
	g_player.RecalcStat();
	g_player.m_hp = g_player.m_stat.max_hp;

	PlacePlayer(spawn_pos);
	RegisterCreature(&g_player);
}

void PlacePlayer(const WorldPos& spawn_pos)
{
	g_player.m_spawn_pos = spawn_pos;
	Respawn();
	RegisterCreature(&g_player);
}

const Player& GetPlayer()
{
	return g_player;
}

Player& MutablePlayer()
{
	return g_player;
}

bool IsPlayerDead()
{
	return g_player.IsDead();
}

void UpdatePlayer(float world_seconds, float real_seconds)
{
	g_player.m_no_weapon_notice = CountDown(g_player.m_no_weapon_notice, real_seconds);
	g_player.m_level_up_glow = CountDown(g_player.m_level_up_glow, real_seconds);

	if (g_player.IsDead())
	{
		g_player.TickCombat(world_seconds);
		g_player.m_respawn_timer -= real_seconds;

		if (g_player.m_respawn_timer <= 0.f)
		{
			Respawn();
		}

		UpdatePose();
		return;
	}

	g_player.m_potion_cooldown = CountDown(g_player.m_potion_cooldown, world_seconds);
	g_player.m_attack_buffer = CountDown(g_player.m_attack_buffer, world_seconds);
	g_player.m_guard_buffer = CountDown(g_player.m_guard_buffer, world_seconds);
	g_player.m_skill_buffer = CountDown(g_player.m_skill_buffer, world_seconds);
	g_player.m_dodge_buffer = CountDown(g_player.m_dodge_buffer, world_seconds);
	g_player.m_combo_reset_timer = CountDown(g_player.m_combo_reset_timer, world_seconds);

	g_player.TickCombat(world_seconds);

	if (g_player.IsDead())
	{
		UpdatePose();
		return;
	}

	UpdateGuard();
	UpdateBuffers();
	UpdateMovement(world_seconds);
	UpdateDashTrail(real_seconds);
	UpdatePose();
}

void SetPlayerMoveInput(float screen_right, float screen_up)
{
	const float length = std::sqrt(screen_right * screen_right + screen_up * screen_up);
	if (length < 0.0001f)
	{
		g_player.m_move_input_x = 0.f;
		g_player.m_move_input_y = 0.f;
		return;
	}

	// 화면에서 먼저 길이를 1 로 맞춰야 대각선이 더 빠르지 않다.
	// 화면 오른쪽은 월드 ( +1, -1 ), 화면 위는 월드 ( -1, -1 )
	const float dir_right = screen_right / length;
	const float dir_up = screen_up / length;

	float world_x = dir_right - dir_up;
	float world_y = -dir_right - dir_up;
	const float world_length = std::sqrt(world_x * world_x + world_y * world_y);

	g_player.m_move_input_x = world_x / world_length;
	g_player.m_move_input_y = world_y / world_length;
}

void SetPlayerAim(const WorldPos& cursor_world)
{
	const float delta_x = GetTileDistanceX(g_player.m_pos, cursor_world);
	const float delta_y = GetTileDistanceY(g_player.m_pos, cursor_world);
	const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);

	// 커서가 발밑이면 방향이 안 나온다. 직전 방향 유지
	if (length < 0.05f)
	{
		return;
	}

	g_player.m_aim_x = delta_x / length;
	g_player.m_aim_y = delta_y / length;
}

void PressPlayerAttack()
{
	if (g_player.IsDead())
	{
		return;
	}

	if (TryStartAttack() == false)
	{
		g_player.m_attack_buffer = GetConfig().combo_buffer_sec;
	}
}

void SetPlayerGuard(bool held)
{
	const bool pressed = held && g_player.m_guard_held == false;
	g_player.m_guard_held = held;

	if (pressed == false || g_player.IsDead())
	{
		return;
	}

	if (TryStartParry() == false)
	{
		g_player.m_guard_buffer = GUARD_BUFFER_SEC;
	}
}

void PressPlayerSkill(int32 skill_id)
{
	const SkillData& skill = GetSkillData(skill_id);
	if (skill.skill_id == SKILL_ID_NONE || g_player.IsDead())
	{
		return;
	}

	if (g_player.HasSkill(skill_id) == false)
	{
		ShowNotice(L"스킬 트리에서 해금하세요 ( K )");
		return;
	}

	if (g_player.HasWeapon() == false)
	{
		g_player.m_no_weapon_notice = NOTICE_SEC;
		ShowNotice(L"무기를 장착하세요");
		return;
	}

	if (g_player.IsSkillReady(skill_id) == false)
	{
		PlaySfx(SFX_UI_ERROR, 0.4f);
		return;
	}

	// 콤보 창이 닫히기 전에 눌렀으면 기억해 뒀다가 닫히는 순간 낸다
	if (TryStartSkill(skill_id) == false)
	{
		g_player.m_skill_buffer_id = skill_id;
		g_player.m_skill_buffer = SKILL_BUFFER_SEC;
	}
}

void PressPlayerDodge(int32 skill_id)
{
	if (g_player.IsDead())
	{
		return;
	}

	// 경직이 풀리는 순간이나 쿨이 도는 순간 나가게 잠깐 기억한다
	if (TryStartDodge(skill_id) == false)
	{
		g_player.m_dodge_buffer_id = skill_id;
		g_player.m_dodge_buffer = DODGE_BUFFER_SEC;
	}
}

bool UsePlayerPotion()
{
	const int32 slot_idx = g_player.m_inventory.FindFirst(ITEM_ID_HP_POTION);
	if (slot_idx < 0)
	{
		ShowNotice(L"체력 물약이 없다");
		return false;
	}

	return UseBagItem(slot_idx);
}

bool UseBagItem(int32 slot_idx)
{
	if (g_player.IsDead())
	{
		return false;
	}

	const ItemSlot& slot = g_player.m_inventory.GetBag(slot_idx);
	if (g_player.m_inventory.IsEmptySlot(slot))
	{
		return false;
	}

	const ItemData& item = GetItemData(slot.item_id);

	if (item.category == ITEM_CATEGORY_EQUIPMENT)
	{
		if (g_player.m_inventory.Equip(slot_idx) == false)
		{
			return false;
		}

		g_player.RecalcStat();
		PlaySfx(SFX_EQUIP, 0.8f);
		return true;
	}

	if (item.category != ITEM_CATEGORY_CONSUMABLE)
	{
		return false;
	}

	// 죽음 · 경직 · 스턴 중엔 못 마신다 ( 서버 UseItem )
	if (g_player.CanAct() == false)
	{
		return false;
	}

	if (g_player.m_potion_cooldown > 0.f)
	{
		PlaySfx(SFX_UI_ERROR, 0.4f);
		return false;
	}

	if (g_player.m_hp >= g_player.m_stat.max_hp)
	{
		ShowNotice(L"체력이 가득 찼다");
		return false;
	}

	for (const ItemEffect& effect : item.effects)
	{
		if (effect.type == ITEM_EFFECT_HEAL_HP)
		{
			const int32 before = g_player.m_hp;
			g_player.Heal(effect.value);
			AddDamageText(g_player.m_pos, PLAYER_CHEST_LIFT + 16.f, L"+" + std::to_wstring(g_player.m_hp - before), DAMAGE_STYLE_HEAL);
		}
	}

	g_player.m_potion_cooldown = item.cooldown_sec;
	g_player.m_inventory.Consume(slot_idx);

	SpawnSparks(g_player.m_pos, 8.f, 0.f, 1.f, 14, 90.f, 1.6f, 0.4f, 1.6f, 0.5f, 0.7f);
	SpawnVfx(VFX_HEAL, g_player.m_pos, HEAL_VFX_LIFT, 0.f, 1.4f);
	PlaySfx(SFX_POTION, 0.8f);

	return true;
}

bool UnequipPlayerItem(int32 area, int32 slot_idx)
{
	if (g_player.m_inventory.Unequip(area, slot_idx) == false)
	{
		return false;
	}

	g_player.RecalcStat();
	PlaySfx(SFX_EQUIP, 0.6f);

	return true;
}

bool GivePlayerItem(int32 item_id, int32 count)
{
	return g_player.m_inventory.AddItem(item_id, count);
}

void AddPlayerGold(int32 gold)
{
	if (gold <= 0)
	{
		return;
	}

	g_player.m_gold += gold;
}

void GrantKillReward(int32 exp, int32 monster_level)
{
	g_player.m_kill_count += 1;

	if (exp <= 0)
	{
		return;
	}

	const int32 rate = GetLevelDiffExpRate(monster_level, g_player.m_level);
	const int32 gained = std::max(1, exp * rate / 100);

	AddDamageText(g_player.m_pos, PLAYER_CHEST_LIFT + 26.f, L"+" + std::to_wstring(gained) + L" 경험치", DAMAGE_STYLE_INFO);
	g_player.AddExp(gained);
}

void DamagePlayer(int32 damage)
{
	if (g_player.IsDead() || damage <= 0)
	{
		return;
	}

	int32 raw = damage;
	if (g_player.IsGuarding())
	{
		raw = static_cast<int32>(static_cast<float>(damage) * GetConfig().guard_damage_rate);
	}

	const int32 taken = CalcTakenDamage(g_player, raw, 0);

	g_player.m_hp -= taken;
	g_player.m_flash = 0.12f;
	g_player.m_hurt_shake = 0.18f;

	AddDamageText(g_player.m_pos, PLAYER_CHEST_LIFT + 14.f, L"-" + std::to_wstring(taken), DAMAGE_STYLE_PLAYER);
	AddShake(0.25f);
	PlaySfx(SFX_PLAYER_HURT, 0.7f);

	if (g_player.m_hp <= 0)
	{
		g_player.m_hp = 0;
		g_player.SetAction(ACTION_DEATH, 0.f);
		g_player.CancelCast();
		g_player.StopMotion();
		g_player.OnDeath(nullptr);
	}
}

int32 TakePlayerGold(int32 amount)
{
	int32 taken = amount;
	if (taken > g_player.m_gold)
	{
		taken = g_player.m_gold;
	}

	g_player.m_gold -= taken;

	return taken;
}

int32 GetPlayerAttack()
{
	return g_player.m_stat.attack;
}

WorldPos GetPlayerPos()
{
	return g_player.m_pos;
}

int32 GetPlayerHp()
{
	return g_player.m_hp;
}

int32 GetPlayerGold()
{
	return g_player.m_gold;
}

int32 GetPlayerLevel()
{
	return g_player.m_level;
}

const wchar_t* GetStatName(int32 stat)
{
	if (stat == STAT_MAX_HP)
	{
		return L"체력";
	}

	if (stat == STAT_ATTACK)
	{
		return L"공격력";
	}

	return L"방어력";
}

int32 GetLevelDiffExpRate(int32 monster_level, int32 player_level)
{
	// 기획서 경험치 - 레벨 차이 보정 ( 최대 120 퍼센트 )
	const int32 diff = monster_level - player_level;

	if (diff >= 8)
	{
		return 50;
	}

	if (diff >= 6)
	{
		return 120;
	}

	if (diff >= 3)
	{
		return 110;
	}

	if (diff >= 1)
	{
		return 105;
	}

	if (diff == 0)
	{
		return 100;
	}

	if (diff >= -2)
	{
		return 90;
	}

	if (diff >= -5)
	{
		return 70;
	}

	if (diff >= -7)
	{
		return 40;
	}

	return 20;
}
