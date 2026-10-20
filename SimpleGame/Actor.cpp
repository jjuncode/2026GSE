#include "stdafx.h"
#include "Actor.h"
#include "GameConfig.h"
#include "Renderer.h"
#include "SpriteAtlas.h"

namespace
{
	// 해가 오른쪽 위에 있어 그림자는 왼쪽 아래로 눕는다 ( 참고 이미지 )
	const float SHADOW_LEAN_X = -0.62f;
	const float SHADOW_LEAN_Y = -0.30f;
	const float SHADOW_ALPHA = 0.32f;
}

Actor::~Actor()
{
}

void Actor::Draw(Renderer* renderer, const WorldPos& camera) const
{
	const ScreenPos screen = WorldToScreen(m_pos, camera);
	const float glow = m_shade * m_emissive;

	if (m_sheet != SHEET_NONE)
	{
		DrawSheetFrame(renderer, m_sheet, m_frame, screen.x, screen.y + m_foot_offset_y, glow, glow, glow, 1.f, 0.f);
		return;
	}

	DrawSprite(renderer, m_sprite, m_frame, screen.x, screen.y + m_foot_offset_y, 1.f, m_flip, glow, glow, glow, 1.f);
}

float Actor::GetArtWidth() const
{
	if (m_sheet != SHEET_NONE)
	{
		return GetSheetBodyWidth(m_sheet);
	}

	return static_cast<float>(GetSpriteWidth(m_sprite));
}

float Actor::GetArtHeight() const
{
	if (m_sheet != SHEET_NONE)
	{
		return GetSheetBodyHeight(m_sheet);
	}

	return static_cast<float>(GetSpriteHeight(m_sprite));
}

void Actor::DrawShadow(Renderer* renderer, const WorldPos& camera) const
{
	const ScreenPos screen = WorldToScreen(m_pos, camera);

	if (m_sheet != SHEET_NONE)
	{
		DrawSheetShadow(renderer, m_sheet, m_frame, screen.x, screen.y, SHADOW_LEAN_X, SHADOW_LEAN_Y, SHADOW_ALPHA);
		return;
	}

	const float unit = static_cast<float>(PIXEL_SCALE);
	const float width = static_cast<float>(GetSpriteWidth(m_sprite)) * unit;
	const float height = static_cast<float>(GetSpriteHeight(m_sprite)) * unit;

	// 실루엣이 그대로라 야자수 그림자는 야자수 모양이다
	AffineQuad quad;
	quad.origin_x = screen.x - width * 0.5f;
	quad.origin_y = screen.y;
	quad.axis_x_x = width;
	quad.axis_y_x = height * SHADOW_LEAN_X;
	quad.axis_y_y = height * SHADOW_LEAN_Y;

	DrawSpriteQuad(renderer, m_sprite, m_frame, quad, m_flip, 0.f, 0.f, 0.f, SHADOW_ALPHA);
}
