#include "stdafx.h"
#include "Input.h"
#include "GameConfig.h"

#include <array>

namespace
{
	const int32 KEY_TABLE_SIZE = 256;

	std::array<bool, KEY_TABLE_SIZE> g_key_down = {};

	ScreenPos g_mouse_screen;
	bool g_attack_held = false;
	bool g_attack_queued = false;

	// 대소문자를 갈라 두면 쉬프트를 누른 순간 이동이 끊긴다
	uint8 ToLowerKey(uint8 key)
	{
		if (key >= 'A' && key <= 'Z')
		{
			return static_cast<uint8>(key + ('a' - 'A'));
		}

		return key;
	}
}

void SetKeyDown(uint8 key, bool down)
{
	g_key_down[static_cast<size_t>(ToLowerKey(key))] = down;
}

bool IsKeyDown(uint8 key)
{
	return g_key_down[static_cast<size_t>(ToLowerKey(key))];
}

void SetMouseWindowPos(int32 x, int32 y)
{
	g_mouse_screen.x = static_cast<float>(x) - static_cast<float>(WINDOW_SIZE_X) * 0.5f;
	g_mouse_screen.y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - static_cast<float>(y);
}

ScreenPos GetMouseScreenPos()
{
	return g_mouse_screen;
}

void SetAttackHeld(bool held)
{
	g_attack_held = held;
}

void QueueAttack()
{
	g_attack_queued = true;
}

bool ConsumeAttackRequest()
{
	if (g_attack_queued)
	{
		g_attack_queued = false;
		return true;
	}

	return g_attack_held;
}
