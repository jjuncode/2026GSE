#include "stdafx.h"
#include "FloatingText.h"
#include "Text.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>

namespace
{
	constexpr size_t MAX_COUNT = 80;	// 무리를 한꺼번에 베면 숫자가 화면을 덮는다

	struct FloatingText
	{
		WorldPos pos;
		float lift = 0.f;
		float age = 0.f;
		float life = 0.9f;
		float drift_x = 0.f;	// 같은 자리 숫자가 겹치지 않게 옆으로 흩는다
		std::wstring text;
		int32 style = DAMAGE_STYLE_NORMAL;
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
	};

	std::vector<FloatingText> g_texts;

	float RollRange(float min_value, float max_value)
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x68e31da4u);
		std::uniform_real_distribution<float> distribution(min_value, max_value);
		return distribution(random);
	}

	void ApplyStyle(FloatingText* entry)
	{
		switch (entry->style)
		{
			case DAMAGE_STYLE_CRIT:
			{
				entry->r = 1.f;
				entry->g = 0.62f;
				entry->b = 0.18f;
				entry->life = 1.0f;
				break;
			}
			case DAMAGE_STYLE_WEAK:
			{
				entry->r = 1.f;
				entry->g = 0.42f;
				entry->b = 0.85f;
				entry->life = 1.0f;
				break;
			}
			case DAMAGE_STYLE_PLAYER:
			{
				entry->r = 1.f;
				entry->g = 0.28f;
				entry->b = 0.22f;
				entry->life = 0.85f;
				break;
			}
			case DAMAGE_STYLE_GUARD:
			{
				entry->r = 0.68f;
				entry->g = 0.78f;
				entry->b = 0.95f;
				entry->life = 0.75f;
				break;
			}
			case DAMAGE_STYLE_BANNER_GOOD:
			{
				entry->r = 1.f;
				entry->g = 0.95f;
				entry->b = 0.72f;
				entry->life = 0.9f;
				entry->drift_x = 0.f;
				break;
			}
			case DAMAGE_STYLE_BANNER_BAD:
			{
				entry->r = 1.f;
				entry->g = 0.45f;
				entry->b = 0.2f;
				entry->life = 1.1f;
				entry->drift_x = 0.f;
				break;
			}
			case DAMAGE_STYLE_HEAL:
			{
				entry->r = 0.45f;
				entry->g = 1.f;
				entry->b = 0.5f;
				entry->life = 1.0f;
				break;
			}
			case DAMAGE_STYLE_INFO:
			{
				entry->r = 0.95f;
				entry->g = 0.88f;
				entry->b = 0.55f;
				entry->life = 1.3f;
				entry->drift_x = 0.f;
				break;
			}
			case DAMAGE_STYLE_CUSTOM:
			{
				break;
			}
			default:
			{
				entry->r = 1.f;
				entry->g = 0.96f;
				entry->b = 0.86f;
				entry->life = 0.8f;
				break;
			}
		}
	}

	int32 GetScale(int32 style)
	{
		switch (style)
		{
			case DAMAGE_STYLE_CRIT:
			case DAMAGE_STYLE_WEAK:
			case DAMAGE_STYLE_BANNER_GOOD:
			case DAMAGE_STYLE_BANNER_BAD:
			{
				return 2;
			}
			default:
			{
				return 1;
			}
		}
	}

	// 처음 0.08 초에 위로 튀었다 내려앉는다. 숫자가 맞는 순간과 같이 터져야 손맛으로 읽힌다
	float GetPopLift(float age)
	{
		const float pop_sec = 0.08f;
		if (age < pop_sec)
		{
			return 10.f * age / pop_sec;
		}

		const float settle = std::min((age - pop_sec) / 0.12f, 1.f);
		return 10.f - 4.f * settle;
	}
}

void ClearFloatingTexts()
{
	g_texts.clear();
}

void AddFloatingText(const WorldPos& pos, float lift, const std::wstring& text, float r, float g, float b)
{
	FloatingText entry;
	entry.pos = pos;
	entry.lift = lift;
	entry.text = text;
	entry.style = DAMAGE_STYLE_CUSTOM;
	entry.r = r;
	entry.g = g;
	entry.b = b;
	entry.life = 1.0f;

	if (g_texts.size() >= MAX_COUNT)
	{
		g_texts.erase(g_texts.begin());
	}

	g_texts.push_back(entry);
}

void AddDamageText(const WorldPos& pos, float lift, const std::wstring& text, int32 style)
{
	FloatingText entry;
	entry.pos = pos;
	entry.lift = lift;
	entry.text = text;
	entry.style = style;
	entry.drift_x = RollRange(-14.f, 14.f);

	ApplyStyle(&entry);

	if (g_texts.size() >= MAX_COUNT)
	{
		g_texts.erase(g_texts.begin());
	}

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
			return entry.age >= entry.life;
		}), g_texts.end());
}

void DrawFloatingTexts(Renderer* renderer, const WorldPos& camera)
{
	for (const FloatingText& entry : g_texts)
	{
		const ScreenPos screen = WorldToScreen(entry.pos, camera);
		const float ratio = entry.age / entry.life;

		// 처음엔 또렷하다가 끝에서 빨리 사라져야 숫자가 읽힌다
		const float alpha = 1.f - ratio * ratio * ratio;
		const float rise = entry.age * 26.f + GetPopLift(entry.age);
		const float x = screen.x + entry.drift_x * std::min(entry.age * 4.f, 1.f);
		const float y = screen.y + entry.lift + rise;

		// 터지는 순간엔 하얗게 시작해 제 색으로 식는다
		const float white = std::max(0.f, 1.f - entry.age / 0.1f);
		const float r = entry.r + (1.f - entry.r) * white;
		const float g = entry.g + (1.f - entry.g) * white;
		const float b = entry.b + (1.f - entry.b) * white;

		DrawStringOutline(renderer, entry.text, x, y, TEXT_ALIGN_CENTER, GetScale(entry.style), r, g, b, alpha);
	}
}
