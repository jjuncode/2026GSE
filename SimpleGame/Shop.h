#pragma once

#include "GameData.h"

struct ItemSlot;

constexpr int32 SHOP_RESULT_OK = 0;
constexpr int32 SHOP_RESULT_NO_GOLD = 1;
constexpr int32 SHOP_RESULT_BAG_FULL = 2;
constexpr int32 SHOP_RESULT_NOT_FOR_SALE = 3;	// 값이 없는 물건

// 사고팔기 규칙. 가방과 지갑만 만진다. 상점이 어디 있는지는 Scene 이 안다
int32 GetShopSellPrice(const ItemSlot& slot);	// 한 개 값. 강화한 무기는 들인 골드 일부를 더 쳐준다
int32 BuyShopItem(int32 item_id);
int32 SellBagItem(int32 slot_idx);	// 묶음이면 하나씩
const wchar_t* GetShopResultText(int32 result);
