#pragma once

#include <array>
#include <vector>

constexpr int32 GROUND_MAX_LIGHT_COUNT = 8;

constexpr uint8 GROUND_CODE_OPEN = 0;
constexpr uint8 GROUND_CODE_CLIFF = 1;
constexpr uint8 GROUND_CODE_WATER = 2;

// 카메라 기준 칸 오프셋. 절대 좌표로 넘기면 멀리서 float 눈금이 벌어진다
struct GroundLight
{
	float tile_offset_x = 0.f;
	float tile_offset_y = 0.f;

	float radius = 0.f;

	float r = 0.f;
	float g = 0.f;
	float b = 0.f;
	float strength = 0.f;
};

// 칸 번호는 정수, 칸 안 위치만 실수로 갈라 넘긴다. 한 덩어리 float 면 얼룩이 떨린다
struct GroundFrame
{
	int32 camera_tile_x = 0;
	int32 camera_tile_y = 0;
	float camera_frac_x = 0.f;
	float camera_frac_y = 0.f;

	std::array<GroundLight, GROUND_MAX_LIGHT_COUNT> lights;
	int32 light_count = 0;

	bool show_chunk_grid = false;
};

bool InitGround(int32 screen_width, int32 screen_height, float pixel_scale);
void ShutdownGround();

// 칸 종류 표를 텍스처로 올린다. origin = 표 (0,0) 칸의 전역 칸 번호
// 낭떠러지 벽은 셰이더가 픽셀마다 화면 위쪽으로 거슬러 올라가 테두리를 찾아 그린다
void SetGroundZone(const std::vector<uint8>& codes, int32 size, int32 origin_tile_x, int32 origin_tile_y);

// 화면 덮는 사각형 한 장. 색이 픽셀마다 정해지므로 칸 경계가 안 생긴다
void DrawGroundLayer(const GroundFrame& frame);
