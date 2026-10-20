#include "stdafx.h"
#include "Feel.h"
#include "Audio.h"
#include "Combat.h"
#include "FloatingText.h"
#include "Player.h"
#include "GameConfig.h"
#include "Renderer.h"
#include "SpriteAtlas.h"
#include "SpriteSheet.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace
{
	const float PI = 3.14159265f;

	const float SOUND_FULL_RANGE = 7.f;	// 칸. 이 안은 제 소리
	const float SOUND_CUT_RANGE = 15.f;	// 이 밖은 안 낸다

	const float SHAKE_DECAY_PER_SEC = 2.4f;
	const float SHAKE_MAX_PIXEL = 16.f;
	const float ZOOM_DECAY_PER_SEC = 9.f;
	const float FLASH_DECAY_PER_SEC = 5.f;

	constexpr size_t MAX_SPARK_COUNT = 600;
	constexpr size_t MAX_VFX_COUNT = 96;

	// 좌클릭 연타 단계마다 멈춤을 키운다 ( 클라 StepStopDurations · StepTimeScales )
	const std::array<float, 4> COMBO_STOP_SEC = { 0.060f, 0.075f, 0.090f, 0.120f };
	const std::array<float, 4> COMBO_STOP_SCALE = { 0.06f, 0.045f, 0.03f, 0.015f };
	const std::array<float, 4> COMBO_SHAKE = { 0.20f, 0.24f, 0.28f, 0.40f };

	constexpr int32 SPARK_KIND_SPARK = 0;
	constexpr int32 SPARK_KIND_STREAK = 1;
	constexpr int32 SPARK_KIND_DUST = 2;
	constexpr int32 SPARK_KIND_GLINT = 3;

	struct Spark
	{
		WorldPos anchor;
		float lift = 0.f;
		float offset_x = 0.f;	// 화면 픽셀
		float offset_y = 0.f;
		float velocity_x = 0.f;
		float velocity_y = 0.f;
		float angle = 0.f;
		float size = 1.f;
		float age = 0.f;
		float life = 0.3f;
		float gravity = 0.f;
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
		int32 kind = SPARK_KIND_SPARK;
	};

	struct Ring
	{
		WorldPos pos;
		float radius = 1.f;
		float age = 0.f;
		float life = 0.3f;
		float r = 1.f;
		float g = 1.f;
		float b = 1.f;
	};

	struct VfxPlay
	{
		int32 vfx = VFX_HIT;
		WorldPos pos;
		float lift = 0.f;
		float angle = 0.f;
		float brightness = 1.f;
		float age = 0.f;
	};

	std::vector<Spark> g_sparks;
	std::vector<Ring> g_rings;
	std::vector<VfxPlay> g_vfx_plays;

	float g_stop_timer = 0.f;
	float g_stop_scale = 1.f;
	float g_slow_timer = 0.f;
	float g_slow_scale = 1.f;

	float g_trauma = 0.f;
	float g_shake_time = 0.f;
	float g_zoom = 0.f;
	float g_flash_r = 0.f;
	float g_flash_g = 0.f;
	float g_flash_b = 0.f;
	float g_flash_strength = 0.f;

	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x27d4eb2fu);
		return random;
	}

	float RollRange(float min_value, float max_value)
	{
		std::uniform_real_distribution<float> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	int32 GetComboStep(int32 skill_id)
	{
		const std::vector<int32>& combo = GetComboSkillIds();

		for (size_t index = 0; index < combo.size(); ++index)
		{
			if (combo[index] == skill_id)
			{
				return static_cast<int32>(index);
			}
		}

		return -1;
	}

	float GetChestLift(const Creature& creature)
	{
		if (creature.m_sheet != SHEET_NONE)
		{
			return GetSheetBodyHeight(creature.m_sheet) * static_cast<float>(PIXEL_SCALE) * 0.55f;
		}

		return static_cast<float>(GetSpriteHeight(creature.m_sprite) * PIXEL_SCALE) * 0.55f;
	}

	// 바닥에 눕는 효과는 캐릭터보다 먼저 그린다
	bool IsGroundVfx(int32 vfx)
	{
		return vfx == VFX_SLAM;
	}

	// 땅을 내려치는 공격. 마지막 창만 내려치는 연타도 있다
	bool IsGroundImpact(const SkillData& skill, int32 window_index)
	{
		const int32 last_window = static_cast<int32>(skill.hit_windows.size()) - 1;

		switch (skill.skill_id)
		{
			case 21:
			case 32:
			case 108:
			{
				return true;
			}
			case 31:
			{
				return window_index == last_window;
			}
			default:
			{
				return false;
			}
		}
	}

	void DrawVfxPlays(Renderer* renderer, const WorldPos& camera, bool ground)
	{
		for (const VfxPlay& play : g_vfx_plays)
		{
			if (IsGroundVfx(play.vfx) != ground)
			{
				continue;
			}

			const float frame_sec = std::max(GetVfxFrameSec(play.vfx), 0.001f);
			const int32 index = static_cast<int32>(play.age / frame_sec);
			const ScreenPos center = GetLiftedScreen(play.pos, play.lift, camera);

			DrawVfxFrame(renderer, play.vfx, index, center.x, center.y, play.angle, 1.f, play.brightness, play.brightness, play.brightness, 1.f);
		}
	}

	// 맞은 자리. 피격자 몸에서 공격자 쪽으로 반경만큼 나온 점
	WorldPos GetContactPos(const HitInfo& hit, const Creature& victim)
	{
		float to_x = GetTileDistanceX(victim.m_pos, hit.origin);
		float to_y = GetTileDistanceY(victim.m_pos, hit.origin);
		const float length = std::sqrt(to_x * to_x + to_y * to_y);

		WorldPos contact = victim.m_pos;
		if (length > 0.001f)
		{
			MoveWorldPos(&contact, to_x / length * victim.m_hit_radius, to_y / length * victim.m_hit_radius);
		}

		return contact;
	}

	// 공격자에서 피격자로 가는 화면 방향
	void GetScreenDir(const WorldPos& from, const WorldPos& to, float* dir_x, float* dir_y)
	{
		const WorldPos origin;
		const ScreenPos a = WorldToScreen(from, origin);
		const ScreenPos b = WorldToScreen(to, origin);

		float x = b.x - a.x;
		float y = b.y - a.y;
		const float length = std::sqrt(x * x + y * y);

		if (length < 0.001f)
		{
			*dir_x = 1.f;
			*dir_y = 0.f;
			return;
		}

		*dir_x = x / length;
		*dir_y = y / length;
	}

	void PushSpark(const Spark& spark)
	{
		if (g_sparks.size() >= MAX_SPARK_COUNT)
		{
			g_sparks.erase(g_sparks.begin());
		}

		g_sparks.push_back(spark);
	}

	void ShowHitOnMonster(const HitInfo& hit, Creature* victim, int32 taken)
	{
		const SkillData& skill = *hit.skill;
		const int32 step = GetComboStep(skill.skill_id);

		float stop_sec = skill.hit_stop_sec * 1.5f + 0.04f;
		float stop_scale = 0.05f;
		float shake = 0.24f;
		float zoom = 0.012f;

		if (step >= 0 && step < static_cast<int32>(COMBO_STOP_SEC.size()))
		{
			stop_sec = COMBO_STOP_SEC[static_cast<size_t>(step)];
			stop_scale = COMBO_STOP_SCALE[static_cast<size_t>(step)];
			shake = COMBO_SHAKE[static_cast<size_t>(step)];
			zoom = 0.008f + 0.006f * static_cast<float>(step);
		}

		if (skill.skill_type == SKILL_TYPE_PARRY_ATTACK)
		{
			stop_sec = 0.16f;
			stop_scale = 0.02f;
			shake = 0.5f;
			zoom = 0.04f;
		}

		if (hit.is_critical)
		{
			stop_sec += 0.025f;
			shake += 0.12f;
			zoom += 0.012f;
		}

		if (hit.is_weak_point)
		{
			stop_sec += 0.03f;
			shake += 0.1f;
		}

		AddHitStop(stop_sec, stop_scale);
		AddShake(shake);
		AddZoomPunch(zoom);

		const WorldPos contact = GetContactPos(hit, *victim);
		const float lift = GetChestLift(*victim);

		float dir_x = 1.f;
		float dir_y = 0.f;
		GetScreenDir(hit.origin, victim->m_pos, &dir_x, &dir_y);

		// 불꽃은 맞은 방향으로 튄다. 치명타는 더 하얗고 많다
		int32 count = 9;
		float spark_r = 2.6f;
		float spark_g = 1.7f;
		float spark_b = 0.8f;

		if (hit.is_critical)
		{
			count = 18;
			spark_r = 3.2f;
			spark_g = 2.6f;
			spark_b = 1.6f;
		}

		SpawnSparks(contact, lift, dir_x, dir_y, count, 260.f, 0.9f, spark_r, spark_g, spark_b, 0.28f);
		SpawnSparks(contact, lift, dir_x, dir_y, 6, 120.f, 1.6f, 0.9f, 0.25f, 0.2f, 0.45f);

		// 터지는 불꽃 + 칼이 지나간 결을 가로지르는 빛줄기
		if (hit.is_critical || step == 3 || skill.skill_type == SKILL_TYPE_PARRY_ATTACK)
		{
			SpawnVfx(VFX_CRIT, contact, lift, 0.f, 1.6f);
		}
		else
		{
			SpawnVfx(VFX_HIT, contact, lift, 0.f, 1.5f);
		}

		const float streak_angle = std::atan2(dir_y, dir_x) + PI * 0.5f + RollRange(-0.35f, 0.35f);
		SpawnVfx(VFX_CUT, contact, lift, streak_angle, 1.8f);

		if (step == 3 || skill.skill_type == SKILL_TYPE_PARRY_ATTACK || skill.stun_sec > 0.f)
		{
			SpawnRing(victim->m_pos, 1.3f, 1.4f, 0.9f, 0.5f, 0.28f);
		}

		std::wstring text = std::to_wstring(taken);
		int32 style = DAMAGE_STYLE_NORMAL;

		if (hit.is_weak_point)
		{
			text = L"급소 " + text;
			style = DAMAGE_STYLE_WEAK;
		}
		else if (hit.is_critical)
		{
			style = DAMAGE_STYLE_CRIT;
		}

		AddDamageText(victim->m_pos, lift + 14.f, text, style);

		if (hit.is_critical || hit.is_weak_point)
		{
			PlaySfx(SFX_HIT_CRIT, 0.9f);
		}
		else if (step == 3 || skill.skill_type == SKILL_TYPE_PARRY_ATTACK)
		{
			PlaySfx(SFX_HIT_HEAVY, 0.9f);
		}
		else
		{
			PlaySfx(SFX_HIT_FLESH, 0.8f);
		}

		if (skill.skill_type == SKILL_TYPE_PARRY_ATTACK)
		{
			PlaySfx(SFX_COUNTER, 0.85f);
		}
	}

	void ShowHitOnPlayer(const HitInfo& hit, Creature* victim, int32 taken)
	{
		AddHitStop(0.07f, 0.1f);
		AddShake(0.45f);
		AddScreenFlash(0.9f, 0.1f, 0.05f, 0.16f);

		const WorldPos contact = GetContactPos(hit, *victim);
		const float lift = GetChestLift(*victim);

		float dir_x = 1.f;
		float dir_y = 0.f;
		GetScreenDir(hit.origin, victim->m_pos, &dir_x, &dir_y);

		SpawnSparks(contact, lift, dir_x, dir_y, 10, 200.f, 1.1f, 1.6f, 0.2f, 0.12f, 0.35f);
		SpawnVfx(VFX_HURT, contact, lift, 0.f, 1.3f);

		AddDamageText(victim->m_pos, lift + 14.f, L"-" + std::to_wstring(taken), DAMAGE_STYLE_PLAYER);
		PlaySfx(SFX_PLAYER_HURT, 0.9f);
	}

	void ShowGuard(const HitInfo& hit, Creature* victim, int32 taken, bool is_break)
	{
		const WorldPos contact = GetContactPos(hit, *victim);
		const float lift = GetChestLift(*victim);

		float dir_x = 1.f;
		float dir_y = 0.f;
		GetScreenDir(victim->m_pos, hit.origin, &dir_x, &dir_y);

		if (is_break)
		{
			AddHitStop(0.12f, 0.03f);
			AddShake(0.7f);
			AddScreenFlash(1.f, 0.55f, 0.15f, 0.35f);
			SpawnSparks(contact, lift, dir_x, dir_y, 26, 300.f, 2.2f, 3.f, 1.8f, 0.6f, 0.45f);
			SpawnRing(victim->m_pos, 1.5f, 1.5f, 0.8f, 0.3f, 0.35f);
			SpawnVfx(VFX_CRIT, contact, lift, 0.f, 1.6f);
			AddDamageText(victim->m_pos, lift + 26.f, L"가드 브레이크", DAMAGE_STYLE_BANNER_BAD);
			PlaySfx(SFX_GUARD_BREAK, 1.f);
			return;
		}

		AddHitStop(0.05f, 0.05f);
		AddShake(0.22f);

		// 쇳소리 불꽃. 막은 쪽에서 공격자 쪽으로 튄다
		SpawnSparks(contact, lift, dir_x, dir_y, 14, 280.f, 1.3f, 2.6f, 2.6f, 3.2f, 0.25f);
		SpawnVfx(VFX_GUARD, contact, lift, 0.f, 1.6f);

		AddDamageText(victim->m_pos, lift + 14.f, L"막음 -" + std::to_wstring(taken), DAMAGE_STYLE_GUARD);
		PlaySfx(SFX_GUARD, 0.9f);
	}

	void ShowParry(const HitInfo& hit, Creature* victim)
	{
		// 세상을 한 박자 완전히 세웠다가 잠깐 느리게 푼다. 이 정지가 패링의 손맛이다
		AddHitStop(0.13f, 0.f);
		AddSlowMotion(0.22f, 0.35f);
		AddShake(0.55f);
		AddZoomPunch(0.06f);
		AddScreenFlash(1.f, 0.92f, 0.75f, 0.24f);

		const WorldPos contact = GetContactPos(hit, *victim);
		const float lift = GetChestLift(*victim);

		float dir_x = 1.f;
		float dir_y = 0.f;
		GetScreenDir(victim->m_pos, hit.origin, &dir_x, &dir_y);

		SpawnSparks(contact, lift, dir_x, dir_y, 34, 360.f, 2.4f, 3.6f, 3.2f, 2.2f, 0.4f);
		SpawnSparks(contact, lift, -dir_x, -dir_y, 12, 180.f, 1.4f, 2.8f, 2.4f, 1.4f, 0.3f);
		SpawnStreak(contact, lift, std::atan2(dir_y, dir_x) + PI * 0.5f, 70.f, 4.f, 3.8f, 3.2f, 0.16f);
		SpawnStreak(contact, lift, std::atan2(dir_y, dir_x), 44.f, 3.f, 2.8f, 2.4f, 0.12f);
		SpawnGlint(contact, lift, 4.f, 3.8f, 3.2f, 1.6f);
		SpawnRing(contact, 1.8f, 2.4f, 2.2f, 1.6f, 0.32f);
		SpawnVfx(VFX_PARRY, contact, lift, 0.f, 2.f);

		AddDamageText(victim->m_pos, GetChestLift(*victim) + 30.f, L"패링!", DAMAGE_STYLE_BANNER_GOOD);
		PlaySfx(SFX_PARRY, 1.f);
	}

	float Approach(float value, float target, float delta)
	{
		if (value > target)
		{
			return std::max(target, value - delta);
		}

		return std::min(target, value + delta);
	}

	ColorVertex MakeVertex(float x, float y, float r, float g, float b, float a)
	{
		ColorVertex vertex;
		vertex.x = x;
		vertex.y = y;
		vertex.r = r;
		vertex.g = g;
		vertex.b = b;
		vertex.a = a;

		return vertex;
	}

	// 가운데에서 두 방향으로 뻗는 마름모 줄. 앞뒤 끝이 뾰족해야 빛줄기로 읽힌다
	void DrawNeedle(Renderer* renderer, float x, float y, float angle, float length, float width, float r, float g, float b, float a)
	{
		const float axis_x = std::cos(angle) * length * 0.5f;
		const float axis_y = std::sin(angle) * length * 0.5f;
		const float side_x = -std::sin(angle) * width * 0.5f;
		const float side_y = std::cos(angle) * width * 0.5f;

		const ColorVertex head = MakeVertex(x + axis_x, y + axis_y, r, g, b, a);
		const ColorVertex tail = MakeVertex(x - axis_x, y - axis_y, r, g, b, a);
		const ColorVertex left = MakeVertex(x + side_x, y + side_y, r, g, b, a);
		const ColorVertex right = MakeVertex(x - side_x, y - side_y, r, g, b, a);

		renderer->DrawQuad(head, left, tail, right);
	}
}

void ClearFeel()
{
	g_sparks.clear();
	g_rings.clear();
	g_vfx_plays.clear();
	g_stop_timer = 0.f;
	g_slow_timer = 0.f;
	g_trauma = 0.f;
	g_zoom = 0.f;
	g_flash_strength = 0.f;
}

void UpdateFeel(float real_seconds)
{
	if (g_stop_timer > 0.f)
	{
		g_stop_timer -= real_seconds;
	}
	else if (g_slow_timer > 0.f)
	{
		g_slow_timer -= real_seconds;
	}

	g_trauma = Approach(g_trauma, 0.f, SHAKE_DECAY_PER_SEC * real_seconds);
	g_shake_time += real_seconds;
	g_zoom = g_zoom * std::exp(-ZOOM_DECAY_PER_SEC * real_seconds);
	g_flash_strength = Approach(g_flash_strength, 0.f, FLASH_DECAY_PER_SEC * real_seconds * std::max(g_flash_strength, 0.2f));

	for (Spark& spark : g_sparks)
	{
		spark.age += real_seconds;

		// 불꽃은 빨리 느려지고 아래로 떨어진다
		const float drag = std::exp(-5.f * real_seconds);
		spark.velocity_x *= drag;
		spark.velocity_y = spark.velocity_y * drag - spark.gravity * real_seconds;
		spark.offset_x += spark.velocity_x * real_seconds;
		spark.offset_y += spark.velocity_y * real_seconds;
	}

	g_sparks.erase(std::remove_if(g_sparks.begin(), g_sparks.end(),
		[](const Spark& spark)
		{
			return spark.age >= spark.life;
		}), g_sparks.end());

	for (Ring& ring : g_rings)
	{
		ring.age += real_seconds;
	}

	g_rings.erase(std::remove_if(g_rings.begin(), g_rings.end(),
		[](const Ring& ring)
		{
			return ring.age >= ring.life;
		}), g_rings.end());

	for (VfxPlay& play : g_vfx_plays)
	{
		play.age += real_seconds;
	}

	g_vfx_plays.erase(std::remove_if(g_vfx_plays.begin(), g_vfx_plays.end(),
		[](const VfxPlay& play)
		{
			return play.age >= GetVfxFrameSec(play.vfx) * static_cast<float>(GetVfxFrameCount(play.vfx));
		}), g_vfx_plays.end());
}

float GetWorldTimeScale()
{
	if (g_stop_timer > 0.f)
	{
		return g_stop_scale;
	}

	if (g_slow_timer > 0.f)
	{
		return g_slow_scale;
	}

	return 1.f;
}

void AddHitStop(float seconds, float time_scale)
{
	// 겹치면 긴 쪽, 더 멈춘 쪽을 따른다. 더하면 다단히트에서 세상이 굳는다
	if (g_stop_timer <= 0.f)
	{
		g_stop_scale = time_scale;
	}
	else
	{
		g_stop_scale = std::min(g_stop_scale, time_scale);
	}

	g_stop_timer = std::max(g_stop_timer, seconds);
}

void AddSlowMotion(float seconds, float time_scale)
{
	g_slow_timer = std::max(g_slow_timer, seconds);
	g_slow_scale = time_scale;
}

void AddShake(float trauma)
{
	g_trauma = std::min(1.f, g_trauma + trauma);
}

void AddZoomPunch(float amount)
{
	g_zoom = std::max(g_zoom, amount);
}

void AddScreenFlash(float r, float g, float b, float strength)
{
	if (strength < g_flash_strength)
	{
		return;
	}

	g_flash_r = r;
	g_flash_g = g;
	g_flash_b = b;
	g_flash_strength = strength;
}

ScreenPos GetShakeOffset()
{
	// 제곱해야 약한 흔들림은 거의 안 보이고 센 것만 확 흔들린다
	const float amount = g_trauma * g_trauma * SHAKE_MAX_PIXEL;

	ScreenPos offset;
	offset.x = amount * (std::sin(g_shake_time * 71.f) * 0.6f + std::sin(g_shake_time * 113.f + 1.3f) * 0.4f);
	offset.y = amount * (std::sin(g_shake_time * 89.f + 0.7f) * 0.6f + std::sin(g_shake_time * 131.f + 2.1f) * 0.4f);

	return offset;
}

float GetZoomPunch()
{
	return 1.f + g_zoom;
}

void GetScreenFlash(float* r, float* g, float* b, float* strength)
{
	*r = g_flash_r;
	*g = g_flash_g;
	*b = g_flash_b;
	*strength = g_flash_strength;
}

void NotifyCombatHit(const HitInfo& hit, Creature* victim, int32 result, int32 taken)
{
	if (hit.skill == nullptr || victim == nullptr)
	{
		return;
	}

	switch (result)
	{
		case HIT_RESULT_PARRY:
		{
			ShowParry(hit, victim);
			break;
		}
		case HIT_RESULT_GUARD:
		{
			ShowGuard(hit, victim, taken, false);
			break;
		}
		case HIT_RESULT_GUARD_BREAK:
		{
			ShowGuard(hit, victim, taken, true);
			break;
		}
		default:
		{
			if (victim->m_team == TEAM_PLAYER)
			{
				ShowHitOnPlayer(hit, victim, taken);
			}
			else
			{
				ShowHitOnMonster(hit, victim, taken);
			}

			break;
		}
	}
}

void SpawnSparks(const WorldPos& pos, float lift, float dir_x, float dir_y, int32 count, float speed, float spread, float r, float g, float b, float life)
{
	const float base_angle = std::atan2(dir_y, dir_x);

	for (int32 index = 0; index < count; ++index)
	{
		const float angle = base_angle + RollRange(-spread * 0.5f, spread * 0.5f);
		const float spark_speed = speed * RollRange(0.35f, 1.f);

		Spark spark;
		spark.anchor = pos;
		spark.lift = lift;
		spark.velocity_x = std::cos(angle) * spark_speed;
		spark.velocity_y = std::sin(angle) * spark_speed + RollRange(0.f, 60.f);
		spark.size = RollRange(1.f, 2.f);
		spark.life = life * RollRange(0.6f, 1.f);
		spark.gravity = 420.f;
		spark.r = r;
		spark.g = g;
		spark.b = b;
		spark.kind = SPARK_KIND_SPARK;

		PushSpark(spark);
	}
}

void SpawnStreak(const WorldPos& pos, float lift, float angle, float length, float r, float g, float b, float life)
{
	Spark spark;
	spark.anchor = pos;
	spark.lift = lift;
	spark.angle = angle;
	spark.size = length;
	spark.life = life;
	spark.r = r;
	spark.g = g;
	spark.b = b;
	spark.kind = SPARK_KIND_STREAK;

	PushSpark(spark);
}

void SpawnRing(const WorldPos& pos, float radius, float r, float g, float b, float life)
{
	Ring ring;
	ring.pos = pos;
	ring.radius = radius;
	ring.life = life;
	ring.r = r;
	ring.g = g;
	ring.b = b;

	g_rings.push_back(ring);
}

void SpawnDust(const WorldPos& pos, int32 count, float r, float g, float b)
{
	for (int32 index = 0; index < count; ++index)
	{
		const float angle = RollRange(0.f, PI * 2.f);
		const float speed = RollRange(20.f, 70.f);

		Spark spark;
		spark.anchor = pos;
		spark.lift = 2.f;
		spark.velocity_x = std::cos(angle) * speed;
		spark.velocity_y = std::sin(angle) * speed * 0.5f + 20.f;
		spark.size = RollRange(2.f, 4.f);
		spark.life = RollRange(0.3f, 0.5f);
		spark.gravity = -10.f;
		spark.r = r;
		spark.g = g;
		spark.b = b;
		spark.kind = SPARK_KIND_DUST;

		PushSpark(spark);
	}
}

void SpawnGlint(const WorldPos& pos, float lift, float r, float g, float b, float size)
{
	Spark spark;
	spark.anchor = pos;
	spark.lift = lift;
	spark.size = size;
	spark.life = 0.22f;
	spark.r = r;
	spark.g = g;
	spark.b = b;
	spark.kind = SPARK_KIND_GLINT;

	PushSpark(spark);
}

void SpawnVfx(int32 vfx, const WorldPos& pos, float lift, float angle, float brightness)
{
	if (GetVfxFrameCount(vfx) <= 0)
	{
		return;
	}

	if (g_vfx_plays.size() >= MAX_VFX_COUNT)
	{
		g_vfx_plays.erase(g_vfx_plays.begin());
	}

	VfxPlay play;
	play.vfx = vfx;
	play.pos = pos;
	play.lift = lift;
	play.angle = angle;
	play.brightness = brightness;

	g_vfx_plays.push_back(play);
}

void NotifyWindowOpen(const Creature& attacker, int32 window_index)
{
	const SkillData* skill = attacker.m_cast_skill;
	if (skill == nullptr || IsGroundImpact(*skill, window_index) == false)
	{
		return;
	}

	// 사방 공격은 제자리, 앞으로 내려치면 사거리 절반쯤 앞 바닥
	WorldPos impact = attacker.m_pos;
	const float sweep = skill->angle_left + skill->angle_right;
	if (sweep < 300.f)
	{
		MoveWorldPos(&impact, attacker.m_face_x * skill->range * 0.55f, attacker.m_face_y * skill->range * 0.55f);
	}

	SpawnVfx(VFX_SLAM, impact, 0.f, 0.f, 1.2f);
	SpawnVfx(VFX_DUST, impact, 6.f, 0.f, 1.f);

	const float falloff = GetSoundFalloff(impact);
	if (falloff > 0.f)
	{
		AddShake(0.3f * falloff);
	}
}

float GetSoundFalloff(const WorldPos& pos)
{
	const WorldPos listener = GetPlayer().m_pos;
	const float delta_x = GetTileDistanceX(listener, pos);
	const float delta_y = GetTileDistanceY(listener, pos);
	const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);

	if (distance <= SOUND_FULL_RANGE)
	{
		return 1.f;
	}

	if (distance >= SOUND_CUT_RANGE)
	{
		return 0.f;
	}

	return 1.f - (distance - SOUND_FULL_RANGE) / (SOUND_CUT_RANGE - SOUND_FULL_RANGE);
}

void PlaySfxAt(int32 sfx, const WorldPos& pos, float volume)
{
	const float falloff = GetSoundFalloff(pos);
	if (falloff <= 0.f)
	{
		return;
	}

	PlaySfx(sfx, volume * falloff);
}

ScreenPos GetLiftedScreen(const WorldPos& pos, float lift, const WorldPos& camera)
{
	ScreenPos screen = WorldToScreen(pos, camera);
	screen.y += lift;

	return screen;
}

void DrawFeelGround(Renderer* renderer, const WorldPos& camera)
{
	constexpr int32 SEGMENT_COUNT = 32;

	DrawVfxPlays(renderer, camera, true);

	// 고리도 도트로 끊는다
	renderer->SetPixelTriangles(true);

	for (const Ring& ring : g_rings)
	{
		const float ratio = ring.age / ring.life;
		const float eased = 1.f - (1.f - ratio) * (1.f - ratio);
		const float outer = ring.radius * (0.35f + 0.65f * eased);
		const float inner = outer * (0.72f + 0.2f * ratio);
		const float alpha = (1.f - ratio) * 0.85f;

		const ScreenPos center = WorldToScreen(ring.pos, camera);

		// 바닥에 누운 원이라 세로를 반으로 누른다 ( 아이소 2 대 1 )
		const float unit_x = TILE_PIXEL_WIDTH * 0.5f * 1.4142f;
		const float unit_y = TILE_PIXEL_HEIGHT * 0.5f * 1.4142f;

		for (int32 segment = 0; segment < SEGMENT_COUNT; ++segment)
		{
			const float angle_now = PI * 2.f * static_cast<float>(segment) / static_cast<float>(SEGMENT_COUNT);
			const float angle_next = PI * 2.f * static_cast<float>(segment + 1) / static_cast<float>(SEGMENT_COUNT);

			const ColorVertex outer_now = MakeVertex(center.x + std::cos(angle_now) * outer * unit_x, center.y + std::sin(angle_now) * outer * unit_y, ring.r, ring.g, ring.b, alpha);
			const ColorVertex outer_next = MakeVertex(center.x + std::cos(angle_next) * outer * unit_x, center.y + std::sin(angle_next) * outer * unit_y, ring.r, ring.g, ring.b, alpha);
			const ColorVertex inner_now = MakeVertex(center.x + std::cos(angle_now) * inner * unit_x, center.y + std::sin(angle_now) * inner * unit_y, ring.r, ring.g, ring.b, 0.f);
			const ColorVertex inner_next = MakeVertex(center.x + std::cos(angle_next) * inner * unit_x, center.y + std::sin(angle_next) * inner * unit_y, ring.r, ring.g, ring.b, 0.f);

			renderer->DrawQuad(inner_now, outer_now, outer_next, inner_next);
		}
	}

	renderer->SetPixelTriangles(false);
}

void DrawFeelWorld(Renderer* renderer, const WorldPos& camera)
{
	const float unit = static_cast<float>(PIXEL_SCALE);

	renderer->SetPixelTriangles(true);

	for (const Spark& spark : g_sparks)
	{
		const ScreenPos base = GetLiftedScreen(spark.anchor, spark.lift, camera);
		const float x = base.x + spark.offset_x;
		const float y = base.y + spark.offset_y;
		const float ratio = spark.age / spark.life;
		const float fade = 1.f - ratio;

		switch (spark.kind)
		{
			case SPARK_KIND_STREAK:
			{
				// 처음에 길게 번쩍였다가 가늘어지며 사라진다
				const float width = unit * 2.5f * fade;
				DrawNeedle(renderer, x, y, spark.angle, spark.size * (0.7f + 0.3f * fade), width, spark.r, spark.g, spark.b, fade);
				break;
			}
			case SPARK_KIND_DUST:
			{
				const float size = spark.size * unit * (0.6f + ratio * 0.8f);
				renderer->DrawSolidBox(x, y, size, size, spark.r, spark.g, spark.b, fade * 0.45f);
				break;
			}
			case SPARK_KIND_GLINT:
			{
				// 십자 별. 커졌다 작아진다
				const float pulse = std::sin(ratio * PI);
				const float length = 30.f * spark.size * pulse;
				DrawNeedle(renderer, x, y, 0.f, length, unit * 2.f, spark.r, spark.g, spark.b, pulse);
				DrawNeedle(renderer, x, y, PI * 0.5f, length * 0.8f, unit * 2.f, spark.r, spark.g, spark.b, pulse);
				DrawNeedle(renderer, x, y, PI * 0.25f, length * 0.35f, unit * 1.5f, spark.r, spark.g, spark.b, pulse * 0.8f);
				DrawNeedle(renderer, x, y, -PI * 0.25f, length * 0.35f, unit * 1.5f, spark.r, spark.g, spark.b, pulse * 0.8f);
				break;
			}
			default:
			{
				// 날아가는 방향으로 늘린 불똥
				const float speed = std::sqrt(spark.velocity_x * spark.velocity_x + spark.velocity_y * spark.velocity_y);
				const float angle = std::atan2(spark.velocity_y, spark.velocity_x);
				const float length = unit * spark.size + speed * 0.025f;
				DrawNeedle(renderer, x, y, angle, length, unit * spark.size, spark.r, spark.g, spark.b, fade);
				break;
			}
		}
	}

	renderer->SetPixelTriangles(false);

	DrawVfxPlays(renderer, camera, false);
}
