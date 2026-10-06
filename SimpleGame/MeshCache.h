#pragma once

#include <functional>
#include <vector>

#include "Dependencies\glew.h"

// 인스턴싱의 밑바탕이 되는 모양 하나. 정점은 x y z 셋씩 들어 있다
struct MeshData
{
	std::vector<float> vertices;
	int32 vertex_count = 0;
};

// 캐시 파일에 있으면 읽고, 없으면 build 로 만들어 저장한다.
// 어느 쪽이든 GPU 버퍼 하나를 돌려주고, 그 뒤로는 이 버퍼를 계속 다시 쓴다
GLuint GetOrCreateMeshBuffer(const char* name, uint64 signature, const std::function<void(MeshData*)>& build, int32* vertex_count);
