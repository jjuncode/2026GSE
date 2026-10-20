#pragma once

#include "Combat.h"

#include <string>

class Renderer;

// 구역 배치와 NPC 가 쓰는 종류 번호. MonsterData.json 의 monster_type 은 이 값 + 1
constexpr int32 MONSTER_BLIGHTED = 0;	// 타락자 - 칼 든 사람 ( 서버 Mask Raider 의 기술 넷 )
constexpr int32 MONSTER_HOUND = 1;	// 재늑대 - 빠르고 약하다
constexpr int32 MONSTER_DEVOURER = 2;	// 소거식귀 - 정예. 등이 급소, 붙잡기는 못 막는다
constexpr int32 MONSTER_GATEBREAKER = 3;	// 성문파쇄자 - 보스

// NPC 가 잡았을 때 받는 것
struct MonsterReward
{
	int32 exp = 0;
	int32 gold = 0;
	int32 trophy = 0;	// 전리품 값어치 ( 골드 기준 )
};

// 머리 위 대상 표시
struct MonsterFocus
{
	std::wstring name;
	int32 level = 1;
	float hp_ratio = 0.f;
	int32 grade = GRADE_NORMAL;
	bool is_elite = false;
	bool is_groggy = false;	// 패링 브레이크 · 스턴
};

void ClearMonsters();
void AddMonster(int32 kind, const WorldPos& pos, int32 band, bool is_elite);

// 보스는 자리만 잡아 두고, 해금 조건을 채울 때 불러낸다
void SetBossSpawnPoint(const WorldPos& pos);
bool IsBossSummoned();
void SummonBoss();

void UpdateMonsters(float world_seconds);

void CollectMonsterActors();
void DrawMonsterTelegraphs(Renderer* renderer, const WorldPos& camera);	// 바닥 판정 예고
void DrawMonsterBars(Renderer* renderer, const WorldPos& camera);	// 머리 위 체력과 그로기

bool GetBossPos(WorldPos* pos);	// 불려 나왔고 아직 살아 있을 때만 true
bool GetBossHpRatio(float* ratio);	// 보스가 싸우는 중일 때만 true
bool IsBossDead();
const wchar_t* GetBossName();

bool GetFocusMonster(MonsterFocus* focus);	// 최근에 때린 몬스터

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

// 지도에 찍을 산 몬스터 위치
void ForEachLiveMonster(void (*visit)(const WorldPos& pos, int32 kind, bool is_elite, void* context), void* context);

// 브라우저 모니터 창구
struct MonsterReport
{
	WorldPos pos;
	WorldPos home;	// 처음 선 자리. 다시 나오길 기다리는 동안은 이 거점 몫으로 센다
	int32 kind = MONSTER_BLIGHTED;
	bool is_elite = false;
	bool is_alive = false;
	bool is_fighting = false;	// 쫓거나 때리는 중
};

bool GetMonsterReport(int32 index, MonsterReport* report);
