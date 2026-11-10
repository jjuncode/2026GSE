#include "stdafx.h"
#include "UiCard.h"
#include "Card.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "Text.h"

#include <algorithm>
#include <string>

namespace
{
	const float CARD_X = 0.f;
	const float CARD_Y = 10.f;
	constexpr int32 COLUMNS = 4;
	const float CELL_WIDTH = 136.f;
	const float CELL_HEIGHT = 176.f;
	const float CARD_WIDTH = 124.f;
	const float CARD_HEIGHT = 164.f;
	const float PORTRAIT_HEIGHT = 94.f;

	float GetWindowWidth()
	{
		return CELL_WIDTH * static_cast<float>(COLUMNS) + 32.f;
	}

	float GetWindowHeight()
	{
		const int32 rows = (GetCardCount() + COLUMNS - 1) / COLUMNS;
		return CELL_HEIGHT * static_cast<float>(rows) + 116.f;
	}

	void GetCardCenter(int32 index, float* x, float* y)
	{
		const int32 column = index % COLUMNS;
		const int32 row = index / COLUMNS;
		const float top = CARD_Y + GetWindowHeight() * 0.5f - 52.f;

		*x = CARD_X - CELL_WIDTH * static_cast<float>(COLUMNS - 1) * 0.5f + CELL_WIDTH * static_cast<float>(column);
		*y = top - CELL_HEIGHT * 0.5f - CELL_HEIGHT * static_cast<float>(row);
	}

	// 테두리 색이 등급. 잡몹 은빛, 정예 금빛, 보스 핏빛
	void GetGradeColor(int32 grade, bool owned, float* r, float* g, float* b)
	{
		*r = 0.45f;
		*g = 0.45f;
		*b = 0.48f;

		if (grade == GRADE_ELITE)
		{
			*r = 0.85f;
			*g = 0.66f;
			*b = 0.25f;
		}
		else if (grade == GRADE_BOSS)
		{
			*r = 0.8f;
			*g = 0.2f;
			*b = 0.25f;
		}

		if (owned == false)
		{
			*r *= 0.4f;
			*g *= 0.4f;
			*b *= 0.4f;
		}
	}
}

bool IsInsideCardWindow(float x, float y)
{
	return IsInsideBox(x, y, CARD_X, CARD_Y, GetWindowWidth() + 8.f, GetWindowHeight() + 8.f);
}

void DrawCardWindow(Renderer* renderer, float mouse_x, float mouse_y, std::vector<TooltipLine>* tooltip)
{
	DrawPanel(renderer, CARD_X, CARD_Y, GetWindowWidth(), GetWindowHeight(), L"카드 도감  ( Y )");

	CombatStat total;
	int32 owned_count = 0;

	for (int32 index = 0; index < GetCardCount(); ++index)
	{
		const CardData& card = GetCardDataAt(index);
		const MonsterData& monster = GetMonsterData(card.monster_type);
		const int32 copies = GetCardCopies(index);
		const int32 level = GetCardLevel(index);
		const bool owned = level > 0;

		float x = 0.f;
		float y = 0.f;
		GetCardCenter(index, &x, &y);
		const bool hovered = IsInsideBox(mouse_x, mouse_y, x, y, CARD_WIDTH, CARD_HEIGHT);

		float frame_r = 0.f;
		float frame_g = 0.f;
		float frame_b = 0.f;
		GetGradeColor(monster.grade, owned, &frame_r, &frame_g, &frame_b);
		if (hovered)
		{
			frame_r = std::min(frame_r * 1.4f, 1.f);
			frame_g = std::min(frame_g * 1.4f, 1.f);
			frame_b = std::min(frame_b * 1.4f, 1.f);
		}

		renderer->DrawSolidBox(x, y, CARD_WIDTH + 4.f, CARD_HEIGHT + 4.f, frame_r, frame_g, frame_b, 1.f);
		renderer->DrawSolidBox(x, y, CARD_WIDTH - 2.f, CARD_HEIGHT - 2.f, 0.05f, 0.045f, 0.04f, 1.f);
		renderer->DrawSolidBox(x, y + 22.f, CARD_WIDTH - 12.f, 92.f, 0.09f, 0.08f, 0.07f, 1.f);

		// 초상. 없는 카드는 검은 실루엣으로 무엇인지만 보여 준다. 그림자 괴물은 어둡게 물들인다
		float tint_r = 1.f;
		float tint_g = 1.f;
		float tint_b = 1.f;
		if (owned == false)
		{
			tint_r = 0.f;
			tint_g = 0.f;
			tint_b = 0.f;
		}
		else if (monster.is_shadow)
		{
			tint_r = 0.45f;
			tint_g = 0.35f;
			tint_b = 0.7f;
		}

		// 도트 한 칸을 3 · 2 · 1 픽셀 중 초상 칸에 들어가는 가장 큰 크기로. 정수가 아니면 칸 크기가 들쭉날쭉하다
		const float art_height = static_cast<float>(GetSpriteHeight(monster.sprite));
		float portrait_scale = 1.f / static_cast<float>(PIXEL_SCALE);
		for (int32 pixels = 3; pixels >= 1; --pixels)
		{
			if (art_height * static_cast<float>(pixels) <= PORTRAIT_HEIGHT)
			{
				portrait_scale = static_cast<float>(pixels) / static_cast<float>(PIXEL_SCALE);
				break;
			}
		}
		DrawSprite(renderer, monster.sprite, 0, x, y - 22.f, portrait_scale, false, tint_r, tint_g, tint_b, 1.f);

		// 카드에 안 들어가는 이름은 앞 말을 뗀다 ( 그림자 기사단장 -> 기사단장 ). 툴팁엔 온 이름
		std::wstring name = L"? ? ?";
		if (owned)
		{
			name = monster.name;
			const size_t space = name.find(L' ');
			if (MeasureString(name) > CARD_WIDTH - 8.f && space != std::wstring::npos)
			{
				name = name.substr(space + 1);
			}
		}

		DrawStringShadow(renderer, name, x, y - 32.f, TEXT_ALIGN_CENTER, 0.92f, 0.88f, 0.8f, 1.f);

		std::wstring line = L"드랍 " + std::to_wstring(static_cast<int32>(card.rate + 0.5f)) + L"%";
		if (owned)
		{
			const int32 next = GetCardNextCopies(index);
			line = L"Lv " + std::to_wstring(level);
			if (next > 0)
			{
				line += L"  " + std::to_wstring(copies) + L" / " + std::to_wstring(next);
			}
		}

		DrawStringShadow(renderer, line, x, y - 56.f, TEXT_ALIGN_CENTER, frame_r * 1.2f, frame_g * 1.2f, frame_b * 1.2f, 1.f);

		if (owned)
		{
			owned_count += 1;
		}

		if (hovered)
		{
			TooltipLine title;
			title.text = L"아직 못 얻은 카드";
			if (owned)
			{
				title.text = monster.name + L" 카드";
			}

			title.r = 1.f;
			title.g = 0.86f;
			title.b = 0.55f;
			tooltip->push_back(title);

			TooltipLine bonus;
			bonus.text = L"수집 효과  " + DescribeCardBonus(card, std::max(level, 1)) + L"  ( 레벨마다 늘어난다 )";
			tooltip->push_back(bonus);

			TooltipLine rule;
			rule.text = L"1 · 3 · 6 장째에 레벨이 오른다. 처치할 때 " + std::to_wstring(static_cast<int32>(card.rate + 0.5f)) + L"% 확률";
			rule.r = 0.6f;
			rule.g = 0.58f;
			rule.b = 0.52f;
			tooltip->push_back(rule);

			if (owned && monster.desc.empty() == false)
			{
				TooltipLine desc;
				desc.text = monster.desc;
				desc.r = 0.7f;
				desc.g = 0.68f;
				desc.b = 0.62f;
				tooltip->push_back(desc);
			}
		}
	}

	AddCardStat(&total);

	std::wstring summary = L"모은 카드 " + std::to_wstring(owned_count) + L" / " + std::to_wstring(GetCardCount()) + L"   ·   공격 +" + std::to_wstring(total.attack)
		+ L"  방어 +" + std::to_wstring(total.defence) + L"  체력 +" + std::to_wstring(total.max_hp) + L"  치명 +" + std::to_wstring(total.crit_rate) + L"%";
	const float bottom = CARD_Y - GetWindowHeight() * 0.5f;
	DrawStringShadow(renderer, summary, CARD_X, bottom + 40.f, TEXT_ALIGN_CENTER, 1.f, 0.84f, 0.4f, 1.f);
}
