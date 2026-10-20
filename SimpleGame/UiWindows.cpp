#include "stdafx.h"
#include "UiWindows.h"
#include "Audio.h"
#include "Enhance.h"
#include "Feel.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Player.h"
#include "Renderer.h"
#include "Scene.h"
#include "SpriteAtlas.h"
#include "Text.h"
#include "UiDraw.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	// 가방
	const float INVENTORY_X = 420.f;
	const float INVENTORY_Y = 10.f;
	const float INVENTORY_WIDTH = 270.f;
	const float INVENTORY_HEIGHT = 480.f;
	const float SLOT_STEP = 48.f;
	constexpr int32 GRID_COLUMNS = 5;
	const float DOUBLE_CLICK_SEC = 0.32f;
	const float DRAG_START_PIXEL = 6.f;

	constexpr int32 TAB_ALL = 0;
	constexpr int32 TAB_EQUIPMENT = 1;
	constexpr int32 TAB_CONSUMABLE = 2;
	constexpr int32 TAB_ETC = 3;
	constexpr int32 TAB_COUNT = 4;

	// 능력치
	const float STAT_X = -450.f;
	const float STAT_Y = 30.f;
	const float STAT_WIDTH = 310.f;
	const float STAT_HEIGHT = 420.f;

	// 스킬 트리
	const float SKILL_X = -40.f;
	const float SKILL_Y = 30.f;
	const float SKILL_WIDTH = 640.f;
	const float SKILL_HEIGHT = 470.f;
	const float NODE_SIZE = 40.f;

	// 강화
	const float ENHANCE_X = 70.f;
	const float ENHANCE_Y = 30.f;
	const float ENHANCE_WIDTH = 360.f;
	const float ENHANCE_HEIGHT = 480.f;
	const float RESULT_SHOW_SEC = 1.6f;

	// 확인 창
	const float CONFIRM_Y = 80.f;
	const float CONFIRM_WIDTH = 320.f;
	const float CONFIRM_HEIGHT = 150.f;

	constexpr int32 DRAG_NONE = 0;
	constexpr int32 DRAG_BAG = 1;
	constexpr int32 DRAG_EQUIP = 2;

	constexpr int32 WINDOW_INVENTORY = 0;
	constexpr int32 WINDOW_STAT = 1;
	constexpr int32 WINDOW_SKILL = 2;
	constexpr int32 WINDOW_ENHANCE = 3;
	constexpr int32 WINDOW_COUNT = 4;

	struct SkillNode
	{
		int32 skill_id = SKILL_ID_NONE;
		float x = 0.f;	// 창 가운데 기준
		float y = 0.f;
		int32 icon = 0;
		const wchar_t* key = L"";
		const wchar_t* label = L"";	// 비어 있으면 스킬 이름. 연타 칸은 이름이 길어 겹친다
	};

	// 서버 SkillTree.json 의 간선을 그대로 쓰고 자리만 여기서 정한다
	const std::array<SkillNode, 11> SKILL_NODES =
	{
		SkillNode{ 101, -220.f, 86.f, SKILL_ICON_SLASH, L"좌", L"1 타" },
		SkillNode{ 102, -140.f, 86.f, SKILL_ICON_SLASH_2, L"좌", L"2 타" },
		SkillNode{ 104, -60.f, 86.f, SKILL_ICON_COMBO, L"좌", L"3 타" },
		SkillNode{ 105, 20.f, 86.f, SKILL_ICON_SPIN_COMBO, L"좌", L"4 타" },
		SkillNode{ 107, -220.f, -24.f, SKILL_ICON_SPIN_SLASH, L"E", L"" },
		SkillNode{ 106, -290.f, -134.f, SKILL_ICON_DASH_SLASH, L"Q", L"" },
		SkillNode{ 108, -130.f, -134.f, SKILL_ICON_LEAP, L"R", L"" },
		SkillNode{ 103, 170.f, 86.f, SKILL_ICON_PARRY, L"우", L"" },
		SkillNode{ 109, 170.f, -24.f, SKILL_ICON_COUNTER, L"좌", L"" },
		SkillNode{ 110, 110.f, -134.f, SKILL_ICON_DODGE, L"Spc", L"" },
		SkillNode{ 111, 230.f, -134.f, SKILL_ICON_ROLL, L"Sft", L"" },
	};

	std::array<bool, WINDOW_COUNT> g_open = {};
	std::array<int32, WINDOW_COUNT> g_open_order = {};	// 늦게 연 것이 위
	int32 g_open_serial = 0;

	float g_mouse_x = 0.f;
	float g_mouse_y = 0.f;

	int32 g_tab = TAB_ALL;

	int32 g_drag_kind = DRAG_NONE;
	int32 g_drag_index = -1;
	bool g_drag_started = false;
	float g_drag_from_x = 0.f;
	float g_drag_from_y = 0.f;

	int32 g_last_click_slot = -100;
	std::chrono::steady_clock::time_point g_last_click_time;

	bool g_confirm_open = false;
	int32 g_confirm_slot = -1;

	int32 g_enhance_result = ENHANCE_RESULT_NONE;
	float g_enhance_result_timer = 0.f;

	// ------------------------------------------------------------ 공통

	float GetLeft(float center_x, float width)
	{
		return center_x - width * 0.5f;
	}

	float GetTop(float center_y, float height)
	{
		return center_y + height * 0.5f;
	}

	void OpenWindow(int32 window)
	{
		g_open[static_cast<size_t>(window)] = true;
		g_open_serial += 1;
		g_open_order[static_cast<size_t>(window)] = g_open_serial;
		PlaySfx(SFX_UI_CLICK, 0.6f);
	}

	void CloseWindow(int32 window)
	{
		if (g_open[static_cast<size_t>(window)] == false)
		{
			return;
		}

		g_open[static_cast<size_t>(window)] = false;
		PlaySfx(SFX_UI_CLICK, 0.4f);

		if (window == WINDOW_INVENTORY)
		{
			g_drag_kind = DRAG_NONE;
			g_confirm_open = false;
		}
	}

	void ToggleWindow(int32 window)
	{
		if (g_open[static_cast<size_t>(window)])
		{
			CloseWindow(window);
			return;
		}

		OpenWindow(window);
	}

	bool IsMouseOverWindow(int32 window, float x, float y)
	{
		if (g_open[static_cast<size_t>(window)] == false)
		{
			return false;
		}

		switch (window)
		{
			case WINDOW_INVENTORY:
			{
				return IsInsideBox(x, y, INVENTORY_X, INVENTORY_Y, INVENTORY_WIDTH + 8.f, INVENTORY_HEIGHT + 8.f);
			}
			case WINDOW_STAT:
			{
				return IsInsideBox(x, y, STAT_X, STAT_Y, STAT_WIDTH + 8.f, STAT_HEIGHT + 8.f);
			}
			case WINDOW_SKILL:
			{
				return IsInsideBox(x, y, SKILL_X, SKILL_Y, SKILL_WIDTH + 8.f, SKILL_HEIGHT + 8.f);
			}
			case WINDOW_ENHANCE:
			{
				return IsInsideBox(x, y, ENHANCE_X, ENHANCE_Y, ENHANCE_WIDTH + 8.f, ENHANCE_HEIGHT + 8.f);
			}
			default:
			{
				return false;
			}
		}
	}

	// 겹친 창 중 맨 위
	int32 FindWindowAt(float x, float y)
	{
		int32 best = -1;
		int32 best_order = -1;

		for (int32 window = 0; window < WINDOW_COUNT; ++window)
		{
			if (IsMouseOverWindow(window, x, y) && g_open_order[static_cast<size_t>(window)] > best_order)
			{
				best = window;
				best_order = g_open_order[static_cast<size_t>(window)];
			}
		}

		return best;
	}

	// ------------------------------------------------------------ 가방

	int32 GetItemCategory(const ItemSlot& slot)
	{
		return GetItemData(slot.item_id).category;
	}

	bool MatchesTab(const ItemSlot& slot)
	{
		if (g_tab == TAB_ALL)
		{
			return true;
		}

		// 빈 칸은 어느 탭에도 나온다. 탭은 같은 가방을 걸러 보일 뿐이다 ( 클라 인벤토리 )
		if (slot.item_id == ITEM_ID_NONE || slot.count <= 0)
		{
			return true;
		}

		const int32 category = GetItemCategory(slot);
		if (g_tab == TAB_EQUIPMENT)
		{
			return category == ITEM_CATEGORY_EQUIPMENT;
		}

		if (g_tab == TAB_CONSUMABLE)
		{
			return category == ITEM_CATEGORY_CONSUMABLE;
		}

		return category == ITEM_CATEGORY_ETC;
	}

	// 보이는 칸 번호 -> 실제 가방 칸
	std::vector<int32> BuildVisibleSlots()
	{
		std::vector<int32> visible;
		const Inventory& inventory = GetPlayer().m_inventory;

		for (int32 slot_idx = 0; slot_idx < inventory.GetBagSize(); ++slot_idx)
		{
			if (MatchesTab(inventory.GetBag(slot_idx)))
			{
				visible.push_back(slot_idx);
			}
		}

		return visible;
	}

	float GetGridTop()
	{
		return GetTop(INVENTORY_Y, INVENTORY_HEIGHT) - 212.f;
	}

	void GetGridCenter(int32 visible_index, float* x, float* y)
	{
		const int32 column = visible_index % GRID_COLUMNS;
		const int32 row = visible_index / GRID_COLUMNS;

		*x = INVENTORY_X - SLOT_STEP * 2.f + static_cast<float>(column) * SLOT_STEP;
		*y = GetGridTop() - static_cast<float>(row) * SLOT_STEP;
	}

	void GetEquipCenter(float* x, float* y)
	{
		*x = INVENTORY_X - 82.f;
		*y = GetTop(INVENTORY_Y, INVENTORY_HEIGHT) - 88.f;
	}

	void GetTabCenter(int32 tab, float* x, float* y)
	{
		*x = INVENTORY_X - 96.f + static_cast<float>(tab) * 64.f;
		*y = GetTop(INVENTORY_Y, INVENTORY_HEIGHT) - 160.f;
	}

	int32 FindBagSlotAt(float x, float y)
	{
		const std::vector<int32> visible = BuildVisibleSlots();

		for (size_t index = 0; index < visible.size(); ++index)
		{
			float slot_x = 0.f;
			float slot_y = 0.f;
			GetGridCenter(static_cast<int32>(index), &slot_x, &slot_y);

			if (IsInsideBox(x, y, slot_x, slot_y, UI_SLOT + 4.f, UI_SLOT + 4.f))
			{
				return visible[index];
			}
		}

		return -1;
	}

	bool IsOverEquipSlot(float x, float y)
	{
		float equip_x = 0.f;
		float equip_y = 0.f;
		GetEquipCenter(&equip_x, &equip_y);

		return IsInsideBox(x, y, equip_x, equip_y, UI_SLOT + 4.f, UI_SLOT + 4.f);
	}

	const ItemSlot& GetDraggedSlot()
	{
		const Inventory& inventory = GetPlayer().m_inventory;

		if (g_drag_kind == DRAG_EQUIP)
		{
			return inventory.GetEquip(EQUIP_AREA_WEAPON);
		}

		return inventory.GetBag(g_drag_index);
	}

	bool IsDoubleClick(int32 slot_key)
	{
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		const std::chrono::duration<float> since = now - g_last_click_time;
		const bool is_double = slot_key == g_last_click_slot && since.count() <= DOUBLE_CLICK_SEC;

		g_last_click_slot = slot_key;
		g_last_click_time = now;

		if (is_double)
		{
			g_last_click_slot = -100;
		}

		return is_double;
	}

	bool HandleInventoryDown(int32 button, float x, float y)
	{
		// 탭
		for (int32 tab = 0; tab < TAB_COUNT; ++tab)
		{
			float tab_x = 0.f;
			float tab_y = 0.f;
			GetTabCenter(tab, &tab_x, &tab_y);

			if (IsInsideBox(x, y, tab_x, tab_y, 60.f, 26.f))
			{
				g_tab = tab;
				g_drag_kind = DRAG_NONE;
				PlaySfx(SFX_UI_CLICK, 0.5f);
				return true;
			}
		}

		const Inventory& inventory = GetPlayer().m_inventory;

		if (IsOverEquipSlot(x, y))
		{
			if (inventory.IsEmptySlot(inventory.GetEquip(EQUIP_AREA_WEAPON)))
			{
				return true;
			}

			if (button == UI_MOUSE_RIGHT || IsDoubleClick(1000))
			{
				UnequipPlayerItem(EQUIP_AREA_WEAPON, -1);
				return true;
			}

			g_drag_kind = DRAG_EQUIP;
			g_drag_index = EQUIP_AREA_WEAPON;
			g_drag_started = false;
			g_drag_from_x = x;
			g_drag_from_y = y;
			return true;
		}

		const int32 slot_idx = FindBagSlotAt(x, y);
		if (slot_idx < 0 || inventory.IsEmptySlot(inventory.GetBag(slot_idx)))
		{
			return true;
		}

		if (button == UI_MOUSE_RIGHT || IsDoubleClick(slot_idx))
		{
			UseBagItem(slot_idx);
			return true;
		}

		g_drag_kind = DRAG_BAG;
		g_drag_index = slot_idx;
		g_drag_started = false;
		g_drag_from_x = x;
		g_drag_from_y = y;

		return true;
	}

	void FinishDrag(float x, float y)
	{
		const int32 kind = g_drag_kind;
		const int32 from = g_drag_index;
		const bool started = g_drag_started;

		g_drag_kind = DRAG_NONE;
		g_drag_index = -1;

		if (started == false)
		{
			return;
		}

		Player& player = MutablePlayer();

		const int32 target_slot = FindBagSlotAt(x, y);
		if (target_slot >= 0)
		{
			if (kind == DRAG_BAG)
			{
				if (player.m_inventory.Move(from, target_slot))
				{
					PlaySfx(SFX_UI_CLICK, 0.7f);
				}

				return;
			}

			UnequipPlayerItem(EQUIP_AREA_WEAPON, target_slot);
			return;
		}

		if (IsOverEquipSlot(x, y))
		{
			if (kind == DRAG_BAG && GetItemData(player.m_inventory.GetBag(from).item_id).equip_area == EQUIP_AREA_WEAPON)
			{
				UseBagItem(from);
			}

			return;
		}

		// 창 밖에 놓으면 파괴를 묻는다. 낀 장비는 먼저 빼야 한다 ( 클라 인벤토리 )
		if (FindWindowAt(x, y) < 0 && kind == DRAG_BAG)
		{
			g_confirm_open = true;
			g_confirm_slot = from;
			PlaySfx(SFX_UI_CLICK, 0.5f);
		}
	}

	void DrawInventory(Renderer* renderer, std::vector<TooltipLine>* tooltip)
	{
		const Player& player = GetPlayer();
		const Inventory& inventory = player.m_inventory;
		const float left = GetLeft(INVENTORY_X, INVENTORY_WIDTH);
		const float top = GetTop(INVENTORY_Y, INVENTORY_HEIGHT);

		DrawPanel(renderer, INVENTORY_X, INVENTORY_Y, INVENTORY_WIDTH, INVENTORY_HEIGHT, L"가방  ( I )");

		// 장비칸과 요약
		float equip_x = 0.f;
		float equip_y = 0.f;
		GetEquipCenter(&equip_x, &equip_y);

		const ItemSlot& weapon = inventory.GetEquip(EQUIP_AREA_WEAPON);
		const bool weapon_empty = inventory.IsEmptySlot(weapon);
		const bool equip_hovered = IsOverEquipSlot(g_mouse_x, g_mouse_y);

		// 빈 무기칸은 숨 쉬듯 깜빡인다. 처음 받은 검을 여기 끼우라는 뜻
		DrawSlotBox(renderer, equip_x, equip_y, UI_SLOT, equip_hovered, weapon_empty);
		DrawStringShadow(renderer, L"무기", equip_x, equip_y + UI_SLOT * 0.5f + 26.f, TEXT_ALIGN_CENTER, 0.75f, 0.7f, 0.6f, 1.f);

		if (weapon_empty == false && (g_drag_kind != DRAG_EQUIP || g_drag_started == false))
		{
			DrawItemInSlot(renderer, weapon, equip_x, equip_y, 1.f);
		}

		if (equip_hovered && weapon_empty == false && g_drag_kind == DRAG_NONE)
		{
			BuildItemTooltip(weapon, tooltip);
		}

		const float summary_x = equip_x + 52.f;
		DrawStringShadow(renderer, L"공격력  " + std::to_wstring(player.m_stat.attack), summary_x, equip_y + 30.f, TEXT_ALIGN_LEFT, 0.95f, 0.65f, 0.3f, 1.f);
		DrawStringShadow(renderer, L"방어력  " + std::to_wstring(player.m_stat.defence), summary_x, equip_y + 6.f, TEXT_ALIGN_LEFT, 0.5f, 0.68f, 0.95f, 1.f);
		DrawStringShadow(renderer, L"치명타  " + std::to_wstring(player.m_stat.crit_rate) + L"%", summary_x, equip_y - 18.f, TEXT_ALIGN_LEFT, 0.85f, 0.82f, 0.75f, 1.f);

		// 탭
		const std::array<const wchar_t*, TAB_COUNT> tab_names = { L"전체", L"장비", L"소비", L"기타" };
		for (int32 tab = 0; tab < TAB_COUNT; ++tab)
		{
			float tab_x = 0.f;
			float tab_y = 0.f;
			GetTabCenter(tab, &tab_x, &tab_y);

			float shade = 0.12f;
			float text = 0.6f;
			if (tab == g_tab)
			{
				shade = 0.3f;
				text = 1.f;
			}

			renderer->DrawSolidBox(tab_x, tab_y, 60.f, 26.f, shade, shade * 0.8f, shade * 0.55f, 1.f);
			DrawStringShadow(renderer, tab_names[static_cast<size_t>(tab)], tab_x, tab_y + 12.f, TEXT_ALIGN_CENTER, text, text * 0.92f, text * 0.78f, 1.f);
		}

		// 칸
		const std::vector<int32> visible = BuildVisibleSlots();
		const int32 hovered_slot = FindBagSlotAt(g_mouse_x, g_mouse_y);

		for (size_t index = 0; index < visible.size(); ++index)
		{
			const int32 slot_idx = visible[index];
			float slot_x = 0.f;
			float slot_y = 0.f;
			GetGridCenter(static_cast<int32>(index), &slot_x, &slot_y);

			const bool hovered = slot_idx == hovered_slot;
			DrawSlotBox(renderer, slot_x, slot_y, UI_SLOT, hovered, false);

			const bool being_dragged = g_drag_kind == DRAG_BAG && g_drag_started && g_drag_index == slot_idx;
			float alpha = 1.f;
			if (being_dragged)
			{
				alpha = 0.3f;
			}

			DrawItemInSlot(renderer, inventory.GetBag(slot_idx), slot_x, slot_y, alpha);

			if (hovered && g_drag_kind == DRAG_NONE)
			{
				BuildItemTooltip(inventory.GetBag(slot_idx), tooltip);
			}
		}

		// 아래 줄
		const float bottom = INVENTORY_Y - INVENTORY_HEIGHT * 0.5f;
		DrawSprite(renderer, SPRITE_ICON_GOLD, 0, left + 26.f, bottom + 12.f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, 1.f);
		DrawStringShadow(renderer, std::to_wstring(player.m_gold), left + 48.f, bottom + 38.f, TEXT_ALIGN_LEFT, 1.f, 0.84f, 0.35f, 1.f);

		int32 used = 0;
		for (int32 slot_idx = 0; slot_idx < inventory.GetBagSize(); ++slot_idx)
		{
			if (inventory.IsEmptySlot(inventory.GetBag(slot_idx)) == false)
			{
				used += 1;
			}
		}

		const std::wstring capacity = std::to_wstring(used) + L" / " + std::to_wstring(inventory.GetBagSize());
		DrawStringShadow(renderer, capacity, INVENTORY_X + INVENTORY_WIDTH * 0.5f - 14.f, bottom + 38.f, TEXT_ALIGN_RIGHT, 0.7f, 0.66f, 0.6f, 1.f);
	}

	void DrawDraggedItem(Renderer* renderer)
	{
		if (g_drag_kind == DRAG_NONE || g_drag_started == false)
		{
			return;
		}

		DrawItemInSlot(renderer, GetDraggedSlot(), g_mouse_x, g_mouse_y, 0.85f);
	}

	void DrawConfirm(Renderer* renderer)
	{
		if (g_confirm_open == false)
		{
			return;
		}

		const ItemSlot& slot = GetPlayer().m_inventory.GetBag(g_confirm_slot);
		if (slot.item_id == ITEM_ID_NONE)
		{
			g_confirm_open = false;
			return;
		}

		// 뒤를 어둡게 눌러 다른 데를 못 누르는 창이라는 걸 보여 준다
		renderer->DrawSolidBox(0.f, 0.f, static_cast<float>(WINDOW_SIZE_X), static_cast<float>(WINDOW_SIZE_Y), 0.f, 0.f, 0.f, 0.45f);
		DrawPanel(renderer, 0.f, CONFIRM_Y, CONFIRM_WIDTH, CONFIRM_HEIGHT, L"");

		std::wstring name = GetItemData(slot.item_id).name;
		if (slot.count > 1)
		{
			name += L" x" + std::to_wstring(slot.count);
		}

		DrawStringShadow(renderer, name, 0.f, CONFIRM_Y + 56.f, TEXT_ALIGN_CENTER, 1.f, 0.86f, 0.55f, 1.f);
		DrawStringShadow(renderer, L"파괴하시겠습니까?", 0.f, CONFIRM_Y + 28.f, TEXT_ALIGN_CENTER, 0.95f, 0.92f, 0.85f, 1.f);

		DrawButton(renderer, -70.f, CONFIRM_Y - 34.f, 100.f, 34.f, L"예", true, IsInsideBox(g_mouse_x, g_mouse_y, -70.f, CONFIRM_Y - 34.f, 100.f, 34.f));
		DrawButton(renderer, 70.f, CONFIRM_Y - 34.f, 100.f, 34.f, L"아니오", true, IsInsideBox(g_mouse_x, g_mouse_y, 70.f, CONFIRM_Y - 34.f, 100.f, 34.f));
	}

	bool HandleConfirmDown(float x, float y)
	{
		if (IsInsideBox(x, y, -70.f, CONFIRM_Y - 34.f, 100.f, 34.f))
		{
			MutablePlayer().m_inventory.Destroy(g_confirm_slot);
			PlaySfx(SFX_ENHANCE_FAIL, 0.5f);
			g_confirm_open = false;
			return true;
		}

		if (IsInsideBox(x, y, 70.f, CONFIRM_Y - 34.f, 100.f, 34.f))
		{
			g_confirm_open = false;
			PlaySfx(SFX_UI_CLICK, 0.5f);
			return true;
		}

		// 확인 창이 떠 있으면 다른 곳은 다 막는다
		return true;
	}

	// ------------------------------------------------------------ 능력치

	float GetStatRowY(int32 stat)
	{
		return GetTop(STAT_Y, STAT_HEIGHT) - 146.f - static_cast<float>(stat) * 44.f;
	}

	const float STAT_PLUS_X = STAT_X + STAT_WIDTH * 0.5f - 34.f;

	bool HandleStatDown(float x, float y)
	{
		for (int32 stat = 0; stat < STAT_COUNT; ++stat)
		{
			if (IsInsideBox(x, y, STAT_PLUS_X, GetStatRowY(stat), 30.f, 30.f))
			{
				if (MutablePlayer().TryStatUp(stat))
				{
					PlaySfx(SFX_UI_CLICK, 0.8f);
				}
				else
				{
					PlaySfx(SFX_UI_ERROR, 0.5f);
				}

				return true;
			}
		}

		return true;
	}

	int32 GetStatValue(const Player& player, int32 stat)
	{
		if (stat == STAT_MAX_HP)
		{
			return player.m_stat.max_hp;
		}

		if (stat == STAT_ATTACK)
		{
			return player.m_stat.attack;
		}

		return player.m_stat.defence;
	}

	void DrawStatWindow(Renderer* renderer, std::vector<TooltipLine>* tooltip)
	{
		const Player& player = GetPlayer();
		const float left = GetLeft(STAT_X, STAT_WIDTH);
		const float top = GetTop(STAT_Y, STAT_HEIGHT);

		DrawPanel(renderer, STAT_X, STAT_Y, STAT_WIDTH, STAT_HEIGHT, L"능력치  ( C )");

		DrawStringShadow(renderer, L"레벨 " + std::to_wstring(player.m_level), left + 18.f, top - 46.f, TEXT_ALIGN_LEFT, 0.95f, 0.9f, 1.f, 1.f);

		const int32 need = GetNeedExp(player.m_level);
		std::wstring exp_text = L"최고 레벨";
		float exp_ratio = 1.f;
		if (need > 0)
		{
			exp_text = std::to_wstring(player.m_exp) + L" / " + std::to_wstring(need);
			exp_ratio = static_cast<float>(player.m_exp) / static_cast<float>(need);
		}

		DrawStringShadow(renderer, exp_text, STAT_X + STAT_WIDTH * 0.5f - 18.f, top - 46.f, TEXT_ALIGN_RIGHT, 0.65f, 0.85f, 0.65f, 1.f);
		DrawBarLeft(renderer, left + 18.f, top - 78.f, STAT_WIDTH - 36.f, 4.f, exp_ratio, 0.55f, 0.35f, 0.85f);

		const bool has_point = player.m_stat_point > 0;
		const std::wstring point_text = L"남은 포인트  " + std::to_wstring(player.m_stat_point);
		if (has_point)
		{
			const float pulse = GetUiPulse();
			DrawStringShadow(renderer, point_text, left + 18.f, top - 90.f, TEXT_ALIGN_LEFT, pulse, 0.85f * pulse, 0.3f, 1.f);
		}
		else
		{
			DrawStringShadow(renderer, point_text, left + 18.f, top - 90.f, TEXT_ALIGN_LEFT, 0.5f, 0.48f, 0.44f, 1.f);
		}

		const GameConfig& config = GetConfig();
		const std::array<std::array<float, 3>, STAT_COUNT> colors = { { { 0.9f, 0.3f, 0.25f }, { 0.95f, 0.62f, 0.25f }, { 0.45f, 0.65f, 0.95f } } };
		const std::array<int32, STAT_COUNT> per_point = { config.point_max_hp, config.point_attack, config.point_defence };

		for (int32 stat = 0; stat < STAT_COUNT; ++stat)
		{
			const float row_y = GetStatRowY(stat);
			const std::array<float, 3>& color = colors[static_cast<size_t>(stat)];

			renderer->DrawSolidBox(STAT_X, row_y, STAT_WIDTH - 24.f, 38.f, 0.07f, 0.06f, 0.05f, 1.f);
			DrawStringShadow(renderer, GetStatName(stat), left + 22.f, row_y + 12.f, TEXT_ALIGN_LEFT, color[0], color[1], color[2], 1.f);
			DrawStringShadow(renderer, std::to_wstring(GetStatValue(player, stat)), left + 120.f, row_y + 12.f, TEXT_ALIGN_LEFT, 0.95f, 0.92f, 0.85f, 1.f);
			DrawString(renderer, L"찍음 " + std::to_wstring(player.m_stat_invest[static_cast<size_t>(stat)]), left + 180.f, row_y + 10.f, TEXT_ALIGN_LEFT, 0.5f, 0.48f, 0.44f, 1.f);

			const bool hovered = IsInsideBox(g_mouse_x, g_mouse_y, STAT_PLUS_X, row_y, 30.f, 30.f);
			DrawButton(renderer, STAT_PLUS_X, row_y, 28.f, 28.f, L"+", has_point, hovered);

			if (hovered)
			{
				TooltipLine line;
				line.text = GetStatName(stat) + std::wstring(L" +") + std::to_wstring(per_point[static_cast<size_t>(stat)]) + L" ( 포인트 1 )";
				tooltip->push_back(line);
			}
		}

		// 레벨과 포인트로 안 오르는 것
		const float derived_y = GetStatRowY(STAT_COUNT) - 6.f;
		const std::array<std::wstring, 4> derived =
		{
			L"치명타 확률   " + std::to_wstring(player.m_stat.crit_rate) + L"%",
			L"치명타 피해   " + std::to_wstring(player.m_stat.crit_damage) + L"%",
			L"관통력         " + std::to_wstring(player.m_stat.penetration),
			L"가드 게이지   " + std::to_wstring(static_cast<int32>(player.m_guard.gage_max)),
		};

		for (size_t index = 0; index < derived.size(); ++index)
		{
			DrawString(renderer, derived[index], left + 22.f, derived_y - static_cast<float>(index) * 24.f, TEXT_ALIGN_LEFT, 0.72f, 0.7f, 0.64f, 1.f);
		}

		DrawString(renderer, L"레벨업마다 포인트 " + std::to_wstring(config.level_up_stat_point), STAT_X, STAT_Y - STAT_HEIGHT * 0.5f + 30.f, TEXT_ALIGN_CENTER, 0.48f, 0.46f, 0.42f, 1.f);
	}

	// ------------------------------------------------------------ 스킬 트리

	const SkillNode* FindNode(int32 skill_id)
	{
		for (const SkillNode& node : SKILL_NODES)
		{
			if (node.skill_id == skill_id)
			{
				return &node;
			}
		}

		return nullptr;
	}

	const SkillNode* FindNodeAt(float x, float y)
	{
		for (const SkillNode& node : SKILL_NODES)
		{
			if (IsInsideBox(x, y, SKILL_X + node.x, SKILL_Y + node.y, NODE_SIZE + 6.f, NODE_SIZE + 6.f))
			{
				return &node;
			}
		}

		return nullptr;
	}

	bool HandleSkillDown(float x, float y)
	{
		const SkillNode* node = FindNodeAt(x, y);
		if (node == nullptr)
		{
			return true;
		}

		Player& player = MutablePlayer();
		const int32 check = player.CheckUnlockSkill(node->skill_id);

		if (check == UNLOCK_OK && player.TryUnlockSkill(node->skill_id))
		{
			AddScreenFlash(0.9f, 0.8f, 0.5f, 0.15f);
			PlaySfx(SFX_SKILL_UNLOCK, 0.9f);
			AddDamageText(player.m_pos, 60.f, GetSkillData(node->skill_id).alias + L" 해금", DAMAGE_STYLE_BANNER_GOOD);
			return true;
		}

		if (check != UNLOCK_ALREADY_OWNED)
		{
			PlaySfx(SFX_UI_ERROR, 0.5f);
		}

		return true;
	}

	std::wstring FormatRate(float value)
	{
		const int32 tenths = static_cast<int32>(std::lround(value * 10.f));
		return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10);
	}

	void BuildSkillTooltip(int32 skill_id, std::vector<TooltipLine>* lines)
	{
		const SkillData& skill = GetSkillData(skill_id);
		const Player& player = GetPlayer();

		TooltipLine title;
		title.text = skill.alias;
		title.r = 1.f;
		title.g = 0.86f;
		title.b = 0.55f;
		lines->push_back(title);

		if (skill.hit_windows.empty() == false)
		{
			float total = 0.f;
			for (const HitWindow& window : skill.hit_windows)
			{
				total += window.damage_rate;
			}

			TooltipLine damage;
			damage.text = L"공격력 x" + FormatRate(total);
			if (skill.hit_windows.size() > 1)
			{
				damage.text += L"  ( " + std::to_wstring(skill.hit_windows.size()) + L"회 )";
			}

			lines->push_back(damage);
		}

		if (skill.skill_type == SKILL_TYPE_PARRY)
		{
			TooltipLine parry;
			parry.text = L"누른 뒤 " + FormatRate(skill.parry_sec) + L"초 패링, 누르고 있으면 가드";
			lines->push_back(parry);
		}

		if (skill.skill_type == SKILL_TYPE_PARRY_ATTACK)
		{
			TooltipLine counter;
			counter.text = L"패링 성공 직후 좌클릭";
			lines->push_back(counter);
		}

		if (skill.stun_sec > 0.f)
		{
			TooltipLine stun;
			stun.text = L"스턴 " + FormatRate(skill.stun_sec) + L"초";
			stun.r = 1.f;
			stun.g = 0.85f;
			stun.b = 0.4f;
			lines->push_back(stun);
		}

		if (skill.knockback.movements.empty() == false)
		{
			TooltipLine knockback;
			knockback.text = L"넉백";
			lines->push_back(knockback);
		}

		if (skill.cooldown_sec > 0.f)
		{
			TooltipLine cooldown;
			cooldown.text = L"재사용 " + FormatRate(skill.cooldown_sec) + L"초";
			cooldown.r = 0.6f;
			cooldown.g = 0.75f;
			cooldown.b = 1.f;
			lines->push_back(cooldown);
		}

		TooltipLine state;
		state.r = 0.55f;
		state.g = 0.52f;
		state.b = 0.48f;

		switch (player.CheckUnlockSkill(skill_id))
		{
			case UNLOCK_ALREADY_OWNED:
			{
				state.text = L"보유";
				break;
			}
			case UNLOCK_OK:
			{
				state.text = L"클릭: 스킬 포인트 1 로 해금";
				state.r = 1.f;
				state.g = 0.9f;
				state.b = 0.45f;
				break;
			}
			case UNLOCK_NO_POINT:
			{
				state.text = L"스킬 포인트가 없다";
				break;
			}
			default:
			{
				state.text = L"이어진 스킬을 먼저 해금";
				break;
			}
		}

		lines->push_back(state);
	}

	void DrawSkillWindow(Renderer* renderer, std::vector<TooltipLine>* tooltip)
	{
		const Player& player = GetPlayer();
		const float top = GetTop(SKILL_Y, SKILL_HEIGHT);

		DrawPanel(renderer, SKILL_X, SKILL_Y, SKILL_WIDTH, SKILL_HEIGHT, L"스킬 트리  ( K )");

		const std::wstring point_text = L"스킬 포인트  " + std::to_wstring(player.m_skill_point);
		if (player.m_skill_point > 0)
		{
			const float pulse = GetUiPulse();
			DrawStringShadow(renderer, point_text, SKILL_X, top - 44.f, TEXT_ALIGN_CENTER, pulse, 0.85f * pulse, 0.3f, 1.f);
		}
		else
		{
			DrawStringShadow(renderer, point_text, SKILL_X, top - 44.f, TEXT_ALIGN_CENTER, 0.55f, 0.52f, 0.48f, 1.f);
		}

		// 좌클릭 연계 순서 ( 트리 간선이 아니라 콤보 화살표 )
		const std::vector<int32>& combo = GetComboSkillIds();
		for (size_t index = 0; index + 1 < combo.size(); ++index)
		{
			const SkillNode* from = FindNode(combo[index]);
			const SkillNode* to = FindNode(combo[index + 1]);
			if (from == nullptr || to == nullptr)
			{
				continue;
			}

			DrawLine(renderer, SKILL_X + from->x + NODE_SIZE * 0.5f, SKILL_Y + from->y, SKILL_X + to->x - NODE_SIZE * 0.5f, SKILL_Y + to->y, 3.f, 0.35f, 0.32f, 0.28f, 1.f);
		}

		// 트리 간선. 한쪽이라도 가졌으면 밝게
		for (const std::array<int32, 2>& edge : GetSkillTreeEdges())
		{
			const SkillNode* from = FindNode(edge[0]);
			const SkillNode* to = FindNode(edge[1]);
			if (from == nullptr || to == nullptr)
			{
				continue;
			}

			float shade = 0.25f;
			if (player.HasSkill(edge[0]) && player.HasSkill(edge[1]))
			{
				shade = 0.85f;
			}
			else if (player.HasSkill(edge[0]) || player.HasSkill(edge[1]))
			{
				shade = 0.5f;
			}

			DrawLine(renderer, SKILL_X + from->x, SKILL_Y + from->y, SKILL_X + to->x, SKILL_Y + to->y, 4.f, shade, shade * 0.78f, shade * 0.45f, 1.f);
		}

		const SkillNode* hovered = FindNodeAt(g_mouse_x, g_mouse_y);

		for (const SkillNode& node : SKILL_NODES)
		{
			const float x = SKILL_X + node.x;
			const float y = SKILL_Y + node.y;
			const int32 check = player.CheckUnlockSkill(node.skill_id);
			const bool owned = check == UNLOCK_ALREADY_OWNED;
			const bool can_unlock = check == UNLOCK_OK;

			DrawSlotBox(renderer, x, y, NODE_SIZE, &node == hovered, can_unlock);

			float brightness = 0.3f;
			if (owned)
			{
				brightness = 1.f;
			}
			else if (can_unlock)
			{
				brightness = 0.6f + 0.3f * GetUiPulse();
			}

			DrawSprite(renderer, SPRITE_SKILL_ICON, node.icon, x, y - 16.f, UI_SPRITE_SCALE, false, brightness, brightness, brightness, 1.f);
			DrawString(renderer, node.key, x, y + NODE_SIZE * 0.5f + 26.f, TEXT_ALIGN_CENTER, 0.75f, 0.7f, 0.6f, 1.f);

			float name_shade = 0.45f;
			if (owned)
			{
				name_shade = 0.92f;
			}

			std::wstring label = node.label;
			if (label.empty())
			{
				label = GetSkillData(node.skill_id).alias;
			}

			DrawString(renderer, label, x, y - NODE_SIZE * 0.5f - 6.f, TEXT_ALIGN_CENTER, name_shade, name_shade * 0.95f, name_shade * 0.85f, 1.f);
		}

		if (hovered != nullptr)
		{
			BuildSkillTooltip(hovered->skill_id, tooltip);
		}

		DrawString(renderer, L"이어진 스킬을 가졌으면 포인트 1 로 해금한다. 레벨업마다 포인트 " + std::to_wstring(GetConfig().level_up_skill_point),
			SKILL_X, SKILL_Y - SKILL_HEIGHT * 0.5f + 30.f, TEXT_ALIGN_CENTER, 0.48f, 0.46f, 0.42f, 1.f);
	}

	// ------------------------------------------------------------ 강화

	const float ENHANCE_BUTTON_Y = ENHANCE_Y - ENHANCE_HEIGHT * 0.5f + 52.f;

	void DoEnhance()
	{
		const int32 result = TryEnhanceWeapon();
		const Player& player = GetPlayer();

		switch (result)
		{
			case ENHANCE_RESULT_SUCCESS:
			{
				PlaySfx(SFX_ENHANCE_SUCCESS, 1.f);
				AddScreenFlash(1.f, 0.85f, 0.4f, 0.25f);
				AddShake(0.25f);
				SpawnSparks(player.m_pos, 20.f, 0.f, 1.f, 24, 220.f, 1.6f, 2.6f, 2.f, 0.9f, 0.6f);
				break;
			}
			case ENHANCE_RESULT_KEEP:
			case ENHANCE_RESULT_DOWN:
			{
				PlaySfx(SFX_ENHANCE_FAIL, 1.f);
				AddShake(0.15f);
				break;
			}
			case ENHANCE_RESULT_RESET:
			{
				PlaySfx(SFX_ENHANCE_BREAK, 1.f);
				AddScreenFlash(0.8f, 0.2f, 0.1f, 0.35f);
				AddShake(0.5f);
				break;
			}
			default:
			{
				PlaySfx(SFX_UI_ERROR, 0.6f);
				break;
			}
		}

		g_enhance_result = result;
		g_enhance_result_timer = RESULT_SHOW_SEC;
	}

	bool HandleEnhanceDown(float x, float y)
	{
		if (IsInsideBox(x, y, ENHANCE_X, ENHANCE_BUTTON_Y, 180.f, 40.f))
		{
			DoEnhance();
		}

		return true;
	}

	std::wstring GetResultText(int32 result, float* r, float* g, float* b)
	{
		*r = 0.9f;
		*g = 0.88f;
		*b = 0.8f;

		switch (result)
		{
			case ENHANCE_RESULT_SUCCESS:
			{
				*r = 1.f;
				*g = 0.85f;
				*b = 0.35f;
				return L"성공!";
			}
			case ENHANCE_RESULT_KEEP:
			{
				*r = 0.7f;
				*g = 0.7f;
				*b = 0.7f;
				return L"실패 - 단계 유지";
			}
			case ENHANCE_RESULT_DOWN:
			{
				*r = 0.95f;
				*g = 0.55f;
				*b = 0.3f;
				return L"실패 - 한 단계 하락";
			}
			case ENHANCE_RESULT_RESET:
			{
				*r = 1.f;
				*g = 0.25f;
				*b = 0.2f;
				return L"파괴 - 0 단계로";
			}
			case ENHANCE_RESULT_NO_WEAPON:
			{
				return L"낀 무기가 없다";
			}
			case ENHANCE_RESULT_NO_MATERIAL:
			{
				*r = 0.95f;
				*g = 0.45f;
				*b = 0.35f;
				return L"재료가 모자라다";
			}
			case ENHANCE_RESULT_MAX_LEVEL:
			{
				return L"최고 단계";
			}
			default:
			{
				return L"";
			}
		}
	}

	void DrawEnhanceWindow(Renderer* renderer, std::vector<TooltipLine>* tooltip)
	{
		const Player& player = GetPlayer();
		const Inventory& inventory = player.m_inventory;
		const float left = GetLeft(ENHANCE_X, ENHANCE_WIDTH);
		const float right = left + ENHANCE_WIDTH;
		const float top = GetTop(ENHANCE_Y, ENHANCE_HEIGHT);

		DrawPanel(renderer, ENHANCE_X, ENHANCE_Y, ENHANCE_WIDTH, ENHANCE_HEIGHT, L"강화 - 모루  ( U )");

		const ItemSlot& weapon = inventory.GetEquip(EQUIP_AREA_WEAPON);
		const bool has_weapon = inventory.IsEmptySlot(weapon) == false;

		const float slot_y = top - 104.f;
		DrawSlotBox(renderer, ENHANCE_X, slot_y, UI_SLOT, IsInsideBox(g_mouse_x, g_mouse_y, ENHANCE_X, slot_y, UI_SLOT, UI_SLOT), false);

		if (has_weapon == false)
		{
			DrawStringShadow(renderer, L"무기를 먼저 장착하세요", ENHANCE_X, slot_y - 32.f, TEXT_ALIGN_CENTER, 0.9f, 0.5f, 0.4f, 1.f);
			return;
		}

		DrawItemInSlot(renderer, weapon, ENHANCE_X, slot_y, 1.f);
		if (IsInsideBox(g_mouse_x, g_mouse_y, ENHANCE_X, slot_y, UI_SLOT, UI_SLOT))
		{
			BuildItemTooltip(weapon, tooltip);
		}

		const int32 level = weapon.enhance;
		const int32 max_level = GetMaxEnhanceLevel();
		DrawStringShadow(renderer, L"+" + std::to_wstring(level) + L" " + GetItemData(weapon.item_id).name, ENHANCE_X, slot_y - 26.f, TEXT_ALIGN_CENTER, 1.f, 0.86f, 0.55f, 1.f);

		// 단계 구슬. 켜짐과 꺼짐 자체가 보상과 처벌이라 숫자보다 먼저 읽힌다
		const float pip_y = slot_y - 70.f;
		const float pip_step = 20.f;
		const float pip_left = ENHANCE_X - pip_step * static_cast<float>(max_level - 1) * 0.5f;
		for (int32 pip = 0; pip < max_level; ++pip)
		{
			const float pip_x = pip_left + pip_step * static_cast<float>(pip);
			renderer->DrawSolidBox(pip_x, pip_y, 14.f, 14.f, 0.03f, 0.03f, 0.03f, 1.f);

			float r = 0.12f;
			float g = 0.1f;
			float b = 0.08f;
			if (pip < level)
			{
				// 구간마다 색이 다르다. 10 과 15 에서 크게 뛴다
				r = 0.9f;
				g = 0.75f;
				b = 0.3f;
				if (pip >= 10)
				{
					r = 1.f;
					g = 0.45f;
					b = 0.15f;
				}
			}

			renderer->DrawSolidBox(pip_x, pip_y, 10.f, 10.f, r, g, b, 1.f);
		}

		if (level >= max_level)
		{
			DrawStringShadow(renderer, L"최고 단계에 닿았다", ENHANCE_X, pip_y - 30.f, TEXT_ALIGN_CENTER, 1.f, 0.8f, 0.4f, 1.f);
			return;
		}

		const int32 bonus_now = GetEnhanceAttackBonus(level);
		const int32 bonus_next = GetEnhanceAttackBonus(level + 1);
		DrawStringShadow(renderer, L"공격력 +" + std::to_wstring(bonus_now) + L"  ->  +" + std::to_wstring(bonus_next), ENHANCE_X, pip_y - 22.f, TEXT_ALIGN_CENTER, 0.95f, 0.7f, 0.35f, 1.f);

		// 비용
		const EnhanceCost cost = GetEnhanceCost(level);
		const int32 trace_have = inventory.CountItem(ITEM_ID_SPELL_TRACE);
		const float cost_y = pip_y - 72.f;

		DrawSprite(renderer, SPRITE_ICON_TRACE, 0, left + 44.f, cost_y - 16.f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, 1.f);
		const bool trace_ok = trace_have >= cost.trace;
		std::array<float, 3> trace_color = { 0.8f, 0.75f, 1.f };
		if (trace_ok == false)
		{
			trace_color = { 0.95f, 0.4f, 0.35f };
		}

		DrawStringShadow(renderer, L"주문의 흔적  " + std::to_wstring(trace_have) + L" / " + std::to_wstring(cost.trace), left + 70.f, cost_y + 12.f, TEXT_ALIGN_LEFT,
			trace_color[0], trace_color[1], trace_color[2], 1.f);

		DrawSprite(renderer, SPRITE_ICON_GOLD, 0, left + 44.f, cost_y - 60.f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, 1.f);
		const bool gold_ok = player.m_gold >= cost.gold;
		std::array<float, 3> gold_color = { 1.f, 0.84f, 0.35f };
		if (gold_ok == false)
		{
			gold_color = { 0.95f, 0.4f, 0.35f };
		}

		DrawStringShadow(renderer, L"골드  " + std::to_wstring(player.m_gold) + L" / " + std::to_wstring(cost.gold), left + 70.f, cost_y - 32.f, TEXT_ALIGN_LEFT,
			gold_color[0], gold_color[1], gold_color[2], 1.f);

		// 확률과 위험
		const int32 rate = GetEnhanceSuccessRate(level);
		const float rate_y = cost_y - 84.f;
		DrawStringShadow(renderer, L"성공 " + std::to_wstring(rate) + L"%", left + 22.f, rate_y + 12.f, TEXT_ALIGN_LEFT, 0.55f, 0.95f, 0.6f, 1.f);
		DrawBarLeft(renderer, left + 110.f, rate_y, right - left - 132.f, 8.f, static_cast<float>(rate) / 100.f, 0.3f, 0.8f, 0.35f);

		std::wstring risk = L"실패해도 단계 유지";
		float risk_r = 0.6f;
		float risk_g = 0.6f;
		float risk_b = 0.6f;
		if (CanResetOnFail(level))
		{
			risk = L"실패 시 하락, " + std::to_wstring(GetConfig().enhance_reset_share) + L"% 확률로 0 단계";
			risk_r = 1.f;
			risk_g = 0.35f;
			risk_b = 0.25f;
		}
		else if (CanDropOnFail(level))
		{
			risk = L"실패 시 한 단계 하락";
			risk_r = 0.95f;
			risk_g = 0.6f;
			risk_b = 0.3f;
		}

		DrawStringShadow(renderer, risk, ENHANCE_X, rate_y - 16.f, TEXT_ALIGN_CENTER, risk_r, risk_g, risk_b, 1.f);

		const bool can_pay = trace_ok && gold_ok;
		DrawButton(renderer, ENHANCE_X, ENHANCE_BUTTON_Y, 180.f, 40.f, L"강화하기  ( Enter )", can_pay, IsInsideBox(g_mouse_x, g_mouse_y, ENHANCE_X, ENHANCE_BUTTON_Y, 180.f, 40.f));

		if (g_enhance_result_timer > 0.f)
		{
			float r = 1.f;
			float g = 1.f;
			float b = 1.f;
			const std::wstring text = GetResultText(g_enhance_result, &r, &g, &b);
			const float alpha = std::min(1.f, g_enhance_result_timer / 0.4f);

			DrawStringOutline(renderer, text, ENHANCE_X, top - 40.f, TEXT_ALIGN_CENTER, 1, r, g, b, alpha);
		}
	}

	bool HandleWindowDown(int32 window, int32 button, float x, float y)
	{
		switch (window)
		{
			case WINDOW_INVENTORY:
			{
				return HandleInventoryDown(button, x, y);
			}
			case WINDOW_STAT:
			{
				return HandleStatDown(x, y);
			}
			case WINDOW_SKILL:
			{
				return HandleSkillDown(x, y);
			}
			case WINDOW_ENHANCE:
			{
				return HandleEnhanceDown(x, y);
			}
			default:
			{
				return false;
			}
		}
	}
}

void ToggleInventoryWindow()
{
	ToggleWindow(WINDOW_INVENTORY);
}

void ToggleStatWindow()
{
	ToggleWindow(WINDOW_STAT);
}

void ToggleSkillWindow()
{
	ToggleWindow(WINDOW_SKILL);
}

void ToggleEnhanceWindow()
{
	if (g_open[static_cast<size_t>(WINDOW_ENHANCE)])
	{
		CloseWindow(WINDOW_ENHANCE);
		return;
	}

	OpenEnhanceWindow();
}

void OpenEnhanceWindow()
{
	// 강화는 성채 모루에서만. 성채로 돌아올 이유가 여기서 생긴다
	if (IsPlayerNearAnvil() == false)
	{
		AddDamageText(GetPlayer().m_pos, 60.f, L"성채 광장의 모루에서 강화할 수 있다", DAMAGE_STYLE_INFO);
		PlaySfx(SFX_UI_ERROR, 0.5f);
		return;
	}

	g_enhance_result_timer = 0.f;
	OpenWindow(WINDOW_ENHANCE);
}

bool CloseTopWindow()
{
	if (g_confirm_open)
	{
		g_confirm_open = false;
		return true;
	}

	if (g_drag_kind != DRAG_NONE)
	{
		g_drag_kind = DRAG_NONE;
		return true;
	}

	int32 top = -1;
	int32 top_order = -1;
	for (int32 window = 0; window < WINDOW_COUNT; ++window)
	{
		if (g_open[static_cast<size_t>(window)] && g_open_order[static_cast<size_t>(window)] > top_order)
		{
			top = window;
			top_order = g_open_order[static_cast<size_t>(window)];
		}
	}

	if (top < 0)
	{
		return false;
	}

	CloseWindow(top);
	return true;
}

bool IsAnyWindowOpen()
{
	for (bool open : g_open)
	{
		if (open)
		{
			return true;
		}
	}

	return g_confirm_open;
}

bool UiMouseDown(int32 button, float x, float y)
{
	g_mouse_x = x;
	g_mouse_y = y;

	if (g_confirm_open)
	{
		if (button == UI_MOUSE_LEFT)
		{
			HandleConfirmDown(x, y);
		}

		return true;
	}

	const int32 window = FindWindowAt(x, y);
	if (window < 0)
	{
		return false;
	}

	// 누른 창을 맨 위로
	g_open_serial += 1;
	g_open_order[static_cast<size_t>(window)] = g_open_serial;

	if (button == UI_MOUSE_MIDDLE)
	{
		return true;
	}

	return HandleWindowDown(window, button, x, y);
}

bool UiMouseUp(int32 button, float x, float y)
{
	g_mouse_x = x;
	g_mouse_y = y;

	if (button != UI_MOUSE_LEFT || g_drag_kind == DRAG_NONE)
	{
		return g_confirm_open || FindWindowAt(x, y) >= 0;
	}

	FinishDrag(x, y);
	return true;
}

void UiMouseMove(float x, float y)
{
	g_mouse_x = x;
	g_mouse_y = y;

	if (g_drag_kind != DRAG_NONE && g_drag_started == false)
	{
		const float moved_x = x - g_drag_from_x;
		const float moved_y = y - g_drag_from_y;

		if (moved_x * moved_x + moved_y * moved_y > DRAG_START_PIXEL * DRAG_START_PIXEL)
		{
			g_drag_started = true;
		}
	}
}

bool UiKeyEnter()
{
	if (g_confirm_open)
	{
		MutablePlayer().m_inventory.Destroy(g_confirm_slot);
		PlaySfx(SFX_ENHANCE_FAIL, 0.5f);
		g_confirm_open = false;
		return true;
	}

	if (g_open[static_cast<size_t>(WINDOW_ENHANCE)])
	{
		DoEnhance();
		return true;
	}

	return false;
}

void UpdateUiWindows(float real_seconds)
{
	TickUiClock(real_seconds);

	if (g_enhance_result_timer > 0.f)
	{
		g_enhance_result_timer -= real_seconds;
	}

	// 모루에서 멀어지면 강화 창을 닫는다
	if (g_open[static_cast<size_t>(WINDOW_ENHANCE)] && IsPlayerNearAnvil() == false)
	{
		CloseWindow(WINDOW_ENHANCE);
	}

	// 죽으면 끌던 것을 놓는다
	if (IsPlayerDead())
	{
		g_drag_kind = DRAG_NONE;
		g_confirm_open = false;
	}
}

void DrawUiWindows(Renderer* renderer)
{
	std::vector<TooltipLine> tooltip;

	// 늦게 연 창이 위
	std::array<int32, WINDOW_COUNT> order = { WINDOW_INVENTORY, WINDOW_STAT, WINDOW_SKILL, WINDOW_ENHANCE };
	std::sort(order.begin(), order.end(),
		[](int32 left, int32 right)
		{
			return g_open_order[static_cast<size_t>(left)] < g_open_order[static_cast<size_t>(right)];
		});

	const int32 hovered_window = FindWindowAt(g_mouse_x, g_mouse_y);

	for (int32 window : order)
	{
		if (g_open[static_cast<size_t>(window)] == false)
		{
			continue;
		}

		// 툴팁은 맨 위 창 것만
		std::vector<TooltipLine> window_tooltip;

		switch (window)
		{
			case WINDOW_INVENTORY:
			{
				DrawInventory(renderer, &window_tooltip);
				break;
			}
			case WINDOW_STAT:
			{
				DrawStatWindow(renderer, &window_tooltip);
				break;
			}
			case WINDOW_SKILL:
			{
				DrawSkillWindow(renderer, &window_tooltip);
				break;
			}
			case WINDOW_ENHANCE:
			{
				DrawEnhanceWindow(renderer, &window_tooltip);
				break;
			}
			default:
			{
				break;
			}
		}

		if (window == hovered_window)
		{
			tooltip = window_tooltip;
		}
	}

	DrawDraggedItem(renderer);
	DrawConfirm(renderer);

	if (g_confirm_open == false)
	{
		DrawTooltip(renderer, g_mouse_x, g_mouse_y, tooltip);
	}
}
