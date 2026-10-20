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

// 정수 배율로 키운다. 글자 한 점이 배율 x 배율 칸이 돼서 도트 격자는 안 깨진다
void DrawStringScaled(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, int32 scale, float r, float g, float b, float a);

// 네 방향 검은 테두리. 불꽃과 섬광 위에서도 숫자가 읽혀야 한다
void DrawStringOutline(Renderer* renderer, const std::wstring& text, float x, float y, int32 align, int32 scale, float r, float g, float b, float a);

float MeasureString(const std::wstring& text);
float GetLineHeight();
