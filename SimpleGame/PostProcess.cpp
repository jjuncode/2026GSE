#include "stdafx.h"
#include "PostProcess.h"
#include "ShaderUtil.h"

#include "Dependencies\glew.h"

#include <iostream>

namespace
{
	// 1 을 넘는 부분만 번지게 한다. 이 값이 0 이면 어두운 곳까지 뿌옇게 뜬다
	const float BLOOM_THRESHOLD = 1.0f;
	const float BLOOM_STRENGTH = 0.9f;

	// 톤매핑이 밝은 쪽을 눌러 전체가 어두워지므로 되올린다
	const float EXPOSURE = 1.0f;

	// 화면 정규화 거리가 이만큼 넘어가야 흐려지기 시작한다
	const float BLUR_START = 0.45f;
	const float BLUR_RADIUS = 3.5f;
	const float VIGNETTE_EDGE = 0.15f;

	// 왕복 횟수를 늘리면 번짐이 넓어진다. 해상도를 낮춰 잡아 한 번이 싸다
	const int BLUR_PASS_COUNT = 3;
	const int BLOOM_DIVISOR = 4;

	struct RenderTarget
	{
		GLuint m_fbo = 0;
		GLuint m_texture = 0;
		int m_width = 0;
		int m_height = 0;
	};

	int g_screen_width = 0;
	int g_screen_height = 0;
	bool g_ready = false;

	RenderTarget g_scene;
	RenderTarget g_bright;
	RenderTarget g_ping;
	RenderTarget g_pong;

	GLuint g_fullscreen_vbo = 0;

	GLuint g_bright_program = 0;
	GLint g_bright_scene = -1;
	GLint g_bright_size = -1;
	GLint g_bright_threshold = -1;
	GLint g_bright_position = -1;

	GLuint g_blur_program = 0;
	GLint g_blur_source = -1;
	GLint g_blur_size = -1;
	GLint g_blur_direction = -1;
	GLint g_blur_position = -1;

	GLuint g_composite_program = 0;
	GLint g_composite_scene = -1;
	GLint g_composite_bloom = -1;
	GLint g_composite_size = -1;
	GLint g_composite_strength = -1;
	GLint g_composite_exposure = -1;
	GLint g_composite_blur_start = -1;
	GLint g_composite_blur_radius = -1;
	GLint g_composite_vignette = -1;
	GLint g_composite_position = -1;

	bool CreateRenderTarget(RenderTarget* target, int width, int height)
	{
		target->m_width = width;
		target->m_height = height;

		glGenTextures(1, &target->m_texture);
		glBindTexture(GL_TEXTURE_2D, target->m_texture);

		// 부동소수 포맷이라야 1 을 넘는 값이 저장된다. 8비트면 여기서 잘린다
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

		// 축소본을 다시 키워 읽으므로 선형 보간이 필수다
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// 가장자리에서 반대편을 물어 오면 빛이 화면 끝에서 반대쪽으로 샌다
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &target->m_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, target->m_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->m_texture, 0);

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cout << "[ PostProcess - CreateRenderTarget ] : framebuffer incomplete\n";
			return false;
		}

		return true;
	}

	void DestroyRenderTarget(RenderTarget* target)
	{
		if (target->m_fbo != 0)
		{
			glDeleteFramebuffers(1, &target->m_fbo);
			target->m_fbo = 0;
		}

		if (target->m_texture != 0)
		{
			glDeleteTextures(1, &target->m_texture);
			target->m_texture = 0;
		}
	}

	void CreateFullScreenQuad()
	{
		// 정규화 장치 좌표를 그대로 쓰므로 정점 셰이더가 통과만 시키면 된다.
		// 텍스처 좌표는 gl_FragCoord 로 만들어 쓰기 때문에 속성이 필요 없다
		const float quad[]
			=
		{
			-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 1.f, 1.f, 0.f,
			-1.f, -1.f, 0.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f,
		};

		glGenBuffers(1, &g_fullscreen_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_fullscreen_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	}

	void DrawFullScreen(GLint position_attrib)
	{
		if (position_attrib < 0)
		{
			return;
		}

		glEnableVertexAttribArray(position_attrib);
		glBindBuffer(GL_ARRAY_BUFFER, g_fullscreen_vbo);
		glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(position_attrib);
	}

	void BlurOnce(const RenderTarget& source, const RenderTarget& target, float direction_x, float direction_y)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, target.m_fbo);
		glViewport(0, 0, target.m_width, target.m_height);

		glUseProgram(g_blur_program);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, source.m_texture);
		glUniform1i(g_blur_source, 0);

		glUniform2f(g_blur_size, static_cast<float>(target.m_width), static_cast<float>(target.m_height));
		glUniform2f(g_blur_direction, direction_x, direction_y);

		DrawFullScreen(g_blur_position);
	}
}

bool InitPostProcess(int screen_width, int screen_height)
{
	g_screen_width = screen_width;
	g_screen_height = screen_height;

	const int bloom_width = screen_width / BLOOM_DIVISOR;
	const int bloom_height = screen_height / BLOOM_DIVISOR;

	if (!CreateRenderTarget(&g_scene, screen_width, screen_height))
	{
		return false;
	}

	if (!CreateRenderTarget(&g_bright, bloom_width, bloom_height))
	{
		return false;
	}

	if (!CreateRenderTarget(&g_ping, bloom_width, bloom_height))
	{
		return false;
	}

	if (!CreateRenderTarget(&g_pong, bloom_width, bloom_height))
	{
		return false;
	}

	CreateFullScreenQuad();

	g_bright_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/BrightPass.fs");
	g_blur_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Blur.fs");
	g_composite_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Composite.fs");

	if (g_bright_program == 0 || g_blur_program == 0 || g_composite_program == 0)
	{
		return false;
	}

	g_bright_scene = glGetUniformLocation(g_bright_program, "u_Scene");
	g_bright_size = glGetUniformLocation(g_bright_program, "u_TargetSize");
	g_bright_threshold = glGetUniformLocation(g_bright_program, "u_Threshold");
	g_bright_position = glGetAttribLocation(g_bright_program, "a_Position");

	g_blur_source = glGetUniformLocation(g_blur_program, "u_Source");
	g_blur_size = glGetUniformLocation(g_blur_program, "u_TargetSize");
	g_blur_direction = glGetUniformLocation(g_blur_program, "u_Direction");
	g_blur_position = glGetAttribLocation(g_blur_program, "a_Position");

	g_composite_scene = glGetUniformLocation(g_composite_program, "u_Scene");
	g_composite_bloom = glGetUniformLocation(g_composite_program, "u_Bloom");
	g_composite_size = glGetUniformLocation(g_composite_program, "u_ScreenSize");
	g_composite_strength = glGetUniformLocation(g_composite_program, "u_BloomStrength");
	g_composite_exposure = glGetUniformLocation(g_composite_program, "u_Exposure");
	g_composite_blur_start = glGetUniformLocation(g_composite_program, "u_BlurStart");
	g_composite_blur_radius = glGetUniformLocation(g_composite_program, "u_BlurRadius");
	g_composite_vignette = glGetUniformLocation(g_composite_program, "u_VignetteEdge");
	g_composite_position = glGetAttribLocation(g_composite_program, "a_Position");

	g_ready = true;

	std::cout << "[ PostProcess - InitPostProcess ] : ready\n";

	return true;
}

void ShutdownPostProcess()
{
	DestroyRenderTarget(&g_scene);
	DestroyRenderTarget(&g_bright);
	DestroyRenderTarget(&g_ping);
	DestroyRenderTarget(&g_pong);

	if (g_fullscreen_vbo != 0)
	{
		glDeleteBuffers(1, &g_fullscreen_vbo);
		g_fullscreen_vbo = 0;
	}

	if (g_bright_program != 0)
	{
		glDeleteProgram(g_bright_program);
		g_bright_program = 0;
	}

	if (g_blur_program != 0)
	{
		glDeleteProgram(g_blur_program);
		g_blur_program = 0;
	}

	if (g_composite_program != 0)
	{
		glDeleteProgram(g_composite_program);
		g_composite_program = 0;
	}

	g_ready = false;
}

void BeginScenePass()
{
	if (!g_ready)
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, g_scene.m_fbo);
	glViewport(0, 0, g_scene.m_width, g_scene.m_height);
}

void EndScenePassAndComposite()
{
	if (!g_ready)
	{
		return;
	}

	// 후처리는 덮어쓰기라 알파 혼합이 켜져 있으면 결과가 섞인다
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, g_bright.m_fbo);
	glViewport(0, 0, g_bright.m_width, g_bright.m_height);

	glUseProgram(g_bright_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_scene.m_texture);
	glUniform1i(g_bright_scene, 0);
	glUniform2f(g_bright_size, static_cast<float>(g_bright.m_width), static_cast<float>(g_bright.m_height));
	glUniform1f(g_bright_threshold, BLOOM_THRESHOLD);

	DrawFullScreen(g_bright_position);

	// 가로와 세로를 갈라 걸어야 한 번에 아홉 점만 물어도 넓게 번진다
	BlurOnce(g_bright, g_ping, 1.f, 0.f);
	BlurOnce(g_ping, g_pong, 0.f, 1.f);

	for (int pass = 1; pass < BLUR_PASS_COUNT; ++pass)
	{
		BlurOnce(g_pong, g_ping, 1.f, 0.f);
		BlurOnce(g_ping, g_pong, 0.f, 1.f);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_screen_width, g_screen_height);

	glUseProgram(g_composite_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_scene.m_texture);
	glUniform1i(g_composite_scene, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_pong.m_texture);
	glUniform1i(g_composite_bloom, 1);

	glUniform2f(g_composite_size, static_cast<float>(g_screen_width), static_cast<float>(g_screen_height));
	glUniform1f(g_composite_strength, BLOOM_STRENGTH);
	glUniform1f(g_composite_exposure, EXPOSURE);
	glUniform1f(g_composite_blur_start, BLUR_START);
	glUniform1f(g_composite_blur_radius, BLUR_RADIUS);
	glUniform1f(g_composite_vignette, VIGNETTE_EDGE);

	DrawFullScreen(g_composite_position);

	// 다음 프레임의 도형 그리기가 0번 유닛을 기대하므로 되돌린다
	glActiveTexture(GL_TEXTURE0);

	glEnable(GL_BLEND);
}
