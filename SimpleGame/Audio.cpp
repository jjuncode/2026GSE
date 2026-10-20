#include "stdafx.h"
#include "Audio.h"
#include "GameData.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xaudio2.h>

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
	constexpr int32 VOICE_COUNT = 40;
	constexpr int32 MAX_VARIANT_COUNT = 8;
	const float SAME_SOUND_GAP_SEC = 0.035f;	// 다단 히트가 한 틱에 소리를 겹쳐 쌓지 않게
	const float PITCH_JITTER = 0.06f;
	const float VOLUME_JITTER = 0.08f;

	const std::array<const char*, SFX_COUNT> SOUND_NAMES =
	{
		"swing_light", "swing_heavy", "hit_flesh", "hit_heavy", "hit_crit",
		"guard", "guard_break", "parry", "player_hurt", "dodge",
		"glint", "danger", "monster_die", "potion", "pickup",
		"gold", "level_up", "ui_click", "ui_error", "enhance_success",
		"enhance_fail", "enhance_break", "equip", "monster_swing", "player_die",
		"skill_unlock", "counter",
	};

	struct SoundClip
	{
		std::vector<uint8> pcm;
	};

	struct VoiceSlot
	{
		IXAudio2SourceVoice* voice = nullptr;
		uint64 started = 0;	// 가장 오래된 것부터 뺏는다
	};

	IXAudio2* g_engine = nullptr;
	IXAudio2MasteringVoice* g_master = nullptr;
	WAVEFORMATEX g_format = {};
	bool g_format_known = false;
	std::array<std::vector<SoundClip>, SFX_COUNT> g_clips;
	std::array<VoiceSlot, VOICE_COUNT> g_voices;
	std::array<std::chrono::steady_clock::time_point, SFX_COUNT> g_last_played;
	uint64 g_play_serial = 0;
	bool g_com_ready = false;
	float g_master_volume = 0.3f;

	std::mt19937& GetRandom()
	{
		static std::mt19937 random(static_cast<uint32>(std::chrono::steady_clock::now().time_since_epoch().count()) ^ 0x165667b1u);
		return random;
	}

	float RollRange(float min_value, float max_value)
	{
		std::uniform_real_distribution<float> distribution(min_value, max_value);
		return distribution(GetRandom());
	}

	uint32 ReadU32(const std::vector<uint8>& bytes, size_t offset)
	{
		return static_cast<uint32>(bytes[offset]) | (static_cast<uint32>(bytes[offset + 1]) << 8)
			| (static_cast<uint32>(bytes[offset + 2]) << 16) | (static_cast<uint32>(bytes[offset + 3]) << 24);
	}

	uint16 ReadU16(const std::vector<uint8>& bytes, size_t offset)
	{
		return static_cast<uint16>(static_cast<uint32>(bytes[offset]) | (static_cast<uint32>(bytes[offset + 1]) << 8));
	}

	// RIFF 안에서 fmt 와 data 덩어리만 찾는다. 소리는 전부 같은 포맷이라야 목소리를 돌려 쓴다
	bool LoadWave(const std::string& path, WAVEFORMATEX* format, std::vector<uint8>* pcm)
	{
		std::ifstream file(path, std::ios::binary);
		if (file.is_open() == false)
		{
			return false;
		}

		const std::vector<uint8> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
		{
			return false;
		}

		bool has_format = false;
		size_t offset = 12;

		while (offset + 8 <= bytes.size())
		{
			const uint32 chunk_size = ReadU32(bytes, offset + 4);
			const size_t body = offset + 8;

			if (body + chunk_size > bytes.size())
			{
				break;
			}

			if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0 && chunk_size >= 16)
			{
				*format = {};
				format->wFormatTag = ReadU16(bytes, body);
				format->nChannels = ReadU16(bytes, body + 2);
				format->nSamplesPerSec = ReadU32(bytes, body + 4);
				format->nAvgBytesPerSec = ReadU32(bytes, body + 8);
				format->nBlockAlign = ReadU16(bytes, body + 12);
				format->wBitsPerSample = ReadU16(bytes, body + 14);
				has_format = true;
			}
			else if (std::memcmp(bytes.data() + offset, "data", 4) == 0)
			{
				pcm->assign(bytes.begin() + static_cast<std::ptrdiff_t>(body), bytes.begin() + static_cast<std::ptrdiff_t>(body + chunk_size));
			}

			// 덩어리는 짝수 바이트로 맞춘다
			offset = body + chunk_size + (chunk_size & 1u);
		}

		return has_format && pcm->empty() == false && format->wFormatTag == WAVE_FORMAT_PCM;
	}

	bool IsSameFormat(const WAVEFORMATEX& left, const WAVEFORMATEX& right)
	{
		return left.nChannels == right.nChannels && left.nSamplesPerSec == right.nSamplesPerSec && left.wBitsPerSample == right.wBitsPerSample;
	}

	void LoadClips()
	{
		int32 loaded = 0;

		for (size_t sfx = 0; sfx < SOUND_NAMES.size(); ++sfx)
		{
			for (int32 variant = 1; variant <= MAX_VARIANT_COUNT; ++variant)
			{
				const std::string path = std::string("./Sounds/") + SOUND_NAMES[sfx] + "_" + std::to_string(variant) + ".wav";

				WAVEFORMATEX format = {};
				SoundClip clip;
				if (LoadWave(path, &format, &clip.pcm) == false)
				{
					break;
				}

				if (g_format_known == false)
				{
					g_format = format;
					g_format_known = true;
				}

				if (IsSameFormat(g_format, format) == false)
				{
					std::cout << "[ Audio - LoadClips ] : format mismatch " << path << "\n";
					continue;
				}

				g_clips[sfx].push_back(clip);
				loaded += 1;
			}
		}

		std::cout << "[ Audio - LoadClips ] : clips " << loaded << "\n";
	}

	VoiceSlot* FindVoice()
	{
		VoiceSlot* oldest = nullptr;

		for (VoiceSlot& slot : g_voices)
		{
			if (slot.voice == nullptr)
			{
				continue;
			}

			XAUDIO2_VOICE_STATE state = {};
			slot.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

			if (state.BuffersQueued == 0)
			{
				return &slot;
			}

			if (oldest == nullptr || slot.started < oldest->started)
			{
				oldest = &slot;
			}
		}

		// 다 차면 제일 먼저 시작한 소리를 끊는다
		if (oldest != nullptr)
		{
			oldest->voice->Stop(0);
			oldest->voice->FlushSourceBuffers();
		}

		return oldest;
	}
}

bool InitAudio()
{
	const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	g_com_ready = SUCCEEDED(com_result);

	if (FAILED(XAudio2Create(&g_engine, 0, XAUDIO2_DEFAULT_PROCESSOR)))
	{
		std::cout << "[ Audio - InitAudio ] : XAudio2Create failed\n";
		g_engine = nullptr;
		return false;
	}

	if (FAILED(g_engine->CreateMasteringVoice(&g_master)))
	{
		std::cout << "[ Audio - InitAudio ] : no output device\n";
		g_engine->Release();
		g_engine = nullptr;
		return false;
	}

	LoadClips();

	if (g_format_known == false)
	{
		std::cout << "[ Audio - InitAudio ] : no sound files in ./Sounds\n";
		return false;
	}

	for (VoiceSlot& slot : g_voices)
	{
		// 높이를 올려 재생할 수 있게 최대 비율을 넉넉히
		if (FAILED(g_engine->CreateSourceVoice(&slot.voice, &g_format, 0, 2.f)))
		{
			slot.voice = nullptr;
		}
	}

	// 녹음과 합성 소리가 거의 최대치로 맞춰져 있어 전체를 크게 낮춰 둔다. - = 키로 조절
	g_master_volume = GetConfig().audio_master_volume;
	g_master->SetVolume(g_master_volume);
	return true;
}

void ShutdownAudio()
{
	for (VoiceSlot& slot : g_voices)
	{
		if (slot.voice != nullptr)
		{
			slot.voice->DestroyVoice();
			slot.voice = nullptr;
		}
	}

	if (g_master != nullptr)
	{
		g_master->DestroyVoice();
		g_master = nullptr;
	}

	if (g_engine != nullptr)
	{
		g_engine->Release();
		g_engine = nullptr;
	}

	for (std::vector<SoundClip>& clips : g_clips)
	{
		clips.clear();
	}

	if (g_com_ready)
	{
		CoUninitialize();
		g_com_ready = false;
	}
}

void PlaySfx(int32 sfx, float volume)
{
	PlaySfxPitch(sfx, volume, 1.f + RollRange(-PITCH_JITTER, PITCH_JITTER));
}

void PlaySfxPitch(int32 sfx, float volume, float pitch)
{
	if (g_engine == nullptr || sfx < 0 || sfx >= SFX_COUNT)
	{
		return;
	}

	const std::vector<SoundClip>& clips = g_clips[static_cast<size_t>(sfx)];
	if (clips.empty())
	{
		return;
	}

	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	const std::chrono::duration<float> since = now - g_last_played[static_cast<size_t>(sfx)];
	if (since.count() < SAME_SOUND_GAP_SEC)
	{
		return;
	}

	g_last_played[static_cast<size_t>(sfx)] = now;

	VoiceSlot* slot = FindVoice();
	if (slot == nullptr)
	{
		return;
	}

	std::uniform_int_distribution<int32> pick(0, static_cast<int32>(clips.size()) - 1);
	const SoundClip& clip = clips[static_cast<size_t>(pick(GetRandom()))];

	XAUDIO2_BUFFER buffer = {};
	buffer.AudioBytes = static_cast<UINT32>(clip.pcm.size());
	buffer.pAudioData = clip.pcm.data();
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	if (FAILED(slot->voice->SubmitSourceBuffer(&buffer)))
	{
		return;
	}

	float jittered = volume * (1.f + RollRange(-VOLUME_JITTER, VOLUME_JITTER));
	if (jittered < 0.f)
	{
		jittered = 0.f;
	}

	slot->voice->SetVolume(jittered);
	slot->voice->SetFrequencyRatio(pitch);
	slot->voice->Start(0);

	g_play_serial += 1;
	slot->started = g_play_serial;
}

void SetMasterVolume(float volume)
{
	if (volume < 0.f)
	{
		volume = 0.f;
	}

	if (volume > 1.f)
	{
		volume = 1.f;
	}

	g_master_volume = volume;

	if (g_master != nullptr)
	{
		g_master->SetVolume(volume);
	}
}

float GetMasterVolume()
{
	return g_master_volume;
}

float ChangeMasterVolume(float delta)
{
	SetMasterVolume(g_master_volume + delta);
	return g_master_volume;
}
