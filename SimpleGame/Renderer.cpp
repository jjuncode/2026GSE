#include "stdafx.h"
#include "Renderer.h"
#include "AssetCache.h"
#include "MeshCache.h"
#include "WorldPos.h"
#include "ShaderUtil.h"
#include "Profiler.h"

#include <array>
#include <cmath>
#include <vector>

namespace
{
	const int32 CIRCLE_SEGMENT_COUNT = 48;
	const float PI = 3.14159265f;

	// 한 번에 내보내는 최대 인스턴스 수. 이 칸이 차면 중간에 한 번 끊어 낸다
	constexpr int32 MAX_BATCH_INSTANCE_COUNT = 4096;

	constexpr int32 BATCH_KIND_NONE = 0;
	constexpr int32 BATCH_KIND_SOLID = 1;
	constexpr int32 BATCH_KIND_AFFINE = 2;
	constexpr int32 BATCH_KIND_TRIANGLE = 3;

	// trans 4 + color 4
	constexpr int32 SOLID_INSTANCE_FLOAT_COUNT = 8;
	// origin 2 + axis_x 2 + axis_y 2 + region 4 + color 4 + flash 1
	constexpr int32 AFFINE_INSTANCE_FLOAT_COUNT = 15;

	// 위치 2 + 색 4. 인스턴스 칸을 그대로 빌려 쓴다
	constexpr int32 TRIANGLE_VERTEX_FLOAT_COUNT = 6;
	constexpr int32 MAX_TRIANGLE_VERTEX_COUNT = MAX_BATCH_INSTANCE_COUNT * AFFINE_INSTANCE_FLOAT_COUNT / TRIANGLE_VERTEX_FLOAT_COUNT;

	// 다른 층이 비워 달라고 부를 수 있게 지금 살아 있는 것을 들고 있는다
	Renderer* g_active_renderer = nullptr;

	void PushVertex(std::vector<float>* target, float x, float y)
	{
		target->push_back(x);
		target->push_back(y);
		target->push_back(0.f);
	}

	// glVertexAttribPointer 는 버퍼 안 옮김값을 포인터 자리로 받는다
	const void* BufferOffset(size_t byte_offset)
	{
		return reinterpret_cast<const void*>(byte_offset);
	}
}

void FlushRenderBatch()
{
	if (g_active_renderer == nullptr)
	{
		return;
	}

	g_active_renderer->FlushBatch();
}

Renderer::Renderer(int32 window_size_x, int32 window_size_y)
{
	Initialize(window_size_x, window_size_y);

	g_active_renderer = this;
}


Renderer::~Renderer()
{
	if (g_active_renderer == this)
	{
		g_active_renderer = nullptr;
	}

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

	if (m_vbo_instance != 0)
	{
		glDeleteBuffers(1, &m_vbo_instance);
		m_vbo_instance = 0;
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

	// 인스턴싱은 3.3 에 들어왔다. 셰이더가 이미 330 을 요구하니 여기서 걸리면 그림 자체가 안 뜬다
	if (glewIsSupported("GL_VERSION_3_3") == false)
	{
		std::cout << "[ Renderer - Initialize ] : instancing not supported\n";
		return;
	}

	CreateVertexBufferObjects();
	CreateInstanceBuffer();

	if (m_solid_rect_shader > 0 && m_affine_shader > 0 && m_vbo_rect > 0 && m_vbo_diamond > 0 && m_vbo_circle > 0 && m_vbo_unit_quad > 0 && m_vbo_instance > 0)
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
	// 창 크기와 타일 크기가 정점에 박혀 있어서, 그게 바뀌면 캐시가 어긋난다.
	// 지문에 같이 섞어 두면 값이 달라질 때 저절로 다시 만든다
	uint64 screen_signature = HashValue(static_cast<uint64>(m_window_size_x), 1469598103934665603ull);
	screen_signature = HashValue(static_cast<uint64>(m_window_size_y), screen_signature);

	const int32 window_size_x = m_window_size_x;
	const int32 window_size_y = m_window_size_y;

	int32 vertex_count = 0;

	m_vbo_unit_quad = GetOrCreateMeshBuffer("mesh_unit_quad", HashValue(1u, screen_signature),
		[](MeshData* mesh)
		{
			// 0 ~ 1 사각형. 실제 모양은 셰이더가 원점과 두 축으로 만든다
			mesh->vertices =
			{
				0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 0.f,
			};
			mesh->vertex_count = 6;
		}, &vertex_count);

	m_vbo_rect = GetOrCreateMeshBuffer("mesh_rect", HashValue(2u, screen_signature),
		[window_size_x, window_size_y](MeshData* mesh)
		{
			const float window_width = static_cast<float>(window_size_x);
			const float window_height = static_cast<float>(window_size_y);

			mesh->vertices =
			{
				-1.f / window_width, -1.f / window_height, 0.f, -1.f / window_width, 1.f / window_height, 0.f, 1.f / window_width, 1.f / window_height, 0.f,
				-1.f / window_width, -1.f / window_height, 0.f, 1.f / window_width, 1.f / window_height, 0.f, 1.f / window_width, -1.f / window_height, 0.f,
			};
			mesh->vertex_count = 6;
		}, &vertex_count);

	m_vbo_diamond = GetOrCreateMeshBuffer("mesh_diamond", HashValue(3u, screen_signature),
		[window_size_x, window_size_y](MeshData* mesh)
		{
			// 타일 크기를 정점에 미리 박아 두면 스케일 1 이 타일 한 칸이 된다
			const float half_width = TILE_PIXEL_WIDTH * 0.5f * 2.f / static_cast<float>(window_size_x);
			const float half_height = TILE_PIXEL_HEIGHT * 0.5f * 2.f / static_cast<float>(window_size_y);

			mesh->vertices =
			{
				0.f, half_height, 0.f, half_width, 0.f, 0.f, 0.f, -half_height, 0.f,
				0.f, half_height, 0.f, 0.f, -half_height, 0.f, -half_width, 0.f, 0.f,
			};
			mesh->vertex_count = 6;
		}, &vertex_count);

	m_vbo_circle = GetOrCreateMeshBuffer("mesh_circle", HashValue(static_cast<uint64>(CIRCLE_SEGMENT_COUNT) + 4u, screen_signature),
		[window_size_x, window_size_y](MeshData* mesh)
		{
			// 가로세로에 각각 다른 픽셀 단위를 쓰므로 창 비율이 달라도 원이 안 찌그러진다
			const float unit_x = 2.f / static_cast<float>(window_size_x);
			const float unit_y = 2.f / static_cast<float>(window_size_y);

			mesh->vertices.reserve(static_cast<size_t>(CIRCLE_SEGMENT_COUNT * 3 * 3));

			for (int32 segment = 0; segment < CIRCLE_SEGMENT_COUNT; ++segment)
			{
				const float angle_now = 2.f * PI * static_cast<float>(segment) / static_cast<float>(CIRCLE_SEGMENT_COUNT);
				const float angle_next = 2.f * PI * static_cast<float>(segment + 1) / static_cast<float>(CIRCLE_SEGMENT_COUNT);

				PushVertex(&mesh->vertices, 0.f, 0.f);
				PushVertex(&mesh->vertices, std::cos(angle_now) * unit_x, std::sin(angle_now) * unit_y);
				PushVertex(&mesh->vertices, std::cos(angle_next) * unit_x, std::sin(angle_next) * unit_y);
			}

			mesh->vertex_count = CIRCLE_SEGMENT_COUNT * 3;
		}, &m_circle_vertex_count);
}

void Renderer::CreateInstanceBuffer()
{
	// 매 프레임 다시 채울 자리다. 가장 큰 칸 기준으로 한 번만 잡아 두고 그 뒤로는 덮어쓴다
	const size_t byte_count = static_cast<size_t>(MAX_BATCH_INSTANCE_COUNT) * static_cast<size_t>(AFFINE_INSTANCE_FLOAT_COUNT) * sizeof(float);

	m_instances.reserve(static_cast<size_t>(MAX_BATCH_INSTANCE_COUNT) * static_cast<size_t>(AFFINE_INSTANCE_FLOAT_COUNT));

	glGenBuffers(1, &m_vbo_instance);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(byte_count), nullptr, GL_STREAM_DRAW);
}

void Renderer::CacheShaderLocations()
{
	m_solid_pixel_mode = glGetUniformLocation(m_solid_rect_shader, "u_PixelMode");

	m_affine_texture = glGetUniformLocation(m_affine_shader, "u_Texture");
	m_affine_undo = glGetUniformLocation(m_affine_shader, "u_UndoToneMap");

	m_affine_position = glGetAttribLocation(m_affine_shader, "a_Position");
	m_affine_origin_attrib = glGetAttribLocation(m_affine_shader, "a_Origin");
	m_affine_axis_x_attrib = glGetAttribLocation(m_affine_shader, "a_AxisX");
	m_affine_axis_y_attrib = glGetAttribLocation(m_affine_shader, "a_AxisY");
	m_affine_region_attrib = glGetAttribLocation(m_affine_shader, "a_Region");
	m_affine_color_attrib = glGetAttribLocation(m_affine_shader, "a_Color");
	m_affine_flash_attrib = glGetAttribLocation(m_affine_shader, "a_Flash");

	m_position_attrib = glGetAttribLocation(m_solid_rect_shader, "a_Position");
	m_solid_trans_attrib = glGetAttribLocation(m_solid_rect_shader, "a_Trans");
	m_solid_color_attrib = glGetAttribLocation(m_solid_rect_shader, "a_Color");
}

void Renderer::DrawSolidBox(float x, float y, float width, float height, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	PushSolidInstance(m_vbo_rect, 6, new_x, new_y, width, height, r, g, b, a);
}

void Renderer::DrawSolidDiamond(float x, float y, float scale, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	PushSolidInstance(m_vbo_diamond, 6, new_x, new_y, scale, scale, r, g, b, a);
}

void Renderer::DrawSolidCircle(float x, float y, float radius, float r, float g, float b, float a)
{
	float new_x, new_y;

	GetGLPosition(x, y, &new_x, &new_y);

	PushSolidInstance(m_vbo_circle, m_circle_vertex_count, new_x, new_y, radius, radius, r, g, b, a);
}

void Renderer::PushSolidInstance(GLuint mesh_vbo, int32 mesh_vertex_count, float new_x, float new_y, float scale_x, float scale_y, float r, float g, float b, float a)
{
	if (m_position_attrib < 0 || m_solid_trans_attrib < 0 || m_solid_color_attrib < 0)
	{
		return;
	}

	// 밑바탕 모양이 다르면 한 드로우콜에 못 담는다. 도형 종류가 바뀌는 자리가 곧 끊기는 자리다
	if (m_batch_kind != BATCH_KIND_SOLID || m_batch_mesh_vbo != mesh_vbo)
	{
		FlushCurrentBatch(FLUSH_REASON_STATE);

		m_batch_kind = BATCH_KIND_SOLID;
		m_batch_mesh_vbo = mesh_vbo;
		m_batch_mesh_vertex_count = mesh_vertex_count;
	}

	m_instances.push_back(new_x);
	m_instances.push_back(new_y);
	m_instances.push_back(scale_x);
	m_instances.push_back(scale_y);
	m_instances.push_back(r);
	m_instances.push_back(g);
	m_instances.push_back(b);
	m_instances.push_back(a);

	m_batch_instance_count += 1;
	CountQuadSubmit();

	if (m_batch_instance_count >= MAX_BATCH_INSTANCE_COUNT)
	{
		FlushCurrentBatch(FLUSH_REASON_FULL);
	}
}

void Renderer::DrawAffine(GLuint texture, const TexRegion& region, const AffineQuad& quad, float r, float g, float b, float a)
{
	DrawAffineFlash(texture, region, quad, r, g, b, a, 0.f);
}

void Renderer::DrawAffineFlash(GLuint texture, const TexRegion& region, const AffineQuad& quad, float r, float g, float b, float a, float flash)
{
	if (m_affine_position < 0 || m_affine_origin_attrib < 0)
	{
		return;
	}

	const float to_ndc_x = 2.f / static_cast<float>(m_window_size_x);
	const float to_ndc_y = 2.f / static_cast<float>(m_window_size_y);

	// 텍스처와 톤매핑 되돌리기는 유니폼이라 인스턴스마다 못 바꾼다. 바뀌면 거기서 끊는다
	if (m_batch_kind != BATCH_KIND_AFFINE || m_batch_texture != texture || m_batch_undo_tone_map != m_undo_tone_map)
	{
		FlushCurrentBatch(FLUSH_REASON_STATE);

		m_batch_kind = BATCH_KIND_AFFINE;
		m_batch_texture = texture;
		m_batch_undo_tone_map = m_undo_tone_map;
	}

	m_instances.push_back(quad.origin_x * to_ndc_x);
	m_instances.push_back(quad.origin_y * to_ndc_y);
	m_instances.push_back(quad.axis_x_x * to_ndc_x);
	m_instances.push_back(quad.axis_x_y * to_ndc_y);
	m_instances.push_back(quad.axis_y_x * to_ndc_x);
	m_instances.push_back(quad.axis_y_y * to_ndc_y);
	m_instances.push_back(region.u0);
	m_instances.push_back(region.v0);
	m_instances.push_back(region.u1);
	m_instances.push_back(region.v1);
	m_instances.push_back(r);
	m_instances.push_back(g);
	m_instances.push_back(b);
	m_instances.push_back(a);
	m_instances.push_back(flash);

	m_batch_instance_count += 1;
	CountQuadSubmit();

	if (m_batch_instance_count >= MAX_BATCH_INSTANCE_COUNT)
	{
		FlushCurrentBatch(FLUSH_REASON_FULL);
	}
}

void Renderer::PushColorVertex(const ColorVertex& vertex)
{
	float new_x, new_y;
	GetGLPosition(vertex.x, vertex.y, &new_x, &new_y);

	m_instances.push_back(new_x);
	m_instances.push_back(new_y);
	m_instances.push_back(vertex.r);
	m_instances.push_back(vertex.g);
	m_instances.push_back(vertex.b);
	m_instances.push_back(vertex.a);
}

void Renderer::DrawTriangle(const ColorVertex& first, const ColorVertex& second, const ColorVertex& third)
{
	if (m_position_attrib < 0 || m_solid_trans_attrib < 0 || m_solid_color_attrib < 0)
	{
		return;
	}

	if (m_batch_kind != BATCH_KIND_TRIANGLE || m_batch_pixel_triangles != m_pixel_triangles)
	{
		FlushCurrentBatch(FLUSH_REASON_STATE);
		m_batch_kind = BATCH_KIND_TRIANGLE;
		m_batch_pixel_triangles = m_pixel_triangles;
	}

	// 셋 단위로 끊어야 다음 묶음이 남은 꼭짓점을 이어받지 않는다
	if (m_batch_instance_count + 3 > MAX_TRIANGLE_VERTEX_COUNT)
	{
		FlushCurrentBatch(FLUSH_REASON_FULL);
		m_batch_kind = BATCH_KIND_TRIANGLE;
		m_batch_pixel_triangles = m_pixel_triangles;
	}

	PushColorVertex(first);
	PushColorVertex(second);
	PushColorVertex(third);

	m_batch_instance_count += 3;
	CountQuadSubmit();
}

void Renderer::DrawQuad(const ColorVertex& first, const ColorVertex& second, const ColorVertex& third, const ColorVertex& fourth)
{
	DrawTriangle(first, second, third);
	DrawTriangle(first, third, fourth);
}

void Renderer::FlushBatch()
{
	FlushCurrentBatch(FLUSH_REASON_LAYER);
}

void Renderer::FlushCurrentBatch(int32 reason)
{
	if (m_batch_instance_count <= 0)
	{
		m_batch_kind = BATCH_KIND_NONE;
		m_instances.clear();

		return;
	}

	if (m_batch_kind == BATCH_KIND_SOLID)
	{
		FlushSolidBatch();
	}
	else if (m_batch_kind == BATCH_KIND_AFFINE)
	{
		FlushAffineBatch();
	}
	else if (m_batch_kind == BATCH_KIND_TRIANGLE)
	{
		FlushTriangleBatch();
	}

	CountBatchFlush(reason, m_batch_instance_count);

	m_instances.clear();
	m_batch_instance_count = 0;
	m_batch_kind = BATCH_KIND_NONE;
	m_batch_mesh_vbo = 0;
	m_batch_texture = 0;
}

void Renderer::UploadInstanceBuffer()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_instance);

	// 같은 크기로 다시 잡아 주면 드라이버가 새 자리를 내줘서, 앞 프레임 그리기가 끝나기를 안 기다린다
	const size_t capacity = static_cast<size_t>(MAX_BATCH_INSTANCE_COUNT) * static_cast<size_t>(AFFINE_INSTANCE_FLOAT_COUNT) * sizeof(float);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m_instances.size() * sizeof(float)), m_instances.data());
}

void Renderer::BindInstanceAttrib(GLint location, int32 component_count, size_t float_offset, int32 stride_float_count)
{
	if (location < 0)
	{
		return;
	}

	const GLsizei stride = static_cast<GLsizei>(sizeof(float) * static_cast<size_t>(stride_float_count));

	glEnableVertexAttribArray(static_cast<GLuint>(location));
	glVertexAttribPointer(static_cast<GLuint>(location), component_count, GL_FLOAT, GL_FALSE, stride, BufferOffset(sizeof(float) * float_offset));

	// 1 이면 정점마다가 아니라 인스턴스마다 한 번 읽는다. 이게 인스턴싱의 전부다
	glVertexAttribDivisor(static_cast<GLuint>(location), 1);
}

void Renderer::ReleaseInstanceAttrib(GLint location)
{
	if (location < 0)
	{
		return;
	}

	// 나눔값을 0 으로 안 되돌리면 이 속성 칸을 쓰는 다음 층이 인스턴스처럼 읽어 화면이 비거나 깨진다
	glVertexAttribDivisor(static_cast<GLuint>(location), 0);
	glDisableVertexAttribArray(static_cast<GLuint>(location));
}

void Renderer::FlushSolidBatch()
{
	glUseProgram(m_solid_rect_shader);

	UploadInstanceBuffer();
	BindInstanceAttrib(m_solid_trans_attrib, 4, 0, SOLID_INSTANCE_FLOAT_COUNT);
	BindInstanceAttrib(m_solid_color_attrib, 4, 4, SOLID_INSTANCE_FLOAT_COUNT);

	glEnableVertexAttribArray(static_cast<GLuint>(m_position_attrib));
	glBindBuffer(GL_ARRAY_BUFFER, m_batch_mesh_vbo);
	glVertexAttribPointer(static_cast<GLuint>(m_position_attrib), 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, BufferOffset(0));

	CountDrawCall();
	glDrawArraysInstanced(GL_TRIANGLES, 0, m_batch_mesh_vertex_count, m_batch_instance_count);

	glDisableVertexAttribArray(static_cast<GLuint>(m_position_attrib));
	ReleaseInstanceAttrib(m_solid_trans_attrib);
	ReleaseInstanceAttrib(m_solid_color_attrib);
}

void Renderer::FlushAffineBatch()
{
	glUseProgram(m_affine_shader);

	int32 undo = 0;
	if (m_batch_undo_tone_map)
	{
		undo = 1;
	}
	glUniform1i(m_affine_undo, undo);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_batch_texture);
	glUniform1i(m_affine_texture, 0);

	UploadInstanceBuffer();
	BindInstanceAttrib(m_affine_origin_attrib, 2, 0, AFFINE_INSTANCE_FLOAT_COUNT);
	BindInstanceAttrib(m_affine_axis_x_attrib, 2, 2, AFFINE_INSTANCE_FLOAT_COUNT);
	BindInstanceAttrib(m_affine_axis_y_attrib, 2, 4, AFFINE_INSTANCE_FLOAT_COUNT);
	BindInstanceAttrib(m_affine_region_attrib, 4, 6, AFFINE_INSTANCE_FLOAT_COUNT);
	BindInstanceAttrib(m_affine_color_attrib, 4, 10, AFFINE_INSTANCE_FLOAT_COUNT);
	BindInstanceAttrib(m_affine_flash_attrib, 1, 14, AFFINE_INSTANCE_FLOAT_COUNT);

	glEnableVertexAttribArray(static_cast<GLuint>(m_affine_position));
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_unit_quad);
	glVertexAttribPointer(static_cast<GLuint>(m_affine_position), 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, BufferOffset(0));

	CountDrawCall();
	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, m_batch_instance_count);

	glDisableVertexAttribArray(static_cast<GLuint>(m_affine_position));
	ReleaseInstanceAttrib(m_affine_origin_attrib);
	ReleaseInstanceAttrib(m_affine_axis_x_attrib);
	ReleaseInstanceAttrib(m_affine_axis_y_attrib);
	ReleaseInstanceAttrib(m_affine_region_attrib);
	ReleaseInstanceAttrib(m_affine_color_attrib);
	ReleaseInstanceAttrib(m_affine_flash_attrib);
}

void Renderer::FlushTriangleBatch()
{
	glUseProgram(m_solid_rect_shader);

	UploadInstanceBuffer();

	const GLsizei stride = static_cast<GLsizei>(sizeof(float) * static_cast<size_t>(TRIANGLE_VERTEX_FLOAT_COUNT));

	// 위치와 색은 꼭짓점마다, 옮김과 배율은 고정값 하나 ( 도형 셰이더를 그대로 빌린다 )
	glEnableVertexAttribArray(static_cast<GLuint>(m_position_attrib));
	glVertexAttribPointer(static_cast<GLuint>(m_position_attrib), 2, GL_FLOAT, GL_FALSE, stride, BufferOffset(0));

	glEnableVertexAttribArray(static_cast<GLuint>(m_solid_color_attrib));
	glVertexAttribPointer(static_cast<GLuint>(m_solid_color_attrib), 4, GL_FLOAT, GL_FALSE, stride, BufferOffset(sizeof(float) * 2));

	glDisableVertexAttribArray(static_cast<GLuint>(m_solid_trans_attrib));
	glVertexAttrib4f(static_cast<GLuint>(m_solid_trans_attrib), 0.f, 0.f, 1.f, 1.f);

	if (m_batch_pixel_triangles)
	{
		glUniform1i(m_solid_pixel_mode, 1);
	}

	CountDrawCall();
	glDrawArrays(GL_TRIANGLES, 0, m_batch_instance_count);

	// 상자 · 원도 같은 셰이더라 바로 되돌린다
	if (m_batch_pixel_triangles)
	{
		glUniform1i(m_solid_pixel_mode, 0);
	}

	glDisableVertexAttribArray(static_cast<GLuint>(m_position_attrib));
	glDisableVertexAttribArray(static_cast<GLuint>(m_solid_color_attrib));
}

void Renderer::GetGLPosition(float x, float y, float* new_x, float* new_y)
{
	*new_x = x * 2.f / static_cast<float>(m_window_size_x);
	*new_y = y * 2.f / static_cast<float>(m_window_size_y);
}

void Renderer::SetPixelTriangles(bool on)
{
	m_pixel_triangles = on;
}

void Renderer::SetUndoToneMap(bool undo)
{
	if (m_undo_tone_map == undo)
	{
		return;
	}

	// 쌓아 둔 것은 바뀌기 전 설정으로 그려야 한다
	FlushCurrentBatch(FLUSH_REASON_STATE);

	m_undo_tone_map = undo;
}
