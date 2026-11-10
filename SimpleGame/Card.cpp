#include "stdafx.h"
#include "Card.h"
#include "Audio.h"
#include "Feel.h"
#include "FloatingText.h"
#include "Player.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

namespace
{
	// 구역을 새로 만들어도 모은 카드는 남는다 ( 성장 )
	std::vector<int32> g_copies;

	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x51ed270bu);
		return random;
	}

	void EnsureSize()
	{
		if (g_copies.size() != GetCardDatas().size())
		{
			g_copies.resize(GetCardDatas().size(), 0);
		}
	}

	int32 FindCard(int32 monster_type)
	{
		const std::vector<CardData>& cards = GetCardDatas();
		for (size_t index = 0; index < cards.size(); ++index)
		{
			if (cards[index].monster_type == monster_type)
			{
				return static_cast<int32>(index);
			}
		}

		return -1;
	}

	void AddStat(CombatStat* stat, int32 kind, int32 value)
	{
		switch (kind)
		{
			case CARD_STAT_ATTACK:
			{
				stat->attack += value;
				break;
			}
			case CARD_STAT_DEFENCE:
			{
				stat->defence += value;
				break;
			}
			case CARD_STAT_MAX_HP:
			{
				stat->max_hp += value;
				break;
			}
			case CARD_STAT_CRIT_RATE:
			{
				stat->crit_rate += value;
				break;
			}
			case CARD_STAT_CRIT_DAMAGE:
			{
				stat->crit_damage += value;
				break;
			}
			case CARD_STAT_PENETRATION:
			{
				stat->penetration += value;
				break;
			}
			default:
			{
				stat->toughness += value;
				break;
			}
		}
	}
}

int32 GetCardCount()
{
	return static_cast<int32>(GetCardDatas().size());
}

const CardData& GetCardDataAt(int32 index)
{
	return GetCardDatas()[static_cast<size_t>(index)];
}

int32 GetCardCopies(int32 index)
{
	EnsureSize();
	if (index < 0 || index >= static_cast<int32>(g_copies.size()))
	{
		return 0;
	}

	return g_copies[static_cast<size_t>(index)];
}

int32 GetCardLevel(int32 index)
{
	const int32 copies = GetCardCopies(index);
	int32 level = 0;

	for (int32 need : GetCardLevelCopies())
	{
		if (copies >= need)
		{
			level += 1;
		}
	}

	return level;
}

int32 GetCardNextCopies(int32 index)
{
	const int32 copies = GetCardCopies(index);
	for (int32 need : GetCardLevelCopies())
	{
		if (copies < need)
		{
			return need;
		}
	}

	return 0;
}

std::wstring DescribeCardBonus(const CardData& card, int32 level)
{
	const std::array<const wchar_t*, 7> names = { L"공격력", L"방어력", L"최대 체력", L"치명타", L"치명 피해", L"관통", L"경직 저항" };
	const int32 value = card.value * std::max(level, 1);

	std::wstring text = std::wstring(names[static_cast<size_t>(std::min(std::max(card.stat, 0), 6))]) + L" +" + std::to_wstring(value);
	if (card.stat == CARD_STAT_CRIT_RATE || card.stat == CARD_STAT_CRIT_DAMAGE || card.stat == CARD_STAT_TOUGHNESS)
	{
		text += L"%";
	}

	return text;
}

void RollCardDrop(int32 monster_type, const WorldPos& pos)
{
	const int32 index = FindCard(monster_type);
	if (index < 0)
	{
		return;
	}

	const CardData& card = GetCardDataAt(index);
	std::uniform_real_distribution<float> roll(0.f, 100.f);
	if (roll(GetRandom()) >= card.rate)
	{
		return;
	}

	EnsureSize();
	const int32 before = GetCardLevel(index);
	g_copies[static_cast<size_t>(index)] += 1;
	const int32 after = GetCardLevel(index);

	std::wstring text = L"카드  " + GetMonsterData(monster_type).name;
	if (after > before)
	{
		// 레벨이 올랐으면 능력치를 다시 센다
		text += L"  Lv " + std::to_wstring(after) + L"  ·  " + DescribeCardBonus(card, after);
		MutablePlayer().RecalcStat();
	}

	AddDamageText(pos, 120.f, text, DAMAGE_STYLE_BANNER_GOOD);
	SpawnRing(pos, 1.6f, 1.6f, 1.3f, 0.5f, 0.6f);
	PlaySfx(SFX_SKILL_UNLOCK, 0.7f);

	std::cout << "[ Card - RollCardDrop ] : monster " << monster_type << " copies " << g_copies[static_cast<size_t>(index)] << " level " << after << "\n";
}

void AddCardStat(CombatStat* stat)
{
	for (int32 index = 0; index < GetCardCount(); ++index)
	{
		const int32 level = GetCardLevel(index);
		if (level > 0)
		{
			AddStat(stat, GetCardDataAt(index).stat, GetCardDataAt(index).value * level);
		}
	}
}
