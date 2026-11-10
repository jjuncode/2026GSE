#include "stdafx.h"
#include "Minimap.h"
#include "Monster.h"
#include "Npc.h"
#include "NpcData.h"
#include "Player.h"
#include "Quest.h"
#include "Renderer.h"
#include "Scene.h"
#include "Text.h"
#include "UiDraw.h"
#include "Village.h"
#include "ZoneGen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
	// 칸 하나가 화면에서 차지하는 반폭 ( 픽셀 ). 아이소라 세로는 이 절반
	const float WORLD_SPAN = 1.5f;	// 352 칸 대각이 1056 픽셀. 창 1280 안에 들어간다
	const float WORLD_CENTER_Y = -6.f;
	const float MINI_SPAN = 2.6f;	// 구석 지도 폭 220 픽셀에 앞뒤 마흔 칸 남짓
	const float MINI_WIDTH = 220.f;
	const float MINI_HEIGHT = 150.f;
	const float MINI_MARGIN = 16.f;

	GLuint g_texture = 0;
	bool g_world_open = false;
	std::vector<ZoneSite> g_sites;

	// 지도 한 장을 어디에 어떤 배율로 펼치나. 대륙지도는 구역 한가운데, 구석 지도는 나를 가운데에 둔다
	struct MapView
	{
		float screen_x = 0.f;
		float screen_y = 0.f;
		float origin_x = 0.f;	// 화면 가운데에 오는 칸
		float origin_y = 0.f;
		float span = 1.f;
	};

	struct DotContext
	{
		Renderer* renderer = nullptr;
		const MapView* view = nullptr;
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
	void ProjectTile(const MapView& view, float tile_x, float tile_y, float* screen_x, float* screen_y)
	{
		const float offset_x = tile_x - view.origin_x;
		const float offset_y = tile_y - view.origin_y;

		*screen_x = view.screen_x + (offset_x - offset_y) * view.span;
		*screen_y = view.screen_y - (offset_x + offset_y) * view.span * 0.5f;
	}

	float GetZoneTileX(const WorldPos& pos)
	{
		return static_cast<float>((pos.chunk_x + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_x;
	}

	float GetZoneTileY(const WorldPos& pos)
	{
		return static_cast<float>((pos.chunk_y + ZONE_CHUNK_RADIUS) * CHUNK_TILE_COUNT) + pos.local_y;
	}

	void ProjectWorld(const MapView& view, const WorldPos& pos, float* screen_x, float* screen_y)
	{
		ProjectTile(view, GetZoneTileX(pos), GetZoneTileY(pos), screen_x, screen_y);
	}

	MapView MakeWorldView()
	{
		MapView view;
		view.screen_x = 0.f;
		view.screen_y = WORLD_CENTER_Y;
		view.origin_x = static_cast<float>(ZONE_TILE_COUNT) * 0.5f;
		view.origin_y = static_cast<float>(ZONE_TILE_COUNT) * 0.5f;
		view.span = WORLD_SPAN;

		return view;
	}

	MapView MakeMiniView()
	{
		const WorldPos& player = GetPlayer().m_pos;

		MapView view;
		view.screen_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f - MINI_MARGIN - MINI_WIDTH * 0.5f;
		view.screen_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - MINI_MARGIN - MINI_HEIGHT * 0.5f;
		view.origin_x = GetZoneTileX(player);
		view.origin_y = GetZoneTileY(player);
		view.span = MINI_SPAN;

		return view;
	}

	// 디버그( G )일 때만 대륙지도에 사람과 몬스터를 찍는다. 평소엔 숨긴다 ( 팀 일정 - 다른 사람 · 몬스터 안 보임 )
	void DrawNpcDot(const WorldPos& pos, int32 job, bool is_outlaw, bool is_selected, void* context)
	{
		const DotContext& dots = *static_cast<DotContext*>(context);

		float x = 0.f;
		float y = 0.f;
		ProjectWorld(*dots.view, pos, &x, &y);

		float r = 0.35f;
		float g = 0.95f;
		float b = 0.88f;
		if (is_outlaw)
		{
			r = 1.f;
			g = 0.3f;
			b = 0.25f;
		}

		dots.renderer->DrawSolidBox(x, y, 4.f, 4.f, r, g, b, 1.f);
	}

	void DrawMonsterDot(const WorldPos& pos, int32 kind, bool is_elite, void* context)
	{
		const DotContext& dots = *static_cast<DotContext*>(context);

		float x = 0.f;
		float y = 0.f;
		ProjectWorld(*dots.view, pos, &x, &y);

		float size = 3.f;
		if (is_elite || kind == MONSTER_GATEBREAKER)
		{
			size = 6.f;
		}

		dots.renderer->DrawSolidBox(x, y, size, size, 0.75f, 0.12f, 0.08f, 1.f);
	}

	void DrawMapTexture(Renderer* renderer, const MapView& view, float alpha)
	{
		const float span = static_cast<float>(ZONE_TILE_COUNT) * view.span;

		// 텍스처 u 가 칸 x, v 가 칸 y. 두 축을 아이소 방향으로 눕히면 지도가 화면과 같은 각도로 선다
		AffineQuad quad;
		ProjectTile(view, 0.f, 0.f, &quad.origin_x, &quad.origin_y);
		quad.axis_x_x = span;
		quad.axis_x_y = -span * 0.5f;
		quad.axis_y_x = -span;
		quad.axis_y_y = -span * 0.5f;

		TexRegion region;
		renderer->DrawAffine(g_texture, region, quad, 1.f, 1.f, 1.f, alpha);
	}

	void DrawSiteMarks(Renderer* renderer, const MapView& view, bool with_labels)
	{
		for (const ZoneSite& site : g_sites)
		{
			float x = 0.f;
			float y = 0.f;
			ProjectTile(view, static_cast<float>(site.tile.x) + 0.5f, static_cast<float>(site.tile.y) + 0.5f, &x, &y);

			switch (site.kind)
			{
				case SITE_OUTPOST:
				{
					renderer->DrawSolidBox(x, y, 9.f, 9.f, 0.02f, 0.02f, 0.02f, 1.f);
					renderer->DrawSolidBox(x, y, 6.f, 6.f, 0.20f, 0.65f, 0.60f, 1.f);
					break;
				}
				case SITE_HIDEOUT:
				{
					renderer->DrawSolidBox(x, y, 9.f, 9.f, 0.02f, 0.02f, 0.02f, 1.f);
					renderer->DrawSolidBox(x, y, 6.f, 6.f, 0.75f, 0.12f, 0.10f, 1.f);
					if (with_labels)
					{
						DrawStringShadow(renderer, L"소굴", x, y + 26.f, TEXT_ALIGN_CENTER, 0.85f, 0.4f, 0.35f, 0.9f);
					}

					break;
				}
				case SITE_VILLAGE:
				{
					renderer->DrawSolidBox(x, y, 6.f, 6.f, 0.60f, 0.58f, 0.50f, 1.f);
					break;
				}
				case SITE_LAIR:
				{
					renderer->DrawSolidCircle(x, y, 6.f, 0.02f, 0.02f, 0.02f, 1.f);
					renderer->DrawSolidCircle(x, y, 4.f, 0.85f, 0.15f, 0.75f, 1.f);
					if (with_labels)
					{
						DrawStringShadow(renderer, L"둥지", x, y + 26.f, TEXT_ALIGN_CENTER, 0.9f, 0.5f, 0.85f, 0.9f);
					}

					break;
				}
				case SITE_TOWN:
				{
					// 일곱 마을은 이름까지. 어디로 가는 길인지가 지도에서 먼저 읽혀야 한다
					renderer->DrawSolidBox(x, y, 12.f, 12.f, 0.02f, 0.02f, 0.02f, 1.f);
					renderer->DrawSolidBox(x, y, 8.f, 8.f, 1.f, 0.78f, 0.38f, 1.f);
					DrawStringShadow(renderer, GetVillageName(site.village), x, y + 30.f, TEXT_ALIGN_CENTER, 1.f, 0.88f, 0.66f, 0.95f);
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}

	// 받은 의뢰가 갈 곳. 안내 중인 것은 크게. 구석 지도에선 밖에 있으면 테두리에 붙인다
	void DrawQuestMarks(Renderer* renderer, const MapView& view, bool clamp_to_mini)
	{
		const std::vector<Quest>& quests = GetActiveQuests();
		const int32 tracked = GetTrackedQuest();
		const float half_width = MINI_WIDTH * 0.5f - 8.f;
		const float half_height = MINI_HEIGHT * 0.5f - 8.f;

		for (size_t index = 0; index < quests.size(); ++index)
		{
			WorldPos guide;
			if (GetQuestGuide(quests[index], &guide) == false)
			{
				continue;
			}

			float mark_x = 0.f;
			float mark_y = 0.f;
			ProjectWorld(view, guide, &mark_x, &mark_y);

			if (clamp_to_mini)
			{
				mark_x = std::min(std::max(mark_x, view.screen_x - half_width), view.screen_x + half_width);
				mark_y = std::min(std::max(mark_y, view.screen_y - half_height), view.screen_y + half_height);
			}

			float size = 0.32f;
			if (static_cast<int32>(index) == tracked)
			{
				size = 0.5f;
			}

			renderer->DrawSolidDiamond(mark_x, mark_y, size + 0.12f, 0.05f, 0.03f, 0.01f, 1.f);
			renderer->DrawSolidDiamond(mark_x, mark_y, size, 1.f, 0.82f, 0.3f, 1.f);
		}
	}

	// 흰 점 위에 보는 쪽을 가리키는 짧은 막대
	void DrawPlayerMark(Renderer* renderer, const MapView& view)
	{
		const Player& player = GetPlayer();

		float x = 0.f;
		float y = 0.f;
		ProjectWorld(view, player.m_pos, &x, &y);

		const float dir_x = player.m_face_x - player.m_face_y;
		const float dir_y = -(player.m_face_x + player.m_face_y) * 0.5f;
		const float length = std::max(std::sqrt(dir_x * dir_x + dir_y * dir_y), 0.001f);

		DrawLine(renderer, x, y, x + dir_x / length * 11.f, y + dir_y / length * 11.f, 3.f, 1.f, 1.f, 1.f, 1.f);
		renderer->DrawSolidBox(x, y, 9.f, 9.f, 0.02f, 0.02f, 0.02f, 1.f);
		renderer->DrawSolidBox(x, y, 6.f, 6.f, 1.f, 1.f, 1.f, 1.f);
	}

	// 백버퍼 좌표로 자른다. 창 좌표는 가운데 원점 · 위가 양수라 아래 왼쪽 원점으로 옮긴다
	void BeginClip(float center_x, float center_y, float width, float height)
	{
		FlushRenderBatch();

		const float left = center_x - width * 0.5f + static_cast<float>(WINDOW_SIZE_X) * 0.5f;
		const float bottom = center_y - height * 0.5f + static_cast<float>(WINDOW_SIZE_Y) * 0.5f;

		glEnable(GL_SCISSOR_TEST);
		glScissor(static_cast<GLint>(left), static_cast<GLint>(bottom), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	}

	void EndClip()
	{
		FlushRenderBatch();
		glDisable(GL_SCISSOR_TEST);
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

void ToggleWorldMap()
{
	g_world_open = !g_world_open;
}

bool IsWorldMapOpen()
{
	return g_world_open;
}

float GetMinimapBottom()
{
	return static_cast<float>(WINDOW_SIZE_Y) * 0.5f - MINI_MARGIN - MINI_HEIGHT;
}

void DrawMinimap(Renderer* renderer, bool hidden)
{
	if (hidden || g_world_open || g_texture == 0)
	{
		return;
	}

	const MapView view = MakeMiniView();

	// 틀과 바탕. 지도 밖( 구역 바깥 )은 균열처럼 검다
	renderer->DrawSolidBox(view.screen_x, view.screen_y, MINI_WIDTH + 6.f, MINI_HEIGHT + 6.f, 0.26f, 0.21f, 0.14f, 1.f);
	renderer->DrawSolidBox(view.screen_x, view.screen_y, MINI_WIDTH, MINI_HEIGHT, 0.03f, 0.03f, 0.035f, 1.f);

	BeginClip(view.screen_x, view.screen_y, MINI_WIDTH, MINI_HEIGHT);
	DrawMapTexture(renderer, view, 0.95f);
	DrawSiteMarks(renderer, view, false);
	DrawPlayerMark(renderer, view);
	EndClip();

	// 의뢰 표식은 자르지 않고 테두리에 붙인다. 밖에 있어도 방향은 보여야 한다
	DrawQuestMarks(renderer, view, true);

	const int32 village = GetVillageAt(GetPlayer().m_pos);
	std::wstring place = L"벽 밖";
	if (village >= 0)
	{
		place = GetVillageName(village);
	}

	DrawStringShadow(renderer, place + L"   M 대륙지도", view.screen_x, view.screen_y - MINI_HEIGHT * 0.5f - 4.f, TEXT_ALIGN_CENTER, 0.78f, 0.72f, 0.6f, 1.f);
}

void DrawWorldMap(Renderer* renderer)
{
	if (g_world_open == false || g_texture == 0)
	{
		return;
	}

	const MapView view = MakeWorldView();
	const float top = static_cast<float>(WINDOW_SIZE_Y) * 0.5f;
	const float bottom = -top;

	renderer->DrawSolidBox(0.f, 0.f, static_cast<float>(WINDOW_SIZE_X), static_cast<float>(WINDOW_SIZE_Y), 0.01f, 0.01f, 0.012f, 0.82f);

	DrawMapTexture(renderer, view, 1.f);
	DrawSiteMarks(renderer, view, true);

	if (IsChunkGridShown())
	{
		DotContext context;
		context.renderer = renderer;
		context.view = &view;

		ForEachLiveMonster(DrawMonsterDot, &context);
		ForEachLiveNpc(DrawNpcDot, &context);
	}

	DrawQuestMarks(renderer, view, false);
	DrawPlayerMark(renderer, view);

	DrawStringOutline(renderer, L"대륙지도", 0.f, top - 18.f, TEXT_ALIGN_CENTER, 1, 0.96f, 0.86f, 0.62f, 1.f);
	DrawStringShadow(renderer, L"흰 점  나      금빛 마름모  받은 의뢰 ( T 로 안내 )", 0.f, bottom + 54.f, TEXT_ALIGN_CENTER, 0.7f, 0.66f, 0.58f, 1.f);
	DrawStringShadow(renderer, L"노란 네모  마을    청록  초소    붉은  소굴    보라  둥지      M  닫기", 0.f, bottom + 28.f, TEXT_ALIGN_CENTER, 0.6f, 0.56f, 0.5f, 1.f);
}
