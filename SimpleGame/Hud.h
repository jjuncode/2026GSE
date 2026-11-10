#pragma once

class Renderer;

// 늘 떠 있는 전투 화면. 체력 · 가드 · 행동 칸 · 경험치 · 대상 · 보스 · 알림
void UpdateHud(float real_seconds);
void DrawHud(Renderer* renderer);

void ToggleKeyHelp();

// 패링으로 공격 스킬 쿨이 깎였다. 그 칸들이 잠깐 번쩍인다
void NotifyCooldownCut();

// 죽어서 잃은 경험치. 쓰러진 화면에 띄운다
void NotifyExpLost(int32 amount);
