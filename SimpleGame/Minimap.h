#pragma once

class Renderer;
struct ZoneLayout;

// 구역 칸 색을 텍스처 한 장으로 굽는다. GL 문맥이 선 뒤, 구역을 만들 때마다 부른다
void BuildMinimap(const ZoneLayout& layout);
void ShutdownMinimap();

// M 키 대륙지도. 구석 미니맵은 늘 떠 있고 대륙지도가 열리면 숨는다
void ToggleWorldMap();
bool IsWorldMapOpen();
float GetMinimapBottom();	// 구석 미니맵 아래변 ( 창 좌표 ). 의뢰 목록이 이 아래에 선다

// HUD 에서 부른다. 다른 사람과 몬스터는 안 찍는다 ( G 디버그일 때 대륙지도만 )
void DrawMinimap(Renderer* renderer, bool hidden);
void DrawWorldMap(Renderer* renderer);
