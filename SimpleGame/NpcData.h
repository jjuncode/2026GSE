#pragma once

#include "Actor.h"
#include "BehaviorTree.h"
#include "Board.h"
#include "ZoneGen.h"

#include <array>
#include <string>
#include <vector>

constexpr int32 NPC_JOB_HUNTER = 0;	// 사냥꾼
constexpr int32 NPC_JOB_RAIDER = 1;	// 약탈자 ( 들개단 )
constexpr int32 NPC_JOB_MERCHANT = 2;	// 행상
constexpr int32 NPC_JOB_GATHERER = 3;	// 채집꾼
constexpr int32 NPC_JOB_WARDEN = 4;	// 개척대 순찰병
constexpr int32 NPC_JOB_COURIER = 5;	// 전령. 이웃 마을 게시판에 편지를 나른다
constexpr int32 NPC_JOB_COUNT = 6;

// 일과. 0 ~ 13 이 가중 선택 대상이다
constexpr int32 NPC_TASK_HUNT = 0;
constexpr int32 NPC_TASK_RAID = 1;
constexpr int32 NPC_TASK_SELL = 2;
constexpr int32 NPC_TASK_GATHER = 3;
constexpr int32 NPC_TASK_PEDDLE = 4;
constexpr int32 NPC_TASK_PATROL = 5;
constexpr int32 NPC_TASK_EAT = 6;
constexpr int32 NPC_TASK_REST = 7;
constexpr int32 NPC_TASK_UPGRADE = 8;
constexpr int32 NPC_TASK_SCAVENGE = 9;
constexpr int32 NPC_TASK_WANDER = 10;
constexpr int32 NPC_TASK_READ = 11;	// 게시판 보기. 이사 결심이 여기서 난다
constexpr int32 NPC_TASK_DELIVER = 12;	// 편지 배달
constexpr int32 NPC_TASK_MOVE = 13;	// 이사
constexpr int32 NPC_DAILY_TASK_COUNT = 14;

// 반응층 행동. 라벨로만 쓴다
constexpr int32 NPC_TASK_FIGHT = 14;	// 맞은 자에게 반격
constexpr int32 NPC_TASK_FLEE = 15;
constexpr int32 NPC_TASK_ENGAGE = 16;	// 보이는 적에게 먼저 덤빈다
constexpr int32 NPC_TASK_AVOID = 17;
constexpr int32 NPC_TASK_COUNT = 18;

constexpr int32 NPC_TARGET_NONE = 0;
constexpr int32 NPC_TARGET_MONSTER = 1;
constexpr int32 NPC_TARGET_NPC = 2;
constexpr int32 NPC_TARGET_PLAYER = 3;
constexpr int32 NPC_TARGET_SITE = 4;
constexpr int32 NPC_TARGET_RESOURCE = 5;
constexpr int32 NPC_TARGET_CORPSE = 6;
constexpr int32 NPC_TARGET_SPOT = 7;

// 보이는 적을 어떻게 대할지. 몇 초마다 가중치로 다시 굴린다
constexpr int32 NPC_STANCE_IGNORE = 0;
constexpr int32 NPC_STANCE_ENGAGE = 1;
constexpr int32 NPC_STANCE_AVOID = 2;

constexpr int32 RESOURCE_HERB = 0;
constexpr int32 RESOURCE_RELIC = 1;

constexpr float NPC_BODY_RADIUS = 0.28f;
constexpr int32 NPC_FOOD_PRICE = 4;

struct NpcResource
{
	Actor body;
	ZoneTile tile;
	int32 kind = RESOURCE_HERB;
	int32 value = 0;
	float regrow_timer = 0.f;	// 0 보다 크면 캐인 뒤 다시 자라는 중
};

// 죽은 자리에 남는 주머니
struct NpcCorpse
{
	Actor body;
	int32 id = 0;	// 목록에서 지워지면 순번이 밀려 다른 주머니를 가리킨다. 번호로 찾는다
	int32 gold = 0;
	int32 goods = 0;
	float age = 0.f;
};

struct NpcArrow
{
	WorldPos from;
	WorldPos to;
	float age = 0.f;
};

struct NpcEvent
{
	int32 serial = 0;	// 모니터가 이미 받은 줄을 거른다
	std::wstring text;
	float r = 1.f;
	float g = 1.f;
	float b = 1.f;
	float age = 0.f;
};

struct Npc
{
	Actor body;
	std::wstring name;
	int32 job = NPC_JOB_HUNTER;
	int32 home_site = 0;	// 쉬고 배회할 때 기대는 거점
	int32 home_village = -1;	// 약탈자는 -1
	int32 house = -1;
	bool is_alive = false;
	bool is_inside = false;	// 집 안에서 쉰다. 안 보이고 안 맞는다

	// 소식 나르기. 마을을 나설 때 그 게시판 소식을 챙기고, 다른 마을에 들어서면 푼다
	int32 current_village = -1;
	int32 carry_from = -1;
	int32 carry_via = VIA_RUMOR;
	std::vector<int32> letters;
	int32 deliver_target = -1;	// 전령이 가는 마을
	float village_check_timer = 0.f;

	// 이사. 게시판을 읽고 결심하면 빈집을 잡아 두고 짐을 싼다
	std::array<VillageView, ZONE_VILLAGE_COUNT> opinion;	// 마지막으로 읽은 게시판이 말한 마을 평판
	int32 opinion_board = -1;
	float read_timer = 0.f;	// 게시판을 읽은 뒤 흐른 시간
	float settle_timer = 0.f;	// 0 보다 크면 아직 이사를 안 생각한다
	int32 move_target = -1;
	int32 move_house = -1;
	int32 move_from = -1;	// 처음 오는 사람은 -1
	std::wstring move_reason;

	int32 hp = 0;
	int32 max_hp = 0;
	int32 attack = 0;
	float move_speed = 0.f;
	float attack_range = 0.f;
	float attack_interval = 0.f;
	int32 level = 1;
	int32 exp = 0;
	int32 kills = 0;
	int32 gear = 0;	// 장비 단계

	int32 gold = 0;
	int32 goods = 0;	// 짐 값어치 ( 골드 기준 )
	int32 goods_capacity = 0;

	float hunger = 0.f;	// 1 이면 굶는다

	// 성향 0.5 ~ 1.5. 같은 직업도 성격이 갈린다
	float greed = 1.f;
	float bravery = 1.f;
	float cruelty = 1.f;
	float diligence = 1.f;

	float outlaw_timer = 0.f;	// 0 보다 크면 무법자. 순찰병이 쫓는다

	// 일과 칠판 - 잎 노드가 목표를 적어 두고 다음 잎이 읽는다
	int32 task = NPC_TASK_WANDER;
	int32 target_kind = NPC_TARGET_NONE;
	int32 target_index = -1;
	WorldPos spot;
	float task_timer = 0.f;
	float task_limit = 0.f;
	int32 robbed_total = 0;
	std::wstring prey_title;
	float stall_heartbeat = 0.f;	// 0 보다 크면 노점이 열려 있다

	// 길
	std::vector<ZoneTile> path;
	size_t path_cursor = 0;
	ZoneTile path_goal;
	int32 path_fail_count = 0;
	float progress_timer = 0.f;
	WorldPos progress_pos;
	WorldPos flow_waypoint;	// 마을 사이 먼 길은 거리 지도를 따라간다
	int32 flow_field = -1;
	float flow_timer = 0.f;

	// 싸움
	float attack_cooldown = 0.f;
	float hit_flash = 0.f;
	float since_hit = 99.f;
	float regen_carry = 0.f;
	float starve_carry = 0.f;
	int32 threat_kind = NPC_TARGET_NONE;	// 마지막으로 나를 때린 자
	int32 threat_index = -1;
	float threat_timer = 0.f;
	int32 enemy_kind = NPC_TARGET_NONE;	// 감지한 가장 가까운 적
	int32 enemy_index = -1;
	float enemy_distance = 0.f;
	int32 stance = NPC_STANCE_IGNORE;
	float stance_timer = 0.f;
	float sense_timer = 0.f;

	float walk_distance = 0.f;
	float idle_timer = 0.f;
	float face_x = 1.f;	// 월드 방향. 그림 8 방향을 고른다
	float face_y = 0.f;
	float attack_anim = 0.f;	// 휘두르는 그림 남은 시간
	float work_anim = 0.f;	// 채집 그림 남은 시간

	BtMemory brain;
};

// 세계 ( Npc.cpp )
std::vector<Npc>& GetNpcList();
const std::vector<ZoneSite>& GetNpcSites();
std::vector<NpcResource>& GetNpcResources();
std::vector<NpcCorpse>& GetNpcCorpses();
const std::vector<NpcEvent>& GetNpcEvents();
int32 GetTreasury();	// 마을 금고 전부 합친 것
void PushNpcEvent(const std::wstring& text, float r, float g, float b);
bool IsNpcOutlaw(const Npc& npc);
bool IsNpcVisible(const Npc& npc);	// 살아 있고 집 밖
bool IsWorldPosInTown(const WorldPos& pos);	// 어느 마을이든 그 안
WorldPos GetSitePos(int32 site);
bool IsSiteInTown(int32 site);
int32 GetNpcIndex(const Npc& npc);
float GetNpcDistance(const Npc& npc, const WorldPos& pos);
bool GetNpcTargetPos(int32 kind, int32 index, WorldPos* pos);	// 표적이 아직 있으면 true
std::wstring GetNpcTitle(const Npc& npc);	// "사냥꾼 로안"
const wchar_t* GetNpcJobName(int32 job);
std::wstring WithSubject(const std::wstring& word);	// 받침 보고 이 / 가
std::wstring WithObject(const std::wstring& word);	// 받침 보고 을 / 를
std::wstring WithToward(const std::wstring& word);	// 받침 보고 로 / 으로
void NoteNpcRobbery();
void NoteNpcMove();
int32 GetNpcMoveCount();	// 구역을 만든 뒤로 이사한 수
int32 GetNpcUpgradeCost(const Npc& npc);
float GetNpcTickMicroseconds();
int32 GetNpcDeathCount();	// 구역을 만든 뒤로
int32 GetNpcRobberyCount();

// 몸 ( Npc.cpp ) - 판단 쪽이 부른다
void ResetNpcPath(Npc* npc);
int32 NpcMoveTo(Npc* npc, const WorldPos& goal, float arrive_radius, float delta_seconds);
int32 NpcEngage(Npc* npc, int32 target_kind, int32 target_index, bool rob, float delta_seconds);
void NpcMoveAway(Npc* npc, const WorldPos& from, float delta_seconds);
void NpcHeal(Npc* npc, float per_second, float delta_seconds);
void NpcGainExp(Npc* npc, int32 exp);
void NpcShowText(const Npc& npc, const std::wstring& text, float r, float g, float b);
void NpcEnterHouse(Npc* npc);
void NpcLeaveHouse(Npc* npc);
int32 NpcPostLetters(Npc* npc, int32 village);	// 들고 온 소식을 그 마을 게시판에. 새로 붙은 수
int32 GetSiteMarketVillage(int32 site);	// 그 거점 장사 몫이 가는 마을
