#include "stdafx.h"
#include "MeshCache.h"
#include "AssetCache.h"
#include "Profiler.h"

#include <chrono>
#include <iostream>
#include <string>

namespace
{
	// 정점 셋씩 들어오지 않으면 스트라이드가 안 맞아 화면이 통째로 어긋난다
	bool IsValidMesh(const MeshData& mesh)
	{
		if (mesh.vertex_count <= 0)
		{
			return false;
		}

		return mesh.vertices.size() == static_cast<size_t>(mesh.vertex_count) * 3;
	}

	bool DecodeMesh(const std::vector<uint8>& blob, MeshData* mesh)
	{
		BlobReader reader;
		reader.bytes = &blob;

		const int32 vertex_count = reader.ReadInt();

		if (reader.failed || vertex_count <= 0 || vertex_count > 1000000)
		{
			return false;
		}

		mesh->vertex_count = vertex_count;
		mesh->vertices.assign(static_cast<size_t>(vertex_count) * 3, 0.f);
		reader.ReadRaw(mesh->vertices.data(), mesh->vertices.size() * sizeof(float));

		if (reader.failed)
		{
			return false;
		}

		return IsValidMesh(*mesh);
	}

	std::vector<uint8> EncodeMesh(const MeshData& mesh)
	{
		BlobWriter writer;
		writer.WriteInt(mesh.vertex_count);
		writer.WriteRaw(mesh.vertices.data(), mesh.vertices.size() * sizeof(float));

		return writer.bytes;
	}

	GLuint UploadMesh(const MeshData& mesh)
	{
		GLuint buffer = 0;

		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)), mesh.vertices.data(), GL_STATIC_DRAW);

		return buffer;
	}
}

GLuint GetOrCreateMeshBuffer(const char* name, uint64 signature, const std::function<void(MeshData*)>& build, int32* vertex_count)
{
	const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	MeshData mesh;
	std::vector<uint8> blob;
	bool from_cache = false;

	if (LoadCachedBlob(name, signature, &blob))
	{
		from_cache = DecodeMesh(blob, &mesh);
	}

	if (from_cache == false)
	{
		mesh = MeshData();
		build(&mesh);

		if (IsValidMesh(mesh) == false)
		{
			std::cout << "[ MeshCache - GetOrCreateMeshBuffer ] : bad mesh " << name << "\n";

			*vertex_count = 0;
			return 0;
		}

		SaveCachedBlob(name, signature, EncodeMesh(mesh));
	}

	const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - begin;
	LogAssetLoad(name, from_cache, mesh.vertices.size() * sizeof(float), elapsed.count() * 1000.f);

	*vertex_count = mesh.vertex_count;

	return UploadMesh(mesh);
}
