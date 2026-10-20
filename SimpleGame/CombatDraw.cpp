#include "stdafx.h"
#include "CombatDraw.h"
#include "Combat.h"
#include "GameConfig.h"
#include "Renderer.h"
#include "SpriteAtlas.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	const float PI = 3.14159265f;
	const float DEG = PI / 180.f;

	constexpr int32 ARC_SEGMENT_COUNT = 18;
	constexpr int32 CRESCENT_SEGMENT_COUNT = 28;
	constexpr int32 LANCE_SEGMENT_COUNT = 12;
	const float FULL_CIRCLE_SWEEP = 300.f * PI / 180.f;	// 이보다 넓으면 사방 공격
	const float SLASH_DESCENT_PER_RADIAN = 7.f;	// 도트 칸
	const float SLASH_ROUNDNESS = 1.7f;
	const float MIN_TAIL_ANGLE = 50.f * PI / 180.f;

	ColorVertex MakeVertex(const ScreenPos& screen, float r, float g, float b, float a)
	{
		ColorVertex vertex;
		vertex.x = screen.x;
		vertex.y = screen.y;
		vertex.r = r;
		vertex.g = g;
		vertex.b = b;
		vertex.a = a;

		return vertex;
	}

	ScreenPos ProjectOnGround(const WorldPos& center, float angle, float radius, float lift, const WorldPos& camera)
	{
		WorldPos point = center;
		MoveWorldPos(&point, std::cos(angle) * radius, std::sin(angle) * radius);

		ScreenPos screen = WorldToScreen(point, camera);
		screen.y += lift;

		return screen;
	}

	ScreenPos ProjectOffset(const WorldPos& center, float offset_x, float offset_y, float lift, const WorldPos& camera)
	{
		WorldPos point = center;
		MoveWorldPos(&point, offset_x, offset_y);

		ScreenPos screen = WorldToScreen(point, camera);
		screen.y += lift;

		return screen;
	}

	// 바닥 위 점을 발 기준으로 세로만 늘려 세운다. 아이소에선 바닥 원이 2 대 1 로 눌려 정면 쪽 궤적이 가는 줄로 보였다
	ScreenPos Stand(const ScreenPos& ground, const ScreenPos& hub, float lift, float roundness)
	{
		ScreenPos screen = ground;
		screen.y = hub.y + (ground.y - hub.y) * roundness + lift;
		return screen;
	}

	// 초승달 두께. 꼬리에서 가늘게 시작해 머리 쪽 3 분의 2 에서 가장 두껍고 머리 끝은 뾰족하다
	float CrescentShape(float t)
	{
		return std::pow(std::sin(PI * std::pow(t, 1.35f)), 0.65f);
	}

	float EaseOut(float t)
	{
		return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
	}

	float EaseInOut(float t)
	{
		if (t < 0.5f)
		{
			return 4.f * t * t * t;
		}

		const float back = -2.f * t + 2.f;
		return 1.f - back * back * back * 0.5f;
	}
}

float GetWorldAngle(float dir_x, float dir_y)
{
	return std::atan2(dir_y, dir_x);
}

float WorldAngleToScreenAngle(float world_angle)
{
	// 아이소 변환: 월드 x 는 화면 오른쪽 아래, y 는 왼쪽 아래 ( 화면 가로 = x - y, 세로 = -(x + y) / 2 )
	const float world_x = std::cos(world_angle);
	const float world_y = std::sin(world_angle);
	const float screen_x = (world_x - world_y) * TILE_PIXEL_WIDTH * 0.5f;
	const float screen_y = -(world_x + world_y) * TILE_PIXEL_HEIGHT * 0.5f;

	return std::atan2(screen_y, screen_x);
}

SwingState GetSwingState(const Creature& creature)
{
	SwingState state;

	const SkillData* skill = creature.m_cast_skill;
	if (skill == nullptr || skill->hit_windows.empty())
	{
		return state;
	}

	if (creature.IsAttacking() == false)
	{
		return state;
	}

	state.active = true;

	const float facing = GetWorldAngle(creature.m_face_x, creature.m_face_y);

	// 월드에서 반시계( + )가 화면 오른쪽이다. 판정의 angle_right 쪽
	const float left_edge = facing - skill->angle_left * DEG;
	const float right_edge = facing + skill->angle_right * DEG;

	int32 window_index = -1;
	const float progress = GetWindowProgress(creature, &window_index);
	state.window_index = window_index;
	state.progress = progress;

	switch (skill->attack_dir)
	{
		case ATTACK_DIR_LEFT_TO_RIGHT:
		{
			state.angle_from = left_edge;
			state.angle_to = right_edge;
			break;
		}
		case ATTACK_DIR_RIGHT_TO_LEFT:
		{
			state.angle_from = right_edge;
			state.angle_to = left_edge;
			break;
		}
		case ATTACK_DIR_FRONT:
		{
			state.angle_from = left_edge;
			state.angle_to = right_edge;
			state.is_thrust = true;
			break;
		}
		default:
		{
			// 사방이 아닌 연타는 창마다 좌우를 번갈아 벤다 ( 그림이 그렇게 휘두른다 )
			const float sweep = (skill->angle_left + skill->angle_right) * DEG;
			if (sweep < FULL_CIRCLE_SWEEP && window_index % 2 == 1)
			{
				state.angle_from = right_edge;
				state.angle_to = left_edge;
			}
			else if (sweep < FULL_CIRCLE_SWEEP)
			{
				state.angle_from = left_edge;
				state.angle_to = right_edge;
			}
			else
			{
				state.angle_from = facing - PI;
				state.angle_to = facing + PI;
			}

			break;
		}
	}

	const HitWindow& window = skill->hit_windows[static_cast<size_t>(std::max(window_index, 0))];
	const float time = creature.m_action_time;

	if (time < window.start_sec)
	{
		// 휘두르기 전 - 시작 쪽으로 천천히 당겨 둔다 ( 예비 동작 )
		const float windup = std::min(time / std::max(window.start_sec, 0.01f), 1.f);
		state.angle_now = facing + (state.angle_from - facing) * EaseInOut(windup);

		if (state.is_thrust)
		{
			state.angle_now = facing;
		}
	}
	else if (time <= window.end_sec)
	{
		state.in_window = true;
		state.angle_now = state.angle_from + (state.angle_to - state.angle_from) * EaseOut(progress);

		if (state.is_thrust)
		{
			state.angle_now = facing;
		}
	}
	else
	{
		state.after_window = true;
		state.angle_now = state.angle_to;

		if (state.is_thrust)
		{
			state.angle_now = facing;
		}
	}

	return state;
}

void DrawWorldArc(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift,
	float angle_from, float angle_to, float radius_in, float radius_out,
	float r, float g, float b, float head_alpha, float tail_alpha)
{
	for (int32 segment = 0; segment < ARC_SEGMENT_COUNT; ++segment)
	{
		const float t0 = static_cast<float>(segment) / static_cast<float>(ARC_SEGMENT_COUNT);
		const float t1 = static_cast<float>(segment + 1) / static_cast<float>(ARC_SEGMENT_COUNT);

		const float angle0 = angle_from + (angle_to - angle_from) * t0;
		const float angle1 = angle_from + (angle_to - angle_from) * t1;

		// 머리 쪽일수록 두껍고 밝다. 꼬리는 가늘게 사라진다
		const float alpha0 = tail_alpha + (head_alpha - tail_alpha) * t0;
		const float alpha1 = tail_alpha + (head_alpha - tail_alpha) * t1;
		const float inner0 = radius_out - (radius_out - radius_in) * (0.35f + 0.65f * t0);
		const float inner1 = radius_out - (radius_out - radius_in) * (0.35f + 0.65f * t1);

		const ScreenPos outer_a = ProjectOnGround(center, angle0, radius_out, lift, camera);
		const ScreenPos outer_b = ProjectOnGround(center, angle1, radius_out, lift, camera);
		const ScreenPos inner_a = ProjectOnGround(center, angle0, inner0, lift, camera);
		const ScreenPos inner_b = ProjectOnGround(center, angle1, inner1, lift, camera);

		renderer->DrawQuad(
			MakeVertex(inner_a, r, g, b, alpha0 * 0.2f),
			MakeVertex(outer_a, r, g, b, alpha0),
			MakeVertex(outer_b, r, g, b, alpha1),
			MakeVertex(inner_b, r, g, b, alpha1 * 0.2f));
	}
}

void DrawGroundSector(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float facing_angle, const SkillData& skill,
	float fill, float r, float g, float b, float alpha)
{
	const float left_edge = facing_angle - skill.angle_left * DEG;
	const float right_edge = facing_angle + skill.angle_right * DEG;
	const float radius = skill.range;
	const float fill_radius = radius * std::min(std::max(fill, 0.f), 1.f);

	const ScreenPos hub = WorldToScreen(center, camera);

	for (int32 segment = 0; segment < ARC_SEGMENT_COUNT; ++segment)
	{
		const float t0 = static_cast<float>(segment) / static_cast<float>(ARC_SEGMENT_COUNT);
		const float t1 = static_cast<float>(segment + 1) / static_cast<float>(ARC_SEGMENT_COUNT);
		const float angle0 = left_edge + (right_edge - left_edge) * t0;
		const float angle1 = left_edge + (right_edge - left_edge) * t1;

		// 바닥 판 전체는 흐리게
		const ScreenPos edge_a = ProjectOnGround(center, angle0, radius, 0.f, camera);
		const ScreenPos edge_b = ProjectOnGround(center, angle1, radius, 0.f, camera);
		renderer->DrawTriangle(MakeVertex(hub, r, g, b, alpha * 0.25f), MakeVertex(edge_a, r, g, b, alpha * 0.45f), MakeVertex(edge_b, r, g, b, alpha * 0.45f));

		// 차오르는 앞면. 다 차면 그때 맞는다
		if (fill_radius > 0.01f)
		{
			const ScreenPos fill_a = ProjectOnGround(center, angle0, fill_radius, 0.f, camera);
			const ScreenPos fill_b = ProjectOnGround(center, angle1, fill_radius, 0.f, camera);
			renderer->DrawTriangle(MakeVertex(hub, r, g, b, alpha * 0.3f), MakeVertex(fill_a, r, g, b, alpha * 0.85f), MakeVertex(fill_b, r, g, b, alpha * 0.85f));
		}

		// 테두리
		const ScreenPos rim_a = ProjectOnGround(center, angle0, radius * 0.94f, 0.f, camera);
		const ScreenPos rim_b = ProjectOnGround(center, angle1, radius * 0.94f, 0.f, camera);
		renderer->DrawQuad(MakeVertex(rim_a, r, g, b, alpha * 0.2f), MakeVertex(edge_a, r, g, b, alpha), MakeVertex(edge_b, r, g, b, alpha), MakeVertex(rim_b, r, g, b, alpha * 0.2f));
	}
}

void DrawBlade(Renderer* renderer, int32 sprite, float hand_x, float hand_y, float screen_angle, float brightness, float alpha, float flash)
{
	const float unit = static_cast<float>(PIXEL_SCALE);
	const float blade_x = std::cos(screen_angle);
	const float blade_y = std::sin(screen_angle);
	const float length = static_cast<float>(GetSpriteHeight(sprite)) * unit;
	const float width = static_cast<float>(GetSpriteWidth(sprite)) * unit;
	const float grip = 2.f * unit;	// 손잡이 끝에서 쥐는 자리까지

	// 세로축이 칼날 방향, 가로축은 그 수직
	AffineQuad quad;
	quad.axis_y_x = blade_x * length;
	quad.axis_y_y = blade_y * length;
	quad.axis_x_x = blade_y * width;
	quad.axis_x_y = -blade_x * width;
	quad.origin_x = hand_x - quad.axis_x_x * 0.5f - blade_x * grip;
	quad.origin_y = hand_y - quad.axis_x_y * 0.5f - blade_y * grip;

	DrawSpriteQuadFlash(renderer, sprite, 0, quad, false, brightness, brightness, brightness, alpha, flash);
}

void DrawSlashCrescent(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift,
	float tail_angle, float head_angle, float radius, float thickness, float r, float g, float b, float strength, float descent, float roundness)
{
	const ScreenPos hub = WorldToScreen(center, camera);

	for (int32 segment = 0; segment < CRESCENT_SEGMENT_COUNT; ++segment)
	{
		const float t0 = static_cast<float>(segment) / static_cast<float>(CRESCENT_SEGMENT_COUNT);
		const float t1 = static_cast<float>(segment + 1) / static_cast<float>(CRESCENT_SEGMENT_COUNT);

		const float angle0 = tail_angle + (head_angle - tail_angle) * t0;
		const float angle1 = tail_angle + (head_angle - tail_angle) * t1;
		const float width0 = thickness * CrescentShape(t0);
		const float width1 = thickness * CrescentShape(t1);

		// 앞쪽 절반은 꽉 차고 꼬리 쪽만 식는다. 전부 흩으면 망점처럼 흐려 보였다
		const float bright0 = strength * std::min(0.4f + 1.1f * t0, 1.f);
		const float bright1 = strength * std::min(0.4f + 1.1f * t1, 1.f);

		// 바깥 날 - 심지 - 몸통 - 안쪽. 도트 모드에서 0.75 위는 흰 날, 0.4 ~ 0.75 는 색, 그 아래는 어두운 점이 된다
		const std::array<float, 4> depth = { 0.f, 0.18f, 0.5f, 1.f };
		const std::array<float, 4> alpha = { 1.f, 0.95f, 0.72f, 0.f };

		const float lift0 = lift + descent * (0.5f - t0);
		const float lift1 = lift + descent * (0.5f - t1);

		for (size_t row = 0; row + 1 < depth.size(); ++row)
		{
			const ScreenPos a0 = Stand(ProjectOnGround(center, angle0, radius - width0 * depth[row], 0.f, camera), hub, lift0, roundness);
			const ScreenPos a1 = Stand(ProjectOnGround(center, angle1, radius - width1 * depth[row], 0.f, camera), hub, lift1, roundness);
			const ScreenPos b1 = Stand(ProjectOnGround(center, angle1, radius - width1 * depth[row + 1], 0.f, camera), hub, lift1, roundness);
			const ScreenPos b0 = Stand(ProjectOnGround(center, angle0, radius - width0 * depth[row + 1], 0.f, camera), hub, lift0, roundness);

			renderer->DrawQuad(
				MakeVertex(a0, r, g, b, bright0 * alpha[row]),
				MakeVertex(a1, r, g, b, bright1 * alpha[row]),
				MakeVertex(b1, r, g, b, bright1 * alpha[row + 1]),
				MakeVertex(b0, r, g, b, bright0 * alpha[row + 1]));
		}
	}
}

void DrawBladeGlint(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift, float angle, float radius, float strength)
{
	DrawGlintAt(renderer, ProjectOnGround(center, angle, radius, lift, camera), strength);
}

void DrawGlintAt(Renderer* renderer, const ScreenPos& tip, float strength)
{
	const float unit = static_cast<float>(PIXEL_SCALE);
	const float arm = unit * (2.5f + 3.5f * strength);
	const float thin = unit * 0.55f;

	// 날 끝 십자 반짝임. 가로가 길고 세로가 짧다
	const std::array<std::array<float, 2>, 2> axes = { { { arm, thin }, { thin, arm * 0.7f } } };
	for (const std::array<float, 2>& axis : axes)
	{
		ColorVertex left;
		left.x = tip.x - axis[0];
		left.y = tip.y;
		ColorVertex up;
		up.x = tip.x;
		up.y = tip.y + axis[1];
		ColorVertex right;
		right.x = tip.x + axis[0];
		right.y = tip.y;
		ColorVertex down;
		down.x = tip.x;
		down.y = tip.y - axis[1];

		for (ColorVertex* vertex : { &left, &up, &right, &down })
		{
			vertex->r = 3.f;
			vertex->g = 3.f;
			vertex->b = 3.f;
			vertex->a = strength;
		}

		renderer->DrawQuad(left, up, right, down);
	}
}

void DrawThrustLance(Renderer* renderer, const WorldPos& camera, const WorldPos& center, float lift,
	float angle, float reach_from, float reach_to, float half_width, float r, float g, float b, float strength)
{
	const float dir_x = std::cos(angle);
	const float dir_y = std::sin(angle);
	const float side_x = -dir_y;
	const float side_y = dir_x;

	// 가운데 줄이 가장 밝고 양옆으로 식는다
	const std::array<float, 5> across = { -1.f, -0.45f, 0.f, 0.45f, 1.f };
	const std::array<float, 5> alpha = { 0.f, 0.6f, 0.98f, 0.6f, 0.f };

	for (int32 segment = 0; segment < LANCE_SEGMENT_COUNT; ++segment)
	{
		const float t0 = static_cast<float>(segment) / static_cast<float>(LANCE_SEGMENT_COUNT);
		const float t1 = static_cast<float>(segment + 1) / static_cast<float>(LANCE_SEGMENT_COUNT);
		const float reach0 = reach_from + (reach_to - reach_from) * t0;
		const float reach1 = reach_from + (reach_to - reach_from) * t1;

		// 뿌리와 끝이 뾰족하고 앞쪽 3 분의 1 에서 가장 굵다
		const float width0 = half_width * std::sin(PI * std::pow(t0, 0.6f));
		const float width1 = half_width * std::sin(PI * std::pow(t1, 0.6f));
		const float bright0 = strength * (0.45f + 0.55f * t0);
		const float bright1 = strength * (0.45f + 0.55f * t1);

		for (size_t row = 0; row + 1 < across.size(); ++row)
		{
			const ScreenPos a0 = ProjectOffset(center, dir_x * reach0 + side_x * width0 * across[row], dir_y * reach0 + side_y * width0 * across[row], lift, camera);
			const ScreenPos a1 = ProjectOffset(center, dir_x * reach1 + side_x * width1 * across[row], dir_y * reach1 + side_y * width1 * across[row], lift, camera);
			const ScreenPos b1 = ProjectOffset(center, dir_x * reach1 + side_x * width1 * across[row + 1], dir_y * reach1 + side_y * width1 * across[row + 1], lift, camera);
			const ScreenPos b0 = ProjectOffset(center, dir_x * reach0 + side_x * width0 * across[row + 1], dir_y * reach0 + side_y * width0 * across[row + 1], lift, camera);

			renderer->DrawQuad(
				MakeVertex(a0, r, g, b, bright0 * alpha[row]),
				MakeVertex(a1, r, g, b, bright1 * alpha[row]),
				MakeVertex(b1, r, g, b, bright1 * alpha[row + 1]),
				MakeVertex(b0, r, g, b, bright0 * alpha[row + 1]));
		}
	}
}

void DrawSwingTrail(Renderer* renderer, const WorldPos& camera, const Creature& creature, float lift, float radius_scale, float r, float g, float b)
{
	const SwingState swing = GetSwingState(creature);
	if (swing.active == false || creature.m_cast_skill == nullptr)
	{
		return;
	}

	if (swing.in_window == false && swing.after_window == false)
	{
		return;
	}

	const SkillData& skill = *creature.m_cast_skill;
	const float sweep = (skill.angle_left + skill.angle_right) * DEG;
	if (skill.attack_dir == ATTACK_DIR_NONE && sweep >= FULL_CIRCLE_SWEEP)
	{
		return;
	}

	const float radius = skill.range * radius_scale;

	// 창이 끝난 뒤엔 남은 궤적이 짧게 식는다
	float fade = 1.f;
	if (swing.after_window)
	{
		const HitWindow& window = skill.hit_windows[static_cast<size_t>(std::max(swing.window_index, 0))];
		fade = 1.f - std::min((creature.m_action_time - window.end_sec) / 0.16f, 1.f);
		if (fade <= 0.f)
		{
			return;
		}
	}

	if (swing.is_thrust)
	{
		const float facing = GetWorldAngle(creature.m_face_x, creature.m_face_y);
		const float reach = radius * (0.55f + 0.45f * std::min(swing.progress, 1.f));
		DrawThrustLance(renderer, camera, creature.m_pos, lift, facing, radius * 0.12f, reach, radius * 0.16f, r, g, b, fade);
		return;
	}

	// 꼬리는 휘두른 각의 85 퍼센트, 처음부터 50 도는 끌린다. 짧으면 점처럼 보인다
	const float swept = swing.angle_now - swing.angle_from;
	float direction = 1.f;
	if (swing.angle_to < swing.angle_from)
	{
		direction = -1.f;
	}

	const float tail = swing.angle_now - direction * std::max(std::fabs(swept) * 0.85f, MIN_TAIL_ANGLE);

	// 휘두른 각만큼 비스듬히 내려 벤다 ( 그림도 높은 데서 낮은 데로 휘두른다 )
	const float descent = sweep * SLASH_DESCENT_PER_RADIAN * static_cast<float>(PIXEL_SCALE);
	DrawSlashCrescent(renderer, camera, creature.m_pos, lift, tail, swing.angle_now, radius, radius * 0.62f, r, g, b, fade, descent, SLASH_ROUNDNESS);

	if (swing.in_window)
	{
		const ScreenPos hub = WorldToScreen(creature.m_pos, camera);
		const ScreenPos tip = Stand(ProjectOnGround(creature.m_pos, swing.angle_now, radius, 0.f, camera), hub, lift - descent * 0.5f, SLASH_ROUNDNESS);
		DrawGlintAt(renderer, tip, 1.f - swing.progress * 0.6f);
	}
}
