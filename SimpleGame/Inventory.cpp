#include "stdafx.h"
#include "Inventory.h"
#include "Enhance.h"

#include <algorithm>

namespace
{
	const ItemSlot EMPTY_SLOT;

	bool IsValidArea(int32 area)
	{
		return area >= 0 && area < EQUIP_AREA_COUNT;
	}
}

void Inventory::Init(int32 slot_count)
{
	m_bag.assign(static_cast<size_t>(std::max(1, slot_count)), ItemSlot());
	m_equips.fill(ItemSlot());
	m_last_uid = 0;
}

int32 Inventory::GetBagSize() const
{
	return static_cast<int32>(m_bag.size());
}

const ItemSlot& Inventory::GetBag(int32 slot_idx) const
{
	if (slot_idx < 0 || slot_idx >= GetBagSize())
	{
		return EMPTY_SLOT;
	}

	return m_bag[static_cast<size_t>(slot_idx)];
}

const ItemSlot& Inventory::GetEquip(int32 area) const
{
	if (IsValidArea(area) == false)
	{
		return EMPTY_SLOT;
	}

	return m_equips[static_cast<size_t>(area)];
}

ItemSlot* Inventory::MutableEquip(int32 area)
{
	if (IsValidArea(area) == false)
	{
		return nullptr;
	}

	return &m_equips[static_cast<size_t>(area)];
}

bool Inventory::IsEmptySlot(const ItemSlot& slot) const
{
	return slot.item_id == ITEM_ID_NONE || slot.count <= 0;
}

bool Inventory::CanAdd(int32 item_id, int32 count) const
{
	const ItemData& item = GetItemData(item_id);
	if (item.item_id == ITEM_ID_NONE || count < 1)
	{
		return false;
	}

	int32 room = 0;
	for (const ItemSlot& slot : m_bag)
	{
		if (IsEmptySlot(slot))
		{
			room += item.max_stack;
		}
		else if (slot.item_id == item_id && slot.count < item.max_stack)
		{
			room += item.max_stack - slot.count;
		}
	}

	return room >= count;
}

bool Inventory::AddItem(int32 item_id, int32 count)
{
	if (CanAdd(item_id, count) == false)
	{
		return false;
	}

	const ItemData& item = GetItemData(item_id);
	int32 remain = count;

	// 같은 아이템 칸부터 채운다
	for (ItemSlot& slot : m_bag)
	{
		if (remain <= 0)
		{
			break;
		}

		if (IsEmptySlot(slot) || slot.item_id != item_id || slot.count >= item.max_stack)
		{
			continue;
		}

		const int32 add = std::min(remain, item.max_stack - slot.count);
		slot.count += add;
		remain -= add;
	}

	// 남으면 빈 칸에 새 묶음
	for (ItemSlot& slot : m_bag)
	{
		if (remain <= 0)
		{
			break;
		}

		if (IsEmptySlot(slot) == false)
		{
			continue;
		}

		const int32 add = std::min(remain, item.max_stack);
		m_last_uid += 1;
		slot.item_id = item_id;
		slot.count = add;
		slot.enhance = 0;
		slot.uid = m_last_uid;
		remain -= add;
	}

	return true;
}

bool Inventory::Move(int32 from_slot_idx, int32 to_slot_idx)
{
	if (from_slot_idx == to_slot_idx)
	{
		return false;
	}

	if (from_slot_idx < 0 || from_slot_idx >= GetBagSize() || to_slot_idx < 0 || to_slot_idx >= GetBagSize())
	{
		return false;
	}

	ItemSlot& from = m_bag[static_cast<size_t>(from_slot_idx)];
	ItemSlot& to = m_bag[static_cast<size_t>(to_slot_idx)];

	if (IsEmptySlot(from))
	{
		return false;
	}

	// 같은 묶음 아이템이면 받을 칸을 채우고 남는 것만 남긴다
	const ItemData& item = GetItemData(from.item_id);
	if (IsEmptySlot(to) == false && to.item_id == from.item_id && item.max_stack > 1 && to.count < item.max_stack)
	{
		const int32 add = std::min(from.count, item.max_stack - to.count);
		to.count += add;
		from.count -= add;

		if (from.count <= 0)
		{
			from = ItemSlot();
		}

		return true;
	}

	std::swap(from, to);
	return true;
}

bool Inventory::Equip(int32 slot_idx)
{
	if (slot_idx < 0 || slot_idx >= GetBagSize())
	{
		return false;
	}

	ItemSlot& bag = m_bag[static_cast<size_t>(slot_idx)];
	if (IsEmptySlot(bag))
	{
		return false;
	}

	const int32 area = GetItemData(bag.item_id).equip_area;
	if (IsValidArea(area) == false)
	{
		return false;
	}

	std::swap(bag, m_equips[static_cast<size_t>(area)]);
	return true;
}

bool Inventory::Unequip(int32 area, int32 slot_idx)
{
	if (IsValidArea(area) == false)
	{
		return false;
	}

	ItemSlot& equip = m_equips[static_cast<size_t>(area)];
	if (IsEmptySlot(equip))
	{
		return false;
	}

	if (slot_idx < 0)
	{
		slot_idx = FindEmptySlot();
	}

	if (slot_idx < 0 || slot_idx >= GetBagSize())
	{
		return false;
	}

	ItemSlot& bag = m_bag[static_cast<size_t>(slot_idx)];

	// 받을 칸에 같은 부위 장비가 있으면 맞바꿔 낀다. 다른 아이템이 있으면 못 뺀다
	if (IsEmptySlot(bag) == false)
	{
		if (GetItemData(bag.item_id).equip_area != area)
		{
			return false;
		}
	}

	std::swap(bag, equip);
	return true;
}

bool Inventory::Destroy(int32 slot_idx)
{
	if (slot_idx < 0 || slot_idx >= GetBagSize())
	{
		return false;
	}

	ItemSlot& bag = m_bag[static_cast<size_t>(slot_idx)];
	if (IsEmptySlot(bag))
	{
		return false;
	}

	bag = ItemSlot();
	return true;
}

bool Inventory::Consume(int32 slot_idx)
{
	if (slot_idx < 0 || slot_idx >= GetBagSize())
	{
		return false;
	}

	ItemSlot& bag = m_bag[static_cast<size_t>(slot_idx)];
	if (IsEmptySlot(bag))
	{
		return false;
	}

	bag.count -= 1;
	if (bag.count <= 0)
	{
		bag = ItemSlot();
	}

	return true;
}

int32 Inventory::FindFirst(int32 item_id) const
{
	for (int32 slot_idx = 0; slot_idx < GetBagSize(); ++slot_idx)
	{
		const ItemSlot& slot = m_bag[static_cast<size_t>(slot_idx)];
		if (IsEmptySlot(slot) == false && slot.item_id == item_id)
		{
			return slot_idx;
		}
	}

	return -1;
}

int32 Inventory::CountItem(int32 item_id) const
{
	int32 count = 0;

	for (const ItemSlot& slot : m_bag)
	{
		if (IsEmptySlot(slot) == false && slot.item_id == item_id)
		{
			count += slot.count;
		}
	}

	return count;
}

bool Inventory::RemoveItem(int32 item_id, int32 count)
{
	if (count <= 0)
	{
		return true;
	}

	if (CountItem(item_id) < count)
	{
		return false;
	}

	int32 remain = count;

	// 뒤 칸부터 깎아야 앞쪽 묶음이 안 쪼개진다
	for (int32 slot_idx = GetBagSize() - 1; slot_idx >= 0 && remain > 0; --slot_idx)
	{
		ItemSlot& slot = m_bag[static_cast<size_t>(slot_idx)];
		if (IsEmptySlot(slot) || slot.item_id != item_id)
		{
			continue;
		}

		const int32 take = std::min(remain, slot.count);
		slot.count -= take;
		remain -= take;

		if (slot.count <= 0)
		{
			slot = ItemSlot();
		}
	}

	return true;
}

int32 Inventory::FindEmptySlot() const
{
	for (int32 slot_idx = 0; slot_idx < GetBagSize(); ++slot_idx)
	{
		if (IsEmptySlot(m_bag[static_cast<size_t>(slot_idx)]))
		{
			return slot_idx;
		}
	}

	return -1;
}

void Inventory::AddEquipStat(CombatStat* stat) const
{
	for (const ItemSlot& equip : m_equips)
	{
		if (IsEmptySlot(equip))
		{
			continue;
		}

		const CombatStat& item_stat = GetItemData(equip.item_id).stat;
		stat->max_hp += item_stat.max_hp;
		stat->attack += item_stat.attack + GetEnhanceAttackBonus(equip.enhance);
		stat->defence += item_stat.defence;
		stat->crit_rate += item_stat.crit_rate;
		stat->crit_damage += item_stat.crit_damage;
		stat->penetration += item_stat.penetration;
		stat->toughness += item_stat.toughness;
	}
}
