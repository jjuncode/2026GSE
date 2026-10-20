#pragma once

#include "WorldPos.h"

class Renderer;

void InitScene();
void RegenerateZone();	// 성장은 두고 새 씨앗으로 구역만 다시 만든다

// 실제 시간을 받는다. 판정 쪽은 안에서 히트스톱 배율을 곱해 넘긴다
void UpdateScene(float real_seconds);
void DrawScene(Renderer* renderer);

// 후처리 뒤 저해상도 버퍼에 얹는 것. 체력 막대, 아이템 이름표, 떠오르는 글자
void DrawSceneOverlay(Renderer* renderer);

WorldPos GetCameraPos();
WorldPos GetCursorWorldPos();
void Interact();	// F. 가까운 아이템을 줍거나 모루를 연다
void SetInteractHeld(bool held);	// 누르고 있으면 계속 줍는다
bool IsPlayerNearAnvil();
void SelectNpcAtCursor();	// 커서 아래 NPC 를 관찰창에. 없으면 닫는다
void ToggleChunkGrid();
void ToggleLighting();
