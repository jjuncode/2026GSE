#include "stdafx.h"
#include "Board.h"
#include "Village.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <string>

namespace
{
	constexpr size_t NEWS_MAX_COUNT = 800;	// 게시판 일곱 장이 다 붙여도 남는 양
	constexpr size_t BOARD_MAX_POSTS = 48;
	constexpr float VIEW_FORGET_SEC = 300.f;	// 소식이 평판에 주는 무게의 반감기
	constexpr float LETTER_MAX_AGE_SEC = 600.f;	// 이보다 묵은 소식은 안 챙긴다

	// 낱낱의 사건은 덜 친다. 제 마을 죽음은 다 붙고 남의 마을 것은 편지로 일부만 와서,
	// 다 같이 세면 누구에게나 남의 마을이 더 안전해 보였다 ( 득 1 ~ 10 ). 소식지 숫자가 바탕이다
	constexpr float EVENT_WEIGHT = 0.4f;

	struct NewsEffect
	{
		float safety = 0.f;
		float prosperity = 0.f;
	};

	// NEWS_ 번호 순서
	const std::array<NewsEffect, NEWS_KIND_COUNT> EFFECTS =
	{ {
		{ -1.0f, 0.f },
		{ -1.5f, 0.f },
		{ -1.0f, 0.f },
		{ -0.2f, -0.8f },
		{ -0.6f, -0.2f },
		{ -0.5f, 0.f },
		{ 0.5f, 0.f },
		{ 0.4f, 0.1f },
		{ 0.f, 0.5f },
		{ 0.1f, 0.3f },
		{ 0.f, 0.2f },
		{ 0.f, -0.3f },
		{ 0.f, 0.3f },
		{ 0.f, -1.5f },
		{ 0.f, 0.f },
		{ 0.6f, 0.f },
		{ -2.f, 0.f },
	} };

	std::deque<News> g_news;	// 번호 순서. 앞에서부터 버린다
	std::vector<std::vector<BoardPost>> g_boards;
	std::vector<int32> g_board_serials;
	int32 g_next_news_id = 1;
	float g_clock = 0.f;

	const std::vector<BoardPost> EMPTY_POSTS;

	bool IsValidBoard(int32 village)
	{
		return village >= 0 && village < static_cast<int32>(g_boards.size());
	}

	float Clamp(float value, float min_value, float max_value)
	{
		if (value < min_value)
		{
			return min_value;
		}

		if (value > max_value)
		{
			return max_value;
		}

		return value;
	}

	News* FindNewsMutable(int32 id)
	{
		if (g_news.empty())
		{
			return nullptr;
		}

		const int32 first = g_news.front().id;
		if (id < first || id >= first + static_cast<int32>(g_news.size()))
		{
			return nullptr;
		}

		return &g_news[static_cast<size_t>(id - first)];
	}

	int32 StoreNews(News news)
	{
		news.id = g_next_news_id;
		g_next_news_id += 1;
		news.time = g_clock;

		g_news.push_back(news);

		while (g_news.size() > NEWS_MAX_COUNT)
		{
			g_news.pop_front();
		}

		return news.id;
	}

	// 같은 마을 소식지는 가장 새 것 하나만 남긴다. 안 그러면 일곱 마을 소식지가 게시판을 덮는다
	bool ReplaceOlderReport(std::vector<BoardPost>* posts, const News& incoming, const BoardPost& post)
	{
		for (BoardPost& existing : *posts)
		{
			const News* news = FindNews(existing.news);
			if (news == nullptr || news->kind != NEWS_REPORT || news->origin != incoming.origin)
			{
				continue;
			}

			if (news->time >= incoming.time)
			{
				return true;
			}

			existing = post;
			return true;
		}

		return false;
	}

	void TrimBoard(std::vector<BoardPost>* posts)
	{
		while (posts->size() > BOARD_MAX_POSTS)
		{
			size_t oldest = 0;

			for (size_t index = 1; index < posts->size(); ++index)
			{
				if ((*posts)[index].posted < (*posts)[oldest].posted)
				{
					oldest = index;
				}
			}

			posts->erase(posts->begin() + static_cast<std::ptrdiff_t>(oldest));
		}
	}
}

void BuildBoards()
{
	g_news.clear();
	g_boards.assign(static_cast<size_t>(GetVillageCount()), std::vector<BoardPost>());
	g_board_serials.assign(static_cast<size_t>(GetVillageCount()), 0);
	g_clock = 0.f;
}

void UpdateBoards(float delta_seconds)
{
	g_clock += delta_seconds;
}

float GetWorldClock()
{
	return g_clock;
}

int32 PostNews(int32 origin, int32 kind, const std::wstring& text)
{
	if (IsValidBoard(origin) == false || kind < 0 || kind >= NEWS_KIND_COUNT)
	{
		return -1;
	}

	News news;
	news.kind = kind;
	news.origin = origin;
	news.text = text;
	news.safety = EFFECTS[static_cast<size_t>(kind)].safety;
	news.prosperity = EFFECTS[static_cast<size_t>(kind)].prosperity;

	const int32 id = StoreNews(news);
	DeliverNews(origin, id, VIA_WITNESS, L"");

	return id;
}

int32 PostNewsAt(const WorldPos& pos, int32 kind, const std::wstring& text)
{
	int32 origin = GetVillageAt(pos);
	if (origin < 0)
	{
		origin = FindNearestVillage(pos);
	}

	return PostNews(origin, kind, text);
}

int32 PostVillageReport(int32 village, int32 population, int32 treasury, int32 deaths)
{
	if (IsValidBoard(village) == false)
	{
		return -1;
	}

	News news;
	news.kind = NEWS_REPORT;
	news.origin = village;
	news.population = population;
	news.treasury = treasury;
	news.deaths = deaths;
	news.text = std::wstring(GetVillageName(village)) + L" 소식지 - 주민 " + std::to_wstring(population) + L"명, 금고 " + std::to_wstring(treasury) + L", 근래 사망 " + std::to_wstring(deaths);

	const int32 id = StoreNews(news);
	DeliverNews(village, id, VIA_WITNESS, L"");

	return id;
}

const News* FindNews(int32 id)
{
	return FindNewsMutable(id);
}

const std::vector<BoardPost>& GetBoardPosts(int32 village)
{
	if (IsValidBoard(village) == false)
	{
		return EMPTY_POSTS;
	}

	return g_boards[static_cast<size_t>(village)];
}

bool BoardHasNews(int32 village, int32 news)
{
	if (IsValidBoard(village) == false)
	{
		return false;
	}

	for (const BoardPost& post : g_boards[static_cast<size_t>(village)])
	{
		if (post.news == news)
		{
			return true;
		}
	}

	return false;
}

bool DeliverNews(int32 village, int32 news, int32 via, const std::wstring& carrier)
{
	if (IsValidBoard(village) == false)
	{
		return false;
	}

	const News* incoming = FindNews(news);
	if (incoming == nullptr || BoardHasNews(village, news))
	{
		return false;
	}

	BoardPost post;
	post.news = news;
	post.via = via;
	post.carrier = carrier;
	post.posted = g_clock;

	std::vector<BoardPost>& posts = g_boards[static_cast<size_t>(village)];

	if (incoming->kind == NEWS_REPORT)
	{
		if (ReplaceOlderReport(&posts, *incoming, post))
		{
			g_board_serials[static_cast<size_t>(village)] += 1;
			return BoardHasNews(village, news);
		}
	}

	posts.push_back(post);
	TrimBoard(&posts);
	g_board_serials[static_cast<size_t>(village)] += 1;

	return true;
}

int32 GetBoardPostSerial(int32 village)
{
	if (IsValidBoard(village) == false)
	{
		return 0;
	}

	return g_board_serials[static_cast<size_t>(village)];
}

void CollectLetters(int32 village, int32 max_count, std::vector<int32>* letters)
{
	letters->clear();

	if (IsValidBoard(village) == false)
	{
		return;
	}

	std::vector<const News*> fresh;

	for (const BoardPost& post : g_boards[static_cast<size_t>(village)])
	{
		const News* news = FindNews(post.news);
		if (news != nullptr && g_clock - news->time <= LETTER_MAX_AGE_SEC)
		{
			fresh.push_back(news);
		}
	}

	std::sort(fresh.begin(), fresh.end(),
		[](const News* left, const News* right)
		{
			return left->time > right->time;
		});

	for (const News* news : fresh)
	{
		if (static_cast<int32>(letters->size()) >= max_count)
		{
			return;
		}

		letters->push_back(news->id);
	}
}

int32 CountUnknownNews(int32 from_village, int32 to_village)
{
	if (IsValidBoard(from_village) == false || IsValidBoard(to_village) == false)
	{
		return 0;
	}

	int32 count = 0;

	for (const BoardPost& post : g_boards[static_cast<size_t>(from_village)])
	{
		if (BoardHasNews(to_village, post.news) == false)
		{
			count += 1;
		}
	}

	return count;
}

VillageView EvaluateBoardView(int32 board_village, int32 about_village)
{
	VillageView view;

	if (IsValidBoard(board_village) == false)
	{
		return view;
	}

	const News* report = nullptr;

	for (const BoardPost& post : g_boards[static_cast<size_t>(board_village)])
	{
		const News* news = FindNews(post.news);
		if (news == nullptr || news->origin != about_village)
		{
			continue;
		}

		if (news->time > view.newest)
		{
			view.newest = news->time;
		}

		if (news->kind == NEWS_REPORT)
		{
			if (report == nullptr || news->time > report->time)
			{
				report = news;
			}

			continue;
		}

		const float age = g_clock - news->time;
		const float weight = std::exp(-age * 0.693f / VIEW_FORGET_SEC) * EVENT_WEIGHT;

		view.safety += news->safety * weight;
		view.prosperity += news->prosperity * weight;
		view.news_count += 1;
	}

	// 소식지의 숫자가 바탕, 낱낱의 사건이 그 위에 얹힌다
	if (report != nullptr)
	{
		view.population = report->population;
		view.treasury = report->treasury;
		view.prosperity += Clamp((static_cast<float>(report->treasury) - 450.f) / 450.f, -1.2f, 1.2f);
		view.safety += Clamp(-0.35f * static_cast<float>(report->deaths), -2.5f, 0.f);
		view.news_count += 1;
	}

	view.known = view.news_count > 0;

	return view;
}

const wchar_t* GetViaName(int32 via)
{
	switch (via)
	{
		case VIA_LETTER:
		{
			return L"편지";
		}
		case VIA_RUMOR:
		{
			return L"소문";
		}
		default:
		{
			return L"목격";
		}
	}
}

std::wstring FormatAgo(float seconds)
{
	if (seconds < 15.f)
	{
		return L"방금";
	}

	if (seconds < 60.f)
	{
		return std::to_wstring(static_cast<int32>(seconds)) + L"초 전";
	}

	return std::to_wstring(static_cast<int32>(seconds / 60.f)) + L"분 전";
}
