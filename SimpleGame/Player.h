#pragma once

#include "Combat.h"
#include "Inventory.h"

#include <array>
#include <string>

constexpr int32 UNLOCK_OK = 0;
constexpr int32 UNLOCK_ALREADY_OWNED = 1;
constexpr int32 UNLOCK_NO_LINK = 2;	// 이어진 스킬을 하나도 안 가졌다
constexpr int32 UNLOCK_NO_POINT = 3;
constexpr int32 UNLOCK_UNKNOWN = 4;

constexpr int32 DASH_TRAIL_COUNT = 4;

class Player : public Creature
{
public:
	void Draw(Renderer* renderer, const WorldPos& camera) const override;

	bool IsParryable() const override;
	void ApplyHitReact(const HitInfo& hit) override;
	void OnDamaged(const HitInfo& hit, int32 taken) override;
	void OnParry(const HitInfo& hit) override;
	void OnDeath(Creature* killer) override;
	void OnWindowCheck(const HitInfo& hit) override;

	void RecalcStat();	// 레벨 · 찍은 포인트 · 장비를 다시 더한다
	bool AddExp(int32 exp);	// 레벨업 했으면 true
	bool TryStatUp(int32 stat);
	int32 CheckUnlockSkill(int32 skill_id) const;
	bool TryUnlockSkill(int32 skill_id);
	bool HasWeapon() const;

public:
	WorldPos m_spawn_pos;
	int32 m_exp = 0;	// 지금 레벨에서 쌓은 경험치
	int32 m_stat_point = 0;
	int32 m_skill_point = 0;
	std::array<int32, STAT_COUNT> m_stat_invest = {};
	int32 m_gold = 0;
	Inventory m_inventory;

	float m_potion_cooldown = 0.f;
	float m_respawn_timer = 0.f;

	// 조작
	float m_move_input_x = 0.f;	// 월드 방향. 길이 0 이면 안 움직인다
	float m_move_input_y = 0.f;
	float m_aim_x = 1.f;
	float m_aim_y = 0.f;
	bool m_guard_held = false;
	float m_attack_buffer = 0.f;	// 좌클릭을 미리 눌러 둔 남은 시간
	float m_guard_buffer = 0.f;
	int32 m_skill_buffer_id = SKILL_ID_NONE;	// 콤보 도중 누른 Q · E · R. 창이 닫히면 나간다
	float m_skill_buffer = 0.f;
	int32 m_dodge_buffer_id = SKILL_ID_NONE;	// 경직이 풀리는 순간 나가는 회피
	float m_dodge_buffer = 0.f;
	int32 m_dash_skill_id = SKILL_ID_NONE;	// 대쉬와 구르기는 그림이 다르다
	int32 m_combo_step = -1;	// 지금 휘두르는 좌클릭 단계
	float m_combo_reset_timer = 0.f;	// 이 시간 안에 다시 누르면 다음 단계
	int32 m_npc_hit_serial = -1;	// NPC 판정을 이미 본 시전 번호와 창
	int32 m_npc_hit_window = -1;

	// 연출
	float m_walk_distance = 0.f;
	float m_idle_timer = 0.f;
	float m_level_up_glow = 0.f;
	float m_no_weapon_notice = 0.f;
	std::array<WorldPos, DASH_TRAIL_COUNT> m_dash_trail;
	float m_dash_trail_timer = 0.f;
	int32 m_kill_count = 0;
};

void InitPlayer(const WorldPos& spawn_pos);
void PlacePlayer(const WorldPos& spawn_pos);	// 성장은 두고 자리만 옮긴다
const Player& GetPlayer();
Player& MutablePlayer();
bool IsPlayerDead();

// 히트스톱이 곱해진 시간으로 판정을, 실제 시간으로 부활 대기와 알림을 돌린다
void UpdatePlayer(float world_seconds, float real_seconds);

// 입력. 화면 축 이동은 갱신마다 새로 넣는다
void SetPlayerMoveInput(float screen_right, float screen_up);
void SetPlayerAim(const WorldPos& cursor_world);
void PressPlayerAttack();
void SetPlayerGuard(bool held);
void PressPlayerSkill(int32 skill_id);	// Q · E · R
void PressPlayerDodge(int32 skill_id);	// 대쉬 · 구르기
bool UsePlayerPotion();	// 단축키 1
bool UseBagItem(int32 slot_idx);	// 소비는 쓰고 장비는 낀다
bool UnequipPlayerItem(int32 area, int32 slot_idx);

// 보상
bool GivePlayerItem(int32 item_id, int32 count);	// 가방이 차면 false
void AddPlayerGold(int32 gold);
void GrantKillReward(int32 exp, int32 monster_level);

// NPC 쪽 창구
void DamagePlayer(int32 damage);
int32 TakePlayerGold(int32 amount);
int32 GetPlayerAttack();
WorldPos GetPlayerPos();
int32 GetPlayerHp();
int32 GetPlayerGold();
int32 GetPlayerLevel();

const wchar_t* GetStatName(int32 stat);
int32 GetLevelDiffExpRate(int32 monster_level, int32 player_level);	// %
