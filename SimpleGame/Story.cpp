#include "stdafx.h"
#include "Story.h"
#include "Audio.h"
#include "Cinematic.h"
#include "Feel.h"
#include "FloatingText.h"
#include "Instance.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Inventory.h"
#include "Monster.h"
#include "Player.h"
#include "Quest.h"
#include "Renderer.h"
#include "Text.h"
#include "World.h"
#include "ZoneGen.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
	constexpr int32 STATE_NONE = 0;
	constexpr int32 STATE_OPENING = 1;	// 경비대와 그림자
	constexpr int32 STATE_TUTORIAL = 2;	// 최강자로 그림자를 벤다
	constexpr int32 STATE_FALL = 3;	// 최강자의 죽음
	constexpr int32 STATE_PRESENT = 4;	// 수백 년 뒤. 메인 퀘스트
	constexpr int32 STATE_SCENE = 5;	// 메인 퀘스트 사이 컷신

	const float PI = 3.14159265f;
	const float TUTORIAL_RING = 4.5f;
	const float FALL_DELAY_SEC = 1.6f;

	int32 g_state = STATE_NONE;
	std::vector<int32> g_tutorial_monsters;
	float g_fall_timer = 0.f;
	int32 g_next_step = 0;	// 컷신이 끝나면 줄 메인 퀘스트
	std::string g_reveal;	// 컷신이 끝나면 드러낼 입구

	int32 CountAlive()
	{
		int32 alive = 0;
		for (int32 monster : g_tutorial_monsters)
		{
			if (IsMonsterAlive(monster))
			{
				alive += 1;
			}
		}

		return alive;
	}

	// 최강자는 세다. 레벨업이 능력치를 다시 짜니 튜토리얼 동안 매번 덧씌운다
	void ApplyChampionBuff()
	{
		const StoryConfig& config = GetStoryConfig();
		Player& player = MutablePlayer();
		const int32 hp = player.m_hp;

		player.RecalcStat();
		player.m_stat.attack *= config.tutorial_attack_rate;
		player.m_stat.max_hp *= config.tutorial_hp_rate;
		player.m_hp = std::min(hp, player.m_stat.max_hp);
	}

	void StartTutorial()
	{
		g_state = STATE_TUTORIAL;
		Player& player = MutablePlayer();

		// 검부터 쥐여 준다. 최강자가 맨손이면 안 된다
		const int32 sword = player.m_inventory.FindFirst(ITEM_ID_SWORD);
		if (player.HasWeapon() == false && sword >= 0)
		{
			UseBagItem(sword);
		}

		ApplyChampionBuff();
		player.m_hp = player.m_stat.max_hp;

		g_tutorial_monsters.clear();
		const int32 count = GetStoryConfig().tutorial_kills;
		for (int32 index = 0; index < count; ++index)
		{
			const float angle = PI * 2.f * static_cast<float>(index) / static_cast<float>(count) + 0.4f;
			WorldPos pos = player.m_pos;
			MoveWorldPos(&pos, std::cos(angle) * TUTORIAL_RING, std::sin(angle) * TUTORIAL_RING);

			// 집 · 벽 속이면 반지름을 줄여 본다
			for (float radius = TUTORIAL_RING; radius > 1.5f && IsBodyBlocked(pos, 0.45f); radius -= 0.5f)
			{
				pos = player.m_pos;
				MoveWorldPos(&pos, std::cos(angle) * radius, std::sin(angle) * radius);
			}

			const int32 monster = AddMonster(MONSTER_SHADOW_SOLDIER, pos, BAND_INTRO, false);
			SetMonsterInstanced(monster, true);
			g_tutorial_monsters.push_back(monster);
		}

		AddDamageText(player.m_pos, 140.f, L"그림자들이 일어섰다", DAMAGE_STYLE_BANNER_BAD);
		PlaySfx(SFX_DANGER, 0.9f);
		std::cout << "[ Story - StartTutorial ] : monsters " << g_tutorial_monsters.size() << "\n";
	}

	void StartPresent()
	{
		g_state = STATE_PRESENT;

		// 최강자의 힘을 거두고 신참 용병으로 성채에서 일어선다
		Player& player = MutablePlayer();
		player.RecalcStat();
		player.m_hp = player.m_stat.max_hp;
		TeleportPlayer(player.m_spawn_pos);

		for (int32 monster : g_tutorial_monsters)
		{
			SetMonsterInstanced(monster, false);
		}

		g_tutorial_monsters.clear();
		StartStoryQuest(0);
	}
}

void InitStory()
{
	if (GetStoryConfig().play_opening == false)
	{
		SkipStoryOpening();
		return;
	}

	g_state = STATE_OPENING;
	StartCinematic("opening");
}

void SkipStoryOpening()
{
	if (IsCinematicPlaying())
	{
		SkipCinematic();
		PopFinishedCinematic();
	}

	if (g_state == STATE_PRESENT)
	{
		return;
	}

	StartPresent();
}

void UpdateStory(float real_seconds)
{
	const std::string finished = PopFinishedCinematic();

	switch (g_state)
	{
		case STATE_OPENING:
		{
			if (finished == "opening")
			{
				// 시작 컷신의 거리에서 바로 싸운다
				WorldPos street = GetPlayer().m_pos;
				GetCinematicAnchor("citadel_street", &street);
				TeleportPlayer(street);
				StartTutorial();
			}

			break;
		}
		case STATE_TUTORIAL:
		{
			ApplyChampionBuff();

			if (CountAlive() > 0)
			{
				g_fall_timer = FALL_DELAY_SEC;
				break;
			}

			g_fall_timer -= real_seconds;
			if (g_fall_timer <= 0.f && IsPlayerDead() == false)
			{
				g_state = STATE_FALL;
				SetCinematicAnchor("player", GetPlayer().m_pos);
				StartCinematic("knight_fall");
			}

			break;
		}
		case STATE_FALL:
		{
			if (finished == "knight_fall")
			{
				StartPresent();
			}

			break;
		}
		case STATE_SCENE:
		{
			if (finished.empty() == false)
			{
				g_state = STATE_PRESENT;
				if (g_reveal.empty() == false)
				{
					RevealInstance(g_reveal);
					g_reveal.clear();
				}

				StartStoryQuest(g_next_step);
			}

			break;
		}
		default:
		{
			break;
		}
	}
}

void OnStoryStepReported(int32 step)
{
	const std::vector<StoryStep>& steps = GetStorySteps();
	if (step < 0 || step >= static_cast<int32>(steps.size()))
	{
		return;
	}

	QueueNarration(steps[static_cast<size_t>(step)].outro);
	g_next_step = step + 1;
	g_reveal = steps[static_cast<size_t>(step)].reveal;

	// 컷신이 있으면 다 본 뒤 다음 단계를 준다
	if (steps[static_cast<size_t>(step)].cinematic.empty() == false)
	{
		g_state = STATE_SCENE;
		SetCinematicAnchor("player", GetPlayer().m_pos);
		StartCinematic(steps[static_cast<size_t>(step)].cinematic);
		return;
	}

	if (g_reveal.empty() == false)
	{
		RevealInstance(g_reveal);
		g_reveal.clear();
	}

	StartStoryQuest(g_next_step);
}

void DrawStoryOverlay(Renderer* renderer)
{
	if (g_state != STATE_TUTORIAL || IsCinematicPlaying())
	{
		return;
	}

	const int32 total = static_cast<int32>(g_tutorial_monsters.size());
	const int32 killed = total - CountAlive();
	const float top = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - 90.f;

	renderer->DrawSolidBox(0.f, top - 40.f, 560.f, 104.f, 0.02f, 0.01f, 0.03f, 0.6f);
	DrawStringOutline(renderer, L"최강자  —  그림자 괴물 처치  " + std::to_wstring(killed) + L" / " + std::to_wstring(total), 0.f, top, TEXT_ALIGN_CENTER, 1, 1.f, 0.85f, 0.45f, 1.f);
	DrawStringShadow(renderer, L"WASD 이동   좌클릭 베기   우클릭 패링 ( 칼날이 번쩍일 때 )", 0.f, top - 34.f, TEXT_ALIGN_CENTER, 0.85f, 0.8f, 0.7f, 1.f);
	DrawStringShadow(renderer, L"Space 대쉬   Shift 구르기   패링하면 스킬 쿨이 줄어든다", 0.f, top - 60.f, TEXT_ALIGN_CENTER, 0.7f, 0.66f, 0.58f, 1.f);
}

bool IsStoryTutorial()
{
	return g_state == STATE_TUTORIAL;
}
