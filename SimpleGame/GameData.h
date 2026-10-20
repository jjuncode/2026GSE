#pragma once

#include <array>
#include <string>
#include <vector>

// 서버 Data/*.json 과 같은 모양. 거리 cm 와 속도 cm/s 는 읽을 때 칸 단위로 바꿔 둔다
constexpr int32 SKILL_ID_NONE = 0;
constexpr int32 ITEM_ID_NONE = 0;
constexpr int32 MONSTER_TYPE_NONE = 0;

constexpr int32 SKILL_TYPE_ATTACK = 0;
constexpr int32 SKILL_TYPE_PARRY = 1;
constexpr int32 SKILL_TYPE_PARRY_ATTACK = 2;	// 패링 성공 창에서만 나가는 반격
constexpr int32 SKILL_TYPE_DASH = 3;
constexpr int32 SKILL_TYPE_INVALID = 4;

// 칼이 지나간 방향. 피격 반응과 맞는 순서를 정한다
constexpr int32 ATTACK_DIR_NONE = 0;
constexpr int32 ATTACK_DIR_RIGHT_TO_LEFT = 1;
constexpr int32 ATTACK_DIR_LEFT_TO_RIGHT = 2;
constexpr int32 ATTACK_DIR_FRONT = 3;

// 잡몹은 맞을 때마다 경직, 정예와 보스는 피격 쿨이 있다 ( 기획서 잡몹 피격 로직 )
constexpr int32 GRADE_NORMAL = 0;
constexpr int32 GRADE_ELITE = 1;
constexpr int32 GRADE_BOSS = 2;

constexpr int32 ITEM_CATEGORY_ETC = 0;
constexpr int32 ITEM_CATEGORY_EQUIPMENT = 1;
constexpr int32 ITEM_CATEGORY_CONSUMABLE = 2;

constexpr int32 EQUIP_AREA_NONE = -1;
constexpr int32 EQUIP_AREA_WEAPON = 0;
constexpr int32 EQUIP_AREA_COUNT = 1;

constexpr int32 ITEM_EFFECT_HEAL_HP = 0;
constexpr int32 ITEM_EFFECT_COUNT = 1;

constexpr int32 STAT_MAX_HP = 0;
constexpr int32 STAT_ATTACK = 1;
constexpr int32 STAT_DEFENCE = 2;
constexpr int32 STAT_COUNT = 3;

constexpr int32 ITEM_ID_SWORD = 1;
constexpr int32 ITEM_ID_SPELL_TRACE = 2;	// 주문의 흔적
constexpr int32 ITEM_ID_HP_POTION = 3;

struct MoveSegment
{
	float start_sec = 0.f;
	float end_sec = 0.f;
	float forward = 0.f;	// 칸. 음수면 뒤로
	float right = 0.f;
};

struct AnimationData
{
	std::string name;
	float length = 0.f;
	std::vector<MoveSegment> movements;
};

struct HitWindow
{
	float start_sec = 0.f;
	float end_sec = 0.f;
	float damage_rate = 0.f;	// 공격력 배율
};

struct SkillData
{
	int32 skill_id = SKILL_ID_NONE;
	std::string name;
	std::wstring alias;	// 화면에 내는 이름
	std::string input;
	int32 skill_type = SKILL_TYPE_INVALID;
	int32 attack_dir = ATTACK_DIR_NONE;

	float angle_left = 0.f;	// 도
	float angle_right = 0.f;
	float range = 0.f;	// 칸

	float parry_sec = 0.f;
	float guard_gage_max = 0.f;

	std::vector<HitWindow> hit_windows;
	float hit_stop_sec = 0.f;
	float stun_sec = 0.f;
	bool pierce_guard = false;	// 가드도 패링도 안 된다
	float cooldown_sec = 0.f;

	AnimationData anim;	// 시전자 루트모션과 잠금 길이
	AnimationData hit_movement;	// 맞은 쪽이 밀리는 이동 ( 앞 = 공격자 쪽 )
	AnimationData knockback;	// 비어 있으면 넉백 없음

	// 대쉬 전용
	float dash_duration = 0.f;
	float dash_recovery = 0.f;
	float dash_distance = 0.f;	// 칸

	int32 combo_next_skill_id = SKILL_ID_NONE;
};

struct CombatStat
{
	int32 max_hp = 0;
	int32 attack = 0;
	int32 defence = 0;
	int32 crit_rate = 0;	// %
	int32 crit_damage = 0;	// %
	int32 penetration = 0;
	int32 toughness = 0;	// CC 저항 %
};

struct DropInfo
{
	int32 item_id = ITEM_ID_NONE;
	float rate = 0.f;	// %
	int32 min_count = 1;
	int32 max_count = 1;
};

struct MonsterRewardData
{
	int32 exp = 0;
	int32 gold_min = 0;
	int32 gold_max = 0;
	std::vector<DropInfo> drops;	// 줄마다 따로 굴림
};

struct MonsterData
{
	int32 monster_type = MONSTER_TYPE_NONE;
	std::wstring name;
	int32 grade = GRADE_NORMAL;
	int32 level = 1;
	int32 sprite = -1;
	bool has_blade = false;	// 손에 든 칼을 따로 돌려 그린다

	float body_radius = 0.3f;	// 칸
	float hit_radius = 0.2f;
	float move_speed = 0.f;	// 칸/초
	float patrol_speed = 0.f;
	float standoff_speed = 0.f;
	float search_range = 0.f;	// 칸
	float patrol_radius = 0.f;
	float keep_distance = 0.f;
	bool patrols = true;
	bool chases = true;
	bool attacks = true;

	CombatStat stat;
	float attack_cooldown_sec = 0.f;
	float respawn_sec = 0.f;
	std::vector<int32> skill_ids;
	float weak_point_rate = 1.f;	// 등 뒤에서 벤 배율

	MonsterRewardData reward;
};

struct ItemEffect
{
	int32 type = ITEM_EFFECT_HEAL_HP;
	int32 value = 0;
};

struct ItemData
{
	int32 item_id = ITEM_ID_NONE;
	std::wstring name;
	std::wstring desc;
	int32 category = ITEM_CATEGORY_ETC;
	int32 equip_area = EQUIP_AREA_NONE;
	int32 max_stack = 1;
	CombatStat stat;	// 착용하면 더해지는 수치
	float cooldown_sec = 0.f;
	std::vector<ItemEffect> effects;
	int32 icon_sprite = -1;
	int32 drop_sprite = -1;
};

struct PresetItem
{
	int32 slot_idx = 0;
	int32 item_id = ITEM_ID_NONE;
	int32 count = 0;
};

struct PlayerPreset
{
	int32 level = 1;
	int32 exp = 0;
	int32 gold = 0;
	std::vector<PresetItem> items;
};

// Server.config.json 과 같은 묶음. 거리는 칸, 시간은 초
struct GameConfig
{
	float cm_per_tile = 150.f;

	float guard_damage_rate = 0.15f;
	float guard_gage_usage_per_sec = 0.05f;
	float guard_gage_recover_per_sec = 0.1f;
	float guard_gage_recover_delay_sec = 3.f;
	float guard_gage_cost_rate = 0.8f;
	float guard_angle_half_cos = 0.5f;
	float guard_break_sec = 2.2f;
	float guard_move_speed_rate = 0.4f;

	float player_hit_radius = 0.2f;
	float player_body_radius = 0.27f;
	float player_hit_freeze_cool_sec = 0.6f;
	float player_parry_success_sec = 1.f;
	float player_parry_success_lock_sec = 0.3f;
	float player_interrupted_sec = 1.f;
	float player_move_speed = 4.f;
	float player_respawn_sec = 2.f;
	int32 level_up_stat_point = 3;
	int32 level_up_skill_point = 1;
	int32 inventory_slot_count = 20;
	float combo_buffer_sec = 0.4f;
	float combo_reset_sec = 0.5f;

	float audio_master_volume = 0.3f;
	float audio_volume_step = 0.1f;

	float drop_lifetime_sec = 60.f;
	float pickup_range = 1.f;
	float gold_magnet_range = 2.f;

	int32 base_max_hp = 100;
	int32 base_attack = 10;
	int32 base_defence = 0;
	int32 base_crit_rate = 5;
	int32 base_crit_damage = 150;
	int32 base_penetration = 0;
	int32 base_toughness = 0;
	int32 level_max_hp = 10;
	int32 level_attack = 0;
	int32 level_defence = 0;
	int32 point_max_hp = 10;
	int32 point_attack = 1;
	int32 point_defence = 1;

	float monster_return_heal_per_sec = 0.2f;
	float monster_chase_margin = 2.f;
	float monster_standoff_step_deg = 30.f;
	float monster_hit_recovery_sec = 0.4f;
	float elite_hit_react_cool_sec = 2.f;
	float boss_hit_react_cool_sec = 4.f;
	float corpse_sec = 5.f;
	float respawn_min_distance = 12.f;
	float pack_alert_range = 5.f;
	float track_turn_deg_per_sec = 220.f;
	float track_lock_lead_sec = 0.3f;

	float hit_freeze_sec = 0.4f;
	float defence_constant = 100.f;
	int32 toughness_max = 40;

	int32 enhance_max_level = 15;
	std::vector<int32> enhance_success_rate;	// idx = 지금 단계
	int32 enhance_fail_down_from = 6;
	int32 enhance_reset_from = 11;
	int32 enhance_reset_share = 25;
	std::vector<int32> enhance_trace_cost;
	std::vector<int32> enhance_gold_cost;
	std::vector<int32> enhance_attack_bonus;	// idx = 단계
};

bool LoadGameData();

const GameConfig& GetConfig();
const SkillData& GetSkillData(int32 skill_id);	// 없으면 빈 스킬 ( skill_id 0 )
const MonsterData& GetMonsterData(int32 monster_type);
const ItemData& GetItemData(int32 item_id);
const PlayerPreset& GetPlayerPreset();

int32 GetNeedExp(int32 level);	// 0 이면 만렙
int32 GetMaxLevel();

const std::vector<int32>& GetBasicSkillIds();
const std::vector<int32>& GetSkillLinks(int32 skill_id);	// 트리에서 간선이 이어진 스킬
const std::vector<std::array<int32, 2>>& GetSkillTreeEdges();
const std::vector<int32>& GetComboSkillIds();	// 좌클릭 1타부터 순서대로

float CmToTile(float cm);
