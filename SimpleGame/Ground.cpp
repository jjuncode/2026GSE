#include "stdafx.h"
#include "Ground.h"
#include "Renderer.h"
#include "ShaderUtil.h"
#include "Profiler.h"
#include "WorldPos.h"

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
	GLint g_tile_size = -1;
	GLint g_camera_tile = -1;
	GLint g_camera_frac = -1;
	GLint g_chunk_tile_count = -1;
	GLint g_show_chunk_grid = -1;

	GLint g_light_count = -1;
	GLint g_light_offset = -1;
	GLint g_light_radius = -1;
	GLint g_light_color = -1;
	GLint g_light_strength = -1;

	GLuint g_zone_texture = 0;
	int32 g_zone_size = 0;
	int32 g_zone_origin_x = 0;
	int32 g_zone_origin_y = 0;

	GLint g_zone_tiles_uniform = -1;
	GLint g_zone_origin_uniform = -1;
	GLint g_zone_size_uniform = -1;

	void CreateFullScreenQuad()
	{
		// 정규화 장치 좌표를 그대로 쓴다. 월드 좌표는 셰이더가 gl_FragCoord 로 역산한다
		const std::array<float, 18> quad =
		{
			-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 1.f, 1.f, 0.f,
			-1.f, -1.f, 0.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f,
		};

		glGenBuffers(1, &g_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(float) * quad.size()), quad.data(), GL_STATIC_DRAW);
	}

	void CacheUniformLocations()
	{
		g_position_attrib = glGetAttribLocation(g_program, "a_Position");

		g_screen_size = glGetUniformLocation(g_program, "u_ScreenSize");
		g_pixel_scale_uniform = glGetUniformLocation(g_program, "u_PixelScale");
		g_tile_size = glGetUniformLocation(g_program, "u_TileSize");
		g_camera_tile = glGetUniformLocation(g_program, "u_CameraTile");
		g_camera_frac = glGetUniformLocation(g_program, "u_CameraFrac");
		g_chunk_tile_count = glGetUniformLocation(g_program, "u_ChunkTileCount");
		g_show_chunk_grid = glGetUniformLocation(g_program, "u_ShowChunkGrid");

		g_light_count = glGetUniformLocation(g_program, "u_LightCount");
		g_light_offset = glGetUniformLocation(g_program, "u_LightOffset");
		g_light_radius = glGetUniformLocation(g_program, "u_LightRadius");
		g_light_color = glGetUniformLocation(g_program, "u_LightColor");
		g_light_strength = glGetUniformLocation(g_program, "u_LightStrength");

		g_zone_tiles_uniform = glGetUniformLocation(g_program, "u_ZoneTiles");
		g_zone_origin_uniform = glGetUniformLocation(g_program, "u_ZoneOrigin");
		g_zone_size_uniform = glGetUniformLocation(g_program, "u_ZoneSize");
	}

	int32 ClampLightCount(int32 count)
	{
		if (count < 0)
		{
			return 0;
		}

		if (count > GROUND_MAX_LIGHT_COUNT)
		{
			return GROUND_MAX_LIGHT_COUNT;
		}

		return count;
	}

	// 구조체 배열은 uniform 배열로 못 올린다. 항목별로 갈라 담는다
	void UploadLights(const GroundFrame& frame, int32 count)
	{
		std::array<float, GROUND_MAX_LIGHT_COUNT * 2> offsets = {};
		std::array<float, GROUND_MAX_LIGHT_COUNT> radius = {};
		std::array<float, GROUND_MAX_LIGHT_COUNT * 3> colors = {};
		std::array<float, GROUND_MAX_LIGHT_COUNT> strength = {};

		for (int32 index = 0; index < count; ++index)
		{
			const GroundLight& light = frame.lights[static_cast<size_t>(index)];

			offsets[static_cast<size_t>(index) * 2 + 0] = light.tile_offset_x;
			offsets[static_cast<size_t>(index) * 2 + 1] = light.tile_offset_y;

			radius[static_cast<size_t>(index)] = light.radius;

			colors[static_cast<size_t>(index) * 3 + 0] = light.r;
			colors[static_cast<size_t>(index) * 3 + 1] = light.g;
			colors[static_cast<size_t>(index) * 3 + 2] = light.b;

			strength[static_cast<size_t>(index)] = light.strength;
		}

		glUniform1i(g_light_count, count);

		if (count == 0)
		{
			return;
		}

		glUniform2fv(g_light_offset, count, offsets.data());
		glUniform1fv(g_light_radius, count, radius.data());
		glUniform3fv(g_light_color, count, colors.data());
		glUniform1fv(g_light_strength, count, strength.data());
	}
}

bool InitGround(int32 screen_width, int32 screen_height, float pixel_scale)
{
	g_screen_width = screen_width;
	g_screen_height = screen_height;
	g_pixel_scale = pixel_scale;

	// 정점 셰이더는 좌표를 통과만 시키므로 후처리 것을 그대로 쓴다
	g_program = CompileShaderProgram("./Shaders/FullScreen.vs", "./Shaders/Ground.fs");

	if (g_program == 0)
	{
		return false;
	}

	CreateFullScreenQuad();

	if (g_vbo == 0)
	{
		return false;
	}

	CacheUniformLocations();

	if (g_position_attrib < 0)
	{
		return false;
	}

	g_ready = true;

	std::cout << "[ Ground - InitGround ] : ready\n";

	return true;
}

void ShutdownGround()
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

	if (g_zone_texture != 0)
	{
		glDeleteTextures(1, &g_zone_texture);
		g_zone_texture = 0;
	}

	g_ready = false;
}

void SetGroundZone(const std::vector<uint8>& codes, int32 size, int32 origin_tile_x, int32 origin_tile_y)
{
	if (static_cast<int32>(codes.size()) != size * size)
	{
		std::cout << "[ Ground - SetGroundZone ] : code count mismatch\n";
		return;
	}

	if (g_zone_texture == 0)
	{
		glGenTextures(1, &g_zone_texture);
	}

	glBindTexture(GL_TEXTURE_2D, g_zone_texture);

	// 한 칸 한 바이트라 행 끝 정렬을 1 로 바꿔야 줄이 밀리지 않는다
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, codes.data());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	g_zone_size = size;
	g_zone_origin_x = origin_tile_x;
	g_zone_origin_y = origin_tile_y;
}

void DrawGroundLayer(const GroundFrame& frame)
{
	// 모아 둔 그림을 먼저 내보내야 바닥이 그 위를 덮지 않는다
	FlushRenderBatch();

	if (g_ready == false)
	{
		return;
	}

	BeginProfileStage(PROFILE_STAGE_GROUND);

	glUseProgram(g_program);

	glUniform2f(g_screen_size, static_cast<float>(g_screen_width), static_cast<float>(g_screen_height));
	glUniform1f(g_pixel_scale_uniform, g_pixel_scale);
	glUniform2f(g_tile_size, TILE_PIXEL_WIDTH, TILE_PIXEL_HEIGHT);

	glUniform2i(g_camera_tile, frame.camera_tile_x, frame.camera_tile_y);
	glUniform2f(g_camera_frac, frame.camera_frac_x, frame.camera_frac_y);

	glUniform1i(g_chunk_tile_count, CHUNK_TILE_COUNT);

	int32 show_chunk_grid = 0;
	if (frame.show_chunk_grid)
	{
		show_chunk_grid = 1;
	}
	glUniform1i(g_show_chunk_grid, show_chunk_grid);

	const int32 count = ClampLightCount(frame.light_count);
	UploadLights(frame, count);

	// 0 번 유닛은 도형과 그림이 쓴다
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_zone_texture);
	glUniform1i(g_zone_tiles_uniform, 1);
	glUniform2i(g_zone_origin_uniform, g_zone_origin_x, g_zone_origin_y);
	glUniform1i(g_zone_size_uniform, g_zone_size);

	glEnableVertexAttribArray(g_position_attrib);
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glVertexAttribPointer(g_position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	CountDrawCall();
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(g_position_attrib);
	glActiveTexture(GL_TEXTURE0);

	EndProfileStage();
}
