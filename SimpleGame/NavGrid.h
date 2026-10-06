#pragma once

#include "ZoneGen.h"

#include <vector>

// 구역 칸 막힘 표 위의 A*. NPC 길찾기 전용
void BuildNavGrid(const std::vector<uint8>& tiles);
bool IsNavBlocked(int32 x, int32 y);

// 시작 칸은 빼고 도착 칸까지. 도착이 막혀 있으면 가장 가까운 빈 칸으로 바꿔 찾는다.
// 탐색 상한에 걸리면 목표에 제일 가까이 간 칸까지만 돌려주고 false
bool FindNavPath(const ZoneTile& start, const ZoneTile& goal, std::vector<ZoneTile>* path);

// 몸 사각형이 곧장 걸어갈 수 있나
bool HasNavLine(const WorldPos& from, const WorldPos& to, float radius);

// 틱마다 찾기 횟수를 나눠 준다. 한 틱에 수십 명이 몰리면 그 틱만 튄다
void ResetNavBudget();
bool TakeNavBudget();

int32 PopNavSearchCount();	// 지난 호출 뒤 돌린 탐색 수
