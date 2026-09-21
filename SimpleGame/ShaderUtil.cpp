#include "stdafx.h"
#include "ShaderUtil.h"

#include <array>
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
		GLuint shader_object = glCreateShader(shader_type);
		if (shader_object == 0)
		{
			std::cout << "[ ShaderUtil - AttachShader ] : cannot create shader object\n";
			return;
		}

		const std::array<const GLchar*, 1> sources = { shader_text };
		const std::array<GLint, 1> lengths = { static_cast<GLint>(strlen(shader_text)) };

		glShaderSource(shader_object, 1, sources.data(), lengths.data());
		glCompileShader(shader_object);

		GLint success = 0;
		glGetShaderiv(shader_object, GL_COMPILE_STATUS, &success);

		if (success == 0)
		{
			std::array<GLchar, 1024> info_log = {};
			glGetShaderInfoLog(shader_object, static_cast<GLsizei>(info_log.size()), nullptr, info_log.data());

			std::cout << "[ ShaderUtil - AttachShader ] : compile failed\n" << info_log.data() << "\n";

			glDeleteShader(shader_object);
			return;
		}

		glAttachShader(program, shader_object);

		// attach 뒤엔 프로그램이 참조를 쥐므로 링크까지 살아 있다
		glDeleteShader(shader_object);
	}
}

GLuint CompileShaderProgram(const char* filename_vs, const char* filename_fs)
{
	GLuint program = glCreateProgram();
	if (program == 0)
	{
		std::cout << "[ ShaderUtil - CompileShaderProgram ] : cannot create program\n";
		return 0;
	}

	std::string vertex_source;
	std::string fragment_source;

	if (ReadTextFile(filename_vs, &vertex_source) == false)
	{
		glDeleteProgram(program);
		return 0;
	}

	if (ReadTextFile(filename_fs, &fragment_source) == false)
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
		std::array<GLchar, 1024> error_log = {};
		glGetProgramInfoLog(program, static_cast<GLsizei>(error_log.size()), nullptr, error_log.data());

		std::cout << "[ ShaderUtil - CompileShaderProgram ] : link failed " << filename_vs << "\n" << error_log.data() << "\n";

		glDeleteProgram(program);
		return 0;
	}

	std::cout << "[ ShaderUtil - CompileShaderProgram ] : " << filename_vs << " ok\n";

	return program;
}
