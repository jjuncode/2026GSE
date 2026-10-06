#include "stdafx.h"
#include "Fog.h"
#include "Renderer.h"
#include "ShaderUtil.h"
#include "Profiler.h"

#include "Dependencies\glew.h"

#include <array>
#include <iostream>

namespace
{
	int32 g_screen_width = 0;
	int32 g_screen_height = 0;
	float g_pixel_scale = 1.f;
	bool g_ready = false;

	GLuint g_program = 0;
	GLuint g_vbo = 0;

	GLint g_position_attrib = -1;
	GLint g_screen_size = -1;
	GLint g_pixel_scale_uniform = -1;
	GLint g_scroll = -1;

	void CreateFullScreenQuad()
	{
		const std::array<float, 18> quad =
		{
			-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 1.f, 1.f, 0.f,
			-1.f, -1.f, 0.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f,
		};

		glGenBuffers(1, &g_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * quad.size()), quad.data(), GL_STATIC_DRAW);
	}
}

bool InitFog(int32 screen_width, int32 screen_height, float pixel_scale)
{
	g_screen_width = screen_width;
	g_screen_height = screen_height;
	g_pixel_scale = pixel_scale;

	g_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Fog.fs");

	if (g_program == 0)
	{
		return false;
	}

	CreateFullScreenQuad();

	if (g_vbo == 0)
	{
		return false;
	}

	g_position_attrib = glGetAttribLocation(g_program, "a_Position");
	g_screen_size = glGetUniformLocation(g_program, "u_ScreenSize");
	g_pixel_scale_uniform = glGetUniformLocation(g_program, "u_PixelScale");
	g_scroll = glGetUniformLocation(g_program, "u_Scroll");

	if (g_position_attrib < 0)
	{
		return false;
	}

	g_ready = true;

	std::cout << "[ Fog - InitFog ] : ready\n";

	return true;
}

void ShutdownFog()
{
	if (g_vbo != 0)
	{
		glDeleteBuffers(1, &g_vbo);
		g_vbo = 0;
	}

	if (g_program != 0)
	{
		glDeleteProgram(g_program);
		g_program = 0;
	}

	g_ready = false;
}

void DrawFogLayer(float scroll)
{
	// 안개는 물체 위에 얹히는 층이라 쌓아 둔 그림이 먼저 나가야 한다
	FlushRenderBatch();

	if (g_ready == false)
	{
		return;
	}

	BeginProfileStage(PROFILE_STAGE_FOG);

	glUseProgram(g_program);

	glUniform2f(g_screen_size, static_cast<float>(g_screen_width), static_cast<float>(g_screen_height));
	glUniform1f(g_pixel_scale_uniform, g_pixel_scale);
	glUniform1f(g_scroll, scroll);

	glEnableVertexAttribArray(g_position_attrib);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glVertexAttribPointer(g_position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	CountDrawCall();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(g_position_attrib);

	EndProfileStage();
}
