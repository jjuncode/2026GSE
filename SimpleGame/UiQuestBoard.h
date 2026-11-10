#pragma once

class Renderer;

// 게시판 창의 의뢰 탭. 왼쪽에 붙은 의뢰 열 개, 오른쪽에 고른 의뢰 자세히와 수락 단추
struct BoardRect
{
	float left = 0.f;
	float right = 0.f;
	float top = 0.f;	// 목록이 시작하는 높이
	float bottom = 0.f;	// 목록이 끝나는 높이
};

void ResetQuestBoardSelection();
void DrawQuestBoardTab(Renderer* renderer, int32 village, const BoardRect& rect, float mouse_x, float mouse_y);
bool HandleQuestBoardDown(int32 village, const BoardRect& rect, float x, float y);
