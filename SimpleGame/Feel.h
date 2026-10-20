#pragma once

#include "WorldPos.h"

class Renderer;
class Creature;
struct HitInfo;

// 타격감 한곳 모음. 히트스톱 · 화면 흔들림 · 확대 펀치 · 섬광 · 파편 · 숫자 · 소리를
// 맞은 결과 하나에서 같이 낸다. 한 곳에 모아야 세기 균형을 한눈에 맞출 수 있다
void ClearFeel();
void UpdateFeel(float real_seconds);	// 히트스톱과 상관없이 실제 시간으로 돈다

// 1 이면 평소, 0 이면 멈춤. 게임 갱신에만 곱하고 연출은 실제 시간으로 돈다
float GetWorldTimeScale();
void AddHitStop(float seconds, float time_scale);
void AddSlowMotion(float seconds, float time_scale);	// 히트스톱이 끝난 뒤 이어지는 느린 구간

void AddShake(float trauma);	// 0 ~ 1. 제곱으로 세진다
void AddZoomPunch(float amount);
void AddScreenFlash(float r, float g, float b, float strength);

ScreenPos GetShakeOffset();	// 창 픽셀
float GetZoomPunch();	// 1 이 평소
void GetScreenFlash(float* r, float* g, float* b, float* strength);

void NotifyCombatHit(const HitInfo& hit, Creature* victim, int32 result, int32 taken);

// 파편. lift 는 발밑에서 띄운 높이 ( 창 픽셀 ), 방향은 화면 기준
void SpawnSparks(const WorldPos& pos, float lift, float dir_x, float dir_y, int32 count, float speed, float spread, float r, float g, float b, float life);
void SpawnStreak(const WorldPos& pos, float lift, float angle, float length, float r, float g, float b, float life);	// 베인 자리를 가로지르는 빛줄기
void SpawnRing(const WorldPos& pos, float radius, float r, float g, float b, float life);	// 바닥에 퍼지는 고리 ( 칸 )
void SpawnDust(const WorldPos& pos, int32 count, float r, float g, float b);
void SpawnGlint(const WorldPos& pos, float lift, float r, float g, float b, float size);	// 패링 타이밍 번쩍임

// 효과 시트 한 번 재생 ( VFX_ ). angle 은 화면 라디안, brightness 1 보다 크면 블룸을 탄다
void SpawnVfx(int32 vfx, const WorldPos& pos, float lift, float angle, float brightness);

// 판정 창이 막 열렸다. 땅을 내려치는 공격이면 그 자리에 충격파
void NotifyWindowOpen(const Creature& attacker, int32 window_index);

void DrawFeelGround(Renderer* renderer, const WorldPos& camera);	// 바닥 고리
void DrawFeelWorld(Renderer* renderer, const WorldPos& camera);	// 불꽃 · 빛줄기 · 번쩍임 ( 블룸을 탄다 )

// 플레이어에게서 먼 소리는 줄이다 끊는다. 맵 반대편에서 NPC 와 싸우는 몬스터 소리가 다 들리면 신호가 묻힌다
float GetSoundFalloff(const WorldPos& pos);
void PlaySfxAt(int32 sfx, const WorldPos& pos, float volume);

// 화면 픽셀 좌표 ( 창 기준, 가운데 원점 ) 로 월드를 옮긴 점. 그림 쪽 계산 공용
ScreenPos GetLiftedScreen(const WorldPos& pos, float lift, const WorldPos& camera);
