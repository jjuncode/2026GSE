#pragma once

#include "WorldPos.h"

#include <string>

class Renderer;

constexpr int32 DAMAGE_STYLE_NORMAL = 0;
constexpr int32 DAMAGE_STYLE_CRIT = 1;
constexpr int32 DAMAGE_STYLE_WEAK = 2;	// 등 뒤 급소
constexpr int32 DAMAGE_STYLE_PLAYER = 3;	// 내가 맞음
constexpr int32 DAMAGE_STYLE_GUARD = 4;
constexpr int32 DAMAGE_STYLE_BANNER_GOOD = 5;	// 패링! 처럼 크게 한 번
constexpr int32 DAMAGE_STYLE_BANNER_BAD = 6;
constexpr int32 DAMAGE_STYLE_HEAL = 7;
constexpr int32 DAMAGE_STYLE_INFO = 8;	// 획득 · 경험치
constexpr int32 DAMAGE_STYLE_CUSTOM = 9;

void ClearFloatingTexts();

// lift = 발밑에서 띄워 시작할 높이 ( 픽셀 )
void AddFloatingText(const WorldPos& pos, float lift, const std::wstring& text, float r, float g, float b);
void AddDamageText(const WorldPos& pos, float lift, const std::wstring& text, int32 style);
void UpdateFloatingTexts(float delta_seconds);

// 후처리 뒤에 부른다. 블룸과 흐림을 안 타야 글자가 읽힌다
void DrawFloatingTexts(Renderer* renderer, const WorldPos& camera);
