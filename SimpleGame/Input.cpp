#include "stdafx.h"
#include "Input.h"

namespace
{
	const int KEY_TABLE_SIZE = 256;

	bool g_key_down[KEY_TABLE_SIZE] = { false };
	bool g_special_key_down[KEY_TABLE_SIZE] = { false };

	bool IsIndexValid(int index)
	{
		if (index < 0)
		{
			return false;
		}

		if (index >= KEY_TABLE_SIZE)
		{
			return false;
		}

		return true;
	}

	// 대소문자를 갈라 두면 쉬프트를 누른 순간 이동이 끊긴다
	unsigned char ToLowerKey(unsigned char key)
	{
		if (key >= 'A' && key <= 'Z')
		{
			return key + ('a' - 'A');
		}

		return key;
	}
}

void SetKeyDown(unsigned char key, bool down)
{
	g_key_down[ToLowerKey(key)] = down;
}

void SetSpecialKeyDown(int key, bool down)
{
	if (!IsIndexValid(key))
	{
		return;
	}

	g_special_key_down[key] = down;
}

void ClearAllKeys()
{
	for (int index = 0; index < KEY_TABLE_SIZE; ++index)
	{
		g_key_down[index] = false;
		g_special_key_down[index] = false;
	}
}

bool IsKeyDown(unsigned char key)
{
	return g_key_down[ToLowerKey(key)];
}

bool IsSpecialKeyDown(int key)
{
	if (!IsIndexValid(key))
	{
		return false;
	}

	return g_special_key_down[key];
}
