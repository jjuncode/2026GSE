#pragma once

#include "Actor.h"
#include "GameData.h"

#include <vector>

// 서버 Creature 와 Cluster 히트 판정을 한 프로세스로 옮긴 것.
// 서버는 시각( tick )을 들고 비교하지만 여기선 남은 시간을 깎는다 - 히트스톱으로 시간이 멈춰도 어긋나지 않게
constexpr int32 TEAM_PLAYER = 0;
constexpr int32 TEAM_MONSTER = 1;

constexpr int32 ACTION_NORMAL = 0;
constexpr int32 ACTION_ATTACK_TRY = 1;
constexpr int32 ACTION_ATTACK_SUCCESS = 2;	// 이번 공격이 한 번이라도 맞았다
constexpr int32 ACTION_ATTACK_INTERRUPTED = 3;	// 패링당함
constexpr int32 ACTION_GUARD = 4;
constexpr int32 ACTION_PARRY_SUCCESS = 5;
constexpr int32 ACTION_DAMAGED = 6;	// 경직 · 스턴 · 넉백 · 가드 브레이크
constexpr int32 ACTION_DASH = 7;
constexpr int32 ACTION_DEATH = 8;

constexpr int32 HIT_RESULT_HIT = 0;
constexpr int32 HIT_RESULT_GUARD = 1;
constexpr int32 HIT_RESULT_GUARD_BREAK = 2;
constexpr int32 HIT_RESULT_PARRY = 3;

class Creature;

// 창 하나에 굴린 공격. 이 창에 맞은 대상은 같이 치명타다 ( 서버 SkillUser )
struct HitInfo
{
	Creature* attacker = nullptr;
	const SkillData* skill = nullptr;
	int32 window_index = 0;
	int32 damage = 0;	// 방어 전
	int32 penetration = 0;
	bool is_critical = false;
	bool is_weak_point = false;
	WorldPos origin;
	float dir_x = 1.f;
	float dir_y = 0.f;
};

struct OwnedSkill
{
	int32 skill_id = SKILL_ID_NONE;
	float cooldown = 0.f;	// 남은 쿨
};

struct GuardGage
{
	float gage = 0.f;
	float gage_max = 0.f;	// 0 이면 가드 못 함
	float recover_delay = 0.f;	// 이게 다 깎여야 차기 시작
};

class Creature : public Actor
{
public:
	virtual ~Creature();

	// 맞는 쪽
	virtual bool IsParryable() const;
	virtual void ApplyHitReact(const HitInfo& hit);	// 경직 · 스턴 · 넉백
	virtual void OnDamaged(const HitInfo& hit, int32 taken);
	virtual void OnGuarded(const HitInfo& hit, int32 taken, bool is_break);
	virtual void OnParry(const HitInfo& hit);
	virtual void OnDeath(Creature* killer);

	// 때리는 쪽
	virtual void OnParried(const HitInfo& hit);
	virtual void OnHitLanded(const HitInfo& hit, Creature* victim, int32 result);
	virtual void OnWindowCheck(const HitInfo& hit);	// 창이 열려 있는 동안 매 틱. NPC 처럼 판정 목록 밖 대상

	bool IsDead() const;
	bool CanAct() const;	// 죽음 · 경직 · 스턴이 아님
	bool CanMove() const;
	bool IsLocked() const;	// 행동 잠금이 살아 있나
	bool IsGuarding() const;
	bool IsAttacking() const;
	bool IsSuperArmor() const;
	bool IsGuardDirValid(const WorldPos& attacker_pos) const;
	bool IsBehind(const WorldPos& attacker_pos) const;

	void SetAction(int32 action, float lock_sec);	// lock_sec 0 이면 마감 유지
	void Face(float dir_x, float dir_y);
	void FaceToward(const WorldPos& target);

	void InitSkills(const std::vector<int32>& skill_ids);
	void AddSkill(int32 skill_id);
	bool HasSkill(int32 skill_id) const;
	bool IsSkillReady(int32 skill_id) const;	// 가졌고 쿨이 돌았나
	float GetSkillCooldown(int32 skill_id) const;
	void StartCooldown(const SkillData& skill);
	void ResetCooldownOfType(int32 skill_type);

	void CastAttack(const SkillData& skill);	// 잠금 + 루트모션 + 히트 창 예약
	void CancelCast();	// 예약된 히트 창 무효
	void StartMotion(const AnimationData& motion, float dir_x, float dir_y);
	void StopMotion();

	void InitGuard(float gage_max);
	void StartGuard(float parry_sec);
	void EndGuard();
	float GetGuardRatio() const;

	void Heal(int32 amount);
	float GetHpRatio() const;
	float GetLockElapsed() const;	// 지금 상태가 시작된 뒤 흐른 시간

	void TickCombat(float delta_seconds);	// 히트스톱 배율이 이미 곱해진 시간

private:
	void TickTimers(float delta_seconds);
	void TickGuardGage(float delta_seconds);
	void TickMotion(float delta_seconds);
	void ProcessHitWindows();
	void RollWindowDamage(const HitWindow& window);
	bool CheckWindowHits();	// 패링당했으면 true

public:
	int32 m_team = TEAM_MONSTER;
	float m_face_x = 1.f;	// 월드 방향 ( 길이 1 )
	float m_face_y = 0.f;
	float m_body_radius = 0.27f;	// 벽 충돌
	float m_hit_radius = 0.2f;	// 판정
	float m_weak_point_rate = 1.f;

	int32 m_level = 1;
	CombatStat m_stat;
	int32 m_hp = 0;

	int32 m_action = ACTION_NORMAL;
	float m_state_timer = 0.f;	// 잠금 남은 시간
	float m_state_length = 0.f;	// 잠금이 걸릴 때 길이
	float m_action_time = 0.f;	// 지금 행동이 시작된 뒤 흐른 시간

	// 시전 중인 공격
	const SkillData* m_cast_skill = nullptr;
	int32 m_cast_serial = 0;	// 새 공격이 시작되면 오른다. 지난 공격의 창을 걸러낸다
	int32 m_window_index = 0;
	bool m_window_open = false;
	std::vector<const Creature*> m_window_victims;
	int32 m_cast_power = 0;	// 시전 당시 공격력
	int32 m_window_damage = 0;
	bool m_window_critical = false;

	float m_parry_timer = 0.f;	// 패링 판정 남은 시간
	GuardGage m_guard;

	float m_freeze_timer = 0.f;	// 경직
	float m_stun_timer = 0.f;
	float m_super_armor_timer = 0.f;
	float m_hit_react_cool = 0.f;

	// 루트모션 · 피격 이동
	const AnimationData* m_motion = nullptr;
	float m_motion_time = 0.f;
	float m_motion_dir_x = 1.f;
	float m_motion_dir_y = 0.f;

	std::vector<OwnedSkill> m_skills;

	// 연출
	float m_flash = 0.f;	// 피격 흰빛 남은 시간
	float m_hurt_shake = 0.f;	// 맞은 직후 몸 떨림
};

// 판정 목록. 플레이어와 몬스터가 스폰 때 올라온다
void RegisterCreature(Creature* creature);
void UnregisterCreature(Creature* creature);
void ClearCreatures();
const std::vector<Creature*>& GetCreatures();

// 방어력을 감소율로 바꿔 깎는다 ( 기획서 스탯 - 관통력을 먼저 빼고 공식에 넣는다 )
int32 CalcTakenDamage(const Creature& victim, int32 damage, int32 penetration);

// 서버 CheckSkillHit 의 부채꼴. 대상 반경만큼 공격 축 쪽으로 당겨서 본다
bool IsInAttackFan(const WorldPos& origin, float dir_x, float dir_y, const SkillData& skill, const WorldPos& target, float target_radius);

// 공격 연출에서 쓴다. 지금 창 기준 진행 ( 창 전 0, 창 끝 1 )
float GetWindowProgress(const Creature& creature, int32* window_index);
