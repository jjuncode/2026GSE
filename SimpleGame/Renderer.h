#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();

	// 정사각형. size 는 픽셀
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);
	// 가로세로가 다른 사각형. 막대와 패널용
	void DrawSolidBox(float x, float y, float width, float height, float r, float g, float b, float a);
	// 아이소 타일. scale 1 이 타일 한 칸
	void DrawSolidDiamond(float x, float y, float scale, float r, float g, float b, float a);
	// radius 는 픽셀. 창 비율이 달라도 찌그러지지 않는다
	void DrawSolidCircle(float x, float y, float radius, float r, float g, float b, float a);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	void CreateVertexBufferObjects();
	void CreateRectVertexBuffer();
	void CreateDiamondVertexBuffer();
	void CreateCircleVertexBuffer();
	void CacheShaderLocations();
	void GetGLPosition(float x, float y, float *newX, float *newY);
	void DrawTriangles(GLuint vbo, int vertex_count, float newX, float newY, float scale_x, float scale_y, float r, float g, float b, float a);

	bool m_Initialized = false;
	
	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_VBORect = 0;
	GLuint m_VBODiamond = 0;
	GLuint m_VBOCircle = 0;
	GLuint m_SolidRectShader = 0;

	int m_circle_vertex_count = 0;

	// 그리기마다 문자열로 찾으면 프레임당 수천 번 검색이라 링크 시점에 받아 둔다
	GLint m_trans_uniform = -1;
	GLint m_color_uniform = -1;
	GLint m_position_attrib = -1;
};
