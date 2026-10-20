#include "stdafx.h"
#include "UiDraw.h"
#include "Enhance.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Inventory.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "Text.h"

#include <algorithm>
#include <cmath>

namespace
{
	const float TOOLTIP_PAD = 10.f;
	const float TOOLTIP_LINE = 26.f;

	float g_ui_time = 0.f;

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

	std::wstring GetCategoryName(int32 category)
	{
		if (category == ITEM_CATEGORY_EQUIPMENT)
		{
			return L"장비";
		}

		if (category == ITEM_CATEGORY_CONSUMABLE)
		{
			return L"소비";
		}

		return L"기타";
	}
}

bool IsInsideBox(float x, float y, float center_x, float center_y, float width, float height)
{
	if (std::fabs(x - center_x) > width * 0.5f)
	{
		return false;
	}

	return std::fabs(y - center_y) <= height * 0.5f;
}

float GetUiPulse()
{
	return 0.6f + 0.4f * (0.5f + 0.5f * std::sin(g_ui_time * 5.f));
}

void TickUiClock(float real_seconds)
{
	g_ui_time += real_seconds;
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

void DrawPanel(Renderer* renderer, float center_x, float center_y, float width, float height, const std::wstring& title)
{
	// 바깥 그림자, 금속 테두리, 어두운 바탕. 다크판타지 톤은 바탕이 거의 검어야 산다
	renderer->DrawSolidBox(center_x + 4.f, center_y - 4.f, width + 8.f, height + 8.f, 0.f, 0.f, 0.f, 0.5f);
	renderer->DrawSolidBox(center_x, center_y, width + 6.f, height + 6.f, 0.26f, 0.21f, 0.14f, 1.f);
	renderer->DrawSolidBox(center_x, center_y, width + 2.f, height + 2.f, 0.09f, 0.075f, 0.06f, 1.f);
	renderer->DrawSolidBox(center_x, center_y, width - 2.f, height - 2.f, 0.045f, 0.04f, 0.035f, 0.97f);

	if (title.empty())
	{
		return;
	}

	const float top = center_y + height * 0.5f;
	renderer->DrawSolidBox(center_x, top - 18.f, width - 8.f, 30.f, 0.11f, 0.085f, 0.06f, 1.f);
	renderer->DrawSolidBox(center_x, top - 34.f, width - 8.f, 2.f, 0.36f, 0.28f, 0.16f, 1.f);
	DrawStringShadow(renderer, title, center_x, top - 6.f, TEXT_ALIGN_CENTER, 0.95f, 0.84f, 0.6f, 1.f);
}

void DrawBarLeft(Renderer* renderer, float left_x, float center_y, float width, float height, float ratio, float r, float g, float b)
{
	const float clamped = std::min(std::max(ratio, 0.f), 1.f);

	renderer->DrawSolidBox(left_x + width * 0.5f, center_y, width + 4.f, height + 4.f, 0.03f, 0.028f, 0.026f, 1.f);
	renderer->DrawSolidBox(left_x + width * 0.5f, center_y, width, height, 0.1f, 0.085f, 0.075f, 1.f);

	if (clamped <= 0.f)
	{
		return;
	}

	const float fill = width * clamped;
	renderer->DrawSolidBox(left_x + fill * 0.5f, center_y, fill, height, r, g, b, 1.f);

	// 윗줄을 살짝 밝게. 평평한 막대보다 덩어리로 읽힌다
	renderer->DrawSolidBox(left_x + fill * 0.5f, center_y + height * 0.5f - 1.f, fill, 2.f, std::min(r * 1.4f, 1.f), std::min(g * 1.4f, 1.f), std::min(b * 1.4f, 1.f), 0.8f);
}

void DrawButton(Renderer* renderer, float center_x, float center_y, float width, float height, const std::wstring& label, bool enabled, bool hovered)
{
	float frame = 0.3f;
	float face_r = 0.12f;
	float face_g = 0.095f;
	float face_b = 0.07f;
	float text = 0.5f;

	if (enabled)
	{
		frame = 0.55f;
		face_r = 0.32f;
		face_g = 0.2f;
		face_b = 0.08f;
		text = 1.f;

		if (hovered)
		{
			face_r = 0.48f;
			face_g = 0.3f;
			face_b = 0.1f;
		}
	}

	renderer->DrawSolidBox(center_x, center_y, width + 4.f, height + 4.f, frame, frame * 0.8f, frame * 0.5f, 1.f);
	renderer->DrawSolidBox(center_x, center_y, width, height, face_r, face_g, face_b, 1.f);
	DrawStringShadow(renderer, label, center_x, center_y + UI_LINE * 0.5f, TEXT_ALIGN_CENTER, text, text * 0.92f, text * 0.75f, 1.f);
}

void DrawSlotBox(Renderer* renderer, float center_x, float center_y, float size, bool hovered, bool highlight)
{
	float edge = 0.2f;
	if (hovered)
	{
		edge = 0.55f;
	}

	if (highlight)
	{
		edge = 0.85f * GetUiPulse();
	}

	renderer->DrawSolidBox(center_x, center_y, size + 4.f, size + 4.f, edge, edge * 0.82f, edge * 0.55f, 1.f);
	renderer->DrawSolidBox(center_x, center_y, size, size, 0.03f, 0.028f, 0.026f, 1.f);
	renderer->DrawSolidBox(center_x, center_y + size * 0.25f, size - 4.f, size * 0.45f, 0.06f, 0.055f, 0.05f, 1.f);
}

void DrawItemInSlot(Renderer* renderer, const ItemSlot& slot, float center_x, float center_y, float alpha)
{
	if (slot.item_id == ITEM_ID_NONE || slot.count <= 0)
	{
		return;
	}

	const ItemData& item = GetItemData(slot.item_id);
	const float icon_height = static_cast<float>(GetSpriteHeight(item.icon_sprite) * UI_SCALE);

	DrawSprite(renderer, item.icon_sprite, 0, center_x, center_y - icon_height * 0.5f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, alpha);

	if (slot.count > 1)
	{
		DrawStringShadow(renderer, std::to_wstring(slot.count), center_x + UI_SLOT * 0.5f - 2.f, center_y - 2.f, TEXT_ALIGN_RIGHT, 1.f, 1.f, 0.95f, alpha);
	}

	if (slot.enhance > 0)
	{
		DrawStringShadow(renderer, L"+" + std::to_wstring(slot.enhance), center_x - UI_SLOT * 0.5f + 2.f, center_y + UI_SLOT * 0.5f, TEXT_ALIGN_LEFT, 1.f, 0.78f, 0.3f, alpha);
	}
}

void DrawTooltip(Renderer* renderer, float anchor_x, float anchor_y, const std::vector<TooltipLine>& lines)
{
	if (lines.empty())
	{
		return;
	}

	float width = 0.f;
	for (const TooltipLine& line : lines)
	{
		width = std::max(width, MeasureString(line.text));
	}

	width += TOOLTIP_PAD * 2.f;
	const float height = static_cast<float>(lines.size()) * TOOLTIP_LINE + TOOLTIP_PAD * 2.f;

	// 화면 밖으로 안 나가게 커서 오른쪽 아래에 붙이되 넘치면 뒤집는다
	float left = anchor_x + 18.f;
	float top = anchor_y - 10.f;
	const float half_width = static_cast<float>(WINDOW_SIZE_X) * 0.5f;
	const float half_height = static_cast<float>(WINDOW_SIZE_Y) * 0.5f;

	if (left + width > half_width - 6.f)
	{
		left = anchor_x - 18.f - width;
	}

	if (top - height < -half_height + 6.f)
	{
		top = -half_height + 6.f + height;
	}

	const float center_x = left + width * 0.5f;
	const float center_y = top - height * 0.5f;

	renderer->DrawSolidBox(center_x, center_y, width + 4.f, height + 4.f, 0.4f, 0.32f, 0.2f, 1.f);
	renderer->DrawSolidBox(center_x, center_y, width, height, 0.03f, 0.027f, 0.025f, 0.97f);

	float y = top - TOOLTIP_PAD + 2.f;
	for (const TooltipLine& line : lines)
	{
		DrawStringShadow(renderer, line.text, left + TOOLTIP_PAD, y, TEXT_ALIGN_LEFT, line.r, line.g, line.b, 1.f);
		y -= TOOLTIP_LINE;
	}
}

void DrawLine(Renderer* renderer, float from_x, float from_y, float to_x, float to_y, float width, float r, float g, float b, float a)
{
	const float delta_x = to_x - from_x;
	const float delta_y = to_y - from_y;
	const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
	if (length < 0.001f)
	{
		return;
	}

	const float side_x = -delta_y / length * width * 0.5f;
	const float side_y = delta_x / length * width * 0.5f;

	renderer->DrawQuad(
		MakeVertex(from_x + side_x, from_y + side_y, r, g, b, a),
		MakeVertex(to_x + side_x, to_y + side_y, r, g, b, a),
		MakeVertex(to_x - side_x, to_y - side_y, r, g, b, a),
		MakeVertex(from_x - side_x, from_y - side_y, r, g, b, a));
}

void BuildItemTooltip(const ItemSlot& slot, std::vector<TooltipLine>* lines)
{
	lines->clear();

	if (slot.item_id == ITEM_ID_NONE || slot.count <= 0)
	{
		return;
	}

	const ItemData& item = GetItemData(slot.item_id);

	TooltipLine title;
	title.text = item.name;
	if (slot.enhance > 0)
	{
		title.text = L"+" + std::to_wstring(slot.enhance) + L" " + item.name;
	}

	title.r = 1.f;
	title.g = 0.86f;
	title.b = 0.55f;

	if (item.item_id == ITEM_ID_SPELL_TRACE)
	{
		title.r = 0.75f;
		title.g = 0.65f;
		title.b = 1.f;
	}

	lines->push_back(title);

	TooltipLine category;
	category.text = GetCategoryName(item.category);
	category.r = 0.55f;
	category.g = 0.52f;
	category.b = 0.48f;
	lines->push_back(category);

	if (item.category == ITEM_CATEGORY_EQUIPMENT)
	{
		TooltipLine attack;
		const int32 bonus = GetEnhanceAttackBonus(slot.enhance);
		attack.text = L"공격력 +" + std::to_wstring(item.stat.attack + bonus);
		if (bonus > 0)
		{
			attack.text += L"  ( 강화 +" + std::to_wstring(bonus) + L" )";
		}

		lines->push_back(attack);

		if (item.stat.crit_rate > 0)
		{
			TooltipLine crit;
			crit.text = L"치명타 +" + std::to_wstring(item.stat.crit_rate) + L"%";
			lines->push_back(crit);
		}
	}

	for (const ItemEffect& effect : item.effects)
	{
		if (effect.type == ITEM_EFFECT_HEAL_HP)
		{
			TooltipLine heal;
			heal.text = L"체력 " + std::to_wstring(effect.value) + L" 회복";
			heal.r = 0.5f;
			heal.g = 1.f;
			heal.b = 0.55f;
			lines->push_back(heal);
		}
	}

	if (item.cooldown_sec > 0.f)
	{
		TooltipLine cooldown;
		cooldown.text = L"재사용 " + std::to_wstring(static_cast<int32>(item.cooldown_sec)) + L"초";
		lines->push_back(cooldown);
	}

	if (item.desc.empty() == false)
	{
		TooltipLine desc;
		desc.text = item.desc;
		desc.r = 0.7f;
		desc.g = 0.68f;
		desc.b = 0.62f;
		lines->push_back(desc);
	}

	TooltipLine hint;
	hint.r = 0.5f;
	hint.g = 0.48f;
	hint.b = 0.44f;

	if (item.category == ITEM_CATEGORY_CONSUMABLE)
	{
		hint.text = L"더블클릭 · 우클릭: 사용";
		lines->push_back(hint);
	}
	else if (item.category == ITEM_CATEGORY_EQUIPMENT)
	{
		hint.text = L"더블클릭 · 우클릭: 장착 / 해제";
		lines->push_back(hint);
	}
}
