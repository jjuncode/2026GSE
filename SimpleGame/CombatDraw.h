#pragma once

#include "WorldPos.h"

class Renderer;
class Creature;
struct SkillData;

// 공격 연출 공용. 칼 궤적 초승달과 바닥 예고를 판정 부채꼴과 같은 월드 각도로 그린다.
// 그림이 판정보다 크거나 작으면 "맞았는데 안 맞았다" 가 생긴다
struct SwingState
{
	bool active = false;
	float angle_from = 0.f;	// 월드 각도 ( 라디안 )
	float angle_to = 0.f;
	float angle_now = 0.f;
	float progress = 0.f;	// 지금 창 안에서 0 ~ 1
	bool in_window = false;
	bool after_window = false;
	bool is_thrust = false;
	int32 window_index = -1;
};

float GetWorldAngle(float dir_x, float dir_y);
float WorldAngleToScreenAngle(float world_angle);

// 공격자 상태에서 지금 칼이 어디쯤인지
SwingState GetSwingState(const Creature& creature);

// 월드 바닥 위 고리 조각을 lift 만큼 띄워 그린다. 꼬리에서 머리로 갈수록 밝다
void DrawWorldArc(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift,
	float angle_from, float angle_to, float radius_in, float radius_out,
	float r, float g, float b, float head_alpha, float tail_alpha);

// 바닥 예고. fill 0 ~ 1 만큼 안에서 밖으로 차오른다
void DrawGroundSector(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float facing_angle, const SkillData& skill,
	float fill, float r, float g, float b, float alpha);

// 손에 쥔 칼. 화면 각도로 돌린다
void DrawBlade(Renderer* renderer, int32 sprite, float hand_x, float hand_y, float screen_angle, float brightness, float alpha, float flash);

// 칼 궤적 초승달. 바깥 날이 희게 빛나고 가운데가 두껍고 꼬리는 점으로 흩어진다.
// 도트 모드에서 그려야 한다 - 알파가 밝기 단 ( 흰 심지 · 몸통 · 어두운 가장자리 ) 이 된다
// descent 는 꼬리에서 머리로 내려가는 높이 ( 창 픽셀 ). 바닥과 평행하게만 그리면 2 대 1 로 눌려 납작해 보인다
// roundness 는 세로 늘림. 1 이면 바닥에 누운 호, 2 면 카메라를 보는 둥근 초승달
void DrawSlashCrescent(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift,
	float tail_angle, float head_angle, float radius, float thickness, float r, float g, float b, float strength, float descent, float roundness);

// 날 끝 반짝임 ( 휘두르는 동안 궤적 머리에 )
void DrawBladeGlint(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift, float angle, float radius, float strength);
void DrawGlintAt(Renderer* renderer, const ScreenPos& tip, float strength);

// 찌르기 빛창. 몸 앞에서 끝까지 가늘어지며 뻗는다
void DrawThrustLance(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift,
	float angle, float reach_from, float reach_to, float half_width, float r, float g, float b, float strength);

// 공격 중인 창의 궤적 ( 플레이어 · 몬스터 공용 ). radius_scale 은 사거리에 곱한다 - 판정 끝까지 그리면 칼보다 한참 커서 떠 보인다.
// 사방 공격 ( 회전 · 땅울림 ) 은 그리지 않는다. 회전은 그림 칼 각도를 아는 쪽이 따로 그린다
void DrawSwingTrail(Renderer* renderer, const WorldPos& camera, const Creature& creature, float lift, float radius_scale, float r, float g, float b);
