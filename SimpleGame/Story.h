#pragma once

class Renderer;

// 이야기 흐름. 시작 컷신 -> 최강자로 그림자를 베는 튜토리얼 -> 최강자의 죽음 -> 수백 년 뒤 메인 퀘스트
void InitStory();	// 첫 구역을 세운 뒤 한 번
void SkipStoryOpening();	// 시험 장치. 컷신 · 튜토리얼을 건너뛰고 메인 퀘스트부터
void UpdateStory(float real_seconds);
void OnStoryStepReported(int32 step);	// 메인 퀘스트 한 단계를 넘겼다
void DrawStoryOverlay(Renderer* renderer);	// 튜토리얼 목표와 조작 안내
bool IsStoryTutorial();
