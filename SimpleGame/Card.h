#pragma once

#include "GameData.h"

#include <string>
#include <vector>

class Renderer;
struct WorldPos;

// 카드 수집. 몬스터마다 한 장, 처치하면 확률로 얻는다. 모은 장 수로 레벨이 오르고 능력치가 붙는다 ( 내실 )
int32 GetCardCount();
const CardData& GetCardDataAt(int32 index);
int32 GetCardCopies(int32 index);
int32 GetCardLevel(int32 index);	// 0 이면 아직 없다
int32 GetCardNextCopies(int32 index);	// 다음 레벨까지 필요한 장 수. 만렙이면 0
std::wstring DescribeCardBonus(const CardData& card, int32 level);

void RollCardDrop(int32 monster_type, const WorldPos& pos);	// 플레이어가 잡았을 때
void AddCardStat(CombatStat* stat);	// 능력치를 다시 셀 때 더한다
