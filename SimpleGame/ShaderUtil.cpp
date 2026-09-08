#include "stdafx.h"
#include "ShaderUtil.h"

#include <string>
#include <fstream>
#include <iostream>
#include <cstring>

namespace
{
	bool ReadTextFile(const char* filename, std::string* target)
	{
		std::ifstream file(filename);
		if (file.fail())
		{
			std::cout << "[ ShaderUtil - ReadTextFile ] : " << filename << " loading failed\n";
			return false;
		}

		std::string line;
		while (getline(file, line))
		{
			target->append(line);
			target->append("\n");
		}

		return true;
	}

	void AttachShader(GLuint program, const char* shader_text, GLenum shader_type)
	{
		//쉐이더 오브젝트 생성
		GLuint shader_object = glCreateShader(shader_type);
		if (shader_object == 0)
		{
			std::cout << "[ ShaderUtil - AttachShader ] : cannot create shader object\n";
			return;
		}

		const GLchar* sources[1];
		sources[0] = shader_text;

		GLint lengths[1];
		lengths[0] = static_cast<GLint>(strlen(shader_text));

		//쉐이더 코드를 쉐이더 오브젝트에 할당하고 컴파일
		glShaderSource(shader_object, 1, sources, lengths);
		glCompileShader(shader_object);

		GLint success = 0;
		glGetShaderiv(shader_object, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			GLchar info_log[1024] = { 0 };
			glGetShaderInfoLog(shader_object, sizeof(info_log), NULL, info_log);

			std::cout << "[ ShaderUtil - AttachShader ] : compile failed\n" << info_log << "\n";

			glDeleteShader(shader_object);
			return;
		}

		glAttachShader(program, shader_object);

		// attach 뒤엔 프로그램이 참조를 쥐므로 링크까지 살아 있다
		glDeleteShader(shader_object);
	}
}

GLuint CompileShaderProgram(const char* filenameVS, const char* filenameFS)
{
	GLuint program = glCreateProgram();
	if (program == 0)
	{
		std::cout << "[ ShaderUtil - CompileShaderProgram ] : cannot create program\n";
		return 0;
	}

	std::string vertex_source;
	std::string fragment_source;

	if (!ReadTextFile(filenameVS, &vertex_source))
	{
		glDeleteProgram(program);
		return 0;
	}

	if (!ReadTextFile(filenameFS, &fragment_source))
	{
		glDeleteProgram(program);
		return 0;
	}

	AttachShader(program, vertex_source.c_str(), GL_VERTEX_SHADER);
	AttachShader(program, fragment_source.c_str(), GL_FRAGMENT_SHADER);

	glLinkProgram(program);

	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	if (success == 0)
	{
		GLchar error_log[1024] = { 0 };
		glGetProgramInfoLog(program, sizeof(error_log), NULL, error_log);

		std::cout << "[ ShaderUtil - CompileShaderProgram ] : link failed " << filenameVS << "\n" << error_log << "\n";

		glDeleteProgram(program);
		return 0;
	}

	std::cout << "[ ShaderUtil - CompileShaderProgram ] : " << filenameVS << " ok\n";

	return program;
}
