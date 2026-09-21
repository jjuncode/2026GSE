#include "stdafx.h"
#include "Hud.h"
#include "Enhance.h"
#include "GameConfig.h"
#include "Monster.h"
#include "Player.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "Text.h"

#include <array>
#include <cmath>
#include <string>

namespace
{
	const float PANEL_HEIGHT = 96.f;
	const float ORB_RADIUS = 54.f;
	const float TEXT_HALF_LINE = 12.f;	// 굴림 12 픽셀 x 배율 2 의 절반. 가운데 맞출 때 윗줄을 이만큼 올린다
	constexpr int32 BELT_SLOT_COUNT = 8;
	constexpr int32 ORB_SLICE_COUNT = 24;
	constexpr int32 PIPS_PER_LINE = 20;
	constexpr float RESULT_FLASH_SEC = 1.2f;

	const float STAT_PANEL_X = -420.f;
	const float STAT_PANEL_Y = 40.f;
	const float STAT_PANEL_WIDTH = 360.f;
	const float STAT_PANEL_HEIGHT = 300.f;
	const float STAT_PLUS_X = -265.f;
	const float BUTTON_SIZE = 30.f;

	const float ENHANCE_PANEL_X = 420.f;
	const float ENHANCE_PANEL_Y = 40.f;
	const float ENHANCE_PANEL_WIDTH = 380.f;
	const float ENHANCE_PANEL_HEIGHT = 320.f;
	const float ENHANCE_BUTTON_Y = -50.f;
	const float ENHANCE_BUTTON_WIDTH = 180.f;
	const float ENHANCE_BUTTON_HEIGHT = 44.f;

	bool g_stat_window_open = false;
	bool g_enhance_window_open = false;
	int32 g_last_enhance_result = ENHANCE_RESULT_NONE;
	int32 g_last_enhance_level = 0;
	float g_result_flash = 0.f;
	float g_pulse_time = 0.f;

	float GetScreenBottom()
	{
		return -static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	}

	float GetScreenTop()
	{
		return static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	}

	float GetPulse()
	{
		return 0.6f + 0.4f * std::sin(g_pulse_time * 6.f);
	}

	float ClampRatio(float ratio)
	{
		if (ratio < 0.f)
		{
			return 0.f;
		}

		if (ratio > 1.f)
		{
			return 1.f;
		}

		return ratio;
	}

	bool IsInsideBox(float x, float y, float center_x, float center_y, float width, float height)
	{
		if (std::fabs(x - center_x) > width * 0.5f)
		{
			return false;
		}

		if (std::fabs(y - center_y) > height * 0.5f)
		{
			return false;
		}

		return true;
	}

	std::array<float, 3> GetStatColor(int32 stat)
	{
		if (stat == STAT_MAX_HP)
		{
			return { 0.85f, 0.25f, 0.20f };
		}

		if (stat == STAT_ATTACK)
		{
			return { 0.95f, 0.60f, 0.20f };
		}

		return { 0.40f, 0.60f, 0.95f };
	}

	std::array<float, 3> GetMaterialColor(int32 material)
	{
		if (material == MATERIAL_CORE)
		{
			return { 1.00f, 0.52f, 0.12f };
		}

		if (material == MATERIAL_FRAGMENT)
		{
			return { 0.35f, 0.62f, 1.00f };
		}

		return { 0.90f, 0.92f, 0.98f };
	}

	// 1 ~ 5 가르치는 구간 / 6 ~ 10 손실 구간 / 11 ~ 15 도박 구간
	std::array<float, 3> GetEnhanceBandColor(int32 level)
	{
		if (level > 10)
		{
			return { 1.00f, 0.52f, 0.12f };
		}

		if (level > 5)
		{
			return { 0.35f, 0.62f, 1.00f };
		}

		return { 0.85f, 0.85f, 0.90f };
	}

	int32 MaterialSprite(int32 material)
	{
		if (material == MATERIAL_CORE)
		{
			return SPRITE_CORE;
		}

		if (material == MATERIAL_FRAGMENT)
		{
			return SPRITE_FRAGMENT;
		}

		return SPRITE_DUST;
	}

	void DrawPanel(Renderer* renderer, float center_x, float center_y, float width, float height)
	{
		renderer->DrawSolidBox(center_x, center_y, width + 6.f, height + 6.f, 0.30f, 0.24f, 0.15f, 1.f);
		renderer->DrawSolidBox(center_x, center_y, width, height, 0.060f, 0.050f, 0.040f, 0.95f);
	}

	void DrawFrame(Renderer* renderer, float center_x, float center_y, float width, float height, float thickness, float r, float g, float b, float a)
	{
		const float half_width = width * 0.5f;
		const float half_height = height * 0.5f;

		renderer->DrawSolidBox(center_x, center_y + half_height, width, thickness, r, g, b, a);
		renderer->DrawSolidBox(center_x, center_y - half_height, width, thickness, r, g, b, a);
		renderer->DrawSolidBox(center_x - half_width, center_y, thickness, height, r, g, b, a);
		renderer->DrawSolidBox(center_x + half_width, center_y, thickness, height, r, g, b, a);
	}

	void DrawPlus(Renderer* renderer, float x, float y, float size, float r, float g, float b, float a)
	{
		const float bar = size * 0.28f;

		renderer->DrawSolidBox(x, y, size, bar, r, g, b, a);
		renderer->DrawSolidBox(x, y, bar, size, r, g, b, a);
	}

	void DrawBar(Renderer* renderer, float x, float y, float width, float height, float fill_ratio, float r, float g, float b)
	{
		renderer->DrawSolidBox(x, y, width + 4.f, height + 4.f, 0.040f, 0.038f, 0.036f, 1.f);
		renderer->DrawSolidBox(x, y, width, height, 0.090f, 0.080f, 0.070f, 1.f);

		// 왼쪽 끝을 고정한 채 줄어들어야 게이지로 읽힌다
		const float fill_width = width * ClampRatio(fill_ratio);
		const float fill_x = x - (width - fill_width) * 0.5f;

		renderer->DrawSolidBox(fill_x, y, fill_width, height, r, g, b, 1.f);
	}

	// 칸 수로 개수를 낸다. 넘치는 건 마지막 칸이 깜빡여 알린다
	void DrawPips(Renderer* renderer, float start_x, float y, int32 count, float r, float g, float b)
	{
		const float step = 11.f;
		const float size = 8.f;

		for (int32 index = 0; index < PIPS_PER_LINE; ++index)
		{
			const float pip_x = start_x + step * static_cast<float>(index);

			if (index < count)
			{
				renderer->DrawSolidBox(pip_x, y, size, size, r, g, b, 1.f);
			}
			else
			{
				renderer->DrawSolidBox(pip_x, y, size, size, 0.12f, 0.10f, 0.08f, 1.f);
			}
		}

		if (count > PIPS_PER_LINE)
		{
			const float pulse = GetPulse();
			const float last_x = start_x + step * static_cast<float>(PIPS_PER_LINE - 1);

			renderer->DrawSolidBox(last_x, y, size + 2.f, size + 2.f, pulse, pulse, pulse, 1.f);
		}
	}

	// 원을 가로 띠로 잘라 쌓는다. 원 클리핑 없이 차오르는 액체가 된다
	void DrawOrb(Renderer* renderer, float x, float y, float fill_ratio, float r, float g, float b)
	{
		// 테두리를 먼저 크게 깔면 구슬이 패널에 박힌 것처럼 보인다
		renderer->DrawSolidCircle(x, y, ORB_RADIUS + 7.f, 0.080f, 0.065f, 0.050f, 1.f);
		renderer->DrawSolidCircle(x, y, ORB_RADIUS + 3.f, 0.300f, 0.240f, 0.150f, 1.f);
		renderer->DrawSolidCircle(x, y, ORB_RADIUS, 0.020f, 0.018f, 0.018f, 1.f);

		const float ratio = ClampRatio(fill_ratio);
		const float slice_height = ORB_RADIUS * 2.f / static_cast<float>(ORB_SLICE_COUNT);

		for (int32 slice = 0; slice < ORB_SLICE_COUNT; ++slice)
		{
			const float slice_center = (static_cast<float>(slice) + 0.5f) / static_cast<float>(ORB_SLICE_COUNT);
			if (slice_center > ratio)
			{
				break;
			}

			const float offset = -ORB_RADIUS + slice_height * (static_cast<float>(slice) + 0.5f);
			float chord_sq = ORB_RADIUS * ORB_RADIUS - offset * offset;
			if (chord_sq < 0.f)
			{
				chord_sq = 0.f;
			}

			const float chord = std::sqrt(chord_sq) * 2.f;
			renderer->DrawSolidBox(x, y + offset, chord, slice_height + 0.5f, r, g, b, 1.f);
		}

		// 위쪽에 밝은 점을 얹어야 평면 원이 아니라 구로 읽힌다
		renderer->DrawSolidCircle(x, y + ORB_RADIUS * 0.34f, ORB_RADIUS * 0.42f, 1.f, 1.f, 1.f, 0.08f);
		renderer->DrawSolidCircle(x - ORB_RADIUS * 0.22f, y + ORB_RADIUS * 0.45f, ORB_RADIUS * 0.18f, 1.f, 1.f, 1.f, 0.16f);
	}

	// 벨트 = 가진 재화 한눈에. 재료 셋, 돈, 강화 단계
	void DrawBelt(Renderer* renderer, float center_y)
	{
		const PlayerState& player = GetPlayer();
		const float slot_size = 38.f;
		const float slot_gap = 50.f;
		const float start_x = -slot_gap * static_cast<float>(BELT_SLOT_COUNT - 1) * 0.5f;

		for (int32 slot = 0; slot < BELT_SLOT_COUNT; ++slot)
		{
			const float x = start_x + slot_gap * static_cast<float>(slot);

			renderer->DrawSolidBox(x, center_y, slot_size + 3.f, slot_size + 3.f, 0.300f, 0.240f, 0.150f, 1.f);
			renderer->DrawSolidBox(x, center_y, slot_size, slot_size, 0.045f, 0.042f, 0.040f, 1.f);
		}

		// 그림을 두 배로 키워 칸 안에 앉히고, 개수는 칸 아래에 쓴다
		const float icon_y = center_y - 6.f;
		const float count_y = center_y - slot_size * 0.5f - 2.f;

		for (int32 material = 0; material < MATERIAL_COUNT; ++material)
		{
			const float x = start_x + slot_gap * static_cast<float>(material);
			DrawSprite(renderer, MaterialSprite(material), 0, x, icon_y, 2.f, false, 1.f, 1.f, 1.f, 1.f);
			DrawStringShadow(renderer, std::to_wstring(player.materials[static_cast<size_t>(material)]), x, count_y, TEXT_ALIGN_CENTER, 0.95f, 0.92f, 0.85f, 1.f);
		}

		const float gold_x = start_x + slot_gap * 3.f;
		DrawSprite(renderer, SPRITE_GOLD, 0, gold_x, icon_y, 2.f, false, 1.f, 1.f, 1.f, 1.f);
		DrawStringShadow(renderer, std::to_wstring(player.gold), gold_x, count_y, TEXT_ALIGN_CENTER, 1.f, 0.82f, 0.30f, 1.f);

		const float sword_x = start_x + slot_gap * 4.f;
		DrawSprite(renderer, SPRITE_SWORD, 0, sword_x, center_y - 14.f, 1.f, false, 1.f, 1.f, 1.f, 1.f);
		DrawStringShadow(renderer, L"+" + std::to_wstring(player.enhance_level), sword_x, count_y, TEXT_ALIGN_CENTER, 0.75f, 0.60f, 1.f, 1.f);
	}

	float GetStatRowY(int32 stat)
	{
		return STAT_PANEL_Y + 60.f - static_cast<float>(stat) * 64.f;
	}

	std::wstring GetStatValueText(int32 stat)
	{
		if (stat == STAT_MAX_HP)
		{
			return L"최대 체력 " + std::to_wstring(GetPlayerMaxHp());
		}

		if (stat == STAT_ATTACK)
		{
			return L"공격력 " + std::to_wstring(GetPlayerAttack());
		}

		return L"받는 피해 -" + std::to_wstring(GetPlayerDefence());
	}

	void DrawStatWindow(Renderer* renderer)
	{
		const PlayerState& player = GetPlayer();
		const float left_x = STAT_PANEL_X - STAT_PANEL_WIDTH * 0.5f;
		const float right_x = STAT_PANEL_X + STAT_PANEL_WIDTH * 0.5f;
		const float top_y = STAT_PANEL_Y + STAT_PANEL_HEIGHT * 0.5f;
		const float pulse = GetPulse();

		DrawPanel(renderer, STAT_PANEL_X, STAT_PANEL_Y, STAT_PANEL_WIDTH, STAT_PANEL_HEIGHT);
		DrawStringShadow(renderer, L"능력치", STAT_PANEL_X, top_y - 6.f, TEXT_ALIGN_CENTER, 0.95f, 0.85f, 0.60f, 1.f);

		DrawStringShadow(renderer, L"레벨 " + std::to_wstring(player.level), left_x + 16.f, top_y - 36.f, TEXT_ALIGN_LEFT, 0.85f, 0.80f, 1.f, 1.f);

		// 남은 포인트가 있으면 깜빡여서 찍으라고 알린다
		const std::wstring points_text = L"남은 포인트 " + std::to_wstring(player.stat_point);
		if (player.stat_point > 0)
		{
			DrawStringShadow(renderer, points_text, right_x - 16.f, top_y - 36.f, TEXT_ALIGN_RIGHT, pulse, 0.85f * pulse, 0.30f, 1.f);
		}
		else
		{
			DrawStringShadow(renderer, points_text, right_x - 16.f, top_y - 36.f, TEXT_ALIGN_RIGHT, 0.55f, 0.52f, 0.48f, 1.f);
		}

		for (int32 stat = 0; stat < STAT_COUNT; ++stat)
		{
			const float row_y = GetStatRowY(stat);
			const std::array<float, 3> color = GetStatColor(stat);

			DrawStringShadow(renderer, GetStatName(stat), left_x + 16.f, row_y + 26.f, TEXT_ALIGN_LEFT, color[0], color[1], color[2], 1.f);
			DrawStringShadow(renderer, GetStatValueText(stat), left_x + 110.f, row_y + 26.f, TEXT_ALIGN_LEFT, 0.90f, 0.88f, 0.82f, 1.f);
			DrawPips(renderer, left_x + 22.f, row_y - 10.f, player.stats[static_cast<size_t>(stat)], color[0], color[1], color[2]);

			if (player.stat_point > 0)
			{
				renderer->DrawSolidBox(STAT_PLUS_X, row_y, BUTTON_SIZE, BUTTON_SIZE, 0.30f, 0.24f, 0.12f, 1.f);
				DrawPlus(renderer, STAT_PLUS_X, row_y, BUTTON_SIZE * 0.6f, 0.95f * pulse, 0.85f * pulse, 0.40f * pulse, 1.f);
			}
			else
			{
				renderer->DrawSolidBox(STAT_PLUS_X, row_y, BUTTON_SIZE, BUTTON_SIZE, 0.08f, 0.07f, 0.06f, 1.f);
				DrawPlus(renderer, STAT_PLUS_X, row_y, BUTTON_SIZE * 0.6f, 0.20f, 0.18f, 0.16f, 1.f);
			}
		}

		DrawString(renderer, L"+ 버튼이나 1 2 3 키로 찍는다", STAT_PANEL_X, STAT_PANEL_Y - STAT_PANEL_HEIGHT * 0.5f + 34.f, TEXT_ALIGN_CENTER, 0.55f, 0.52f, 0.48f, 1.f);
	}

	std::wstring GetRiskText(int32 level)
	{
		if (CanResetOnFail(level))
		{
			return L"실패: 하락/초기화";
		}

		if (CanDropOnFail(level))
		{
			return L"실패: 한 단계 하락";
		}

		return L"실패: 단계 유지";
	}

	std::wstring GetResultText()
	{
		if (g_last_enhance_result == ENHANCE_RESULT_SUCCESS)
		{
			return L"성공! +" + std::to_wstring(g_last_enhance_level);
		}

		if (g_last_enhance_result == ENHANCE_RESULT_DOWN)
		{
			return L"실패 - +" + std::to_wstring(g_last_enhance_level) + L" 로 하락";
		}

		if (g_last_enhance_result == ENHANCE_RESULT_RESET)
		{
			return L"실패 - +0 초기화";
		}

		return L"실패 - 단계 유지";
	}

	std::array<float, 3> GetResultColor()
	{
		if (g_last_enhance_result == ENHANCE_RESULT_SUCCESS)
		{
			return { 0.40f, 1.00f, 0.50f };
		}

		if (g_last_enhance_result == ENHANCE_RESULT_DOWN)
		{
			return { 1.00f, 0.35f, 0.25f };
		}

		if (g_last_enhance_result == ENHANCE_RESULT_RESET)
		{
			return { 0.85f, 0.10f, 0.10f };
		}

		return { 0.70f, 0.70f, 0.70f };
	}

	void DrawEnhanceCost(Renderer* renderer, const PlayerState& player, float left_x, float right_x)
	{
		const EnhanceCost cost = GetEnhanceCost(player.enhance_level);
		const bool can_pay = CanPayEnhance(player, cost);

		// 돈. 모자라면 붉게
		const std::wstring gold_text = L"골드 " + std::to_wstring(player.gold) + L" / " + std::to_wstring(cost.gold);
		if (player.gold >= cost.gold)
		{
			DrawStringShadow(renderer, gold_text, left_x + 16.f, ENHANCE_PANEL_Y + 72.f, TEXT_ALIGN_LEFT, 1.f, 0.82f, 0.30f, 1.f);
		}
		else
		{
			DrawStringShadow(renderer, gold_text, left_x + 16.f, ENHANCE_PANEL_Y + 72.f, TEXT_ALIGN_LEFT, 0.95f, 0.40f, 0.30f, 1.f);
		}

		DrawBar(renderer, ENHANCE_PANEL_X, ENHANCE_PANEL_Y + 40.f, 340.f, 8.f, static_cast<float>(player.gold) / static_cast<float>(cost.gold), 0.95f, 0.72f, 0.20f);

		// 재료. 필요한 개수만큼 그림을 깔고 가진 만큼 밝힌다
		const float material_y = ENHANCE_PANEL_Y + 14.f;
		DrawStringShadow(renderer, L"재료", left_x + 16.f, material_y + TEXT_HALF_LINE, TEXT_ALIGN_LEFT, 0.90f, 0.88f, 0.82f, 1.f);

		float cursor_x = left_x + 84.f;

		for (int32 material = 0; material < MATERIAL_COUNT; ++material)
		{
			const int32 need = cost.materials[static_cast<size_t>(material)];
			const int32 have = player.materials[static_cast<size_t>(material)];

			for (int32 index = 0; index < need; ++index)
			{
				float brightness = 0.25f;
				if (index < have)
				{
					brightness = 1.f;
				}

				DrawSprite(renderer, MaterialSprite(material), 0, cursor_x, material_y - 10.f, 2.f, false, brightness, brightness, brightness, 1.f);
				cursor_x += 26.f;
			}

			if (need > 0)
			{
				const std::array<float, 3> color = GetMaterialColor(material);
				DrawStringShadow(renderer, GetMaterialName(material), cursor_x, material_y + TEXT_HALF_LINE, TEXT_ALIGN_LEFT, color[0], color[1], color[2], 1.f);
				cursor_x += MeasureString(GetMaterialName(material)) + 20.f;
			}
		}

		// 성공 확률
		const int32 rate_percent = static_cast<int32>(GetEnhanceSuccessRate(player.enhance_level) * 100.f + 0.5f);
		DrawStringShadow(renderer, L"확률 " + std::to_wstring(rate_percent) + L"%", left_x + 16.f, ENHANCE_PANEL_Y - 10.f, TEXT_ALIGN_LEFT, 0.55f, 0.95f, 0.60f, 1.f);
		DrawBar(renderer, ENHANCE_PANEL_X, ENHANCE_PANEL_Y - 46.f, 340.f, 8.f, GetEnhanceSuccessRate(player.enhance_level), 0.30f, 0.80f, 0.35f);

		// 실패하면 무엇이 걸렸나
		std::array<float, 3> risk_color = { 0.70f, 0.70f, 0.70f };
		if (CanResetOnFail(player.enhance_level))
		{
			risk_color = { 1.00f, 0.35f, 0.25f };
		}
		else if (CanDropOnFail(player.enhance_level))
		{
			risk_color = { 1.00f, 0.65f, 0.35f };
		}

		DrawStringShadow(renderer, GetRiskText(player.enhance_level), right_x - 16.f, ENHANCE_PANEL_Y - 10.f, TEXT_ALIGN_RIGHT, risk_color[0], risk_color[1], risk_color[2], 1.f);

		// 버튼
		if (can_pay)
		{
			renderer->DrawSolidBox(ENHANCE_PANEL_X, ENHANCE_BUTTON_Y, ENHANCE_BUTTON_WIDTH, ENHANCE_BUTTON_HEIGHT, 0.55f, 0.32f, 0.10f, 1.f);
			DrawFrame(renderer, ENHANCE_PANEL_X, ENHANCE_BUTTON_Y, ENHANCE_BUTTON_WIDTH, ENHANCE_BUTTON_HEIGHT, 3.f, 0.95f * GetPulse(), 0.70f, 0.30f, 1.f);
			DrawStringShadow(renderer, L"강화하기 (Enter)", ENHANCE_PANEL_X, ENHANCE_BUTTON_Y + TEXT_HALF_LINE, TEXT_ALIGN_CENTER, 1.f, 0.95f, 0.85f, 1.f);
		}
		else
		{
			renderer->DrawSolidBox(ENHANCE_PANEL_X, ENHANCE_BUTTON_Y, ENHANCE_BUTTON_WIDTH, ENHANCE_BUTTON_HEIGHT, 0.12f, 0.10f, 0.08f, 1.f);
			DrawString(renderer, L"재료가 모자라다", ENHANCE_PANEL_X, ENHANCE_BUTTON_Y + TEXT_HALF_LINE, TEXT_ALIGN_CENTER, 0.50f, 0.46f, 0.42f, 1.f);
		}
	}

	void DrawEnhanceWindow(Renderer* renderer)
	{
		const PlayerState& player = GetPlayer();
		const float left_x = ENHANCE_PANEL_X - ENHANCE_PANEL_WIDTH * 0.5f;
		const float right_x = ENHANCE_PANEL_X + ENHANCE_PANEL_WIDTH * 0.5f;
		const float top_y = ENHANCE_PANEL_Y + ENHANCE_PANEL_HEIGHT * 0.5f;

		DrawPanel(renderer, ENHANCE_PANEL_X, ENHANCE_PANEL_Y, ENHANCE_PANEL_WIDTH, ENHANCE_PANEL_HEIGHT);
		DrawStringShadow(renderer, L"무기 강화", ENHANCE_PANEL_X, top_y - 6.f, TEXT_ALIGN_CENTER, 0.95f, 0.85f, 0.60f, 1.f);

		// 단계 열다섯 칸. 켜짐과 꺼짐 자체가 보상과 처벌이다 ( 2.10 )
		const float level_y = top_y - 40.f;
		const float level_start_x = ENHANCE_PANEL_X - 22.f * 7.f;

		for (int32 level = 1; level <= MAX_ENHANCE_LEVEL; ++level)
		{
			const float x = level_start_x + 22.f * static_cast<float>(level - 1);
			const std::array<float, 3> color = GetEnhanceBandColor(level);

			if (level <= player.enhance_level)
			{
				renderer->DrawSolidBox(x, level_y, 16.f, 16.f, color[0], color[1], color[2], 1.f);
			}
			else
			{
				renderer->DrawSolidBox(x, level_y, 16.f, 16.f, color[0] * 0.15f, color[1] * 0.15f, color[2] * 0.15f, 1.f);
			}
		}

		const std::wstring current_text = L"현재 +" + std::to_wstring(player.enhance_level) + L"   공격력 보너스 +" + std::to_wstring(GetEnhanceAttackBonus(player.enhance_level));
		DrawStringShadow(renderer, current_text, ENHANCE_PANEL_X, level_y - 16.f, TEXT_ALIGN_CENTER, 0.90f, 0.88f, 0.82f, 1.f);

		if (player.enhance_level < MAX_ENHANCE_LEVEL)
		{
			DrawEnhanceCost(renderer, player, left_x, right_x);
		}
		else
		{
			DrawStringShadow(renderer, L"최대 강화 달성", ENHANCE_PANEL_X, ENHANCE_PANEL_Y + TEXT_HALF_LINE, TEXT_ALIGN_CENTER, 1.f, 0.60f, 0.20f, 1.f);
		}

		if (g_result_flash <= 0.f)
		{
			return;
		}

		const float alpha = ClampRatio(g_result_flash / RESULT_FLASH_SEC * 2.f);
		const std::array<float, 3> color = GetResultColor();

		DrawFrame(renderer, ENHANCE_PANEL_X, ENHANCE_PANEL_Y, ENHANCE_PANEL_WIDTH + 12.f, ENHANCE_PANEL_HEIGHT + 12.f, 8.f, color[0], color[1], color[2], alpha * 0.6f);
		DrawStringShadow(renderer, GetResultText(), ENHANCE_PANEL_X, ENHANCE_BUTTON_Y - 30.f, TEXT_ALIGN_CENTER, color[0], color[1], color[2], alpha);
	}
}

void UpdateHud(float delta_seconds)
{
	g_pulse_time += delta_seconds;

	g_result_flash -= delta_seconds;
	if (g_result_flash < 0.f)
	{
		g_result_flash = 0.f;
	}
}

void DrawHud(Renderer* renderer)
{
	const PlayerState& player = GetPlayer();
	const float bottom = GetScreenBottom();
	const float top = GetScreenTop();
	const float panel_center_y = bottom + PANEL_HEIGHT * 0.5f;
	const float window_width = static_cast<float>(WINDOW_SIZE_X);
	const float half_width = window_width * 0.5f;

	if (IsPlayerDead())
	{
		renderer->DrawSolidBox(0.f, 0.f, window_width, static_cast<float>(WINDOW_SIZE_Y), 0.25f, 0.f, 0.f, 0.35f);
		DrawStringShadow(renderer, L"쓰러졌다... 곧 모닥불 곁에서 깨어난다", 0.f, 60.f, TEXT_ALIGN_CENTER, 1.f, 0.55f, 0.45f, 1.f);
	}

	DrawString(renderer, L"[C] 능력치   [U] 강화   [E] 재료 줍기   [N] 새 지역", -half_width + 16.f, top - 10.f, TEXT_ALIGN_LEFT, 0.80f, 0.74f, 0.62f, 0.9f);

	// 하단 패널
	renderer->DrawSolidBox(0.f, panel_center_y, window_width, PANEL_HEIGHT, 0.075f, 0.060f, 0.045f, 1.f);
	renderer->DrawSolidBox(0.f, bottom + PANEL_HEIGHT, window_width, 3.f, 0.300f, 0.240f, 0.150f, 1.f);

	// 경험치 막대
	const float exp_bar_y = bottom + PANEL_HEIGHT + 12.f;
	const int32 need_exp = GetNeedExp(player.level);
	DrawBar(renderer, 0.f, exp_bar_y, window_width - 60.f, 7.f, static_cast<float>(player.exp) / static_cast<float>(need_exp), 0.45f, 0.40f, 0.85f);
	DrawStringShadow(renderer, L"Lv " + std::to_wstring(player.level), -half_width + 34.f, exp_bar_y + 32.f, TEXT_ALIGN_LEFT, 0.85f, 0.80f, 1.f, 1.f);
	DrawStringShadow(renderer, L"경험치 " + std::to_wstring(player.exp) + L" / " + std::to_wstring(need_exp), half_width - 34.f, exp_bar_y + 32.f, TEXT_ALIGN_RIGHT, 0.80f, 0.76f, 0.95f, 1.f);

	DrawBelt(renderer, panel_center_y);

	// 왼쪽 체력, 오른쪽 강화 단계 ( 별 개수가 곧 진행도 )
	const float left_orb_x = -half_width + 78.f;
	const float right_orb_x = half_width - 78.f;
	const float orb_y = panel_center_y + 6.f;
	const int32 max_hp = GetPlayerMaxHp();
	const float hp_ratio = static_cast<float>(player.hp) / static_cast<float>(max_hp);
	const float enhance_ratio = static_cast<float>(player.enhance_level) / static_cast<float>(MAX_ENHANCE_LEVEL);

	DrawOrb(renderer, left_orb_x, orb_y, hp_ratio, 0.520f, 0.055f, 0.055f);
	DrawOrb(renderer, right_orb_x, orb_y, enhance_ratio, 0.090f, 0.130f, 0.560f);

	DrawStringShadow(renderer, std::to_wstring(player.hp) + L"/" + std::to_wstring(max_hp), left_orb_x, orb_y + TEXT_HALF_LINE, TEXT_ALIGN_CENTER, 1.f, 0.92f, 0.88f, 1.f);
	DrawStringShadow(renderer, L"+" + std::to_wstring(player.enhance_level), right_orb_x, orb_y + TEXT_HALF_LINE + 8.f, TEXT_ALIGN_CENTER, 0.90f, 0.85f, 1.f, 1.f);
	DrawString(renderer, L"강화", right_orb_x, orb_y - 8.f, TEXT_ALIGN_CENTER, 0.70f, 0.68f, 0.90f, 1.f);

	// 찍을 포인트가 있으면 체력 구슬 위에서 깜빡인다
	if (player.stat_point > 0)
	{
		const float pulse = GetPulse();
		DrawPlus(renderer, left_orb_x + 60.f, orb_y + 58.f, 18.f, 0.95f * pulse, 0.80f * pulse, 0.25f * pulse, 1.f);
		DrawStringShadow(renderer, L"포인트 " + std::to_wstring(player.stat_point) + L" (C)", left_orb_x + 76.f, orb_y + 70.f, TEXT_ALIGN_LEFT, pulse, 0.85f * pulse, 0.30f, 1.f);
	}

	float boss_ratio = 0.f;
	if (GetBossHpRatio(&boss_ratio))
	{
		// 윗줄 조작 안내와 안 겹치게 한 줄 내린다
		DrawStringShadow(renderer, GetBossName(), 0.f, top - 40.f, TEXT_ALIGN_CENTER, 1.f, 0.55f, 0.45f, 1.f);
		DrawBar(renderer, 0.f, top - 76.f, 340.f, 9.f, boss_ratio, 0.480f, 0.055f, 0.055f);
	}

	if (g_stat_window_open)
	{
		DrawStatWindow(renderer);
	}

	if (g_enhance_window_open)
	{
		DrawEnhanceWindow(renderer);
	}
}

void ToggleStatWindow()
{
	g_stat_window_open = !g_stat_window_open;
}

void ToggleEnhanceWindow()
{
	g_enhance_window_open = !g_enhance_window_open;
}

void RequestEnhance()
{
	if (g_enhance_window_open == false)
	{
		return;
	}

	const int32 result = TryEnhance();
	if (result == ENHANCE_RESULT_NONE)
	{
		return;
	}

	g_last_enhance_result = result;
	g_last_enhance_level = GetPlayer().enhance_level;
	g_result_flash = RESULT_FLASH_SEC;
}

bool HandleHudClick(float screen_x, float screen_y)
{
	if (g_stat_window_open)
	{
		for (int32 stat = 0; stat < STAT_COUNT; ++stat)
		{
			if (IsInsideBox(screen_x, screen_y, STAT_PLUS_X, GetStatRowY(stat), BUTTON_SIZE, BUTTON_SIZE))
			{
				TryStatUp(stat);
				return true;
			}
		}

		// 창 위 클릭이 공격으로 새면 포인트 찍다가 칼을 휘두른다
		if (IsInsideBox(screen_x, screen_y, STAT_PANEL_X, STAT_PANEL_Y, STAT_PANEL_WIDTH, STAT_PANEL_HEIGHT))
		{
			return true;
		}
	}

	if (g_enhance_window_open)
	{
		if (IsInsideBox(screen_x, screen_y, ENHANCE_PANEL_X, ENHANCE_BUTTON_Y, ENHANCE_BUTTON_WIDTH, ENHANCE_BUTTON_HEIGHT))
		{
			RequestEnhance();
			return true;
		}

		if (IsInsideBox(screen_x, screen_y, ENHANCE_PANEL_X, ENHANCE_PANEL_Y, ENHANCE_PANEL_WIDTH, ENHANCE_PANEL_HEIGHT))
		{
			return true;
		}
	}

	return false;
}
