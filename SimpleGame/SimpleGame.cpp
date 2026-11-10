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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>

// windows.h 뒤. glfw3.h 가 APIENTRY 를 정의한 채로 두어서 거꾸로면 재정의 경고다
// gl 헤더는 glew 가 이미 넣었다
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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
#include "SpriteSheet.h"
#include "HouseArt.h"
#include "Text.h"
#include "Profiler.h"
#include "Minimap.h"
#include "Npc.h"
#include "NpcHud.h"
#include "Audio.h"
#include "FloatingText.h"
#include "GameData.h"
#include "UiWindows.h"
#include "Cinematic.h"
#include "Monitor.h"
#include "Quest.h"

const uint8 KEY_NONE = 0;
const uint8 KEY_ESCAPE = 27;
const uint8 KEY_ENTER = 13;
const uint8 KEY_TAB = 9;
const uint8 KEY_SPACE = 32;

const float QUIT_CONFIRM_SEC = 2.f;

Renderer* g_renderer = nullptr;

// 고정 간격으로 못 쪼갠 나머지. 다음 프레임으로 넘긴다
float g_accumulated_seconds = 0.f;

bool g_post_ready = false;

bool g_left_held = false;
float g_quit_confirm_timer = 0.f;

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

	SetPlayerMoveInput(screen_right, screen_up);

	// 좌클릭을 누르고 있으면 연타로 친다. 콤보를 손가락으로 두드리지 않아도 이어진다
	if (g_left_held)
	{
		PressPlayerAttack();
	}

	if (g_quit_confirm_timer > 0.f)
	{
		g_quit_confirm_timer -= delta_seconds;
	}
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
		UpdateMonitor(FIXED_STEP_SECONDS);

		g_accumulated_seconds -= FIXED_STEP_SECONDS;
	}

	EndProfileStage();
}


void RenderScene(GLFWwindow* window)
{
	// 상태 변경이라 지우기 전에 정해야 이번 프레임부터 먹는다
	BeginScenePass();

	glClearColor(0.012f, 0.014f, 0.018f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// 씬 버퍼는 뒤에서 톤매핑을 타므로 그림 색을 미리 역변환한다. 후처리가 죽었으면 안 탄다
	g_renderer->SetUndoToneMap(g_post_ready);
	DrawScene(g_renderer);

	EndScenePassAndComposite();
	PresentUpscaled();

	// 확대한 뒤 백버퍼에 바로 그린다. 글자는 월드보다 고운 두 배 격자라 저해상도 버퍼에 못 얹는다
	g_renderer->SetUndoToneMap(false);
	DrawSceneOverlay(g_renderer);
	DrawHud(g_renderer);

	// 프레임 경계. 남은 것이 있으면 다음 프레임 첫 층 뒤로 밀린다
	FlushRenderBatch();

	glfwSwapBuffers(window);

	EndProfileFrame();

	// 새 글자를 구운 프레임에만 실제로 쓴다
	SaveTextCache();
}

void MouseInput(GLFWwindow* window, int32 button, int32 action, int32 mods)
{
	double cursor_x = 0.0;
	double cursor_y = 0.0;
	glfwGetCursorPos(window, &cursor_x, &cursor_y);

	SetMouseWindowPos(static_cast<int32>(cursor_x), static_cast<int32>(cursor_y));
	const ScreenPos screen = GetMouseScreenPos();

	if (action != GLFW_PRESS)
	{
		UiMouseUp(button, screen.x, screen.y);

		// 뗄 때는 창 위여도 풀어야 버튼이 눌린 채로 안 남는다
		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			g_left_held = false;
		}
		else if (button == GLFW_MOUSE_BUTTON_RIGHT)
		{
			SetPlayerGuard(false);
		}

		return;
	}

	if (IsCinematicPlaying() || UiMouseDown(button, screen.x, screen.y))
	{
		return;
	}

	switch (button)
	{
		case GLFW_MOUSE_BUTTON_LEFT:
		{
			PressPlayerAttack();
			g_left_held = true;
			break;
		}
		case GLFW_MOUSE_BUTTON_RIGHT:
		{
			SetPlayerGuard(true);
			break;
		}
		case GLFW_MOUSE_BUTTON_MIDDLE:
		{
			// 관찰은 휠 버튼. 좌우 버튼은 전투가 쓴다
			SelectNpcAtCursor();
			break;
		}
		default:
		{
			break;
		}
	}
}

void MouseMotion(GLFWwindow* window, double x, double y)
{
	SetMouseWindowPos(static_cast<int32>(x), static_cast<int32>(y));

	const ScreenPos screen = GetMouseScreenPos();
	UiMouseMove(screen.x, screen.y);
}

void KeyInput(GLFWwindow* window, uint8 key)
{
	SetKeyDown(key, true);

	// 컷신 동안은 넘기기만 받는다
	if (IsCinematicPlaying())
	{
		if (key == KEY_ESCAPE || key == KEY_ENTER)
		{
			SkipCinematic();
		}

		return;
	}

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
		case 'i':
		case 'I':
		{
			ToggleInventoryWindow();
			break;
		}
		case 'k':
		case 'K':
		{
			ToggleSkillWindow();
			break;
		}
		case 'u':
		case 'U':
		{
			ToggleEnhanceWindow();
			break;
		}
		case 'h':
		case 'H':
		{
			ToggleKeyHelp();
			break;
		}
		case 'q':
		case 'Q':
		{
			PressPlayerSkill(106);
			break;
		}
		case 'e':
		case 'E':
		{
			PressPlayerSkill(107);
			break;
		}
		case 'r':
		case 'R':
		{
			PressPlayerSkill(108);
			break;
		}
		case KEY_SPACE:
		{
			PressPlayerDodge(110);
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
		case 'b':
		case 'B':
		{
			if (OpenMonitorPage())
			{
				AddDamageText(GetPlayer().m_pos, 80.f, L"모니터를 브라우저로 열었다", DAMAGE_STYLE_INFO);
			}
			else
			{
				AddDamageText(GetPlayer().m_pos, 80.f, L"모니터가 꺼져 있다", DAMAGE_STYLE_INFO);
			}

			break;
		}
		case 'f':
		case 'F':
		{
			Interact();
			SetInteractHeld(true);
			break;
		}
		case 'o':
		case 'O':
		{
			ToggleFollowNpc();
			break;
		}
		case 't':
		case 'T':
		{
			CycleTrackedQuest();
			break;
		}
		case 'y':
		case 'Y':
		{
			ToggleCardWindow();
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
			ToggleWorldMap();
			break;
		}
		case KEY_TAB:
		{
			SelectNextNpc();
			break;
		}
		case '1':
		{
			UsePlayerPotion();
			break;
		}
		case '-':
		case '_':
		{
			const float volume = ChangeMasterVolume(-GetConfig().audio_volume_step);
			AddDamageText(GetPlayer().m_pos, 80.f, L"소리 " + std::to_wstring(static_cast<int32>(volume * 100.f + 0.5f)) + L"%", DAMAGE_STYLE_INFO);
			break;
		}
		case '=':
		case '+':
		{
			const float volume = ChangeMasterVolume(GetConfig().audio_volume_step);
			PlaySfx(SFX_UI_CLICK, 0.8f);
			AddDamageText(GetPlayer().m_pos, 80.f, L"소리 " + std::to_wstring(static_cast<int32>(volume * 100.f + 0.5f)) + L"%", DAMAGE_STYLE_INFO);
			break;
		}
		case KEY_ENTER:
		{
			UiKeyEnter();
			break;
		}
		case KEY_ESCAPE:
		{
			// 창이 열려 있으면 닫기만. 아무것도 없을 때 두 번 눌러야 끈다
			if (CloseTopWindow())
			{
				break;
			}

			if (g_quit_confirm_timer > 0.f)
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
				break;
			}

			g_quit_confirm_timer = QUIT_CONFIRM_SEC;
			AddDamageText(GetPlayer().m_pos, 80.f, L"한 번 더 누르면 종료", DAMAGE_STYLE_INFO);
			break;
		}
		default:
		{
			break;
		}
	}
}

void KeyUpInput(uint8 key)
{
	SetKeyDown(key, false);

	if (key == 'f' || key == 'F')
	{
		SetInteractHeld(false);
	}
}

// 게임 키 처리는 freeglut 시절 문자 코드로 짜여 있다. 글자는 소문자로 준다
static uint8 ToGameKey(int32 key)
{
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
	{
		return static_cast<uint8>('a' + (key - GLFW_KEY_A));
	}

	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
	{
		return static_cast<uint8>('0' + (key - GLFW_KEY_0));
	}

	if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
	{
		return static_cast<uint8>('0' + (key - GLFW_KEY_KP_0));
	}

	switch (key)
	{
		case GLFW_KEY_SPACE:
		{
			return KEY_SPACE;
		}
		case GLFW_KEY_MINUS:
		case GLFW_KEY_KP_SUBTRACT:
		{
			return '-';
		}
		case GLFW_KEY_EQUAL:
		{
			return '=';
		}
		case GLFW_KEY_KP_ADD:
		{
			return '+';
		}
		case GLFW_KEY_ESCAPE:
		{
			return KEY_ESCAPE;
		}
		case GLFW_KEY_ENTER:
		case GLFW_KEY_KP_ENTER:
		{
			return KEY_ENTER;
		}
		case GLFW_KEY_TAB:
		{
			return KEY_TAB;
		}
		default:
		{
			return KEY_NONE;
		}
	}
}

void KeyboardInput(GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods)
{
	// 반복 통보를 받으면 누른 채로 토글이 연타된다
	if (action == GLFW_REPEAT)
	{
		return;
	}

	if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
	{
		if (action == GLFW_PRESS)
		{
			PressPlayerDodge(111);
		}

		return;
	}

	const uint8 game_key = ToGameKey(key);
	if (game_key == KEY_NONE)
	{
		return;
	}

	if (action == GLFW_PRESS)
	{
		KeyInput(window, game_key);
	}
	else
	{
		KeyUpInput(game_key);
	}
}

void GlfwError(int32 code, const char* description)
{
	std::cout << "[ SimpleGame - GlfwError ] : " << code << " " << description << "\n";
}

int main()
{
	// 파일로 넘기면 버퍼에 갇혀 있다가 강제 종료 때 통째로 날아간다
	std::cout << std::unitbuf;

	// freeglut 때처럼 DPI 비인식. glfwInit 이 인식으로 바꾸기 전에 먼저 정해 둔다
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);

	// freeglut 이 glutInit 에서 걸던 1ms 타이머. 모니터 스레드 select 대기가 이걸 탄다
	timeBeginPeriod(1);

	glfwSetErrorCallback(GlfwError);
	if (glfwInit() == GLFW_FALSE)
	{
		std::cout << "[ SimpleGame - main ] : glfw init failed\n";
		return 1;
	}

	// freeglut 기본과 같은 호환 컨텍스트. 깊이 버퍼는 안 쓴다(2.4)
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(WINDOW_SIZE_X, WINDOW_SIZE_Y, "Ashen Walls - Game Software Engineering KPU", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cout << "[ SimpleGame - main ] : glfw window failed\n";
		glfwTerminate();
		return 1;
	}

	// glfw 위치는 그리는 영역 기준이라 0, 0 이면 제목 표시줄이 화면 밖이다. 테두리만큼 민다
	int32 frame_left = 0;
	int32 frame_top = 0;
	glfwGetWindowFrameSize(window, &frame_left, &frame_top, nullptr, nullptr);
	glfwSetWindowPos(window, frame_left, frame_top);
	glfwShowWindow(window);

	// 수직동기는 freeglut 때처럼 드라이버 기본값. glfwSwapInterval 을 부르면 바뀐다
	glfwMakeContextCurrent(window);

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

	if (LoadGameData() == false)
	{
		std::cout << "[ SimpleGame - main ] : game data missing - check ./Data\n";
		return 1;
	}

	if (InitAudio() == false)
	{
		// 소리만 빠지고 게임은 돈다
		std::cout << "[ SimpleGame - main ] : audio disabled\n";
	}

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

	// 그림판을 읽은 뒤라야 새 아이템 아이콘을 이름으로 찾는다
	BindItemArt();

	if (InitSpriteSheets() == false)
	{
		std::cout << "[ SimpleGame - main ] : sprite sheets missing. run tools/art/bake.py and vfx.py\n";
	}

	// 없으면 집을 옛 칸 블록으로 세운다
	InitHouseArt();

	if (InitText() == false)
	{
		std::cout << "[ SimpleGame - main ] : text disabled\n";
	}

	if (InitMonitor() == false)
	{
		// 브라우저 모니터만 빠진다
		std::cout << "[ SimpleGame - main ] : monitor disabled\n";
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

	std::cout << "\nWASD        : move\n";
	std::cout << "LMB         : attack combo ( hold to chain ), counter after parry\n";
	std::cout << "RMB         : parry ( tap ) / guard ( hold )\n";
	std::cout << "Q E R       : dash slash / spin slash / leap slam ( skill tree )\n";
	std::cout << "Space Shift : dash / roll\n";
	std::cout << "1           : hp potion\n";
	std::cout << "F           : pick up / anvil\n";
	std::cout << "I C K U H   : bag / stats / skill tree / enhance / help\n";
	std::cout << "MMB Tab O   : inspect npc / next / follow\n";
	std::cout << "V J M       : npc labels / journal / world map\n";
	std::cout << "N G L P     : new zone / blocked overlay / lighting / profiler\n";
	std::cout << "B           : monitor page in browser\n";
	std::cout << "T           : next quest guide ( board quests )\n";
	std::cout << "Y           : card collection\n";
	std::cout << "ESC         : close window, twice to quit\n";

	// 포커스를 잃으면 glfw 가 눌린 키 · 버튼마다 뗌을 보내 준다. 창 밖에서 떼도 눌린 채로 안 남는다
	glfwSetKeyCallback(window, KeyboardInput);
	glfwSetMouseButtonCallback(window, MouseInput);
	glfwSetCursorPosCallback(window, MouseMotion);

	while (glfwWindowShouldClose(window) == GLFW_FALSE)
	{
		glfwPollEvents();
		Update();

		// 최소화 중엔 freeglut 처럼 그리기를 건너뛰고 갱신만 돈다
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_FALSE)
		{
			RenderScene(window);
		}
	}

	ShutdownMonitor();
	ShutdownAudio();
	ShutdownMinimap();
	ShutdownFog();
	ShutdownGround();
	ShutdownPostProcess();
	ShutdownText();
	ShutdownSpriteSheets();
	ShutdownHouseArt();
	ShutdownSpriteAtlas();
	ShutdownProfiler();

	delete g_renderer;
	g_renderer = nullptr;

	glfwDestroyWindow(window);
	glfwTerminate();
	timeEndPeriod(1);

	return 0;
}
