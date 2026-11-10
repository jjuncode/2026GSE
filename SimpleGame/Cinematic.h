#pragma once

#include "WorldPos.h"

#include <string>
#include <vector>

class Renderer;

// 컷신. 세상 갱신을 멈추고 카메라 · 배우 · 자막을 시간표( CinematicData.json )대로 돌린다
void SetCinematicAnchor(const std::string& name, const WorldPos& pos);	// 구역을 만들 때 Scene 이 정한다
bool GetCinematicAnchor(const std::string& name, WorldPos* pos);
void StartCinematic(const std::string& id);
bool IsCinematicPlaying();
void SkipCinematic();	// ESC · Enter
std::string PopFinishedCinematic();	// 막 끝난 컷신. 없으면 빈 문자열

void UpdateCinematic(float real_seconds);
bool GetCinematicCamera(WorldPos* camera);
bool IsCinematicHidingPlayer();
bool IsCinematicHidingWorld();	// 어둠이 깔렸다. 배우만 그린다
void CollectCinematicActors();
void DrawCinematicGround(Renderer* renderer, const WorldPos& camera);	// 어둠 배경. 배우 밑에 깐다
void DrawCinematicOverlay(Renderer* renderer);	// 레터박스 · 자막 · 제목 · 암전

// 게임을 멈추지 않는 자막. 메인 퀘스트를 받고 넘길 때
void QueueNarration(const std::vector<std::wstring>& lines);
void DrawNarration(Renderer* renderer);
