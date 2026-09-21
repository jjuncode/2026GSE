#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Text.h"
#include "GameConfig.h"
#include "Renderer.h"

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
		const float unit = static_cast<float>(PIXEL_SCALE);
		return std::floor(value / unit) * unit;
	}
}

bool InitText()
{
	g_dc = CreateCompatibleDC(nullptr);
	if (g_dc == nullptr)
	{
		std::cout << "[ Text - InitText ] : cannot create dc\n";
		return false;
	}

	// 굴림 12 픽셀은 글꼴 안 비트맵으로 찍혀서 흐린 가장자리가 없다
	g_font = CreateFontW(-FONT_PIXEL_HEIGHT, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGUL_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Gulim");

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

	const std::vector<uint8> empty(static_cast<size_t>(ATLAS_SIZE * ATLAS_SIZE * 4), 0);

	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, empty.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::cout << "[ Text - InitText ] : ready\n";

	return true;
}

void ShutdownText()
{
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
	g_next_cell = 0;
}

void DrawString(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a)
{
	if (g_texture == 0)
	{
		return;
	}

	const float unit = static_cast<float>(PIXEL_SCALE);
	const float cell_size = static_cast<float>(CELL) * unit;

	float pen_x = x;
	if (align == TEXT_ALIGN_CENTER)
	{
		pen_x = x - MeasureString(text) * 0.5f;
	}
	else if (align == TEXT_ALIGN_RIGHT)
	{
		pen_x = x - MeasureString(text);
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

void DrawStringShadow(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a)
{
	const float unit = static_cast<float>(PIXEL_SCALE);

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

	return width * static_cast<float>(PIXEL_SCALE);
}

float GetLineHeight()
{
	return static_cast<float>(g_line_height * PIXEL_SCALE);
}
