#pragma once

#include "UiDraw.h"

#include <vector>

class Renderer;

// 상점 창. 여닫기와 겹침 순서는 UiWindows 가 하고 여기는 그리기와 누르기만 한다
bool IsInsideShopWindow(float x, float y);
void DrawShopWindow(Renderer* renderer, int32 shop_kind, float mouse_x, float mouse_y, std::vector<TooltipLine>* tooltip);
bool HandleShopDown(int32 shop_kind, float x, float y);

// 가방에서 판 결과를 창 아래 줄에 띄운다
void NotifyShopSold(int32 result, int32 gold);
