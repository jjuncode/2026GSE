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
constexpr int32 EQUIP_AREA_ARMOR = 1;
constexpr int32 EQUIP_AREA_COUNT = 2;

constexpr int32 ITEM_EFFECT_HEAL_HP = 0;
constexpr int32 ITEM_EFFECT_COUNT = 1;

constexpr int32 STAT_MAX_HP = 0;
constexpr int32 STAT_ATTACK = 1;
constexpr int32 STAT_DEFENCE = 2;
constexpr int32 STAT_COUNT = 3;

constexpr int32 ITEM_ID_SWORD = 1;
constexpr int32 ITEM_ID_SPELL_TRACE = 2;	// 주문의 흔적
constexpr int32 ITEM_ID_HP_POTION = 3;
constexpr int32 ITEM_ID_BIG_HP_POTION = 4;

constexpr int32 SHOP_WEAPON = 0;
constexpr int32 SHOP_ARMOR = 1;
constexpr int32 SHOP_APOTHECARY = 2;
constexpr int32 SHOP_GENERAL = 3;
constexpr int32 SHOP_KIND_COUNT = 4;

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

// 엘리트 보스 페이즈. 체력 비율이 hp_ratio 아래로 내려가면 넘어간다
struct BossPhase
{
	float hp_ratio = 1.f;
	std::wstring name;	// 넘어갈 때 띄우는 이름. 비면 조용히 넘어간다
	float attack_cooldown_rate = 1.f;	// 패턴이 끝난 뒤 쉬는 시간 배율
	std::vector<std::vector<int32>> patterns;	// 기술 순서 묶음. 하나를 골라 끝까지 잇는다
};

// 엘리트 보스 피격 규칙 - 슈퍼아머 게이지. 맞고 패링당할수록 차고, 다 차면 그로기
struct BossRuleData
{
	bool enabled = false;
	int32 art_scale = 1;	// 정수 배율. 도트 칸 크기를 안 흐트린다
	float stagger_max = 100.f;
	float stagger_per_hit = 3.f;
	float stagger_per_parry = 20.f;
	float stagger_regen_per_sec = 5.f;
	float stagger_regen_delay = 3.f;	// 마지막으로 찬 뒤 이만큼 지나야 줄기 시작
	float groggy_sec = 4.f;
	float groggy_damage_rate = 1.5f;
	float pattern_chain_sec = 0.3f;	// 패턴 안 기술 사이 쉼. 짧아야 연속 공격으로 읽힌다
	std::vector<BossPhase> phases;
};

struct MonsterData
{
	int32 monster_type = MONSTER_TYPE_NONE;
	std::wstring name;
	int32 grade = GRADE_NORMAL;
	int32 level = 1;
	int32 sprite = -1;
	bool has_blade = false;	// 손에 든 칼을 따로 돌려 그린다
	bool is_shadow = false;	// 그림자 괴물. 본뜬 몬스터 그림을 어둡게 물들여 그린다
	BossRuleData boss;
	int32 base_type = MONSTER_TYPE_NONE;	// 본뜬 몬스터
	std::wstring desc;

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
	std::string icon_name;	// "ext:" 그림판 이름. 그림판을 읽은 뒤 icon_sprite 로 바꾼다
	int32 price = 0;	// 상점에서 살 때. 팔 땐 sell_rate 를 곱한다
};

constexpr int32 QUEST_TIER_DAILY = 0;	// 일일 - 가까운 잡몹 · 납품 · 순찰
constexpr int32 QUEST_TIER_NORMAL = 1;	// 보상 - 제 레벨 사냥터
constexpr int32 QUEST_TIER_HARD = 2;	// 고난 - 정예 · 깊은 곳
constexpr int32 QUEST_TIER_TRIAL = 3;	// 특급 시련 - 보스 · 레벨 위 정예
constexpr int32 QUEST_TIER_COUNT = 4;

constexpr int32 QUEST_GOAL_HUNT = 0;	// 그 몬스터 N 마리
constexpr int32 QUEST_GOAL_ELITE = 1;	// 정예 아무거나 N 마리
constexpr int32 QUEST_GOAL_DELIVER = 2;	// 아이템 N 개를 게시판에 맡긴다
constexpr int32 QUEST_GOAL_SCOUT = 3;	// 그 거점까지 가 본다
constexpr int32 QUEST_GOAL_BOSS = 4;
constexpr int32 QUEST_GOAL_DUNGEON = 5;	// 그 인스턴스를 끝까지 비운다

struct QuestReward
{
	int32 gold = 0;
	int32 exp = 0;
	int32 trace = 0;	// 주문의 흔적
	int32 item_id = ITEM_ID_NONE;
};

struct QuestTemplate
{
	int32 tier = QUEST_TIER_DAILY;
	int32 goal = QUEST_GOAL_HUNT;
	int32 monster_type = MONSTER_TYPE_NONE;	// HUNT · BOSS
	int32 item_id = ITEM_ID_NONE;	// DELIVER
	int32 site_kind = 0;	// SCOUT ( SITE_ )
	int32 target = 0;	// DUNGEON - 인스턴스 번호
	int32 count_min = 1;
	int32 count_max = 1;
	int32 level = 1;	// 권장 레벨. 사냥 의뢰는 사냥터 몬스터 레벨로 덮는다
	int32 level_bonus = 0;	// 정예를 이만큼 높은 레벨로 띄운다 ( 쉽게 못 밀게 )
	QuestReward reward;
	std::wstring title;
	std::wstring desc;
};

struct QuestConfig
{
	int32 board_size = 10;
	int32 max_active = 5;
	float refresh_sec = 600.f;
	float scout_radius = 4.f;
	std::array<int32, QUEST_TIER_COUNT> tier_slots = { 4, 3, 2, 1 };
	std::array<std::wstring, QUEST_TIER_COUNT> tier_names;
};

// 모험가 등급. 의뢰를 넘길 때마다 공헌이 쌓이고, 다음 등급 기준을 넘으면 승급 심사 의뢰가 붙는다
struct RankData
{
	std::wstring name;
	std::array<float, 3> color = { 1.f, 1.f, 1.f };
	int32 need_points = 0;
	bool has_exam = false;
	QuestTemplate exam;
};

struct RankConfig
{
	std::vector<RankData> ranks;
	std::array<int32, QUEST_TIER_COUNT> tier_points = { 10, 25, 60, 150 };
	std::array<int32, QUEST_TIER_COUNT> tier_min_rank = { 0, 0, 1, 2 };	// 이 등급부터 받는다
};

constexpr int32 INSTANCE_KIND_DUNGEON = 0;
constexpr int32 INSTANCE_KIND_RAID = 1;

struct InstanceSpawn
{
	int32 monster_type = MONSTER_TYPE_NONE;
	int32 count = 1;
};

struct InstanceRoom
{
	std::vector<InstanceSpawn> spawns;
};

// 던전 · 레이드. 구역 구석을 막힌 방 여럿으로 파고, 방을 비울 때마다 다음 문이 열린다
struct InstanceData
{
	std::string id;
	std::wstring name;
	std::wstring desc;
	int32 kind = INSTANCE_KIND_DUNGEON;
	int32 min_rank = 0;
	bool hidden = false;	// 이야기가 드러내기 전엔 입구가 없다
	int32 entrance_site = 2;	// SITE_ 번호. 가장 가까운 그 거점 곁에 입구
	int32 room_width = 16;	// 칸
	int32 room_height = 18;
	std::vector<InstanceRoom> rooms;
	MonsterRewardData reward;
	int32 rank_points = 0;
};

// 컷신 시간표 한 줄. 종류( type )마다 쓰는 칸이 다르다 - 자리는 시작점( anchor )에서 칸 단위
struct CineStep
{
	float time = 0.f;
	std::string type;
	int32 actor = -1;
	int32 target = -1;	// follow 가 따라붙는 배우
	std::string sheet;
	std::string anim;
	std::string style;	// shadow - 어둡게 물들고 보랏빛 테 / silhouette - 단색 그림자
	std::string sound;
	float x = 0.f;
	float y = 0.f;
	float face_x = 1.f;
	float face_y = 0.f;
	float duration = 0.f;
	float from = 0.f;
	float to = 0.f;
	float scale = 1.f;
	std::array<float, 3> color = { 1.f, 1.f, 1.f };
	bool flag = false;	// letterbox · dark 켜기, 등불 들기
	std::wstring text;
	std::wstring sub;
	std::wstring speaker;
};

struct CinematicData
{
	std::string id;
	std::string anchor;	// citadel_street · player
	bool hide_player = false;	// 플레이어가 안 나오는 장면
	float length = 0.f;
	std::vector<CineStep> steps;
};

// 메인 퀘스트 한 단계. 받을 때 intro, 넘길 때 outro 를 자막으로 흘리고, cinematic 이 있으면 넘긴 뒤 튼다
struct StoryStep
{
	QuestTemplate quest;
	std::vector<std::wstring> intro;
	std::vector<std::wstring> outro;
	std::string cinematic;
	std::string reveal;	// 넘긴 뒤 드러나는 인스턴스 ( 레이드 입구 )
};

struct StoryConfig
{
	bool play_opening = true;
	int32 tutorial_kills = 4;
	int32 tutorial_attack_rate = 3;	// 최강자는 세다. 튜토리얼 동안만
	int32 tutorial_hp_rate = 4;
};

constexpr int32 CARD_STAT_ATTACK = 0;
constexpr int32 CARD_STAT_DEFENCE = 1;
constexpr int32 CARD_STAT_MAX_HP = 2;
constexpr int32 CARD_STAT_CRIT_RATE = 3;
constexpr int32 CARD_STAT_CRIT_DAMAGE = 4;
constexpr int32 CARD_STAT_PENETRATION = 5;
constexpr int32 CARD_STAT_TOUGHNESS = 6;

// 몬스터 카드 한 장. 붙은 능력치는 카드 레벨만큼 곱해진다
struct CardData
{
	int32 monster_type = MONSTER_TYPE_NONE;
	float rate = 0.f;	// 처치 한 번에 나올 확률 %
	int32 stat = CARD_STAT_ATTACK;
	int32 value = 0;
};

struct ShopData
{
	int32 kind = SHOP_GENERAL;
	std::wstring name;
	std::wstring greeting;
	std::string sign;	// 간판 그림 ( items.png 이름 )
	std::vector<int32> goods;	// 파는 아이템. 값은 ItemData::price
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
	float player_parry_cooldown_cut_sec = 1.5f;	// 패링 성공마다 도는 공격 스킬 쿨을 깎는다
	float player_interrupted_sec = 1.f;
	float player_move_speed = 4.f;
	float player_respawn_sec = 2.f;
	float player_death_exp_loss_rate = 1.f;	// 죽으면 이 레벨에서 쌓은 경험치를 이만큼 잃는다
	int32 level_up_stat_point = 3;
	int32 level_up_skill_point = 1;
	int32 inventory_slot_count = 20;
	float combo_buffer_sec = 0.4f;
	float combo_reset_sec = 0.5f;

	float audio_master_volume = 0.3f;
	float audio_volume_step = 0.1f;

	float shop_sell_rate = 0.4f;

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
const ShopData& GetShopData(int32 kind);
const QuestConfig& GetQuestConfig();
const std::vector<QuestTemplate>& GetQuestTemplates();
const RankConfig& GetRankConfig();
const std::vector<InstanceData>& GetInstanceDatas();
const CinematicData* FindCinematic(const std::string& id);	// 없으면 nullptr
const std::vector<StoryStep>& GetStorySteps();
const StoryConfig& GetStoryConfig();
const std::vector<CardData>& GetCardDatas();
const std::vector<int32>& GetCardLevelCopies();	// 레벨마다 필요한 누적 장 수

// 아이템 그림판을 읽은 뒤 부른다. "ext:" 아이콘을 그림 번호로 잇는다
void BindItemArt();

int32 GetNeedExp(int32 level);	// 0 이면 만렙
int32 GetMaxLevel();

const std::vector<int32>& GetBasicSkillIds();
const std::vector<int32>& GetSkillLinks(int32 skill_id);	// 트리에서 간선이 이어진 스킬
const std::vector<std::array<int32, 2>>& GetSkillTreeEdges();
const std::vector<int32>& GetComboSkillIds();	// 좌클릭 1타부터 순서대로

float CmToTile(float cm);
