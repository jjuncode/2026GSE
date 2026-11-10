#pragma once

#include "WorldPos.h"

#include <string>

class Renderer;
struct ZoneLayout;

// 던전 · 레이드. 구역 구석 하나를 낭떠러지로 두르고 방 여럿으로 판 투기장이다.
// 입구 균열에서 F 로 들어가고, 방을 비울 때마다 다음 방 결계가 걷힌다. 죽거나 나오면 처음 상태로 돌아간다
void CarveInstances(ZoneLayout* layout);	// 구역을 만든 직후. 다른 것이 서기 전에 판다
void BuildInstances();	// 몬스터 · 마을이 선 뒤. 방 몬스터를 꺼 둔 채로 세운다
void UpdateInstances(float real_seconds);

bool InteractInstance();	// F. 입구 · 출구 앞이면 true
bool IsPlayerInInstance();
bool GetInstanceGuide(WorldPos* pos);	// 안에 있을 때 다음에 갈 곳 ( 열린 문 · 출구 ). 싸우는 중이면 false
int32 GetInstanceCount();
bool GetInstanceEntrance(int32 index, WorldPos* pos, std::wstring* name);

void DrawInstanceGround(Renderer* renderer, const WorldPos& camera);	// 균열 · 결계
void DrawInstanceOverlay(Renderer* renderer, const WorldPos& camera);	// 안내 글자와 방 진행
