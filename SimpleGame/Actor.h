#pragma once

#include "SpriteId.h"
#include "WorldPos.h"

class Renderer;

// 화면에 놓이는 것 하나. 장식, 몬스터, 플레이어, 드랍이 전부 이 모양으로 그려진다
class Actor
{
public:
	virtual ~Actor();

	// 발밑 가운데 기준으로 세워 그린다
	virtual void Draw(Renderer* renderer, const WorldPos& camera) const;

	// 같은 그림을 검게 칠해 바닥에 눕힌다
	virtual void DrawShadow(Renderer* renderer, const WorldPos& camera) const;

	WorldPos m_pos;

	int32 m_sprite = SPRITE_NONE;
	int32 m_frame = 0;
	bool m_flip = false;	// 그림은 오른쪽을 본다. 켜면 왼쪽
	float m_shade = 1.f;	// 같은 그림을 조금씩 다른 밝기로
	float m_foot_offset_y = 0.f;	// 그림 발밑을 위치에서 이만큼 띄운다 ( 픽셀 )

	// 0 보다 크면 바닥을 밝히고 빛무리를 낸다
	float m_light_radius = 0.f;
	float m_light_r = 0.f;
	float m_light_g = 0.f;
	float m_light_b = 0.f;
	float m_light_strength = 0.f;

	// 1 보다 크면 스스로 빛나는 것으로 친다. 번짐은 후처리 블룸이 만든다
	float m_emissive = 1.f;

	// 바닥에 눕는 것은 그림자를 안 받는다
	bool m_is_upright = true;
	bool m_casts_shadow = true;
};
