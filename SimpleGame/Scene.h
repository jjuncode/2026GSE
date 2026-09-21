#pragma once

#include "WorldPos.h"

class Renderer;

void InitScene();
void RegenerateZone();	// 성장은 두고 새 씨앗으로 구역만 다시 만든다
void UpdateScene(float delta_seconds);
void DrawScene(Renderer* renderer);

// 후처리 뒤 저해상도 버퍼에 얹는 것. 체력 막대, 재료 이름표, 떠오르는 글자
void DrawSceneOverlay(Renderer* renderer);

WorldPos GetCameraPos();
void MovePlayerByScreenAxis(float screen_right, float screen_up, float delta_seconds);
void MovePlayerByWorldAxis(float world_x, float world_y, float delta_seconds);
void PickupMaterials();
void ToggleChunkGrid();
void ToggleLighting();
