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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
#include "Text.h"
#include "Profiler.h"
#include "Minimap.h"
#include "Npc.h"
#include "NpcHud.h"
#include "Audio.h"
#include "FloatingText.h"
#include "GameData.h"
#include "UiWindows.h"
#include "Monitor.h"

const uint8 KEY_ESCAPE = 27;
const uint8 KEY_ENTER = 13;
const uint8 KEY_TAB = 9;
const uint8 KEY_SPACE = 32;

const float QUIT_CONFIRM_SEC = 2.f;

Renderer* g_renderer = nullptr;

// 고정 간격으로 못 쪼갠 나머지. 다음 프레임으로 넘긴다
float g_accumulated_seconds = 0.f;

bool g_post_ready = false;

// 쉬프트는 glut 가 따로 안 알려 준다. 매 프레임 직접 묻고 눌린 순간만 쓴다
bool g_shift_was_down = false;
bool g_left_held = false;
float g_quit_confirm_timer = 0.f;

static bool IsWindowFocused()
{
	return GetForegroundWindow() == FindWindowA(nullptr, "Ashen Walls - Game Software Engineering KPU");
}

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

	const bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 && IsWindowFocused();
	if (shift_down && g_shift_was_down == false)
	{
		PressPlayerDodge(111);
	}

	g_shift_was_down = shift_down;

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
	PresentUpscaled();

	// 확대한 뒤 백버퍼에 바로 그린다. 글자는 월드보다 고운 두 배 격자라 저해상도 버퍼에 못 얹는다
	g_renderer->SetUndoToneMap(false);
	DrawSceneOverlay(g_renderer);
	DrawHud(g_renderer);

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
	const ScreenPos screen = GetMouseScreenPos();

	if (state != GLUT_DOWN)
	{
		UiMouseUp(button, screen.x, screen.y);

		// 뗄 때는 창 위여도 풀어야 버튼이 눌린 채로 안 남는다
		if (button == GLUT_LEFT_BUTTON)
		{
			g_left_held = false;
		}
		else if (button == GLUT_RIGHT_BUTTON)
		{
			SetPlayerGuard(false);
		}

		return;
	}

	if (UiMouseDown(button, screen.x, screen.y))
	{
		return;
	}

	switch (button)
	{
		case GLUT_LEFT_BUTTON:
		{
			PressPlayerAttack();
			g_left_held = true;
			break;
		}
		case GLUT_RIGHT_BUTTON:
		{
			SetPlayerGuard(true);
			break;
		}
		case GLUT_MIDDLE_BUTTON:
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

void MouseMotion(int32 x, int32 y)
{
	SetMouseWindowPos(x, y);

	const ScreenPos screen = GetMouseScreenPos();
	UiMouseMove(screen.x, screen.y);
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
				glutLeaveMainLoop();
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

void KeyUpInput(uint8 key, int32 x, int32 y)
{
	SetKeyDown(key, false);

	if (key == 'f' || key == 'F')
	{
		SetInteractHeld(false);
	}
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
		g_left_held = false;
		SetPlayerGuard(false);
		SetInteractHeld(false);
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

	if (InitSpriteSheets() == false)
	{
		std::cout << "[ SimpleGame - main ] : sprite sheets missing. run tools/art/bake.py and vfx.py\n";
	}

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
	std::cout << "V J M       : npc labels / journal / map\n";
	std::cout << "N G L P     : new zone / blocked overlay / lighting / profiler\n";
	std::cout << "B           : monitor page in browser\n";
	std::cout << "ESC         : close window, twice to quit\n";

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

	ShutdownMonitor();
	ShutdownAudio();
	ShutdownMinimap();
	ShutdownFog();
	ShutdownGround();
	ShutdownPostProcess();
	ShutdownText();
	ShutdownSpriteSheets();
	ShutdownSpriteAtlas();
	ShutdownProfiler();

	delete g_renderer;
	g_renderer = nullptr;

	return 0;
}
