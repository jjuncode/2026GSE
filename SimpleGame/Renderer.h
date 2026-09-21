#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"

// 텍스처 좌표. v0 = 사각형 아래변, v1 = 윗변
struct TexRegion
{
	float u0 = 0.f;
	float v0 = 0.f;
	float u1 = 1.f;
	float v1 = 1.f;
};

// 원점 + 두 축으로 늘이고 기울인 사각형. 회전, 반전, 그림자 기울임이 전부 이걸로 된다
struct AffineQuad
{
	float origin_x = 0.f;	// 아래변 왼쪽 끝 ( 픽셀 )
	float origin_y = 0.f;
	float axis_x_x = 0.f;	// 가로 변
	float axis_x_y = 0.f;
	float axis_y_x = 0.f;	// 세로 변
	float axis_y_y = 0.f;
};

class Renderer
{
public:
	Renderer(int32 window_size_x, int32 window_size_y);
	~Renderer();

	bool IsInitialized() const;

	// 정사각형. size 는 픽셀
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);
	// 가로세로가 다른 사각형. 막대와 패널용
	void DrawSolidBox(float x, float y, float width, float height, float r, float g, float b, float a);
	// 아이소 타일. scale 1 이 타일 한 칸
	void DrawSolidDiamond(float x, float y, float scale, float r, float g, float b, float a);
	// radius 는 픽셀. 창 비율이 달라도 찌그러지지 않는다
	void DrawSolidCircle(float x, float y, float radius, float r, float g, float b, float a);

	// 알파 0.5 미만 텍셀은 버린다. 도트 가장자리가 반투명으로 번지지 않게
	void DrawAffine(GLuint texture, const TexRegion& region, const AffineQuad& quad, float r, float g, float b, float a);

	// 씬 버퍼는 톤매핑 전이라 그림 색을 역변환해야 그린 색 그대로 나온다. HUD 는 끈다
	void SetUndoToneMap(bool undo);

private:
	void Initialize(int32 window_size_x, int32 window_size_y);
	void CreateVertexBufferObjects();
	void CreateRectVertexBuffer();
	void CreateDiamondVertexBuffer();
	void CreateCircleVertexBuffer();
	void CreateUnitQuadVertexBuffer();
	void CacheShaderLocations();
	void GetGLPosition(float x, float y, float* new_x, float* new_y);
	void DrawTriangles(GLuint vbo, int32 vertex_count, float new_x, float new_y, float scale_x, float scale_y, float r, float g, float b, float a);

	bool m_initialized = false;
	
	int32 m_window_size_x = 0;
	int32 m_window_size_y = 0;

	GLuint m_vbo_rect = 0;
	GLuint m_vbo_diamond = 0;
	GLuint m_vbo_circle = 0;
	GLuint m_vbo_unit_quad = 0;
	GLuint m_solid_rect_shader = 0;
	GLuint m_affine_shader = 0;

	bool m_undo_tone_map = false;

	int32 m_circle_vertex_count = 0;

	// 그리기마다 문자열로 찾으면 프레임당 수천 번 검색이라 링크 시점에 받아 둔다
	GLint m_trans_uniform = -1;
	GLint m_color_uniform = -1;
	GLint m_position_attrib = -1;

	GLint m_affine_origin = -1;
	GLint m_affine_axis_x = -1;
	GLint m_affine_axis_y = -1;
	GLint m_affine_region = -1;
	GLint m_affine_color = -1;
	GLint m_affine_texture = -1;
	GLint m_affine_undo = -1;
	GLint m_affine_position = -1;
};
