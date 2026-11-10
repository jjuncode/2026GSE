#include "stdafx.h"
#include "SkillStone.h"
#include "Audio.h"
#include "Feel.h"
#include "FloatingText.h"
#include "GameData.h"
#include "Inventory.h"
#include "Player.h"
#include "UiWindows.h"

#include <algorithm>
#include <iostream>
#include <map>

namespace
{
	// 구역을 새로 만들어도 남는다 ( 성장 )
	std::map<int32, int32> g_levels;	// 스킬 -> 강화 단계
	std::map<int32, int32> g_inscribed;	// 스킬 -> 새긴 각인 번호

	const SkillInscription* FindInscription(int32 skill_id)
	{
		const auto found = g_inscribed.find(skill_id);
		if (found == g_inscribed.end())
		{
			return nullptr;
		}

		const std::vector<SkillInscription>& inscriptions = GetSkillStoneConfig().inscriptions;
		if (found->second < 0 || found->second >= static_cast<int32>(inscriptions.size()))
		{
			return nullptr;
		}

		return &inscriptions[static_cast<size_t>(found->second)];
	}
}

int32 GetSkillUpgradeLevel(int32 skill_id)
{
	const auto found = g_levels.find(skill_id);
	if (found == g_levels.end())
	{
		return 0;
	}

	return found->second;
}

int32 GetSkillUpgradeMax()
{
	return GetSkillStoneConfig().upgrade_max;
}

bool TryUpgradeSkill(int32 skill_id)
{
	Player& player = MutablePlayer();
	const SkillStoneConfig& config = GetSkillStoneConfig();
	const SkillData& skill = GetSkillData(skill_id);

	if (player.HasSkill(skill_id) == false || skill.skill_type != SKILL_TYPE_ATTACK)
	{
		return false;
	}

	if (GetSkillUpgradeLevel(skill_id) >= config.upgrade_max)
	{
		AddDamageText(player.m_pos, 80.f, skill.alias + L" 은(는) 더 강화할 수 없다", DAMAGE_STYLE_INFO);
		PlaySfx(SFX_UI_ERROR, 0.5f);
		return true;
	}

	if (player.m_inventory.RemoveItem(config.upgrade_item, 1) == false)
	{
		AddDamageText(player.m_pos, 80.f, L"스킬 강화석이 없다", DAMAGE_STYLE_INFO);
		PlaySfx(SFX_UI_ERROR, 0.5f);
		return true;
	}

	g_levels[skill_id] = GetSkillUpgradeLevel(skill_id) + 1;
	const int32 percent = static_cast<int32>(config.upgrade_damage_rate * 100.f * static_cast<float>(g_levels[skill_id]) + 0.5f);

	AddDamageText(player.m_pos, 100.f, skill.alias + L"  +" + std::to_wstring(g_levels[skill_id]) + L"  ( 피해 +" + std::to_wstring(percent) + L"% )", DAMAGE_STYLE_BANNER_GOOD);
	AddScreenFlash(0.5f, 0.8f, 1.f, 0.15f);
	PlaySfx(SFX_ENHANCE_SUCCESS, 0.9f);

	std::cout << "[ SkillStone - TryUpgradeSkill ] : skill " << skill_id << " level " << g_levels[skill_id] << "\n";
	return true;
}

bool UseSkillStoneItem(int32 slot_idx)
{
	Player& player = MutablePlayer();
	const ItemSlot& slot = player.m_inventory.GetBag(slot_idx);
	const SkillStoneConfig& config = GetSkillStoneConfig();

	// 강화석은 어느 스킬에 쓸지 골라야 해서 스킬 창으로 보낸다
	if (slot.item_id == config.upgrade_item)
	{
		ToggleSkillWindow();
		AddDamageText(player.m_pos, 80.f, L"스킬 창에서 강화할 스킬을 누르세요", DAMAGE_STYLE_INFO);
		return true;
	}

	const std::vector<SkillInscription>& inscriptions = config.inscriptions;
	for (size_t index = 0; index < inscriptions.size(); ++index)
	{
		const SkillInscription& inscription = inscriptions[index];
		if (inscription.item_id != slot.item_id)
		{
			continue;
		}

		const SkillData& skill = GetSkillData(inscription.skill_id);
		if (player.HasSkill(inscription.skill_id) == false)
		{
			AddDamageText(player.m_pos, 80.f, L"먼저 " + skill.alias + L" 을(를) 해금하세요 ( K )", DAMAGE_STYLE_INFO);
			PlaySfx(SFX_UI_ERROR, 0.5f);
			return true;
		}

		player.m_inventory.Consume(slot_idx);
		g_inscribed[inscription.skill_id] = static_cast<int32>(index);

		AddDamageText(player.m_pos, 120.f, L"각인  " + inscription.name, DAMAGE_STYLE_BANNER_GOOD);
		AddScreenFlash(1.f, 0.3f, 0.4f, 0.3f);
		AddShake(0.3f);
		PlaySfx(SFX_LEVEL_UP, 0.9f);

		std::cout << "[ SkillStone - UseSkillStoneItem ] : skill " << inscription.skill_id << " inscribed\n";
		return true;
	}

	return false;
}

float GetPlayerSkillDamageRate(int32 skill_id)
{
	float rate = 1.f + GetSkillStoneConfig().upgrade_damage_rate * static_cast<float>(GetSkillUpgradeLevel(skill_id));

	const SkillInscription* inscription = FindInscription(skill_id);
	if (inscription != nullptr)
	{
		rate *= 1.f + inscription->damage_rate;
	}

	return rate;
}

float GetPlayerSkillExtraStun(int32 skill_id)
{
	const SkillInscription* inscription = FindInscription(skill_id);
	if (inscription == nullptr)
	{
		return 0.f;
	}

	return inscription->stun_sec;
}

float GetPlayerSkillCooldownCut(int32 skill_id)
{
	const SkillInscription* inscription = FindInscription(skill_id);
	if (inscription == nullptr)
	{
		return 0.f;
	}

	return inscription->cooldown_cut;
}

std::wstring GetSkillInscriptionName(int32 skill_id)
{
	const SkillInscription* inscription = FindInscription(skill_id);
	if (inscription == nullptr)
	{
		return L"";
	}

	return inscription->name;
}
