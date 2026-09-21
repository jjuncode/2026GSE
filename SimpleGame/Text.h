#pragma once

#include <string>

class Renderer;

constexpr int32 TEXT_ALIGN_LEFT = 0;
constexpr int32 TEXT_ALIGN_CENTER = 1;
constexpr int32 TEXT_ALIGN_RIGHT = 2;

// GL 문맥이 선 뒤에 부른다
bool InitText();
void ShutdownText();

// y 는 글줄 위쪽. 글자 한 점 = 저해상도 한 픽셀
void DrawString(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a);

// 검은 그림자를 한 점 밀어 깐다. 밝은 모래 위에서도 읽히게
void DrawStringShadow(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a);

float MeasureString(const std::wstring& text);
float GetLineHeight();
