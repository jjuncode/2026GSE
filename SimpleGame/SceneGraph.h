#pragma once

#include "WorldPos.h"

class Actor;
class Renderer;

// 이번 프레임에 그릴 것을 모았다가 깊이 순으로 낸다
void BeginSceneGraph();
void AddSceneActor(const Actor* actor);
void DrawSceneGraph(Renderer* renderer, const WorldPos& camera);
