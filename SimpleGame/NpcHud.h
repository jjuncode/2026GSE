#pragma once

#include "WorldPos.h"

class Renderer;

// 머리 위 일과 표시와 체력. 후처리 뒤 오버레이에서 부른다
void DrawNpcOverlay(Renderer* renderer, const WorldPos& camera);

// 고른 NPC 관찰창과 변경 일지. HUD 에서 부른다
void DrawNpcPanels(Renderer* renderer, bool hide_inspector);

void CycleNpcLabelMode();	// 끔 / 일과 / 이름과 일과
void ToggleNpcJournal();
