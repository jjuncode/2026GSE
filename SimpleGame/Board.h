#pragma once

#include "WorldPos.h"

#include <string>
#include <vector>

// 소식 종류. 평판에 어떻게 먹히는지는 Board.cpp 의 표가 정한다
constexpr int32 NEWS_DEATH = 0;	// 몬스터에게 죽음
constexpr int32 NEWS_DEVOURED = 1;	// 거인에게 먹힘
constexpr int32 NEWS_MURDER = 2;	// 사람에게 베임
constexpr int32 NEWS_STARVE = 3;
constexpr int32 NEWS_ROBBERY = 4;
constexpr int32 NEWS_OUTLAW = 5;	// 선량한 사람이 약탈에 손을 댐
constexpr int32 NEWS_BOUNTY = 6;	// 무법자를 벰
constexpr int32 NEWS_HUNT = 7;	// 큰 짐승을 잡음
constexpr int32 NEWS_TRADE = 8;
constexpr int32 NEWS_GEAR = 9;
constexpr int32 NEWS_ARRIVAL = 10;	// 새 사람이 들어옴
constexpr int32 NEWS_MOVE_OUT = 11;
constexpr int32 NEWS_MOVE_IN = 12;
constexpr int32 NEWS_TREASURY = 13;	// 금고가 바닥남
constexpr int32 NEWS_REPORT = 14;	// 마을 소식지. 인구 · 금고 · 사망을 숫자로
constexpr int32 NEWS_HERO = 15;	// 개척대원이 큰 놈을 벰
constexpr int32 NEWS_BOSS = 16;	// 성문파쇄자가 깨어남 · 쓰러짐
constexpr int32 NEWS_KIND_COUNT = 17;

// 게시판에 어떻게 왔나
constexpr int32 VIA_WITNESS = 0;	// 그 마을에서 일어난 일
constexpr int32 VIA_LETTER = 1;	// 전령 · 행상이 들고 온 편지
constexpr int32 VIA_RUMOR = 2;	// 다른 마을에서 온 사람이 전한 말

struct News
{
	int32 id = 0;
	int32 kind = NEWS_DEATH;
	int32 origin = 0;	// 일이 난 마을 ( 벽 밖이면 가장 가까운 마을 )
	std::wstring text;
	float time = 0.f;	// 세계 시계 ( 초 )
	float safety = 0.f;
	float prosperity = 0.f;
	int32 population = 0;	// 소식지만
	int32 treasury = 0;
	int32 deaths = 0;
};

struct BoardPost
{
	int32 news = 0;
	int32 via = VIA_WITNESS;
	std::wstring carrier;	// 들고 온 사람
	float posted = 0.f;
};

// 한 게시판이 아는 만큼의 마을 평판
struct VillageView
{
	bool known = false;
	float safety = 0.f;
	float prosperity = 0.f;
	float newest = -1.f;	// 가장 새 소식이 일어난 시각
	int32 population = 0;
	int32 treasury = 0;
	int32 news_count = 0;
};

void BuildBoards();
void UpdateBoards(float delta_seconds);
float GetWorldClock();

// 일어난 일. 그 마을 게시판에 바로 붙고, 다른 마을로는 누가 들고 가야 퍼진다
int32 PostNews(int32 origin, int32 kind, const std::wstring& text);
int32 PostNewsAt(const WorldPos& pos, int32 kind, const std::wstring& text);	// 가장 가까운 마을 몫
int32 PostVillageReport(int32 village, int32 population, int32 treasury, int32 deaths);

const News* FindNews(int32 id);
const std::vector<BoardPost>& GetBoardPosts(int32 village);
bool BoardHasNews(int32 village, int32 news);
bool DeliverNews(int32 village, int32 news, int32 via, const std::wstring& carrier);	// 새로 붙었으면 true
int32 GetBoardPostSerial(int32 village);	// 붙을 때마다 는다. 창이 다시 그릴지 본다

// 떠나는 사람이 챙기는 소식. 그 게시판의 최근 것부터
void CollectLetters(int32 village, int32 max_count, std::vector<int32>* letters);
int32 CountUnknownNews(int32 from_village, int32 to_village);	// from 에는 있고 to 에는 없는 것

VillageView EvaluateBoardView(int32 board_village, int32 about_village);

const wchar_t* GetViaName(int32 via);
std::wstring FormatAgo(float seconds);	// "방금" · "3분 전"
