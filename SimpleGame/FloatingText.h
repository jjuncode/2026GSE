#pragma once

#include "WorldPos.h"

#include <string>

class Renderer;

void ClearFloatingTexts();

// lift = 발밑에서 띄워 시작할 높이 ( 픽셀 )
void AddFloatingText(const WorldPos& pos, float lift, const std::wstring& text, float r, float g, float b);
void UpdateFloatingTexts(float delta_seconds);

// 후처리 뒤에 부른다. 블룸과 흐림을 안 타야 글자가 읽힌다
void DrawFloatingTexts(Renderer* renderer, const WorldPos& camera);
