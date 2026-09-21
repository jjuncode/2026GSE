#include "stdafx.h"
#include "FloatingText.h"
#include "Text.h"

#include <algorithm>
#include <vector>

namespace
{
	constexpr float LIFE_SEC = 0.9f;
	constexpr float RISE_PER_SEC = 34.f;
	constexpr size_t MAX_COUNT = 64;	// 무리를 한꺼번에 베면 숫자가 화면을 덮는다

	struct FloatingText
	{
		WorldPos pos;
		float lift = 0.f;
		float age = 0.f;
		std::wstring text;
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
	};

	std::vector<FloatingText> g_texts;
}

void ClearFloatingTexts()
{
	g_texts.clear();
}

void AddFloatingText(const WorldPos& pos, float lift, const std::wstring& text, float r, float g, float b)
{
	if (g_texts.size() >= MAX_COUNT)
	{
		g_texts.erase(g_texts.begin());
	}

	FloatingText entry;
	entry.pos = pos;
	entry.lift = lift;
	entry.text = text;
	entry.r = r;
	entry.g = g;
	entry.b = b;

	g_texts.push_back(entry);
}

void UpdateFloatingTexts(float delta_seconds)
{
	for (FloatingText& entry : g_texts)
	{
		entry.age += delta_seconds;
	}

	g_texts.erase(std::remove_if(g_texts.begin(), g_texts.end(),
		[](const FloatingText& entry)
		{
			return entry.age >= LIFE_SEC;
		}), g_texts.end());
}

void DrawFloatingTexts(Renderer* renderer, const WorldPos& camera)
{
	for (const FloatingText& entry : g_texts)
	{
		const ScreenPos screen = WorldToScreen(entry.pos, camera);
		const float ratio = entry.age / LIFE_SEC;

		// 처음엔 또렷하다가 끝에서 빨리 사라져야 숫자가 읽힌다
		const float alpha = 1.f - ratio * ratio;
		const float y = screen.y + entry.lift + entry.age * RISE_PER_SEC;

		DrawStringShadow(renderer, entry.text, screen.x, y, TEXT_ALIGN_CENTER, entry.r, entry.g, entry.b, alpha);
	}
}
