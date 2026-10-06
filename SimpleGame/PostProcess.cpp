#include "stdafx.h"
#include "PostProcess.h"
#include "Renderer.h"
#include "ShaderUtil.h"
#include "Profiler.h"

#include "Dependencies\glew.h"

#include <array>
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
	const int32 BLUR_PASS_COUNT = 3;

	// 씬 자체가 이미 절반이라 여기서 또 넷으로 나누면 번짐이 두 배로 퍼진다
	const int32 BLOOM_DIVISOR = 2;

	struct RenderTarget
	{
		GLuint fbo = 0;
		GLuint texture = 0;
		int32 width = 0;
		int32 height = 0;
	};

	int32 g_render_width = 0;
	int32 g_render_height = 0;
	int32 g_window_width = 0;
	int32 g_window_height = 0;
	bool g_ready = false;

	RenderTarget g_scene;
	RenderTarget g_bright;
	RenderTarget g_ping;
	RenderTarget g_pong;
	RenderTarget g_post;

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

	GLuint g_upscale_program = 0;
	GLint g_upscale_source = -1;
	GLint g_upscale_size = -1;
	GLint g_upscale_position = -1;

	bool CreateRenderTarget(RenderTarget* target, int32 width, int32 height, GLint filter)
	{
		target->width = width;
		target->height = height;

		glGenTextures(1, &target->texture);
		glBindTexture(GL_TEXTURE_2D, target->texture);

		// 부동소수 포맷이라야 1 을 넘는 값이 저장된다. 8비트면 여기서 잘린다
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

		// 블룸 축소본은 되키워 읽으니 선형, 도트 확대는 최근접이라야 블록이 안 뭉갠다
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

		// 가장자리에서 반대편을 물어 오면 빛이 화면 끝에서 반대쪽으로 샌다
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &target->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);

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
		if (target->fbo != 0)
		{
			glDeleteFramebuffers(1, &target->fbo);
			target->fbo = 0;
		}

		if (target->texture != 0)
		{
			glDeleteTextures(1, &target->texture);
			target->texture = 0;
		}
	}

	void CreateFullScreenQuad()
	{
		// 정규화 장치 좌표를 그대로 쓰므로 정점 셰이더가 통과만 시키면 된다.
		// 텍스처 좌표는 gl_FragCoord 로 만들어 쓰기 때문에 속성이 필요 없다
		const std::array<float, 18> quad =
		{
			-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 1.f, 1.f, 0.f,
			-1.f, -1.f, 0.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f,
		};

		glGenBuffers(1, &g_fullscreen_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_fullscreen_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * quad.size()), quad.data(), GL_STATIC_DRAW);
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

		CountDrawCall();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(position_attrib);
	}

	void BlurOnce(const RenderTarget& source, const RenderTarget& target, float direction_x, float direction_y)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
		glViewport(0, 0, target.width, target.height);

		glUseProgram(g_blur_program);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, source.texture);
		glUniform1i(g_blur_source, 0);

		glUniform2f(g_blur_size, static_cast<float>(target.width), static_cast<float>(target.height));
		glUniform2f(g_blur_direction, direction_x, direction_y);

		DrawFullScreen(g_blur_position);
	}
}

bool InitPostProcess(int32 render_width, int32 render_height, int32 window_width, int32 window_height)
{
	g_render_width = render_width;
	g_render_height = render_height;
	g_window_width = window_width;
	g_window_height = window_height;

	const int32 bloom_width = render_width / BLOOM_DIVISOR;
	const int32 bloom_height = render_height / BLOOM_DIVISOR;

	if (CreateRenderTarget(&g_scene, render_width, render_height, GL_LINEAR) == false)
	{
		return false;
	}

	if (CreateRenderTarget(&g_bright, bloom_width, bloom_height, GL_LINEAR) == false)
	{
		return false;
	}

	if (CreateRenderTarget(&g_ping, bloom_width, bloom_height, GL_LINEAR) == false)
	{
		return false;
	}

	if (CreateRenderTarget(&g_pong, bloom_width, bloom_height, GL_LINEAR) == false)
	{
		return false;
	}

	if (CreateRenderTarget(&g_post, render_width, render_height, GL_NEAREST) == false)
	{
		return false;
	}

	CreateFullScreenQuad();

	g_bright_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/BrightPass.fs");
	g_blur_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Blur.fs");
	g_composite_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Composite.fs");
	g_upscale_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Upscale.fs");

	if (g_bright_program == 0 || g_blur_program == 0 || g_composite_program == 0 || g_upscale_program == 0)
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

	g_upscale_source = glGetUniformLocation(g_upscale_program, "u_Source");
	g_upscale_size = glGetUniformLocation(g_upscale_program, "u_ScreenSize");
	g_upscale_position = glGetAttribLocation(g_upscale_program, "a_Position");

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
	DestroyRenderTarget(&g_post);

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

	if (g_upscale_program != 0)
	{
		glDeleteProgram(g_upscale_program);
		g_upscale_program = 0;
	}

	g_ready = false;
}

void BeginScenePass()
{
	// 앞 프레임이 남긴 것이 새 버퍼에 딸려 들어가면 안 된다
	FlushRenderBatch();

	if (g_ready == false)
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, g_scene.fbo);
	glViewport(0, 0, g_scene.width, g_scene.height);
}

void EndScenePassAndComposite()
{
	// 씬에 그린 것이 전부 나간 뒤라야 밝은 부분을 뽑을 수 있다
	FlushRenderBatch();

	if (g_ready == false)
	{
		return;
	}

	BeginProfileStage(PROFILE_STAGE_POST);

	// 후처리는 덮어쓰기라 알파 혼합이 켜져 있으면 결과가 섞인다
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, g_bright.fbo);
	glViewport(0, 0, g_bright.width, g_bright.height);

	glUseProgram(g_bright_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_scene.texture);
	glUniform1i(g_bright_scene, 0);
	glUniform2f(g_bright_size, static_cast<float>(g_bright.width), static_cast<float>(g_bright.height));
	glUniform1f(g_bright_threshold, BLOOM_THRESHOLD);

	DrawFullScreen(g_bright_position);

	// 가로와 세로를 갈라 걸어야 한 번에 아홉 점만 물어도 넓게 번진다
	BlurOnce(g_bright, g_ping, 1.f, 0.f);
	BlurOnce(g_ping, g_pong, 0.f, 1.f);

	for (int32 pass = 1; pass < BLUR_PASS_COUNT; ++pass)
	{
		BlurOnce(g_pong, g_ping, 1.f, 0.f);
		BlurOnce(g_ping, g_pong, 0.f, 1.f);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, g_post.fbo);
	glViewport(0, 0, g_post.width, g_post.height);

	glUseProgram(g_composite_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_scene.texture);
	glUniform1i(g_composite_scene, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_pong.texture);
	glUniform1i(g_composite_bloom, 1);

	glUniform2f(g_composite_size, static_cast<float>(g_render_width), static_cast<float>(g_render_height));
	glUniform1f(g_composite_strength, BLOOM_STRENGTH);
	glUniform1f(g_composite_exposure, EXPOSURE);
	glUniform1f(g_composite_blur_start, BLUR_START);
	const float render_ratio = static_cast<float>(g_render_width) / static_cast<float>(g_window_width);
	glUniform1f(g_composite_blur_radius, BLUR_RADIUS * render_ratio);
	glUniform1f(g_composite_vignette, VIGNETTE_EDGE);

	DrawFullScreen(g_composite_position);

	// 다음 프레임의 도형 그리기가 0번 유닛을 기대하므로 되돌린다
	glActiveTexture(GL_TEXTURE0);

	// 저해상도 버퍼를 묶은 채로 나간다. HUD 가 여기 얹혀야 같은 격자를 탄다
	glEnable(GL_BLEND);

	EndProfileStage();
}

void PresentUpscaled()
{
	// HUD 까지 전부 내보낸 뒤에 확대한다
	FlushRenderBatch();

	if (g_ready == false)
	{
		return;
	}

	BeginProfileStage(PROFILE_STAGE_POST);

	// 복사라서 섞이면 안 된다
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_window_width, g_window_height);

	glUseProgram(g_upscale_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_post.texture);
	glUniform1i(g_upscale_source, 0);

	glUniform2f(g_upscale_size, static_cast<float>(g_window_width), static_cast<float>(g_window_height));

	DrawFullScreen(g_upscale_position);

	glEnable(GL_BLEND);

	EndProfileStage();
}
