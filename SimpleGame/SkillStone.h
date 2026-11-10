#pragma once

#include <string>

// 스킬 강화석과 보스 스킬석. 강화석은 스킬 레벨( 피해 ), 보스 스킬석은 그 보스의 힘을 스킬 하나에 새긴다 ( 각인 )
int32 GetSkillUpgradeLevel(int32 skill_id);
int32 GetSkillUpgradeMax();
bool TryUpgradeSkill(int32 skill_id);	// 가방의 강화석 하나를 쓴다. 스킬 창에서 해금한 스킬을 누를 때
bool UseSkillStoneItem(int32 slot_idx);	// 가방에서 쓴 아이템이 스킬석이면 true

// 판정이 묻는다. 플레이어 스킬에만
float GetPlayerSkillDamageRate(int32 skill_id);
float GetPlayerSkillExtraStun(int32 skill_id);
float GetPlayerSkillCooldownCut(int32 skill_id);
std::wstring GetSkillInscriptionName(int32 skill_id);	// 새기지 않았으면 빈 문자열
