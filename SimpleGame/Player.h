#pragma once

#include "Actor.h"

#include <array>
#include <string>

constexpr int32 STAT_MAX_HP = 0;
constexpr int32 STAT_ATTACK = 1;
constexpr int32 STAT_DEFENCE = 2;
constexpr int32 STAT_COUNT = 3;

constexpr int32 MATERIAL_DUST = 0;	// 재가루
constexpr int32 MATERIAL_FRAGMENT = 1;	// 푸른 마석
constexpr int32 MATERIAL_CORE = 2;	// 심장석
constexpr int32 MATERIAL_COUNT = 3;

// 몸을 그린 뒤 손에 쥔 칼을 이어 그린다
class PlayerActor : public Actor
{
public:
	void Draw(Renderer* renderer, const WorldPos& camera) const override;
};

struct PlayerState
{
	PlayerActor body;
	WorldPos spawn_pos;

	float facing_x = 1.f;	// 월드 방향 ( 길이 1 )
	float facing_y = 0.f;
	float walk_distance = 0.f;	// 걸은 거리로 걸음 프레임을 넘긴다
	float idle_timer = 0.f;

	int32 hp = 0;
	int32 level = 1;
	int32 exp = 0;	// 현재 레벨에서 쌓은 경험치
	int32 stat_point = 0;
	std::array<int32, STAT_COUNT> stats = {};

	int32 gold = 0;
	std::array<int32, MATERIAL_COUNT> materials = {};
	int32 enhance_level = 0;

	float swing_cooldown = 0.f;
	float swing_timer = 0.f;	// 휘두르는 연출 남은 시간
	float hit_flash = 0.f;
	float since_hit = 0.f;	// 마지막 피격 뒤 흐른 시간
	float regen_carry = 0.f;	// 1 이 안 되는 회복 누적
	float respawn_timer = 0.f;	// 0 보다 크면 죽어서 부활 대기
};

void InitPlayer(const WorldPos& spawn_pos);
void PlacePlayer(const WorldPos& spawn_pos);	// 성장은 두고 자리만 옮긴다
const PlayerState& GetPlayer();
PlayerState& MutablePlayer();

bool IsPlayerDead();
void UpdatePlayer(float delta_seconds);
void MovePlayer(float velocity_x, float velocity_y, float delta_seconds);	// 초당 칸
void AimPlayer(float world_dir_x, float world_dir_y);
bool TryStartSwing();	// 쿨이 돌았으면 휘두르기 시작
void DamagePlayer(int32 damage);
int32 TakePlayerGold(int32 amount);	// 실제로 빼앗긴 양

int32 GetPlayerMaxHp();
int32 GetPlayerAttack();
int32 GetPlayerDefence();

int32 GetNeedExp(int32 level);
void AddPlayerExp(int32 exp);
bool TryStatUp(int32 stat);	// 잔여 포인트 없으면 false

const wchar_t* GetMaterialName(int32 material);
const wchar_t* GetStatName(int32 stat);
