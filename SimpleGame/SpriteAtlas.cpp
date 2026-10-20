#include "stdafx.h"
#include "SpriteAtlas.h"
#include "GameConfig.h"
#include "SpriteSheet.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	using Json = nlohmann::json;

	// tools/art/bake_props.py 가 SpriteId.h 순서대로 구운 한 장
	const char* ATLAS_IMAGE = "./Art/props.png";
	const char* ATLAS_TABLE = "./Art/props.json";

	struct SpriteEntry
	{
		int32 width = 0;
		int32 height = 0;
		std::vector<TexRegion> frames;
	};

	std::vector<SpriteEntry> g_entries;
	GLuint g_texture = 0;

	bool IsValidSprite(int32 sprite)
	{
		if (sprite < 0)
		{
			return false;
		}

		return sprite < static_cast<int32>(g_entries.size());
	}

	void UploadAtlasTexture(const std::vector<uint8>& pixels, int32 width, int32 height)
	{
		glGenTextures(1, &g_texture);
		glBindTexture(GL_TEXTURE_2D, g_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		// 선형으로 읽으면 도트 경계가 번진다
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	// PNG 는 윗줄이 텍스처 v 0 쪽이라 아래변 v 가 더 크다
	bool ReadAtlasTable(int32 atlas_width, int32 atlas_height)
	{
		std::ifstream file(ATLAS_TABLE, std::ios::binary);
		if (file.is_open() == false)
		{
			std::cout << "[ SpriteAtlas - ReadAtlasTable ] : table missing\n";
			return false;
		}

		const Json root = Json::parse(file, nullptr, false);
		if (root.is_discarded() || root.contains("sprites") == false)
		{
			std::cout << "[ SpriteAtlas - ReadAtlasTable ] : table broken\n";
			return false;
		}

		const Json& sprites = root["sprites"];
		if (static_cast<int32>(sprites.size()) != SPRITE_COUNT)
		{
			// 번호가 어긋나면 엉뚱한 그림이 나온다. SpriteId.h 를 고쳤으면 다시 굽는다
			std::cout << "[ SpriteAtlas - ReadAtlasTable ] : sprite count " << sprites.size() << " != " << SPRITE_COUNT << "\n";
			return false;
		}

		const float texture_width = static_cast<float>(atlas_width);
		const float texture_height = static_cast<float>(atlas_height);

		g_entries.assign(sprites.size(), SpriteEntry());

		for (size_t index = 0; index < sprites.size(); ++index)
		{
			const Json& sprite = sprites[index];
			SpriteEntry& entry = g_entries[index];
			entry.width = sprite.value("w", 0);
			entry.height = sprite.value("h", 0);

			for (const Json& frame : sprite["frames"])
			{
				const float x = frame[0].get<float>();
				const float y = frame[1].get<float>();

				TexRegion region;
				region.u0 = x / texture_width;
				region.u1 = (x + static_cast<float>(entry.width)) / texture_width;
				region.v0 = (y + static_cast<float>(entry.height)) / texture_height;
				region.v1 = y / texture_height;

				entry.frames.push_back(region);
			}
		}

		return true;
	}
}

bool InitSpriteAtlas()
{
	const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	std::vector<uint8> pixels;
	int32 width = 0;
	int32 height = 0;

	if (LoadPngImage(ATLAS_IMAGE, &pixels, &width, &height) == false)
	{
		std::cout << "[ SpriteAtlas - InitSpriteAtlas ] : image missing. run tools/art/bake_props.py\n";
		return false;
	}

	if (ReadAtlasTable(width, height) == false)
	{
		g_entries.clear();
		return false;
	}

	UploadAtlasTexture(pixels, width, height);

	const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - begin;
	std::cout << "[ SpriteAtlas - InitSpriteAtlas ] : " << width << " x " << height << " " << elapsed.count() * 1000.f << " ms\n";

	return true;
}

void ShutdownSpriteAtlas()
{
	if (g_texture != 0)
	{
		glDeleteTextures(1, &g_texture);
		g_texture = 0;
	}

	g_entries.clear();
}

int32 GetSpriteWidth(int32 sprite)
{
	if (IsValidSprite(sprite) == false)
	{
		return 0;
	}

	return g_entries[static_cast<size_t>(sprite)].width;
}

int32 GetSpriteHeight(int32 sprite)
{
	if (IsValidSprite(sprite) == false)
	{
		return 0;
	}

	return g_entries[static_cast<size_t>(sprite)].height;
}

void DrawSprite(Renderer* renderer, int32 sprite, int32 frame, float foot_x, float foot_y, float scale, bool flip, float r, float g, float b, float a)
{
	DrawSpriteFlash(renderer, sprite, frame, foot_x, foot_y, scale, flip, r, g, b, a, 0.f);
}

void DrawSpriteFlash(Renderer* renderer, int32 sprite, int32 frame, float foot_x, float foot_y, float scale, bool flip, float r, float g, float b, float a, float flash)
{
	if (IsValidSprite(sprite) == false)
	{
		return;
	}

	const SpriteEntry& entry = g_entries[static_cast<size_t>(sprite)];
	const float unit = static_cast<float>(PIXEL_SCALE) * scale;
	const float width = static_cast<float>(entry.width) * unit;
	const float height = static_cast<float>(entry.height) * unit;

	AffineQuad quad;
	quad.origin_x = foot_x - width * 0.5f;
	quad.origin_y = foot_y;
	quad.axis_x_x = width;
	quad.axis_y_y = height;

	DrawSpriteQuadFlash(renderer, sprite, frame, quad, flip, r, g, b, a, flash);
}

void DrawSpriteQuad(Renderer* renderer, int32 sprite, int32 frame, const AffineQuad& quad, bool flip, float r, float g, float b, float a)
{
	DrawSpriteQuadFlash(renderer, sprite, frame, quad, flip, r, g, b, a, 0.f);
}

void DrawSpriteQuadFlash(Renderer* renderer, int32 sprite, int32 frame, const AffineQuad& quad, bool flip, float r, float g, float b, float a, float flash)
{
	if (IsValidSprite(sprite) == false || frame < 0)
	{
		return;
	}

	const SpriteEntry& entry = g_entries[static_cast<size_t>(sprite)];
	if (entry.frames.empty())
	{
		return;
	}

	TexRegion region = entry.frames[static_cast<size_t>(frame) % entry.frames.size()];

	if (flip)
	{
		std::swap(region.u0, region.u1);
	}

	renderer->DrawAffineFlash(g_texture, region, quad, r, g, b, a, flash);
}
