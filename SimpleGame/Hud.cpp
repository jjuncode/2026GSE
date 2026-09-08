#include "stdafx.h"
#include "Hud.h"
#include "Renderer.h"
#include "GameConfig.h"

namespace
{
	const float PANEL_HEIGHT = 96.f;
	const float ORB_RADIUS = 54.f;
	const int BELT_SLOT_COUNT = 8;

	float GetScreenBottom()
	{
		return -static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	}

	float GetScreenTop()
	{
		return static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	}

	void DrawOrb(Renderer* renderer, float x, float y, float r, float g, float b)
	{
		// 테두리를 먼저 크게 깔면 구슬이 패널에 박힌 것처럼 보인다
		renderer->DrawSolidCircle(x, y, ORB_RADIUS + 7.f, 0.055f, 0.050f, 0.045f, 1.f);
		renderer->DrawSolidCircle(x, y, ORB_RADIUS + 3.f, 0.145f, 0.125f, 0.100f, 1.f);
		renderer->DrawSolidCircle(x, y, ORB_RADIUS, r, g, b, 1.f);

		// 위쪽에 밝은 점을 얹어야 평면 원이 아니라 구로 읽힌다
		renderer->DrawSolidCircle(x, y + ORB_RADIUS * 0.34f, ORB_RADIUS * 0.42f, 1.f, 1.f, 1.f, 0.10f);
		renderer->DrawSolidCircle(x - ORB_RADIUS * 0.22f, y + ORB_RADIUS * 0.45f, ORB_RADIUS * 0.18f, 1.f, 1.f, 1.f, 0.18f);
	}

	void DrawBelt(Renderer* renderer, float center_y)
	{
		const float slot_size = 38.f;
		const float slot_gap = 46.f;
		const float start_x = -slot_gap * (BELT_SLOT_COUNT - 1) * 0.5f;

		for (int slot = 0; slot < BELT_SLOT_COUNT; ++slot)
		{
			const float x = start_x + slot_gap * static_cast<float>(slot);

			renderer->DrawSolidBox(x, center_y, slot_size + 3.f, slot_size + 3.f, 0.130f, 0.110f, 0.085f, 1.f);
			renderer->DrawSolidBox(x, center_y, slot_size, slot_size, 0.045f, 0.042f, 0.040f, 1.f);
		}
	}

	void DrawBar(Renderer* renderer, float x, float y, float width, float height, float fill_ratio, float r, float g, float b)
	{
		renderer->DrawSolidBox(x, y, width + 4.f, height + 4.f, 0.040f, 0.038f, 0.036f, 1.f);
		renderer->DrawSolidBox(x, y, width, height, 0.090f, 0.080f, 0.070f, 1.f);

		// 왼쪽 끝을 고정한 채 줄어들어야 게이지로 읽힌다
		const float fill_width = width * fill_ratio;
		const float fill_x = x - (width - fill_width) * 0.5f;

		renderer->DrawSolidBox(fill_x, y, fill_width, height, r, g, b, 1.f);
	}
}

void DrawHud(Renderer* renderer)
{
	const float bottom = GetScreenBottom();
	const float top = GetScreenTop();
	const float panel_center_y = bottom + PANEL_HEIGHT * 0.5f;
	const float window_width = static_cast<float>(WINDOW_SIZE_X);

	// 하단 패널
	renderer->DrawSolidBox(0.f, panel_center_y, window_width, PANEL_HEIGHT, 0.055f, 0.048f, 0.042f, 1.f);
	renderer->DrawSolidBox(0.f, bottom + PANEL_HEIGHT, window_width, 3.f, 0.150f, 0.125f, 0.095f, 1.f);

	// 경험치 막대. 패널 바로 위에 가늘게 깐다
	DrawBar(renderer, 0.f, bottom + PANEL_HEIGHT + 12.f, window_width - 60.f, 7.f, 0.42f, 0.35f, 0.30f, 0.75f);

	DrawBelt(renderer, panel_center_y);

	DrawOrb(renderer, -window_width * 0.5f + 78.f, panel_center_y + 6.f, 0.520f, 0.055f, 0.055f);
	DrawOrb(renderer, window_width * 0.5f - 78.f, panel_center_y + 6.f, 0.090f, 0.130f, 0.560f);

	// 상단 보스 체력. 이름은 글자라 못 그리므로 막대만 남긴다
	DrawBar(renderer, 0.f, top - 34.f, 340.f, 9.f, 0.78f, 0.480f, 0.055f, 0.055f);
}
