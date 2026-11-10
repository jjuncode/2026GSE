#include "stdafx.h"
#include "Shop.h"
#include "Audio.h"
#include "Enhance.h"
#include "Inventory.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <iostream>

int32 GetShopSellPrice(const ItemSlot& slot)
{
	if (slot.item_id == ITEM_ID_NONE || slot.count <= 0)
	{
		return 0;
	}

	const ItemData& item = GetItemData(slot.item_id);
	if (item.price <= 0)
	{
		return 0;
	}

	int32 enhance_gold = 0;
	for (int32 level = 0; level < slot.enhance; ++level)
	{
		enhance_gold += GetEnhanceCost(level).gold;
	}

	const float rate = GetConfig().shop_sell_rate;
	return std::max(1, static_cast<int32>(std::floor(static_cast<float>(item.price + enhance_gold) * rate)));
}

int32 BuyShopItem(int32 item_id)
{
	const ItemData& item = GetItemData(item_id);
	if (item.price <= 0)
	{
		return SHOP_RESULT_NOT_FOR_SALE;
	}

	Player& player = MutablePlayer();
	if (player.m_gold < item.price)
	{
		return SHOP_RESULT_NO_GOLD;
	}

	// 넣을 자리부터 본다. 돈을 먼저 빼면 가방이 찼을 때 되돌려야 한다
	if (player.m_inventory.CanAdd(item_id, 1) == false)
	{
		return SHOP_RESULT_BAG_FULL;
	}

	player.m_inventory.AddItem(item_id, 1);
	player.m_gold -= item.price;

	std::cout << "[ Shop - BuyShopItem ] : item " << item_id << " price " << item.price << " gold " << player.m_gold << "\n";
	return SHOP_RESULT_OK;
}

int32 SellBagItem(int32 slot_idx)
{
	Player& player = MutablePlayer();
	const ItemSlot& slot = player.m_inventory.GetBag(slot_idx);
	const int32 price = GetShopSellPrice(slot);

	if (price <= 0)
	{
		return SHOP_RESULT_NOT_FOR_SALE;
	}

	const int32 item_id = slot.item_id;
	if (player.m_inventory.Consume(slot_idx) == false)
	{
		return SHOP_RESULT_NOT_FOR_SALE;
	}

	player.m_gold += price;

	std::cout << "[ Shop - SellBagItem ] : item " << item_id << " price " << price << " gold " << player.m_gold << "\n";
	return SHOP_RESULT_OK;
}

const wchar_t* GetShopResultText(int32 result)
{
	switch (result)
	{
		case SHOP_RESULT_NO_GOLD:
		{
			return L"골드가 모자라다";
		}
		case SHOP_RESULT_BAG_FULL:
		{
			return L"가방에 자리가 없다";
		}
		case SHOP_RESULT_NOT_FOR_SALE:
		{
			return L"팔 수 없는 물건이다";
		}
		default:
		{
			return L"";
		}
	}
}
