#include "stdafx.h"
#include "UiQuestBoard.h"
#include "Audio.h"
#include "Player.h"
#include "Quest.h"
#include "Renderer.h"
#include "Text.h"
#include "UiDraw.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
	const float ROW_HEIGHT = 42.f;
	const float ACCEPT_WIDTH = 180.f;
	const float ACCEPT_HEIGHT = 36.f;
	const float LIST_SHARE = 0.47f;	// 창 폭 중 목록 몫

	int32 g_selected = 0;
	std::wstring g_message;
	bool g_message_good = true;

	float GetListRight(const BoardRect& rect)
	{
		return rect.left + (rect.right - rect.left) * LIST_SHARE;
	}

	float GetRowY(const BoardRect& rect, int32 row)
	{
		return rect.top - 10.f - static_cast<float>(row) * ROW_HEIGHT;
	}

	float GetAcceptX(const BoardRect& rect)
	{
		return (GetListRight(rect) + rect.right) * 0.5f;
	}

	float GetAcceptY(const BoardRect& rect)
	{
		return rect.bottom + 30.f;
	}

	// 띄어쓰기에서 끊어 너비 안에 담는다
	std::vector<std::wstring> Wrap(const std::wstring& text, float width)
	{
		std::vector<std::wstring> lines;
		std::wstring line;
		std::wstring word;

		auto flush_word = [&]()
		{
			if (word.empty())
			{
				return;
			}

			std::wstring candidate = word;
			if (line.empty() == false)
			{
				candidate = line + L" " + word;
			}

			if (MeasureString(candidate) > width && line.empty() == false)
			{
				lines.push_back(line);
				line = word;
			}
			else
			{
				line = candidate;
			}

			word.clear();
		};

		for (wchar_t letter : text)
		{
			if (letter == L' ')
			{
				flush_word();
				continue;
			}

			word += letter;
		}

		flush_word();
		if (line.empty() == false)
		{
			lines.push_back(line);
		}

		return lines;
	}

	std::wstring DescribeReward(const QuestReward& reward)
	{
		std::wstring text = L"골드 " + std::to_wstring(reward.gold) + L"  ·  경험치 " + std::to_wstring(reward.exp);
		if (reward.trace > 0)
		{
			text += L"  ·  흔적 " + std::to_wstring(reward.trace);
		}

		return text;
	}

	// 권장 레벨이 내 레벨보다 높을수록 붉다. 쉽게 못 미는 의뢰라는 게 목록에서 보인다
	void GetLevelColor(int32 level, float* r, float* g, float* b)
	{
		const int32 gap = level - GetPlayer().m_level;
		if (gap >= 3)
		{
			*r = 1.f;
			*g = 0.35f;
			*b = 0.3f;
			return;
		}

		if (gap >= 1)
		{
			*r = 1.f;
			*g = 0.75f;
			*b = 0.35f;
			return;
		}

		*r = 0.7f;
		*g = 0.85f;
		*b = 0.65f;
	}

	const wchar_t* GetStateLabel(int32 state)
	{
		switch (state)
		{
			case QUEST_STATE_ACTIVE:
			{
				return L"진행 중";
			}
			case QUEST_STATE_DONE:
			{
				return L"보고 대기";
			}
			case QUEST_STATE_REPORTED:
			{
				return L"완료";
			}
			default:
			{
				return L"";
			}
		}
	}

	void DrawDetail(Renderer* renderer, const Quest& quest, const BoardRect& rect, float mouse_x, float mouse_y)
	{
		const float left = GetListRight(rect) + 30.f;
		const float width = rect.right - left - 24.f;
		float y = rect.top;

		float tier_r = 0.f;
		float tier_g = 0.f;
		float tier_b = 0.f;
		GetQuestTierColor(quest.tier, &tier_r, &tier_g, &tier_b);

		float level_r = 0.f;
		float level_g = 0.f;
		float level_b = 0.f;
		GetLevelColor(quest.level, &level_r, &level_g, &level_b);

		DrawStringShadow(renderer, GetQuestTierName(quest.tier) + L" 의뢰", left, y, TEXT_ALIGN_LEFT, tier_r, tier_g, tier_b, 1.f);
		DrawStringShadow(renderer, L"권장 Lv " + std::to_wstring(quest.level), rect.right - 24.f, y, TEXT_ALIGN_RIGHT, level_r, level_g, level_b, 1.f);
		y -= 34.f;

		DrawStringShadow(renderer, quest.title, left, y, TEXT_ALIGN_LEFT, 1.f, 0.9f, 0.7f, 1.f);
		y -= 34.f;

		for (const std::wstring& line : Wrap(quest.desc, width))
		{
			DrawStringShadow(renderer, line, left, y, TEXT_ALIGN_LEFT, 0.72f, 0.68f, 0.6f, 1.f);
			y -= 25.f;
		}

		y -= 12.f;
		DrawStringShadow(renderer, L"목표", left, y, TEXT_ALIGN_LEFT, 0.55f, 0.52f, 0.46f, 1.f);
		DrawStringShadow(renderer, DescribeQuestGoal(quest, quest.state != QUEST_STATE_OFFERED), left + 60.f, y, TEXT_ALIGN_LEFT, 0.92f, 0.88f, 0.8f, 1.f);
		y -= 30.f;

		if (quest.has_place)
		{
			const int32 tiles = static_cast<int32>(GetPlayerDistanceTo(quest.place));
			DrawStringShadow(renderer, L"장소", left, y, TEXT_ALIGN_LEFT, 0.55f, 0.52f, 0.46f, 1.f);
			DrawStringShadow(renderer, quest.place_name + L"  ·  " + std::to_wstring(tiles) + L"칸", left + 60.f, y, TEXT_ALIGN_LEFT, 0.92f, 0.88f, 0.8f, 1.f);
			y -= 30.f;
		}

		DrawStringShadow(renderer, L"보상", left, y, TEXT_ALIGN_LEFT, 0.55f, 0.52f, 0.46f, 1.f);
		DrawStringShadow(renderer, DescribeReward(quest.reward), left + 60.f, y, TEXT_ALIGN_LEFT, 1.f, 0.84f, 0.4f, 1.f);
		y -= 30.f;

		if (quest.reward.item_id != ITEM_ID_NONE)
		{
			DrawStringShadow(renderer, L"+ " + GetItemData(quest.reward.item_id).name, left + 60.f, y, TEXT_ALIGN_LEFT, 0.55f, 0.78f, 1.f, 1.f);
		}

		const int32 active = static_cast<int32>(GetActiveQuests().size());
		const int32 max_active = GetQuestConfig().max_active;
		std::wstring label = L"수락";
		bool enabled = quest.state == QUEST_STATE_OFFERED && active < max_active;

		if (quest.state != QUEST_STATE_OFFERED)
		{
			label = GetStateLabel(quest.state);
		}
		else if (active >= max_active)
		{
			label = L"진행 중 " + std::to_wstring(active) + L" / " + std::to_wstring(max_active);
		}

		const float accept_x = GetAcceptX(rect);
		const float accept_y = GetAcceptY(rect);
		DrawButton(renderer, accept_x, accept_y, ACCEPT_WIDTH, ACCEPT_HEIGHT, label, enabled, IsInsideBox(mouse_x, mouse_y, accept_x, accept_y, ACCEPT_WIDTH, ACCEPT_HEIGHT));

		if (g_message.empty() == false)
		{
			float r = 0.6f;
			float g = 0.95f;
			float b = 0.6f;
			if (g_message_good == false)
			{
				r = 1.f;
				g = 0.45f;
				b = 0.35f;
			}

			DrawStringShadow(renderer, g_message, accept_x, accept_y + 50.f, TEXT_ALIGN_CENTER, r, g, b, 1.f);
		}
	}
}

void ResetQuestBoardSelection()
{
	g_selected = 0;
	g_message.clear();
}

void DrawQuestBoardTab(Renderer* renderer, int32 village, const BoardRect& rect, float mouse_x, float mouse_y)
{
	const std::vector<Quest>& offers = GetBoardQuests(village);
	if (offers.empty())
	{
		DrawStringShadow(renderer, L"붙은 의뢰가 없다", (rect.left + rect.right) * 0.5f, rect.top - 40.f, TEXT_ALIGN_CENTER, 0.6f, 0.56f, 0.5f, 1.f);
		return;
	}

	g_selected = std::min(std::max(g_selected, 0), static_cast<int32>(offers.size()) - 1);

	const float list_left = rect.left + 14.f;
	const float list_right = GetListRight(rect);
	const float list_center = (list_left + list_right) * 0.5f;

	for (size_t index = 0; index < offers.size(); ++index)
	{
		const Quest& quest = offers[index];
		const int32 row = static_cast<int32>(index);
		const float y = GetRowY(rect, row);
		const float center_y = y - 12.f;
		const bool hovered = IsInsideBox(mouse_x, mouse_y, list_center, center_y, list_right - list_left, ROW_HEIGHT - 4.f);

		if (row == g_selected)
		{
			renderer->DrawSolidBox(list_center, center_y, list_right - list_left, ROW_HEIGHT - 4.f, 0.2f, 0.15f, 0.09f, 1.f);
		}
		else if (hovered)
		{
			renderer->DrawSolidBox(list_center, center_y, list_right - list_left, ROW_HEIGHT - 4.f, 0.12f, 0.095f, 0.07f, 1.f);
		}

		float tier_r = 0.f;
		float tier_g = 0.f;
		float tier_b = 0.f;
		GetQuestTierColor(quest.tier, &tier_r, &tier_g, &tier_b);

		float title_r = 0.92f;
		float title_g = 0.88f;
		float title_b = 0.8f;
		if (quest.state == QUEST_STATE_REPORTED)
		{
			tier_r = 0.45f;
			tier_g = 0.45f;
			tier_b = 0.45f;
			title_r = 0.5f;
			title_g = 0.5f;
			title_b = 0.5f;
		}

		const std::wstring tag = L"[" + GetQuestTierName(quest.tier) + L"]";
		DrawStringShadow(renderer, tag, list_left + 8.f, y, TEXT_ALIGN_LEFT, tier_r, tier_g, tier_b, 1.f);
		const float title_x = list_left + 16.f + MeasureString(tag);
		DrawStringShadow(renderer, quest.title, title_x, y, TEXT_ALIGN_LEFT, title_r, title_g, title_b, 1.f);

		if (quest.state == QUEST_STATE_REPORTED)
		{
			renderer->DrawSolidBox(title_x + MeasureString(quest.title) * 0.5f, y - 12.f, MeasureString(quest.title) + 6.f, 2.f, 0.55f, 0.55f, 0.55f, 1.f);
		}

		const std::wstring state = GetStateLabel(quest.state);
		if (state.empty() == false)
		{
			DrawStringShadow(renderer, state, list_right - 8.f, y, TEXT_ALIGN_RIGHT, 0.55f, 0.85f, 0.6f, 1.f);
			continue;
		}

		float level_r = 0.f;
		float level_g = 0.f;
		float level_b = 0.f;
		GetLevelColor(quest.level, &level_r, &level_g, &level_b);
		DrawStringShadow(renderer, L"Lv " + std::to_wstring(quest.level), list_right - 8.f, y, TEXT_ALIGN_RIGHT, level_r, level_g, level_b, 1.f);
	}

	// 목록과 자세히 사이 세로줄
	renderer->DrawSolidBox(list_right + 14.f, (rect.top + rect.bottom) * 0.5f, 1.f, rect.top - rect.bottom + 10.f, 0.24f, 0.19f, 0.13f, 1.f);

	DrawDetail(renderer, offers[static_cast<size_t>(g_selected)], rect, mouse_x, mouse_y);

	const int32 minutes = static_cast<int32>(std::ceil(GetBoardRefreshLeft(village) / 60.f));
	DrawStringShadow(renderer, L"새 의뢰까지 " + std::to_wstring(minutes) + L"분  ·  끝낸 의뢰는 아무 게시판에서나 보고", list_center, rect.bottom + 18.f, TEXT_ALIGN_CENTER, 0.55f, 0.52f, 0.46f, 1.f);
}

bool HandleQuestBoardDown(int32 village, const BoardRect& rect, float x, float y)
{
	const std::vector<Quest>& offers = GetBoardQuests(village);
	const float list_left = rect.left + 14.f;
	const float list_right = GetListRight(rect);
	const float list_center = (list_left + list_right) * 0.5f;

	for (size_t index = 0; index < offers.size(); ++index)
	{
		const float center_y = GetRowY(rect, static_cast<int32>(index)) - 12.f;
		if (IsInsideBox(x, y, list_center, center_y, list_right - list_left, ROW_HEIGHT - 4.f))
		{
			g_selected = static_cast<int32>(index);
			g_message.clear();
			PlaySfx(SFX_UI_CLICK, 0.4f);
			return true;
		}
	}

	if (IsInsideBox(x, y, GetAcceptX(rect), GetAcceptY(rect), ACCEPT_WIDTH, ACCEPT_HEIGHT) == false)
	{
		return true;
	}

	const int32 result = AcceptBoardQuest(village, g_selected);
	if (result == QUEST_ACCEPT_OK)
	{
		g_message = L"받았다. 바닥의 빛을 따라가라";
		g_message_good = true;
		PlaySfx(SFX_EQUIP, 0.8f);
	}
	else if (result == QUEST_ACCEPT_FULL)
	{
		g_message = L"진행 중인 의뢰가 꽉 찼다";
		g_message_good = false;
		PlaySfx(SFX_UI_ERROR, 0.6f);
	}

	return true;
}
