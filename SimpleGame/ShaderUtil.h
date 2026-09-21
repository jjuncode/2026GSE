#pragma once

#include "Dependencies\glew.h"

// 실패하면 0 을 준다. OpenGL 에서 0 은 유효하지 않은 객체로 예약돼 있어
// 부호 없는 정수에 -1 을 담는 것과 달리 성공 검사에 안 걸린다
GLuint CompileShaderProgram(const char* filename_vs, const char* filename_fs);
