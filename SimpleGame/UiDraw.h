#pragma once

#include <string>
#include <vector>

class Renderer;
struct ItemSlot;

// 화면 좌표는 창 픽셀, 가운데 원점, 위가 양수. 저해상도 버퍼에 그려 글자 한 점 = 2 픽셀이다
constexpr float UI_LINE = 24.f;	// 굴림 12 x 배율 2
constexpr float UI_SLOT = 40.f;

struct TooltipLine
{
	std::wstring text;
	float r = 0.9f;
	float g = 0.88f;
	float b = 0.82f;
};

bool IsInsideBox(float x, float y, float center_x, float center_y, float width, float height);
float GetUiPulse();	// 0.6 ~ 1 사이를 오가는 숨
void TickUiClock(float real_seconds);

void DrawPanel(Renderer* renderer, float center_x, float center_y, float width, float height, const std::wstring& title);
void DrawFrame(Renderer* renderer, float center_x, float center_y, float width, float height, float thickness, float r, float g, float b, float a);
void DrawBarLeft(Renderer* renderer, float left_x, float center_y, float width, float height, float ratio, float r, float g, float b);
void DrawButton(Renderer* renderer, float center_x, float center_y, float width, float height, const std::wstring& label, bool enabled, bool hovered);
void DrawSlotBox(Renderer* renderer, float center_x, float center_y, float size, bool hovered, bool highlight);
void DrawItemInSlot(Renderer* renderer, const ItemSlot& slot, float center_x, float center_y, float alpha);
void DrawTooltip(Renderer* renderer, float anchor_x, float anchor_y, const std::vector<TooltipLine>& lines);
void DrawLine(Renderer* renderer, float from_x, float from_y, float to_x, float to_y, float width, float r, float g, float b, float a);

void BuildItemTooltip(const ItemSlot& slot, std::vector<TooltipLine>* lines);
