#pragma once

#include <string>
#include <vector>

class Renderer;

// tools/art/bake.py 가 구운 캐릭터 시트 ( Art/*.png + *.json ). 동작마다 8 방향 프레임이 있다
constexpr int32 SHEET_NONE = -1;
constexpr int32 SHEET_PLAYER = 0;
constexpr int32 SHEET_BLIGHTED = 1;
constexpr int32 SHEET_HOUND = 2;
constexpr int32 SHEET_DEVOURER = 3;
constexpr int32 SHEET_GATEBREAKER = 4;
constexpr int32 SHEET_NPC_HUNTER = 5;
constexpr int32 SHEET_NPC_RAIDER = 6;
constexpr int32 SHEET_NPC_MERCHANT = 7;
constexpr int32 SHEET_NPC_GATHERER = 8;
constexpr int32 SHEET_NPC_WARDEN = 9;
constexpr int32 SHEET_COUNT = 10;

// 이름이 정해진 동작. 스킬 동작은 GetSheetSkillAnim 으로 찾는다
constexpr int32 ANIM_IDLE = 0;
constexpr int32 ANIM_RUN = 1;
constexpr int32 ANIM_GUARD = 2;
constexpr int32 ANIM_GUARD_WALK = 3;
constexpr int32 ANIM_PARRY = 4;
constexpr int32 ANIM_PARRY_SUCCESS = 5;
constexpr int32 ANIM_HIT = 6;
constexpr int32 ANIM_STUN = 7;
constexpr int32 ANIM_PARRIED = 8;
constexpr int32 ANIM_DEATH = 9;
constexpr int32 ANIM_ATTACK = 10;	// NPC 한 방
constexpr int32 ANIM_WORK = 11;	// NPC 채집
constexpr int32 ANIM_SLOT_COUNT = 12;

// tools/art/vfx.py 의 효과 시트
constexpr int32 VFX_HIT = 0;
constexpr int32 VFX_CRIT = 1;
constexpr int32 VFX_HURT = 2;
constexpr int32 VFX_PARRY = 3;
constexpr int32 VFX_GUARD = 4;
constexpr int32 VFX_DUST = 5;
constexpr int32 VFX_SLAM = 6;
constexpr int32 VFX_LEVEL_UP = 7;
constexpr int32 VFX_HEAL = 8;
constexpr int32 VFX_ASH = 9;
constexpr int32 VFX_CUT = 10;
constexpr int32 VFX_COUNT = 11;

bool InitSpriteSheets();
void ShutdownSpriteSheets();

// RGBA 8 비트, 윗줄부터. 소품 아틀라스도 이걸로 읽는다
bool LoadPngImage(const std::string& path, std::vector<uint8>* pixels, int32* width, int32* height);

int32 GetSheetAnim(int32 sheet, int32 slot);	// 없으면 -1
int32 GetSheetSkillAnim(int32 sheet, int32 skill_id);	// 없으면 -1
float GetSheetAnimPeriod(int32 sheet, int32 anim);	// 반복 동작 한 바퀴 ( 초 또는 칸 ). 한 번 도는 동작은 0
float GetSheetBodyHeight(int32 sheet);	// 서 있을 때 발에서 머리끝까지 ( 도트 칸 )
float GetSheetBodyWidth(int32 sheet);

// 월드 바라보는 방향을 그림 방향으로. 0 = 화면 오른쪽에서 시계 방향으로 8 개
int32 GetDir8(float world_dir_x, float world_dir_y);

// 동작 종류가 셋이라 재료도 셋을 다 받는다 - 한 번 도는 동작은 ratio ( 0 ~ 1 ), 반복은 seconds, 걷기는 걸은 칸 distance
int32 PickSheetFrame(int32 sheet, int32 anim, int32 dir, float ratio, float seconds, float distance);

void DrawSheetFrame(Renderer* renderer, int32 sheet, int32 frame, float foot_x, float foot_y, float r, float g, float b, float a, float flash);
void DrawSheetShadow(Renderer* renderer, int32 sheet, int32 frame, float foot_x, float foot_y, float lean_x, float lean_y, float alpha);

int32 GetVfxFrameCount(int32 vfx);
float GetVfxFrameSec(int32 vfx);
void DrawVfxFrame(Renderer* renderer, int32 vfx, int32 index, float center_x, float center_y, float angle, float scale, float r, float g, float b, float a);
