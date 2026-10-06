#include "stdafx.h"
#include "Profiler.h"

#include "Dependencies\glew.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	constexpr float REPORT_INTERVAL_SEC = 1.f;
	const char* LOG_DIRECTORY = "Logs";

	using Clock = std::chrono::steady_clock;

	// 한 프레임 몫. 보고 구간이 끝나면 합으로 접힌다
	struct FrameStats
	{
		std::array<int32, PROFILE_STAGE_COUNT> stage_draw_calls = {};
		std::array<int32, PROFILE_STAGE_COUNT> stage_quads = {};
		std::array<float, PROFILE_STAGE_COUNT> stage_seconds = {};
		std::array<int32, FLUSH_REASON_COUNT> flush_reasons = {};

		int32 draw_calls = 0;
		int32 quad_submits = 0;
		int32 batch_flushes = 0;
		int32 batch_instances = 0;
		int32 scene_actors = 0;
		int32 glyph_rasters = 0;
		float seconds = 0.f;
	};

	// 보고 구간 ( 기본 1 초 ) 누적
	struct WindowStats
	{
		std::array<int64, PROFILE_STAGE_COUNT> stage_draw_calls = {};
		std::array<int64, PROFILE_STAGE_COUNT> stage_quads = {};
		std::array<double, PROFILE_STAGE_COUNT> stage_seconds = {};
		std::array<int64, FLUSH_REASON_COUNT> flush_reasons = {};

		int64 draw_calls = 0;
		int64 quad_submits = 0;
		int64 batch_flushes = 0;
		int64 batch_instances = 0;
		int64 scene_actors = 0;
		int64 glyph_rasters = 0;

		int32 frame_count = 0;
		double elapsed = 0.0;
	};

	FrameStats g_frame;
	FrameStats g_worst_frame;
	WindowStats g_window;

	std::vector<float> g_frame_samples;

	int32 g_active_stage = -1;
	Clock::time_point g_stage_begin;

	Clock::time_point g_last_time_point;
	bool g_started = false;
	bool g_show_detail = false;
	double g_total_seconds = 0.0;

	std::ofstream g_csv;
	bool g_csv_ready = false;

	const char* GetStageName(int32 stage)
	{
		switch (stage)
		{
			case PROFILE_STAGE_UPDATE:
			{
				return "update";
			}
			case PROFILE_STAGE_GROUND:
			{
				return "ground";
			}
			case PROFILE_STAGE_SCENE:
			{
				return "scene";
			}
			case PROFILE_STAGE_FOG:
			{
				return "fog";
			}
			case PROFILE_STAGE_POST:
			{
				return "post";
			}
			case PROFILE_STAGE_OVERLAY:
			{
				return "overlay";
			}
			case PROFILE_STAGE_HUD:
			{
				return "hud";
			}
			default:
			{
				return "other";
			}
		}
	}

	const char* GetFlushReasonName(int32 reason)
	{
		switch (reason)
		{
			case FLUSH_REASON_STATE:
			{
				return "state";
			}
			case FLUSH_REASON_FULL:
			{
				return "full";
			}
			case FLUSH_REASON_LAYER:
			{
				return "layer";
			}
			default:
			{
				return "other";
			}
		}
	}

	std::string MakeTimeStamp()
	{
		const std::time_t now = std::time(nullptr);
		std::tm local = {};
		localtime_s(&local, &now);

		std::array<char, 32> buffer = {};
		std::strftime(buffer.data(), buffer.size(), "%Y%m%d_%H%M%S", &local);

		return std::string(buffer.data());
	}

	// 정렬된 표본에서 비율 자리를 집는다. 표본이 비면 0
	float GetPercentile(const std::vector<float>& sorted, float ratio)
	{
		if (sorted.empty())
		{
			return 0.f;
		}

		const size_t last = sorted.size() - 1;
		size_t index = static_cast<size_t>(ratio * static_cast<float>(last) + 0.5f);

		if (index > last)
		{
			index = last;
		}

		return sorted[index];
	}

	float ToAverage(int64 total, int32 frame_count)
	{
		if (frame_count <= 0)
		{
			return 0.f;
		}

		return static_cast<float>(total) / static_cast<float>(frame_count);
	}

	float ToAverageMs(double total_seconds, int32 frame_count)
	{
		if (frame_count <= 0)
		{
			return 0.f;
		}

		return static_cast<float>(total_seconds / static_cast<double>(frame_count) * 1000.0);
	}

	const char* ReadGlString(GLenum name)
	{
		const GLubyte* text = glGetString(name);

		if (text == nullptr)
		{
			return "unknown";
		}

		return reinterpret_cast<const char*>(text);
	}

	void WriteCsvHeader()
	{
		g_csv << "sec,fps,frame_avg_ms,frame_p50_ms,frame_p95_ms,frame_p99_ms,frame_max_ms,";
		g_csv << "draw_calls,batches,quads,instances,merge_ratio,";
		g_csv << "flush_state,flush_full,flush_layer,";

		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			g_csv << "draw_" << GetStageName(stage) << ",";
		}

		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			g_csv << "quad_" << GetStageName(stage) << ",";
		}

		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			g_csv << "ms_" << GetStageName(stage) << ",";
		}

		g_csv << "scene_actors,glyph_bakes\n";
		g_csv.flush();
	}

	// 정밀도를 건드린 채로 두면 다른 로그까지 따라 바뀐다
	void PrintReport(float fps, float average_draw, float average_batch, float average_quad, float merge, float average_ms, float p95_ms, float worst_ms)
	{
		const std::ios::fmtflags saved_flags = std::cout.flags();
		const std::streamsize saved_precision = std::cout.precision();

		std::cout << std::fixed << std::setprecision(1);
		std::cout << "[ Profiler - EndProfileFrame ] : fps " << fps;
		std::cout << " draw " << average_draw << " batch " << average_batch << " quad " << average_quad << " merge " << merge << "x";
		std::cout << " frame " << average_ms << "ms p95 " << p95_ms << "ms max " << worst_ms << "ms\n";

		std::cout.flags(saved_flags);
		std::cout.precision(saved_precision);
	}

	void PrintDetail(float worst_ms)
	{
		const std::ios::fmtflags saved_flags = std::cout.flags();
		const std::streamsize saved_precision = std::cout.precision();

		std::cout << std::fixed << std::setprecision(2);

		std::cout << "[ Profiler - PrintDetail ] : draw  ";
		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			std::cout << GetStageName(stage) << " " << ToAverage(g_window.stage_draw_calls[static_cast<size_t>(stage)], g_window.frame_count) << "  ";
		}
		std::cout << "\n";

		std::cout << "[ Profiler - PrintDetail ] : quad  ";
		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			std::cout << GetStageName(stage) << " " << ToAverage(g_window.stage_quads[static_cast<size_t>(stage)], g_window.frame_count) << "  ";
		}
		std::cout << "\n";

		std::cout << "[ Profiler - PrintDetail ] : cpu   ";
		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			std::cout << GetStageName(stage) << " " << ToAverageMs(g_window.stage_seconds[static_cast<size_t>(stage)], g_window.frame_count) << "ms  ";
		}
		std::cout << "\n";

		std::cout << "[ Profiler - PrintDetail ] : flush ";
		for (int32 reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
		{
			std::cout << GetFlushReasonName(reason) << " " << ToAverage(g_window.flush_reasons[static_cast<size_t>(reason)], g_window.frame_count) << "  ";
		}
		std::cout << "actor " << ToAverage(g_window.scene_actors, g_window.frame_count);
		std::cout << "  glyph_bake " << g_window.glyph_rasters << "\n";

		// 가장 느렸던 프레임은 평균에 묻힌다. 그 한 프레임이 어디서 샜는지 따로 남긴다
		std::cout << "[ Profiler - PrintDetail ] : worst " << worst_ms << "ms  ";
		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			std::cout << GetStageName(stage) << " " << g_worst_frame.stage_seconds[static_cast<size_t>(stage)] * 1000.f << "ms  ";
		}
		std::cout << "draw " << g_worst_frame.draw_calls << "\n";

		std::cout.flags(saved_flags);
		std::cout.precision(saved_precision);
	}

	void ResetWindow()
	{
		g_window = WindowStats();
		g_worst_frame = FrameStats();
		g_frame_samples.clear();
	}
}

bool InitProfiler()
{
	g_frame_samples.reserve(512);

	_mkdir(LOG_DIRECTORY);

	const std::string path = std::string(LOG_DIRECTORY) + "/perf_" + MakeTimeStamp() + ".csv";
	g_csv.open(path.c_str(), std::ios::trunc);

	if (g_csv.is_open() == false)
	{
		std::cout << "[ Profiler - InitProfiler ] : cannot open " << path << "\n";
		return false;
	}

	g_csv_ready = true;
	std::cout << "[ Profiler - InitProfiler ] : logging to " << path << "\n";

	return true;
}

void ShutdownProfiler()
{
	if (g_csv_ready)
	{
		g_csv.flush();
		g_csv.close();
		g_csv_ready = false;
	}
}

void LogGraphicsCaps()
{
	const char* vendor = ReadGlString(GL_VENDOR);
	const char* device = ReadGlString(GL_RENDERER);
	const char* version = ReadGlString(GL_VERSION);
	const char* shading = ReadGlString(GL_SHADING_LANGUAGE_VERSION);

	GLint max_texture_size = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

	GLint max_vertex_attribs = 0;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);

	std::cout << "[ Profiler - LogGraphicsCaps ] : gpu " << device << " ( " << vendor << " )\n";
	std::cout << "[ Profiler - LogGraphicsCaps ] : gl " << version << " glsl " << shading << "\n";
	std::cout << "[ Profiler - LogGraphicsCaps ] : max texture " << max_texture_size << " max attrib " << max_vertex_attribs << "\n";

	if (g_csv_ready == false)
	{
		return;
	}

	// 수치만 남기면 나중에 어느 기계 것인지 못 가린다. 주석 줄로 같이 박아 둔다
	g_csv << "# gpu=" << device << " vendor=" << vendor << " gl=" << version << " glsl=" << shading << "\n";
	g_csv << "# max_texture=" << max_texture_size << " max_attrib=" << max_vertex_attribs << "\n";

	WriteCsvHeader();
}

void BeginProfileStage(int32 stage)
{
	if (stage < 0 || stage >= PROFILE_STAGE_COUNT)
	{
		return;
	}

	// 겹쳐 열면 안쪽 시간이 바깥쪽에도 이중으로 쌓인다. 앞 구간을 닫고 넘긴다
	if (g_active_stage >= 0)
	{
		EndProfileStage();
	}

	g_active_stage = stage;
	g_stage_begin = Clock::now();
}

void EndProfileStage()
{
	if (g_active_stage < 0)
	{
		return;
	}

	const std::chrono::duration<float> elapsed = Clock::now() - g_stage_begin;
	g_frame.stage_seconds[static_cast<size_t>(g_active_stage)] += elapsed.count();

	g_active_stage = -1;
}

void CountDrawCall()
{
	g_frame.draw_calls += 1;

	if (g_active_stage >= 0)
	{
		g_frame.stage_draw_calls[static_cast<size_t>(g_active_stage)] += 1;
	}
}

void CountQuadSubmit()
{
	g_frame.quad_submits += 1;

	// 드로우콜은 배치를 비우는 구간 몫으로 적힌다. 어느 구간이 얼마나 밀어 넣었는지는 이쪽을 봐야 한다
	if (g_active_stage >= 0)
	{
		g_frame.stage_quads[static_cast<size_t>(g_active_stage)] += 1;
	}
}

void CountBatchFlush(int32 reason, int32 instance_count)
{
	g_frame.batch_flushes += 1;
	g_frame.batch_instances += instance_count;

	if (reason >= 0 && reason < FLUSH_REASON_COUNT)
	{
		g_frame.flush_reasons[static_cast<size_t>(reason)] += 1;
	}
}

void CountSceneActor(int32 count)
{
	g_frame.scene_actors += count;
}

void CountGlyphRaster()
{
	g_frame.glyph_rasters += 1;
}

void LogAssetLoad(const char* name, bool from_cache, size_t byte_count, float milliseconds)
{
	const std::ios::fmtflags saved_flags = std::cout.flags();
	const std::streamsize saved_precision = std::cout.precision();

	const char* source = "baked";
	if (from_cache)
	{
		source = "cached";
	}

	std::cout << std::fixed << std::setprecision(2);
	std::cout << "[ Profiler - LogAssetLoad ] : " << name << " " << source << " " << (byte_count / 1024) << "KB " << milliseconds << "ms\n";

	std::cout.flags(saved_flags);
	std::cout.precision(saved_precision);
}

void ToggleProfileDetail()
{
	g_show_detail = !g_show_detail;

	if (g_show_detail)
	{
		std::cout << "[ Profiler - ToggleProfileDetail ] : detail on\n";
	}
	else
	{
		std::cout << "[ Profiler - ToggleProfileDetail ] : detail off\n";
	}
}

void EndProfileFrame()
{
	// 열린 채로 프레임이 끝나면 다음 프레임 시간까지 그 구간에 붙는다
	EndProfileStage();

	const Clock::time_point now = Clock::now();

	// 첫 프레임은 기준 시각이 없어 길이를 못 잰다
	if (g_started == false)
	{
		g_started = true;
		g_last_time_point = now;
		g_frame = FrameStats();

		return;
	}

	const std::chrono::duration<float> elapsed = now - g_last_time_point;
	g_last_time_point = now;

	g_frame.seconds = elapsed.count();

	// 평균만 보면 가끔 튀는 프레임이 묻힌다. 표본을 다 들고 있다가 끝에서 비율 자리를 집는다
	g_frame_samples.push_back(g_frame.seconds);

	if (g_frame.seconds > g_worst_frame.seconds)
	{
		g_worst_frame = g_frame;
	}

	for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
	{
		const size_t index = static_cast<size_t>(stage);
		g_window.stage_draw_calls[index] += g_frame.stage_draw_calls[index];
		g_window.stage_quads[index] += g_frame.stage_quads[index];
		g_window.stage_seconds[index] += static_cast<double>(g_frame.stage_seconds[index]);
	}

	for (int32 reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
	{
		const size_t index = static_cast<size_t>(reason);
		g_window.flush_reasons[index] += g_frame.flush_reasons[index];
	}

	g_window.draw_calls += g_frame.draw_calls;
	g_window.quad_submits += g_frame.quad_submits;
	g_window.batch_flushes += g_frame.batch_flushes;
	g_window.batch_instances += g_frame.batch_instances;
	g_window.scene_actors += g_frame.scene_actors;
	g_window.glyph_rasters += g_frame.glyph_rasters;
	g_window.frame_count += 1;
	g_window.elapsed += static_cast<double>(g_frame.seconds);

	g_frame = FrameStats();

	if (g_window.elapsed < static_cast<double>(REPORT_INTERVAL_SEC))
	{
		return;
	}

	g_total_seconds += g_window.elapsed;

	std::vector<float> sorted = g_frame_samples;
	std::sort(sorted.begin(), sorted.end());

	const float frame_count = static_cast<float>(g_window.frame_count);
	const float fps = frame_count / static_cast<float>(g_window.elapsed);
	const float average_draw = ToAverage(g_window.draw_calls, g_window.frame_count);
	const float average_batch = ToAverage(g_window.batch_flushes, g_window.frame_count);
	const float average_quad = ToAverage(g_window.quad_submits, g_window.frame_count);
	const float average_instance = ToAverage(g_window.batch_instances, g_window.frame_count);
	const float average_ms = ToAverageMs(g_window.elapsed, g_window.frame_count);

	const float p50_ms = GetPercentile(sorted, 0.50f) * 1000.f;
	const float p95_ms = GetPercentile(sorted, 0.95f) * 1000.f;
	const float p99_ms = GetPercentile(sorted, 0.99f) * 1000.f;
	const float worst_ms = g_worst_frame.seconds * 1000.f;

	// 모으기 전 개수를 실제 드로우콜로 나눈 값. 1 이면 배치가 하나도 안 붙은 것이다
	float merge = 0.f;
	if (g_window.draw_calls > 0)
	{
		merge = static_cast<float>(g_window.quad_submits) / static_cast<float>(g_window.draw_calls);
	}

	PrintReport(fps, average_draw, average_batch, average_quad, merge, average_ms, p95_ms, worst_ms);

	if (g_show_detail)
	{
		PrintDetail(worst_ms);
	}

	if (g_csv_ready)
	{
		g_csv << std::fixed << std::setprecision(3);
		g_csv << g_total_seconds << "," << fps << "," << average_ms << "," << p50_ms << "," << p95_ms << "," << p99_ms << "," << worst_ms << ",";
		g_csv << average_draw << "," << average_batch << "," << average_quad << "," << average_instance << "," << merge << ",";

		for (int32 reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
		{
			g_csv << ToAverage(g_window.flush_reasons[static_cast<size_t>(reason)], g_window.frame_count) << ",";
		}

		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			g_csv << ToAverage(g_window.stage_draw_calls[static_cast<size_t>(stage)], g_window.frame_count) << ",";
		}

		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			g_csv << ToAverage(g_window.stage_quads[static_cast<size_t>(stage)], g_window.frame_count) << ",";
		}

		for (int32 stage = 0; stage < PROFILE_STAGE_COUNT; ++stage)
		{
			g_csv << ToAverageMs(g_window.stage_seconds[static_cast<size_t>(stage)], g_window.frame_count) << ",";
		}

		g_csv << ToAverage(g_window.scene_actors, g_window.frame_count) << "," << g_window.glyph_rasters << "\n";

		// 강제 종료로 닫아도 지금까지 잰 것은 남아 있어야 한다
		g_csv.flush();
	}

	ResetWindow();
}
