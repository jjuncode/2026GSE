#pragma once

#include "GameData.h"

class Renderer;

// 모험가 등급. 구역을 새로 만들어도 성장처럼 남는다
int32 GetPlayerRank();
int32 GetRankPoints();	// 지금까지 쌓은 공헌
int32 GetRankCount();
const RankData& GetRankData(int32 rank);
std::wstring GetRankName(int32 rank);

bool IsRankExamReady();	// 다음 등급 공헌을 채웠고 아직 심사를 안 넘겼다
void AddRankPoints(int32 points);
void PromotePlayerRank();	// 승급 심사를 넘겼다

int32 GetTierMinRank(int32 tier);
bool CanAcceptQuestTier(int32 tier);

// 휘장. 아래변 가운데 기준
void DrawRankBadge(Renderer* renderer, int32 rank, float x, float y, float scale);
