#pragma once

class Renderer;

// 글자가 없으므로 전부 도형으로 낸다. 폰트는 6단계 뒤에 얹는다
void UpdateHud(float delta_seconds);
void DrawHud(Renderer* renderer);

void ToggleStatWindow();
void ToggleEnhanceWindow();
void RequestEnhance();	// 강화 창이 열려 있을 때만 시도
bool HandleHudClick(float screen_x, float screen_y);	// 창이 클릭을 먹었으면 true
