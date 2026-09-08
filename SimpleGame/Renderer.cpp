#include "stdafx.h"
#include "Renderer.h"
#include "WorldPos.h"
#include "ShaderUtil.h"

#include <vector>
#include <cmath>

namespace
{
	const int CIRCLE_SEGMENT_COUNT = 48;
	const float PI = 3.14159265f;

	void PushVertex(std::vector<float>* target, float x, float y)
	{
		target->push_back(x);
		target->push_back(y);
		target->push_back(0.f);
	}
}

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}


Renderer::~Renderer()
{
	if (m_VBORect != 0)
	{
		glDeleteBuffers(1, &m_VBORect);
		m_VBORect = 0;
	}

	if (m_VBODiamond != 0)
	{
		glDeleteBuffers(1, &m_VBODiamond);
		m_VBODiamond = 0;
	}

	if (m_VBOCircle != 0)
	{
		glDeleteBuffers(1, &m_VBOCircle);
		m_VBOCircle = 0;
	}

	if (m_SolidRectShader != 0)
	{
		glDeleteProgram(m_SolidRectShader);
		m_SolidRectShader = 0;
	}
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	//Set window size
	m_WindowSizeX = static_cast<unsigned int>(windowSizeX);
	m_WindowSizeY = static_cast<unsigned int>(windowSizeY);

	// 켜두지 않으면 알파 인자가 통째로 죽는다
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Load shaders
	m_SolidRectShader = CompileShaderProgram("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	
	//Create VBOs
	CreateVertexBufferObjects();

	if (m_SolidRectShader > 0 && m_VBORect > 0 && m_VBODiamond > 0 && m_VBOCircle > 0)
	{
		CacheShaderLocations();
		m_Initialized = true;
	}
}

bool Renderer::IsInitialized()
{
	return m_Initialized;
}

void Renderer::CreateVertexBufferObjects()
{
	CreateRectVertexBuffer();
	CreateDiamondVertexBuffer();
	CreateCircleVertexBuffer();
}

void Renderer::CreateRectVertexBuffer()
{
	float rect[]
		=
	{
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, -1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, //Triangle1
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f,  1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, //Triangle2
	};

	glGenBuffers(1, &m_VBORect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);
}

void Renderer::CreateDiamondVertexBuffer()
{
	// 타일 크기를 정점에 미리 박아 두면 스케일 1 이 타일 한 칸이 된다
	const float half_width = TILE_PIXEL_WIDTH * 0.5f * 2.f / static_cast<float>(m_WindowSizeX);
	const float half_height = TILE_PIXEL_HEIGHT * 0.5f * 2.f / static_cast<float>(m_WindowSizeY);

	float diamond[]
		=
	{
		0.f, half_height, 0.f, half_width, 0.f, 0.f, 0.f, -half_height, 0.f, //Triangle1
		0.f, half_height, 0.f, 0.f, -half_height, 0.f, -half_width, 0.f, 0.f, //Triangle2
	};

	glGenBuffers(1, &m_VBODiamond);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODiamond);
	glBufferData(GL_ARRAY_BUFFER, sizeof(diamond), diamond, GL_STATIC_DRAW);
}

void Renderer::CreateCircleVertexBuffer()
{
	// 가로세로에 각각 다른 픽셀 단위를 쓰므로 창 비율이 달라도 원이 안 찌그러진다
	const float unit_x = 2.f / static_cast<float>(m_WindowSizeX);
	const float unit_y = 2.f / static_cast<float>(m_WindowSizeY);

	std::vector<float> circle;
	circle.reserve(CIRCLE_SEGMENT_COUNT * 3 * 3);

	for (int segment = 0; segment < CIRCLE_SEGMENT_COUNT; ++segment)
	{
		const float angle_now = 2.f * PI * static_cast<float>(segment) / static_cast<float>(CIRCLE_SEGMENT_COUNT);
		const float angle_next = 2.f * PI * static_cast<float>(segment + 1) / static_cast<float>(CIRCLE_SEGMENT_COUNT);

		PushVertex(&circle, 0.f, 0.f);
		PushVertex(&circle, std::cos(angle_now) * unit_x, std::sin(angle_now) * unit_y);
		PushVertex(&circle, std::cos(angle_next) * unit_x, std::sin(angle_next) * unit_y);
	}

	m_circle_vertex_count = CIRCLE_SEGMENT_COUNT * 3;

	glGenBuffers(1, &m_VBOCircle);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOCircle);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(circle.size() * sizeof(float)), circle.data(), GL_STATIC_DRAW);
}

void Renderer::CacheShaderLocations()
{
	m_trans_uniform = glGetUniformLocation(m_SolidRectShader, "u_Trans");
	m_color_uniform = glGetUniformLocation(m_SolidRectShader, "u_Color");
	m_position_attrib = glGetAttribLocation(m_SolidRectShader, "a_Position");
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	DrawTriangles(m_VBORect, 6, newX, newY, size, size, r, g, b, a);
}

void Renderer::DrawSolidBox(float x, float y, float width, float height, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	DrawTriangles(m_VBORect, 6, newX, newY, width, height, r, g, b, a);
}

void Renderer::DrawSolidDiamond(float x, float y, float scale, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	DrawTriangles(m_VBODiamond, 6, newX, newY, scale, scale, r, g, b, a);
}

void Renderer::DrawSolidCircle(float x, float y, float radius, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	DrawTriangles(m_VBOCircle, m_circle_vertex_count, newX, newY, radius, radius, r, g, b, a);
}

void Renderer::DrawTriangles(GLuint vbo, int vertex_count, float newX, float newY, float scale_x, float scale_y, float r, float g, float b, float a)
{
	// 속성을 못 찾았으면 음수라 그대로 넘기면 안 된다
	if (m_position_attrib < 0)
	{
		return;
	}

	//Program select
	glUseProgram(m_SolidRectShader);

	glUniform4f(m_trans_uniform, newX, newY, scale_x, scale_y);
	glUniform4f(m_color_uniform, r, g, b, a);

	glEnableVertexAttribArray(m_position_attrib);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(m_position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, vertex_count);

	glDisableVertexAttribArray(m_position_attrib);
}

void Renderer::GetGLPosition(float x, float y, float *newX, float *newY)
{
	*newX = x * 2.f / m_WindowSizeX;
	*newY = y * 2.f / m_WindowSizeY;
}
