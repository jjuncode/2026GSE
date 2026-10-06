#pragma once

#include <string>

class Renderer;

constexpr int32 TEXT_ALIGN_LEFT = 0;
constexpr int32 TEXT_ALIGN_CENTER = 1;
constexpr int32 TEXT_ALIGN_RIGHT = 2;

// GL 문맥이 선 뒤에 부른다
bool InitText();
void ShutdownText();

// 프레임 끝마다 부른다. 새로 구운 글자가 없으면 바로 돌아간다.
// 끝낼 때만 내리면 강제 종료나 크래시에서 이번 실행분이 통째로 날아간다
void SaveTextCache();

// y 는 글줄 위쪽. 글자 한 점 = 저해상도 한 픽셀
void DrawString(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a);

// 검은 그림자를 한 점 밀어 깐다. 밝은 모래 위에서도 읽히게
void DrawStringShadow(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, float r, float g, float b, float a);

float MeasureString(const std::wstring& text);
float GetLineHeight();
