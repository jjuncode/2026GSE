#pragma once

#include "Actor.h"

class Renderer;

void ClearMonsters();
void AddZombie(const WorldPos& pos);
void AddSkeleton(const WorldPos& pos);
void AddBoss(const WorldPos& pos);

void UpdateMonsters(float delta_seconds);

// 부채꼴 안의 산 몹을 전부 때린다. 맞은 수
int32 HitMonstersInFan(const WorldPos& origin, float dir_x, float dir_y, float range, float half_cos, int32 damage);

void CollectMonsterActors();
void DrawMonsterTelegraphs(Renderer* renderer, const WorldPos& camera);	// 바닥 판정 표시
void DrawMonsterBars(Renderer* renderer, const WorldPos& camera);	// 머리 위 체력

bool GetBossHpRatio(float* ratio);	// 보스가 싸우는 중일 때만 true
bool IsBossDead();
const wchar_t* GetBossName();
