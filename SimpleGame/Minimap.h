#pragma once

class Renderer;
struct ZoneLayout;

// 구역 칸 색을 텍스처 한 장으로 굽는다. GL 문맥이 선 뒤, 구역을 만들 때마다 부른다
void BuildMinimap(const ZoneLayout& layout);
void ShutdownMinimap();

void ToggleMinimap();

// HUD 에서 부른다. NPC 와 몬스터 점은 매 프레임 새로 찍는다
void DrawMinimap(Renderer* renderer, bool hidden);
