#pragma once

#include "Actor.h"

class Renderer;

void ClearDrops();
constexpr int32 DROP_TIER_NORMAL = 0;
constexpr int32 DROP_TIER_ELITE = 1;	// 재불 정예와 거식귀
constexpr int32 DROP_TIER_BOSS = 2;

void SpawnMonsterDrops(const WorldPos& pos, int32 exp, int32 gold_min, int32 gold_max, int32 tier);
void SpawnGoldDrop(const WorldPos& pos, int32 gold);	// 벤 약탈자가 들고 있던 돈

// 경험치와 돈은 끌려와서 저절로 먹힌다
void UpdateDrops(float delta_seconds);

// 재료는 줍기 키로만 먹는다. 주운 개수
int32 PickupNearbyMaterials();

void CollectDropActors();

// 땅에 떨어진 재료 이름표. 후처리 뒤에 부른다
void DrawDropLabels(Renderer* renderer, const WorldPos& camera);
