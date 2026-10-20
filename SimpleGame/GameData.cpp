#include "stdafx.h"
#include "GameData.h"
#include "SpriteId.h"

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace
{
	using Json = nlohmann::json;

	constexpr int32 MAX_SKILL_ID = 255;
	constexpr int32 MAX_ITEM_ID = 255;
	constexpr int32 MAX_MONSTER_TYPE = 63;

	const char* DATA_DIR = "./Data/";

	GameConfig g_config;
	std::vector<SkillData> g_skills;	// idx = skill_id
	std::vector<MonsterData> g_monsters;	// idx = monster_type
	std::vector<ItemData> g_items;	// idx = item_id
	std::vector<int32> g_need_exps;	// idx = 레벨
	std::vector<int32> g_basic_skill_ids;
	std::unordered_map<int32, std::vector<int32>> g_skill_links;
	std::vector<std::array<int32, 2>> g_skill_edges;
	std::vector<int32> g_combo_skill_ids;
	PlayerPreset g_preset;

	const std::vector<int32> EMPTY_LINKS;

	std::wstring Utf8ToWide(const std::string& text)
	{
		if (text.empty())
		{
			return std::wstring();
		}

		const int32 length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32>(text.size()), nullptr, 0);
		std::wstring wide(static_cast<size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32>(text.size()), &wide[0], length);

		return wide;
	}

	bool ReadJson(const char* file_name, Json* out)
	{
		const std::string path = std::string(DATA_DIR) + file_name;
		std::ifstream file(path, std::ios::binary);

		if (file.is_open() == false)
		{
			std::cout << "[ GameData - ReadJson ] : cannot open " << path << "\n";
			return false;
		}

		// 메모장으로 고치면 BOM 이 붙는다
		std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (text.size() >= 3 && static_cast<uint8>(text[0]) == 0xEF && static_cast<uint8>(text[1]) == 0xBB && static_cast<uint8>(text[2]) == 0xBF)
		{
			text.erase(0, 3);
		}

		*out = Json::parse(text, nullptr, false);
		if (out->is_discarded())
		{
			std::cout << "[ GameData - ReadJson ] : parse failed " << path << "\n";
			return false;
		}

		return true;
	}

	float ReadFloat(const Json& node, const char* key, float fallback)
	{
		if (node.contains(key) && node[key].is_number())
		{
			return node[key].get<float>();
		}

		return fallback;
	}

	int32 ReadInt(const Json& node, const char* key, int32 fallback)
	{
		if (node.contains(key) && node[key].is_number())
		{
			return static_cast<int32>(std::lround(node[key].get<double>()));
		}

		return fallback;
	}

	std::string ReadString(const Json& node, const char* key)
	{
		if (node.contains(key) && node[key].is_string())
		{
			return node[key].get<std::string>();
		}

		return std::string();
	}

	std::vector<int32> ReadIntArray(const Json& node, const char* key)
	{
		std::vector<int32> values;

		if (node.contains(key) == false || node[key].is_array() == false)
		{
			return values;
		}

		for (const Json& value : node[key])
		{
			if (value.is_number())
			{
				values.push_back(static_cast<int32>(std::lround(value.get<double>())));
			}
		}

		return values;
	}

	// 나쁜 구간은 버린다. 이동량은 cm 라 칸으로 바꾼다
	std::vector<MoveSegment> ReadMoveSegments(const Json& node, const std::string& log_name)
	{
		std::vector<MoveSegment> segments;

		if (node.is_array() == false)
		{
			return segments;
		}

		for (const Json& entry : node)
		{
			MoveSegment segment;
			segment.start_sec = ReadFloat(entry, "start_sec", 0.f);
			segment.end_sec = ReadFloat(entry, "end_sec", 0.f);
			segment.forward = CmToTile(ReadFloat(entry, "forward", 0.f));
			segment.right = CmToTile(ReadFloat(entry, "right", 0.f));

			if (segment.end_sec <= segment.start_sec)
			{
				std::cout << "[ GameData - ReadMoveSegments ] : bad segment in " << log_name << "\n";
				continue;
			}

			segments.push_back(segment);
		}

		std::sort(segments.begin(), segments.end(),
			[](const MoveSegment& left, const MoveSegment& right)
			{
				return left.start_sec < right.start_sec;
			});

		return segments;
	}

	CombatStat ReadStat(const Json& node)
	{
		CombatStat stat;
		stat.max_hp = ReadInt(node, "max_hp", 0);
		stat.attack = ReadInt(node, "attack", 0);
		stat.defence = ReadInt(node, "defence", 0);
		stat.crit_rate = ReadInt(node, "crit_rate", 0);
		stat.crit_damage = ReadInt(node, "crit_damage", 0);
		stat.penetration = ReadInt(node, "penetration", 0);
		stat.toughness = ReadInt(node, "toughness", 0);

		return stat;
	}

	int32 ParseSkillType(const std::string& text)
	{
		if (text == "ATTACK")
		{
			return SKILL_TYPE_ATTACK;
		}

		if (text == "PARRY")
		{
			return SKILL_TYPE_PARRY;
		}

		if (text == "PARRY_ATTACK")
		{
			return SKILL_TYPE_PARRY_ATTACK;
		}

		if (text == "DASH")
		{
			return SKILL_TYPE_DASH;
		}

		return SKILL_TYPE_INVALID;
	}

	int32 ParseAttackDir(const std::string& text)
	{
		if (text == "RIGHT_TO_LEFT")
		{
			return ATTACK_DIR_RIGHT_TO_LEFT;
		}

		if (text == "LEFT_TO_RIGHT")
		{
			return ATTACK_DIR_LEFT_TO_RIGHT;
		}

		if (text == "FRONT")
		{
			return ATTACK_DIR_FRONT;
		}

		return ATTACK_DIR_NONE;
	}

	int32 ParseGrade(const std::string& text)
	{
		if (text == "ELITE")
		{
			return GRADE_ELITE;
		}

		if (text == "BOSS")
		{
			return GRADE_BOSS;
		}

		return GRADE_NORMAL;
	}

	// 한글 분류명은 넓은 문자로 비교한다. 좁은 문자열 상수는 CP949 로 컴파일돼 UTF-8 과 안 맞는다
	int32 ParseItemCategory(const std::string& text)
	{
		const std::wstring wide = Utf8ToWide(text);

		if (text == "EQUIPMENT" || wide == L"장비")
		{
			return ITEM_CATEGORY_EQUIPMENT;
		}

		if (text == "CONSUMABLE" || wide == L"소비")
		{
			return ITEM_CATEGORY_CONSUMABLE;
		}

		return ITEM_CATEGORY_ETC;
	}

	int32 ParseEquipArea(const std::string& text)
	{
		if (text == "EQUIP_WEAPON")
		{
			return EQUIP_AREA_WEAPON;
		}

		return EQUIP_AREA_NONE;
	}

	int32 ParseMonsterSprite(const std::string& text)
	{
		if (text == "HOUND")
		{
			return SPRITE_HOUND;
		}

		if (text == "DEVOURER")
		{
			return SPRITE_DEVOURER;
		}

		if (text == "GATEBREAKER")
		{
			return SPRITE_GATEBREAKER;
		}

		return SPRITE_BLIGHTED;
	}

	// 서버 아이콘 파일 이름을 그대로 두고 여기서 도트 그림으로 잇는다. 데이터 에디터와 같은 파일을 쓰려고
	void ResolveItemSprites(ItemData* item, const std::string& icon)
	{
		if (icon.find("Sword") != std::string::npos)
		{
			item->icon_sprite = SPRITE_ICON_SWORD;
			item->drop_sprite = SPRITE_DROP_SWORD;
			return;
		}

		if (Utf8ToWide(icon).find(L"물약") != std::wstring::npos || icon.find("Potion") != std::string::npos)
		{
			item->icon_sprite = SPRITE_ICON_POTION;
			item->drop_sprite = SPRITE_DROP_POTION;
			return;
		}

		item->icon_sprite = SPRITE_ICON_TRACE;
		item->drop_sprite = SPRITE_DROP_TRACE;
	}

	// 넉백 거리와 시간만 적어 두면 줄어드는 구간 넷으로 펴서 넣는다 ( 앞 = 공격자 쪽이라 음수 )
	AnimationData MakeKnockbackMotion(float distance, float duration)
	{
		AnimationData motion;
		motion.name = "knockback";
		motion.length = duration;

		const std::array<float, 4> shares = { 0.4f, 0.3f, 0.2f, 0.1f };
		const float step = duration / static_cast<float>(shares.size());

		for (size_t index = 0; index < shares.size(); ++index)
		{
			MoveSegment segment;
			segment.start_sec = step * static_cast<float>(index);
			segment.end_sec = step * static_cast<float>(index + 1);
			segment.forward = -distance * shares[index];
			motion.movements.push_back(segment);
		}

		return motion;
	}

	void LoadConfig()
	{
		Json root;
		if (ReadJson("Game.config.json", &root) == false)
		{
			return;
		}

		GameConfig& config = g_config;

		const Json& world = root["world"];
		config.cm_per_tile = ReadFloat(world, "cm_per_tile", config.cm_per_tile);

		const Json& guard = root["guard"];
		config.guard_damage_rate = ReadFloat(guard, "damage_rate", config.guard_damage_rate);
		config.guard_gage_usage_per_sec = ReadFloat(guard, "gage_usage_per_sec", config.guard_gage_usage_per_sec);
		config.guard_gage_recover_per_sec = ReadFloat(guard, "gage_recover_per_sec", config.guard_gage_recover_per_sec);
		config.guard_gage_recover_delay_sec = ReadFloat(guard, "gage_recover_delay_sec", config.guard_gage_recover_delay_sec);
		config.guard_gage_cost_rate = ReadFloat(guard, "gage_cost_rate", config.guard_gage_cost_rate);
		config.guard_angle_half_cos = std::cos(ReadFloat(guard, "angle_half", 60.f) * 3.14159265f / 180.f);
		config.guard_break_sec = ReadFloat(guard, "break_duration_sec", config.guard_break_sec);
		config.guard_move_speed_rate = ReadFloat(guard, "move_speed_rate", config.guard_move_speed_rate);

		const Json& player = root["player"];
		config.player_hit_radius = CmToTile(ReadFloat(player, "hit_radius", 30.f));
		config.player_body_radius = CmToTile(ReadFloat(player, "body_radius", 40.f));
		config.player_hit_freeze_cool_sec = ReadFloat(player, "hit_freeze_cool_sec", config.player_hit_freeze_cool_sec);
		config.player_parry_success_sec = ReadFloat(player, "parry_success_sec", config.player_parry_success_sec);
		config.player_parry_success_lock_sec = ReadFloat(player, "parry_success_lock_sec", config.player_parry_success_lock_sec);
		config.player_interrupted_sec = ReadFloat(player, "interrupted_sec", config.player_interrupted_sec);
		config.player_move_speed = CmToTile(ReadFloat(player, "move_speed", 600.f));
		config.player_respawn_sec = ReadFloat(player, "respawn_sec", config.player_respawn_sec);
		config.level_up_stat_point = ReadInt(player, "level_up_stat_point", config.level_up_stat_point);
		config.level_up_skill_point = ReadInt(player, "level_up_skill_point", config.level_up_skill_point);
		config.inventory_slot_count = ReadInt(player, "inventory_slot_count", config.inventory_slot_count);
		config.combo_buffer_sec = ReadFloat(player, "combo_buffer_sec", config.combo_buffer_sec);
		config.combo_reset_sec = ReadFloat(player, "combo_reset_sec", config.combo_reset_sec);

		const Json& audio = root["audio"];
		config.audio_master_volume = ReadFloat(audio, "master_volume", config.audio_master_volume);
		config.audio_volume_step = ReadFloat(audio, "volume_step", config.audio_volume_step);

		const Json& item = root["item"];
		config.drop_lifetime_sec = ReadFloat(item, "drop_lifetime_sec", config.drop_lifetime_sec);
		config.pickup_range = CmToTile(ReadFloat(item, "pickup_range", 160.f));
		config.gold_magnet_range = CmToTile(ReadFloat(item, "gold_magnet_range", 330.f));

		const Json& stat = root["stat"];
		config.base_max_hp = ReadInt(stat, "base_max_hp", config.base_max_hp);
		config.base_attack = ReadInt(stat, "base_attack", config.base_attack);
		config.base_defence = ReadInt(stat, "base_defence", config.base_defence);
		config.base_crit_rate = ReadInt(stat, "base_crit_rate", config.base_crit_rate);
		config.base_crit_damage = ReadInt(stat, "base_crit_damage", config.base_crit_damage);
		config.base_penetration = ReadInt(stat, "base_penetration", config.base_penetration);
		config.base_toughness = ReadInt(stat, "base_toughness", config.base_toughness);
		config.level_max_hp = ReadInt(stat, "level_max_hp", config.level_max_hp);
		config.level_attack = ReadInt(stat, "level_attack", config.level_attack);
		config.level_defence = ReadInt(stat, "level_defence", config.level_defence);
		config.point_max_hp = ReadInt(stat, "point_max_hp", config.point_max_hp);
		config.point_attack = ReadInt(stat, "point_attack", config.point_attack);
		config.point_defence = ReadInt(stat, "point_defence", config.point_defence);

		const Json& monster = root["monster"];
		config.monster_return_heal_per_sec = ReadFloat(monster, "return_heal_per_sec", config.monster_return_heal_per_sec);
		config.monster_chase_margin = ReadFloat(monster, "chase_margin", config.monster_chase_margin);
		config.monster_standoff_step_deg = ReadFloat(monster, "standoff_step_deg", config.monster_standoff_step_deg);
		config.monster_hit_recovery_sec = ReadFloat(monster, "hit_recovery_sec", config.monster_hit_recovery_sec);
		config.elite_hit_react_cool_sec = ReadFloat(monster, "elite_hit_react_cool_sec", config.elite_hit_react_cool_sec);
		config.boss_hit_react_cool_sec = ReadFloat(monster, "boss_hit_react_cool_sec", config.boss_hit_react_cool_sec);
		config.corpse_sec = ReadFloat(monster, "corpse_sec", config.corpse_sec);
		config.respawn_min_distance = CmToTile(ReadFloat(monster, "respawn_min_distance", 1800.f));
		config.pack_alert_range = CmToTile(ReadFloat(monster, "pack_alert_range", 750.f));
		config.track_turn_deg_per_sec = ReadFloat(monster, "track_turn_deg_per_sec", config.track_turn_deg_per_sec);
		config.track_lock_lead_sec = ReadFloat(monster, "track_lock_lead_sec", config.track_lock_lead_sec);

		const Json& hit = root["hit"];
		config.hit_freeze_sec = ReadFloat(hit, "hit_freeze_sec", config.hit_freeze_sec);
		config.defence_constant = ReadFloat(hit, "defence_equation_constant", config.defence_constant);
		config.toughness_max = ReadInt(hit, "toughness_max", config.toughness_max);

		const Json& enhance = root["enhance"];
		config.enhance_max_level = ReadInt(enhance, "max_level", config.enhance_max_level);
		config.enhance_success_rate = ReadIntArray(enhance, "success_rate");
		config.enhance_fail_down_from = ReadInt(enhance, "fail_down_from", config.enhance_fail_down_from);
		config.enhance_reset_from = ReadInt(enhance, "reset_from", config.enhance_reset_from);
		config.enhance_reset_share = ReadInt(enhance, "reset_share", config.enhance_reset_share);
		config.enhance_trace_cost = ReadIntArray(enhance, "trace_cost");
		config.enhance_gold_cost = ReadIntArray(enhance, "gold_cost");
		config.enhance_attack_bonus = ReadIntArray(enhance, "attack_bonus");
	}

	void LoadAnimations(std::unordered_map<std::string, AnimationData>* animations)
	{
		Json root;
		if (ReadJson("AnimationData.json", &root) == false)
		{
			return;
		}

		for (const Json& entry : root["animations"])
		{
			AnimationData animation;
			animation.name = ReadString(entry, "name");
			animation.length = ReadFloat(entry, "length", 0.f);

			if (entry.contains("movements"))
			{
				animation.movements = ReadMoveSegments(entry["movements"], animation.name);
			}

			(*animations)[animation.name] = animation;
		}
	}

	void LoadSkills(const std::unordered_map<std::string, AnimationData>& animations)
	{
		Json root;
		if (ReadJson("SkillData.json", &root) == false)
		{
			return;
		}

		g_skills.assign(static_cast<size_t>(MAX_SKILL_ID + 1), SkillData());

		for (const Json& entry : root["skills"])
		{
			const int32 skill_id = ReadInt(entry, "skill_id", SKILL_ID_NONE);
			if (skill_id <= SKILL_ID_NONE || skill_id > MAX_SKILL_ID)
			{
				std::cout << "[ GameData - LoadSkills ] : bad skill_id " << skill_id << "\n";
				continue;
			}

			SkillData skill;
			skill.skill_id = skill_id;
			skill.name = ReadString(entry, "name");
			skill.alias = Utf8ToWide(ReadString(entry, "alias"));
			if (skill.alias.empty())
			{
				skill.alias = Utf8ToWide(skill.name);
			}

			skill.input = ReadString(entry, "input");
			skill.skill_type = ParseSkillType(ReadString(entry, "skill_type"));
			skill.attack_dir = ParseAttackDir(ReadString(entry, "attack_dir"));
			skill.angle_left = ReadFloat(entry, "angle_left", 0.f);
			skill.angle_right = ReadFloat(entry, "angle_right", 0.f);
			skill.range = CmToTile(ReadFloat(entry, "range", 0.f));
			skill.parry_sec = ReadFloat(entry, "parry_sec", 0.f);
			skill.guard_gage_max = ReadFloat(entry, "guard_gage_max", 0.f);
			skill.hit_stop_sec = ReadFloat(entry, "hit_stop_sec", 0.f);
			skill.stun_sec = ReadFloat(entry, "stun_sec", 0.f);
			skill.cooldown_sec = ReadFloat(entry, "cooldown_sec", 0.f);
			skill.combo_next_skill_id = ReadInt(entry, "combo_next_skill_id", SKILL_ID_NONE);

			if (entry.contains("pierce_guard") && entry["pierce_guard"].is_boolean())
			{
				skill.pierce_guard = entry["pierce_guard"].get<bool>();
			}

			if (entry.contains("hit_windows") && entry["hit_windows"].is_array())
			{
				for (const Json& window_node : entry["hit_windows"])
				{
					HitWindow window;
					window.start_sec = ReadFloat(window_node, "start_sec", 0.f);
					window.end_sec = ReadFloat(window_node, "end_sec", 0.f);
					window.damage_rate = ReadFloat(window_node, "damage_rate", 0.f);

					if (window.end_sec < window.start_sec)
					{
						std::cout << "[ GameData - LoadSkills ] : bad window in " << skill.name << "\n";
						continue;
					}

					skill.hit_windows.push_back(window);
				}
			}

			const std::string anim_name = ReadString(entry, "anim_id");
			auto found = animations.find(anim_name);
			if (found != animations.end())
			{
				skill.anim = found->second;
			}
			else
			{
				std::cout << "[ GameData - LoadSkills ] : no animation " << anim_name << " for " << skill.name << "\n";
			}

			if (entry.contains("hit_movement"))
			{
				skill.hit_movement.name = "hit_movement";
				skill.hit_movement.movements = ReadMoveSegments(entry["hit_movement"], skill.name);
				if (skill.hit_movement.movements.empty() == false)
				{
					skill.hit_movement.length = skill.hit_movement.movements.back().end_sec;
				}
			}

			if (entry.contains("knockback") && entry["knockback"].is_object())
			{
				const float distance = CmToTile(ReadFloat(entry["knockback"], "distance", 0.f));
				const float duration = ReadFloat(entry["knockback"], "duration_sec", 0.f);

				if (distance > 0.f && duration > 0.f)
				{
					skill.knockback = MakeKnockbackMotion(distance, duration);
				}
			}

			if (entry.contains("client_movement") && entry["client_movement"].is_object())
			{
				const Json& movement = entry["client_movement"];
				skill.dash_duration = ReadFloat(movement, "duration_sec", 0.f);
				skill.dash_recovery = ReadFloat(movement, "recovery_sec", 0.f);
				skill.dash_distance = CmToTile(ReadFloat(movement, "distance", 400.f));

				// 대쉬는 애니 이동 대신 거리 하나로 적는다
				MoveSegment segment;
				segment.start_sec = 0.f;
				segment.end_sec = skill.dash_duration;
				segment.forward = skill.dash_distance;

				skill.anim.movements.clear();
				skill.anim.movements.push_back(segment);
				skill.anim.length = skill.dash_duration + skill.dash_recovery;
			}

			if (skill.anim.length <= 0.f)
			{
				skill.anim.length = 0.5f;
			}

			g_skills[static_cast<size_t>(skill_id)] = skill;
		}
	}

	void LoadMonsters()
	{
		Json root;
		if (ReadJson("MonsterData.json", &root) == false)
		{
			return;
		}

		g_monsters.assign(static_cast<size_t>(MAX_MONSTER_TYPE + 1), MonsterData());

		for (const Json& entry : root["monsters"])
		{
			const int32 monster_type = ReadInt(entry, "monster_type", MONSTER_TYPE_NONE);
			if (monster_type <= MONSTER_TYPE_NONE || monster_type > MAX_MONSTER_TYPE)
			{
				std::cout << "[ GameData - LoadMonsters ] : bad monster_type " << monster_type << "\n";
				continue;
			}

			MonsterData monster;
			monster.monster_type = monster_type;
			monster.name = Utf8ToWide(ReadString(entry, "name"));
			monster.grade = ParseGrade(ReadString(entry, "grade"));
			monster.level = ReadInt(entry, "level", 1);
			monster.sprite = ParseMonsterSprite(ReadString(entry, "sprite"));
			monster.has_blade = ReadString(entry, "weapon") == "CLEAVER";
			monster.body_radius = CmToTile(ReadFloat(entry, "body_radius", 40.f));
			monster.hit_radius = CmToTile(ReadFloat(entry, "hit_radius", 30.f));
			monster.move_speed = CmToTile(ReadFloat(entry, "move_speed", 0.f));
			monster.patrol_speed = CmToTile(ReadFloat(entry, "patrol_speed", 0.f));
			monster.standoff_speed = CmToTile(ReadFloat(entry, "standoff_speed", 0.f));
			monster.search_range = CmToTile(ReadFloat(entry, "search_range", 0.f));
			monster.patrol_radius = CmToTile(ReadFloat(entry, "patrol_radius", 0.f));
			monster.keep_distance = CmToTile(ReadFloat(entry, "keep_distance", 0.f));
			monster.patrols = ReadString(entry, "patrol_policy") == "SPAWN";
			monster.chases = ReadString(entry, "target_policy") != "NONE";
			monster.attacks = ReadString(entry, "skill_policy") != "NONE";
			monster.attack_cooldown_sec = ReadFloat(entry, "attack_cooldown_sec", 0.f);
			monster.respawn_sec = ReadFloat(entry, "respawn_sec", 0.f);
			monster.skill_ids = ReadIntArray(entry, "skill_ids");
			monster.weak_point_rate = ReadFloat(entry, "weak_point_rate", 1.f);

			if (entry.contains("stat"))
			{
				monster.stat = ReadStat(entry["stat"]);
			}

			if (entry.contains("reward"))
			{
				const Json& reward = entry["reward"];
				monster.reward.exp = ReadInt(reward, "exp", 0);

				const std::vector<int32> gold = ReadIntArray(reward, "gold");
				if (gold.size() >= 2)
				{
					monster.reward.gold_min = gold[0];
					monster.reward.gold_max = gold[1];
				}

				if (reward.contains("drops") && reward["drops"].is_array())
				{
					for (const Json& drop_node : reward["drops"])
					{
						DropInfo drop;
						drop.item_id = ReadInt(drop_node, "item_id", ITEM_ID_NONE);
						drop.rate = ReadFloat(drop_node, "rate", 0.f);
						drop.min_count = ReadInt(drop_node, "min_count", 1);
						drop.max_count = ReadInt(drop_node, "max_count", drop.min_count);
						monster.reward.drops.push_back(drop);
					}
				}
			}

			g_monsters[static_cast<size_t>(monster_type)] = monster;
		}
	}

	void LoadItems()
	{
		Json root;
		if (ReadJson("ItemData.json", &root) == false)
		{
			return;
		}

		g_items.assign(static_cast<size_t>(MAX_ITEM_ID + 1), ItemData());

		for (const Json& entry : root["items"])
		{
			const int32 item_id = ReadInt(entry, "item_id", ITEM_ID_NONE);
			if (item_id <= ITEM_ID_NONE || item_id > MAX_ITEM_ID)
			{
				std::cout << "[ GameData - LoadItems ] : bad item_id " << item_id << "\n";
				continue;
			}

			ItemData item;
			item.item_id = item_id;
			item.name = Utf8ToWide(ReadString(entry, "name"));
			item.desc = Utf8ToWide(ReadString(entry, "desc"));
			item.category = ParseItemCategory(ReadString(entry, "category"));
			item.equip_area = ParseEquipArea(ReadString(entry, "equip_area"));
			item.max_stack = std::max(1, ReadInt(entry, "max_stack", 1));
			item.cooldown_sec = ReadFloat(entry, "cooldown_sec", 0.f);

			// 장비칸이 있으면 분류를 안 적어도 장비다 ( 클라 인벤토리와 같은 규칙 )
			if (item.equip_area != EQUIP_AREA_NONE)
			{
				item.category = ITEM_CATEGORY_EQUIPMENT;
			}

			if (entry.contains("stat"))
			{
				item.stat = ReadStat(entry["stat"]);
			}

			if (entry.contains("effects") && entry["effects"].is_array())
			{
				for (const Json& effect_node : entry["effects"])
				{
					if (ReadString(effect_node, "type") != "HEAL_HP")
					{
						std::cout << "[ GameData - LoadItems ] : unknown effect in item " << item_id << "\n";
						continue;
					}

					ItemEffect effect;
					effect.type = ITEM_EFFECT_HEAL_HP;
					effect.value = ReadInt(effect_node, "value", 0);
					item.effects.push_back(effect);
				}
			}

			ResolveItemSprites(&item, ReadString(entry, "icon"));
			g_items[static_cast<size_t>(item_id)] = item;
		}
	}

	void LoadLevels()
	{
		Json root;
		if (ReadJson("LevelTable.json", &root) == false)
		{
			return;
		}

		g_need_exps = ReadIntArray(root, "need_exp");
	}

	void LoadSkillTree()
	{
		Json root;
		if (ReadJson("SkillTree.json", &root) == false)
		{
			return;
		}

		g_basic_skill_ids = ReadIntArray(root, "basic_skill_ids");

		if (root.contains("edges") == false || root["edges"].is_array() == false)
		{
			return;
		}

		for (const Json& edge : root["edges"])
		{
			if (edge.is_array() == false || edge.size() != 2)
			{
				continue;
			}

			const int32 from = edge[0].get<int32>();
			const int32 to = edge[1].get<int32>();

			// 간선은 양방향이다. 어느 쪽을 가졌든 다른 쪽을 찍을 수 있다
			g_skill_links[from].push_back(to);
			g_skill_links[to].push_back(from);
			g_skill_edges.push_back({ from, to });
		}
	}

	void LoadPreset()
	{
		Json root;
		if (ReadJson("NewPlayerPreset.json", &root) == false)
		{
			return;
		}

		g_preset.level = ReadInt(root, "level", 1);
		g_preset.exp = ReadInt(root, "exp", 0);
		g_preset.gold = ReadInt(root, "gold", 0);

		if (root.contains("items") == false || root["items"].is_array() == false)
		{
			return;
		}

		for (const Json& entry : root["items"])
		{
			PresetItem item;
			item.slot_idx = ReadInt(entry, "slot_idx", 0);
			item.item_id = ReadInt(entry, "item_id", ITEM_ID_NONE);
			item.count = ReadInt(entry, "count", 1);
			g_preset.items.push_back(item);
		}
	}

	// 좌클릭 연타 순서. 입력 이름에서 뽑아야 데이터만 고쳐도 콤보가 바뀐다
	void BuildComboChain()
	{
		const std::array<const char*, 4> inputs = { "LEFT_CLICK", "LEFT_CLICK_COMBO_2", "LEFT_CLICK_COMBO_3", "LEFT_CLICK_COMBO_4" };

		for (const char* input : inputs)
		{
			for (const SkillData& skill : g_skills)
			{
				if (skill.skill_id != SKILL_ID_NONE && skill.input == input)
				{
					g_combo_skill_ids.push_back(skill.skill_id);
					break;
				}
			}
		}
	}
}

float CmToTile(float cm)
{
	return cm / g_config.cm_per_tile;
}

bool LoadGameData()
{
	// 거리 변환이 칸 크기를 보므로 설정부터
	LoadConfig();

	std::unordered_map<std::string, AnimationData> animations;
	LoadAnimations(&animations);
	LoadSkills(animations);
	LoadMonsters();
	LoadItems();
	LoadLevels();
	LoadSkillTree();
	LoadPreset();
	BuildComboChain();

	if (g_skills.empty() || g_items.empty() || g_monsters.empty())
	{
		std::cout << "[ GameData - LoadGameData ] : data missing\n";
		return false;
	}

	std::cout << "[ GameData - LoadGameData ] : skills " << g_skills.size() << " combo " << g_combo_skill_ids.size()
		<< " cm_per_tile " << g_config.cm_per_tile << "\n";

	return true;
}

const GameConfig& GetConfig()
{
	return g_config;
}

const SkillData& GetSkillData(int32 skill_id)
{
	static const SkillData empty;

	if (skill_id <= SKILL_ID_NONE || skill_id >= static_cast<int32>(g_skills.size()))
	{
		return empty;
	}

	return g_skills[static_cast<size_t>(skill_id)];
}

const MonsterData& GetMonsterData(int32 monster_type)
{
	static const MonsterData empty;

	if (monster_type <= MONSTER_TYPE_NONE || monster_type >= static_cast<int32>(g_monsters.size()))
	{
		return empty;
	}

	return g_monsters[static_cast<size_t>(monster_type)];
}

const ItemData& GetItemData(int32 item_id)
{
	static const ItemData empty;

	if (item_id <= ITEM_ID_NONE || item_id >= static_cast<int32>(g_items.size()))
	{
		return empty;
	}

	return g_items[static_cast<size_t>(item_id)];
}

const PlayerPreset& GetPlayerPreset()
{
	return g_preset;
}

int32 GetNeedExp(int32 level)
{
	if (level <= 0 || level >= static_cast<int32>(g_need_exps.size()))
	{
		return 0;
	}

	return g_need_exps[static_cast<size_t>(level)];
}

int32 GetMaxLevel()
{
	// 마지막 0 칸이 만렙이다
	for (int32 level = 1; level < static_cast<int32>(g_need_exps.size()); ++level)
	{
		if (g_need_exps[static_cast<size_t>(level)] <= 0)
		{
			return level;
		}
	}

	return static_cast<int32>(g_need_exps.size());
}

const std::vector<int32>& GetBasicSkillIds()
{
	return g_basic_skill_ids;
}

const std::vector<int32>& GetSkillLinks(int32 skill_id)
{
	auto found = g_skill_links.find(skill_id);
	if (found == g_skill_links.end())
	{
		return EMPTY_LINKS;
	}

	return found->second;
}

const std::vector<std::array<int32, 2>>& GetSkillTreeEdges()
{
	return g_skill_edges;
}

const std::vector<int32>& GetComboSkillIds()
{
	return g_combo_skill_ids;
}
