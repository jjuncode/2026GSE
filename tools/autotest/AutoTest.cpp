#include "stdafx.h"
#include "AutoTest.h"
#include "Combat.h"
#include "GameConfig.h"
#include "GameTime.h"
#include "Input.h"
#include "Instance.h"
#include "Minimap.h"
#include "Monster.h"
#include "NpcData.h"
#include "Player.h"
#include "Quest.h"
#include "Rank.h"
#include "Scene.h"
#include "UiWindows.h"
#include "Village.h"
#include "World.h"
#include "ZoneGen.h"

#include "Dependencies\glew.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
	bool g_on = false;
	std::string g_mode;
	float g_time = 0.f;
	float g_log_timer = 0.f;
	std::vector<std::string> g_captures;

	bool g_setup_done = false;
	int32 g_spawn_stage = 0;
	float g_gallery_timer = 0.f;
	int32 g_gallery_count = 0;
	float g_guard_release = 0.f;
	int32 g_parry_count = 0;
	int32 g_attack_seen = 0;
	int32 g_last_action = -1;
	int32 g_last_serial_parried = -1;
	const Creature* g_last_attacker = nullptr;
	int32 g_hits_taken = 0;
	int32 g_last_hp = 0;
	float g_parry_capture_timer = -1.f;
	float g_hit_capture_timer = -1.f;
	bool g_tele_captured = false;
	bool g_combo_captured = false;
	bool g_hit_spark_captured = false;
	bool g_ui_done = false;
	int32 g_ui_step = 0;
	int32 g_enhance_tries = 0;

	Creature* FindNearestMonster(float* distance)
	{
		Creature* best = nullptr;
		float best_distance = 1.0e9f;
		const Player& player = GetPlayer();

		for (Creature* creature : GetCreatures())
		{
			if (creature->m_team != TEAM_MONSTER || creature->IsDead())
			{
				continue;
			}

			const float delta_x = GetTileDistanceX(player.m_pos, creature->m_pos);
			const float delta_y = GetTileDistanceY(player.m_pos, creature->m_pos);
			const float distance_now = std::sqrt(delta_x * delta_x + delta_y * delta_y);
			if (distance_now < best_distance)
			{
				best_distance = distance_now;
				best = creature;
			}
		}

		*distance = best_distance;
		return best;
	}

	void AimAt(const Creature& target)
	{
		const ScreenPos screen = WorldToScreen(target.m_pos, GetCameraPos());
		const int32 window_x = static_cast<int32>(screen.x + static_cast<float>(WINDOW_SIZE_X) * 0.5f);
		const int32 window_y = static_cast<int32>(static_cast<float>(WINDOW_SIZE_Y) * 0.5f - screen.y);
		SetMouseWindowPos(window_x, window_y);
	}

	void SpawnNear(int32 kind, float offset_x, float offset_y)
	{
		WorldPos pos = GetPlayer().m_pos;
		MoveWorldPos(&pos, offset_x, offset_y);

		// 벽 속에 세우면 못 움직인다. 빈 칸을 찾을 때까지 돌려 본다
		for (int32 attempt = 0; attempt < 16 && IsBodyBlocked(pos, 0.45f); ++attempt)
		{
			pos = GetPlayer().m_pos;
			const float angle = static_cast<float>(attempt) * 0.7f;
			MoveWorldPos(&pos, std::cos(angle) * 2.6f, std::sin(angle) * 2.6f);
		}

		AddMonster(kind, pos, BAND_FIELD, false);
		std::cout << "[ AutoTest ] : spawn kind " << kind << "\n";
	}

	void Capture(const std::string& name)
	{
		g_captures.push_back(name);
	}

	void UpdateCombat(float real_seconds)
	{
		Player& player = MutablePlayer();

		float distance = 0.f;
		Creature* monster = FindNearestMonster(&distance);

		if (monster != nullptr)
		{
			AimAt(*monster);
		}

		if (g_guard_release > 0.f)
		{
			g_guard_release -= real_seconds;
			if (g_guard_release <= 0.f)
			{
				SetPlayerGuard(false);
			}
		}

		// 다가오는 창을 보고 패링. 네 번째 공격은 일부러 맞아 본다
		for (Creature* creature : GetCreatures())
		{
			if (creature->m_team != TEAM_MONSTER || creature->IsAttacking() == false || creature->m_cast_skill == nullptr)
			{
				continue;
			}

			if (creature->m_cast_serial == g_last_serial_parried)
			{
				continue;
			}

			const float near_x = GetTileDistanceX(player.m_pos, creature->m_pos);
			const float near_y = GetTileDistanceY(player.m_pos, creature->m_pos);
			if (near_x * near_x + near_y * near_y > 36.f)
			{
				continue;
			}

			const SkillData& skill = *creature->m_cast_skill;
			float lead = -1.f;
			for (const HitWindow& window : skill.hit_windows)
			{
				if (window.start_sec > creature->m_action_time)
				{
					lead = window.start_sec - creature->m_action_time;
					break;
				}
			}

			if (g_tele_captured == false && skill.hit_windows.empty() == false && creature->m_action_time > skill.hit_windows.front().start_sec * 0.6f)
			{
				g_tele_captured = true;
				Capture("telegraph");
			}

			if (lead < 0.f || lead > 0.12f)
			{
				continue;
			}

			g_last_serial_parried = creature->m_cast_serial;
			g_attack_seen += 1;

			if (g_attack_seen % 4 == 0 || skill.pierce_guard)
			{
				std::cout << "[ AutoTest ] : let attack " << g_attack_seen << " land ( skill " << skill.skill_id << " )\n";
				continue;
			}

			SetPlayerGuard(true);
			g_guard_release = 0.05f;
		}

		// 패링 성공 감지
		if (player.m_action == ACTION_PARRY_SUCCESS && g_last_action != ACTION_PARRY_SUCCESS)
		{
			g_parry_count += 1;
			g_parry_capture_timer = 0.03f;
			std::cout << "[ AutoTest ] : parry " << g_parry_count << " at " << g_time
				<< " cd Q " << player.GetSkillCooldown(106) << " E " << player.GetSkillCooldown(107) << " R " << player.GetSkillCooldown(108) << "\n";
		}

		if (player.m_hp < g_last_hp)
		{
			g_hits_taken += 1;
			if (g_hit_capture_timer < 0.f && g_hits_taken == 1)
			{
				g_hit_capture_timer = 0.02f;
			}
		}

		g_last_hp = player.m_hp;
		g_last_action = player.m_action;

		if (g_parry_capture_timer >= 0.f)
		{
			g_parry_capture_timer -= real_seconds;
			if (g_parry_capture_timer < 0.f && g_parry_count <= 2)
			{
				Capture("parry_" + std::to_string(g_parry_count));
			}
		}

		if (g_hit_capture_timer >= 0.f)
		{
			g_hit_capture_timer -= real_seconds;
			if (g_hit_capture_timer < 0.f)
			{
				Capture("player_hit");
				g_hit_capture_timer = -2.f;
			}
		}

		// 반격 구간과 몬스터가 멈춰 있을 때 친다
		const bool monster_open = monster != nullptr && (monster->m_action == ACTION_ATTACK_INTERRUPTED || monster->m_action == ACTION_DAMAGED || monster->IsAttacking() == false);
		const bool patient = g_parry_count < 3 && g_time < 12.f;
		if (monster != nullptr && distance < 2.2f && monster_open && player.m_action != ACTION_GUARD && patient == false)
		{
			PressPlayerAttack();
		}

		if (monster != nullptr && monster->m_flash > 0.1f && g_time > 2.f && g_hit_spark_captured == false)
		{
			g_hit_spark_captured = true;
			Capture("hit_spark");
		}

		if (g_combo_captured == false && player.m_cast_skill != nullptr && player.m_cast_skill->skill_id == 105)
		{
			int32 window_index = -1;
			const float progress = GetWindowProgress(player, &window_index);
			if (progress > 0.4f && progress < 0.9f)
			{
				g_combo_captured = true;
				Capture("combo_finisher");
			}
		}
	}

	void UpdateUi()
	{
		if (g_ui_done)
		{
			return;
		}

		const float base = 26.f;
		if (g_ui_step == 0 && g_time >= base)
		{
			ToggleInventoryWindow();
			g_ui_step = 1;
		}
		else if (g_ui_step == 1 && g_time >= base + 0.4f)
		{
			Capture("inventory");
			g_ui_step = 11;
		}
		else if (g_ui_step == 11 && g_time >= base + 0.6f)
		{
			ToggleInventoryWindow();
			ToggleSkillWindow();
			g_ui_step = 2;
		}
		else if (g_ui_step == 2 && g_time >= base + 1.0f)
		{
			Capture("skills");
			g_ui_step = 21;
		}
		else if (g_ui_step == 21 && g_time >= base + 1.2f)
		{
			ToggleSkillWindow();
			ToggleStatWindow();
			g_ui_step = 3;
		}
		else if (g_ui_step == 3 && g_time >= base + 1.6f)
		{
			Capture("stats");
			g_ui_step = 31;
		}
		else if (g_ui_step == 31 && g_time >= base + 1.8f)
		{
			ToggleStatWindow();
			MutablePlayer().m_pos = GetAnvilPos();
			MoveWorldPos(&MutablePlayer().m_pos, -1.f, 0.f);
			g_ui_step = 4;
		}
		else if (g_ui_step == 4 && g_time >= base + 2.2f)
		{
			OpenEnhanceWindow();
			g_ui_step = 5;
		}
		else if (g_ui_step == 5 && g_time >= base + 2.5f + static_cast<float>(g_enhance_tries) * 0.3f)
		{
			UiKeyEnter();
			g_enhance_tries += 1;
			if (g_enhance_tries >= 8)
			{
				g_ui_step = 6;
			}
		}
		else if (g_ui_step == 6 && g_time >= base + 5.0f)
		{
			Capture("enhance");
			g_ui_done = true;
		}
	}
}

namespace
{
	int32 g_skill_step = 0;
	float g_next_at = 0.f;
	WorldPos g_last_death_pos;
	bool g_has_death_pos = false;
	std::vector<const Creature*> g_alive_last;
	bool g_guard_hold = false;

	void TrackDeaths()
	{
		std::vector<const Creature*> alive;
		for (Creature* creature : GetCreatures())
		{
			if (creature->m_team != TEAM_MONSTER)
			{
				continue;
			}

			if (creature->IsDead())
			{
				for (const Creature* before : g_alive_last)
				{
					if (before == creature)
					{
						g_last_death_pos = creature->m_pos;
						g_has_death_pos = true;
					}
				}

				continue;
			}

			alive.push_back(creature);
		}

		g_alive_last = alive;
	}

	void Step(float delay)
	{
		g_skill_step += 1;
		g_next_at = g_time + delay;
	}

	void UpdateSkills(float real_seconds)
	{
		Player& player = MutablePlayer();
		TrackDeaths();

		float distance = 0.f;
		Creature* monster = FindNearestMonster(&distance);
		if (monster != nullptr && g_skill_step >= 3)
		{
			AimAt(*monster);
		}

		if (g_guard_release > 0.f)
		{
			g_guard_release -= real_seconds;
			if (g_guard_release <= 0.f && g_guard_hold == false)
			{
				SetPlayerGuard(false);
			}
		}

		if (g_parry_capture_timer >= 0.f)
		{
			g_parry_capture_timer -= real_seconds;
			if (g_parry_capture_timer < 0.f)
			{
				Capture("counter");
			}
		}

		if (g_time < g_next_at)
		{
			return;
		}

		switch (g_skill_step)
		{
			case 0:
			{
				UseBagItem(0);
				GivePlayerItem(ITEM_ID_SPELL_TRACE, 10);
				AddPlayerGold(500);
				player.AddExp(1500);
				std::cout << "[ AutoTest ] : level " << player.m_level << " skill_point " << player.m_skill_point << " stat_point " << player.m_stat_point << "\n";
				Step(0.35f);
				break;
			}
			case 1:
			{
				Capture("levelup");
				for (int32 skill_id : { 107, 106, 108, 109 })
				{
					const bool unlocked = player.TryUnlockSkill(skill_id);
					std::cout << "[ AutoTest ] : unlock " << skill_id << " " << unlocked << "\n";
				}

				for (int32 index = 0; index < 6; ++index)
				{
					player.TryStatUp(STAT_ATTACK);
				}

				Step(0.5f);
				break;
			}
			case 2:
			{
				SpawnNear(MONSTER_BLIGHTED, 2.f, 0.6f);
				SpawnNear(MONSTER_BLIGHTED, 1.6f, -1.4f);
				SpawnNear(MONSTER_HOUND, -1.8f, 1.2f);
				Step(1.2f);
				break;
			}
			case 3:
			{
				PressPlayerSkill(107);
				Step(0.62f);
				break;
			}
			case 4:
			{
				Capture("spin");
				Step(2.4f);
				break;
			}
			case 5:
			{
				PressPlayerSkill(106);
				Step(0.32f);
				break;
			}
			case 6:
			{
				Capture("dash_slash");
				Step(1.4f);
				break;
			}
			case 7:
			{
				PressPlayerSkill(108);
				Step(0.4f);
				break;
			}
			case 8:
			{
				Capture("leap");
				Step(1.2f);
				break;
			}
			case 9:
			{
				PressPlayerDodge(110);
				Step(0.14f);
				break;
			}
			case 10:
			{
				Capture("dodge");
				Step(0.6f);
				break;
			}
			case 11:
			{
				// 몬스터가 칠 때까지 가드를 들고 버틴다
				SetPlayerGuard(true);
				g_guard_hold = true;
				Step(2.5f);
				break;
			}
			case 12:
			{
				Capture("guard");
				g_guard_hold = false;
				SetPlayerGuard(false);
				Step(0.3f);
				break;
			}
			case 13:
			{
				player.m_hp = 30;
				UsePlayerPotion();
				Step(0.25f);
				break;
			}
			case 14:
			{
				Capture("potion");
				std::cout << "[ AutoTest ] : potion hp " << player.m_hp << " potions " << player.m_inventory.CountItem(ITEM_ID_HP_POTION) << "\n";
				Step(0.2f);
				break;
			}
			case 15:
			{
				// 다가오는 공격을 패링하고 반격까지
				for (Creature* creature : GetCreatures())
				{
					if (creature->m_team != TEAM_MONSTER || creature->IsAttacking() == false || creature->m_cast_skill == nullptr || creature->m_cast_skill->pierce_guard)
					{
						continue;
					}

					for (const HitWindow& window : creature->m_cast_skill->hit_windows)
					{
						const float lead = window.start_sec - creature->m_action_time;
						if (lead > 0.f && lead < 0.1f)
						{
							SetPlayerGuard(true);
							g_guard_release = 0.05f;
						}
					}
				}

				if (player.m_action == ACTION_PARRY_SUCCESS)
				{
					PressPlayerAttack();
					g_parry_capture_timer = 0.32f;
					std::cout << "[ AutoTest ] : counter pressed" << std::endl;
					Step(1.f);
				}

				break;
			}
			case 16:
			{
				if (g_has_death_pos)
				{
					player.m_pos = g_last_death_pos;
				}

				Step(0.2f);
				break;
			}
			case 17:
			{
				const int32 before = player.m_inventory.CountItem(ITEM_ID_SPELL_TRACE) + player.m_inventory.CountItem(ITEM_ID_HP_POTION);
				for (int32 index = 0; index < 6; ++index)
				{
					Interact();
				}

				const int32 after = player.m_inventory.CountItem(ITEM_ID_SPELL_TRACE) + player.m_inventory.CountItem(ITEM_ID_HP_POTION);
				std::cout << "[ AutoTest ] : pickup items " << before << " -> " << after << " death_pos " << g_has_death_pos << "\n";
				Capture("pickup");
				Step(0.5f);
				break;
			}
			default:
			{
				break;
			}
		}

		// 해금한 스킬 사이사이엔 평타로 무리를 정리한다
		if (g_skill_step >= 4 && g_skill_step <= 14 && monster != nullptr && distance < 2.f && player.m_action == ACTION_NORMAL && g_guard_hold == false)
		{
			PressPlayerAttack();
		}
	}
}

void InitAutoTest()
{
	char* mode = nullptr;
	size_t length = 0;
	if (_dupenv_s(&mode, &length, "SG_AUTOTEST") != 0 || mode == nullptr)
	{
		return;
	}

	g_on = true;
	g_mode = mode;
	free(mode);
	std::cout << "[ AutoTest ] : mode " << g_mode << "\n";
}

bool IsAutoTestOn()
{
	return g_on;
}

void UpdateAutoTest(float real_seconds)
{
	if (g_on == false)
	{
		return;
	}

	g_time += real_seconds;

	if (g_mode == "skills")
	{
		if (g_time >= 0.5f)
		{
			UpdateSkills(real_seconds);
		}

		g_log_timer -= real_seconds;
		if (g_log_timer <= 0.f)
		{
			g_log_timer = 1.f;
			const Player& player = GetPlayer();
			std::cout << "[ AutoTest ] : t " << g_time << " step " << g_skill_step << " hp " << player.m_hp << " / " << player.m_stat.max_hp
				<< " atk " << player.m_stat.attack << " guard " << static_cast<int32>(player.m_guard.gage) << " action " << player.m_action
				<< " lv " << player.m_level << " exp " << player.m_exp << " kills " << player.m_kill_count
				<< " cd E " << player.GetSkillCooldown(107) << " Q " << player.GetSkillCooldown(106) << " R " << player.GetSkillCooldown(108) << "\n";
		}

		return;
	}

	if (g_mode == "showcase")
	{
		// 평타 셋 -> 대쉬 -> 회전베기를 연사로 찍는다
		Player& player = MutablePlayer();

		// 화면 오른쪽 아래를 겨눈다 ( 몬스터를 세우면 맞아서 동작이 끊긴다 )
		SetMouseWindowPos(WINDOW_SIZE_X / 2 + 220, WINDOW_SIZE_Y / 2 + 90);

		const std::array<float, 6> presses = { 1.5f, 1.86f, 2.24f, 3.6f, 4.6f, 0.5f };
		for (size_t index = 0; index < presses.size(); ++index)
		{
			if (g_time - real_seconds < presses[index] && g_time >= presses[index])
			{
				if (index < 3)
				{
					PressPlayerAttack();
				}
				else if (index == 3)
				{
					PressPlayerDodge(110);
				}
				else if (index == 4)
				{
					PressPlayerSkill(107);
				}
				else
				{
					UseBagItem(0);
					player.AddExp(1500);

					// 사람 없는 성문 밖에서 찍는다
					for (const ZoneSite& site : GetNpcSites())
					{
						if (site.kind == SITE_OUTPOST)
						{
							PlacePlayer(ZoneTileToWorld(site.tile.x, site.tile.y, 0.5f, 0.5f));
							break;
						}
					}

					for (int32 skill_id : { 107, 106, 108, 109 })
					{
						player.TryUnlockSkill(skill_id);
					}
				}
			}
		}

		g_gallery_timer -= real_seconds;
		const bool in_burst = (g_time > 1.5f && g_time < 2.9f) || (g_time > 3.58f && g_time < 4.1f) || (g_time > 4.7f && g_time < 5.5f);
		if (in_burst && g_gallery_timer <= 0.f)
		{
			g_gallery_timer = 0.05f;
			Capture("show_" + std::to_string(g_gallery_count));
			g_gallery_count += 1;
		}

		return;
	}

	if (g_mode == "shop")
	{
		// 성채 무기상 -> 방어구상에서 사고, 갑옷을 끼고, 가방에서 판다
		const auto crossed = [](float mark)
		{
			return g_time - FIXED_STEP_SECONDS < mark && g_time >= mark;
		};

		Player& player = MutablePlayer();
		WorldPos town;
		for (const ZoneSite& site : GetNpcSites())
		{
			if (site.kind == SITE_TOWN && site.village == ZONE_CITADEL)
			{
				town = ZoneTileToWorld(site.tile.x, site.tile.y, 0.5f, 0.5f);
			}
		}

		const auto log_state = [&player](const char* tag)
		{
			std::cout << "[ AutoTest ] : " << tag << " gold " << player.m_gold << " atk " << player.m_stat.attack << " def " << player.m_stat.defence
				<< " hp_max " << player.m_stat.max_hp << " weapon " << player.m_inventory.GetEquip(EQUIP_AREA_WEAPON).item_id
				<< " armor " << player.m_inventory.GetEquip(EQUIP_AREA_ARMOR).item_id << "\n";
		};

		if (crossed(0.5f))
		{
			AddPlayerGold(6000);
			WorldPos spot = town;
			MoveWorldPos(&spot, -1.9f, -0.5f);
			PlacePlayer(spot);
			log_state("start");
		}

		if (crossed(1.0f))
		{
			Interact();
		}

		if (crossed(1.2f))
		{
			UiMouseMove(-158.f, 72.f);
		}

		if (crossed(1.4f))
		{
			Capture("shop_weapon");
		}

		if (crossed(1.6f))
		{
			UiMouseDown(UI_MOUSE_LEFT, 186.f, 72.f);
			UiMouseUp(UI_MOUSE_LEFT, 186.f, 72.f);
			log_state("bought steel sword");
		}

		if (crossed(2.0f))
		{
			Capture("shop_bought");
		}

		if (crossed(2.4f))
		{
			CloseTopWindow();
			CloseTopWindow();
			WorldPos spot = town;
			MoveWorldPos(&spot, 2.1f, -2.1f);
			PlacePlayer(spot);
		}

		if (crossed(2.8f))
		{
			Interact();
		}

		if (crossed(3.0f))
		{
			UiMouseDown(UI_MOUSE_LEFT, 186.f, 72.f);
			UiMouseUp(UI_MOUSE_LEFT, 186.f, 72.f);
			UseBagItem(player.m_inventory.FindFirst(22));
			UseBagItem(player.m_inventory.FindFirst(11));
			log_state("bought chain, equipped");
		}

		if (crossed(3.3f))
		{
			UiMouseMove(372.f, 172.f);
		}

		if (crossed(3.5f))
		{
			Capture("shop_armor");
		}

		if (crossed(3.8f))
		{
			// 가방 첫 칸 ( 갈아 낀 초보자의 검 ) 을 우클릭으로 판다
			UiMouseMove(324.f, 38.f);
		}

		if (crossed(4.0f))
		{
			Capture("shop_sell_hover");
			UiMouseDown(UI_MOUSE_RIGHT, 324.f, 38.f);
			UiMouseUp(UI_MOUSE_RIGHT, 324.f, 38.f);
			log_state("sold slot 0");
		}

		if (crossed(4.3f))
		{
			Capture("shop_sold");
		}

		return;
	}

	if (g_mode == "quest")
	{
		// 성채 게시판에서 의뢰 넷을 받고, 길 안내를 보고, 목표를 채워 보고한다
		const auto crossed = [](float mark)
		{
			return g_time - FIXED_STEP_SECONDS < mark && g_time >= mark;
		};

		Player& player = MutablePlayer();
		const auto log_quests = [&player](const char* tag)
		{
			std::cout << "[ AutoTest ] : " << tag << " gold " << player.m_gold << " lv " << player.m_level << " exp " << player.m_exp << " active " << GetActiveQuests().size();
			for (const Quest& quest : GetActiveQuests())
			{
				std::cout << " | goal " << quest.goal << " " << quest.progress << "/" << quest.need << " state " << quest.state;
			}

			std::cout << "\n";
		};

		const auto click = [](float x, float y)
		{
			UiMouseMove(x, y);
			UiMouseDown(UI_MOUSE_LEFT, x, y);
			UiMouseUp(UI_MOUSE_LEFT, x, y);
		};

		if (crossed(0.5f))
		{
			WorldPos spot = GetBoardPos(ZONE_CITADEL);
			MoveWorldPos(&spot, 1.2f, 1.2f);
			PlacePlayer(spot);
		}

		if (crossed(1.0f))
		{
			Interact();
		}

		if (crossed(1.2f))
		{
			UiMouseMove(-231.f, 174.f - 38.f * 4.f);
		}

		if (crossed(1.4f))
		{
			Capture("quest_board");
		}

		// 일일 · 보상 · 고난 하나씩과 일일 하나 더
		const std::array<int32, 4> rows = { 0, 4, 7, 1 };
		for (size_t index = 0; index < rows.size(); ++index)
		{
			const float at = 1.6f + static_cast<float>(index) * 0.3f;
			if (crossed(at))
			{
				click(-231.f, 174.f - 38.f * static_cast<float>(rows[index]));
			}

			if (crossed(at + 0.15f))
			{
				click(211.f, -250.f);
			}
		}

		if (crossed(3.0f))
		{
			Capture("quest_accepted");
			log_quests("accepted");
		}

		if (crossed(3.2f))
		{
			CloseTopWindow();
			UiMouseMove(0.f, 0.f);
		}

		if (crossed(4.0f))
		{
			Capture("quest_guide");
		}

		if (crossed(4.3f))
		{
			ToggleWorldMap();
		}

		if (crossed(4.6f))
		{
			Capture("quest_map");
		}

		if (crossed(4.8f))
		{
			ToggleWorldMap();
		}

		// 사냥 목표는 처치 알림을 그대로 흘려 채운다. 처치 경로 자체는 Monster::OnDeath 가 부른다
		if (crossed(5.0f))
		{
			const std::vector<Quest> quests = GetActiveQuests();
			for (const Quest& quest : quests)
			{
				for (int32 count = 0; count < quest.need; ++count)
				{
					NotifyQuestKill(quest.target, quest.goal == QUEST_GOAL_ELITE, quest.goal == QUEST_GOAL_BOSS, 99);
				}

				if (quest.goal == QUEST_GOAL_DELIVER)
				{
					GivePlayerItem(quest.target, quest.need);
				}

				if (quest.goal == QUEST_GOAL_SCOUT)
				{
					PlacePlayer(quest.place);
				}
			}

			log_quests("filled");
		}

		if (crossed(5.6f))
		{
			Capture("quest_done");
			log_quests("before report");
		}

		if (crossed(6.0f))
		{
			WorldPos spot = GetBoardPos(ZONE_CITADEL);
			MoveWorldPos(&spot, 1.2f, 1.2f);
			PlacePlayer(spot);
		}

		if (crossed(6.4f))
		{
			Interact();
			log_quests("reported");
		}

		if (crossed(6.7f))
		{
			Capture("quest_reported");
		}

		return;
	}

	if (g_mode == "rank")
	{
		// 공헌을 채워 승급 심사를 띄우고, 받아서 넘겨 실버가 되는지 본다
		const auto crossed = [](float mark)
		{
			return g_time - FIXED_STEP_SECONDS < mark && g_time >= mark;
		};

		const auto click = [](float x, float y)
		{
			UiMouseMove(x, y);
			UiMouseDown(UI_MOUSE_LEFT, x, y);
			UiMouseUp(UI_MOUSE_LEFT, x, y);
		};

		const auto log_rank = [](const char* tag)
		{
			std::cout << "[ AutoTest ] : " << tag << " rank " << GetPlayerRank() << " points " << GetRankPoints() << " exam_ready " << IsRankExamReady()
				<< " offers0 " << GetBoardQuests(ZONE_CITADEL).size() << " first_is_exam " << (GetBoardQuests(ZONE_CITADEL).empty() == false && GetBoardQuests(ZONE_CITADEL).front().is_exam) << "\n";
		};

		if (crossed(0.5f))
		{
			WorldPos spot = GetBoardPos(ZONE_CITADEL);
			MoveWorldPos(&spot, 1.2f, 1.2f);
			PlacePlayer(spot);
			log_rank("start");
		}

		if (crossed(1.0f))
		{
			Interact();
		}

		// 고난 의뢰 ( 8 번째 줄 ) 는 브론즈라 막혀야 한다
		if (crossed(1.2f))
		{
			click(-231.f, 174.f - 38.f * 7.f);
		}

		if (crossed(1.35f))
		{
			click(211.f, -250.f);
			std::cout << "[ AutoTest ] : hard accept as bronze active " << GetActiveQuests().size() << "\n";
		}

		if (crossed(1.6f))
		{
			Capture("rank_locked");
		}

		if (crossed(1.8f))
		{
			AddRankPoints(60);
			CloseTopWindow();
		}

		// 공헌이 차도 심사는 게시판을 다시 열 때 붙는다
		if (crossed(2.2f))
		{
			ReportQuestsAt(ZONE_CITADEL);
			Interact();
			log_rank("after points");
		}

		if (crossed(2.4f))
		{
			click(-231.f, 174.f);
		}

		if (crossed(2.6f))
		{
			Capture("rank_exam");
			click(211.f, -250.f);
		}

		if (crossed(2.9f))
		{
			const std::vector<Quest> quests = GetActiveQuests();
			for (const Quest& quest : quests)
			{
				for (int32 count = 0; count < quest.need; ++count)
				{
					NotifyQuestKill(quest.target, quest.goal == QUEST_GOAL_ELITE, quest.goal == QUEST_GOAL_BOSS, 99);
				}
			}

			CloseTopWindow();
		}

		if (crossed(3.4f))
		{
			Interact();
			log_rank("reported exam");
		}

		if (crossed(3.8f))
		{
			Capture("rank_up");
		}

		return;
	}

	if (g_mode == "tour")
	{
		// 거점마다 옮겨 가며 찍는다 - 성 밖 소품 확인용
		const std::vector<ZoneSite>& sites = GetNpcSites();
		const int32 stop = static_cast<int32>(g_time / 1.6f);
		const float within = g_time - static_cast<float>(stop) * 1.6f;

		if (stop >= 1 && stop <= static_cast<int32>(sites.size()) && within < 0.02f)
		{
			const ZoneSite& site = sites[static_cast<size_t>(stop - 1)];
			PlacePlayer(ZoneTileToWorld(site.tile.x, site.tile.y, 0.5f, 0.5f));
		}

		if (stop >= 1 && stop <= static_cast<int32>(sites.size()) && within >= 1.2f && within < 1.22f)
		{
			Capture("tour_" + std::to_string(stop) + "_kind" + std::to_string(sites[static_cast<size_t>(stop - 1)].kind));
		}

		return;
	}

	if (g_mode == "village")
	{
		// 지도 한 장, 그다음 마을마다 광장 전경 · 게시판 소식 · 게시판 평판. 소식이 쌓이게 90 초 뒤부터 돈다
		const auto crossed = [](float mark)
		{
			return g_time - FIXED_STEP_SECONDS < mark && g_time >= mark;
		};

		if (crossed(4.f))
		{
			ToggleWorldMap();
		}

		if (crossed(4.6f))
		{
			Capture("village_map");
		}

		if (crossed(5.2f))
		{
			ToggleWorldMap();
		}

		const float start = 90.f;
		const float stop_sec = 4.f;

		for (int32 village = 0; village < GetVillageCount(); ++village)
		{
			const float base = start + static_cast<float>(village) * stop_sec;
			const std::string tag = "village_" + std::to_string(village);

			if (crossed(base))
			{
				WorldPos plaza = GetVillageCenterPos(village);
				MoveWorldPos(&plaza, 3.f, 3.f);
				PlacePlayer(plaza);
			}

			if (crossed(base + 0.8f))
			{
				Capture(tag + "_plaza");
			}

			if (crossed(base + 1.0f))
			{
				WorldPos board = GetBoardPos(village);
				MoveWorldPos(&board, 1.f, 1.f);
				PlacePlayer(board);
			}

			if (crossed(base + 1.4f))
			{
				Interact();
			}

			if (crossed(base + 2.2f))
			{
				Capture(tag + "_news");
			}

			// 평판 탭 ( 창 가운데 원점, 위가 양수 )
			if (crossed(base + 2.4f))
			{
				UiMouseDown(UI_MOUSE_LEFT, 110.f, 238.f);
			}

			if (crossed(base + 3.2f))
			{
				Capture(tag + "_view");
			}

			if (crossed(base + 3.6f))
			{
				CloseTopWindow();
			}
		}

		return;
	}

	if (g_mode == "dungeon")
	{
		// 실버로 올려 균열에 들어가 자동 전투로 방 셋을 비운다. 다음 방은 가장 가까운 적 쪽으로 걸어간다
		Player& player = MutablePlayer();

		if (g_spawn_stage == 0 && g_time >= 0.5f)
		{
			g_spawn_stage = 1;
			UseBagItem(0);
			player.AddExp(5000);
			for (int32 skill_id : { 106, 107, 108, 109 })
			{
				player.TryUnlockSkill(skill_id);
			}

			WorldPos rift;
			std::wstring name;
			if (GetInstanceEntrance(0, &rift, &name))
			{
				PlacePlayer(rift);
			}

			// 브론즈로 먼저 두드려 막히는지 본다
			const bool entered_as_bronze = InteractInstance() && IsPlayerInInstance();
			std::cout << "[ AutoTest ] : bronze entered " << entered_as_bronze << "\n";
			PromotePlayerRank();
		}

		// 레벨업이 능력치를 다시 계산하니 매 틱 다시 건다
		if (g_spawn_stage >= 1)
		{
			player.m_stat.max_hp = 99999;
			player.m_hp = 99999;
			player.m_stat.attack = 120;
		}

		if (g_spawn_stage == 1 && g_time >= 1.f)
		{
			g_spawn_stage = 2;
			InteractInstance();
			std::cout << "[ AutoTest ] : silver entered " << IsPlayerInInstance() << "\n";
		}

		if (g_spawn_stage == 2 && g_time >= 1.3f)
		{
			g_spawn_stage = 3;
			Capture("dungeon_enter");
		}

		// 다 비우면 출구로 나간다
		WorldPos exit_guide;
		if (g_spawn_stage == 3 && GetInstanceGuide(&exit_guide) && GetRankPoints() > 0)
		{
			const float exit_x = GetTileDistanceX(player.m_pos, exit_guide);
			const float exit_y = GetTileDistanceY(player.m_pos, exit_guide);
			if (exit_x * exit_x + exit_y * exit_y < 2.f)
			{
				g_spawn_stage = 4;
				Capture("dungeon_cleared");
			}
		}

		if (g_spawn_stage == 4 && g_time > 0.f)
		{
			g_spawn_stage = 5;
			InteractInstance();
			std::cout << "[ AutoTest ] : left dungeon inside " << IsPlayerInInstance() << "\n";
		}

		if (g_spawn_stage >= 2 && g_time < 80.f)
		{
			UpdateCombat(real_seconds);

			float distance = 0.f;
			Creature* target = FindNearestMonster(&distance);
			WorldPos guide;
			if (GetInstanceGuide(&guide) && player.m_action == ACTION_NORMAL)
			{
				// 비운 방에선 금빛 문을 지나 다음 방 안쪽까지 걷는다
				WorldPos beyond = guide;
				const float guide_x = GetTileDistanceX(player.m_pos, guide);
				const float guide_y = GetTileDistanceY(player.m_pos, guide);
				if (guide_x * guide_x + guide_y * guide_y < 1.f)
				{
					MoveWorldPos(&beyond, 4.f, 0.f);
				}
				const float delta_x = GetTileDistanceX(player.m_pos, beyond);
				const float delta_y = GetTileDistanceY(player.m_pos, beyond);
				SetPlayerMoveInput(delta_x - delta_y, -(delta_x + delta_y) * 0.5f);
			}
			else if (target != nullptr && distance > 1.8f && distance < 40.f && player.m_action == ACTION_NORMAL)
			{
				const float delta_x = GetTileDistanceX(player.m_pos, target->m_pos);
				const float delta_y = GetTileDistanceY(player.m_pos, target->m_pos);
				SetPlayerMoveInput(delta_x - delta_y, -(delta_x + delta_y) * 0.5f);
			}
			else if (target != nullptr && distance <= 2.4f && target->IsAttacking() == false && player.m_action == ACTION_NORMAL)
			{
				PressPlayerAttack();
			}
		}

		g_log_timer -= real_seconds;
		if (g_log_timer <= 0.f)
		{
			g_log_timer = 2.f;
			float distance = 0.f;
			FindNearestMonster(&distance);
			std::cout << "[ AutoTest ] : t " << g_time << " inside " << IsPlayerInInstance() << " nearest " << distance << " lv " << player.m_level << " rank " << GetPlayerRank() << " points " << GetRankPoints() << "\n";
		}

		g_gallery_timer -= real_seconds;
		if (g_time > 3.f && g_gallery_timer <= 0.f)
		{
			g_gallery_timer = 6.f;
			Capture("dungeon_" + std::to_string(g_gallery_count));
			g_gallery_count += 1;
		}

		return;
	}

	if (g_mode == "boss")
	{
		// 기사단장 앞에서 패링 · 평타로 게이지를 채워 그로기, 체력 반에서 분노 페이즈를 본다
		Player& player = MutablePlayer();
		static bool s_groggy_seen = false;
		static std::wstring s_phase_seen;

		if (g_spawn_stage == 0 && g_time >= 0.5f)
		{
			g_spawn_stage = 1;
			UseBagItem(0);
			player.AddExp(5000);
			for (int32 skill_id : { 106, 107, 108, 109 })
			{
				player.TryUnlockSkill(skill_id);
			}

			WorldPos home;
			std::wstring name;
			if (GetEliteBossHome(&home, &name))
			{
				MoveWorldPos(&home, 3.f, 0.f);
				PlacePlayer(home);
			}
		}

		if (g_spawn_stage == 1 && g_time >= 0.8f)
		{
			// 시험이 죽음으로 끊기지 않게 버틴다. 때리는 힘은 페이즈를 보려고 키운다
			player.m_stat.max_hp = 99999;
			player.m_hp = 99999;
			player.m_stat.attack = 70;
		}

		if (g_time >= 1.f && g_time < 40.f)
		{
			UpdateCombat(real_seconds);

			float distance = 0.f;
			Creature* boss = FindNearestMonster(&distance);
			if (boss != nullptr && distance < 2.6f && boss->IsAttacking() == false && player.m_action == ACTION_NORMAL)
			{
				PressPlayerAttack();
			}
		}

		BossBarInfo bar;
		const bool has_bar = GetBossBar(&bar);
		if (has_bar && bar.is_groggy && s_groggy_seen == false)
		{
			s_groggy_seen = true;
			Capture("boss_groggy");
		}

		if (has_bar && bar.phase_name.empty() == false && s_phase_seen.empty())
		{
			s_phase_seen = bar.phase_name;
			Capture("boss_phase");
		}

		g_log_timer -= real_seconds;
		if (g_log_timer <= 0.f)
		{
			g_log_timer = 1.f;
			std::cout << "[ AutoTest ] : t " << g_time << " bar " << has_bar << " hp " << bar.hp_ratio << " stagger " << bar.stagger_ratio
				<< " groggy " << bar.is_groggy << " phase " << (bar.phase_name.empty() ? 0 : 1) << " parry " << g_parry_count << "\n";
		}

		g_gallery_timer -= real_seconds;
		if (g_time > 2.f && g_gallery_timer <= 0.f)
		{
			g_gallery_timer = 3.f;
			Capture("boss_" + std::to_string(g_gallery_count));
			g_gallery_count += 1;
		}

		return;
	}

	if (g_mode == "shadow")
	{
		// 그림자 다섯과 견줄 타락자 하나를 세운다. 마을 안이라 덤비지 않고 서 있는다
		if (g_spawn_stage == 0 && g_time >= 1.f)
		{
			g_spawn_stage = 1;
			SpawnNear(MONSTER_SHADOW_SOLDIER, 2.2f, -1.6f);
			SpawnNear(MONSTER_SHADOW_HOUND, 3.0f, 0.6f);
			SpawnNear(MONSTER_SHADOW_DEVOURER, -3.2f, -1.2f);
			SpawnNear(MONSTER_SHADOW_KNIGHT, -1.6f, 2.6f);
			SpawnNear(MONSTER_SHADOW_COLOSSUS, 1.0f, 3.6f);
			SpawnNear(MONSTER_BLIGHTED, -2.6f, 0.8f);

			std::array<int32, MONSTER_KIND_COUNT> counts = {};
			for (int32 index = 0; index < GetMonsterCount(); ++index)
			{
				MonsterReport report;
				if (GetMonsterReport(index, &report))
				{
					counts[static_cast<size_t>(report.kind)] += 1;
				}
			}

			std::cout << "[ AutoTest ] : zone monsters";
			for (size_t kind = 0; kind < counts.size(); ++kind)
			{
				std::cout << " " << kind << ":" << counts[kind];
			}

			std::cout << "\n";
		}

		g_gallery_timer -= real_seconds;
		if (g_time > 2.f && g_gallery_timer <= 0.f)
		{
			g_gallery_timer = 1.1f;
			Capture("shadow_" + std::to_string(g_gallery_count));
			g_gallery_count += 1;
		}

		return;
	}

	if (g_mode == "gallery")
	{
		if (g_spawn_stage == 0 && g_time >= 1.f)
		{
			g_spawn_stage = 1;
			SpawnNear(MONSTER_HOUND, 2.2f, -1.6f);
			SpawnNear(MONSTER_DEVOURER, 3.2f, 1.4f);
			SpawnNear(MONSTER_GATEBREAKER, -3.4f, 0.4f);
			SpawnNear(MONSTER_BLIGHTED, -1.8f, 2.4f);
		}

		SetPlayerGuard(true);
		g_gallery_timer -= real_seconds;
		if (g_time > 2.f && g_gallery_timer <= 0.f)
		{
			g_gallery_timer = 1.1f;
			Capture("gallery_" + std::to_string(g_gallery_count));
			g_gallery_count += 1;
		}

		return;
	}

	if (g_setup_done == false && g_time >= 0.5f)
	{
		g_setup_done = true;
		UseBagItem(0);
		GivePlayerItem(ITEM_ID_SPELL_TRACE, 40);
		AddPlayerGold(3000);
		g_last_hp = GetPlayer().m_hp;
		Capture("start");
	}

	if (g_spawn_stage == 0 && g_time >= 1.f)
	{
		g_spawn_stage = 1;

		// 마을 안 표적은 몬스터가 안 노린다 ( 2.27 ). 성문 밖 초소로 나가서 싸운다
		for (const ZoneSite& site : GetNpcSites())
		{
			if (site.kind == SITE_OUTPOST)
			{
				PlacePlayer(ZoneTileToWorld(site.tile.x, site.tile.y, 0.5f, 0.5f));
				break;
			}
		}

		SpawnNear(MONSTER_BLIGHTED, 2.4f, 0.f);

		// 패링 쿨 감소를 보려고 Q E R 을 풀어 쿨을 돌려 둔다
		Player& player = MutablePlayer();
		player.AddExp(1500);
		for (int32 skill_id : { 106, 107, 108 })
		{
			player.TryUnlockSkill(skill_id);
			player.StartCooldown(GetSkillData(skill_id));
		}
	}

	if (g_spawn_stage == 1 && g_time >= 14.f)
	{
		g_spawn_stage = 2;
		SpawnNear(MONSTER_DEVOURER, 3.f, 1.f);
	}

	if (g_time < 25.f)
	{
		UpdateCombat(real_seconds);
	}
	else
	{
		SetPlayerGuard(false);
	}

	UpdateUi();

	g_log_timer -= real_seconds;
	if (g_log_timer <= 0.f)
	{
		g_log_timer = 1.f;

		float distance = 0.f;
		Creature* monster = FindNearestMonster(&distance);
		const Player& player = GetPlayer();

		std::cout << "[ AutoTest ] : t " << g_time << " hp " << player.m_hp << " / " << player.m_stat.max_hp
			<< " guard " << static_cast<int32>(player.m_guard.gage) << " action " << player.m_action
			<< " lv " << player.m_level << " exp " << player.m_exp << " gold " << player.m_gold
			<< " parry " << g_parry_count << " hits_taken " << g_hits_taken << " kills " << player.m_kill_count;

		if (monster != nullptr)
		{
			std::cout << " | monster hp " << monster->m_hp << " / " << monster->m_stat.max_hp << " action " << monster->m_action << " dist " << distance;
		}

		std::cout << "\n";
	}
}

bool PopCaptureRequest(std::string* name)
{
	if (g_captures.empty())
	{
		return false;
	}

	*name = g_captures.front();
	g_captures.erase(g_captures.begin());

	return true;
}

void SaveFrameBmp(const std::string& name, int32 width, int32 height)
{
	std::vector<uint8> pixels(static_cast<size_t>(width * height * 3));

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

	const std::string path = "Logs/cap_" + name + ".bmp";
	std::ofstream file(path, std::ios::binary);

	const uint32 row = static_cast<uint32>(width * 3);
	const uint32 image_size = row * static_cast<uint32>(height);
	const uint32 file_size = 54 + image_size;

	uint8 header[54] = {};
	header[0] = 'B';
	header[1] = 'M';
	header[2] = static_cast<uint8>(file_size);
	header[3] = static_cast<uint8>(file_size >> 8);
	header[4] = static_cast<uint8>(file_size >> 16);
	header[5] = static_cast<uint8>(file_size >> 24);
	header[10] = 54;
	header[14] = 40;
	header[18] = static_cast<uint8>(width);
	header[19] = static_cast<uint8>(width >> 8);
	header[22] = static_cast<uint8>(height);
	header[23] = static_cast<uint8>(height >> 8);
	header[26] = 1;
	header[28] = 24;

	file.write(reinterpret_cast<const char*>(header), 54);
	file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));

	std::cout << "[ AutoTest ] : captured " << path << "\n";
}
