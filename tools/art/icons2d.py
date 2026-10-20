# -*- coding: utf-8 -*-
# 판 위에 찍는 2D 도트 - 스킬 아이콘 11 개 ( 16 x 16 ) 와 작은 조각들
import math

from vfx import BLUE, CYAN, DEEP_BLUE, GOLD, GOLD_DARK, ORANGE, PALE, RED, WHITE, YELLOW, Canvas

STEEL = (176, 190, 214)
STEEL_DARK = (84, 98, 128)
INK = (18, 16, 24)


def crescent(c, cx, cy, radius, a0, a1, width, colors):
    # 가운데가 두껍고 끝이 가는 칼 궤적. 색은 바깥에서 안으로 겹쳐 칠한다
    steps = 60
    for layer, (color, ratio) in enumerate(colors):
        for i in range(steps + 1):
            t = i / steps
            a = a0 + (a1 - a0) * t
            w = width * ratio * math.sin(t * math.pi) ** 0.8
            if w < 0.45:
                continue
            x = cx + math.cos(a) * radius
            y = cy + math.sin(a) * radius
            c.disc(x, y, w * 0.5, color)


SLASH_COLORS = [(STEEL_DARK, 1.0), (STEEL, 0.62), (WHITE, 0.28)]


def icon_slash(flip):
    c = Canvas(16, 16)
    if flip:
        crescent(c, 3.0, 13.0, 10.5, -1.45, 0.05, 4.2, SLASH_COLORS)
    else:
        crescent(c, 12.0, 13.0, 10.5, math.pi + 1.45, math.pi - 0.05, 4.2, SLASH_COLORS)
    c.outline(INK)
    return c.px


def icon_combo():
    c = Canvas(16, 16)
    for k in range(3):
        crescent(c, 2.0 + k * 3.0, 15.0, 9.0, -1.35, -0.2, 2.8, SLASH_COLORS)
    c.outline(INK)
    return c.px


def icon_spin_combo():
    c = Canvas(16, 16)
    crescent(c, 7.5, 7.5, 5.2, -0.6, 4.6, 3.2, SLASH_COLORS)
    # 끝 화살촉
    c.disc(7.5 + math.cos(4.6) * 5.2, 7.5 + math.sin(4.6) * 5.2, 1.4, WHITE)
    c.outline(INK)
    return c.px


def icon_parry():
    c = Canvas(16, 16)
    c.line(2, 2, 13, 13, STEEL, width=2)
    c.line(13, 2, 2, 13, STEEL, width=2)
    c.line(3, 3, 12, 12, WHITE)
    for k in range(4):
        a = k * math.pi / 2.0
        c.spike(7.5, 7.5, a, 0.5, 6.5, 2.4, CYAN)
    c.disc(7.5, 7.5, 2.4, WHITE)
    c.outline(DEEP_BLUE)
    return c.px


def icon_dash_slash():
    c = Canvas(16, 16)
    for k, y in enumerate((4, 8, 12)):
        c.line(1, y, 5 + k, y, BLUE)
    c.line(4, 8, 13, 8, STEEL, width=2)
    c.line(5, 7, 13, 7, WHITE)
    c.spike(13.5, 8.0, 0.0, 0.0, 2.5, 2.5, WHITE)
    c.line(4, 5, 4, 11, GOLD)
    c.outline(INK)
    return c.px


def icon_spin_slash():
    c = Canvas(16, 16)
    colors = [((150, 60, 20), 1.0), (ORANGE, 0.62), (YELLOW, 0.3)]
    crescent(c, 7.5, 7.5, 6.0, 0.0, 3.4, 3.4, colors)
    crescent(c, 7.5, 7.5, 6.0, math.pi, math.pi + 3.4, 3.4, colors)
    c.disc(7.5, 7.5, 1.6, PALE)
    c.outline(INK)
    return c.px


def icon_leap():
    c = Canvas(16, 16)
    c.line(7.5, 1, 7.5, 9, STEEL, width=2)
    c.spike(7.5, 9.5, math.pi * 0.5, 0.0, 3.0, 4.5, WHITE)
    c.line(1, 14, 14, 14, ORANGE, width=2)
    c.line(3, 12, 1, 10, YELLOW)
    c.line(12, 12, 14, 10, YELLOW)
    c.outline(INK)
    return c.px


def icon_counter():
    c = Canvas(16, 16)
    for k in range(8):
        a = k * math.pi / 4.0
        c.spike(7.5, 7.5, a, 1.0, 7.0 if k % 2 == 0 else 4.5, 2.6, GOLD)
    crescent(c, 3.0, 13.0, 10.0, -1.45, 0.05, 3.2, [(GOLD_DARK, 1.0), (YELLOW, 0.6), (WHITE, 0.28)])
    c.disc(7.5, 7.5, 1.6, WHITE)
    c.outline(INK)
    return c.px


def icon_dodge():
    c = Canvas(16, 16)
    for k, (y, length) in enumerate(((4, 9), (8, 12), (12, 7))):
        c.line(14 - length, y, 14, y, CYAN, width=2)
        c.line(15 - length, y, 13, y, WHITE)
    c.outline(DEEP_BLUE)
    return c.px


def icon_roll():
    c = Canvas(16, 16)
    colors = [(DEEP_BLUE, 1.0), (CYAN, 0.6), (WHITE, 0.25)]
    crescent(c, 7.5, 7.5, 5.0, 0.4, 5.6, 3.0, colors)
    c.spike(7.5 + math.cos(5.6) * 5.0, 7.5 + math.sin(5.6) * 5.0, 5.6 + math.pi * 0.5, 0.0, 3.0, 3.0, CYAN)
    c.outline((10, 20, 50))
    return c.px


SKILL_ICONS = [lambda: icon_slash(False), lambda: icon_slash(True), icon_combo, icon_spin_combo, icon_parry,
               icon_dash_slash, icon_spin_slash, icon_leap, icon_counter, icon_dodge, icon_roll]


def small_orb(core, rim):
    c = Canvas(5, 5)
    c.disc(2.0, 2.0, 2.2, rim)
    c.disc(1.6, 1.6, 1.0, core)
    return c.px


def pixel():
    c = Canvas(1, 1)
    c.put(0, 0, WHITE)
    return c.px


FLAT = {
    'SPRITE_EXP_ORB': [lambda: small_orb((200, 255, 210), (60, 190, 110))],
    'SPRITE_DUST': [lambda: small_orb((170, 162, 150), (110, 104, 96))],
    'SPRITE_FRAGMENT': [lambda: small_orb(WHITE, (176, 178, 196))],
    'SPRITE_CORE': [lambda: small_orb((255, 228, 140), (255, 156, 48))],
    'SPRITE_PIXEL': [pixel],
    'SPRITE_SKILL_ICON': SKILL_ICONS,
}
