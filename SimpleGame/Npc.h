#pragma once

#include "WorldPos.h"

class Renderer;
struct ZoneLayout;

// 벽 밖의 사람들. 구역을 새로 만들 때마다 다시 채운다
void BuildNpcs(const ZoneLayout& layout);
void UpdateNpcs(float delta_seconds);

void CollectNpcActors();
void DrawNpcGroundMarks(Renderer* renderer, const WorldPos& camera);	// 고른 NPC 발밑 표식
void DrawNpcArrows(Renderer* renderer, const WorldPos& camera);

// 몬스터 쪽 창구
bool FindNpcForMonster(const WorldPos& pos, float range, int32* npc_index, float* distance);
bool GetNpcPos(int32 npc_index, WorldPos* pos);	// 살아 있을 때만 true
void DamageNpcFromMonster(int32 npc_index, int32 damage, int32 monster_index);

// 플레이어는 약탈자와 무법자만 벨 수 있다. 오폭으로 행상이 죽으면 구경하는 재미가 깨진다
int32 HitHostileNpcsInFan(const WorldPos& origin, float dir_x, float dir_y, float range, float half_cos, int32 damage);

// 관찰
int32 PickNpcAt(const WorldPos& pos, float radius);	// 없으면 -1
void SelectNpc(int32 npc_index);
int32 GetSelectedNpc();
void SelectNextNpc();
void ToggleFollowNpc();
bool GetFollowPos(WorldPos* pos);	// 따라가는 중이고 살아 있으면 true

void ForEachLiveNpc(void (*visit)(const WorldPos& pos, int32 job, bool is_outlaw, bool is_selected, void* context), void* context);
