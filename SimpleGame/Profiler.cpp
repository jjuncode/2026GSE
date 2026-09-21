#include "stdafx.h"
#include "Profiler.h"

#include <chrono>
#include <iomanip>
#include <iostream>

namespace
{
	constexpr float REPORT_INTERVAL_SEC = 1.f;

	int32 g_draw_call_count = 0;	// 이번 프레임
	int64 g_draw_call_sum = 0;	// 보고 구간 누적
	int32 g_frame_count = 0;
	float g_elapsed_sum = 0.f;
	float g_worst_frame_sec = 0.f;

	std::chrono::steady_clock::time_point g_last_time_point;
	bool g_started = false;

	// 정밀도를 건드린 채로 두면 다른 로그까지 따라 바뀐다
	void PrintReport(float fps, float average_draw, float average_ms, float worst_ms)
	{
		const std::ios::fmtflags saved_flags = std::cout.flags();
		const std::streamsize saved_precision = std::cout.precision();

		std::cout << std::fixed << std::setprecision(1);
		std::cout << "[ Profiler - EndProfileFrame ] : fps " << fps << " draw " << average_draw << " frame " << average_ms << "ms max " << worst_ms << "ms\n";

		std::cout.flags(saved_flags);
		std::cout.precision(saved_precision);
	}
}

void CountDrawCall()
{
	g_draw_call_count += 1;
}

void EndProfileFrame()
{
	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	// 첫 프레임은 기준 시각이 없어 길이를 못 잰다
	if (g_started == false)
	{
		g_started = true;
		g_last_time_point = now;
		g_draw_call_count = 0;

		return;
	}

	const std::chrono::duration<float> elapsed = now - g_last_time_point;
	g_last_time_point = now;

	const float frame_seconds = elapsed.count();

	g_draw_call_sum += g_draw_call_count;
	g_draw_call_count = 0;
	g_frame_count += 1;
	g_elapsed_sum += frame_seconds;

	// 평균만 보면 가끔 튀는 프레임이 묻힌다
	if (frame_seconds > g_worst_frame_sec)
	{
		g_worst_frame_sec = frame_seconds;
	}

	if (g_elapsed_sum < REPORT_INTERVAL_SEC)
	{
		return;
	}

	const float frame_count = static_cast<float>(g_frame_count);
	const float fps = frame_count / g_elapsed_sum;
	const float average_draw = static_cast<float>(g_draw_call_sum) / frame_count;
	const float average_ms = g_elapsed_sum / frame_count * 1000.f;
	const float worst_ms = g_worst_frame_sec * 1000.f;

	PrintReport(fps, average_draw, average_ms, worst_ms);

	g_draw_call_sum = 0;
	g_frame_count = 0;
	g_elapsed_sum = 0.f;
	g_worst_frame_sec = 0.f;
}
