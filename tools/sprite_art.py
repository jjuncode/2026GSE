# -*- coding: utf-8 -*-
# 도트 그림 원본. 실행하면 SimpleGame/SpriteArt.cpp 를 다시 만든다.
# python sprite_art.py --preview 경로.png  -> 확대 미리보기만 그린다
import math
import os
import sys

PALETTE = [
    # 외곽선과 어둠
    ('k', (20, 16, 18)), ('K', (44, 38, 40)),
    # 피부
    ('s', (196, 150, 120)), ('S', (140, 100, 80)),
    # 개척대 청록 망토
    ('t', (46, 112, 104)), ('T', (26, 70, 68)),
    # 가죽
    ('b', (118, 84, 56)), ('B', (74, 52, 36)),
    # 쇠
    ('a', (176, 180, 188)), ('A', (108, 112, 122)), ('d', (60, 62, 70)),
    # 천
    ('c', (160, 152, 136)), ('C', (104, 98, 88)), ('x', (64, 58, 56)),
    # 붉은색과 불씨 눈
    ('r', (150, 40, 36)), ('R', (92, 24, 26)), ('e', (255, 96, 40)),
    # 금
    ('y', (222, 180, 84)), ('Y', (142, 106, 46)),
    # 보라 ( 행상 옷 )
    ('p', (124, 66, 112)), ('P', (78, 38, 72)),
    # 이끼와 약초
    ('g', (98, 114, 74)), ('G', (58, 70, 46)), ('h', (140, 236, 160)),
    # 나무
    ('w', (124, 96, 64)), ('W', (80, 60, 42)),
    # 뼈
    ('o', (216, 208, 188)), ('O', (150, 142, 124)),
    # 거식귀 살과 입
    ('f', (212, 182, 164)), ('F', (158, 124, 110)), ('m', (104, 50, 50)), ('n', (240, 234, 216)),
    # 타락자 살
    ('u', (116, 106, 124)), ('U', (72, 64, 84)),
    # 재늑대 털
    ('j', (112, 108, 106)), ('J', (70, 66, 66)), ('i', (166, 160, 152)),
    # 돌 ( 밝은 쪽부터 )
    ('1', (140, 134, 126)), ('2', (108, 104, 98)), ('3', (80, 76, 74)), ('4', (48, 44, 44)), ('5', (34, 30, 32)),
    # 지붕과 회벽
    ('6', (112, 60, 46)), ('7', (70, 38, 32)), ('8', (158, 140, 112)), ('9', (114, 98, 80)),
    # 폐허 이끼
    ('l', (78, 94, 60)),
    # 재료 등급 - 흰 재가루 / 푸른 마석 / 주황 심장석
    ('v', (238, 238, 246)), ('V', (176, 178, 196)),
    ('q', (96, 156, 255)), ('Q', (40, 82, 206)),
    ('z', (255, 156, 48)), ('Z', (255, 228, 140)),
    ('#', (255, 255, 255)),
    # 경험치 구슬
    ('E', (140, 255, 170)), ('L', (60, 190, 110)),
    # 불꽃
    ('H', (255, 206, 96)), ('I', (255, 122, 42)), ('M', (186, 52, 22)),
    # 죽은 나무껍질
    ('N', (84, 72, 64)), ('X', (48, 42, 40)),
    # 마른 풀
    ('D', (128, 114, 76)), ('0', (86, 76, 54)),
]


# ------------------------------------------------------------ 도구

class Canvas:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.cells = [['.'] * width for _ in range(height)]

    def put(self, x, y, key):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.cells[y][x] = key

    def get(self, x, y):
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.cells[y][x]
        return '.'

    # 선분까지 거리가 반경 안이면 칠한다. 뒤쪽 ( 왼쪽 ) 은 그늘색
    def capsule(self, x0, y0, x1, y1, radius, key, shade=None):
        min_x = int(min(x0, x1) - radius - 1)
        max_x = int(max(x0, x1) + radius + 1)
        min_y = int(min(y0, y1) - radius - 1)
        max_y = int(max(y0, y1) + radius + 1)
        dx = x1 - x0
        dy = y1 - y0
        length_sq = dx * dx + dy * dy
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                px = x + 0.5
                py = y + 0.5
                t = 0.0
                if length_sq > 0:
                    t = max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / length_sq))
                cx = x0 + dx * t
                cy = y0 + dy * t
                if (px - cx) ** 2 + (py - cy) ** 2 <= radius * radius:
                    use = key
                    if shade is not None and px - cx < -radius * 0.25:
                        use = shade
                    self.put(x, y, use)

    def ellipse(self, cx, cy, rx, ry, key, shade=None):
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                nx = (x + 0.5 - cx) / rx
                ny = (y + 0.5 - cy) / ry
                if nx * nx + ny * ny <= 1.0:
                    use = key
                    if shade is not None and (nx < -0.35 or ny > 0.62):
                        use = shade
                    self.put(x, y, use)

    # 도트 그림은 외곽선이 있어야 어두운 바닥에서 덩어리로 읽힌다
    def outline(self, key='k'):
        marks = []
        for y in range(self.height):
            for x in range(self.width):
                if self.cells[y][x] != '.':
                    continue
                for ox, oy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    near = self.get(x + ox, y + oy)
                    if near != '.' and near != key:
                        marks.append((x, y))
                        break
        for x, y in marks:
            self.cells[y][x] = key

    def rows(self):
        return [''.join(row) for row in self.cells]


def hash2(x, y, salt):
    value = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((value ^ (value >> 16)) & 0xFFFF) / 65535.0


def swap_keys(rows, mapping):
    return [''.join(mapping.get(ch, ch) for ch in row) for row in rows]


# ------------------------------------------------------------ 사람 ( 13 x 21 )

def legs(pants, boots, walk):
    if walk:
        return [
            "....k%s.%sk...." % (pants, pants),
            "...k%sk.k%sk..." % (pants, pants),
            "..k%sk...k%sk.." % (pants, pants),
            "..k%sk...k%sk.." % (boots, boots),
            ".k%s%sk...k%s%sk." % (boots, boots, boots, boots),
            ".kkk.....kkk.",
        ]
    return [
        "....k%sk%sk...." % (pants, pants),
        "....k%sk%sk...." % (pants, pants),
        "....k%sk%sk...." % (pants, pants),
        "....k%sk%sk...." % (boots, boots),
        "...k%s%sk%s%sk..." % (boots, boots, boots, boots),
        "...kkk.kkk...",
    ]


def with_tail(rows, column_key):
    # 창 자루처럼 발밑까지 내려오는 것
    result = []
    for row in rows:
        result.append(row[:11] + column_key + row[12:])
    return result


PLAYER_TOP = [
    "....kkkkk....",
    "...kTtttTk...",
    "..kTttttttk..",
    "..kTtkksssk..",
    "..kTkssskSk..",
    "..kTksssssk..",
    "...kTkSSSk...",
    "..kTTkbbbkk..",
    ".kTttkbbybak.",
    ".kTttkbybbAk.",
    ".kTttkaBBsk..",
    ".kTttkBBBk...",
    ".kTTtkxxxk...",
    "..kTTkxxxk...",
    "...kkkxkxk...",
]

HUNTER_TOP = [
    ".W..kkkk.....",
    "W..kbbbbk....",
    "W.kbBbbbbk...",
    "W.kbkksssk...",
    "WkbBksskSk...",
    "WkiiksssskK..",
    "WkiiikSSk....",
    "kiiiigGgkk...",
    "kjiigGggsk...",
    "WkjgGgGgk....",
    "WkgGgggBk....",
    "W.kbbybbk....",
    ".WkGgggGk....",
    "..kGgggGk....",
    "...kkGkGk....",
]

RAIDER_TOP = [
    "....kkkk.....",
    "...kKKKKk....",
    "..kKKKKKKk...",
    "..kKksskSk...",
    "..kKrrrrrk...",
    "..kKrRrRrk.a.",
    "...kRrrrk.kak",
    "..kaKkxxkkWk.",
    ".kaaKxxxxWk..",
    ".kKKxrxxsk...",
    ".kKKxxxxk....",
    ".kKxBBBBk....",
    ".kKxxxxxk....",
    "..kxxxxxk....",
    "...kkxkxk....",
]

MERCHANT_TOP = [
    "....kkkkk....",
    "...kYyyyYk...",
    ".kkYyyyyyYkk.",
    "kYYYYYYYYYYYk",
    ".kkkssskskk..",
    "..wkssssssk..",
    ".wWkkSSSSk...",
    "kwWWkpppppk..",
    "kwwWkpPyPpk..",
    "kwWWkpppysk..",
    "kWwWkpPppk...",
    ".kWWkyyyyk...",
    "..kkpPpPpk...",
    "...kpPpPpk...",
    "...kkPpPpk...",
]

MERCHANT_LEGS_STAND = [
    "...kpPpPpk...",
    "...kPpPpPk...",
    "...kpPpPpk...",
    "...kkBkBkk...",
    "....kBkBk....",
    "....kk.kk....",
]

MERCHANT_LEGS_WALK = [
    "...kpPpPpk...",
    "...kPpPpPk...",
    "..kpPpPpPpk..",
    "..kkBk.kBkk..",
    "..kBk...kBk..",
    "..kk.....kk..",
]

GATHERER_TOP = [
    "....kkkk.....",
    "...kCccck....",
    "..kCcccccCk..",
    "..kCckssssk..",
    ".gkCksskSk...",
    "gGkCkssssk...",
    "kgGgkkSSk....",
    "kwwwwkcccck..",
    "kWwWwkcCcsk..",
    "kwWwWkccck...",
    "kWwWwkcCck...",
    ".kWWkkbbbk...",
    "..kkCcccCk...",
    "...kCcccCk...",
    "...kkCkCkk...",
]

WARDEN_TOP = [
    "....kkkk..ka.",
    "...kaaaAk.kak",
    "..kaAAAAAk.W.",
    "..kAkksssk.W.",
    "..kTksskSk.W.",
    "..kTkssssk.W.",
    "...kTkSSk..W.",
    "..kTTkAAAkkW.",
    ".kTttkAdAAsW.",
    ".kTttkdAdAkW.",
    ".kTttkAAAk.W.",
    ".kTttkyyyk.W.",
    ".kTTtkxxxk.W.",
    "..kTTkxxxk.W.",
    "...kkkxkxk.W.",
]


def humanoid(top, pants, boots):
    return [top + legs(pants, boots, False), top + legs(pants, boots, True)]


# ------------------------------------------------------------ 몬스터

BLIGHTED_TOP = [
    "...kkkk.....",
    "..kuUuuk....",
    "..kueuuek...",
    "..kuUUuk....",
    "...kmnmk....",
    "..kkUuUkkk..",
    ".kTTuuuuuuk.",
    ".kTTTuTkkuuk",
    ".kTuTTTk..uk",
    ".kTTTuTk...k",
    "..kTTTTk....",
    "..kTuTTk....",
    "..kUUUUk....",
]

BLIGHTED_STAND = BLIGHTED_TOP + [
    "..kUk.kUk...",
    "..kuk.kuk...",
    "..kUk.kUk...",
    "..kuk.kuk...",
    ".kUUk.kUUk..",
    ".kkk..kkk...",
]

BLIGHTED_WALK = BLIGHTED_TOP + [
    "..kUk.kUk...",
    ".kUk...kUk..",
    ".kuk...kuk..",
    ".kUk...kUk..",
    "kUUk...kUUk.",
    "kkk.....kkk.",
]

HOUND_TOP = [
    "..........k.k...",
    ".........kJkJk..",
    "k.......kjjjjjk.",
    "kJk....kjjjejjjk",
    ".kJkkkkjjjjjiiik",
    "..kjjjjjjjjjkkk.",
    "..kjiijjjjjjk...",
    "..kjjjjjjjJJk...",
]

HOUND_STAND = HOUND_TOP + [
    "..kJJk...kJJk...",
    "..kJJk...kJJk...",
    "..kkkk...kkkk...",
]

HOUND_WALK = HOUND_TOP + [
    ".kJkkJk..kJkkJk.",
    "kJk..kJ.kJk..kJk",
    "kk....kkkk....kk",
]


def devourer(walk):
    # 소거식귀. 머리가 크고 팔이 무릎까지 내려오는 창백한 거인
    c = Canvas(22, 38)
    if walk:
        c.capsule(8.5, 26, 6.0, 36.2, 2.3, 'F', 'F')
        c.capsule(13.5, 26, 16.0, 36.2, 2.3, 'f', 'F')
    else:
        c.capsule(8.5, 26, 8.0, 36.2, 2.3, 'F', 'F')
        c.capsule(13.5, 26, 14.0, 36.2, 2.3, 'f', 'F')
    arm_swing = 1.2 if walk else 0.0
    c.capsule(4.5, 14, 3.0 - arm_swing, 25, 1.7, 'F', 'F')
    c.ellipse(3.0 - arm_swing, 26.2, 2.0, 2.2, 'F', 'F')
    c.ellipse(11.0, 20.0, 6.6, 8.2, 'f', 'F')
    c.ellipse(12.0, 22.5, 5.4, 4.6, 'f', 'F')
    c.ellipse(11.0, 13.8, 7.6, 3.4, 'f', 'F')
    c.capsule(12.0, 10.0, 12.0, 12.5, 2.2, 'f', 'F')
    c.ellipse(12.8, 6.2, 5.2, 5.6, 'f', 'F')
    c.capsule(17.5, 14, 19.0 + arm_swing, 25, 1.8, 'f', 'F')
    c.ellipse(19.0 + arm_swing, 26.4, 2.1, 2.3, 'f', 'F')
    # 갈비와 배꼽 음영
    for y in (17, 19, 21):
        c.put(9, y, 'F')
        c.put(10, y, 'F')
    c.put(12, 25, 'F')
    # 눈은 뚫린 구멍, 입은 귀밑까지 찢어진 웃음
    c.put(13, 4, 'k')
    c.put(16, 4, 'k')
    for x in range(11, 18):
        c.put(x, 8, 'm')
        c.put(x, 9, 'm')
    for x in range(11, 18, 2):
        c.put(x, 8, 'n')
    for x in range(12, 18, 2):
        c.put(x, 9, 'n')
    c.put(10, 7, 'm')
    c.put(18, 7, 'm')
    c.outline()
    return c.rows()


def gatebreaker(walk):
    # 성문파쇄자. 뼈 갑주를 두른 거식귀. 급소는 등허리
    c = Canvas(34, 54)
    if walk:
        c.capsule(12.0, 38, 9.0, 52.3, 3.6, 'F', 'm')
        c.capsule(21.5, 38, 24.5, 52.3, 3.6, 'F', 'm')
    else:
        c.capsule(12.0, 38, 11.5, 52.3, 3.6, 'F', 'm')
        c.capsule(21.5, 38, 22.0, 52.3, 3.6, 'F', 'm')
    arm_swing = 1.5 if walk else 0.0
    c.capsule(6.0, 18, 3.5 - arm_swing, 35, 3.0, 'F', 'm')
    c.ellipse(4.0 - arm_swing, 38.0, 3.8, 3.6, 'F', 'm')
    c.ellipse(16.5, 28.0, 10.4, 11.2, 'F', 'm')
    c.ellipse(19.0, 23.5, 6.2, 5.2, 'o', 'O')
    c.ellipse(17.0, 33.0, 5.4, 3.0, 'o', 'O')
    c.ellipse(7.0, 17.0, 5.4, 3.6, 'o', 'O')
    c.ellipse(26.0, 17.0, 5.4, 3.6, 'o', 'O')
    c.capsule(18.0, 12.0, 18.0, 15.5, 3.2, 'F', 'm')
    c.ellipse(18.5, 9.0, 6.2, 7.0, 'F', 'm')
    c.capsule(28.0, 18, 30.5 + arm_swing, 35, 3.2, 'F', 'm')
    c.ellipse(30.0 + arm_swing, 38.2, 3.9, 3.8, 'F', 'm')
    # 뼈 뿔
    c.capsule(14.5, 4.0, 12.5, 0.8, 0.8, 'o', 'O')
    c.capsule(21.5, 3.5, 23.5, 0.6, 0.8, 'o', 'O')
    # 불타는 눈과 이빨
    c.put(19, 7, 'e')
    c.put(20, 7, 'e')
    c.put(23, 7, 'e')
    for x in range(16, 25):
        c.put(x, 12, 'k')
        c.put(x, 13, 'm')
    for x in range(16, 25, 2):
        c.put(x, 12, 'n')
        c.put(x + 1, 13, 'n')
    # 상처
    for step in range(4):
        c.put(11 + step, 27 + step, 'R')
    c.outline()
    return c.rows()


# ------------------------------------------------------------ 성벽 블록 ( 칸 하나 = 폭 32 도트 )

def iso_block(face_height, top_key, left_key, right_key):
    width = 32
    height = 16 + face_height
    cells = [['.'] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            if y < 16:
                half = y if y < 8 else 15 - y
                x0 = 14 - 2 * half
                x1 = 17 + 2 * half
                if x0 <= x <= x1:
                    cells[y][x] = top_key(x, y, x0, x1)
                    continue
            if x <= 15:
                v = y - (8 + x // 2) - 1
                if 0 <= v < face_height:
                    cells[y][x] = left_key(x, v)
            else:
                v = y - (8 + (31 - x) // 2) - 1
                if 0 <= v < face_height:
                    cells[y][x] = right_key(x - 16, v)
    return [''.join(row) for row in cells]


def brick(local_x, v, face_height, base, speck, salt, course=5, joint=8):
    if v == face_height - 1:
        return 'k'
    if v % course == course - 1:
        return '4'
    offset = (v // course % 2) * (joint // 2)
    if (local_x + offset) % joint == joint - 1:
        return '4'
    if hash2(local_x, v, salt) > 0.86:
        return speck
    return base


def top_stone(x, y, x0, x1, base, speck, edge):
    if x == x0 or x == x1 or y == 0 or y == 15:
        return edge
    if hash2(x, y, 11) > 0.8:
        return speck
    return base


def wall_block():
    face = 26

    def left(x, v):
        if x == 0:
            return 'k'
        return brick(x, v, face, '3', '5', 3)

    def right(x, v):
        if x == 15:
            return 'k'
        return brick(x, v, face, '2', '1', 5)

    return iso_block(face, lambda x, y, x0, x1: top_stone(x, y, x0, x1, '2', '1', '3'), left, right)


def tower_block():
    face = 40

    def slit(x, v):
        return 7 <= x <= 8 and 8 <= v <= 15

    def left(x, v):
        if x == 0:
            return 'k'
        if slit(x, v):
            return 'k'
        return brick(x, v, face, '3', '4', 7, course=6)

    def right(x, v):
        if x == 15:
            return 'k'
        if slit(x, v):
            return 'k'
        # 개척대 깃발 띠
        if 20 <= v <= 27 and 3 <= x <= 12:
            if v == 27 and x % 2 == 0:
                return '.'
            return 't' if x > 4 else 'T'
        return brick(x, v, face, '2', '1', 9, course=6)

    def top(x, y, x0, x1):
        # 흉벽 안쪽이 한 단 낮아 보이게 테두리를 밝게 두른다
        if x == x0 or x == x1 or y == 0 or y == 15:
            return 'k'
        if x <= x0 + 2 or x >= x1 - 2 or y <= 1 or y >= 14:
            return '1'
        return '3'

    return iso_block(face, top, left, right)


def house_block(variant):
    face = 16

    def window(x, v):
        if 4 <= x <= 9 and 3 <= v <= 8:
            if x in (4, 9) or v in (3, 8):
                return 'W'
            if x in (6, 7) or v == 5:
                return 'W'
            return 'H'
        return None

    def door(x, v):
        if 6 <= x <= 10 and 6 <= v <= face - 2:
            if x in (6, 10) or v == 6:
                return 'k'
            return 'W' if (x + v) % 3 else 'B'
        return None

    def left(x, v):
        if x == 0 or v == face - 1:
            return 'k'
        if v >= face - 3:
            return '3'
        if x in (1, 14, 15) or v == 0:
            return 'W'
        mark = window(x, v)
        if mark is not None and variant == 1:
            return mark
        return '9'

    def right(x, v):
        if x == 15 or v == face - 1:
            return 'k'
        if v >= face - 3:
            return '2'
        if x in (0, 1, 14) or v == 0:
            return 'W'
        if variant == 0:
            mark = door(x, v)
        else:
            mark = window(x, v)
        if mark is not None:
            return mark
        return '8'

    def top(x, y, x0, x1):
        if x == x0 or x == x1 or y == 0 or y == 15:
            return 'k'
        if (x // 2 + y) % 3 == 0:
            return '7'
        return '6'

    return iso_block(face, top, left, right)


def ruin_block(salt):
    face = 11

    def jag(x):
        return int(hash2(x // 2, 0, salt) * 5)

    def left(x, v):
        if v < jag(x):
            return '.'
        if v == face - 1 or x == 0:
            return 'k'
        if hash2(x, v, salt + 3) > 0.72:
            return 'l'
        return brick(x, v, face, '3', '4', salt)

    def right(x, v):
        if v < jag(x + 16):
            return '.'
        if v == face - 1 or x == 15:
            return 'k'
        if hash2(x, v, salt + 5) > 0.78:
            return 'l'
        return brick(x, v, face, '2', '1', salt + 1)

    def top(x, y, x0, x1):
        # 무너진 윗면은 돌무더기라 군데군데 비어 바닥이 보인다
        if hash2(x // 2, y // 2, salt + 9) > 0.62:
            return '.'
        if hash2(x, y, salt + 11) > 0.7:
            return 'l'
        return '3' if hash2(x, y, salt + 13) > 0.5 else '2'

    return iso_block(face, top, left, right)


# ------------------------------------------------------------ 소품

def dead_tree(variant):
    c = Canvas(21, 32)
    c.capsule(10.0, 31.5, 10.5, 13.0, 1.7, 'N', 'X')
    c.capsule(10.0, 31.0, 6.5, 31.6, 0.9, 'X')
    c.capsule(10.5, 31.0, 14.0, 31.6, 0.9, 'N')
    if variant == 0:
        branches = [
            (10.3, 21.0, 4.5, 13.5, 0.95), (10.5, 17.0, 16.5, 10.0, 0.95),
            (10.5, 14.0, 7.5, 5.5, 0.8), (10.6, 13.5, 13.5, 3.5, 0.75),
            (4.5, 13.5, 1.8, 9.0, 0.5), (4.5, 13.5, 6.0, 8.0, 0.5),
            (16.5, 10.0, 19.5, 6.5, 0.5), (16.5, 10.0, 15.5, 5.0, 0.5),
            (7.5, 5.5, 5.5, 1.5, 0.45), (13.5, 3.5, 15.5, 0.5, 0.45),
        ]
    else:
        branches = [
            (10.3, 23.0, 3.0, 17.0, 0.95), (10.5, 19.0, 17.5, 14.0, 0.95),
            (10.5, 14.0, 5.5, 7.0, 0.85), (10.6, 13.5, 15.5, 5.0, 0.8),
            (3.0, 17.0, 1.0, 12.0, 0.5), (17.5, 14.0, 19.8, 10.5, 0.5),
            (5.5, 7.0, 3.0, 3.0, 0.45), (5.5, 7.0, 8.0, 2.5, 0.45),
            (15.5, 5.0, 13.5, 1.0, 0.45), (15.5, 5.0, 19.0, 2.0, 0.45),
        ]
    for x0, y0, x1, y1, radius in branches:
        c.capsule(x0, y0, x1, y1, radius, 'N', 'X')
    c.outline()
    return c.rows()


def tent(light, shade, flap, flap_shade):
    c = Canvas(30, 20)
    for y in range(19):
        half = (y + 1) / 19.0 * 15.0
        for x in range(30):
            px = x + 0.5
            if abs(px - 15.0) <= half:
                c.put(x, y, light if px >= 15.0 else shade)
    # 입구 삼각형
    for y in range(8, 19):
        half = (y - 7) / 11.0 * 5.5
        for x in range(30):
            px = x + 0.5
            if abs(px - 15.0) <= half:
                edge = abs(px - 15.0) > half - 1.0
                c.put(x, y, 'k' if edge else (flap if px >= 15.0 else flap_shade))
    for x in range(30):
        c.put(x, 19, 'k')
    c.outline()
    return c.rows()


def stall():
    c = Canvas(28, 24)
    # 기둥
    for y in range(6, 23):
        c.put(2, y, 'W')
        c.put(25, y, 'W')
    # 줄무늬 차양
    for y in range(0, 7):
        inset = 6 - y
        for x in range(inset // 2, 28 - inset // 2):
            stripe = (x // 3) % 2
            key = 'r' if stripe == 0 else 'c'
            if y == 6:
                key = 'R' if stripe == 0 else 'C'
            c.put(x, y, key)
    # 좌판과 물건
    for y in range(15, 23):
        for x in range(1, 27):
            c.put(x, y, 'w' if y < 17 else 'W')
    for x in range(4, 24):
        roll = hash2(x, 3, 41)
        if roll < 0.3:
            c.put(x, 14, 'y')
        elif roll < 0.55:
            c.put(x, 14, 'g')
            c.put(x, 13, 'g')
        elif roll < 0.75:
            c.put(x, 14, 'b')
            c.put(x, 13, 'B')
    c.outline()
    return c.rows()


SWORD = [
    "..a..",
    ".aAa.",
    ".aAa.",
    ".aAa.",
    ".aAa.",
    ".aAa.",
    ".aAa.",
    ".aAa.",
    ".aAa.",
    "YyYyY",
    "..W..",
    "..W..",
    "..y..",
]

GOLD = [
    "..yy..",
    ".yYYy.",
    "yYyyYy",
    "yyYYyy",
    ".YYYY.",
]

EXP_ORB = [
    ".LEL.",
    "LEEEL",
    "EE#EE",
    "LEEEL",
    ".LEL.",
]

DUST = [
    "..v..",
    ".vVv.",
    "vV#Vv",
    ".vVv.",
    "..v..",
]

FRAGMENT = [
    "..q..",
    ".qQq.",
    "qQ#Qq",
    ".qQq.",
    "..q..",
]

CORE = [
    "..z..",
    ".zZz.",
    "zZ#Zz",
    ".zZz.",
    "..z..",
]

ROCK = [
    "....kkkk...",
    "..kk2211k..",
    ".k3322111k.",
    "k333222211k",
    "k333322221k",
    "k533332222k",
    ".kkkkkkkkk.",
]

PEBBLE = [
    ".kkkk..",
    "k3221k.",
    "k33221k",
    "k53322k",
    ".kkkkk.",
]

THORN = [
    "..0...D..0.",
    ".0D..0D.0D.",
    "..0D.D00D..",
    ".D.0DD0D.0.",
    "..D0000D0..",
    ".D00X0X00D.",
    "..0XXXXX0..",
]

GRASS = [
    ".D..D..",
    "..D0.D.",
    ".0D0D..",
    "..0X0..",
    ".0XXX0.",
]

REEDS = [
    ".g...g.",
    ".G.g.G.",
    ".g.G.g.",
    "gG.g.Gg",
    "Gg.G.gG",
    ".G.g.G.",
    ".g.G.g.",
    "gG.g.Gg",
    ".GgGgG.",
    ".gGgGg.",
    "GGgGgGG",
    ".GGGGG.",
]

BRAZIER = [
    "...H...",
    "..HIH..",
    ".IHHII.",
    ".IIHIM.",
    "kYyyyYk",
    ".kYYYk.",
    "..kdk..",
    "..kdk..",
    ".kdddk.",
    "kkkkkkk",
]

BONES = [
    "o.....o",
    ".oOOOo.",
    "o.....o",
]

BANNER = [
    ".kk......",
    "kakkkkkkk",
    "kWkTttttk",
    "kWkToootk",
    "kWkToTotk",
    "kWkToTotk",
    "kWkTtttTk",
    "kWkTttTTk",
    "kWkTTkTTk",
    "kWkTk.kTk",
    "kWkk...kk",
] + [".kWk....."] * 13 + [
    "kWWWk....",
    "kkkkk....",
]

TOTEM = [
    "..kkkk...",
    ".kooook..",
    ".kokoko..",
    ".koooOk..",
    "..kOnOk..",
    "...kWk...",
    ".rkrWkr..",
    "rRkrWkRr.",
    ".r.kWk.r.",
] + ["...kWk..."] * 11 + [
    ".kkWWWkk.",
    "kBBBBBBBk",
]

GRAVE = [
    "...k...",
    "..kwk..",
    "..kwk..",
    "kkkwkkk",
    "kwwwwwk",
    "kkkwkkk",
    "..kwt..",
    "..kwTt.",
    "..kwkT.",
    ".kBBBk.",
    "kBBBBBk",
]

POUCH = [
    "..kk..",
    ".kbbk.",
    "kbyybk",
    "kbBbBk",
    ".kkkk.",
]

HERB = [
    "...h...",
    ".h.g.h.",
    "..hgh..",
    ".gGgGg.",
    "..gGg..",
    "...G...",
]

ARROW = [
    ".a.",
    "aAa",
    ".W.",
    ".W.",
    ".W.",
    ".W.",
    ".W.",
    "cWc",
    "c.c",
]

CROPS_GREEN = [
    ".g...g...",
    "gGg.gGg.g",
    ".G...G...",
]

CROPS_DEAD = [
    ".D...0...",
    "0D0.D0D.0",
    ".0...X...",
]

LAMP = [
    ".kkk.",
    "kHHHk",
    "kHIHk",
    ".kdk.",
] + [".kdk."] * 13 + [
    ".kddk",
    "kkkkk",
]


# ------------------------------------------------------------ 목록 ( 순서 = SpriteId.h )

def build_sprites():
    return [
        ('SPRITE_PLAYER', humanoid(PLAYER_TOP, 'x', 'B')),
        ('SPRITE_BLIGHTED', [BLIGHTED_STAND, BLIGHTED_WALK]),
        ('SPRITE_HOUND', [HOUND_STAND, HOUND_WALK]),
        ('SPRITE_DEVOURER', [devourer(False), devourer(True)]),
        ('SPRITE_GATEBREAKER', [gatebreaker(False), gatebreaker(True)]),
        ('SPRITE_SWORD', [SWORD]),
        ('SPRITE_GOLD', [GOLD]),
        ('SPRITE_EXP_ORB', [EXP_ORB]),
        ('SPRITE_DUST', [DUST]),
        ('SPRITE_FRAGMENT', [FRAGMENT]),
        ('SPRITE_CORE', [CORE]),
        ('SPRITE_DEAD_TREE', [dead_tree(0), dead_tree(1)]),
        ('SPRITE_ROCK', [ROCK]),
        ('SPRITE_PEBBLE', [PEBBLE]),
        ('SPRITE_THORN', [THORN]),
        ('SPRITE_GRASS', [GRASS]),
        ('SPRITE_REEDS', [REEDS]),
        ('SPRITE_BRAZIER', [BRAZIER]),
        ('SPRITE_BONES', [BONES]),
        ('SPRITE_NPC_HUNTER', humanoid(HUNTER_TOP, 'B', 'K')),
        ('SPRITE_NPC_RAIDER', humanoid(RAIDER_TOP, 'x', 'K')),
        ('SPRITE_NPC_MERCHANT', [MERCHANT_TOP + MERCHANT_LEGS_STAND, MERCHANT_TOP + MERCHANT_LEGS_WALK]),
        ('SPRITE_NPC_GATHERER', humanoid(GATHERER_TOP, 'C', 'B')),
        ('SPRITE_NPC_WARDEN', [with_tail(WARDEN_TOP + legs('x', 'B', False), 'W'), with_tail(WARDEN_TOP + legs('x', 'B', True), 'W')]),
        ('SPRITE_BLOCK_WALL', [wall_block()]),
        ('SPRITE_BLOCK_TOWER', [tower_block()]),
        ('SPRITE_BLOCK_HOUSE', [house_block(0), house_block(1)]),
        ('SPRITE_BLOCK_RUIN', [ruin_block(17), ruin_block(29)]),
        ('SPRITE_TENT', [tent('c', 'C', 't', 'T'), tent('b', 'B', 'r', 'R')]),
        ('SPRITE_BANNER', [BANNER]),
        ('SPRITE_TOTEM', [TOTEM]),
        ('SPRITE_GRAVE', [GRAVE]),
        ('SPRITE_POUCH', [POUCH]),
        ('SPRITE_HERB', [HERB]),
        ('SPRITE_ARROW', [ARROW]),
        ('SPRITE_STALL', [stall()]),
        ('SPRITE_CROPS', [CROPS_GREEN, CROPS_DEAD]),
        ('SPRITE_LAMP', [LAMP]),
    ]


def validate(sprites, keys):
    errors = 0
    for name, frames in sprites:
        width = len(frames[0][0])
        height = len(frames[0])
        for fi, frame in enumerate(frames):
            if len(frame) != height:
                print('HEIGHT', name, fi, len(frame), height)
                errors += 1
            for ri, row in enumerate(frame):
                if len(row) != width:
                    print('WIDTH', name, fi, ri, repr(row), len(row), width)
                    errors += 1
                for ch in row:
                    if ch not in keys:
                        print('KEY', name, fi, ri, ch)
                        errors += 1
        print(name, width, 'x', height, 'frames', len(frames))
    return errors


def preview(sprites, path):
    from PIL import Image, ImageDraw
    colors = {k: rgb for k, rgb in PALETTE}
    scale = 4
    pad = 6
    cursor_x = pad
    cursor_y = pad
    shelf = 0
    sheet_width = 1400
    placements = []
    for name, frames in sprites:
        for fi, frame in enumerate(frames):
            w = len(frame[0]) * scale
            h = len(frame) * scale
            if cursor_x + w + pad > sheet_width:
                cursor_x = pad
                cursor_y += shelf + pad + 12
                shelf = 0
            placements.append((name, fi, frame, cursor_x, cursor_y))
            cursor_x += w + pad
            shelf = max(shelf, h)
    sheet_height = cursor_y + shelf + pad + 12
    image = Image.new('RGB', (sheet_width, sheet_height), (46, 42, 38))
    draw = ImageDraw.Draw(image)
    for name, fi, frame, ox, oy in placements:
        for ry, row in enumerate(frame):
            for rx, ch in enumerate(row):
                if ch == '.':
                    continue
                x = ox + rx * scale
                y = oy + ry * scale
                draw.rectangle([x, y, x + scale - 1, y + scale - 1], fill=colors[ch])
        draw.text((ox, oy + len(frame) * scale + 1), name.replace('SPRITE_', '')[:10] + str(fi), fill=(200, 200, 200))
    image.save(path)
    print('preview', path)


def write_cpp(sprites, used):
    out = []
    out.append('#include "stdafx.h"')
    out.append('#include "SpriteArt.h"')
    out.append('#include "SpriteId.h"')
    out.append('')
    out.append('#include <cstring>')
    out.append('')
    out.append('namespace')
    out.append('{')
    out.append('\tstd::array<float, 4> Rgb(int32 r, int32 g, int32 b)')
    out.append('\t{')
    out.append('\t\treturn { static_cast<float>(r) / 255.f, static_cast<float>(g) / 255.f, static_cast<float>(b) / 255.f, 1.f };')
    out.append('\t}')
    out.append('')
    out.append('\tvoid SetArt(std::vector<SpriteArt>* arts, int32 sprite, const std::vector<std::vector<const char*>>& frames)')
    out.append('\t{')
    out.append('\t\tSpriteArt& art = (*arts)[static_cast<size_t>(sprite)];')
    out.append('\t\tart.frames = frames;')
    out.append('\t\tart.height = static_cast<int32>(frames.front().size());')
    out.append('\t\tart.width = static_cast<int32>(std::strlen(frames.front().front()));')
    out.append('\t}')
    out.append('}')
    out.append('')
    out.append('std::array<float, 4> GetPaletteColor(char key)')
    out.append('{')
    out.append('\tswitch (key)')
    out.append('\t{')
    for k, (r, g, b) in PALETTE:
        if k not in used:
            continue
        out.append("\t\tcase '%s':" % k)
        out.append('\t\t{')
        out.append('\t\t\treturn Rgb(%d, %d, %d);' % (r, g, b))
        out.append('\t\t}')
    out.append('\t\tdefault:')
    out.append('\t\t{')
    out.append('\t\t\treturn { 0.f, 0.f, 0.f, 0.f };')
    out.append('\t\t}')
    out.append('\t}')
    out.append('}')
    out.append('')
    out.append('std::vector<SpriteArt> MakeSpriteArts()')
    out.append('{')
    out.append('\tstd::vector<SpriteArt> arts(static_cast<size_t>(SPRITE_COUNT));')
    for name, frames in sprites:
        out.append('')
        out.append('\tSetArt(&arts, %s,' % name)
        out.append('\t\t{')
        for frame in frames:
            out.append('\t\t\t{')
            for row in frame:
                out.append('\t\t\t\t"%s",' % row)
            out.append('\t\t\t},')
        out.append('\t\t});')
    out.append('')
    out.append('\treturn arts;')
    out.append('}')

    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SimpleGame', 'SpriteArt.cpp')
    with open(path, 'w', encoding='utf-8-sig', newline='\n') as f:
        f.write('\n'.join(out) + '\n')
    print('written', len(out), 'lines')


def main():
    sprites = build_sprites()
    keys = {k for k, _ in PALETTE} | {'.'}
    if len(keys) != len(PALETTE) + 1:
        print('duplicate palette key')
        return
    errors = validate(sprites, keys)

    used = set()
    for _, frames in sprites:
        for frame in frames:
            for row in frame:
                used |= set(row)
    used.discard('.')
    print('unused palette:', sorted(keys - used - {'.'}))
    print('errors', errors)
    if errors:
        return

    if len(sys.argv) >= 3 and sys.argv[1] == '--preview':
        preview(sprites, sys.argv[2])
        return

    write_cpp(sprites, used)


main()
