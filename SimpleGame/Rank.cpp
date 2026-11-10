#include "stdafx.h"
#include "Rank.h"
#include "Audio.h"
#include "Feel.h"
#include "FloatingText.h"
#include "Player.h"
#include "SpriteAtlas.h"

#include <algorithm>
#include <iostream>

namespace
{
	int32 g_rank = 0;
	int32 g_points = 0;

	const float RANK_BANNER_LIFT = 170.f;
}

int32 GetPlayerRank()
{
	return g_rank;
}

int32 GetRankPoints()
{
	return g_points;
}

int32 GetRankCount()
{
	return static_cast<int32>(GetRankConfig().ranks.size());
}

const RankData& GetRankData(int32 rank)
{
	static const RankData empty;

	const std::vector<RankData>& ranks = GetRankConfig().ranks;
	if (rank < 0 || rank >= static_cast<int32>(ranks.size()))
	{
		return empty;
	}

	return ranks[static_cast<size_t>(rank)];
}

std::wstring GetRankName(int32 rank)
{
	return GetRankData(rank).name;
}

bool IsRankExamReady()
{
	const int32 next = g_rank + 1;
	if (next >= GetRankCount())
	{
		return false;
	}

	return g_points >= GetRankData(next).need_points && GetRankData(next).has_exam;
}

void AddRankPoints(int32 points)
{
	if (points <= 0)
	{
		return;
	}

	const bool was_ready = IsRankExamReady();
	g_points += points;

	AddDamageText(GetPlayer().m_pos, RANK_BANNER_LIFT - 60.f, L"공헌 +" + std::to_wstring(points), DAMAGE_STYLE_INFO);

	// 심사 자격이 막 생긴 순간에만 알린다. 의뢰마다 알리면 시끄럽다
	if (was_ready == false && IsRankExamReady())
	{
		AddDamageText(GetPlayer().m_pos, RANK_BANNER_LIFT, GetRankName(g_rank + 1) + L" 승급 심사가 게시판에 붙었다", DAMAGE_STYLE_BANNER_GOOD);
	}
}

void PromotePlayerRank()
{
	if (g_rank + 1 >= GetRankCount())
	{
		return;
	}

	g_rank += 1;

	const RankData& rank = GetRankData(g_rank);
	AddDamageText(GetPlayer().m_pos, RANK_BANNER_LIFT, L"승급  " + rank.name + L" 등급", DAMAGE_STYLE_BANNER_GOOD);
	AddScreenFlash(rank.color[0], rank.color[1], rank.color[2], 0.35f);
	AddZoomPunch(0.04f);
	PlaySfx(SFX_LEVEL_UP, 1.f);

	std::cout << "[ Rank - PromotePlayerRank ] : rank " << g_rank << " points " << g_points << "\n";
}

int32 GetTierMinRank(int32 tier)
{
	if (tier < 0 || tier >= QUEST_TIER_COUNT)
	{
		return 0;
	}

	return GetRankConfig().tier_min_rank[static_cast<size_t>(tier)];
}

bool CanAcceptQuestTier(int32 tier)
{
	return g_rank >= GetTierMinRank(tier);
}

void DrawRankBadge(Renderer* renderer, int32 rank, float x, float y, float scale)
{
	static int32 s_sprite = -2;
	if (s_sprite == -2)
	{
		s_sprite = FindSpriteByName("rank_badge");
	}

	if (s_sprite == SPRITE_NONE)
	{
		return;
	}

	DrawSprite(renderer, s_sprite, rank, x, y, scale, false, 1.f, 1.f, 1.f, 1.f);
}
