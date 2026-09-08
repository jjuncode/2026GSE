#include "stdafx.h"
#include "Fog.h"
#include "ShaderUtil.h"

#include "Dependencies\glew.h"

#include <iostream>

namespace
{
	int g_screen_width = 0;
	int g_screen_height = 0;
	bool g_ready = false;

	GLuint g_program = 0;
	GLuint g_vbo = 0;

	GLint g_position_attrib = -1;
	GLint g_screen_size = -1;
	GLint g_scroll = -1;

	void CreateFullScreenQuad()
	{
		const float quad[]
			=
		{
			-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 1.f, 1.f, 0.f,
			-1.f, -1.f, 0.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f,
		};

		glGenBuffers(1, &g_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	}
}

bool InitFog(int screen_width, int screen_height)
{
	g_screen_width = screen_width;
	g_screen_height = screen_height;

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
	if (!g_ready)
	{
		return;
	}

	glUseProgram(g_program);

	glUniform2f(g_screen_size, static_cast<float>(g_screen_width), static_cast<float>(g_screen_height));
	glUniform1f(g_scroll, scroll);

	glEnableVertexAttribArray(g_position_attrib);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glVertexAttribPointer(g_position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(g_position_attrib);
}
