#pragma once

#include "Renderer.h"
#include "SpriteId.h"

bool InitSpriteAtlas();
void ShutdownSpriteAtlas();

int32 GetSpriteWidth(int32 sprite);	// 도트 칸 수
int32 GetSpriteHeight(int32 sprite);

// 발밑 가운데 기준. scale 1 이면 도트 한 칸 = 저해상도 한 픽셀
void DrawSprite(Renderer* renderer, int32 sprite, int32 frame, float foot_x, float foot_y, float scale, bool flip, float r, float g, float b, float a);

// 회전된 칼, 기울인 그림자
void DrawSpriteQuad(Renderer* renderer, int32 sprite, int32 frame, const AffineQuad& quad, bool flip, float r, float g, float b, float a);
