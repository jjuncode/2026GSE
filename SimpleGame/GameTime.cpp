#include "stdafx.h"
#include "GameTime.h"

#include <chrono>

namespace
{
	// 밀리초 정수로 재면 초당 60프레임에서 16과 17을 오가고 그 흔들림이 속도에 그대로 실린다
	std::chrono::steady_clock::time_point g_last_time_point;
	bool g_clock_started = false;
}

void ResetFrameClock()
{
	g_last_time_point = std::chrono::steady_clock::now();
	g_clock_started = true;
}

float PopElapsedSeconds()
{
	if (g_clock_started == false)
	{
		ResetFrameClock();
		return 0.f;
	}

	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsed = now - g_last_time_point;

	g_last_time_point = now;

	float seconds = elapsed.count();
	if (seconds > MAX_FRAME_SECONDS)
	{
		seconds = MAX_FRAME_SECONDS;
	}

	return seconds;
}
