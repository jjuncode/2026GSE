# -*- coding: utf-8 -*-
# 타격 · 패링 · 가드 · 흙먼지 · 충격파 · 레벨업 · 회복 · 재 흩어짐 프레임 시트.
# 도트 칸 단위로 직접 찍는다. 사라질 때는 반투명 대신 바이어 무늬로 칸을 솎아 낸다
import json
import math
import os
import random

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(ROOT, 'SimpleGame', 'Art')

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=float)
BAYER = (BAYER + 0.5) / 16.0

WHITE = (255, 255, 255)
PALE = (255, 244, 200)
YELLOW = (255, 214, 110)
ORANGE = (255, 150, 50)
RED = (214, 66, 30)
EMBER_DARK = (110, 30, 20)
CYAN = (150, 230, 255)
BLUE = (80, 160, 255)
DEEP_BLUE = (40, 70, 170)
GOLD = (255, 220, 120)
GOLD_DARK = (190, 120, 40)
GREEN = (140, 255, 160)
GREEN_DARK = (40, 160, 90)
DUST = (150, 140, 124)
DUST_DARK = (96, 88, 78)
ASH = (180, 176, 170)
ASH_DARK = (90, 86, 84)


class Canvas:
    def __init__(self, width, height):
        self.w = width
        self.h = height
        self.px = np.zeros((height, width, 4), dtype=np.uint8)
        self.cx = width / 2.0 - 0.5
        self.cy = height / 2.0 - 0.5

    def put(self, x, y, color, fade=0.0):
        x = int(round(x))
        y = int(round(y))
        if 0 <= x < self.w and 0 <= y < self.h:
            if fade > 0.0 and BAYER[y % 4, x % 4] < fade:
                return
            self.px[y, x, :3] = color
            self.px[y, x, 3] = 255

    def disc(self, x, y, radius, color, fade=0.0, squash=1.0):
        r = int(math.ceil(radius)) + 1
        for yy in range(int(y) - r, int(y) + r + 1):
            for xx in range(int(x) - r, int(x) + r + 1):
                dx = xx - x
                dy = (yy - y) / squash
                if dx * dx + dy * dy <= radius * radius:
                    self.put(xx, yy, color, fade)

    def ring(self, x, y, radius, thickness, color, fade=0.0, squash=1.0, gaps=0, gap_phase=0.0):
        r = int(math.ceil(radius + thickness)) + 1
        for yy in range(int(y) - r, int(y) + r + 1):
            for xx in range(int(x) - r, int(x) + r + 1):
                dx = xx - x
                dy = (yy - y) / squash
                d = math.sqrt(dx * dx + dy * dy)
                if radius - thickness * 0.5 <= d <= radius + thickness * 0.5:
                    if gaps > 0:
                        a = (math.atan2(dy, dx) / (2.0 * math.pi) * gaps + gap_phase) % 1.0
                        if a < 0.35:
                            continue
                    self.put(xx, yy, color, fade)

    def line(self, x0, y0, x1, y1, color, fade=0.0, width=1):
        steps = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(steps + 1):
            t = i / max(steps, 1)
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            if width <= 1:
                self.put(x, y, color, fade)
            else:
                self.disc(x, y, width * 0.5, color, fade)

    def spike(self, x, y, angle, inner, outer, base_width, color, fade=0.0):
        # 뿌리가 두껍고 끝이 가는 가시
        steps = int(outer - inner) * 2 + 1
        for i in range(steps + 1):
            t = i / max(steps, 1)
            d = inner + (outer - inner) * t
            px = x + math.cos(angle) * d
            py = y + math.sin(angle) * d
            half = base_width * (1.0 - t) * 0.5
            if half < 0.6:
                self.put(px, py, color, fade)
            else:
                self.disc(px, py, half, color, fade)

    def outline(self, color):
        # 빈 칸 중 그림에 닿은 칸을 어둡게 ( 밝은 배경 위에서도 읽히게 )
        solid = self.px[:, :, 3] > 0
        edge = np.zeros_like(solid)
        edge[1:, :] |= solid[:-1, :]
        edge[:-1, :] |= solid[1:, :]
        edge[:, 1:] |= solid[:, :-1]
        edge[:, :-1] |= solid[:, 1:]
        edge &= ~solid
        self.px[edge, :3] = color
        self.px[edge, 3] = 255


def burst(size, frames, core, mid, edge, dark, spikes, reach, ring_color, seed, big=False):
    rng = random.Random(seed)
    angles = [i * 2.0 * math.pi / spikes + rng.uniform(-0.25, 0.25) for i in range(spikes)]
    lengths = [rng.uniform(0.7, 1.0) for _ in range(spikes)]
    embers = [(rng.uniform(0, 2 * math.pi), rng.uniform(0.5, 1.0)) for _ in range(10)]
    out = []
    for f in range(frames):
        c = Canvas(size, size)
        t = f / (frames - 1)
        x = c.cx
        y = c.cy
        if f == 0:
            c.disc(x, y, reach * 0.22, mid)
            c.disc(x, y, reach * 0.14, core)
            for a in angles[::2]:
                c.spike(x, y, a, 0, reach * 0.4, 2.0, core)
        elif f == 1:
            for a, l in zip(angles, lengths):
                c.spike(x, y, a, reach * 0.1, reach * l, 3.0 if big else 2.2, edge)
                c.spike(x, y, a, 0, reach * l * 0.75, 2.0, mid)
            c.disc(x, y, reach * 0.3, mid)
            c.disc(x, y, reach * 0.2, core)
        elif f == 2:
            c.ring(x, y, reach * 0.55, 2.0, ring_color)
            for a, l in zip(angles, lengths):
                c.spike(x, y, a, reach * 0.45, reach * (l + 0.15), 2.0, edge)
            c.disc(x, y, reach * 0.2, mid)
            c.disc(x, y, reach * 0.12, core)
        else:
            fade = (f - 2) / (frames - 2)
            c.ring(x, y, reach * (0.55 + 0.25 * fade), 1.5, ring_color, fade=fade * 0.9, gaps=spikes // 2, gap_phase=f * 0.1)
            for a, d in embers:
                dist = reach * (0.5 + 0.7 * d * fade)
                c.put(x + math.cos(a) * dist, y + math.sin(a) * dist + fade * 3.0, edge if f < frames - 1 else dark, fade * 0.6)
            if f == 3:
                c.disc(x, y, reach * 0.1, mid)
        c.outline(dark)
        out.append(c.px)
    return out


def parry_frames():
    size = 60
    rng = random.Random(7)
    out = []
    for f in range(9):
        c = Canvas(size, size)
        x = c.cx
        y = c.cy
        if f == 0:
            c.disc(x, y, 10, CYAN)
            c.disc(x, y, 7, WHITE)
        elif f == 1:
            for k in range(8):
                a = k * math.pi / 4.0
                length = 27 if k % 2 == 0 else 15
                c.spike(x, y, a, 3, length, 3.6, CYAN)
                c.spike(x, y, a, 0, length * 0.7, 2.2, WHITE)
            c.disc(x, y, 7, WHITE)
        elif f == 2:
            c.ring(x, y, 14, 3.5, CYAN)
            c.ring(x, y, 14, 1.8, WHITE)
            for k in range(4):
                a = k * math.pi / 2.0 + math.pi / 4.0
                c.spike(x, y, a, 7, 25, 2.8, CYAN)
            c.disc(x, y, 5, WHITE)
        else:
            fade = (f - 3) / 5.0
            c.ring(x, y, 18 + fade * 8, 2.0, CYAN, fade=fade * 0.85)
            c.ring(x, y, 18 + fade * 8, 1.0, WHITE, fade=min(1.0, fade * 1.2))
            for k in range(14):
                a = rng.uniform(0, 2 * math.pi)
                d = rng.uniform(12, 28) * (0.6 + fade * 0.6)
                c.put(x + math.cos(a) * d, y + math.sin(a) * d, WHITE if k % 3 else CYAN, fade * 0.7)
            if f == 3:
                c.disc(x, y, 4, WHITE)
        c.outline(DEEP_BLUE)
        out.append(c.px)
    return out


def guard_frames():
    size = 32
    rng = random.Random(3)
    sparks = [(rng.uniform(-2.4, -0.7), rng.uniform(6, 14)) for _ in range(9)]
    out = []
    for f in range(5):
        c = Canvas(size, size)
        x = c.cx
        y = c.cy + 2
        t = f / 4.0
        if f == 0:
            c.disc(x, y, 4, CYAN)
            c.disc(x, y, 2.5, WHITE)
        else:
            for a, d in sparks:
                dist = d * (0.5 + t)
                tail = dist - 4
                c.line(x + math.cos(a) * tail, y + math.sin(a) * tail + t * t * 4, x + math.cos(a) * dist, y + math.sin(a) * dist + t * t * 5,
                       WHITE if f < 3 else CYAN, fade=max(0.0, t - 0.4))
            if f == 1:
                c.ring(x, y, 5, 1.5, CYAN)
        c.outline(DEEP_BLUE)
        out.append(c.px)
    return out


def dust_frames():
    w = 28
    h = 18
    rng = random.Random(5)
    puffs = [(rng.uniform(-7, 7), rng.uniform(-2, 2), rng.uniform(3, 5)) for _ in range(4)]
    out = []
    for f in range(6):
        c = Canvas(w, h)
        t = f / 5.0
        for dx, dy, r in puffs:
            x = c.cx + dx * (0.6 + t * 0.8)
            y = c.cy + 3 + dy - t * 5.0
            rr = r * (0.7 + t * 0.6)
            c.disc(x, y, rr, DUST_DARK, fade=t * 0.95, squash=0.8)
            c.disc(x - 0.6, y - 0.8, rr * 0.65, DUST, fade=t * 0.95, squash=0.8)
        out.append(c.px)
    return out


def slam_frames():
    w = 112
    h = 56
    rng = random.Random(11)
    rocks = [(rng.uniform(0, 2 * math.pi), rng.uniform(0.4, 1.0), rng.uniform(4, 9)) for _ in range(16)]
    out = []
    for f in range(8):
        c = Canvas(w, h)
        t = f / 7.0
        x = c.cx
        y = c.cy + 6
        radius = 8 + t * 44
        fade = max(0.0, t - 0.3) / 0.7
        c.ring(x, y, radius, 3.0 - t * 1.5, DUST, fade=fade * 0.9, squash=0.5)
        c.ring(x, y, radius - 2.5, 1.2, PALE if f < 3 else DUST, fade=fade, squash=0.5)
        if f < 3:
            c.disc(x, y, 10 - f * 2, ORANGE, squash=0.5)
            c.disc(x, y, 6 - f * 2, PALE, squash=0.5)
        for a, d, up in rocks:
            dist = radius * d
            lift = math.sin(min(t * 1.6, 1.0) * math.pi) * up
            rx = x + math.cos(a) * dist
            ry = y + math.sin(a) * dist * 0.5 - lift
            c.put(rx, ry, DUST_DARK, fade * 0.8)
            c.put(rx + 1, ry, (70, 62, 54), fade * 0.8)
        out.append(c.px)
    return out


def levelup_frames():
    w = 40
    h = 68
    rng = random.Random(13)
    motes = [(rng.uniform(-14, 14), rng.uniform(0, 1)) for _ in range(18)]
    out = []
    for f in range(10):
        c = Canvas(w, h)
        t = f / 9.0
        x = c.cx
        base = h - 10
        width = 7 * (1.0 - t) + 1
        top = base - 50 * min(1.0, t * 3.0)
        fade = max(0.0, t - 0.4) / 0.6
        for yy in range(int(top), base):
            u = (base - yy) / 50.0
            half = width * (1.0 - u * 0.5)
            for xx in range(int(x - half), int(x + half) + 1):
                edge = abs(xx - x) > half - 1.5
                c.put(xx, yy, GOLD if edge else PALE, fade=max(fade, u * 0.5 if edge else fade))
        c.ring(x, base, 7 + t * 11, 1.5, GOLD, fade=fade, squash=0.5)
        for dx, phase in motes:
            yy = base - ((phase + t * 1.4) % 1.0) * 56
            c.put(x + dx * (0.3 + t * 0.5), yy, GOLD if phase > 0.5 else PALE, fade * 0.8)
        c.outline(GOLD_DARK)
        out.append(c.px)
    return out


def heal_frames():
    w = 32
    h = 44
    rng = random.Random(17)
    crosses = [(rng.uniform(-10, 10), rng.uniform(0, 1), rng.choice((1, 2))) for _ in range(6)]
    out = []
    for f in range(8):
        c = Canvas(w, h)
        t = f / 7.0
        for dx, phase, size in crosses:
            yy = h - 6 - ((phase + t) % 1.0) * 34
            xx = c.cx + dx
            fade = max(0.0, t - 0.5) * 1.6
            for k in range(-size, size + 1):
                c.put(xx + k, yy, GREEN, fade)
                c.put(xx, yy + k, GREEN, fade)
        c.ring(c.cx, h - 6, 6 + t * 8, 1.0, GREEN, fade=t, squash=0.5)
        c.outline(GREEN_DARK)
        out.append(c.px)
    return out


def ash_frames():
    size = 56
    rng = random.Random(19)
    flakes = [(rng.uniform(-14, 14), rng.uniform(-18, 4), rng.uniform(-0.6, 0.6), rng.uniform(0.6, 1.4), rng.random() < 0.25) for _ in range(40)]
    out = []
    for f in range(10):
        c = Canvas(size, size)
        t = f / 9.0
        for x0, y0, drift, rise, ember in flakes:
            x = c.cx + x0 + drift * t * 14 + math.sin(t * 6 + x0) * 1.5
            y = c.cy + 12 + y0 - rise * t * 22
            color = ORANGE if ember else (ASH if rise > 1.0 else ASH_DARK)
            c.put(x, y, color, fade=max(0.0, t * 1.1 - 0.1))
        out.append(c.px)
    return out


def cut_frames():
    # 베인 자리 빛줄기. 가로로 그려 두고 게임이 돌린다
    w = 48
    h = 12
    out = []
    for f in range(5):
        c = Canvas(w, h)
        t = f / 4.0
        y = c.cy
        half = 22 * (1.0 - t * 0.35)
        thick = 3.0 * (1.0 - t)
        if thick >= 1.0:
            for xx in range(int(c.cx - half), int(c.cx + half) + 1):
                u = abs(xx - c.cx) / half
                core_half = thick * (1.0 - u) * 0.5
                for yy in range(int(y - core_half - 1), int(y + core_half + 2)):
                    if abs(yy - y) <= core_half:
                        c.put(xx, yy, WHITE)
                    elif abs(yy - y) <= core_half + 1:
                        c.put(xx, yy, PALE)
        else:
            for xx in range(int(c.cx - half), int(c.cx + half) + 1, 2):
                c.put(xx, y, PALE, fade=t * 0.8)
        out.append(c.px)
    return out


EFFECTS = [
    # 이름, 프레임, 한 장 시간
    # 사람 키가 23 칸이라 터지는 반경은 몸 절반 남짓으로
    ('hit', lambda: burst(28, 6, WHITE, PALE, YELLOW, EMBER_DARK, 7, 11, ORANGE, 1), 0.045),
    ('crit', lambda: burst(44, 7, WHITE, YELLOW, ORANGE, EMBER_DARK, 9, 18, RED, 2, big=True), 0.05),
    ('hurt', lambda: burst(28, 6, WHITE, (255, 190, 180), (255, 90, 80), (90, 10, 20), 6, 10, (200, 40, 40), 3), 0.045),
    ('parry', parry_frames, 0.045),
    ('guard', guard_frames, 0.04),
    ('dust', dust_frames, 0.06),
    ('slam', slam_frames, 0.055),
    ('levelup', levelup_frames, 0.07),
    ('heal', heal_frames, 0.07),
    ('ash', ash_frames, 0.08),
    ('cut', cut_frames, 0.035),
]


def main():
    all_frames = []
    effects = {}
    for name, fn, frame_sec in EFFECTS:
        frames = fn()
        h, w = frames[0].shape[:2]
        effects[name] = dict(first=len(all_frames), count=len(frames), frame_sec=frame_sec, w=w, h=h)
        all_frames.extend(frames)

    # 효과마다 크기가 달라 줄 단위로 싣는다
    sheet_w = 1024
    x = 1
    y = 1
    shelf = 0
    places = []
    for img in all_frames:
        h, w = img.shape[:2]
        if x + w + 1 > sheet_w:
            x = 1
            y += shelf + 1
            shelf = 0
        places.append((x, y))
        x += w + 1
        shelf = max(shelf, h)
    sheet_h = 64
    while sheet_h < y + shelf + 1:
        sheet_h *= 2
    sheet = np.zeros((sheet_h, sheet_w, 4), dtype=np.uint8)
    frames = []
    for img, (px, py) in zip(all_frames, places):
        h, w = img.shape[:2]
        sheet[py:py + h, px:px + w] = img
        frames.append([px, py, w, h, w // 2, h // 2])

    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray(sheet, 'RGBA').save(os.path.join(OUT_DIR, 'vfx.png'), optimize=True)
    with open(os.path.join(OUT_DIR, 'vfx.json'), 'w', encoding='utf-8', newline=chr(10)) as f:
        json.dump(dict(name='vfx', width=sheet_w, height=sheet_h, frames=frames, effects=effects), f, separators=(',', ':'))
    print('vfx frames', len(all_frames), 'sheet', sheet_w, 'x', sheet_h)


if __name__ == '__main__':
    main()
