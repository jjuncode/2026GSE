#include "stdafx.h"
#include "SpriteSheet.h"
#include "GameConfig.h"
#include "Renderer.h"

#include <nlohmann/json.hpp>

// windows.h 의 min · max 매크로가 std::min 을 먹는다
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	using Json = nlohmann::json;

	const char* ART_DIR = "./Art/";

	const std::array<const char*, SHEET_COUNT> SHEET_NAMES = { "player", "blighted", "hound", "devourer", "gatebreaker", "npc_hunter", "npc_raider", "npc_merchant", "npc_gatherer", "npc_warden" };
	const std::array<const char*, ANIM_SLOT_COUNT> SLOT_NAMES = { "idle", "run", "guard", "guard_walk", "parry", "parry_success", "hit", "stun", "parried", "death", "attack", "work" };
	const std::array<const char*, VFX_COUNT> VFX_NAMES = { "hit", "crit", "hurt", "parry", "guard", "dust", "slam", "levelup", "heal", "ash", "cut" };

	constexpr int32 DIR_COUNT = 8;
	constexpr int32 MODE_ONCE = 0;
	constexpr int32 MODE_LOOP = 1;
	constexpr int32 MODE_STRIDE = 2;

	const float PI = 3.14159265f;

	struct SheetFrame
	{
		TexRegion region;
		int32 width = 0;
		int32 height = 0;
		int32 anchor_x = 0;	// 발밑 ( 그림 왼쪽 위에서 잰 칸 )
		int32 anchor_y = 0;
	};

	struct SheetAnim
	{
		int32 mode = MODE_ONCE;
		float period = 1.f;
		std::vector<float> times;	// 0 ~ 1. 이 시각부터 그 프레임
		std::array<std::vector<int32>, DIR_COUNT> dirs;
	};

	struct Sheet
	{
		GLuint texture = 0;
		std::vector<SheetFrame> frames;
		std::vector<SheetAnim> anims;
		std::array<int32, ANIM_SLOT_COUNT> slots = {};
		std::unordered_map<int32, int32> skill_anims;
		float body_height = 0.f;
		float body_width = 0.f;
	};

	struct VfxInfo
	{
		int32 first = 0;
		int32 count = 0;
		float frame_sec = 0.05f;
	};

	std::array<Sheet, SHEET_COUNT> g_sheets;
	Sheet g_vfx_sheet;
	std::array<VfxInfo, VFX_COUNT> g_vfx = {};

	template <typename T>
	void ReleaseCom(T** object)
	{
		if (*object != nullptr)
		{
			(*object)->Release();
			*object = nullptr;
		}
	}

	// WIC 로 PNG 를 RGBA 8 비트로 푼다. 윗줄부터 담긴다
	bool LoadPng(const std::wstring& path, std::vector<uint8>* pixels, int32* width, int32* height)
	{
		IWICImagingFactory* factory = nullptr;
		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;
		bool loaded = false;

		HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

		if (SUCCEEDED(result))
		{
			result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
		}

		if (SUCCEEDED(result))
		{
			result = decoder->GetFrame(0, &frame);
		}

		if (SUCCEEDED(result))
		{
			result = factory->CreateFormatConverter(&converter);
		}

		if (SUCCEEDED(result))
		{
			result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		}

		UINT image_width = 0;
		UINT image_height = 0;

		if (SUCCEEDED(result))
		{
			result = converter->GetSize(&image_width, &image_height);
		}

		if (SUCCEEDED(result))
		{
			const UINT stride = image_width * 4;
			pixels->assign(static_cast<size_t>(stride) * static_cast<size_t>(image_height), 0);
			result = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels->size()), pixels->data());
		}

		if (SUCCEEDED(result))
		{
			*width = static_cast<int32>(image_width);
			*height = static_cast<int32>(image_height);
			loaded = true;
		}

		ReleaseCom(&converter);
		ReleaseCom(&frame);
		ReleaseCom(&decoder);
		ReleaseCom(&factory);

		return loaded;
	}

	GLuint UploadTexture(const std::vector<uint8>& pixels, int32 width, int32 height)
	{
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		// 선형으로 읽으면 도트 경계가 번진다
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		return texture;
	}

	bool ReadJson(const std::string& path, Json* root)
	{
		std::ifstream file(path, std::ios::binary);
		if (file.is_open() == false)
		{
			return false;
		}

		*root = Json::parse(file, nullptr, false);

		return root->is_discarded() == false;
	}

	// 그림 한 장과 프레임 자리. PNG 는 윗줄이 텍스처 v 0 쪽에 올라가니 아래변 v 가 더 크다
	bool LoadSheetImage(const std::string& name, Sheet* sheet, Json* root)
	{
		const std::string base = std::string(ART_DIR) + name;

		if (ReadJson(base + ".json", root) == false)
		{
			std::cout << "[ SpriteSheet - LoadSheetImage ] : json missing " << name << "\n";
			return false;
		}

		std::vector<uint8> pixels;
		int32 width = 0;
		int32 height = 0;
		const std::wstring png_path(base.begin(), base.end());

		if (LoadPng(png_path + L".png", &pixels, &width, &height) == false)
		{
			std::cout << "[ SpriteSheet - LoadSheetImage ] : png missing " << name << "\n";
			return false;
		}

		sheet->texture = UploadTexture(pixels, width, height);

		const float texture_width = static_cast<float>(width);
		const float texture_height = static_cast<float>(height);

		for (const Json& entry : (*root)["frames"])
		{
			SheetFrame frame;
			const float x = entry[0].get<float>();
			const float y = entry[1].get<float>();
			frame.width = entry[2].get<int32>();
			frame.height = entry[3].get<int32>();
			frame.anchor_x = entry[4].get<int32>();
			frame.anchor_y = entry[5].get<int32>();

			frame.region.u0 = x / texture_width;
			frame.region.u1 = (x + static_cast<float>(frame.width)) / texture_width;
			frame.region.v0 = (y + static_cast<float>(frame.height)) / texture_height;
			frame.region.v1 = y / texture_height;

			sheet->frames.push_back(frame);
		}

		return true;
	}

	int32 ParseMode(const std::string& mode)
	{
		if (mode == "loop")
		{
			return MODE_LOOP;
		}

		if (mode == "stride")
		{
			return MODE_STRIDE;
		}

		return MODE_ONCE;
	}

	bool LoadCharacterSheet(int32 index)
	{
		Sheet& sheet = g_sheets[static_cast<size_t>(index)];
		sheet.slots.fill(-1);

		Json root;
		if (LoadSheetImage(SHEET_NAMES[static_cast<size_t>(index)], &sheet, &root) == false)
		{
			return false;
		}

		const std::string skill_prefix = "skill_";

		for (auto it = root["anims"].begin(); it != root["anims"].end(); ++it)
		{
			const std::string& name = it.key();
			const Json& data = it.value();

			SheetAnim anim;
			anim.mode = ParseMode(data.value("mode", std::string("once")));
			anim.period = data.value("period", 1.f);

			for (const Json& time : data["times"])
			{
				anim.times.push_back(time.get<float>());
			}

			const Json& dirs = data["dirs"];
			for (size_t dir = 0; dir < static_cast<size_t>(DIR_COUNT) && dir < dirs.size(); ++dir)
			{
				for (const Json& frame : dirs[dir])
				{
					anim.dirs[dir].push_back(frame.get<int32>());
				}
			}

			const int32 anim_index = static_cast<int32>(sheet.anims.size());
			sheet.anims.push_back(anim);

			for (size_t slot = 0; slot < SLOT_NAMES.size(); ++slot)
			{
				if (name == SLOT_NAMES[slot])
				{
					sheet.slots[slot] = anim_index;
				}
			}

			if (name.compare(0, skill_prefix.size(), skill_prefix) == 0)
			{
				const int32 skill_id = std::atoi(name.c_str() + skill_prefix.size());
				sheet.skill_anims[skill_id] = anim_index;
			}
		}

		// 정면 대기 첫 장의 발 위 높이
		const int32 idle = sheet.slots[static_cast<size_t>(ANIM_IDLE)];
		if (idle >= 0)
		{
			const std::vector<int32>& front = sheet.anims[static_cast<size_t>(idle)].dirs[2];
			if (front.empty() == false)
			{
				const SheetFrame& frame = sheet.frames[static_cast<size_t>(front[0])];
				sheet.body_height = static_cast<float>(frame.anchor_y);
				sheet.body_width = static_cast<float>(frame.width);
			}
		}

		return true;
	}

	bool LoadVfxSheet()
	{
		Json root;
		if (LoadSheetImage("vfx", &g_vfx_sheet, &root) == false)
		{
			return false;
		}

		const Json& effects = root["effects"];

		for (size_t index = 0; index < VFX_NAMES.size(); ++index)
		{
			const char* name = VFX_NAMES[index];
			if (effects.contains(name) == false)
			{
				std::cout << "[ SpriteSheet - LoadVfxSheet ] : effect missing " << name << "\n";
				continue;
			}

			const Json& effect = effects[name];
			VfxInfo& info = g_vfx[index];
			info.first = effect.value("first", 0);
			info.count = effect.value("count", 0);
			info.frame_sec = effect.value("frame_sec", 0.05f);
		}

		return true;
	}

	bool IsValidSheet(int32 sheet)
	{
		return sheet >= 0 && sheet < SHEET_COUNT && g_sheets[static_cast<size_t>(sheet)].texture != 0;
	}

	bool IsValidFrame(const Sheet& sheet, int32 frame)
	{
		return frame >= 0 && frame < static_cast<int32>(sheet.frames.size());
	}
}

bool LoadPngImage(const std::string& path, std::vector<uint8>* pixels, int32* width, int32* height)
{
	const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const std::wstring wide_path(path.begin(), path.end());
	const bool loaded = LoadPng(wide_path, pixels, width, height);

	if (SUCCEEDED(com_result))
	{
		CoUninitialize();
	}

	return loaded;
}

bool InitSpriteSheets()
{
	const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	// WIC 는 COM 위에서 돈다. 소리 쪽이 먼저 열었으면 다른 방식이라 실패해도 그대로 쓸 수 있다
	const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool loaded = true;

	for (int32 index = 0; index < SHEET_COUNT; ++index)
	{
		if (LoadCharacterSheet(index) == false)
		{
			loaded = false;
		}
	}

	if (LoadVfxSheet() == false)
	{
		loaded = false;
	}

	if (SUCCEEDED(com_result))
	{
		CoUninitialize();
	}

	const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - begin;
	std::cout << "[ SpriteSheet - InitSpriteSheets ] : " << elapsed.count() * 1000.f << " ms\n";

	return loaded;
}

void ShutdownSpriteSheets()
{
	for (Sheet& sheet : g_sheets)
	{
		if (sheet.texture != 0)
		{
			glDeleteTextures(1, &sheet.texture);
		}

		sheet = Sheet();
	}

	if (g_vfx_sheet.texture != 0)
	{
		glDeleteTextures(1, &g_vfx_sheet.texture);
	}

	g_vfx_sheet = Sheet();
}

int32 GetSheetAnim(int32 sheet, int32 slot)
{
	if (IsValidSheet(sheet) == false || slot < 0 || slot >= ANIM_SLOT_COUNT)
	{
		return -1;
	}

	return g_sheets[static_cast<size_t>(sheet)].slots[static_cast<size_t>(slot)];
}

int32 GetSheetSkillAnim(int32 sheet, int32 skill_id)
{
	if (IsValidSheet(sheet) == false)
	{
		return -1;
	}

	const std::unordered_map<int32, int32>& skill_anims = g_sheets[static_cast<size_t>(sheet)].skill_anims;
	const auto found = skill_anims.find(skill_id);

	if (found == skill_anims.end())
	{
		return -1;
	}

	return found->second;
}

float GetSheetAnimPeriod(int32 sheet, int32 anim)
{
	if (IsValidSheet(sheet) == false)
	{
		return 0.f;
	}

	const Sheet& data = g_sheets[static_cast<size_t>(sheet)];
	if (anim < 0 || anim >= static_cast<int32>(data.anims.size()))
	{
		return 0.f;
	}

	const SheetAnim& info = data.anims[static_cast<size_t>(anim)];
	if (info.mode == MODE_ONCE)
	{
		return 0.f;
	}

	return info.period;
}

float GetSheetBodyHeight(int32 sheet)
{
	if (IsValidSheet(sheet) == false)
	{
		return 0.f;
	}

	return g_sheets[static_cast<size_t>(sheet)].body_height;
}

float GetSheetBodyWidth(int32 sheet)
{
	if (IsValidSheet(sheet) == false)
	{
		return 0.f;
	}

	return g_sheets[static_cast<size_t>(sheet)].body_width;
}

int32 GetDir8(float world_dir_x, float world_dir_y)
{
	// 그림은 월드 -45 도 ( 화면 오른쪽 ) 를 0 번으로 45 도씩 구웠다
	const float degree = std::atan2(world_dir_y, world_dir_x) * 180.f / PI;
	const int32 step = static_cast<int32>(std::floor((degree + 45.f) / 45.f + 0.5f));

	return ((step % DIR_COUNT) + DIR_COUNT) % DIR_COUNT;
}

int32 PickSheetFrame(int32 sheet, int32 anim, int32 dir, float ratio, float seconds, float distance)
{
	if (IsValidSheet(sheet) == false)
	{
		return -1;
	}

	const Sheet& data = g_sheets[static_cast<size_t>(sheet)];
	if (anim < 0 || anim >= static_cast<int32>(data.anims.size()))
	{
		return -1;
	}

	const SheetAnim& info = data.anims[static_cast<size_t>(anim)];
	const std::vector<int32>& frames = info.dirs[static_cast<size_t>(((dir % DIR_COUNT) + DIR_COUNT) % DIR_COUNT)];

	if (frames.empty())
	{
		return -1;
	}

	float phase = std::min(std::max(ratio, 0.f), 1.f);

	if (info.mode == MODE_LOOP)
	{
		const float cycles = seconds / std::max(info.period, 0.001f);
		phase = cycles - std::floor(cycles);
	}
	else if (info.mode == MODE_STRIDE)
	{
		const float cycles = distance / std::max(info.period, 0.001f);
		phase = cycles - std::floor(cycles);
	}

	size_t pick = 0;
	for (size_t index = 0; index < info.times.size() && index < frames.size(); ++index)
	{
		if (info.times[index] <= phase + 0.0001f)
		{
			pick = index;
		}
	}

	return frames[pick];
}

void DrawSheetFrame(Renderer* renderer, int32 sheet, int32 frame, float foot_x, float foot_y, float r, float g, float b, float a, float flash)
{
	if (IsValidSheet(sheet) == false)
	{
		return;
	}

	const Sheet& data = g_sheets[static_cast<size_t>(sheet)];
	if (IsValidFrame(data, frame) == false)
	{
		return;
	}

	const SheetFrame& info = data.frames[static_cast<size_t>(frame)];
	const float unit = static_cast<float>(PIXEL_SCALE);

	// 발밑 칸이 정확히 발 좌표에 오게. 아래로 남는 칸 ( 내민 발, 칼끝 ) 만큼 내려 깐다
	AffineQuad quad;
	quad.origin_x = foot_x - static_cast<float>(info.anchor_x) * unit;
	quad.origin_y = foot_y - static_cast<float>(info.height - info.anchor_y) * unit;
	quad.axis_x_x = static_cast<float>(info.width) * unit;
	quad.axis_y_y = static_cast<float>(info.height) * unit;

	renderer->DrawAffineFlash(data.texture, info.region, quad, r, g, b, a, flash);
}

void DrawSheetShadow(Renderer* renderer, int32 sheet, int32 frame, float foot_x, float foot_y, float lean_x, float lean_y, float alpha)
{
	if (IsValidSheet(sheet) == false)
	{
		return;
	}

	const Sheet& data = g_sheets[static_cast<size_t>(sheet)];
	if (IsValidFrame(data, frame) == false)
	{
		return;
	}

	const SheetFrame& info = data.frames[static_cast<size_t>(frame)];
	const float unit = static_cast<float>(PIXEL_SCALE);
	const float height = static_cast<float>(info.height) * unit;
	const float below = static_cast<float>(info.height - info.anchor_y) / static_cast<float>(std::max(info.height, 1));

	// 발밑 줄을 축으로 눕힌다. 발보다 아래 칸은 눕힌 축 반대쪽으로 나간다
	AffineQuad quad;
	quad.axis_x_x = static_cast<float>(info.width) * unit;
	quad.axis_y_x = height * lean_x;
	quad.axis_y_y = height * lean_y;
	quad.origin_x = foot_x - static_cast<float>(info.anchor_x) * unit - quad.axis_y_x * below;
	quad.origin_y = foot_y - quad.axis_y_y * below;

	renderer->DrawAffine(data.texture, info.region, quad, 0.f, 0.f, 0.f, alpha);
}

int32 GetVfxFrameCount(int32 vfx)
{
	if (vfx < 0 || vfx >= VFX_COUNT)
	{
		return 0;
	}

	return g_vfx[static_cast<size_t>(vfx)].count;
}

float GetVfxFrameSec(int32 vfx)
{
	if (vfx < 0 || vfx >= VFX_COUNT)
	{
		return 0.05f;
	}

	return g_vfx[static_cast<size_t>(vfx)].frame_sec;
}

void DrawVfxFrame(Renderer* renderer, int32 vfx, int32 index, float center_x, float center_y, float angle, float scale, float r, float g, float b, float a)
{
	if (g_vfx_sheet.texture == 0 || vfx < 0 || vfx >= VFX_COUNT)
	{
		return;
	}

	const VfxInfo& info = g_vfx[static_cast<size_t>(vfx)];
	if (index < 0 || index >= info.count)
	{
		return;
	}

	const int32 frame_index = info.first + index;
	if (IsValidFrame(g_vfx_sheet, frame_index) == false)
	{
		return;
	}

	const SheetFrame& frame = g_vfx_sheet.frames[static_cast<size_t>(frame_index)];

	// 도트 배율은 정수로. 1.5 배 같은 값이면 칸 크기가 들쭉날쭉해진다
	const float unit = static_cast<float>(PIXEL_SCALE) * std::max(1.f, std::floor(scale + 0.5f));
	const float width = static_cast<float>(frame.width) * unit;
	const float height = static_cast<float>(frame.height) * unit;
	const float cos_angle = std::cos(angle);
	const float sin_angle = std::sin(angle);

	AffineQuad quad;
	quad.axis_x_x = cos_angle * width;
	quad.axis_x_y = sin_angle * width;
	quad.axis_y_x = -sin_angle * height;
	quad.axis_y_y = cos_angle * height;
	quad.origin_x = center_x - (quad.axis_x_x + quad.axis_y_x) * 0.5f;
	quad.origin_y = center_y - (quad.axis_x_y + quad.axis_y_y) * 0.5f;

	renderer->DrawAffine(g_vfx_sheet.texture, frame.region, quad, r, g, b, a);
}
