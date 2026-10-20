#include "stdafx.h"
#include "Hud.h"
#include "GameConfig.h"
#include "Minimap.h"
#include "Monster.h"
#include "Npc.h"
#include "NpcHud.h"
#include "Player.h"
#include "Profiler.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "Text.h"
#include "UiDraw.h"
#include "UiWindows.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace
{
	const float PI = 3.14159265f;

	const float HP_BAR_Y = -230.f;
	const float HP_BAR_WIDTH = 340.f;
	const float ACTION_Y = -310.f;
	const float ACTION_STEP = 48.f;
	const float EXP_Y = -355.f;
	const float LOW_HP_RATIO = 0.3f;
	const float INTRO_HINT_SEC = 22.f;
	const float LAG_SPEED = 0.6f;	// 줄어든 체력 잔상이 따라 내려오는 속도 ( 비율 / 초 )

	constexpr int32 ACTION_SLOT_COUNT = 8;

	struct ActionSlot
	{
		int32 skill_id = SKILL_ID_NONE;	// 0 이면 물약
		int32 icon = 0;
		const wchar_t* key = L"";
	};

	const std::array<ActionSlot, ACTION_SLOT_COUNT> ACTION_SLOTS =
	{
		ActionSlot{ 101, SKILL_ICON_SLASH, L"좌" },
		ActionSlot{ 103, SKILL_ICON_PARRY, L"우" },
		ActionSlot{ 106, SKILL_ICON_DASH_SLASH, L"Q" },
		ActionSlot{ 107, SKILL_ICON_SPIN_SLASH, L"E" },
		ActionSlot{ 108, SKILL_ICON_LEAP, L"R" },
		ActionSlot{ 110, SKILL_ICON_DODGE, L"Spc" },
		ActionSlot{ 111, SKILL_ICON_ROLL, L"Sft" },
		ActionSlot{ SKILL_ID_NONE, 0, L"1" },
	};

	float g_time = 0.f;
	float g_hp_lag = 1.f;	// 맞은 뒤 잠깐 남는 체력 잔상
	bool g_show_help = false;

	float GetBottom()
	{
		return -static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	}

	float GetTopEdge()
	{
		return static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	}

	ColorVertex MakeVertex(float x, float y, float r, float g, float b, float a)
	{
		ColorVertex vertex;
		vertex.x = x;
		vertex.y = y;
		vertex.r = r;
		vertex.g = g;
		vertex.b = b;
		vertex.a = a;

		return vertex;
	}

	std::wstring FormatSeconds(float seconds)
	{
		if (seconds >= 1.f)
		{
			return std::to_wstring(static_cast<int32>(std::ceil(seconds)));
		}

		const int32 tenths = static_cast<int32>(std::ceil(seconds * 10.f));
		return L"0." + std::to_wstring(tenths);
	}

	void DrawHpBar(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		const float ratio = player.GetHpRatio();
		const float left = -HP_BAR_WIDTH * 0.5f;

		renderer->DrawSolidBox(0.f, HP_BAR_Y, HP_BAR_WIDTH + 6.f, 22.f, 0.03f, 0.025f, 0.02f, 0.95f);
		renderer->DrawSolidBox(0.f, HP_BAR_Y, HP_BAR_WIDTH, 16.f, 0.12f, 0.04f, 0.04f, 1.f);

		// 줄어든 만큼은 잠깐 밝은 잔상으로 남는다. 얼마나 맞았는지가 그 길이로 읽힌다
		if (g_hp_lag > ratio)
		{
			const float lag_width = HP_BAR_WIDTH * g_hp_lag;
			renderer->DrawSolidBox(left + lag_width * 0.5f, HP_BAR_Y, lag_width, 16.f, 0.95f, 0.75f, 0.55f, 1.f);
		}

		float r = 0.78f;
		float g = 0.12f;
		float b = 0.1f;
		if (ratio < LOW_HP_RATIO)
		{
			const float pulse = 0.5f + 0.5f * std::sin(g_time * 8.f);
			r = 0.8f + 0.2f * pulse;
			g = 0.1f + 0.1f * pulse;
		}

		const float fill = HP_BAR_WIDTH * std::max(ratio, 0.f);
		renderer->DrawSolidBox(left + fill * 0.5f, HP_BAR_Y, fill, 16.f, r, g, b, 1.f);
		renderer->DrawSolidBox(left + fill * 0.5f, HP_BAR_Y + 6.f, fill, 3.f, std::min(r * 1.4f, 1.f), g * 1.6f + 0.1f, b * 1.6f + 0.1f, 0.8f);

		const std::wstring text = std::to_wstring(player.m_hp) + L" / " + std::to_wstring(player.m_stat.max_hp);
		DrawStringOutline(renderer, text, 0.f, HP_BAR_Y + 12.f, TEXT_ALIGN_CENTER, 1, 1.f, 0.95f, 0.9f, 1.f);

		// 가드 게이지. 바닥나면 가드가 깨진다
		const float guard_ratio = player.GetGuardRatio();
		const float guard_y = HP_BAR_Y - 16.f;
		renderer->DrawSolidBox(0.f, guard_y, HP_BAR_WIDTH + 6.f, 8.f, 0.03f, 0.025f, 0.02f, 0.95f);

		float guard_r = 0.45f;
		float guard_g = 0.7f;
		float guard_b = 1.f;
		if (player.IsGuarding())
		{
			guard_r = 0.75f;
			guard_g = 0.9f;
			guard_b = 1.f;
		}

		const float guard_fill = HP_BAR_WIDTH * guard_ratio;
		renderer->DrawSolidBox(left + guard_fill * 0.5f, guard_y, guard_fill, 4.f, guard_r, guard_g, guard_b, 1.f);
	}

	int32 GetLeftClickIcon(bool* highlight)
	{
		const Player& player = GetPlayer();
		*highlight = false;

		if (player.m_action == ACTION_PARRY_SUCCESS && player.HasSkill(109))
		{
			*highlight = true;
			return SKILL_ICON_COUNTER;
		}

		const std::array<int32, 4> icons = { SKILL_ICON_SLASH, SKILL_ICON_SLASH_2, SKILL_ICON_COMBO, SKILL_ICON_SPIN_COMBO };
		int32 step = player.m_combo_step;
		if (step < 0 || step >= static_cast<int32>(icons.size()) || player.IsAttacking() == false)
		{
			step = 0;
		}

		return icons[static_cast<size_t>(step)];
	}

	void DrawActionBar(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		const float start_x = -ACTION_STEP * static_cast<float>(ACTION_SLOT_COUNT - 1) * 0.5f;

		renderer->DrawSolidBox(0.f, ACTION_Y, ACTION_STEP * static_cast<float>(ACTION_SLOT_COUNT) + 16.f, ACTION_STEP + 12.f, 0.02f, 0.018f, 0.016f, 0.75f);

		for (int32 index = 0; index < ACTION_SLOT_COUNT; ++index)
		{
			const ActionSlot& slot = ACTION_SLOTS[static_cast<size_t>(index)];
			const float x = start_x + ACTION_STEP * static_cast<float>(index);

			bool highlight = false;
			int32 icon = slot.icon;
			if (slot.skill_id == 101)
			{
				icon = GetLeftClickIcon(&highlight);
			}

			// 패링 판정이 살아 있으면 칸이 빛난다
			if (slot.skill_id == 103 && player.m_parry_timer > 0.f)
			{
				highlight = true;
			}

			DrawSlotBox(renderer, x, ACTION_Y, UI_SLOT, false, highlight);

			if (slot.skill_id == SKILL_ID_NONE)
			{
				// 물약 칸
				const int32 count = player.m_inventory.CountItem(ITEM_ID_HP_POTION);
				float shade = 1.f;
				if (count <= 0)
				{
					shade = 0.3f;
				}

				DrawSprite(renderer, SPRITE_ICON_POTION, 0, x, ACTION_Y - 16.f, UI_SPRITE_SCALE, false, shade, shade, shade, 1.f);
				DrawStringShadow(renderer, std::to_wstring(count), x + UI_SLOT * 0.5f - 2.f, ACTION_Y - 2.f, TEXT_ALIGN_RIGHT, 1.f, 1.f, 0.95f, 1.f);

				const float cooldown = player.m_potion_cooldown;
				const float total = GetItemData(ITEM_ID_HP_POTION).cooldown_sec;
				if (cooldown > 0.f && total > 0.f)
				{
					const float cover = UI_SLOT * cooldown / total;
					renderer->DrawSolidBox(x, ACTION_Y + UI_SLOT * 0.5f - cover * 0.5f, UI_SLOT, cover, 0.f, 0.f, 0.f, 0.65f);
					DrawStringOutline(renderer, FormatSeconds(cooldown), x, ACTION_Y + 12.f, TEXT_ALIGN_CENTER, 1, 1.f, 1.f, 1.f, 1.f);
				}
			}
			else
			{
				const bool owned = player.HasSkill(slot.skill_id);
				float shade = 1.f;
				if (owned == false)
				{
					shade = 0.22f;
				}

				DrawSprite(renderer, SPRITE_SKILL_ICON, icon, x, ACTION_Y - 16.f, UI_SPRITE_SCALE, false, shade, shade, shade, 1.f);

				const float cooldown = player.GetSkillCooldown(slot.skill_id);
				const float total = GetSkillData(slot.skill_id).cooldown_sec;
				if (owned && cooldown > 0.f && total > 0.f)
				{
					// 위에서부터 걷히는 덮개
					const float cover = UI_SLOT * cooldown / total;
					renderer->DrawSolidBox(x, ACTION_Y + UI_SLOT * 0.5f - cover * 0.5f, UI_SLOT, cover, 0.f, 0.f, 0.f, 0.65f);

					if (total >= 1.5f)
					{
						DrawStringOutline(renderer, FormatSeconds(cooldown), x, ACTION_Y + 12.f, TEXT_ALIGN_CENTER, 1, 1.f, 1.f, 1.f, 1.f);
					}
				}

				if (owned == false)
				{
					// 잠긴 칸은 자물쇠 모양만. 글자는 칸보다 넓다
					renderer->DrawSolidBox(x, ACTION_Y - 4.f, 12.f, 10.f, 0.45f, 0.4f, 0.32f, 1.f);
					renderer->DrawSolidBox(x, ACTION_Y + 4.f, 8.f, 8.f, 0.45f, 0.4f, 0.32f, 1.f);
					renderer->DrawSolidBox(x, ACTION_Y + 4.f, 4.f, 6.f, 0.03f, 0.028f, 0.026f, 1.f);
				}
			}

			DrawStringShadow(renderer, slot.key, x, ACTION_Y + UI_SLOT * 0.5f + 26.f, TEXT_ALIGN_CENTER, 0.85f, 0.8f, 0.65f, 1.f);
		}
	}

	void DrawExpBar(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		const float width = static_cast<float>(WINDOW_SIZE_X) - 120.f;
		const float left = -width * 0.5f + 50.f;

		const int32 need = GetNeedExp(player.m_level);
		float ratio = 1.f;
		if (need > 0)
		{
			ratio = static_cast<float>(player.m_exp) / static_cast<float>(need);
		}

		DrawBarLeft(renderer, left, EXP_Y, width, 4.f, ratio, 0.55f, 0.3f, 0.9f);
		DrawStringShadow(renderer, L"Lv " + std::to_wstring(player.m_level), left - 8.f, EXP_Y + 20.f, TEXT_ALIGN_RIGHT, 0.95f, 0.9f, 1.f, 1.f);

		const int32 percent = static_cast<int32>(ratio * 100.f);
		DrawString(renderer, std::to_wstring(percent) + L"%", left + width + 8.f, EXP_Y + 12.f, TEXT_ALIGN_LEFT, 0.7f, 0.62f, 0.85f, 1.f);
	}

	void DrawWallet(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		const float x = -static_cast<float>(WINDOW_SIZE_X) * 0.5f + 30.f;
		const float y = ACTION_Y + 52.f;

		DrawSprite(renderer, SPRITE_ICON_GOLD, 0, x, y - 16.f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, 1.f);
		DrawStringShadow(renderer, std::to_wstring(player.m_gold), x + 22.f, y + 12.f, TEXT_ALIGN_LEFT, 1.f, 0.84f, 0.35f, 1.f);

		DrawSprite(renderer, SPRITE_ICON_TRACE, 0, x, y - 56.f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, 1.f);
		DrawStringShadow(renderer, std::to_wstring(player.m_inventory.CountItem(ITEM_ID_SPELL_TRACE)), x + 22.f, y - 28.f, TEXT_ALIGN_LEFT, 0.78f, 0.7f, 1.f, 1.f);
	}

	void DrawPointBadges(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		const float x = static_cast<float>(WINDOW_SIZE_X) * 0.5f - 24.f;
		float y = ACTION_Y + 14.f;
		const float pulse = GetUiPulse();

		if (player.m_skill_point > 0)
		{
			DrawStringShadow(renderer, L"스킬 포인트 " + std::to_wstring(player.m_skill_point) + L"  ( K )", x, y, TEXT_ALIGN_RIGHT, pulse, 0.85f * pulse, 0.3f, 1.f);
			y -= 26.f;
		}

		if (player.m_stat_point > 0)
		{
			DrawStringShadow(renderer, L"능력치 포인트 " + std::to_wstring(player.m_stat_point) + L"  ( C )", x, y, TEXT_ALIGN_RIGHT, pulse, 0.85f * pulse, 0.3f, 1.f);
			y -= 26.f;
		}

		if (player.HasWeapon() == false)
		{
			DrawStringShadow(renderer, L"가방 ( I ) 에서 검을 장착하세요", x, y, TEXT_ALIGN_RIGHT, 1.f, 0.6f * pulse + 0.3f, 0.35f, 1.f);
		}
	}

	void DrawFocus(Renderer* renderer)
	{
		float boss_ratio = 0.f;
		if (GetBossHpRatio(&boss_ratio))
		{
			const float width = 520.f;
			const float y = GetTopEdge() - 44.f;

			DrawStringOutline(renderer, GetBossName(), 0.f, y + 30.f, TEXT_ALIGN_CENTER, 1, 1.f, 0.55f, 0.4f, 1.f);
			renderer->DrawSolidBox(0.f, y, width + 6.f, 16.f, 0.03f, 0.02f, 0.02f, 0.95f);
			renderer->DrawSolidBox(-width * 0.5f + width * boss_ratio * 0.5f, y, width * boss_ratio, 10.f, 0.75f, 0.12f, 0.08f, 1.f);
			return;
		}

		MonsterFocus focus;
		if (GetFocusMonster(&focus) == false)
		{
			return;
		}

		const float width = 240.f;
		const float y = GetTopEdge() - 40.f;

		float name_r = 0.95f;
		float name_g = 0.9f;
		float name_b = 0.85f;
		if (focus.is_elite || focus.grade == GRADE_ELITE)
		{
			name_r = 1.f;
			name_g = 0.65f;
			name_b = 0.25f;
		}

		const std::wstring title = focus.name + L"  Lv " + std::to_wstring(focus.level);
		DrawStringOutline(renderer, title, 0.f, y + 28.f, TEXT_ALIGN_CENTER, 1, name_r, name_g, name_b, 1.f);

		renderer->DrawSolidBox(0.f, y, width + 4.f, 10.f, 0.03f, 0.02f, 0.02f, 0.9f);
		renderer->DrawSolidBox(-width * 0.5f + width * focus.hp_ratio * 0.5f, y, width * focus.hp_ratio, 6.f, 0.8f, 0.12f, 0.08f, 1.f);

		if (focus.is_groggy)
		{
			const float pulse = GetUiPulse();
			DrawStringOutline(renderer, L"그로기", 0.f, y - 10.f, TEXT_ALIGN_CENTER, 1, 1.f, 0.9f * pulse, 0.3f, 1.f);
		}
	}

	// 체력이 낮으면 화면 가장자리가 붉게 맥박친다
	void DrawLowHpEdge(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		const float ratio = player.GetHpRatio();
		if (ratio >= LOW_HP_RATIO || player.IsDead())
		{
			return;
		}

		const float strength = (1.f - ratio / LOW_HP_RATIO) * (0.35f + 0.25f * std::sin(g_time * 6.f));
		const float half_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f;
		const float half_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
		const float band = 90.f;

		const float r = 0.7f;
		const float g = 0.f;
		const float b = 0.f;

		renderer->DrawQuad(MakeVertex(-half_x, half_y, r, g, b, strength), MakeVertex(half_x, half_y, r, g, b, strength), MakeVertex(half_x, half_y - band, r, g, b, 0.f), MakeVertex(-half_x, half_y - band, r, g, b, 0.f));
		renderer->DrawQuad(MakeVertex(-half_x, -half_y + band, r, g, b, 0.f), MakeVertex(half_x, -half_y + band, r, g, b, 0.f), MakeVertex(half_x, -half_y, r, g, b, strength), MakeVertex(-half_x, -half_y, r, g, b, strength));
		renderer->DrawQuad(MakeVertex(-half_x, half_y, r, g, b, strength), MakeVertex(-half_x + band, half_y, r, g, b, 0.f), MakeVertex(-half_x + band, -half_y, r, g, b, 0.f), MakeVertex(-half_x, -half_y, r, g, b, strength));
		renderer->DrawQuad(MakeVertex(half_x - band, half_y, r, g, b, 0.f), MakeVertex(half_x, half_y, r, g, b, strength), MakeVertex(half_x, -half_y, r, g, b, strength), MakeVertex(half_x - band, -half_y, r, g, b, 0.f));
	}

	void DrawDeathOverlay(Renderer* renderer)
	{
		const Player& player = GetPlayer();
		if (player.IsDead() == false)
		{
			return;
		}

		renderer->DrawSolidBox(0.f, 0.f, static_cast<float>(WINDOW_SIZE_X), static_cast<float>(WINDOW_SIZE_Y), 0.08f, 0.f, 0.f, 0.45f);
		DrawStringOutline(renderer, L"쓰러졌다", 0.f, 70.f, TEXT_ALIGN_CENTER, 2, 0.9f, 0.25f, 0.2f, 1.f);
		DrawStringShadow(renderer, L"성채에서 다시 일어선다", 0.f, 10.f, TEXT_ALIGN_CENTER, 0.8f, 0.72f, 0.65f, 1.f);
	}

	void DrawIntroHint(Renderer* renderer)
	{
		if (g_time > INTRO_HINT_SEC)
		{
			return;
		}

		const float alpha = std::min(1.f, (INTRO_HINT_SEC - g_time) / 2.f);
		const float y = GetTopEdge() - 150.f;

		DrawStringOutline(renderer, L"WASD 이동   좌클릭 공격   우클릭 패링 ( 누르고 있으면 가드 )", 0.f, y, TEXT_ALIGN_CENTER, 1, 0.95f, 0.9f, 0.78f, alpha);
		DrawStringOutline(renderer, L"적의 칼날이 번쩍이는 순간 우클릭   H 조작 도움말", 0.f, y - 26.f, TEXT_ALIGN_CENTER, 1, 0.85f, 0.8f, 0.7f, alpha);
	}

	void DrawKeyHelp(Renderer* renderer)
	{
		if (g_show_help == false)
		{
			return;
		}

		const std::array<const wchar_t*, 18> lines =
		{
			L"WASD        이동",
			L"마우스       조준",
			L"좌클릭       공격 ( 연타 4 단 )",
			L"우클릭       패링 / 누르면 가드",
			L"패링 성공 뒤 좌클릭   반격",
			L"Q  E  R      스킬 ( 스킬 트리 해금 )",
			L"Space        대쉬",
			L"Shift        구르기",
			L"1            체력 물약",
			L"F            줍기 / 모루",
			L"I  C  K      가방 / 능력치 / 스킬",
			L"U            강화 ( 모루 앞 )",
			L"M            지도",
			L"휠클릭  Tab   NPC 관찰",
			L"O            관찰 NPC 따라가기",
			L"B            모니터 ( 브라우저 )",
			L"-  =         소리 크기",
			L"Esc          창 닫기",
		};

		const float width = 330.f;
		const float height = static_cast<float>(lines.size()) * 24.f + 56.f;
		const float x = static_cast<float>(WINDOW_SIZE_X) * 0.5f - width * 0.5f - 16.f;
		const float y = 40.f;

		DrawPanel(renderer, x, y, width, height, L"조작  ( H )");

		float line_y = y + height * 0.5f - 46.f;
		for (const wchar_t* line : lines)
		{
			DrawString(renderer, line, x - width * 0.5f + 16.f, line_y, TEXT_ALIGN_LEFT, 0.85f, 0.82f, 0.74f, 1.f);
			line_y -= 24.f;
		}
	}
}

void UpdateHud(float real_seconds)
{
	g_time += real_seconds;

	const float ratio = GetPlayer().GetHpRatio();
	if (g_hp_lag < ratio)
	{
		g_hp_lag = ratio;
	}
	else
	{
		g_hp_lag = std::max(ratio, g_hp_lag - LAG_SPEED * real_seconds);
	}

	UpdateUiWindows(real_seconds);
}

void DrawHud(Renderer* renderer)
{
	BeginProfileStage(PROFILE_STAGE_HUD);

	DrawLowHpEdge(renderer);
	DrawFocus(renderer);
	DrawHpBar(renderer);
	DrawActionBar(renderer);
	DrawExpBar(renderer);
	DrawWallet(renderer);
	DrawPointBadges(renderer);
	DrawIntroHint(renderer);

	DrawMinimap(renderer, IsAnyWindowOpen() || GetSelectedNpc() >= 0);
	DrawNpcPanels(renderer, IsAnyWindowOpen());

	DrawKeyHelp(renderer);
	DrawUiWindows(renderer);
	DrawDeathOverlay(renderer);

	EndProfileStage();
}

void ToggleKeyHelp()
{
	g_show_help = !g_show_help;
}
