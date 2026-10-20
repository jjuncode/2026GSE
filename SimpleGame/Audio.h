#pragma once

// 효과음 번호. Sounds/ 밑의 이름_번호.wav 를 전부 읽어 변형으로 쓴다 ( tools/sound_build.py 가 만든다 )
constexpr int32 SFX_SWING_LIGHT = 0;
constexpr int32 SFX_SWING_HEAVY = 1;
constexpr int32 SFX_HIT_FLESH = 2;
constexpr int32 SFX_HIT_HEAVY = 3;
constexpr int32 SFX_HIT_CRIT = 4;
constexpr int32 SFX_GUARD = 5;
constexpr int32 SFX_GUARD_BREAK = 6;
constexpr int32 SFX_PARRY = 7;
constexpr int32 SFX_PLAYER_HURT = 8;
constexpr int32 SFX_DODGE = 9;
constexpr int32 SFX_GLINT = 10;	// 패링 타이밍 신호
constexpr int32 SFX_DANGER = 11;	// 못 막는 공격 예고
constexpr int32 SFX_MONSTER_DIE = 12;
constexpr int32 SFX_POTION = 13;
constexpr int32 SFX_PICKUP = 14;
constexpr int32 SFX_GOLD = 15;
constexpr int32 SFX_LEVEL_UP = 16;
constexpr int32 SFX_UI_CLICK = 17;
constexpr int32 SFX_UI_ERROR = 18;
constexpr int32 SFX_ENHANCE_SUCCESS = 19;
constexpr int32 SFX_ENHANCE_FAIL = 20;
constexpr int32 SFX_ENHANCE_BREAK = 21;
constexpr int32 SFX_EQUIP = 22;
constexpr int32 SFX_MONSTER_SWING = 23;
constexpr int32 SFX_PLAYER_DIE = 24;
constexpr int32 SFX_SKILL_UNLOCK = 25;
constexpr int32 SFX_COUNTER = 26;	// 패링 반격 마무리
constexpr int32 SFX_COUNT = 27;

bool InitAudio();
void ShutdownAudio();

// 변형 중 하나를 고르고 높낮이를 조금 흔든다. 같은 소리가 연달아 똑같이 나면 기계처럼 들린다
void PlaySfx(int32 sfx, float volume);
void PlaySfxPitch(int32 sfx, float volume, float pitch);

void SetMasterVolume(float volume);
float GetMasterVolume();
float ChangeMasterVolume(float delta);	// 0 ~ 1 로 자른 새 값
