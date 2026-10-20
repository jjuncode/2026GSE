#pragma once

#include "GameData.h"

#include <array>
#include <vector>

struct ItemSlot
{
	int32 item_id = ITEM_ID_NONE;
	int32 count = 0;
	int32 enhance = 0;	// 장비만. 칸을 옮겨도 아이템을 따라간다
	uint32 uid = 0;
};

// 서버 Inventory 와 같은 규칙. 가방 칸 하나에 한 묶음, 장비칸은 부위마다 하나
class Inventory
{
public:
	void Init(int32 slot_count);

	int32 GetBagSize() const;
	const ItemSlot& GetBag(int32 slot_idx) const;
	const ItemSlot& GetEquip(int32 area) const;
	ItemSlot* MutableEquip(int32 area);
	bool IsEmptySlot(const ItemSlot& slot) const;

	// 들어갈 자리가 모자라면 아무것도 안 넣는다 ( 넣다 말면 되돌려야 해서 )
	bool AddItem(int32 item_id, int32 count);
	bool CanAdd(int32 item_id, int32 count) const;

	bool Move(int32 from_slot_idx, int32 to_slot_idx);	// 빈칸이면 이동, 같은 아이템이면 합치고, 다르면 맞바꿈
	bool Equip(int32 slot_idx);	// 장비칸과 맞바꾼다
	bool Unequip(int32 area, int32 slot_idx);	// slot_idx 가 -1 이면 첫 빈칸
	bool Destroy(int32 slot_idx);
	bool Consume(int32 slot_idx);	// 하나 쓴다

	int32 FindFirst(int32 item_id) const;	// 없으면 -1
	int32 CountItem(int32 item_id) const;
	bool RemoveItem(int32 item_id, int32 count);	// 모자라면 안 뺀다
	int32 FindEmptySlot() const;

	void AddEquipStat(CombatStat* stat) const;

private:
	std::vector<ItemSlot> m_bag;
	std::array<ItemSlot, EQUIP_AREA_COUNT> m_equips;
	uint32 m_last_uid = 0;
};
