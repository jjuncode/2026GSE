#pragma once

#include "WorldPos.h"

class Renderer;

// 세우는 물체와 배회하는 몬스터와 광원을 한 구조체로 든다.
// 종류를 열거형으로 가르는 대신 값이 0 인지로 갈라서, 새 종류가 생겨도 분기가 안 늘어난다
struct GameObject
{
	WorldPos m_pos;

	float m_width = 16.f;
	float m_height = 16.f;

	float m_r = 1.f;
	float m_g = 1.f;
	float m_b = 1.f;
	float m_a = 1.f;

	// 0 보다 크면 스스로 배회한다. 스폰 자리에서 멀어지면 돌아온다
	WorldPos m_home_pos;
	float m_move_speed = 0.f;
	float m_move_dir_x = 0.f;
	float m_move_dir_y = 0.f;
	float m_move_timer = 0.f;

	// 0 보다 크면 바닥을 밝히고 빛무리를 낸다
	float m_light_radius = 0.f;
	float m_light_r = 0.f;
	float m_light_g = 0.f;
	float m_light_b = 0.f;
	float m_light_strength = 0.f;

	// 바닥에 눕는 것은 그림자와 발밑 보정을 안 받는다
	// 1 보다 크면 스스로 빛나는 것으로 친다. 번짐은 후처리 블뢸이 만든다
	float m_emissive = 1.f;

	bool m_is_upright = true;
};

void InitScene();
void UpdateScene(float delta_seconds);
void DrawScene(Renderer* renderer);

WorldPos GetCameraPos();
void MovePlayerByScreenAxis(float screen_right, float screen_up, float delta_seconds);
void MovePlayerByWorldAxis(float world_x, float world_y, float delta_seconds);
void SetPickedTile(const WorldPos& tile);
void ToggleChunkGrid();
void ToggleLighting();
