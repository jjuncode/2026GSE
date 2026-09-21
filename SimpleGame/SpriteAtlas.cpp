#include "stdafx.h"
#include "SpriteAtlas.h"
#include "GameConfig.h"
#include "SpriteArt.h"

#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
	constexpr int32 ATLAS_SIZE = 256;
	constexpr int32 PADDING = 1;	// 최근접으로 읽어도 경계에서 옆 그림을 집지 않게

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

	// 그림 윗줄이 텍스처 위쪽 ( v 가 큰 쪽 ) 에 오게 뒤집어 쓴다
	void BlitFrame(std::vector<uint8>* pixels, const SpriteArt& art, const std::vector<const char*>& rows, int32 base_x, int32 base_y)
	{
		for (int32 row = 0; row < art.height; ++row)
		{
			const char* line = rows[static_cast<size_t>(row)];
			const int32 line_length = static_cast<int32>(std::strlen(line));

			if (line_length != art.width)
			{
				std::cout << "[ SpriteAtlas - BlitFrame ] : row width mismatch " << line << "\n";
			}

			for (int32 column = 0; column < art.width && column < line_length; ++column)
			{
				const std::array<float, 4> color = GetPaletteColor(line[column]);
				const int32 texture_x = base_x + column;
				const int32 texture_y = base_y + (art.height - 1 - row);
				const size_t index = (static_cast<size_t>(texture_y) * static_cast<size_t>(ATLAS_SIZE) + static_cast<size_t>(texture_x)) * 4;

				for (size_t channel = 0; channel < color.size(); ++channel)
				{
					(*pixels)[index + channel] = static_cast<uint8>(color[channel] * 255.f + 0.5f);
				}
			}
		}
	}
}

bool InitSpriteAtlas()
{
	const std::vector<SpriteArt> arts = MakeSpriteArts();
	std::vector<uint8> pixels(static_cast<size_t>(ATLAS_SIZE * ATLAS_SIZE * 4), 0);

	g_entries.assign(arts.size(), SpriteEntry());

	int32 cursor_x = PADDING;
	int32 cursor_y = PADDING;
	int32 shelf_height = 0;
	const float atlas_size = static_cast<float>(ATLAS_SIZE);

	for (size_t sprite = 0; sprite < arts.size(); ++sprite)
	{
		const SpriteArt& art = arts[sprite];
		SpriteEntry& entry = g_entries[sprite];
		entry.width = art.width;
		entry.height = art.height;

		for (const std::vector<const char*>& rows : art.frames)
		{
			if (cursor_x + art.width + PADDING > ATLAS_SIZE)
			{
				cursor_x = PADDING;
				cursor_y += shelf_height + PADDING;
				shelf_height = 0;
			}

			if (cursor_y + art.height + PADDING > ATLAS_SIZE)
			{
				std::cout << "[ SpriteAtlas - InitSpriteAtlas ] : atlas full\n";
				return false;
			}

			BlitFrame(&pixels, art, rows, cursor_x, cursor_y);

			TexRegion region;
			region.u0 = static_cast<float>(cursor_x) / atlas_size;
			region.v0 = static_cast<float>(cursor_y) / atlas_size;
			region.u1 = static_cast<float>(cursor_x + art.width) / atlas_size;
			region.v1 = static_cast<float>(cursor_y + art.height) / atlas_size;
			entry.frames.push_back(region);

			cursor_x += art.width + PADDING;
			if (art.height > shelf_height)
			{
				shelf_height = art.height;
			}
		}
	}

	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	// 선형으로 읽으면 도트 경계가 번진다
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::cout << "[ SpriteAtlas - InitSpriteAtlas ] : ready\n";

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

	DrawSpriteQuad(renderer, sprite, frame, quad, flip, r, g, b, a);
}

void DrawSpriteQuad(Renderer* renderer, int32 sprite, int32 frame, const AffineQuad& quad, bool flip, float r, float g, float b, float a)
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

	renderer->DrawAffine(g_texture, region, quad, r, g, b, a);
}
