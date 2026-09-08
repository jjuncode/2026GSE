#pragma once

constexpr int CHUNK_TILE_COUNT = 32;
constexpr float TILE_PIXEL_WIDTH = 64.f;
constexpr float TILE_PIXEL_HEIGHT = 32.f;

// 청크 번호는 정수, 청크 안 위치만 실수. 멀리 가도 실수 눈금 간격이 안 벌어진다
struct WorldPos
{
	int m_chunk_x = 0;
	int m_chunk_y = 0;
	float m_local_x = 0.f;
	float m_local_y = 0.f;
};

// 화면 정중앙이 원점, 위쪽이 양수인 픽셀 좌표
struct ScreenPos
{
	float m_x = 0.f;
	float m_y = 0.f;
};

void NormalizeWorldPos(WorldPos* pos);
void MoveWorldPos(WorldPos* pos, float delta_x, float delta_y);

float GetTileDistanceX(const WorldPos& from, const WorldPos& to);
float GetTileDistanceY(const WorldPos& from, const WorldPos& to);
float GetDepthKey(const WorldPos& target, const WorldPos& camera);

ScreenPos WorldToScreen(const WorldPos& target, const WorldPos& camera);
WorldPos ScreenToWorld(const ScreenPos& screen, const WorldPos& camera);
