#pragma once

#include <array>

constexpr int GROUND_MAX_LIGHT_COUNT = 8;

// 카메라 기준 칸 오프셋. 절대 좌표로 넘기면 멀리서 float 눈금이 벌어진다
struct GroundLight
{
	float m_tile_offset_x = 0.f;
	float m_tile_offset_y = 0.f;

	float m_radius = 0.f;

	float m_r = 0.f;
	float m_g = 0.f;
	float m_b = 0.f;
	float m_strength = 0.f;
};

// 칸 번호는 정수, 칸 안 위치만 실수로 갈라 넘긴다. 한 덩어리 float 면 얼룩이 떨린다
struct GroundFrame
{
	int m_camera_tile_x = 0;
	int m_camera_tile_y = 0;
	float m_camera_frac_x = 0.f;
	float m_camera_frac_y = 0.f;

	std::array<GroundLight, GROUND_MAX_LIGHT_COUNT> m_lights;
	int m_light_count = 0;

	bool m_show_chunk_grid = false;
};

bool InitGround(int screen_width, int screen_height);
void ShutdownGround();

// 화면 덮는 사각형 한 장. 색이 픽셀마다 정해지므로 칸 경계가 안 생긴다
void DrawGroundLayer(const GroundFrame& frame);
