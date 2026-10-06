#pragma once

// 그림 번호. GL 헤더 없이 게임 로직이 참조할 수 있게 따로 둔다.
// 순서는 tools/sprite_art.py 의 build_sprites 와 같아야 한다
constexpr int32 SPRITE_NONE = -1;
constexpr int32 SPRITE_PLAYER = 0;
constexpr int32 SPRITE_BLIGHTED = 1;	// 타락자
constexpr int32 SPRITE_HOUND = 2;	// 재늑대
constexpr int32 SPRITE_DEVOURER = 3;	// 소거식귀
constexpr int32 SPRITE_GATEBREAKER = 4;	// 성문파쇄자
constexpr int32 SPRITE_SWORD = 5;
constexpr int32 SPRITE_GOLD = 6;
constexpr int32 SPRITE_EXP_ORB = 7;
constexpr int32 SPRITE_DUST = 8;
constexpr int32 SPRITE_FRAGMENT = 9;
constexpr int32 SPRITE_CORE = 10;
constexpr int32 SPRITE_DEAD_TREE = 11;	// 프레임 2 ( 가지 모양 )
constexpr int32 SPRITE_ROCK = 12;
constexpr int32 SPRITE_PEBBLE = 13;
constexpr int32 SPRITE_THORN = 14;
constexpr int32 SPRITE_GRASS = 15;
constexpr int32 SPRITE_REEDS = 16;
constexpr int32 SPRITE_BRAZIER = 17;
constexpr int32 SPRITE_BONES = 18;
constexpr int32 SPRITE_NPC_HUNTER = 19;
constexpr int32 SPRITE_NPC_RAIDER = 20;
constexpr int32 SPRITE_NPC_MERCHANT = 21;
constexpr int32 SPRITE_NPC_GATHERER = 22;
constexpr int32 SPRITE_NPC_WARDEN = 23;
constexpr int32 SPRITE_BLOCK_WALL = 24;	// 칸 하나짜리 입체. 발밑이 칸 앞 꼭짓점
constexpr int32 SPRITE_BLOCK_TOWER = 25;
constexpr int32 SPRITE_BLOCK_HOUSE = 26;	// 프레임 0 문 / 1 창
constexpr int32 SPRITE_BLOCK_RUIN = 27;
constexpr int32 SPRITE_TENT = 28;	// 프레임 0 개척대 / 1 약탈자
constexpr int32 SPRITE_BANNER = 29;
constexpr int32 SPRITE_TOTEM = 30;
constexpr int32 SPRITE_GRAVE = 31;
constexpr int32 SPRITE_POUCH = 32;
constexpr int32 SPRITE_HERB = 33;
constexpr int32 SPRITE_ARROW = 34;
constexpr int32 SPRITE_STALL = 35;
constexpr int32 SPRITE_CROPS = 36;	// 프레임 0 성 안 / 1 말라 죽은 밭
constexpr int32 SPRITE_LAMP = 37;
constexpr int32 SPRITE_COUNT = 38;
