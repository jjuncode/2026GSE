#pragma once

#include "Actor.h"

class Renderer;

constexpr int32 MONSTER_BLIGHTED = 0;	// 타락자 - 느리고 많다
constexpr int32 MONSTER_HOUND = 1;	// 재늑대 - 빠르고 약하다
constexpr int32 MONSTER_DEVOURER = 2;	// 소거식귀 - 느리고 한 방이 크다. 등이 급소
constexpr int32 MONSTER_GATEBREAKER = 3;	// 성문파쇄자 - 영역 지배종

// NPC 가 잡았을 때 받는 것
struct MonsterReward
{
	int32 exp = 0;
	int32 gold = 0;
	int32 trophy = 0;	// 전리품 값어치 ( 골드 기준 )
};

void ClearMonsters();
void AddMonster(int32 kind, const WorldPos& pos, int32 band, bool is_elite);

// 보스는 자리만 잡아 두고, 해금 조건을 채울 때 불러낸다
void SetBossSpawnPoint(const WorldPos& pos);
bool IsBossSummoned();
void SummonBoss();

void UpdateMonsters(float delta_seconds);

// 부채꼴 안의 산 몹을 전부 때린다. 맞은 수
int32 HitMonstersInFan(const WorldPos& origin, float dir_x, float dir_y, float range, float half_cos, int32 damage);

// NPC 쪽 창구. 번호는 구역을 다시 만들 때까지 안 바뀐다
int32 GetMonsterCount();
bool IsMonsterAlive(int32 index);
WorldPos GetMonsterPos(int32 index);
int32 GetMonsterKind(int32 index);
int32 GetMonsterThreat(int32 index);	// 공격력 x 남은 체력. 사냥감 고를 때
int32 FindNearestMonster(const WorldPos& pos, float range, float* distance);	// 없으면 -1
bool DamageMonsterFromNpc(int32 index, int32 damage, int32 npc_index, const WorldPos& attacker_pos, MonsterReward* reward);	// 죽였으면 true
void ForgetMonsterTargetNpc(int32 npc_index);
const wchar_t* GetMonsterKindName(int32 kind);

void CollectMonsterActors();
void DrawMonsterTelegraphs(Renderer* renderer, const WorldPos& camera);	// 바닥 판정 표시
void DrawMonsterBars(Renderer* renderer, const WorldPos& camera);	// 머리 위 체력

bool GetBossPos(WorldPos* pos);	// 불려 나왔고 아직 살아 있을 때만 true
bool GetBossHpRatio(float* ratio);	// 보스가 싸우는 중일 때만 true
bool IsBossDead();
const wchar_t* GetBossName();

// 지도에 찍을 산 몬스터 위치
void ForEachLiveMonster(void (*visit)(const WorldPos& pos, int32 kind, bool is_elite, void* context), void* context);
