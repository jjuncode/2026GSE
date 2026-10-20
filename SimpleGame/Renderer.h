#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

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
// 삼각형 꼭짓점 하나. 좌표는 픽셀, 색은 꼭짓점마다 달라도 된다 ( 꼬리로 갈수록 흐려지는 궤적 )
struct ColorVertex
{
	float x = 0.f;
	float y = 0.f;
	float r = 1.f;
	float g = 1.f;
	float b = 1.f;
	float a = 1.f;
};

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

	// 가로세로가 다른 사각형. 막대와 패널용
	void DrawSolidBox(float x, float y, float width, float height, float r, float g, float b, float a);
	// 아이소 타일. scale 1 이 타일 한 칸
	void DrawSolidDiamond(float x, float y, float scale, float r, float g, float b, float a);
	// radius 는 픽셀. 창 비율이 달라도 찌그러지지 않는다
	void DrawSolidCircle(float x, float y, float radius, float r, float g, float b, float a);

	// 알파 0.5 미만 텍셀은 버린다. 도트 가장자리가 반투명으로 번지지 않게
	void DrawAffine(GLuint texture, const TexRegion& region, const AffineQuad& quad, float r, float g, float b, float a);

	// flash 0 ~ 1. 1 이면 그림 모양 그대로 흰빛이 된다 ( 피격 번쩍임 )
	void DrawAffineFlash(GLuint texture, const TexRegion& region, const AffineQuad& quad, float r, float g, float b, float a, float flash);

	// 부채꼴 판정, 칼 궤적, 충격파 고리처럼 사각형으로 안 나오는 모양
	void DrawTriangle(const ColorVertex& first, const ColorVertex& second, const ColorVertex& third);
	void DrawQuad(const ColorVertex& first, const ColorVertex& second, const ColorVertex& third, const ColorVertex& fourth);

	// 씬 버퍼는 톤매핑 전이라 그림 색을 역변환해야 그린 색 그대로 나온다. HUD 는 끈다
	void SetUndoToneMap(bool undo);

	// 켜면 삼각형을 도트처럼 그린다 - 반투명 대신 바이어 무늬로 칸을 솎고 밝기를 세 단으로 끊는다.
	// 칼 궤적과 불꽃이 매끈한 그라데이션이면 도트 캐릭터 옆에서 혼자 떠 보인다
	void SetPixelTriangles(bool on);

	// 쌓아 둔 것을 한 번에 내보낸다. 깊이 버퍼가 없어 덮임 순서가 곧 그린 순서라,
	// 다른 층이 자기 셰이더로 그리기 전에 반드시 비워야 한다
	void FlushBatch();

private:
	void Initialize(int32 window_size_x, int32 window_size_y);
	void CreateVertexBufferObjects();
	void CreateInstanceBuffer();
	void CacheShaderLocations();
	void GetGLPosition(float x, float y, float* new_x, float* new_y);

	void PushSolidInstance(GLuint mesh_vbo, int32 mesh_vertex_count, float new_x, float new_y, float scale_x, float scale_y, float r, float g, float b, float a);

	void FlushCurrentBatch(int32 reason);
	void FlushSolidBatch();
	void FlushAffineBatch();
	void FlushTriangleBatch();
	void PushColorVertex(const ColorVertex& vertex);
	void UploadInstanceBuffer();
	void BindInstanceAttrib(GLint location, int32 component_count, size_t float_offset, int32 stride_float_count);
	void ReleaseInstanceAttrib(GLint location);

	bool m_initialized = false;

	int32 m_window_size_x = 0;
	int32 m_window_size_y = 0;

	GLuint m_vbo_rect = 0;
	GLuint m_vbo_diamond = 0;
	GLuint m_vbo_circle = 0;
	GLuint m_vbo_unit_quad = 0;
	GLuint m_vbo_instance = 0;
	GLuint m_solid_rect_shader = 0;
	GLuint m_affine_shader = 0;

	bool m_undo_tone_map = false;
	bool m_pixel_triangles = false;
	bool m_batch_pixel_triangles = false;

	int32 m_circle_vertex_count = 0;

	// 모아 둔 인스턴스. 종류가 섞이지 않게 바뀔 때마다 비우고 다시 채운다
	std::vector<float> m_instances;
	int32 m_batch_kind = 0;
	int32 m_batch_instance_count = 0;

	GLuint m_batch_mesh_vbo = 0;
	int32 m_batch_mesh_vertex_count = 0;
	GLuint m_batch_texture = 0;
	bool m_batch_undo_tone_map = false;

	// 그리기마다 문자열로 찾으면 프레임당 수천 번 검색이라 링크 시점에 받아 둔다
	GLint m_position_attrib = -1;
	GLint m_solid_trans_attrib = -1;
	GLint m_solid_color_attrib = -1;
	GLint m_solid_pixel_mode = -1;

	GLint m_affine_texture = -1;
	GLint m_affine_undo = -1;
	GLint m_affine_position = -1;
	GLint m_affine_origin_attrib = -1;
	GLint m_affine_axis_x_attrib = -1;
	GLint m_affine_axis_y_attrib = -1;
	GLint m_affine_region_attrib = -1;
	GLint m_affine_color_attrib = -1;
	GLint m_affine_flash_attrib = -1;
};

// 바닥, 안개, 후처리처럼 자기 셰이더로 직접 그리는 층이 그리기 전에 부른다.
// 안 부르면 쌓아 둔 그림이 그 층 뒤로 밀려 덮임 순서가 뒤집힌다
void FlushRenderBatch();
