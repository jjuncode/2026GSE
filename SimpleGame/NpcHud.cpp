#include "stdafx.h"
#include "NpcHud.h"
#include "GameConfig.h"
#include "Npc.h"
#include "NpcBrain.h"
#include "NpcData.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "Text.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace
{
	constexpr int32 LABEL_OFF = 0;
	constexpr int32 LABEL_TASK = 1;
	constexpr int32 LABEL_FULL = 2;
	constexpr int32 LABEL_MODE_COUNT = 3;

	const float INSPECTOR_X = 440.f;
	const float INSPECTOR_TOP = 316.f;
	const float INSPECTOR_WIDTH = 380.f;
	const float INSPECTOR_HEIGHT = 460.f;
	const float ROW_HEIGHT = 26.f;	// 굴림 12 픽셀 x 배율 2 = 24. 이보다 좁으면 줄이 겹친다
	const float BAR_ROW_HEIGHT = 25.f;
	const float WEIGHT_ROW_HEIGHT = 24.f;
	const float JOURNAL_LINE = 23.f;
	const float JOURNAL_FADE_START = 12.f;
	const float JOURNAL_FADE_SEC = 6.f;

	int32 g_label_mode = LABEL_OFF;
	bool g_journal_open = false;

	std::array<float, 3> GetJobColor(const Npc& npc)
	{
		// 약탈에 손댄 사람은 직업과 상관없이 붉게
		if (IsNpcOutlaw(npc))
		{
			return { 0.95f, 0.38f, 0.32f };
		}

		switch (npc.job)
		{
			case NPC_JOB_HUNTER:
			{
				return { 0.60f, 0.88f, 0.48f };
			}
			case NPC_JOB_MERCHANT:
			{
				return { 0.88f, 0.62f, 0.98f };
			}
			case NPC_JOB_GATHERER:
			{
				return { 0.90f, 0.84f, 0.62f };
			}
			default:
			{
				return { 0.42f, 0.88f, 0.82f };
			}
		}
	}

	bool IsOffScreen(const ScreenPos& screen, float margin)
	{
		if (std::fabs(screen.x) > static_cast<float>(WINDOW_SIZE_X) * 0.5f + margin)
		{
			return true;
		}

		return std::fabs(screen.y) > static_cast<float>(WINDOW_SIZE_Y) * 0.5f + margin;
	}

	bool IsReactiveTask(int32 task)
	{
		return task >= NPC_TASK_FIGHT;
	}

	std::wstring FormatTenths(float value)
	{
		const int32 tenths = static_cast<int32>(value * 10.f + 0.5f);

		return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10);
	}

	std::wstring FormatMilliseconds(float microseconds)
	{
		const int32 hundredths = static_cast<int32>(microseconds / 10.f + 0.5f);
		const int32 remainder = hundredths % 100;

		std::wstring text = std::to_wstring(hundredths / 100) + L".";
		if (remainder < 10)
		{
			text += L"0";
		}

		return text + std::to_wstring(remainder) + L"ms";
	}

	float Clamp01(float value)
	{
		if (value < 0.f)
		{
			return 0.f;
		}

		if (value > 1.f)
		{
			return 1.f;
		}

		return value;
	}

	void DrawBar(Renderer* renderer, float left_x, float y, float width, float ratio, float r, float g, float b)
	{
		const float fill = width * Clamp01(ratio);

		renderer->DrawSolidBox(left_x + width * 0.5f, y, width + 2.f, 8.f, 0.03f, 0.03f, 0.03f, 1.f);
		renderer->DrawSolidBox(left_x + fill * 0.5f, y, fill, 6.f, r, g, b, 1.f);
	}

	void DrawLabeledBar(Renderer* renderer, float left_x, float y, const std::wstring& label, const std::wstring& value, float ratio, float r, float g, float b)
	{
		DrawStringShadow(renderer, label, left_x, y + 10.f, TEXT_ALIGN_LEFT, 0.80f, 0.78f, 0.72f, 1.f);
		DrawBar(renderer, left_x + 64.f, y, 190.f, ratio, r, g, b);
		DrawStringShadow(renderer, value, left_x + 350.f, y + 10.f, TEXT_ALIGN_RIGHT, 0.90f, 0.88f, 0.82f, 1.f);
	}

	// 지금 실제로 돌고 있는 줄기. 뿌리는 뺀다
	std::vector<std::wstring> BuildPathLines(const Npc& npc, float max_width)
	{
		const BehaviorTree<Npc>& tree = GetNpcBrainTree();
		std::vector<std::wstring> lines;
		std::wstring line;

		for (size_t order = 1; order < npc.brain.active_path.size(); ++order)
		{
			const std::wstring name = tree.GetNodeName(npc.brain.active_path[order]);
			std::wstring piece = name;

			if (line.empty() == false)
			{
				piece = L" > " + name;
			}

			if (line.empty() == false && MeasureString(line + piece) > max_width)
			{
				lines.push_back(line);
				line = L"> " + name;
				continue;
			}

			line += piece;
		}

		if (line.empty() == false)
		{
			lines.push_back(line);
		}

		return lines;
	}

	void DrawWeights(Renderer* renderer, const Npc& npc, float left_x, float top_y)
	{
		std::array<float, NPC_DAILY_TASK_COUNT> weights = {};
		float total = 0.f;

		for (int32 task = 0; task < NPC_DAILY_TASK_COUNT; ++task)
		{
			weights[static_cast<size_t>(task)] = GetNpcTaskWeight(npc, task);
			total += weights[static_cast<size_t>(task)];
		}

		// 두 열로 접는다. 한 열 열한 줄이면 창이 화면 밖으로 나간다
		const int32 rows_per_column = (NPC_DAILY_TASK_COUNT + 1) / 2;

		for (int32 task = 0; task < NPC_DAILY_TASK_COUNT; ++task)
		{
			const int32 column = task / rows_per_column;
			const int32 row = task % rows_per_column;
			const float x = left_x + static_cast<float>(column) * 182.f;
			const float y = top_y - static_cast<float>(row) * WEIGHT_ROW_HEIGHT;

			float chance = 0.f;
			if (total > 0.f)
			{
				chance = weights[static_cast<size_t>(task)] / total;
			}

			const bool is_current = npc.task == task;
			float brightness = 0.62f;
			if (is_current)
			{
				brightness = 1.f;
				renderer->DrawSolidBox(x + 86.f, y - 1.f, 176.f, 22.f, 0.30f, 0.24f, 0.12f, 0.8f);
			}
			else if (chance <= 0.f)
			{
				brightness = 0.34f;
			}

			DrawStringShadow(renderer, GetNpcTaskName(task), x, y + 10.f, TEXT_ALIGN_LEFT, brightness, brightness * 0.95f, brightness * 0.85f, 1.f);
			DrawBar(renderer, x + 70.f, y, 54.f, chance, 0.95f * brightness, 0.70f * brightness, 0.25f * brightness);

			const int32 percent = static_cast<int32>(chance * 100.f + 0.5f);
			DrawStringShadow(renderer, std::to_wstring(percent) + L"%", x + 170.f, y + 10.f, TEXT_ALIGN_RIGHT, brightness, brightness, brightness, 1.f);
		}
	}

	void DrawInspector(Renderer* renderer)
	{
		const int32 index = GetSelectedNpc();
		if (index < 0)
		{
			return;
		}

		const Npc& npc = GetNpcList()[static_cast<size_t>(index)];
		const std::array<float, 3> color = GetJobColor(npc);
		const float left_x = INSPECTOR_X - INSPECTOR_WIDTH * 0.5f + 14.f;
		const float center_y = INSPECTOR_TOP - INSPECTOR_HEIGHT * 0.5f;

		renderer->DrawSolidBox(INSPECTOR_X, center_y, INSPECTOR_WIDTH + 6.f, INSPECTOR_HEIGHT + 6.f, 0.30f, 0.24f, 0.15f, 1.f);
		renderer->DrawSolidBox(INSPECTOR_X, center_y, INSPECTOR_WIDTH, INSPECTOR_HEIGHT, 0.055f, 0.048f, 0.040f, 0.94f);

		float y = INSPECTOR_TOP - 6.f;

		DrawStringShadow(renderer, GetNpcTitle(npc) + L"  Lv" + std::to_wstring(npc.level), left_x, y, TEXT_ALIGN_LEFT, color[0], color[1], color[2], 1.f);

		if (IsNpcOutlaw(npc) && npc.job != NPC_JOB_RAIDER)
		{
			DrawStringShadow(renderer, L"무법자", left_x + 352.f, y, TEXT_ALIGN_RIGHT, 1.f, 0.35f, 0.30f, 1.f);
		}

		y -= ROW_HEIGHT + 4.f;
		DrawStringShadow(renderer, L"탐욕 " + FormatTenths(npc.greed) + L"   용맹 " + FormatTenths(npc.bravery), left_x, y, TEXT_ALIGN_LEFT, 0.75f, 0.72f, 0.66f, 1.f);
		y -= ROW_HEIGHT;
		DrawStringShadow(renderer, L"잔혹 " + FormatTenths(npc.cruelty) + L"   근면 " + FormatTenths(npc.diligence), left_x, y, TEXT_ALIGN_LEFT, 0.75f, 0.72f, 0.66f, 1.f);

		// 막대는 글줄 가운데에 오게 한 줄 아래로 내려 잡는다
		y -= ROW_HEIGHT + 18.f;
		DrawLabeledBar(renderer, left_x, y, L"체력", std::to_wstring(npc.hp) + L"/" + std::to_wstring(npc.max_hp), static_cast<float>(npc.hp) / static_cast<float>(npc.max_hp), 0.80f, 0.16f, 0.12f);
		y -= BAR_ROW_HEIGHT;
		DrawLabeledBar(renderer, left_x, y, L"허기", std::to_wstring(static_cast<int32>(npc.hunger * 100.f)) + L"%", npc.hunger, 0.90f, 0.55f, 0.18f);
		y -= BAR_ROW_HEIGHT;
		DrawLabeledBar(renderer, left_x, y, L"짐", std::to_wstring(npc.goods) + L"/" + std::to_wstring(npc.goods_capacity), static_cast<float>(npc.goods) / static_cast<float>(npc.goods_capacity), 0.62f, 0.46f, 0.28f);

		y -= 16.f;
		DrawStringShadow(renderer, L"골드 " + std::to_wstring(npc.gold) + L"   처치 " + std::to_wstring(npc.kills) + L"   장비 " + std::to_wstring(npc.gear), left_x, y, TEXT_ALIGN_LEFT, 1.f, 0.82f, 0.35f, 1.f);

		y -= ROW_HEIGHT;
		std::array<float, 3> task_color = { 0.90f, 0.88f, 0.82f };
		if (IsReactiveTask(npc.task))
		{
			task_color = { 1.f, 0.50f, 0.35f };
		}

		DrawStringShadow(renderer, L"지금 " + std::wstring(GetNpcTaskName(npc.task)), left_x, y, TEXT_ALIGN_LEFT, task_color[0], task_color[1], task_color[2], 1.f);
		DrawStringShadow(renderer, L"적을 보면 " + std::wstring(GetNpcStanceName(npc.stance)), left_x + 352.f, y, TEXT_ALIGN_RIGHT, 0.70f, 0.78f, 0.90f, 1.f);

		// 행동 트리에서 지금 돌고 있는 줄기
		y -= ROW_HEIGHT;
		const std::vector<std::wstring> path_lines = BuildPathLines(npc, INSPECTOR_WIDTH - 28.f);
		for (size_t line = 0; line < path_lines.size() && line < 2; ++line)
		{
			DrawString(renderer, path_lines[line], left_x, y, TEXT_ALIGN_LEFT, 0.55f, 0.85f, 0.80f, 1.f);
			y -= ROW_HEIGHT;
		}

		// 줄 수가 바뀌어도 아래 표가 흔들리지 않게 두 줄 자리를 늘 비워 둔다
		for (size_t line = path_lines.size(); line < 2; ++line)
		{
			y -= ROW_HEIGHT;
		}

		// 마지막 추첨 결과. 같은 형편이어도 확률이라 다른 걸 뽑을 수 있다는 게 보여야 한다
		const int32 daily = GetNpcDailyNode();
		const int32 last_pick = npc.brain.last_pick[static_cast<size_t>(daily)];

		y -= 4.f;
		std::wstring header = L"일과 확률";
		if (last_pick >= 0)
		{
			const int32 chance = static_cast<int32>(npc.brain.last_pick_chance[static_cast<size_t>(daily)] * 100.f + 0.5f);
			header += L"  ( 마지막 " + std::wstring(GetNpcTaskName(last_pick)) + L" " + std::to_wstring(chance) + L"% )";
		}

		DrawStringShadow(renderer, header, left_x, y, TEXT_ALIGN_LEFT, 0.95f, 0.85f, 0.60f, 1.f);

		y -= ROW_HEIGHT + 14.f;
		DrawWeights(renderer, npc, left_x, y);
	}

	void DrawJournal(Renderer* renderer)
	{
		if (g_journal_open == false)
		{
			return;
		}

		int32 alive = 0;
		for (const Npc& npc : GetNpcList())
		{
			if (npc.is_alive)
			{
				alive += 1;
			}
		}

		const float left_x = -static_cast<float>(WINDOW_SIZE_X) * 0.5f + 16.f;
		const float top_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - 38.f;

		const std::wstring header = L"변경 일지   주민 " + std::to_wstring(alive) + L"   성채 금고 " + std::to_wstring(GetTreasury()) + L"   NPC 갱신 " + FormatMilliseconds(GetNpcTickMicroseconds());

		const std::vector<NpcEvent>& events = GetNpcEvents();

		// 배경을 깔아야 밝은 바닥 위에서도 읽힌다
		float widest = MeasureString(header);
		for (const NpcEvent& event : events)
		{
			const float width = MeasureString(event.text);
			if (width > widest)
			{
				widest = width;
			}
		}

		const float box_height = JOURNAL_LINE * static_cast<float>(events.size() + 1) + 8.f;
		renderer->DrawSolidBox(left_x + widest * 0.5f + 2.f, top_y - box_height * 0.5f + 12.f, widest + 16.f, box_height, 0.02f, 0.02f, 0.03f, 0.45f);

		DrawStringShadow(renderer, header, left_x, top_y, TEXT_ALIGN_LEFT, 0.95f, 0.85f, 0.60f, 1.f);

		// 새 소식이 위로
		float y = top_y - JOURNAL_LINE;
		for (size_t order = events.size(); order > 0; --order)
		{
			const NpcEvent& event = events[order - 1];
			const float fade = 1.f - Clamp01((event.age - JOURNAL_FADE_START) / JOURNAL_FADE_SEC);

			DrawStringShadow(renderer, event.text, left_x, y, TEXT_ALIGN_LEFT, event.r, event.g, event.b, fade);
			y -= JOURNAL_LINE;
		}
	}
}

void DrawNpcOverlay(Renderer* renderer, const WorldPos& camera)
{
	const std::vector<Npc>& npcs = GetNpcList();
	const int32 selected = GetSelectedNpc();

	for (size_t index = 0; index < npcs.size(); ++index)
	{
		const Npc& npc = npcs[index];

		if (npc.is_alive == false)
		{
			continue;
		}

		const ScreenPos screen = WorldToScreen(npc.body.m_pos, camera);
		if (IsOffScreen(screen, 20.f))
		{
			continue;
		}

		const bool is_selected = static_cast<int32>(index) == selected;
		const float head_y = screen.y + npc.body.GetArtHeight() * static_cast<float>(PIXEL_SCALE) + 4.f;
		float label_y = head_y + 22.f;

		if (npc.hp < npc.max_hp)
		{
			const float ratio = static_cast<float>(npc.hp) / static_cast<float>(npc.max_hp);
			const float bar_width = 20.f;
			const float fill = bar_width * ratio;

			renderer->DrawSolidBox(screen.x, head_y + 2.f, bar_width + 2.f, 4.f, 0.02f, 0.02f, 0.02f, 0.9f);
			renderer->DrawSolidBox(screen.x - (bar_width - fill) * 0.5f, head_y + 2.f, fill, 2.f, 0.80f, 0.20f, 0.14f, 1.f);
			label_y += 4.f;
		}

		if (g_label_mode == LABEL_OFF && is_selected == false)
		{
			continue;
		}

		const std::array<float, 3> color = GetJobColor(npc);
		std::wstring text = GetNpcTaskName(npc.task);

		if (IsReactiveTask(npc.task))
		{
			text += L"!";
		}

		if (g_label_mode == LABEL_FULL || is_selected)
		{
			text = npc.name + L" " + text;
		}

		DrawStringShadow(renderer, text, screen.x, label_y, TEXT_ALIGN_CENTER, color[0], color[1], color[2], 0.95f);
	}
}

void DrawNpcPanels(Renderer* renderer, bool hide_inspector)
{
	DrawJournal(renderer);

	if (hide_inspector == false)
	{
		DrawInspector(renderer);
	}
}

void CycleNpcLabelMode()
{
	g_label_mode = (g_label_mode + 1) % LABEL_MODE_COUNT;
}

void ToggleNpcJournal()
{
	g_journal_open = !g_journal_open;
}
