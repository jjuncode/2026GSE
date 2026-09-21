#include "stdafx.h"
#include "Renderer.h"
#include "WorldPos.h"
#include "ShaderUtil.h"
#include "Profiler.h"

#include <array>
#include <vector>
#include <cmath>

namespace
{
	const int32 CIRCLE_SEGMENT_COUNT = 48;
	const float PI = 3.14159265f;

	void PushVertex(std::vector<float>* target, float x, float y)
	{
		target->push_back(x);
		target->push_back(y);
		target->push_back(0.f);
	}
}

Renderer::Renderer(int32 window_size_x, int32 window_size_y)
{
	Initialize(window_size_x, window_size_y);
}


Renderer::~Renderer()
{
	if (m_vbo_rect != 0)
	{
		glDeleteBuffers(1, &m_vbo_rect);
		m_vbo_rect = 0;
	}

	if (m_vbo_diamond != 0)
	{
		glDeleteBuffers(1, &m_vbo_diamond);
		m_vbo_diamond = 0;
	}

	if (m_vbo_circle != 0)
	{
		glDeleteBuffers(1, &m_vbo_circle);
		m_vbo_circle = 0;
	}

	if (m_vbo_unit_quad != 0)
	{
		glDeleteBuffers(1, &m_vbo_unit_quad);
		m_vbo_unit_quad = 0;
	}

	if (m_solid_rect_shader != 0)
	{
		glDeleteProgram(m_solid_rect_shader);
		m_solid_rect_shader = 0;
	}

	if (m_affine_shader != 0)
	{
		glDeleteProgram(m_affine_shader);
		m_affine_shader = 0;
	}
}

void Renderer::Initialize(int32 window_size_x, int32 window_size_y)
{
	m_window_size_x = window_size_x;
	m_window_size_y = window_size_y;

	// 켜두지 않으면 알파 인자가 통째로 죽는다
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_solid_rect_shader = CompileShaderProgram("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	m_affine_shader = CompileShaderProgram("./Shaders/Affine.vs", "./Shaders/Affine.fs");

	CreateVertexBufferObjects();

	if (m_solid_rect_shader > 0 && m_affine_shader > 0 && m_vbo_rect > 0 && m_vbo_diamond > 0 && m_vbo_circle > 0 && m_vbo_unit_quad > 0)
	{
		CacheShaderLocations();
		m_initialized = true;
	}
}

bool Renderer::IsInitialized() const
{
	return m_initialized;
}

void Renderer::CreateVertexBufferObjects()
{
	CreateRectVertexBuffer();
	CreateDiamondVertexBuffer();
	CreateCircleVertexBuffer();
	CreateUnitQuadVertexBuffer();
}

void Renderer::CreateUnitQuadVertexBuffer()
{
	// 0 ~ 1 사각형. 실제 모양은 셰이더가 원점과 두 축으로 만든다
	const std::array<float, 18> quad =
	{
		0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 0.f,
	};

	glGenBuffers(1, &m_vbo_unit_quad);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_unit_quad);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * quad.size()), quad.data(), GL_STATIC_DRAW);
}

void Renderer::CreateRectVertexBuffer()
{
	const float window_width = static_cast<float>(m_window_size_x);
	const float window_height = static_cast<float>(m_window_size_y);

	const std::array<float, 18> rect =
	{
		-1.f / window_width, -1.f / window_height, 0.f, -1.f / window_width, 1.f / window_height, 0.f, 1.f / window_width, 1.f / window_height, 0.f,
		-1.f / window_width, -1.f / window_height, 0.f, 1.f / window_width, 1.f / window_height, 0.f, 1.f / window_width, -1.f / window_height, 0.f,
	};

	glGenBuffers(1, &m_vbo_rect);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_rect);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * rect.size()), rect.data(), GL_STATIC_DRAW);
}

void Renderer::CreateDiamondVertexBuffer()
{
	// 타일 크기를 정점에 미리 박아 두면 스케일 1 이 타일 한 칸이 된다
	const float half_width = TILE_PIXEL_WIDTH * 0.5f * 2.f / static_cast<float>(m_window_size_x);
	const float half_height = TILE_PIXEL_HEIGHT * 0.5f * 2.f / static_cast<float>(m_window_size_y);

	const std::array<float, 18> diamond =
	{
		0.f, half_height, 0.f, half_width, 0.f, 0.f, 0.f, -half_height, 0.f,
		0.f, half_height, 0.f, 0.f, -half_height, 0.f, -half_width, 0.f, 0.f,
	};

	glGenBuffers(1, &m_vbo_diamond);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_diamond);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * diamond.size()), diamond.data(), GL_STATIC_DRAW);
}

void Renderer::CreateCircleVertexBuffer()
{
	// 가로세로에 각각 다른 픽셀 단위를 쓰므로 창 비율이 달라도 원이 안 찌그러진다
	const float unit_x = 2.f / static_cast<float>(m_window_size_x);
	const float unit_y = 2.f / static_cast<float>(m_window_size_y);

	std::vector<float> circle;
	circle.reserve(static_cast<size_t>(CIRCLE_SEGMENT_COUNT * 3 * 3));

	for (int32 segment = 0; segment < CIRCLE_SEGMENT_COUNT; ++segment)
	{
		const float angle_now = 2.f * PI * static_cast<float>(segment) / static_cast<float>(CIRCLE_SEGMENT_COUNT);
		const float angle_next = 2.f * PI * static_cast<float>(segment + 1) / static_cast<float>(CIRCLE_SEGMENT_COUNT);

		PushVertex(&circle, 0.f, 0.f);
		PushVertex(&circle, std::cos(angle_now) * unit_x, std::sin(angle_now) * unit_y);
		PushVertex(&circle, std::cos(angle_next) * unit_x, std::sin(angle_next) * unit_y);
	}

	m_circle_vertex_count = CIRCLE_SEGMENT_COUNT * 3;

	glGenBuffers(1, &m_vbo_circle);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_circle);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(circle.size() * sizeof(float)), circle.data(), GL_STATIC_DRAW);
}

void Renderer::CacheShaderLocations()
{
	m_affine_origin = glGetUniformLocation(m_affine_shader, "u_Origin");
	m_affine_axis_x = glGetUniformLocation(m_affine_shader, "u_AxisX");
	m_affine_axis_y = glGetUniformLocation(m_affine_shader, "u_AxisY");
	m_affine_region = glGetUniformLocation(m_affine_shader, "u_Region");
	m_affine_color = glGetUniformLocation(m_affine_shader, "u_Color");
	m_affine_texture = glGetUniformLocation(m_affine_shader, "u_Texture");
	m_affine_undo = glGetUniformLocation(m_affine_shader, "u_UndoToneMap");
	m_affine_position = glGetAttribLocation(m_affine_shader, "a_Position");

	m_trans_uniform = glGetUniformLocation(m_solid_rect_shader, "u_Trans");
	m_color_uniform = glGetUniformLocation(m_solid_rect_shader, "u_Color");
	m_position_attrib = glGetAttribLocation(m_solid_rect_shader, "a_Position");
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	DrawTriangles(m_vbo_rect, 6, new_x, new_y, size, size, r, g, b, a);
}

void Renderer::DrawSolidBox(float x, float y, float width, float height, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	DrawTriangles(m_vbo_rect, 6, new_x, new_y, width, height, r, g, b, a);
}

void Renderer::DrawSolidDiamond(float x, float y, float scale, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	DrawTriangles(m_vbo_diamond, 6, new_x, new_y, scale, scale, r, g, b, a);
}

void Renderer::DrawSolidCircle(float x, float y, float radius, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	DrawTriangles(m_vbo_circle, m_circle_vertex_count, new_x, new_y, radius, radius, r, g, b, a);
}

void Renderer::DrawTriangles(GLuint vbo, int32 vertex_count, float new_x, float new_y, float scale_x, float scale_y, float r, float g, float b, float a)
{
	// 속성을 못 찾았으면 음수라 그대로 넘기면 안 된다
	if (m_position_attrib < 0)
	{
		return;
	}

	glUseProgram(m_solid_rect_shader);

	glUniform4f(m_trans_uniform, new_x, new_y, scale_x, scale_y);
	glUniform4f(m_color_uniform, r, g, b, a);

	glEnableVertexAttribArray(m_position_attrib);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(m_position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	CountDrawCall();
	glDrawArrays(GL_TRIANGLES, 0, vertex_count);

	glDisableVertexAttribArray(m_position_attrib);
}

void Renderer::GetGLPosition(float x, float y, float* new_x, float* new_y)
{
	*new_x = x * 2.f / static_cast<float>(m_window_size_x);
	*new_y = y * 2.f / static_cast<float>(m_window_size_y);
}

void Renderer::DrawAffine(GLuint texture, const TexRegion& region, const AffineQuad& quad, float r, float g, float b, float a)
{
	if (m_affine_position < 0)
	{
		return;
	}

	const float to_ndc_x = 2.f / static_cast<float>(m_window_size_x);
	const float to_ndc_y = 2.f / static_cast<float>(m_window_size_y);

	glUseProgram(m_affine_shader);

	glUniform2f(m_affine_origin, quad.origin_x * to_ndc_x, quad.origin_y * to_ndc_y);
	glUniform2f(m_affine_axis_x, quad.axis_x_x * to_ndc_x, quad.axis_x_y * to_ndc_y);
	glUniform2f(m_affine_axis_y, quad.axis_y_x * to_ndc_x, quad.axis_y_y * to_ndc_y);
	glUniform4f(m_affine_region, region.u0, region.v0, region.u1, region.v1);
	glUniform4f(m_affine_color, r, g, b, a);

	int32 undo = 0;
	if (m_undo_tone_map)
	{
		undo = 1;
	}
	glUniform1i(m_affine_undo, undo);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(m_affine_texture, 0);

	glEnableVertexAttribArray(m_affine_position);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_unit_quad);
	glVertexAttribPointer(m_affine_position, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	CountDrawCall();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(m_affine_position);
}

void Renderer::SetUndoToneMap(bool undo)
{
	m_undo_tone_map = undo;
}
