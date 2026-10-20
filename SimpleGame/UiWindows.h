#pragma once

class Renderer;

constexpr int32 UI_MOUSE_LEFT = 0;	// glut 버튼 번호와 같다
constexpr int32 UI_MOUSE_MIDDLE = 1;
constexpr int32 UI_MOUSE_RIGHT = 2;

// 가방 · 능력치 · 스킬 트리 · 강화 창과 파괴 확인
void ToggleInventoryWindow();
void ToggleStatWindow();
void ToggleSkillWindow();
void ToggleEnhanceWindow();	// 모루 근처에서만 열린다
void OpenEnhanceWindow();
bool CloseTopWindow();	// 닫은 게 있으면 true
bool IsAnyWindowOpen();

// 좌표는 창 픽셀, 가운데 원점, 위가 양수. 창이 먹었으면 true 라 게임 쪽으로 안 넘긴다
bool UiMouseDown(int32 button, float x, float y);
bool UiMouseUp(int32 button, float x, float y);
void UiMouseMove(float x, float y);
bool UiKeyEnter();	// 강화 창이면 강화, 확인 창이면 예

void UpdateUiWindows(float real_seconds);
void DrawUiWindows(Renderer* renderer);
