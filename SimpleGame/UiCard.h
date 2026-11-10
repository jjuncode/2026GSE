#pragma once

#include "UiDraw.h"

#include <vector>

class Renderer;

// 카드 도감 창 ( Y ). 여닫기는 UiWindows 가 한다
bool IsInsideCardWindow(float x, float y);
void DrawCardWindow(Renderer* renderer, float mouse_x, float mouse_y, std::vector<TooltipLine>* tooltip);
