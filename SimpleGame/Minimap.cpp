#include "stdafx.h"
#include "Minimap.h"
#include "Monster.h"
#include "Npc.h"
#include "NpcData.h"
#include "Player.h"
#include "Renderer.h"
#include "ZoneGen.h"

#include <array>
#include <vector>

namespace
{
	// 칸 하나가 화면에서 차지하는 반폭 ( 픽셀 ). 아이소라 세로는 이 절반
	const float TILE_SPAN = 0.7f;
	const float CENTER_X = 470.f;
	const float CENTER_Y = 40.f;	// 오른쪽 가운데. 아래는 경험치 글자, 위는 조작 안내가 있다

	GLuint g_texture = 0;
	bool g_visible = false;
	std::vector<ZoneSite> g_sites;

	struct DotContext
	{
		Renderer* renderer = nullptr;
	};

	std::array<uint8, 3> GetTileColor(uint8 tile)
	{
		switch (tile)
		{
			case TILE_ROAD:
			{
				return { 102, 84, 60 };
			}
			case TILE_PLAZA:
			{
				return { 128, 122, 114 };
			}
			case TILE_FIELD:
			{
				return { 84, 66, 40 };
			}
			case TILE_BLIGHT:
			{
				return { 76, 36, 76 };
			}
			case TILE_CLIFF:
			{
				return { 8, 8, 10 };
			}
			case TILE_WATER:
			{
				return { 16, 30, 30 };
			}
			case TILE_ROCK:
			{
				return { 100, 100, 100 };
			}
			case TILE_TREE:
			{
				return { 26, 34, 22 };
			}
			case TILE_WALL:
			{
				return { 178, 172, 162 };
			}
			case TILE_TOWER:
			{
				return { 204, 198, 188 };
			}
			case TILE_HOUSE:
			{
				return { 132, 76, 60 };
			}
			case TILE_RUIN:
			{
				return { 106, 106, 92 };
			}
			default:
			{
				return { 52, 48, 44 };
			}
		}
	}

	// 칸 (x, y) 를 지도 위 화면 좌표로. WorldToScreen 과 같은 부호 규칙을 배율만 줄여 쓴다
	void ProjectTile(float tile_x, float tile_y, float* screen_x, float* screen_y)
	{
		const float half = static_cast<float>(ZONE_TILE_COUNT) * 0.5f;
		const float offset_x = tile_x - half;
		const float offset_y = tile_y - half;

		*screen_x = CENTER_X + (offset_x - offset_y) * TILE_SPAN;
		*screen_y = CENTER_Y - (offset_x + offset_y) * TILE_SPAN * 0.5f;
	}

	void ProjectWorld(const WorldPos& pos, float* screen_x, float* screen_y)
	{
		const float tile_x = static_cast<float>((pos.chunk_x + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_x;
		const float tile_y = static_cast<float>((pos.chunk_y + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_y;

		ProjectTile(tile_x, tile_y, screen_x, screen_y);
	}

	std::array<float, 3> GetJobDotColor(int32 job, bool is_outlaw)
	{
		if (is_outlaw)
		{
			return { 1.f, 0.30f, 0.25f };
		}

		switch (job)
		{
			case NPC_JOB_HUNTER:
			{
				return { 0.55f, 0.95f, 0.40f };
			}
			case NPC_JOB_MERCHANT:
			{
				return { 0.95f, 0.60f, 1.f };
			}
			case NPC_JOB_GATHERER:
			{
				return { 0.95f, 0.88f, 0.55f };
			}
			default:
			{
				return { 0.35f, 0.95f, 0.88f };
			}
		}
	}

	void DrawNpcDot(const WorldPos& pos, int32 job, bool is_outlaw, bool is_selected, void* context)
	{
		Renderer* renderer = static_cast<DotContext*>(context)->renderer;

		float x = 0.f;
		float y = 0.f;
		ProjectWorld(pos, &x, &y);

		const std::array<float, 3> color = GetJobDotColor(job, is_outlaw);

		if (is_selected)
		{
			renderer->DrawSolidBox(x, y, 9.f, 9.f, 1.f, 1.f, 1.f, 1.f);
		}

		renderer->DrawSolidBox(x, y, 5.f, 5.f, color[0], color[1], color[2], 1.f);
	}

	void DrawMonsterDot(const WorldPos& pos, int32 kind, bool is_elite, void* context)
	{
		Renderer* renderer = static_cast<DotContext*>(context)->renderer;

		float x = 0.f;
		float y = 0.f;
		ProjectWorld(pos, &x, &y);

		if (kind == MONSTER_GATEBREAKER)
		{
			renderer->DrawSolidBox(x, y, 9.f, 9.f, 0.85f, 0.10f, 0.85f, 1.f);
			return;
		}

		if (is_elite)
		{
			renderer->DrawSolidBox(x, y, 5.f, 5.f, 1.f, 0.55f, 0.10f, 1.f);
			return;
		}

		float size = 3.f;
		if (kind == MONSTER_DEVOURER)
		{
			size = 5.f;
		}

		renderer->DrawSolidBox(x, y, size, size, 0.55f, 0.10f, 0.08f, 1.f);
	}

	void DrawSiteMarks(Renderer* renderer)
	{
		for (const ZoneSite& site : g_sites)
		{
			float x = 0.f;
			float y = 0.f;
			ProjectTile(static_cast<float>(site.tile.x) + 0.5f, static_cast<float>(site.tile.y) + 0.5f, &x, &y);

			if (site.kind == SITE_OUTPOST)
			{
				renderer->DrawSolidBox(x, y, 9.f, 9.f, 0.02f, 0.02f, 0.02f, 1.f);
				renderer->DrawSolidBox(x, y, 6.f, 6.f, 0.20f, 0.65f, 0.60f, 1.f);
			}
			else if (site.kind == SITE_HIDEOUT)
			{
				renderer->DrawSolidBox(x, y, 9.f, 9.f, 0.02f, 0.02f, 0.02f, 1.f);
				renderer->DrawSolidBox(x, y, 6.f, 6.f, 0.75f, 0.12f, 0.10f, 1.f);
			}
			else if (site.kind == SITE_VILLAGE)
			{
				renderer->DrawSolidBox(x, y, 6.f, 6.f, 0.60f, 0.58f, 0.50f, 1.f);
			}
		}
	}
}

void BuildMinimap(const ZoneLayout& layout)
{
	g_sites = layout.sites;

	const size_t count = static_cast<size_t>(ZONE_TILE_COUNT * ZONE_TILE_COUNT);
	std::vector<uint8> pixels(count * 4, 255);

	for (size_t index = 0; index < count && index < layout.tiles.size(); ++index)
	{
		const std::array<uint8, 3> color = GetTileColor(layout.tiles[index]);

		pixels[index * 4 + 0] = color[0];
		pixels[index * 4 + 1] = color[1];
		pixels[index * 4 + 2] = color[2];
	}

	if (g_texture == 0)
	{
		glGenTextures(1, &g_texture);
	}

	glBindTexture(GL_TEXTURE_2D, g_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ZONE_TILE_COUNT, ZONE_TILE_COUNT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void ShutdownMinimap()
{
	if (g_texture != 0)
	{
		glDeleteTextures(1, &g_texture);
		g_texture = 0;
	}
}

void ToggleMinimap()
{
	g_visible = !g_visible;
}

void DrawMinimap(Renderer* renderer, bool hidden)
{
	if (g_visible == false || hidden || g_texture == 0)
	{
		return;
	}

	// 텍스처 u 가 칸 x, v 가 칸 y. 두 축을 아이소 방향으로 눕히면 지도가 화면과 같은 각도로 선다
	const float span = static_cast<float>(ZONE_TILE_COUNT) * TILE_SPAN;

	AffineQuad quad;
	ProjectTile(0.f, 0.f, &quad.origin_x, &quad.origin_y);
	quad.axis_x_x = span;
	quad.axis_x_y = -span * 0.5f;
	quad.axis_y_x = -span;
	quad.axis_y_y = -span * 0.5f;

	TexRegion region;
	renderer->DrawAffine(g_texture, region, quad, 1.f, 1.f, 1.f, 0.92f);

	DrawSiteMarks(renderer);

	DotContext context;
	context.renderer = renderer;

	ForEachLiveMonster(DrawMonsterDot, &context);
	ForEachLiveNpc(DrawNpcDot, &context);

	float player_x = 0.f;
	float player_y = 0.f;
	ProjectWorld(GetPlayer().m_pos, &player_x, &player_y);

	renderer->DrawSolidBox(player_x, player_y, 9.f, 9.f, 0.02f, 0.02f, 0.02f, 1.f);
	renderer->DrawSolidBox(player_x, player_y, 6.f, 6.f, 1.f, 1.f, 1.f, 1.f);
}
