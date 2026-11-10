#pragma once

#include "GameData.h"
#include "WorldPos.h"

#include <string>
#include <vector>

class Renderer;
struct ZoneLayout;

constexpr int32 QUEST_STATE_OFFERED = 0;
constexpr int32 QUEST_STATE_ACTIVE = 1;
constexpr int32 QUEST_STATE_DONE = 2;	// 목표를 채웠다. 게시판에 보고하면 보상
constexpr int32 QUEST_STATE_REPORTED = 3;

constexpr int32 QUEST_ACCEPT_OK = 0;
constexpr int32 QUEST_ACCEPT_FULL = 1;	// 진행 중이 꽉 찼다
constexpr int32 QUEST_ACCEPT_TAKEN = 2;

// 게시판에 붙은 의뢰 하나. 받으면 같은 것을 진행 목록에 복사한다
struct Quest
{
	int32 serial = 0;
	int32 village = -1;	// 붙은 게시판
	int32 tier = QUEST_TIER_DAILY;
	int32 goal = QUEST_GOAL_HUNT;
	int32 target = -1;	// 몬스터 종류 · 아이템 번호
	int32 min_level = 0;	// 이 레벨 아래 정예는 안 센다
	int32 need = 1;
	int32 progress = 0;
	int32 level = 1;	// 권장 레벨
	int32 state = QUEST_STATE_OFFERED;
	bool has_place = false;
	WorldPos place;
	std::wstring place_name;
	std::wstring title;
	std::wstring desc;
	QuestReward reward;
};

// 구역을 만들 때 마을 · 몬스터 · 길찾기가 선 뒤에 부른다
void BuildQuests(const ZoneLayout& layout);
void UpdateQuests(float real_seconds);
void NotifyQuestKill(int32 kind, bool is_elite, bool is_boss, int32 level);

// 게시판
const std::vector<Quest>& GetBoardQuests(int32 village);
int32 AcceptBoardQuest(int32 village, int32 index);
int32 ReportQuestsAt(int32 village);	// 끝낸 의뢰를 넘기고 보상을 받는다. 넘긴 수
float GetBoardRefreshLeft(int32 village);

// 진행 중
const std::vector<Quest>& GetActiveQuests();
int32 GetTrackedQuest();	// 길 안내를 받는 진행 중 번호. 없으면 -1
void CycleTrackedQuest();
std::wstring DescribeQuestGoal(const Quest& quest, bool with_progress);
std::wstring GetQuestTierName(int32 tier);
void GetQuestTierColor(int32 tier, float* r, float* g, float* b);
bool GetQuestGuide(const Quest& quest, WorldPos* pos);	// 가야 할 곳. 끝냈으면 가장 가까운 게시판
float GetPlayerDistanceTo(const WorldPos& pos);	// 칸

// 그리기
void DrawQuestGround(Renderer* renderer, const WorldPos& camera);	// 길 점선과 목적지 표식
void DrawQuestOverlay(Renderer* renderer, const WorldPos& camera);	// 화면 밖 목적지 화살표
void DrawQuestTracker(Renderer* renderer, bool hidden);	// 오른쪽 위 의뢰 목록
