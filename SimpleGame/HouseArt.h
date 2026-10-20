#pragma once

class Renderer;

// 여러 칸짜리 집을 한 채로 구워 칸마다 나눈 조각 ( tools/art/bake_houses.py ).
// 칸마다 깊이 정렬해 그려도 지붕 하나로 맞물린다. 없으면 옛 칸 블록으로 그린다
bool InitHouseArt();
void ShutdownHouseArt();
bool HasHouseArt();

int32 FindHousePiece(int32 size_x, int32 size_y, int32 door_col, int32 roof, int32 tile_x, int32 tile_y);	// 없으면 -1
int32 GetHouseRoofCount();
void DrawHousePiece(Renderer* renderer, int32 piece, float foot_x, float foot_y, float r, float g, float b);

// 같은 그림판에 실린 마을 소품 ( 게시판 ). 발밑은 아래변 가운데
int32 FindHouseProp(const char* name);	// 없으면 -1
void DrawHouseProp(Renderer* renderer, int32 prop, float foot_x, float foot_y, float r, float g, float b);
