#include "stdafx.h"
#include "Cinematic.h"
#include "Actor.h"
#include "Audio.h"
#include "Feel.h"
#include "GameConfig.h"
#include "GameData.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Text.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>

namespace
{
	const float LETTERBOX_HEIGHT = 74.f;
	const float LETTERBOX_SPEED = 2.5f;	// 초당 비율
	const float DARK_SPEED = 1.5f;
	const float TYPE_SPEED = 26.f;	// 초당 글자
	const float NARRATION_LINE_SEC = 3.4f;

	constexpr int32 STYLE_NORMAL = 0;
	constexpr int32 STYLE_SHADOW = 1;	// 어둡게 물든 몸 + 보랏빛 테 ( 그림자 괴물과 같다 )
	constexpr int32 STYLE_SILHOUETTE = 2;	// 단색. 등불에 비친 그림자, 어둠 속 거대한 형체

	const std::array<std::array<float, 2>, 4> RIM_OFFSETS =
	{
		std::array<float, 2>{ -1.f, 0.f },
		std::array<float, 2>{ 1.f, 0.f },
		std::array<float, 2>{ 0.f, 1.f },
		std::array<float, 2>{ 0.f, -1.f },
	};

	float Lerp(float from, float to, float ratio)
	{
		return from + (to - from) * std::min(std::max(ratio, 0.f), 1.f);
	}

	int32 ParseSheet(const std::string& name)
	{
		const std::map<std::string, int32> sheets =
		{
			{ "warden", SHEET_NPC_WARDEN }, { "raider", SHEET_NPC_RAIDER }, { "merchant", SHEET_NPC_MERCHANT },
			{ "hunter", SHEET_NPC_HUNTER }, { "gatherer", SHEET_NPC_GATHERER }, { "blighted", SHEET_BLIGHTED },
			{ "hound", SHEET_HOUND }, { "devourer", SHEET_DEVOURER }, { "gatebreaker", SHEET_GATEBREAKER },
		};

		const auto found = sheets.find(name);
		if (found == sheets.end())
		{
			return SHEET_NONE;
		}

		return found->second;
	}

	int32 ParseStyle(const std::string& name)
	{
		if (name == "shadow")
		{
			return STYLE_SHADOW;
		}

		if (name == "silhouette")
		{
			return STYLE_SILHOUETTE;
		}

		return STYLE_NORMAL;
	}

	int32 ParseSound(const std::string& name)
	{
		const std::map<std::string, int32> sounds =
		{
			{ "danger", SFX_DANGER }, { "parry", SFX_PARRY }, { "hurt", SFX_PLAYER_HURT }, { "guard_break", SFX_GUARD_BREAK },
			{ "level_up", SFX_LEVEL_UP }, { "swing", SFX_SWING_HEAVY }, { "die", SFX_MONSTER_DIE }, { "glint", SFX_GLINT },
			{ "hit", SFX_HIT_HEAVY }, { "unlock", SFX_SKILL_UNLOCK },
		};

		const auto found = sounds.find(name);
		if (found == sounds.end())
		{
			return -1;
		}

		return found->second;
	}

	class CineActor : public Actor
	{
	public:
		void Draw(Renderer* renderer, const WorldPos& camera) const override
		{
			const ScreenPos foot = WorldToScreen(m_pos, camera);
			const float unit = static_cast<float>(PIXEL_SCALE);

			if (m_lantern)
			{
				// 손에 든 등불. 1 을 넘겨 블룸으로 번진다
				const float hand_y = foot.y + GetSheetBodyHeight(m_sheet) * unit * m_art_scale * 0.42f;
				float hand_x = foot.x + 6.f * m_art_scale;
				if (m_sheet == SHEET_NONE)
				{
					hand_x = foot.x;
				}

				renderer->DrawSolidCircle(hand_x, hand_y, 4.f, 2.6f * m_alpha, 1.8f * m_alpha, 0.8f * m_alpha, 1.f);
			}

			if (m_sheet == SHEET_NONE || m_alpha <= 0.01f)
			{
				return;
			}

			if (m_style == STYLE_SILHOUETTE)
			{
				DrawSheetFrameScaled(renderer, m_sheet, m_frame, foot.x, foot.y, m_art_scale, m_r, m_g, m_b, m_alpha, 2.f);
				return;
			}

			float r = m_r;
			float g = m_g;
			float b = m_b;
			if (m_style == STYLE_SHADOW)
			{
				const float rim = unit * m_art_scale;
				for (const std::array<float, 2>& offset : RIM_OFFSETS)
				{
					DrawSheetFrameScaled(renderer, m_sheet, m_frame, foot.x + offset[0] * rim, foot.y + offset[1] * rim, m_art_scale, 0.95f * m_r, 0.35f * m_g, 1.7f * m_b, 0.8f * m_alpha, 2.f);
				}

				r = 0.24f;
				g = 0.17f;
				b = 0.34f;
			}

			DrawSheetFrameScaled(renderer, m_sheet, m_frame, foot.x, foot.y, m_art_scale, r, g, b, m_alpha, 0.f);
		}

		void Tick(float delta_seconds, const std::map<int32, std::unique_ptr<CineActor>>& actors)
		{
			m_anim_time += delta_seconds;
			m_idle_time += delta_seconds;

			if (m_moving)
			{
				m_move_time += delta_seconds;
				const float ratio = std::min(m_move_time / std::max(m_move_length, 0.01f), 1.f);
				const WorldPos before = m_pos;

				m_pos = m_move_from;
				MoveWorldPos(&m_pos, m_move_x * ratio, m_move_y * ratio);

				m_walk += std::sqrt(std::pow(GetTileDistanceX(before, m_pos), 2.f) + std::pow(GetTileDistanceY(before, m_pos), 2.f));

				if (ratio >= 1.f)
				{
					m_moving = false;
					m_anim = ANIM_IDLE;
					m_skill_anim = -1;
				}
			}

			if (m_alpha_length > 0.f)
			{
				m_alpha_time += delta_seconds;
				m_alpha = Lerp(m_alpha_from, m_alpha_to, m_alpha_time / m_alpha_length);
			}

			if (m_follow >= 0)
			{
				const auto found = actors.find(m_follow);
				if (found != actors.end())
				{
					m_pos = found->second->m_pos;
					MoveWorldPos(&m_pos, m_follow_x, m_follow_y);
					m_walk = found->second->m_walk;
					m_anim = found->second->m_anim;
					m_face_x = found->second->m_face_x;
					m_face_y = found->second->m_face_y;
				}
			}

			PickFrame();
		}

		void PickFrame()
		{
			if (m_sheet == SHEET_NONE)
			{
				return;
			}

			const int32 dir = GetDir8(m_face_x, m_face_y);
			int32 anim = GetSheetAnim(m_sheet, m_anim);
			if (m_skill_anim >= 0)
			{
				anim = m_skill_anim;
			}

			if (anim < 0)
			{
				anim = GetSheetAnim(m_sheet, ANIM_IDLE);
			}

			const float ratio = std::min(m_anim_time / std::max(m_anim_length, 0.05f), 1.f);
			m_frame = PickSheetFrame(m_sheet, anim, dir, ratio, m_idle_time, m_walk);
		}

		void SetAnim(const std::string& name, float length)
		{
			m_skill_anim = -1;
			m_anim_time = 0.f;
			m_anim_length = length;

			if (name.rfind("skill:", 0) == 0)
			{
				m_skill_anim = GetSheetSkillAnim(m_sheet, std::atoi(name.c_str() + 6));
				return;
			}

			const std::map<std::string, int32> slots =
			{
				{ "idle", ANIM_IDLE }, { "walk", ANIM_RUN }, { "run", ANIM_RUN }, { "attack", ANIM_ATTACK },
				{ "death", ANIM_DEATH }, { "hit", ANIM_HIT }, { "stun", ANIM_STUN }, { "guard", ANIM_GUARD },
			};

			const auto found = slots.find(name);
			m_anim = ANIM_IDLE;
			if (found != slots.end())
			{
				m_anim = found->second;
			}
		}

	public:
		int32 m_anim = ANIM_IDLE;
		int32 m_skill_anim = -1;
		float m_anim_time = 0.f;
		float m_anim_length = 1.f;
		float m_idle_time = 0.f;
		float m_walk = 0.f;
		float m_face_x = 1.f;
		float m_face_y = 0.f;
		float m_r = 1.f;
		float m_g = 1.f;
		float m_b = 1.f;
		float m_alpha = 1.f;
		int32 m_style = STYLE_NORMAL;
		bool m_lantern = false;

		bool m_moving = false;
		WorldPos m_move_from;
		float m_move_x = 0.f;
		float m_move_y = 0.f;
		float m_move_time = 0.f;
		float m_move_length = 0.f;

		float m_alpha_from = 1.f;
		float m_alpha_to = 1.f;
		float m_alpha_time = 0.f;
		float m_alpha_length = 0.f;

		int32 m_follow = -1;
		float m_follow_x = 0.f;
		float m_follow_y = 0.f;
	};

	std::map<std::string, WorldPos> g_anchors;
	const CinematicData* g_playing = nullptr;
	std::string g_finished;
	float g_time = 0.f;
	size_t g_next_step = 0;
	WorldPos g_anchor;
	std::map<int32, std::unique_ptr<CineActor>> g_actors;

	WorldPos g_camera;
	WorldPos g_camera_from;
	float g_camera_x = 0.f;
	float g_camera_y = 0.f;
	float g_camera_time = 0.f;
	float g_camera_length = 0.f;

	float g_letterbox = 0.f;
	float g_letterbox_target = 0.f;
	float g_dark = 0.f;
	float g_dark_target = 0.f;

	float g_fade = 0.f;
	float g_fade_from = 0.f;
	float g_fade_to = 0.f;
	float g_fade_time = 0.f;
	float g_fade_length = 0.f;
	std::array<float, 3> g_fade_color = { 0.f, 0.f, 0.f };

	std::wstring g_text;
	std::wstring g_speaker;
	float g_text_time = 0.f;
	float g_text_length = 0.f;

	std::wstring g_title;
	std::wstring g_title_sub;
	float g_title_time = 0.f;
	float g_title_length = 0.f;

	// 게임을 안 멈추는 자막
	std::vector<std::wstring> g_narration;
	float g_narration_time = 0.f;

	WorldPos OffsetFromAnchor(float x, float y)
	{
		WorldPos pos = g_anchor;
		MoveWorldPos(&pos, x, y);
		return pos;
	}

	CineActor* FindActor(int32 id)
	{
		const auto found = g_actors.find(id);
		if (found == g_actors.end())
		{
			return nullptr;
		}

		return found->second.get();
	}

	void Spawn(const CineStep& step)
	{
		std::unique_ptr<CineActor> actor = std::make_unique<CineActor>();
		actor->m_sheet = ParseSheet(step.sheet);
		actor->m_pos = OffsetFromAnchor(step.x, step.y);
		actor->m_face_x = step.face_x;
		actor->m_face_y = step.face_y;
		actor->m_style = ParseStyle(step.style);
		actor->m_r = step.color[0];
		actor->m_g = step.color[1];
		actor->m_b = step.color[2];
		actor->m_alpha = step.to;
		actor->m_art_scale = std::max(1.f, std::floor(step.scale + 0.5f));
		actor->m_lantern = step.flag;
		actor->m_casts_shadow = actor->m_style == STYLE_NORMAL;
		actor->PickFrame();

		g_actors[step.actor] = std::move(actor);
	}

	void Execute(const CineStep& step)
	{
		CineActor* actor = FindActor(step.actor);

		if (step.type == "letterbox")
		{
			g_letterbox_target = step.flag ? 1.f : 0.f;
		}
		else if (step.type == "dark")
		{
			g_dark_target = step.flag ? 1.f : 0.f;
		}
		else if (step.type == "camera")
		{
			g_camera_from = g_camera;
			const WorldPos to = OffsetFromAnchor(step.x, step.y);
			g_camera_x = GetTileDistanceX(g_camera, to);
			g_camera_y = GetTileDistanceY(g_camera, to);
			g_camera_time = 0.f;
			g_camera_length = step.duration;

			if (step.duration <= 0.f)
			{
				g_camera = to;
				g_camera_length = 0.f;
			}
		}
		else if (step.type == "spawn")
		{
			Spawn(step);
		}
		else if (step.type == "move" && actor != nullptr)
		{
			actor->m_moving = true;
			actor->m_move_from = actor->m_pos;
			const WorldPos to = OffsetFromAnchor(step.x, step.y);
			actor->m_move_x = GetTileDistanceX(actor->m_pos, to);
			actor->m_move_y = GetTileDistanceY(actor->m_pos, to);
			actor->m_move_time = 0.f;
			actor->m_move_length = std::max(step.duration, 0.01f);

			const float length = std::sqrt(actor->m_move_x * actor->m_move_x + actor->m_move_y * actor->m_move_y);
			if (length > 0.01f)
			{
				actor->m_face_x = actor->m_move_x / length;
				actor->m_face_y = actor->m_move_y / length;
			}

			actor->SetAnim("walk", 1.f);
		}
		else if (step.type == "anim" && actor != nullptr)
		{
			actor->SetAnim(step.anim, std::max(step.duration, 0.05f));
		}
		else if (step.type == "face" && actor != nullptr)
		{
			actor->m_face_x = step.face_x;
			actor->m_face_y = step.face_y;
		}
		else if (step.type == "alpha" && actor != nullptr)
		{
			actor->m_alpha_from = actor->m_alpha;
			actor->m_alpha_to = step.to;
			actor->m_alpha_time = 0.f;
			actor->m_alpha_length = std::max(step.duration, 0.01f);
		}
		else if (step.type == "tint" && actor != nullptr)
		{
			actor->m_r = step.color[0];
			actor->m_g = step.color[1];
			actor->m_b = step.color[2];
			actor->m_style = ParseStyle(step.style);
		}
		else if (step.type == "follow" && actor != nullptr)
		{
			actor->m_follow = step.target;
			actor->m_follow_x = step.x;
			actor->m_follow_y = step.y;
		}
		else if (step.type == "remove")
		{
			g_actors.erase(step.actor);
		}
		else if (step.type == "smoke")
		{
			WorldPos pos = OffsetFromAnchor(step.x, step.y);
			if (actor != nullptr)
			{
				pos = actor->m_pos;
			}

			SpawnDust(pos, 18, 0.06f, 0.03f, 0.09f);
			SpawnSparks(pos, 30.f, 0.f, 1.f, 14, 90.f, 2.4f, 1.1f, 0.4f, 1.8f, 0.9f);
		}
		else if (step.type == "text")
		{
			g_text = step.text;
			g_speaker = step.speaker;
			g_text_time = 0.f;
			g_text_length = step.duration;
		}
		else if (step.type == "title")
		{
			g_title = step.text;
			g_title_sub = step.sub;
			g_title_time = 0.f;
			g_title_length = step.duration;
		}
		else if (step.type == "flash")
		{
			AddScreenFlash(step.color[0], step.color[1], step.color[2], step.to);
		}
		else if (step.type == "shake")
		{
			AddShake(step.to);
		}
		else if (step.type == "fade")
		{
			g_fade_from = step.from;
			g_fade_to = step.to;
			g_fade_time = 0.f;
			g_fade_length = std::max(step.duration, 0.01f);
			g_fade_color = step.color;
		}
		else if (step.type == "sound")
		{
			const int32 sound = ParseSound(step.sound);
			if (sound >= 0)
			{
				PlaySfx(sound, 0.9f);
			}
		}
	}

	void Finish()
	{
		std::cout << "[ Cinematic - Finish ] : " << g_playing->id << "\n";

		g_finished = g_playing->id;
		g_playing = nullptr;
		g_actors.clear();
		g_letterbox_target = 0.f;
		g_dark_target = 0.f;
		g_fade = 0.f;
		g_fade_length = 0.f;
		g_text.clear();
		g_title.clear();
	}

	// 아래 가운데 자막. 화자가 있으면 이름을 앞에 금빛으로
	void DrawLine(Renderer* renderer, const std::wstring& speaker, const std::wstring& text, float elapsed, float length, float y)
	{
		float alpha = std::min(elapsed / 0.3f, 1.f);
		if (length > 0.f && elapsed > length - 0.4f)
		{
			alpha = std::max(0.f, (length - elapsed) / 0.4f);
		}

		const size_t shown = std::min(text.size(), static_cast<size_t>(elapsed * TYPE_SPEED) + 1);
		const std::wstring visible = text.substr(0, shown);

		if (speaker.empty() == false)
		{
			DrawStringShadow(renderer, speaker, 0.f, y + 30.f, TEXT_ALIGN_CENTER, 1.f, 0.82f, 0.45f, alpha);
		}

		DrawStringShadow(renderer, visible, 0.f, y, TEXT_ALIGN_CENTER, 0.95f, 0.92f, 0.86f, alpha);
	}
}

void SetCinematicAnchor(const std::string& name, const WorldPos& pos)
{
	g_anchors[name] = pos;
}

bool GetCinematicAnchor(const std::string& name, WorldPos* pos)
{
	const auto found = g_anchors.find(name);
	if (found == g_anchors.end())
	{
		return false;
	}

	*pos = found->second;
	return true;
}

void StartCinematic(const std::string& id)
{
	const CinematicData* cinematic = FindCinematic(id);
	if (cinematic == nullptr)
	{
		std::cout << "[ Cinematic - StartCinematic ] : missing " << id << "\n";
		g_finished = id;
		return;
	}

	g_playing = cinematic;
	g_time = 0.f;
	g_next_step = 0;
	g_actors.clear();
	g_anchor = g_anchors[cinematic->anchor];
	g_camera = g_anchor;
	g_camera_length = 0.f;
	g_text.clear();
	g_title.clear();

	std::cout << "[ Cinematic - StartCinematic ] : " << id << " " << cinematic->length << " sec\n";
}

bool IsCinematicPlaying()
{
	return g_playing != nullptr;
}

void SkipCinematic()
{
	if (g_playing == nullptr)
	{
		return;
	}

	Finish();
}

std::string PopFinishedCinematic()
{
	std::string finished = g_finished;
	g_finished.clear();
	return finished;
}

void UpdateCinematic(float real_seconds)
{
	if (std::fabs(g_letterbox - g_letterbox_target) < LETTERBOX_SPEED * real_seconds)
	{
		g_letterbox = g_letterbox_target;
	}
	else if (g_letterbox < g_letterbox_target)
	{
		g_letterbox += LETTERBOX_SPEED * real_seconds;
	}
	else
	{
		g_letterbox -= LETTERBOX_SPEED * real_seconds;
	}

	if (std::fabs(g_dark - g_dark_target) < DARK_SPEED * real_seconds)
	{
		g_dark = g_dark_target;
	}
	else if (g_dark < g_dark_target)
	{
		g_dark += DARK_SPEED * real_seconds;
	}
	else
	{
		g_dark -= DARK_SPEED * real_seconds;
	}

	// 게임을 안 멈추는 자막은 컷신과 따로 흐른다
	if (g_narration.empty() == false)
	{
		g_narration_time += real_seconds;
		if (g_narration_time >= NARRATION_LINE_SEC)
		{
			g_narration.erase(g_narration.begin());
			g_narration_time = 0.f;
		}
	}

	if (g_playing == nullptr)
	{
		return;
	}

	g_time += real_seconds;

	while (g_next_step < g_playing->steps.size() && g_playing->steps[g_next_step].time <= g_time)
	{
		Execute(g_playing->steps[g_next_step]);
		g_next_step += 1;
	}

	if (g_camera_length > 0.f)
	{
		g_camera_time += real_seconds;
		const float ratio = std::min(g_camera_time / g_camera_length, 1.f);
		const float eased = ratio * ratio * (3.f - 2.f * ratio);

		g_camera = g_camera_from;
		MoveWorldPos(&g_camera, g_camera_x * eased, g_camera_y * eased);
	}

	if (g_fade_length > 0.f)
	{
		g_fade_time += real_seconds;
		g_fade = Lerp(g_fade_from, g_fade_to, g_fade_time / g_fade_length);
	}

	g_text_time += real_seconds;
	g_title_time += real_seconds;

	for (std::pair<const int32, std::unique_ptr<CineActor>>& entry : g_actors)
	{
		entry.second->Tick(real_seconds, g_actors);
	}

	if (g_time >= g_playing->length && g_next_step >= g_playing->steps.size())
	{
		Finish();
	}
}

bool GetCinematicCamera(WorldPos* camera)
{
	if (g_playing == nullptr)
	{
		return false;
	}

	*camera = g_camera;
	return true;
}

bool IsCinematicHidingPlayer()
{
	return g_playing != nullptr && (g_playing->hide_player || g_dark > 0.5f);
}

bool IsCinematicHidingWorld()
{
	return g_playing != nullptr && g_dark > 0.5f;
}

void CollectCinematicActors()
{
	for (const std::pair<const int32, std::unique_ptr<CineActor>>& entry : g_actors)
	{
		AddSceneActor(entry.second.get());
	}
}

void DrawCinematicGround(Renderer* renderer, const WorldPos& camera)
{
	if (g_dark <= 0.f)
	{
		return;
	}

	// 어둠 속에 배우만 남긴다. 배경을 덮고 배우는 그 위에 그려진다
	renderer->DrawSolidBox(0.f, 0.f, static_cast<float>(WINDOW_SIZE_X), static_cast<float>(WINDOW_SIZE_Y), 0.f, 0.f, 0.005f, g_dark);
}

void DrawCinematicOverlay(Renderer* renderer)
{
	const float half_x = static_cast<float>(WINDOW_SIZE_X) * 0.5f;
	const float half_y = static_cast<float>(WINDOW_SIZE_Y) * 0.5f;

	if (g_fade > 0.f)
	{
		renderer->DrawSolidBox(0.f, 0.f, half_x * 2.f, half_y * 2.f, g_fade_color[0], g_fade_color[1], g_fade_color[2], g_fade);
	}

	if (g_letterbox > 0.f)
	{
		const float bar = LETTERBOX_HEIGHT * g_letterbox;
		renderer->DrawSolidBox(0.f, half_y - bar * 0.5f, half_x * 2.f, bar, 0.f, 0.f, 0.f, 1.f);
		renderer->DrawSolidBox(0.f, -half_y + bar * 0.5f, half_x * 2.f, bar, 0.f, 0.f, 0.f, 1.f);
	}

	if (g_playing == nullptr)
	{
		return;
	}

	if (g_title.empty() == false && g_title_time < g_title_length)
	{
		float alpha = std::min(g_title_time / 0.6f, 1.f);
		if (g_title_time > g_title_length - 0.6f)
		{
			alpha = std::max(0.f, (g_title_length - g_title_time) / 0.6f);
		}

		DrawStringOutline(renderer, g_title, 0.f, 40.f, TEXT_ALIGN_CENTER, 2, 0.96f, 0.86f, 0.62f, alpha);
		DrawStringShadow(renderer, g_title_sub, 0.f, -24.f, TEXT_ALIGN_CENTER, 0.78f, 0.74f, 0.66f, alpha);
	}

	if (g_text.empty() == false && g_text_time < g_text_length)
	{
		DrawLine(renderer, g_speaker, g_text, g_text_time, g_text_length, -half_y + 52.f);
	}

	DrawStringShadow(renderer, L"ESC  넘기기", half_x - 20.f, half_y - 24.f, TEXT_ALIGN_RIGHT, 0.5f, 0.48f, 0.44f, 0.8f);
}

void QueueNarration(const std::vector<std::wstring>& lines)
{
	for (const std::wstring& line : lines)
	{
		g_narration.push_back(line);
	}
}

void DrawNarration(Renderer* renderer)
{
	if (g_narration.empty() || g_playing != nullptr)
	{
		return;
	}

	const std::wstring& line = g_narration.front();
	const float y = -170.f;
	renderer->DrawSolidBox(0.f, y - 12.f, MeasureString(line) + 48.f, 34.f, 0.02f, 0.015f, 0.01f, 0.6f);
	DrawLine(renderer, L"", line, g_narration_time, NARRATION_LINE_SEC, y);
}
