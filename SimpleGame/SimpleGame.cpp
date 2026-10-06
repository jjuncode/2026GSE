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
#include "Player.h"
#include "SpriteAtlas.h"
#include "Text.h"
#include "Profiler.h"
#include "Minimap.h"
#include "Npc.h"
#include "NpcHud.h"

const uint8 KEY_ESCAPE = 27;
const uint8 KEY_ENTER = 13;
const uint8 KEY_TAB = 9;

Renderer* g_renderer = nullptr;

// 고정 간격으로 못 쪼갠 나머지. 다음 프레임으로 넘긴다
float g_accumulated_seconds = 0.f;

bool g_post_ready = false;

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

void Update()
{
	BeginProfileStage(PROFILE_STAGE_UPDATE);

	g_accumulated_seconds += PopElapsedSeconds();

	// 남은 시간을 고정 간격으로 쪼개 그 횟수만큼 돌린다.
	// 잰 시간을 그대로 곱하면 한 번 튄 프레임에서 캐릭터가 중간 칸을 안 밟고 건너뛴다
	while (g_accumulated_seconds >= FIXED_STEP_SECONDS)
	{
		ApplyInput(FIXED_STEP_SECONDS);
		UpdateScene(FIXED_STEP_SECONDS);
		UpdateHud(FIXED_STEP_SECONDS);

		g_accumulated_seconds -= FIXED_STEP_SECONDS;
	}

	EndProfileStage();
}


void RenderScene()
{
	// 상태 변경이라 지우기 전에 정해야 이번 프레임부터 먹는다
	BeginScenePass();

	glClearColor(0.012f, 0.014f, 0.018f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// 씬 버퍼는 뒤에서 톤매핑을 타므로 그림 색을 미리 역변환한다. 후처리가 죽었으면 안 탄다
	g_renderer->SetUndoToneMap(g_post_ready);
	DrawScene(g_renderer);

	EndScenePassAndComposite();

	// 후처리 뒤 저해상도 버퍼에 그린다. 흐림과 비네팅은 안 타면서 도트 격자는 같이 탄다
	g_renderer->SetUndoToneMap(false);
	DrawSceneOverlay(g_renderer);
	DrawHud(g_renderer);

	PresentUpscaled();

	// 프레임 경계. 남은 것이 있으면 다음 프레임 첫 층 뒤로 밀린다
	FlushRenderBatch();

	glutSwapBuffers();

	EndProfileFrame();

	// 새 글자를 구운 프레임에만 실제로 쓴다
	SaveTextCache();
}

void Idle()
{
	Update();

	glutPostRedisplay();
}

void MouseInput(int32 button, int32 state, int32 x, int32 y)
{
	SetMouseWindowPos(x, y);

	// 오른쪽은 관찰. 공격 키와 겹치지 않게 떼어 둔다
	if (button == GLUT_RIGHT_BUTTON)
	{
		if (state == GLUT_DOWN)
		{
			SelectNpcAtCursor();
		}

		return;
	}

	if (button != GLUT_LEFT_BUTTON)
	{
		return;
	}

	if (state != GLUT_DOWN)
	{
		SetAttackHeld(false);
		return;
	}

	const ScreenPos screen = GetMouseScreenPos();
	if (HandleHudClick(screen.x, screen.y))
	{
		return;
	}

	QueueAttack();
	SetAttackHeld(true);
}

void MouseMotion(int32 x, int32 y)
{
	SetMouseWindowPos(x, y);
}

void KeyInput(uint8 key, int32 x, int32 y)
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
		case 'c':
		case 'C':
		{
			ToggleStatWindow();
			break;
		}
		case 'u':
		case 'U':
		{
			ToggleEnhanceWindow();
			break;
		}
		case 'e':
		case 'E':
		{
			PickupMaterials();
			break;
		}
		case 'n':
		case 'N':
		{
			RegenerateZone();
			break;
		}
		case 'p':
		case 'P':
		{
			ToggleProfileDetail();
			break;
		}
		case 'f':
		case 'F':
		{
			ToggleFollowNpc();
			break;
		}
		case 'v':
		case 'V':
		{
			CycleNpcLabelMode();
			break;
		}
		case 'j':
		case 'J':
		{
			ToggleNpcJournal();
			break;
		}
		case 'm':
		case 'M':
		{
			ToggleMinimap();
			break;
		}
		case KEY_TAB:
		{
			SelectNextNpc();
			break;
		}
		case '1':
		{
			TryStatUp(STAT_MAX_HP);
			break;
		}
		case '2':
		{
			TryStatUp(STAT_ATTACK);
			break;
		}
		case '3':
		{
			TryStatUp(STAT_DEFENCE);
			break;
		}
		case KEY_ENTER:
		{
			RequestEnhance();
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

void KeyUpInput(uint8 key, int32 x, int32 y)
{
	SetKeyDown(key, false);
}

void SpecialKeyInput(int32 key, int32 x, int32 y)
{
	SetSpecialKeyDown(key, true);
}

void SpecialKeyUpInput(int32 key, int32 x, int32 y)
{
	SetSpecialKeyDown(key, false);
}

void WindowEntry(int32 state)
{
	if (state == GLUT_LEFT)
	{
		// 창 밖에서 뗀 키는 뗌 통보가 안 와서 눌린 채로 남는다
		ClearAllKeys();
	}
}

int main(int argc, char** argv)
{
	// 파일로 넘기면 버퍼에 갇혀 있다가 강제 종료 때 통째로 날아간다
	std::cout << std::unitbuf;

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(WINDOW_SIZE_X, WINDOW_SIZE_Y);
	glutCreateWindow("Ashen Walls - Game Software Engineering KPU");

	// 이걸 줘야 glutMainLoop 가 돌아와서 뒤쪽 정리 코드가 실행된다
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	// 자동 반복을 끄지 않으면 누른 채로 눌림 통보가 계속 와서 토글이 연타된다
	glutIgnoreKeyRepeat(1);

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << "[ SimpleGame - main ] : glew 3.0 supported\n";
	}
	else
	{
		std::cout << "[ SimpleGame - main ] : glew 3.0 not supported\n";
	}

	InitProfiler();
	LogGraphicsCaps();

	g_renderer = new Renderer(WINDOW_SIZE_X, WINDOW_SIZE_Y);
	if (g_renderer->IsInitialized() == false)
	{
		std::cout << "[ SimpleGame - main ] : renderer init failed\n";

		delete g_renderer;
		g_renderer = nullptr;

		return 1;
	}

	if (InitSpriteAtlas() == false)
	{
		// 그림만 빠지고 판정과 HUD 도형은 그대로 돈다
		std::cout << "[ SimpleGame - main ] : sprite atlas disabled\n";
	}

	if (InitText() == false)
	{
		std::cout << "[ SimpleGame - main ] : text disabled\n";
	}

	const bool post_ready = InitPostProcess(RENDER_SIZE_X, RENDER_SIZE_Y, WINDOW_SIZE_X, WINDOW_SIZE_Y);
	g_post_ready = post_ready;

	if (post_ready == false)
	{
		// 몸체는 그대로 돌고 톤매핑과 블룸만 빠진다
		std::cout << "[ SimpleGame - main ] : post process disabled\n";
	}

	// 후처리가 죽으면 씬이 창 해상도에 바로 그려진다. 그땐 배율을 되돌려야 좌표가 안 어긋난다
	float pixel_scale = static_cast<float>(PIXEL_SCALE);
	if (post_ready == false)
	{
		pixel_scale = 1.f;
	}

	if (InitGround(WINDOW_SIZE_X, WINDOW_SIZE_Y, pixel_scale) == false)
	{
		// 바닥만 빠지고 물체와 HUD 는 그대로 나온다
		std::cout << "[ SimpleGame - main ] : ground layer disabled\n";
	}

	if (InitFog(WINDOW_SIZE_X, WINDOW_SIZE_Y, pixel_scale) == false)
	{
		// 안개만 빠진다. 분위기 요소라 없어도 진행에 지장 없다
		std::cout << "[ SimpleGame - main ] : fog layer disabled\n";
	}

	InitScene();
	ResetFrameClock();

	std::cout << "\nWASD  : screen axis move\n";
	std::cout << "Arrow : world axis move\n";
	std::cout << "Mouse : aim / left click (hold) swing\n";
	std::cout << "E     : pick up materials\n";
	std::cout << "C     : stat window (1 2 3 : hp atk def)\n";
	std::cout << "U     : enhance window (Enter : try)\n";
	std::cout << "RMB   : inspect npc ( Tab next, F follow )\n";
	std::cout << "V J M : npc labels / journal / map\n";
	std::cout << "N     : new zone seed\n";
	std::cout << "G     : chunk edge / blocked tile overlay\n";
	std::cout << "L     : lighting on/off\n";
	std::cout << "P     : profiler detail log\n";
	std::cout << "ESC   : quit\n";

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUpInput);
	glutSpecialFunc(SpecialKeyInput);
	glutSpecialUpFunc(SpecialKeyUpInput);
	glutMouseFunc(MouseInput);
	glutMotionFunc(MouseMotion);
	glutPassiveMotionFunc(MouseMotion);
	glutEntryFunc(WindowEntry);

	glutMainLoop();

	ShutdownMinimap();
	ShutdownFog();
	ShutdownGround();
	ShutdownPostProcess();
	ShutdownText();
	ShutdownSpriteAtlas();
	ShutdownProfiler();

	delete g_renderer;
	g_renderer = nullptr;

	return 0;
}
