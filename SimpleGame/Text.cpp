#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Text.h"
#include "AssetCache.h"
#include "GameConfig.h"
#include "Profiler.h"
#include "Renderer.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace
{
	constexpr int32 CELL = 16;
	constexpr int32 ATLAS_SIZE = 512;
	constexpr int32 FONT_PIXEL_HEIGHT = 12;

	const wchar_t* FONT_NAME = L"Gulim";
	const char* GLYPH_CACHE_NAME = "glyph_atlas";

	struct Glyph
	{
		TexRegion region;
		float advance = 0.f;	// 글꼴 픽셀
		bool has_pixels = false;	// 공백은 그리기를 건너뛴다
	};

	HDC g_dc = nullptr;
	HFONT g_font = nullptr;
	HBITMAP g_bitmap = nullptr;
	HGDIOBJ g_old_font = nullptr;
	HGDIOBJ g_old_bitmap = nullptr;
	uint8* g_bits = nullptr;
	int32 g_line_height = FONT_PIXEL_HEIGHT;

	GLuint g_texture = 0;
	int32 g_next_cell = 0;
	std::unordered_map<wchar_t, Glyph> g_glyphs;
	std::vector<uint8> g_cell_pixels;

	// 텍스처는 되읽을 수 없으니 판을 CPU 쪽에도 들고 있어야 파일로 내릴 수 있다.
	// 흰 글자에 알파만 0 이나 255 라 알파 한 장만 남긴다
	std::vector<uint8> g_atlas_alpha;
	bool g_cache_dirty = false;

	uint64 MakeGlyphSignature()
	{
		uint64 signature = HashBytes(FONT_NAME, std::wcslen(FONT_NAME) * sizeof(wchar_t), 1469598103934665603ull);
		signature = HashValue(static_cast<uint64>(FONT_PIXEL_HEIGHT), signature);
		signature = HashValue(static_cast<uint64>(CELL), signature);
		signature = HashValue(static_cast<uint64>(ATLAS_SIZE), signature);

		return signature;
	}

	// 한글은 수천 자라 미리 못 굽는다. 처음 쓰는 글자만 그때 그려 칸에 올린다
	bool RasterizeGlyph(wchar_t character, Glyph* glyph)
	{
		const int32 cells_per_row = ATLAS_SIZE / CELL;
		if (g_next_cell >= cells_per_row * cells_per_row)
		{
			return false;
		}

		std::memset(g_bits, 0, static_cast<size_t>(CELL * CELL * 4));
		TextOutW(g_dc, 0, 0, &character, 1);
		GdiFlush();

		SIZE extent = {};
		GetTextExtentPoint32W(g_dc, &character, 1, &extent);

		const int32 cell_x = (g_next_cell % cells_per_row) * CELL;
		const int32 cell_y = (g_next_cell / cells_per_row) * CELL;
		g_next_cell += 1;

		bool has_pixels = false;

		for (int32 row = 0; row < CELL; ++row)
		{
			for (int32 column = 0; column < CELL; ++column)
			{
				const size_t source = static_cast<size_t>((row * CELL + column) * 4);
				const size_t target = static_cast<size_t>(((CELL - 1 - row) * CELL + column) * 4);

				bool lit = false;
				if (g_bits[source] > 127 || g_bits[source + 1] > 127 || g_bits[source + 2] > 127)
				{
					lit = true;
					has_pixels = true;
				}

				g_cell_pixels[target] = 255;
				g_cell_pixels[target + 1] = 255;
				g_cell_pixels[target + 2] = 255;

				if (lit)
				{
					g_cell_pixels[target + 3] = 255;
				}
				else
				{
					g_cell_pixels[target + 3] = 0;
				}

				// 판 전체 좌표로 되풀어 알파만 따로 적어 둔다
				const int32 atlas_x = cell_x + column;
				const int32 atlas_y = cell_y + (CELL - 1 - row);
				g_atlas_alpha[static_cast<size_t>(atlas_y) * static_cast<size_t>(ATLAS_SIZE) + static_cast<size_t>(atlas_x)] = g_cell_pixels[target + 3];
			}
		}

		glBindTexture(GL_TEXTURE_2D, g_texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, cell_x, cell_y, CELL, CELL, GL_RGBA, GL_UNSIGNED_BYTE, g_cell_pixels.data());

		const float atlas_size = static_cast<float>(ATLAS_SIZE);
		glyph->region.u0 = static_cast<float>(cell_x) / atlas_size;
		glyph->region.v0 = static_cast<float>(cell_y) / atlas_size;
		glyph->region.u1 = static_cast<float>(cell_x + CELL) / atlas_size;
		glyph->region.v1 = static_cast<float>(cell_y + CELL) / atlas_size;
		glyph->advance = static_cast<float>(extent.cx);
		glyph->has_pixels = has_pixels;

		g_cache_dirty = true;
		CountGlyphRaster();

		return true;
	}

	const Glyph* FindGlyph(wchar_t character)
	{
		const auto iter = g_glyphs.find(character);
		if (iter != g_glyphs.end())
		{
			return &iter->second;
		}

		Glyph glyph;
		if (RasterizeGlyph(character, &glyph) == false)
		{
			return nullptr;
		}

		const auto inserted = g_glyphs.emplace(character, glyph);
		return &inserted.first->second;
	}

	// 저해상도 픽셀 경계에 붙여야 글자 점이 두 픽셀로 쪼개지지 않는다
	float SnapToPixel(float value)
	{
		const float unit = static_cast<float>(UI_SCALE);
		return std::floor(value / unit) * unit;
	}

	void UploadAtlasFromAlpha()
	{
		std::vector<uint8> pixels(static_cast<size_t>(ATLAS_SIZE * ATLAS_SIZE * 4), 255);

		for (size_t index = 0; index < g_atlas_alpha.size(); ++index)
		{
			pixels[index * 4 + 3] = g_atlas_alpha[index];
		}

		glBindTexture(GL_TEXTURE_2D, g_texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ATLAS_SIZE, ATLAS_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	}

	std::vector<uint8> EncodeGlyphs()
	{
		BlobWriter writer;
		writer.WriteInt(g_next_cell);
		writer.WriteUint(static_cast<uint32>(g_glyphs.size()));

		for (const std::pair<const wchar_t, Glyph>& entry : g_glyphs)
		{
			writer.WriteUint(static_cast<uint32>(entry.first));
			writer.WriteFloat(entry.second.region.u0);
			writer.WriteFloat(entry.second.region.v0);
			writer.WriteFloat(entry.second.region.u1);
			writer.WriteFloat(entry.second.region.v1);
			writer.WriteFloat(entry.second.advance);

			uint8 has_pixels = 0;
			if (entry.second.has_pixels)
			{
				has_pixels = 1;
			}
			writer.WriteByte(has_pixels);
		}

		writer.WriteUint(static_cast<uint32>(ATLAS_SIZE));
		writer.WriteRaw(g_atlas_alpha.data(), g_atlas_alpha.size());

		return writer.bytes;
	}

	bool DecodeGlyphs(const std::vector<uint8>& blob)
	{
		BlobReader reader;
		reader.bytes = &blob;

		const int32 next_cell = reader.ReadInt();
		const uint32 glyph_count = reader.ReadUint();

		const int32 cells_per_row = ATLAS_SIZE / CELL;
		if (reader.failed || next_cell < 0 || next_cell > cells_per_row * cells_per_row)
		{
			return false;
		}

		std::unordered_map<wchar_t, Glyph> glyphs;

		for (uint32 index = 0; index < glyph_count; ++index)
		{
			const wchar_t character = static_cast<wchar_t>(reader.ReadUint());

			Glyph glyph;
			glyph.region.u0 = reader.ReadFloat();
			glyph.region.v0 = reader.ReadFloat();
			glyph.region.u1 = reader.ReadFloat();
			glyph.region.v1 = reader.ReadFloat();
			glyph.advance = reader.ReadFloat();
			glyph.has_pixels = reader.ReadByte() != 0;

			if (reader.failed)
			{
				return false;
			}

			glyphs.emplace(character, glyph);
		}

		const uint32 atlas_size = reader.ReadUint();

		if (reader.failed || atlas_size != static_cast<uint32>(ATLAS_SIZE))
		{
			return false;
		}

		reader.ReadRaw(g_atlas_alpha.data(), g_atlas_alpha.size());

		if (reader.failed)
		{
			return false;
		}

		g_glyphs.swap(glyphs);
		g_next_cell = next_cell;

		return true;
	}
}

bool InitText()
{
	const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	g_dc = CreateCompatibleDC(nullptr);
	if (g_dc == nullptr)
	{
		std::cout << "[ Text - InitText ] : cannot create dc\n";
		return false;
	}

	// 굴림 12 픽셀은 글꼴 안 비트맵으로 찍혀서 흐린 가장자리가 없다
	g_font = CreateFontW(-FONT_PIXEL_HEIGHT, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGUL_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FONT_NAME);

	if (g_font == nullptr)
	{
		std::cout << "[ Text - InitText ] : cannot create font\n";
		return false;
	}

	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = CELL;
	info.bmiHeader.biHeight = -CELL;	// 음수 = 윗줄부터
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	g_bitmap = CreateDIBSection(g_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);

	if (g_bitmap == nullptr || bits == nullptr)
	{
		std::cout << "[ Text - InitText ] : cannot create dib\n";
		return false;
	}

	g_bits = static_cast<uint8*>(bits);
	g_old_bitmap = SelectObject(g_dc, g_bitmap);
	g_old_font = SelectObject(g_dc, g_font);
	SetTextColor(g_dc, RGB(255, 255, 255));
	SetBkMode(g_dc, TRANSPARENT);

	TEXTMETRICW metrics = {};
	GetTextMetricsW(g_dc, &metrics);
	g_line_height = metrics.tmHeight;

	g_cell_pixels.assign(static_cast<size_t>(CELL * CELL * 4), 0);
	g_atlas_alpha.assign(static_cast<size_t>(ATLAS_SIZE * ATLAS_SIZE), 0);

	const std::vector<uint8> empty(static_cast<size_t>(ATLAS_SIZE * ATLAS_SIZE * 4), 0);

	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, empty.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// 지난 실행에서 구워 둔 글자를 되읽는다. 없으면 게임 중 처음 쓰는 글자마다 GDI 가 돌아 그 프레임이 튄다
	std::vector<uint8> blob;
	bool from_cache = false;

	if (LoadCachedBlob(GLYPH_CACHE_NAME, MakeGlyphSignature(), &blob))
	{
		from_cache = DecodeGlyphs(blob);

		if (from_cache)
		{
			UploadAtlasFromAlpha();
		}
	}

	const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - begin;
	LogAssetLoad(GLYPH_CACHE_NAME, from_cache, g_atlas_alpha.size(), elapsed.count() * 1000.f);

	std::cout << "[ Text - InitText ] : ready glyph " << g_glyphs.size() << "\n";

	return true;
}

void SaveTextCache()
{
	if (g_cache_dirty == false || g_glyphs.empty())
	{
		return;
	}

	// 이번 실행에서 새로 구운 글자가 있으면 다음 실행이 그만큼 덜 튄다
	SaveCachedBlob(GLYPH_CACHE_NAME, MakeGlyphSignature(), EncodeGlyphs());
	g_cache_dirty = false;
}

void ShutdownText()
{
	SaveTextCache();

	if (g_dc != nullptr)
	{
		if (g_old_font != nullptr)
		{
			SelectObject(g_dc, g_old_font);
		}

		if (g_old_bitmap != nullptr)
		{
			SelectObject(g_dc, g_old_bitmap);
		}
	}

	if (g_font != nullptr)
	{
		DeleteObject(g_font);
		g_font = nullptr;
	}

	if (g_bitmap != nullptr)
	{
		DeleteObject(g_bitmap);
		g_bitmap = nullptr;
	}

	if (g_dc != nullptr)
	{
		DeleteDC(g_dc);
		g_dc = nullptr;
	}

	if (g_texture != 0)
	{
		glDeleteTextures(1, &g_texture);
		g_texture = 0;
	}

	g_glyphs.clear();
	g_atlas_alpha.clear();
	g_next_cell = 0;
}

void DrawString(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a)
{
	DrawStringScaled(renderer, text, x, y, align, 1, r, g, b, a);
}

void DrawStringScaled(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, int32 scale, float r, float g, float b, float a)
{
	if (g_texture == 0)
	{
		return;
	}

	if (scale < 1)
	{
		scale = 1;
	}

	const float unit = static_cast<float>(UI_SCALE * scale);
	const float cell_size = static_cast<float>(CELL) * unit;
	const float width = MeasureString(text) * static_cast<float>(scale);

	float pen_x = x;
	if (align == TEXT_ALIGN_CENTER)
	{
		pen_x = x - width * 0.5f;
	}
	else if (align == TEXT_ALIGN_RIGHT)
	{
		pen_x = x - width;
	}

	pen_x = SnapToPixel(pen_x);
	const float top = SnapToPixel(y);

	for (wchar_t character : text)
	{
		const Glyph* glyph = FindGlyph(character);
		if (glyph == nullptr)
		{
			continue;
		}

		if (glyph->has_pixels)
		{
			AffineQuad quad;
			quad.origin_x = pen_x;
			quad.origin_y = top - cell_size;
			quad.axis_x_x = cell_size;
			quad.axis_y_y = cell_size;

			renderer->DrawAffine(g_texture, glyph->region, quad, r, g, b, a);
		}

		pen_x += glyph->advance * unit;
	}
}

void DrawStringOutline(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, int32 scale, float r, float g, float b, float a)
{
	const float unit = static_cast<float>(UI_SCALE);
	const float shadow_alpha = a * 0.9f;

	DrawStringScaled(renderer, text, x + unit, y, align, scale, 0.f, 0.f, 0.f, shadow_alpha);
	DrawStringScaled(renderer, text, x - unit, y, align, scale, 0.f, 0.f, 0.f, shadow_alpha);
	DrawStringScaled(renderer, text, x, y + unit, align, scale, 0.f, 0.f, 0.f, shadow_alpha);
	DrawStringScaled(renderer, text, x, y - unit, align, scale, 0.f, 0.f, 0.f, shadow_alpha);
	DrawStringScaled(renderer, text, x + unit, y - unit, align, scale, 0.f, 0.f, 0.f, shadow_alpha);
	DrawStringScaled(renderer, text, x, y, align, scale, r, g, b, a);
}

void DrawStringShadow(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a)
{
	const float unit = static_cast<float>(UI_SCALE);

	DrawString(renderer, text, x + unit, y - unit, align, 0.f, 0.f, 0.f, a * 0.85f);
	DrawString(renderer, text, x, y, align, r, g, b, a);
}

float MeasureString(const std::wstring& text)
{
	float width = 0.f;

	for (wchar_t character : text)
	{
		const Glyph* glyph = FindGlyph(character);
		if (glyph != nullptr)
		{
			width += glyph->advance;
		}
	}

	return width * static_cast<float>(UI_SCALE);
}

float GetLineHeight()
{
	return static_cast<float>(g_line_height * UI_SCALE);
}
