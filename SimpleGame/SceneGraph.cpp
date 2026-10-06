#include "stdafx.h"
#include "SceneGraph.h"
#include "Actor.h"
#include "Profiler.h"

#include <algorithm>
#include <vector>

namespace
{
	std::vector<const Actor*> g_actors;
}

void BeginSceneGraph()
{
	g_actors.clear();
}

void AddSceneActor(const Actor* actor)
{
	g_actors.push_back(actor);
}

void DrawSceneGraph(Renderer* renderer, const WorldPos& camera)
{
	// 드로우콜과 따로 움직인다. 정렬 비용은 액터 수를 따라가고 드로우콜은 배치를 따라간다
	CountSceneActor(static_cast<int32>(g_actors.size()));

	// 그림자는 전부 몸보다 먼저. 앞 물체 그림자가 뒤 물체 발을 덮는 건 괜찮지만 몸을 덮으면 안 된다
	for (const Actor* actor : g_actors)
	{
		if (actor->m_casts_shadow)
		{
			actor->DrawShadow(renderer, camera);
		}
	}

	// 뒤에 있는 것부터 그려야 앞엣것이 덮는다. 깊이 버퍼는 안 쓴다
	std::sort(g_actors.begin(), g_actors.end(),
		[&camera](const Actor* left, const Actor* right)
		{
			return GetDepthKey(left->m_pos, camera) < GetDepthKey(right->m_pos, camera);
		});

	for (const Actor* actor : g_actors)
	{
		actor->Draw(renderer, camera);
	}
}
