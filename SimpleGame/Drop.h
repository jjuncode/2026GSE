#pragma once

#include "Actor.h"
#include "GameData.h"

class Renderer;

constexpr int32 DROP_TIER_NORMAL = 0;
constexpr int32 DROP_TIER_ELITE = 1;	// 재불 정예와 거식귀
constexpr int32 DROP_TIER_BOSS = 2;

void ClearDrops();

// 몬스터 보상. 드랍 표를 줄마다 roll_count 번 굴린다 ( 서버 GrantKillReward )
void SpawnRewardDrops(const WorldPos& pos, const MonsterRewardData& reward, int32 gold, int32 roll_count);
void SpawnItemDrop(const WorldPos& pos, int32 item_id, int32 count);
void SpawnGoldDrop(const WorldPos& pos, int32 gold);

// NPC 쪽 창구 ( 벤 약탈자 ). 경험치는 바로 주고 돈과 흔적을 떨군다
void SpawnMonsterDrops(const WorldPos& pos, int32 exp, int32 gold_min, int32 gold_max, int32 tier);

// 골드는 끌려와서 저절로 먹힌다. 아이템은 줍기 키로만 ( 앞에서 멈칫하는 순간을 남기려고 )
void UpdateDrops(float world_seconds);
bool PickupNearestDrop();
bool HasPickupNear();

void CollectDropActors();

// 땅에 떨어진 아이템 이름표. 후처리 뒤에 부른다
void DrawDropLabels(Renderer* renderer, const WorldPos& camera);
