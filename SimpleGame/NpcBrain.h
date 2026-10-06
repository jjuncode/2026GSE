#pragma once

#include "BehaviorTree.h"
#include "NpcData.h"

// 트리는 한 벌을 오십 명이 같이 쓴다. 기억만 각자 든다
void BuildNpcBrain();
void InitNpcBrain(Npc* npc, uint32 seed);
void ResetNpcBrain(Npc* npc);

// 주변을 훑어 가장 가까운 적과 그 적을 대할 자세를 정한다
void SenseNpc(Npc* npc, float delta_seconds);
void ThinkNpc(Npc* npc, float delta_seconds);

// 관찰창
const BehaviorTree<Npc>& GetNpcBrainTree();
int32 GetNpcDailyNode();	// 일과 가중 선택 노드
float GetNpcTaskWeight(const Npc& npc, int32 task);
const wchar_t* GetNpcTaskName(int32 task);
const wchar_t* GetNpcStanceName(int32 stance);
