# -*- coding: utf-8 -*-
# 효과음 만들기. 본 프로젝트(Project-Unnamed) wav 를 잘라 쓰고, 없는 소리는 합성한다.
# 결과는 SimpleGame/Sounds/이름_번호.wav ( 48 kHz 모노 16 비트 ) - 게임이 전부 같은 포맷이라야 목소리를 돌려 쓴다
# python sound_build.py
import math
import os
import sys
import wave

import numpy as np

RATE = 48000
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, '..', 'SimpleGame', 'Sounds')
SOURCE_DIR = r'C:\Users\aches\Desktop\Project\Project-Unnamed\Sound'
LEVEL_UP_SOURCE = r'C:\Users\aches\Desktop\Project\Project-Unnamed\Client\AssetSources\LevelUp\SW_LevelUp_Chime.wav'

rng = np.random.default_rng(20261006)


# ------------------------------------------------------------ 입출력

def read_wave(path):
    with wave.open(path, 'rb') as w:
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        frames = w.readframes(w.getnframes())
    if width != 2:
        raise ValueError('16 bit only: ' + path)
    data = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0
    if channels > 1:
        data = data.reshape(-1, channels).mean(axis=1)
    if rate != RATE:
        length = int(len(data) * RATE / rate)
        data = np.interp(np.linspace(0, len(data) - 1, length), np.arange(len(data)), data).astype(np.float32)
    return data


def write_wave(name, data, peak_db=-1.5):
    data = np.asarray(data, dtype=np.float32)
    peak = np.max(np.abs(data)) + 1e-9
    data = data / peak * (10 ** (peak_db / 20.0))
    pcm = np.clip(data * 32767.0, -32768, 32767).astype(np.int16)
    path = os.path.join(OUT_DIR, name + '.wav')
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print('wrote', name, round(len(data) / RATE, 3), 's')


# ------------------------------------------------------------ 자르기

def onset_slice(data, start_sec, length_sec, pre_sec=0.004, fade_out_sec=0.08):
    # 시작점을 소리가 실제로 올라오는 자리로 당긴다 ( 타격과 소리가 같은 틱에 와야 손맛이다 )
    start = int(start_sec * RATE)
    window = data[start:start + int(0.15 * RATE)]
    envelope = np.abs(window)
    threshold = envelope.max() * 0.2
    hits = np.nonzero(envelope > threshold)[0]
    if len(hits):
        start += int(hits[0])
    start = max(0, start - int(pre_sec * RATE))
    piece = data[start:start + int(length_sec * RATE)].copy()
    fade = int(fade_out_sec * RATE)
    if fade < len(piece):
        piece[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2
    attack = int(0.002 * RATE)
    piece[:attack] *= np.linspace(0.0, 1.0, attack)
    return piece


def source(name):
    return read_wave(os.path.join(SOURCE_DIR, name))


# ------------------------------------------------------------ 합성 도구

def t_axis(length_sec):
    return np.arange(int(length_sec * RATE)) / RATE


def env_exp(length_sec, decay_sec, attack_sec=0.001):
    t = t_axis(length_sec)
    env = np.exp(-t / decay_sec)
    attack = int(attack_sec * RATE)
    if attack > 0:
        env[:attack] *= np.linspace(0.0, 1.0, attack)
    return env


def noise(length_sec):
    return rng.standard_normal(int(length_sec * RATE)).astype(np.float32)


def one_pole_lowpass(data, cutoff):
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
    out = np.empty_like(data)
    state = 0.0
    for index in range(len(data)):
        state += alpha * (data[index] - state)
        out[index] = state
    return out


def bandpass(data, low, high):
    return one_pole_lowpass(data, high) - one_pole_lowpass(data, low)


def sweep_sine(length_sec, freq_from, freq_to, curve=1.0):
    t = t_axis(length_sec)
    ratio = (t / max(length_sec, 1e-6)) ** curve
    freq = freq_from + (freq_to - freq_from) * ratio
    phase = 2.0 * math.pi * np.cumsum(freq) / RATE
    return np.sin(phase).astype(np.float32)


def partials(length_sec, freqs, decays, gains):
    t = t_axis(length_sec)
    out = np.zeros_like(t, dtype=np.float32)
    for freq, decay, gain in zip(freqs, decays, gains):
        out += gain * np.sin(2.0 * math.pi * freq * t + rng.uniform(0, 6.28)) * np.exp(-t / decay)
    return out


def pad(data, length_sec):
    target = int(length_sec * RATE)
    if len(data) >= target:
        return data[:target]
    return np.concatenate([data, np.zeros(target - len(data), dtype=np.float32)])


def mix(*layers):
    length = max(len(layer) for layer in layers)
    out = np.zeros(length, dtype=np.float32)
    for layer in layers:
        out[:len(layer)] += layer
    return out


def delay(data, delay_sec, gain):
    offset = int(delay_sec * RATE)
    out = np.concatenate([data, np.zeros(offset, dtype=np.float32)])
    out[offset:] += data * gain
    return out


def small_room(data, tail_sec=0.35, amount=0.25):
    # 짧은 잔향. 지수로 줄어드는 노이즈와 컨볼루션
    ir = noise(tail_sec) * env_exp(tail_sec, tail_sec / 4.0)
    ir = one_pole_lowpass(ir, 5000.0)
    wet = np.convolve(data, ir)[:len(data) + len(ir)] * amount / (np.max(np.abs(ir)) * 20.0)
    return mix(data, wet)


# ------------------------------------------------------------ 소리별

def make_hit_flesh(seed_shift):
    length = 0.28
    thump = sweep_sine(0.16, 110.0 + seed_shift * 6, 45.0, 0.6) * env_exp(0.16, 0.05) * 0.9
    crunch = bandpass(noise(0.12), 250.0, 1400.0) * env_exp(0.12, 0.03) * 1.4
    click = bandpass(noise(0.03), 2500.0, 7000.0) * env_exp(0.03, 0.006) * 0.7
    return pad(mix(thump, crunch, click), length)


def make_hit_heavy(seed_shift):
    thump = sweep_sine(0.32, 80.0 + seed_shift * 5, 30.0, 0.5) * env_exp(0.32, 0.09) * 1.0
    crunch = bandpass(noise(0.22), 180.0, 1800.0) * env_exp(0.22, 0.06) * 1.3
    crack = bandpass(noise(0.05), 1800.0, 6000.0) * env_exp(0.05, 0.01) * 0.9
    return small_room(mix(thump, crunch, crack), 0.4, 0.2)


def make_ring(base, length=0.5):
    ratios = [1.0, 1.51, 2.27, 2.93]
    return partials(length, [base * r for r in ratios], [0.22, 0.15, 0.09, 0.06], [0.6, 0.35, 0.22, 0.12])


def make_hit_crit(seed_shift):
    body = make_hit_heavy(seed_shift)
    shing = make_ring(2900.0 + seed_shift * 140, 0.45) * 0.55
    return mix(body, shing)


def make_guard(seed_shift):
    base = 820.0 + seed_shift * 60
    ring = partials(0.42, [base, base * 2.71, base * 4.43, base * 6.1], [0.16, 0.1, 0.06, 0.04], [0.7, 0.45, 0.3, 0.15])
    click = bandpass(noise(0.04), 1500.0, 9000.0) * env_exp(0.04, 0.008) * 0.9
    thud = sweep_sine(0.1, 160.0, 70.0) * env_exp(0.1, 0.03) * 0.5
    return small_room(mix(ring, click, thud), 0.3, 0.2)


def make_guard_break():
    crash = bandpass(noise(0.6), 300.0, 7000.0) * env_exp(0.6, 0.12) * 1.2
    fall = sweep_sine(0.5, 900.0, 180.0, 0.7) * env_exp(0.5, 0.15) * 0.5
    boom = sweep_sine(0.5, 70.0, 28.0) * env_exp(0.5, 0.18) * 0.9
    ring = partials(0.6, [640.0, 1730.0, 2840.0], [0.25, 0.14, 0.08], [0.5, 0.3, 0.2])
    return small_room(mix(crash, fall, boom, ring), 0.5, 0.3)


def make_player_hurt(seed_shift):
    thud = sweep_sine(0.2, 95.0 + seed_shift * 8, 40.0) * env_exp(0.2, 0.06) * 1.0
    grit = bandpass(noise(0.16), 200.0, 1200.0) * env_exp(0.16, 0.04) * 1.0
    return pad(mix(thud, grit), 0.3)


def make_glint():
    # 패링 타이밍 신호. 짧고 높고 맑아야 전투 소음 위로 뚫고 나온다
    ting = partials(0.35, [3520.0, 5280.0, 7040.0], [0.09, 0.06, 0.04], [0.7, 0.4, 0.2])
    shimmer = partials(0.35, [3560.0], [0.12], [0.3])
    return small_room(mix(ting, shimmer), 0.25, 0.15)


def make_danger():
    t = t_axis(0.5)
    low = np.sign(np.sin(2 * math.pi * 92.0 * t)) * 0.35
    low = one_pole_lowpass(low.astype(np.float32), 900.0)
    pulse = (np.exp(-t / 0.07) + np.exp(-np.maximum(t - 0.17, 0) / 0.07) * (t > 0.17)).astype(np.float32)
    dissonance = partials(0.5, [1480.0, 1568.0], [0.12, 0.12], [0.35, 0.35])
    return mix(low * pulse, dissonance * pulse * 0.6)


def make_monster_die(seed_shift):
    whoosh = bandpass(noise(0.7), 120.0, 900.0 + seed_shift * 100) * env_exp(0.7, 0.18) * 1.2
    crumble = bandpass(noise(0.7), 900.0, 4000.0) * env_exp(0.7, 0.25) * 0.4
    groan = sweep_sine(0.6, 180.0, 70.0) * env_exp(0.6, 0.2) * 0.5
    return mix(whoosh, crumble, groan)


def make_potion():
    out = np.zeros(int(0.55 * RATE), dtype=np.float32)
    for index, start in enumerate([0.0, 0.13, 0.27]):
        pop = sweep_sine(0.09, 260.0 + index * 40, 620.0 + index * 60, 0.8) * env_exp(0.09, 0.03)
        offset = int(start * RATE)
        out[offset:offset + len(pop)] += pop * 0.8
    tink = partials(0.4, [2600.0, 4100.0], [0.08, 0.05], [0.25, 0.15])
    sparkle = partials(0.4, [1568.0, 2093.0], [0.15, 0.12], [0.2, 0.2])
    tink_late = np.concatenate([np.zeros(int(0.32 * RATE), dtype=np.float32), tink])
    sparkle_late = np.concatenate([np.zeros(int(0.3 * RATE), dtype=np.float32), sparkle])
    return mix(out, tink_late, sparkle_late)


def make_pickup():
    first = partials(0.12, [880.0, 1760.0], [0.05, 0.03], [0.6, 0.2])
    second = partials(0.2, [1320.0, 2640.0], [0.07, 0.04], [0.6, 0.2])
    return mix(first, np.concatenate([np.zeros(int(0.06 * RATE), dtype=np.float32), second]))


def make_gold(seed_shift):
    out = np.zeros(int(0.3 * RATE), dtype=np.float32)
    for start in [0.0, 0.05 + seed_shift * 0.01]:
        chink = partials(0.2, [4200.0 + seed_shift * 150, 5900.0, 7300.0], [0.05, 0.035, 0.02], [0.5, 0.3, 0.2])
        offset = int(start * RATE)
        out[offset:offset + len(chink)] += chink[:len(out) - offset]
    return out


def make_ui_click():
    click = bandpass(noise(0.02), 2000.0, 8000.0) * env_exp(0.02, 0.004)
    return pad(mix(click, partials(0.03, [1900.0], [0.01], [0.3])), 0.05)


def make_ui_error():
    t = t_axis(0.16)
    buzz = np.sign(np.sin(2 * math.pi * 140.0 * t)).astype(np.float32) * 0.3
    buzz = one_pole_lowpass(buzz, 1200.0) * env_exp(0.16, 0.08)
    return buzz


def make_hammer():
    clang = partials(0.6, [1180.0, 2950.0, 4400.0], [0.2, 0.12, 0.07], [0.6, 0.35, 0.2])
    hit = bandpass(noise(0.05), 800.0, 8000.0) * env_exp(0.05, 0.01) * 0.8
    return mix(clang, hit)


def make_enhance_success():
    chord = np.zeros(int(1.2 * RATE), dtype=np.float32)
    for index, freq in enumerate([523.25, 659.25, 783.99, 1046.5]):
        note = partials(1.0, [freq, freq * 2.0], [0.35, 0.2], [0.35, 0.12])
        offset = int((0.18 + index * 0.06) * RATE)
        chord[offset:offset + len(note)] += note[:len(chord) - offset]
    return small_room(mix(make_hammer(), chord), 0.6, 0.35)


def make_enhance_fail():
    dull = sweep_sine(0.5, 300.0, 120.0) * env_exp(0.5, 0.15) * 0.6
    return small_room(mix(make_hammer() * 0.7, np.concatenate([np.zeros(int(0.15 * RATE), dtype=np.float32), dull])), 0.4, 0.2)


def make_enhance_break():
    shatter = bandpass(noise(0.9), 1500.0, 9000.0) * env_exp(0.9, 0.2) * 0.9
    shards = np.zeros(int(0.9 * RATE), dtype=np.float32)
    for index in range(10):
        ping = partials(0.25, [rng.uniform(3000, 7000)], [0.05], [0.3])
        offset = int(rng.uniform(0.02, 0.6) * RATE)
        shards[offset:offset + len(ping)] += ping[:len(shards) - offset]
    boom = sweep_sine(0.7, 90.0, 30.0) * env_exp(0.7, 0.2) * 0.8
    return small_room(mix(make_hammer(), shatter, shards, boom), 0.6, 0.3)


def make_monster_swing(seed_shift):
    length = 0.36
    t = t_axis(length)
    whoosh = noise(length)
    center = 500.0 + 900.0 * np.sin(np.pi * t / length)
    out = np.zeros_like(whoosh)
    state_low = 0.0
    state_high = 0.0
    for index in range(len(whoosh)):
        cutoff = center[index] + seed_shift * 50
        alpha_high = 1.0 - math.exp(-2.0 * math.pi * cutoff * 1.6 / RATE)
        alpha_low = 1.0 - math.exp(-2.0 * math.pi * cutoff * 0.6 / RATE)
        state_high += alpha_high * (whoosh[index] - state_high)
        state_low += alpha_low * (whoosh[index] - state_low)
        out[index] = state_high - state_low
    return out * np.sin(np.pi * t / length) ** 2


def make_player_die():
    boom = sweep_sine(1.2, 70.0, 25.0) * env_exp(1.2, 0.35)
    air = bandpass(noise(1.2), 100.0, 600.0) * env_exp(1.2, 0.4) * 0.6
    return small_room(mix(boom, air), 1.0, 0.4)


def make_skill_unlock():
    out = np.zeros(int(1.0 * RATE), dtype=np.float32)
    for index, freq in enumerate([659.25, 987.77, 1318.5, 1975.5]):
        note = partials(0.7, [freq, freq * 1.5], [0.3, 0.15], [0.35, 0.1])
        offset = int(index * 0.07 * RATE)
        out[offset:offset + len(note)] += note[:len(out) - offset]
    return small_room(out, 0.6, 0.35)


def build():
    os.makedirs(OUT_DIR, exist_ok=True)

    # 본 프로젝트 녹음 - 잘라 쓴다
    parry = source('Sword_parry.wav')
    for index, start in enumerate([0.08, 2.0, 4.04, 5.2]):
        clip = onset_slice(parry, start, 0.95, fade_out_sec=0.35)
        # 패링은 맞부딪는 쇳소리 위에 낮은 충격을 깐다. 녹음만으로는 화면 정지의 무게가 안 실린다
        punch = sweep_sine(0.18, 120.0, 50.0) * env_exp(0.18, 0.05) * 0.35
        write_wave('parry_%d' % (index + 1), mix(clip, punch))

    knife = source('Knife_attack_D.wav')
    knife2 = source('Knife_attack_D2.wav')
    swings = [onset_slice(knife, 0.06, 0.32), onset_slice(knife, 0.54, 0.32), onset_slice(knife2, 0.06, 0.3), onset_slice(knife2, 0.38, 0.32)]
    for index, clip in enumerate(swings):
        write_wave('swing_light_%d' % (index + 1), clip, peak_db=-3.0)

    katana = source(os.path.join('기존', 'Katana swing_2.wav'))
    for index, start in enumerate([0.12, 0.76, 2.28]):
        write_wave('swing_heavy_%d' % (index + 1), onset_slice(katana, start, 0.5, fade_out_sec=0.15), peak_db=-2.0)

    dodge = source('Dodge_roll.wav')
    write_wave('dodge_1', onset_slice(dodge, 0.0, 0.62, fade_out_sec=0.15), peak_db=-4.0)

    draw = source(os.path.join('기존', 'Katana draw_3.wav'))
    write_wave('equip_1', onset_slice(draw, 0.38, 0.7, fade_out_sec=0.25), peak_db=-4.0)

    if os.path.exists(LEVEL_UP_SOURCE):
        chime = read_wave(LEVEL_UP_SOURCE)
        chime = chime[:int(2.6 * RATE)]
        chime[-int(0.6 * RATE):] *= np.linspace(1.0, 0.0, int(0.6 * RATE)) ** 2
        write_wave('level_up_1', chime, peak_db=-3.0)
    else:
        write_wave('level_up_1', make_skill_unlock())

    # 합성
    for index in range(3):
        write_wave('hit_flesh_%d' % (index + 1), make_hit_flesh(index), peak_db=-2.0)
    for index in range(2):
        write_wave('hit_heavy_%d' % (index + 1), make_hit_heavy(index), peak_db=-1.5)
        write_wave('hit_crit_%d' % (index + 1), make_hit_crit(index), peak_db=-1.5)
        write_wave('player_hurt_%d' % (index + 1), make_player_hurt(index), peak_db=-2.0)
        write_wave('monster_die_%d' % (index + 1), make_monster_die(index), peak_db=-4.0)
        write_wave('gold_%d' % (index + 1), make_gold(index), peak_db=-8.0)
        write_wave('monster_swing_%d' % (index + 1), make_monster_swing(index), peak_db=-5.0)
    for index in range(3):
        write_wave('guard_%d' % (index + 1), make_guard(index), peak_db=-2.0)

    write_wave('guard_break_1', make_guard_break(), peak_db=-1.0)
    write_wave('glint_1', make_glint(), peak_db=-5.0)
    write_wave('danger_1', make_danger(), peak_db=-4.0)
    write_wave('potion_1', make_potion(), peak_db=-4.0)
    write_wave('pickup_1', make_pickup(), peak_db=-7.0)
    write_wave('ui_click_1', make_ui_click(), peak_db=-10.0)
    write_wave('ui_error_1', make_ui_error(), peak_db=-9.0)
    write_wave('enhance_success_1', make_enhance_success(), peak_db=-3.0)
    write_wave('enhance_fail_1', make_enhance_fail(), peak_db=-3.0)
    write_wave('enhance_break_1', make_enhance_break(), peak_db=-2.0)
    write_wave('player_die_1', make_player_die(), peak_db=-2.0)
    write_wave('skill_unlock_1', make_skill_unlock(), peak_db=-4.0)
    write_wave('counter_1', mix(make_hit_crit(3), make_ring(2400.0, 0.6) * 0.5), peak_db=-1.0)


if __name__ == '__main__':
    build()
