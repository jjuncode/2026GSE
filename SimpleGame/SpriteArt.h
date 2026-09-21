#pragma once

#include <array>
#include <vector>

// 코드 안에 적는 도트 그림. 한 글자 = 한 칸, '.' = 투명
struct SpriteArt
{
	int32 width = 0;
	int32 height = 0;
	std::vector<std::vector<const char*>> frames;	// 프레임마다 윗줄부터
};

// 인덱스 = SPRITE_ 번호
std::vector<SpriteArt> MakeSpriteArts();

// 화면에 보일 최종 색. 알파 0 이면 투명
std::array<float, 4> GetPaletteColor(char key);
