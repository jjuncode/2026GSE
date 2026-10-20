#include "stdafx.h"
#include "HouseArt.h"
#include "GameConfig.h"
#include "Renderer.h"
#include "SpriteSheet.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	using Json = nlohmann::json;

	const char* HOUSE_IMAGE = "./Art/houses.png";
	const char* HOUSE_TABLE = "./Art/houses.json";

	struct HousePiece
	{
		int32 size_x = 0;
		int32 size_y = 0;
		int32 door_col = 0;
		int32 roof = 0;
		int32 tile_x = 0;
		int32 tile_y = 0;
		int32 width = 0;
		int32 height = 0;
		TexRegion region;
	};

	struct HouseProp
	{
		std::string name;
		int32 width = 0;
		int32 height = 0;
		TexRegion region;
	};

	std::vector<HousePiece> g_pieces;
	std::vector<HouseProp> g_props;
	GLuint g_texture = 0;
	int32 g_roof_count = 0;

	// PNG 는 윗줄이 텍스처 v 0 쪽이라 아래변 v 가 더 크다
	TexRegion MakeRegion(const Json& rect, float texture_width, float texture_height, int32* width, int32* height)
	{
		const float x = rect[0].get<float>();
		const float y = rect[1].get<float>();
		*width = rect[2].get<int32>();
		*height = rect[3].get<int32>();

		TexRegion region;
		region.u0 = x / texture_width;
		region.u1 = (x + static_cast<float>(*width)) / texture_width;
		region.v0 = (y + static_cast<float>(*height)) / texture_height;
		region.v1 = y / texture_height;

		return region;
	}

	void DrawRegion(Renderer* renderer, const TexRegion& region, int32 width, int32 height, float foot_x, float foot_y, float r, float g, float b)
	{
		const float unit = static_cast<float>(PIXEL_SCALE);
		const float pixel_width = static_cast<float>(width) * unit;
		const float pixel_height = static_cast<float>(height) * unit;

		AffineQuad quad;
		quad.origin_x = foot_x - pixel_width * 0.5f;
		quad.origin_y = foot_y;
		quad.axis_x_x = pixel_width;
		quad.axis_y_y = pixel_height;

		renderer->DrawAffineFlash(g_texture, region, quad, r, g, b, 1.f, 0.f);
	}

	bool ReadHouseTable(int32 atlas_width, int32 atlas_height)
	{
		std::ifstream file(HOUSE_TABLE, std::ios::binary);
		if (file.is_open() == false)
		{
			return false;
		}

		const Json root = Json::parse(file, nullptr, false);
		if (root.is_discarded() || root.contains("pieces") == false)
		{
			std::cout << "[ HouseArt - ReadHouseTable ] : table broken\n";
			return false;
		}

		const float texture_width = static_cast<float>(atlas_width);
		const float texture_height = static_cast<float>(atlas_height);

		for (const Json& entry : root["pieces"])
		{
			HousePiece piece;
			piece.size_x = entry["size"][0].get<int32>();
			piece.size_y = entry["size"][1].get<int32>();
			piece.door_col = entry["door"].get<int32>();
			piece.roof = entry["roof"].get<int32>();
			piece.tile_x = entry["tile"][0].get<int32>();
			piece.tile_y = entry["tile"][1].get<int32>();
			piece.region = MakeRegion(entry["rect"], texture_width, texture_height, &piece.width, &piece.height);

			if (piece.roof + 1 > g_roof_count)
			{
				g_roof_count = piece.roof + 1;
			}

			g_pieces.push_back(piece);
		}

		if (root.contains("props"))
		{
			for (const Json& entry : root["props"])
			{
				HouseProp prop;
				prop.name = entry["name"].get<std::string>();
				prop.region = MakeRegion(entry["rect"], texture_width, texture_height, &prop.width, &prop.height);
				g_props.push_back(prop);
			}
		}

		return g_pieces.empty() == false;
	}
}

bool InitHouseArt()
{
	std::vector<uint8> pixels;
	int32 width = 0;
	int32 height = 0;

	if (LoadPngImage(HOUSE_IMAGE, &pixels, &width, &height) == false)
	{
		std::cout << "[ HouseArt - InitHouseArt ] : image missing. run tools/art/bake_houses.py\n";
		return false;
	}

	if (ReadHouseTable(width, height) == false)
	{
		g_pieces.clear();
		return false;
	}

	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::cout << "[ HouseArt - InitHouseArt ] : pieces " << g_pieces.size() << " roofs " << g_roof_count << "\n";

	return true;
}

void ShutdownHouseArt()
{
	if (g_texture != 0)
	{
		glDeleteTextures(1, &g_texture);
		g_texture = 0;
	}

	g_pieces.clear();
	g_props.clear();
	g_roof_count = 0;
}

bool HasHouseArt()
{
	return g_pieces.empty() == false;
}

int32 FindHousePiece(int32 size_x, int32 size_y, int32 door_col, int32 roof, int32 tile_x, int32 tile_y)
{
	for (size_t index = 0; index < g_pieces.size(); ++index)
	{
		const HousePiece& piece = g_pieces[index];

		if (piece.size_x != size_x || piece.size_y != size_y || piece.door_col != door_col || piece.roof != roof)
		{
			continue;
		}

		if (piece.tile_x == tile_x && piece.tile_y == tile_y)
		{
			return static_cast<int32>(index);
		}
	}

	return -1;
}

int32 GetHouseRoofCount()
{
	return g_roof_count;
}

void DrawHousePiece(Renderer* renderer, int32 piece, float foot_x, float foot_y, float r, float g, float b)
{
	if (piece < 0 || piece >= static_cast<int32>(g_pieces.size()))
	{
		return;
	}

	const HousePiece& entry = g_pieces[static_cast<size_t>(piece)];
	DrawRegion(renderer, entry.region, entry.width, entry.height, foot_x, foot_y, r, g, b);
}

int32 FindHouseProp(const char* name)
{
	for (size_t index = 0; index < g_props.size(); ++index)
	{
		if (g_props[index].name == name)
		{
			return static_cast<int32>(index);
		}
	}

	return -1;
}

void DrawHouseProp(Renderer* renderer, int32 prop, float foot_x, float foot_y, float r, float g, float b)
{
	if (prop < 0 || prop >= static_cast<int32>(g_props.size()))
	{
		return;
	}

	const HouseProp& entry = g_props[static_cast<size_t>(prop)];
	DrawRegion(renderer, entry.region, entry.width, entry.height, foot_x, foot_y, r, g, b);
}
