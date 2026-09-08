/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include <cmath>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "WorldPos.h"
#include "GameConfig.h"
#include "GameTime.h"
#include "Input.h"
#include "Scene.h"
#include "Hud.h"
#include "Ground.h"
#include "Fog.h"
#include "PostProcess.h"

const unsigned char KEY_ESCAPE = 27;

Renderer *g_Renderer = nullptr;

// 고정 간격으로 못 쪼갠 나머지. 다음 프레임으로 넘긴다
float g_accumulated_seconds = 0.f;

static void ApplyInput(float delta_seconds)
{
	float screen_right = 0.f;
	float screen_up = 0.f;

	if (IsKeyDown('w'))
	{
		screen_up += 1.f;
	}

	if (IsKeyDown('s'))
	{
		screen_up -= 1.f;
	}

	if (IsKeyDown('d'))
	{
		screen_right += 1.f;
	}

	if (IsKeyDown('a'))
	{
		screen_right -= 1.f;
	}

	MovePlayerByScreenAxis(screen_right, screen_up, delta_seconds);

	float world_x = 0.f;
	float world_y = 0.f;

	if (IsSpecialKeyDown(GLUT_KEY_RIGHT))
	{
		world_x += 1.f;
	}

	if (IsSpecialKeyDown(GLUT_KEY_LEFT))
	{
		world_x -= 1.f;
	}

	if (IsSpecialKeyDown(GLUT_KEY_UP))
	{
		world_y += 1.f;
	}

	if (IsSpecialKeyDown(GLUT_KEY_DOWN))
	{
		world_y -= 1.f;
	}

	MovePlayerByWorldAxis(world_x, world_y, delta_seconds);
}

void Update(void)
{
	g_accumulated_seconds += PopElapsedSeconds();

	// 남은 시간을 고정 간격으로 쪼개 그 횟수만큼 돌린다.
	// 잰 시간을 그대로 곱하면 한 번 튄 프레임에서 캐릭터가 중간 칸을 안 밟고 건너뛴다
	while (g_accumulated_seconds >= FIXED_STEP_SECONDS)
	{
		ApplyInput(FIXED_STEP_SECONDS);
		UpdateScene(FIXED_STEP_SECONDS);

		g_accumulated_seconds -= FIXED_STEP_SECONDS;
	}
}

void RenderScene(void)
{
	// 상태 변경이라 지우기 전에 정해야 이번 프레임부터 먹는다
	BeginScenePass();

	glClearColor(0.012f, 0.014f, 0.018f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	DrawScene(g_Renderer);

	EndScenePassAndComposite();

	// 후처리 뒤 저해상도 버퍼에 그린다. 흐림과 비네팅은 안 타면서 도트 격자는 같이 탄다
	DrawHud(g_Renderer);

	PresentUpscaled();

	glutSwapBuffers();
}

void Idle(void)
{
	Update();

	glutPostRedisplay();
}

void MouseInput(int button, int state, int x, int y)
{
	if (button != GLUT_LEFT_BUTTON)
	{
		return;
	}

	if (state != GLUT_DOWN)
	{
		return;
	}

	// glut 은 좌상단 원점, 렌더러는 화면 정중앙 원점에 위쪽이 양수
	ScreenPos screen;
	screen.m_x = static_cast<float>(x) - static_cast<float>(WINDOW_SIZE_X) * 0.5f;
	screen.m_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f - static_cast<float>(y);

	WorldPos picked = ScreenToWorld(screen, GetCameraPos());

	// 마름모가 칸 중심에 그려지므로 반올림해야 그 칸이 나온다
	picked.m_local_x = std::floor(picked.m_local_x + 0.5f);
	picked.m_local_y = std::floor(picked.m_local_y + 0.5f);
	NormalizeWorldPos(&picked);

	SetPickedTile(picked);
}

void KeyInput(unsigned char key, int x, int y)
{
	SetKeyDown(key, true);

	switch (key)
	{
		case 'g':
		case 'G':
		{
			ToggleChunkGrid();
			break;
		}
		case 'l':
		case 'L':
		{
			ToggleLighting();
			break;
		}
		case KEY_ESCAPE:
		{
			glutLeaveMainLoop();
			break;
		}
		default:
		{
			break;
		}
	}
}

void KeyUpInput(unsigned char key, int x, int y)
{
	SetKeyDown(key, false);
}

void SpecialKeyInput(int key, int x, int y)
{
	SetSpecialKeyDown(key, true);
}

void SpecialKeyUpInput(int key, int x, int y)
{
	SetSpecialKeyDown(key, false);
}

void WindowEntry(int state)
{
	if (state == GLUT_LEFT)
	{
		// 창 밖에서 뗀 키는 뗌 통보가 안 와서 눌린 채로 남는다
		ClearAllKeys();
	}
}

int main(int argc, char **argv)
{
	// Initialize GL things
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WINDOW_SIZE_X, WINDOW_SIZE_Y);
	glutCreateWindow("Game Software Engineering KPU");

	// 이걸 줘야 glutMainLoop 가 돌아와서 뒤쪽 정리 코드가 실행된다
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	// 자동 반복을 끄지 않으면 누른 채로 눌림 통보가 계속 와서 토글이 연타된다
	glutIgnoreKeyRepeat(1);

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n ";
	}

	// Initialize Renderer
	g_Renderer = new Renderer(WINDOW_SIZE_X, WINDOW_SIZE_Y);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.. \n";

		delete g_Renderer;
		g_Renderer = nullptr;

		return 1;
	}

	const bool post_ready = InitPostProcess(RENDER_SIZE_X, RENDER_SIZE_Y, WINDOW_SIZE_X, WINDOW_SIZE_Y);
	if (!post_ready)
	{
		// 몸체는 그대로 돌고 톤매핑과 블뢸만 빠진다
		std::cout << "[ SimpleGame - main ] : post process disabled";
	}

	// 후처리가 죽으면 씬이 창 해상도에 바로 그려진다. 그땐 배율을 되돌려야 좌표가 안 어긋난다
	float pixel_scale = static_cast<float>(PIXEL_SCALE);
	if (!post_ready)
	{
		pixel_scale = 1.f;
	}

	if (!InitGround(WINDOW_SIZE_X, WINDOW_SIZE_Y, pixel_scale))
	{
		// 바닥만 빠지고 물체와 HUD 는 그대로 나온다
		std::cout << "[ SimpleGame - main ] : ground layer disabled";
	}

	if (!InitFog(WINDOW_SIZE_X, WINDOW_SIZE_Y, pixel_scale))
	{
		// 안개만 빠진다. 분위기 요소라 없어도 진행에 지장 없다
		std::cout << "[ SimpleGame - main ] : fog layer disabled";
	}

	InitScene();
	ResetFrameClock();

	std::cout << "\nWASD  : screen axis move\n";
	std::cout << "Arrow : world axis move\n";
	std::cout << "Click : pick tile\n";
	std::cout << "G     : chunk edge overlay\n";
	std::cout << "L     : lighting on/off\n";
	std::cout << "ESC   : quit\n";

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUpInput);
	glutSpecialFunc(SpecialKeyInput);
	glutSpecialUpFunc(SpecialKeyUpInput);
	glutMouseFunc(MouseInput);
	glutEntryFunc(WindowEntry);

	glutMainLoop();

	ShutdownFog();
	ShutdownGround();
	ShutdownPostProcess();

	delete g_Renderer;
	g_Renderer = nullptr;

	return 0;
}
