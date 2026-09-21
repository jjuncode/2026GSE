#include "stdafx.h"
#include "Player.h"
#include "Balance.h"
#include "Enhance.h"
#include "FloatingText.h"
#include "GameConfig.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "World.h"

#include <cmath>
#include <iostream>

namespace
{
	constexpr int32 SWORD_GHOST_COUNT = 3;

	PlayerState g_player;

	float CountDown(float value, float delta_seconds)
	{
		const float next = value - delta_seconds;

		if (next < 0.f)
		{
			return 0.f;
		}

		return next;
	}

	// 1 이 안 되는 회복은 모아 뒀다 정수로 넘긴다. 매 틱 버리면 초당 4 회복이 0 이 된다
	void UpdateRegen(float delta_seconds)
	{
		const int32 max_hp = GetPlayerMaxHp();

		if (g_player.hp >= max_hp)
		{
			g_player.regen_carry = 0.f;
			return;
		}

		float per_second = Balance::PLAYER_COMBAT_REGEN_PER_SEC;
		if (g_player.since_hit >= Balance::PLAYER_REGEN_DELAY_SEC)
		{
			per_second = Balance::PLAYER_REGEN_PER_SEC;
		}

		g_player.regen_carry += per_second * delta_seconds;

		const int32 gained = static_cast<int32>(g_player.regen_carry);
		g_player.regen_carry -= static_cast<float>(gained);
		g_player.hp += gained;

		if (g_player.hp > max_hp)
		{
			g_player.hp = max_hp;
		}
	}

	void DrawSword(Renderer* renderer, float hand_x, float hand_y, float angle, float brightness, float alpha)
	{
		const float unit = static_cast<float>(PIXEL_SCALE);
		const float blade_x = std::cos(angle);
		const float blade_y = std::sin(angle);
		const float length = static_cast<float>(GetSpriteHeight(SPRITE_SWORD)) * unit;
		const float width = static_cast<float>(GetSpriteWidth(SPRITE_SWORD)) * unit;
		const float grip = 2.f * unit;	// 손잡이 끝에서 쥐는 자리까지

		// 세로축이 칼날 방향, 가로축은 그 수직
		AffineQuad quad;
		quad.axis_y_x = blade_x * length;
		quad.axis_y_y = blade_y * length;
		quad.axis_x_x = blade_y * width;
		quad.axis_x_y = -blade_x * width;
		quad.origin_x = hand_x - quad.axis_x_x * 0.5f - blade_x * grip;
		quad.origin_y = hand_y - quad.axis_x_y * 0.5f - blade_y * grip;

		DrawSpriteQuad(renderer, SPRITE_SWORD, 0, quad, false, brightness, brightness, brightness, alpha);
	}

	void DrawPlayerSword(Renderer* renderer, const WorldPos& camera)
	{
		const ScreenPos foot = WorldToScreen(g_player.body.m_pos, camera);

		float side = 1.f;
		if (g_player.body.m_flip)
		{
			side = -1.f;
		}

		// 그림 속 손 위치 ( 오른쪽을 볼 때 가운데에서 4칸 오른쪽, 발에서 11칸 위 )
		const float unit = static_cast<float>(PIXEL_SCALE);
		const float hand_x = foot.x + side * 4.f * unit;
		const float hand_y = foot.y + 11.5f * unit;

		if (g_player.swing_timer <= 0.f)
		{
			// 들고 있을 때는 앞쪽으로 비스듬히 세운다
			const float rest_angle = 3.14159265f * 0.5f - side * 0.45f;
			DrawSword(renderer, hand_x, hand_y, rest_angle, 1.f, 1.f);
			return;
		}

		// 조준 방향을 화면 각도로 바꾼다. 월드 방향 그대로 쓰면 아이소 변환만큼 어긋난다
		WorldPos ahead = g_player.body.m_pos;
		MoveWorldPos(&ahead, g_player.facing_x, g_player.facing_y);
		const ScreenPos ahead_screen = WorldToScreen(ahead, camera);
		const float aim_angle = std::atan2(ahead_screen.y - foot.y, ahead_screen.x - foot.x);

		const float half_angle = std::acos(Balance::SWING_HALF_COS);
		const float progress = 1.f - g_player.swing_timer / Balance::SWING_SHOW_SEC;
		const float eased = 1.f - (1.f - progress) * (1.f - progress);

		// 위에서 아래로 내려벤다. 왼쪽을 볼 때는 각도 방향을 뒤집어야 똑같이 위에서 시작한다
		for (int32 ghost = SWORD_GHOST_COUNT; ghost >= 1; --ghost)
		{
			const float ghost_progress = eased - static_cast<float>(ghost) * 0.14f;
			if (ghost_progress < 0.f)
			{
				continue;
			}

			const float ghost_angle = aim_angle + side * (half_angle - ghost_progress * 2.f * half_angle);
			const float ghost_alpha = 0.45f - static_cast<float>(ghost) * 0.11f;
			DrawSword(renderer, hand_x, hand_y, ghost_angle, 1.8f, ghost_alpha);
		}

		const float blade_angle = aim_angle + side * (half_angle - eased * 2.f * half_angle);
		DrawSword(renderer, hand_x, hand_y, blade_angle, 1.4f, 1.f);
	}
}

// 몸을 그린 다음이라야 칼이 몸 위에 얹힌다
void PlayerActor::Draw(Renderer* renderer, const WorldPos& camera) const
{
	Actor::Draw(renderer, camera);
	DrawPlayerSword(renderer, camera);
}

void InitPlayer(const WorldPos& spawn_pos)
{
	g_player = PlayerState();

	g_player.body.m_sprite = SPRITE_PLAYER;

	PlacePlayer(spawn_pos);
}

void PlacePlayer(const WorldPos& spawn_pos)
{
	g_player.spawn_pos = spawn_pos;
	g_player.body.m_pos = spawn_pos;
	g_player.respawn_timer = 0.f;
	g_player.hp = GetPlayerMaxHp();
}

const PlayerState& GetPlayer()
{
	return g_player;
}

PlayerState& MutablePlayer()
{
	return g_player;
}

bool IsPlayerDead()
{
	return g_player.respawn_timer > 0.f;
}

void UpdatePlayer(float delta_seconds)
{
	if (IsPlayerDead())
	{
		g_player.respawn_timer -= delta_seconds;

		if (g_player.respawn_timer <= 0.f)
		{
			PlacePlayer(g_player.spawn_pos);
		}

		return;
	}

	g_player.swing_cooldown = CountDown(g_player.swing_cooldown, delta_seconds);
	g_player.swing_timer = CountDown(g_player.swing_timer, delta_seconds);
	g_player.hit_flash = CountDown(g_player.hit_flash, delta_seconds);
	g_player.since_hit += delta_seconds;
	g_player.idle_timer += delta_seconds;

	UpdateRegen(delta_seconds);

	// 그림은 오른쪽을 본다. 화면 가로 성분이 음수면 뒤집는다
	g_player.body.m_flip = (g_player.facing_x - g_player.facing_y) < 0.f;

	// 걸은 거리로 넘겨야 속도가 달라도 발이 안 미끄러진다
	if (g_player.idle_timer < 0.1f)
	{
		g_player.body.m_frame = static_cast<int32>(g_player.walk_distance * 2.5f) % 2;
	}
	else
	{
		g_player.body.m_frame = 0;
	}

	g_player.body.m_emissive = 1.f + g_player.hit_flash / Balance::HIT_FLASH_SEC * 3.f;
}

void MovePlayer(float velocity_x, float velocity_y, float delta_seconds)
{
	if (IsPlayerDead())
	{
		return;
	}

	const WorldPos before = g_player.body.m_pos;
	MoveWithCollision(&g_player.body.m_pos, Balance::BODY_RADIUS, velocity_x * delta_seconds, velocity_y * delta_seconds);

	const float moved_x = GetTileDistanceX(before, g_player.body.m_pos);
	const float moved_y = GetTileDistanceY(before, g_player.body.m_pos);
	const float moved = std::sqrt(moved_x * moved_x + moved_y * moved_y);

	// 벽에 막혀 제자리면 걷는 모션도 멈춘다
	if (moved > 0.0001f)
	{
		g_player.walk_distance += moved;
		g_player.idle_timer = 0.f;
	}
}

void AimPlayer(float world_dir_x, float world_dir_y)
{
	const float length = std::sqrt(world_dir_x * world_dir_x + world_dir_y * world_dir_y);

	// 커서가 발밑이면 방향이 안 나온다. 직전 방향 유지
	if (length < 0.001f)
	{
		return;
	}

	g_player.facing_x = world_dir_x / length;
	g_player.facing_y = world_dir_y / length;
}

bool TryStartSwing()
{
	if (IsPlayerDead())
	{
		return false;
	}

	if (g_player.swing_cooldown > 0.f)
	{
		return false;
	}

	g_player.swing_cooldown = Balance::SWING_INTERVAL_SEC;
	g_player.swing_timer = Balance::SWING_SHOW_SEC;

	return true;
}

void DamagePlayer(int32 damage)
{
	if (IsPlayerDead())
	{
		return;
	}

	// 하한 1 ( 방어력으로 완전 무적이 되면 몹이 무의미해진다 )
	int32 taken = damage - GetPlayerDefence();
	if (taken < 1)
	{
		taken = 1;
	}

	g_player.hp -= taken;
	g_player.hit_flash = Balance::HIT_FLASH_SEC;
	g_player.since_hit = 0.f;

	AddFloatingText(g_player.body.m_pos, 44.f, L"-" + std::to_wstring(taken), 1.f, 0.35f, 0.28f);

	if (g_player.hp <= 0)
	{
		g_player.hp = 0;
		g_player.respawn_timer = Balance::PLAYER_RESPAWN_SEC;

		std::cout << "[ Player - DamagePlayer ] : player died\n";
	}
}

int32 GetPlayerMaxHp()
{
	return Balance::PLAYER_BASE_HP + g_player.stats[static_cast<size_t>(STAT_MAX_HP)] * Balance::PLAYER_HP_PER_POINT;
}

int32 GetPlayerAttack()
{
	const int32 stat_attack = g_player.stats[static_cast<size_t>(STAT_ATTACK)] * Balance::PLAYER_ATTACK_PER_POINT;

	return Balance::PLAYER_BASE_ATTACK + stat_attack + GetEnhanceAttackBonus(g_player.enhance_level);
}

int32 GetPlayerDefence()
{
	return g_player.stats[static_cast<size_t>(STAT_DEFENCE)] * Balance::PLAYER_DEFENCE_PER_POINT;
}

int32 GetNeedExp(int32 level)
{
	return 30 + (level - 1) * 20;
}

void AddPlayerExp(int32 exp)
{
	g_player.exp += exp;

	// 보스 경험치 한 번에 여러 레벨이 오를 수 있다
	while (g_player.exp >= GetNeedExp(g_player.level))
	{
		g_player.exp -= GetNeedExp(g_player.level);
		g_player.level += 1;
		g_player.stat_point += Balance::LEVEL_UP_STAT_POINT;
		g_player.hp = GetPlayerMaxHp();

		AddFloatingText(g_player.body.m_pos, 56.f, L"레벨 업!", 1.f, 0.85f, 0.30f);
		std::cout << "[ Player - AddPlayerExp ] : level up " << g_player.level << "\n";
	}
}

bool TryStatUp(int32 stat)
{
	if (g_player.stat_point <= 0)
	{
		return false;
	}

	if (stat < 0 || stat >= STAT_COUNT)
	{
		return false;
	}

	// 현재 hp 는 안 따라 올린다 ( 서버와 같은 규칙 )
	g_player.stats[static_cast<size_t>(stat)] += 1;
	g_player.stat_point -= 1;

	return true;
}

const wchar_t* GetMaterialName(int32 material)
{
	if (material == MATERIAL_CORE)
	{
		return L"별핵";
	}

	if (material == MATERIAL_FRAGMENT)
	{
		return L"별조각";
	}

	return L"별가루";
}

const wchar_t* GetStatName(int32 stat)
{
	if (stat == STAT_MAX_HP)
	{
		return L"체력";
	}

	if (stat == STAT_ATTACK)
	{
		return L"공격력";
	}

	return L"방어력";
}
