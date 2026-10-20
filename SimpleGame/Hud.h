#pragma once

class Renderer;

// 늘 떠 있는 전투 화면. 체력 · 가드 · 행동 칸 · 경험치 · 대상 · 보스 · 알림
void UpdateHud(float real_seconds);
void DrawHud(Renderer* renderer);

void ToggleKeyHelp();
