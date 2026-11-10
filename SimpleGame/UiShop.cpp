#include "stdafx.h"
#include "UiShop.h"
#include "Audio.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Inventory.h"
#include "Player.h"
#include "Renderer.h"
#include "Shop.h"
#include "SpriteAtlas.h"
#include "Text.h"

#include <chrono>
#include <string>

namespace
{
	// 가방 창( 오른쪽 420 ) 왼쪽에 붙여 연다. 팔 땐 가방을 같이 봐야 한다
	const float SHOP_X = -10.f;
	const float SHOP_Y = 10.f;
	const float SHOP_WIDTH = 500.f;
	const float SHOP_HEIGHT = 480.f;
	const float ROW_HEIGHT = 66.f;
	const float BUY_WIDTH = 72.f;
	const float BUY_HEIGHT = 30.f;
	const float MESSAGE_SEC = 1.6f;

	std::wstring g_message;
	bool g_message_good = true;
	std::chrono::steady_clock::time_point g_message_time;

	float GetLeft()
	{
		return SHOP_X - SHOP_WIDTH * 0.5f;
	}

	float GetRight()
	{
		return SHOP_X + SHOP_WIDTH * 0.5f;
	}

	float GetTopEdge()
	{
		return SHOP_Y + SHOP_HEIGHT * 0.5f;
	}

	float GetRowY(int32 row)
	{
		return GetTopEdge() - 112.f - static_cast<float>(row) * ROW_HEIGHT;
	}

	float GetBuyX()
	{
		return GetRight() - BUY_WIDTH * 0.5f - 18.f;
	}

	void SetMessage(const std::wstring& text, bool good)
	{
		g_message = text;
		g_message_good = good;
		g_message_time = std::chrono::steady_clock::now();
	}

	// 값이 비싸질수록 이름이 밝은 금빛. 한 줄만 보고도 등급이 읽히게
	void GetTierColor(int32 price, float* r, float* g, float* b)
	{
		if (price >= 2500)
		{
			*r = 0.55f;
			*g = 0.78f;
			*b = 1.f;
			return;
		}

		if (price >= 900)
		{
			*r = 1.f;
			*g = 0.82f;
			*b = 0.4f;
			return;
		}

		*r = 0.92f;
		*g = 0.88f;
		*b = 0.78f;
	}

	std::wstring DescribeItemStats(const ItemData& item)
	{
		std::wstring text;

		auto append = [&text](const std::wstring& part)
		{
			if (text.empty() == false)
			{
				text += L" · ";
			}

			text += part;
		};

		if (item.stat.attack > 0)
		{
			append(L"공격 " + std::to_wstring(item.stat.attack));
		}

		if (item.stat.defence > 0)
		{
			append(L"방어 " + std::to_wstring(item.stat.defence));
		}

		if (item.stat.max_hp > 0)
		{
			append(L"체력 " + std::to_wstring(item.stat.max_hp));
		}

		if (item.stat.crit_rate > 0)
		{
			append(L"치명 " + std::to_wstring(item.stat.crit_rate) + L"%");
		}

		if (item.stat.penetration > 0)
		{
			append(L"관통 " + std::to_wstring(item.stat.penetration));
		}

		for (const ItemEffect& effect : item.effects)
		{
			if (effect.type == ITEM_EFFECT_HEAL_HP)
			{
				append(L"체력 " + std::to_wstring(effect.value) + L" 회복");
			}
		}

		return text;
	}

	// 지금 낀 것과 견줘야 살지 말지 정할 수 있다
	std::wstring DescribeUpgrade(const ItemData& item)
	{
		if (item.equip_area == EQUIP_AREA_NONE)
		{
			return L"";
		}

		const ItemSlot& worn = GetPlayer().m_inventory.GetEquip(item.equip_area);
		if (worn.item_id == item.item_id)
		{
			return L"착용 중";
		}

		const CombatStat& now = GetItemData(worn.item_id).stat;
		int32 delta = item.stat.attack - now.attack;
		if (item.equip_area == EQUIP_AREA_ARMOR)
		{
			delta = item.stat.defence - now.defence;
		}

		if (delta > 0)
		{
			return L"+" + std::to_wstring(delta);
		}

		return L"";
	}
}

bool IsInsideShopWindow(float x, float y)
{
	return IsInsideBox(x, y, SHOP_X, SHOP_Y, SHOP_WIDTH + 8.f, SHOP_HEIGHT + 8.f);
}

void DrawShopWindow(Renderer* renderer, int32 shop_kind, float mouse_x, float mouse_y, std::vector<TooltipLine>* tooltip)
{
	const ShopData& shop = GetShopData(shop_kind);
	const Player& player = GetPlayer();
	const float left = GetLeft();
	const float top = GetTopEdge();

	DrawPanel(renderer, SHOP_X, SHOP_Y, SHOP_WIDTH, SHOP_HEIGHT, shop.name + L"  ( F )");
	DrawStringShadow(renderer, L"\"" + shop.greeting + L"\"", SHOP_X, top - 48.f, TEXT_ALIGN_CENTER, 0.68f, 0.62f, 0.5f, 1.f);

	for (size_t index = 0; index < shop.goods.size(); ++index)
	{
		const int32 row = static_cast<int32>(index);
		const ItemData& item = GetItemData(shop.goods[index]);
		const float y = GetRowY(row);
		const float slot_x = left + 42.f;
		const bool row_hovered = IsInsideBox(mouse_x, mouse_y, SHOP_X, y, SHOP_WIDTH - 20.f, ROW_HEIGHT - 6.f);

		if (row_hovered)
		{
			renderer->DrawSolidBox(SHOP_X, y, SHOP_WIDTH - 20.f, ROW_HEIGHT - 6.f, 0.12f, 0.095f, 0.07f, 1.f);
		}

		ItemSlot shown;
		shown.item_id = item.item_id;
		shown.count = 1;

		const bool icon_hovered = IsInsideBox(mouse_x, mouse_y, slot_x, y, UI_SLOT + 4.f, UI_SLOT + 4.f);
		DrawSlotBox(renderer, slot_x, y, UI_SLOT, icon_hovered, false);
		DrawItemInSlot(renderer, shown, slot_x, y, 1.f);

		if (icon_hovered)
		{
			BuildItemTooltip(shown, tooltip);

			// 끝줄 "더블클릭 : 장착" 은 가방 안내라 상점에선 뺀다
			if (item.category != ITEM_CATEGORY_ETC && tooltip->empty() == false)
			{
				tooltip->pop_back();
			}
		}

		float name_r = 0.f;
		float name_g = 0.f;
		float name_b = 0.f;
		GetTierColor(item.price, &name_r, &name_g, &name_b);

		const float text_x = slot_x + 34.f;
		DrawStringShadow(renderer, item.name, text_x, y + 24.f, TEXT_ALIGN_LEFT, name_r, name_g, name_b, 1.f);
		DrawStringShadow(renderer, DescribeItemStats(item), text_x, y - 2.f, TEXT_ALIGN_LEFT, 0.62f, 0.6f, 0.55f, 1.f);

		// 이름 바로 뒤에 붙인다. 오른쪽 끝에 두면 긴 이름과 겹친다
		const std::wstring upgrade = DescribeUpgrade(item);
		if (upgrade.empty() == false)
		{
			const float upgrade_x = text_x + MeasureString(item.name) + 10.f;
			if (upgrade[0] == L'+')
			{
				DrawStringShadow(renderer, upgrade, upgrade_x, y + 24.f, TEXT_ALIGN_LEFT, 0.5f, 0.9f, 0.55f, 1.f);
			}
			else
			{
				DrawStringShadow(renderer, upgrade, upgrade_x, y + 24.f, TEXT_ALIGN_LEFT, 0.55f, 0.52f, 0.46f, 1.f);
			}
		}

		// 못 사는 값은 붉게. 사러 오기 전에 얼마를 모아야 하는지 보인다
		const bool affordable = player.m_gold >= item.price;
		float price_r = 1.f;
		float price_g = 0.84f;
		float price_b = 0.35f;
		if (affordable == false)
		{
			price_r = 0.85f;
			price_g = 0.3f;
			price_b = 0.25f;
		}

		// 값은 이름 줄에 둔다. 아랫줄은 수치가 길게 늘어선다
		const float price_x = GetBuyX() - BUY_WIDTH * 0.5f - 14.f;
		DrawStringShadow(renderer, std::to_wstring(item.price), price_x, y + 24.f, TEXT_ALIGN_RIGHT, price_r, price_g, price_b, 1.f);
		DrawSprite(renderer, SPRITE_ICON_GOLD, 0, price_x - MeasureString(std::to_wstring(item.price)) - 14.f, y - 2.f, UI_SPRITE_SCALE * 0.75f, false, 1.f, 1.f, 1.f, 1.f);

		const bool buy_hovered = IsInsideBox(mouse_x, mouse_y, GetBuyX(), y, BUY_WIDTH, BUY_HEIGHT);
		DrawButton(renderer, GetBuyX(), y, BUY_WIDTH, BUY_HEIGHT, L"사기", affordable, buy_hovered);
	}

	const float bottom = SHOP_Y - SHOP_HEIGHT * 0.5f;
	DrawSprite(renderer, SPRITE_ICON_GOLD, 0, left + 26.f, bottom + 48.f, UI_SPRITE_SCALE, false, 1.f, 1.f, 1.f, 1.f);
	DrawStringShadow(renderer, std::to_wstring(player.m_gold), left + 48.f, bottom + 74.f, TEXT_ALIGN_LEFT, 1.f, 0.84f, 0.35f, 1.f);

	const int32 sell_percent = static_cast<int32>(GetConfig().shop_sell_rate * 100.f + 0.5f);
	DrawStringShadow(renderer, L"가방에서 우클릭하면 판다 ( 값의 " + std::to_wstring(sell_percent) + L"% )", SHOP_X, bottom + 32.f, TEXT_ALIGN_CENTER, 0.55f, 0.52f, 0.46f, 1.f);

	const std::chrono::duration<float> since = std::chrono::steady_clock::now() - g_message_time;
	if (g_message.empty() == false && since.count() < MESSAGE_SEC)
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

		DrawStringShadow(renderer, g_message, GetRight() - 18.f, bottom + 74.f, TEXT_ALIGN_RIGHT, r, g, b, 1.f);
	}
}

bool HandleShopDown(int32 shop_kind, float x, float y)
{
	const ShopData& shop = GetShopData(shop_kind);

	for (size_t index = 0; index < shop.goods.size(); ++index)
	{
		const float row_y = GetRowY(static_cast<int32>(index));
		if (IsInsideBox(x, y, GetBuyX(), row_y, BUY_WIDTH, BUY_HEIGHT) == false)
		{
			continue;
		}

		const int32 item_id = shop.goods[index];
		const int32 result = BuyShopItem(item_id);

		if (result == SHOP_RESULT_OK)
		{
			SetMessage(GetItemData(item_id).name + L" 샀다", true);
			PlaySfx(SFX_GOLD, 0.8f);
		}
		else
		{
			SetMessage(GetShopResultText(result), false);
			PlaySfx(SFX_UI_ERROR, 0.6f);
		}

		return true;
	}

	return true;
}

void NotifyShopSold(int32 result, int32 gold)
{
	if (result == SHOP_RESULT_OK)
	{
		SetMessage(L"+" + std::to_wstring(gold) + L" 골드", true);
		PlaySfx(SFX_GOLD, 0.7f);
		return;
	}

	SetMessage(GetShopResultText(result), false);
	PlaySfx(SFX_UI_ERROR, 0.6f);
}
