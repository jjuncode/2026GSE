# -*- coding: utf-8 -*-
# 캐릭터 말고 나머지 전부 - 성벽 · 망루 · 집 · 폐허 블록, 천막 · 노점 · 가로등 · 모루, 나무 · 바위 · 풀, 떨어진 물건과 가방 아이콘.
# 크기는 옛 도트 그림과 같게 맞춘다 ( 캐릭터 비율을 거기에 맞춰 놨다 ). 1 칸 = 1.5 m, 배율 1 이 게임 크기
import math

import numpy as np

from pixel3d import Box, Capsule, Convex, Ellipsoid, Material, basis_from_axis, normalize, pitch_matrix, roll_matrix, yaw_matrix
from rig import Builder, vec

I3 = np.eye(3)


# ------------------------------------------------------------ 면 무늬

def hash_cells(cells):
    v = np.sin(cells @ np.array([12.9898, 78.233, 37.719])) * 43758.5453
    return v - np.floor(v)


def face_uv(p, n):
    # 위 · x 면 · y 면 따로 평면 좌표를 잡는다
    an = np.abs(n)
    top = an[:, 2] > 0.7
    x_face = (~top) & (an[:, 0] >= an[:, 1])
    u = np.where(top, p[:, 0], np.where(x_face, p[:, 1], p[:, 0]))
    v = np.where(top, p[:, 1], p[:, 2])
    face = np.where(top, 2.0, np.where(x_face, 0.0, 1.0))
    return u, v, face


def bricks(row_h, brick_w, mortar_v=0.14, mortar_u=0.08, dark=0.18, light=0.85):
    def pattern(p, n):
        u, v, face = face_uv(p, n)
        row = np.floor(v / row_h)
        uu = u + np.mod(row, 2.0) * brick_w * 0.5
        col = np.floor(uu / brick_w)
        fv = v / row_h - row
        fu = uu / brick_w - col
        mortar = (fv < mortar_v) | (fu < mortar_u)
        h = hash_cells(np.stack([row, col, face], axis=1))
        out = np.where(h < dark, -1, np.where(h > light, 1, 0))
        return np.where(mortar, -1, out).astype(int)

    return pattern


def tiles(row_h):
    # 기와 - 높이로 줄을 긋고 줄마다 어긋난 기와 한 장씩
    def pattern(p, n):
        row = np.floor(p[:, 2] / row_h)
        across = p[:, 0] - p[:, 1]
        col = np.floor(across / 0.22 + np.mod(row, 2.0) * 0.5)
        frac = p[:, 2] / row_h - row
        h = hash_cells(np.stack([row, col, np.zeros_like(row)], axis=1))
        out = np.where(h < 0.15, -1, np.where(h > 0.88, 1, 0))
        return np.where(frac < 0.25, -1, out).astype(int)

    return pattern


def planks(width, along_z=False):
    def pattern(p, n):
        u, v, face = face_uv(p, n)
        coord = u
        if along_z:
            coord = v
        idx = np.floor(coord / width)
        frac = coord / width - idx
        h = hash_cells(np.stack([idx, face, np.zeros_like(idx)], axis=1))
        out = np.where(h < 0.3, -1, 0)
        return np.where(frac < 0.15, -1, out).astype(int)

    return pattern


def speckle(cell, dark=0.12, light=0.9):
    def pattern(p, n):
        cells = np.floor(p / cell)
        h = hash_cells(cells)
        return np.where(h < dark, -1, np.where(h > light, 1, 0)).astype(int)

    return pattern


def bark():
    # 세로 결
    def pattern(p, n):
        ang = np.arctan2(p[:, 1], p[:, 0])
        idx = np.floor(ang * 5.0 + p[:, 2] * 0.8)
        h = hash_cells(np.stack([idx, np.floor(p[:, 2] / 0.3), np.zeros_like(idx)], axis=1))
        return np.where(h < 0.35, -1, 0).astype(int)

    return pattern


# ------------------------------------------------------------ 재질 ( 옛 팔레트에서 )

MATS = {
    'stone_wall': Material([(48, 44, 44), (80, 76, 74), (108, 104, 98), (140, 134, 126)], outline=(26, 22, 24), pattern=bricks(0.25, 0.5)),
    'stone_top': Material([(56, 52, 52), (88, 84, 80), (118, 112, 106), (146, 140, 132)], outline=(26, 22, 24), pattern=bricks(0.3, 0.3, 0.12, 0.12)),
    'stone': Material([(40, 36, 36), (72, 68, 66), (104, 98, 92), (136, 130, 120)], outline=(20, 18, 20), pattern=speckle(0.07)),
    'stone_rock': Material([(34, 32, 34), (56, 52, 52), (82, 78, 74), (108, 102, 96)], outline=(16, 14, 16), pattern=speckle(0.06, 0.1, 0.9)),
    'stone_dark': Material([(30, 28, 30), (52, 48, 48), (76, 72, 70)], outline=(14, 12, 14)),
    'moss': Material([(42, 52, 32), (60, 74, 44), (78, 94, 60), (100, 118, 74)], outline=(20, 26, 14), pattern=speckle(0.06, 0.25, 0.8)),
    'plaster': Material([(84, 72, 58), (114, 98, 80), (158, 140, 112), (186, 170, 140)], outline=(40, 32, 24), pattern=speckle(0.08, 0.08, 0.95)),
    'roof': Material([(56, 30, 26), (84, 44, 36), (112, 60, 46), (140, 80, 60)], outline=(30, 14, 12), pattern=tiles(0.1)),
    'timber': Material([(40, 28, 22), (64, 46, 32), (88, 64, 44)], outline=(20, 12, 10)),
    'wood': Material([(56, 40, 28), (80, 60, 42), (110, 84, 56), (138, 108, 72)], outline=(28, 18, 12), pattern=planks(0.12, True)),
    'grave_wood': Material([(90, 70, 50), (130, 104, 74), (170, 140, 100)], outline=(36, 26, 18)),
    'wood_post': Material([(50, 36, 26), (80, 60, 42), (110, 84, 56)], outline=(24, 16, 10)),
    'door': Material([(36, 24, 18), (58, 40, 28), (80, 58, 40)], outline=(18, 10, 8), pattern=planks(0.1)),
    'window': Material([(255, 196, 96), (255, 232, 160)], outline=(90, 50, 20), emissive=True),
    'dark_hole': Material([(14, 12, 14), (24, 20, 22)], outline=(8, 6, 8), flat=True),
    'teal': Material([(20, 48, 54), (30, 78, 78), (46, 112, 104), (80, 152, 134)], outline=(12, 22, 26)),
    'red': Material([(70, 18, 18), (110, 30, 28), (150, 40, 36), (180, 64, 52)], outline=(34, 8, 8)),
    'gold': Material([(108, 70, 30), (168, 120, 48), (222, 180, 84), (250, 226, 150)], outline=(50, 30, 14), metal=True),
    'canvas': Material([(92, 86, 72), (124, 116, 98), (160, 152, 136), (186, 178, 160)], outline=(40, 36, 30)),
    'canvas_dark': Material([(52, 38, 30), (76, 56, 42), (104, 78, 58), (128, 98, 72)], outline=(24, 16, 12)),
    'awning_red': Material([(96, 22, 22), (140, 34, 32), (176, 52, 46)], outline=(44, 10, 10)),
    'awning_white': Material([(150, 142, 128), (196, 190, 176), (226, 222, 210)], outline=(60, 56, 50)),
    'iron': Material([(28, 28, 34), (48, 48, 56), (74, 76, 86), (104, 108, 120)], outline=(12, 12, 16), metal=True),
    'anvil_iron': Material([(52, 54, 62), (82, 86, 98), (120, 126, 140), (164, 170, 186)], outline=(18, 18, 24), metal=True),
    'steel': Material([(52, 56, 70), (96, 104, 120), (152, 162, 178), (204, 212, 224)], outline=(22, 24, 32), metal=True),
    'fire': Material([(255, 122, 42), (255, 206, 96)], outline=(140, 40, 14), emissive=True),
    'fire_core': Material([(255, 236, 170), (255, 250, 220)], outline=(180, 90, 30), emissive=True),
    'lamp_glow': Material([(255, 210, 120), (255, 240, 190)], outline=(120, 70, 20), emissive=True),
    'bark': Material([(34, 30, 28), (48, 42, 40), (68, 60, 54), (84, 72, 64)], outline=(16, 14, 14), pattern=bark()),
    'bone': Material([(120, 112, 96), (150, 142, 124), (196, 188, 168), (222, 214, 196)], outline=(56, 50, 40)),
    'dry_grass': Material([(64, 56, 38), (86, 76, 54), (128, 114, 76), (160, 146, 100)], outline=(34, 28, 18)),
    'green': Material([(36, 50, 30), (58, 70, 46), (82, 98, 62), (110, 128, 80)], outline=(18, 24, 14)),
    'leaf': Material([(30, 70, 40), (50, 104, 56), (84, 150, 84), (130, 200, 120)], outline=(14, 32, 18)),
    'herb_glow': Material([(140, 236, 160), (210, 255, 220)], outline=(30, 80, 40), emissive=True),
    'dirt': Material([(44, 34, 26), (64, 50, 38), (86, 68, 50)], outline=(22, 16, 12), pattern=speckle(0.05)),
    'leather': Material([(54, 36, 30), (88, 60, 42), (124, 88, 58), (164, 122, 82)], outline=(26, 16, 14)),
    'cloth_dark': Material([(28, 26, 34), (44, 42, 50), (64, 60, 66)], outline=(14, 12, 18)),
    'grip': Material([(40, 28, 24), (70, 48, 36)], outline=(18, 12, 10)),
    'blade': Material([(110, 118, 136), (168, 178, 196), (214, 222, 236)], outline=(30, 34, 46), metal=True),
    'rust': Material([(62, 36, 26), (98, 58, 38), (140, 86, 54), (176, 120, 80)], outline=(30, 16, 12), metal=True),
    'trace': Material([(66, 38, 112), (124, 80, 206), (170, 120, 240), (220, 190, 255)], outline=(34, 16, 62), metal=True),
    'trace_glow': Material([(200, 156, 255), (240, 220, 255)], outline=(66, 38, 112), emissive=True),
    'glass_red': Material([(110, 16, 28), (160, 24, 38), (226, 52, 60), (250, 120, 120)], outline=(60, 8, 14), metal=True),
    'glass': Material([(70, 98, 108), (126, 166, 176), (190, 220, 226)], outline=(36, 50, 56), metal=True),
    'cork': Material([(90, 64, 40), (130, 96, 60)], outline=(40, 26, 14)),
    'fruit': Material([(110, 40, 30), (170, 70, 40), (220, 120, 60)], outline=(50, 16, 10)),
    'herb_pile': Material([(40, 80, 44), (70, 120, 64), (110, 160, 90)], outline=(18, 34, 18)),
    'exp': Material([(60, 190, 110), (140, 255, 170)], outline=(20, 80, 40), emissive=True),
}


# ------------------------------------------------------------ 블록 ( 칸 하나짜리 입체 )

HALF = 0.75  # 칸 반 폭 ( m )


def block_wall():
    b = Builder(0.0)
    height = 2.0
    b.box((0.0, 0.0, height * 0.5 - 0.03), (HALF, HALF, height * 0.5 - 0.03), I3, 0.0, 'stone_wall')
    b.box((0.0, 0.0, height - 0.03), (HALF, HALF, 0.03), I3, 0.0, 'stone_top')
    return b


def block_tower():
    b = Builder(0.0)
    height = 3.0
    b.box((0.0, 0.0, height * 0.5), (HALF, HALF, height * 0.5), I3, 0.0, 'stone_wall')
    # 성가퀴. 모서리 넷만 솟는다 ( 가운데까지 세우면 위에서 꽃무늬로 보였다 )
    for x in (-0.52, 0.52):
        for y in (-0.52, 0.52):
            b.box((x, y, height + 0.2), (0.23, 0.23, 0.2), I3, 0.0, 'stone_wall')
    # 화살 구멍 둘과 개척대 깃발
    b.box((HALF + 0.005, 0.25, 1.9), (0.01, 0.04, 0.3), I3, 0.0, 'dark_hole')
    b.box((-0.25, HALF + 0.005, 1.9), (0.04, 0.01, 0.3), I3, 0.0, 'dark_hole')
    b.box((0.22, HALF + 0.02, 2.2), (0.2, 0.02, 0.45), I3, 0.0, 'teal')
    b.capsule((0.0, HALF + 0.04, 2.66), (0.44, HALF + 0.04, 2.66), 0.025, 0.025, 'wood_post')
    b.decal((0.22, HALF + 0.05, 2.3), (222, 180, 84))
    b.decal((0.22, HALF + 0.05, 2.2), (222, 180, 84))
    return b


def plane_through(normal, point):
    n = normalize(np.array(normal, dtype=float))
    return n, float(np.dot(n, np.array(point, dtype=float)))


def hip_roof(b, z0, half, rise, material):
    hr = rise
    a = half
    planes = []
    for nx, ny in ((1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0)):
        n = np.array([nx * hr, ny * hr, a])
        length = np.linalg.norm(n)
        planes.append((n / length, hr * a / length))
        planes.append(((nx, ny, 0.0), a))
    planes.append(((0.0, 0.0, -1.0), 0.07))
    b.prims.append(Convex(b.at((0.0, 0.0, z0)), b.rot, planes, a * 1.5, material))


def block_house(variant):
    def build():
        b = Builder(0.0)
        wall_h = 0.95
        b.box((0.0, 0.0, wall_h * 0.5), (HALF - 0.02, HALF - 0.02, wall_h * 0.5), I3, 0.0, 'plaster')
        # 모서리 기둥과 허리 들보
        for x, y in ((HALF - 0.02, HALF - 0.02), (HALF - 0.02, -HALF + 0.02), (-HALF + 0.02, HALF - 0.02)):
            b.box((x, y, wall_h * 0.5), (0.05, 0.05, wall_h * 0.5), I3, 0.0, 'timber')
        b.box((0.0, HALF - 0.02, wall_h - 0.05), (HALF, 0.03, 0.04), I3, 0.0, 'timber')
        b.box((HALF - 0.02, 0.0, wall_h - 0.05), (0.03, HALF, 0.04), I3, 0.0, 'timber')
        hip_roof(b, wall_h, HALF + 0.07, 0.5, 'roof')
        if variant == 0:
            b.box((-0.15, HALF, 0.36), (0.18, 0.02, 0.36), I3, 0.0, 'door')
            b.decal((-0.05, HALF + 0.03, 0.34), (222, 180, 84))
            b.box((HALF, 0.2, 0.52), (0.02, 0.12, 0.13), I3, 0.0, 'window')
        else:
            b.box((HALF, -0.25, 0.52), (0.02, 0.12, 0.13), I3, 0.0, 'window')
            b.box((HALF, 0.28, 0.52), (0.02, 0.12, 0.13), I3, 0.0, 'window')
            b.box((-0.25, HALF, 0.52), (0.12, 0.02, 0.13), I3, 0.0, 'window')
            b.box((0.28, HALF, 0.52), (0.12, 0.02, 0.13), I3, 0.0, 'window')
        return b

    return build


def block_ruin(variant):
    def build():
        b = Builder(0.0)
        rng = np.random.default_rng(31 + variant)
        # 무너진 담 - 길이와 높이가 제각각인 돌덩이를 칸 가장자리에 세운다
        if variant == 0:
            pieces = [((-0.45, 0.55), (0.3, 0.2), 0.9), ((0.1, 0.55), (0.25, 0.2), 0.55), ((0.55, 0.5), (0.2, 0.25), 0.3),
                      ((-0.55, 0.0), (0.2, 0.3), 0.7), ((-0.55, -0.5), (0.2, 0.25), 0.4)]
        else:
            pieces = [((0.55, -0.45), (0.2, 0.3), 0.8), ((0.55, 0.15), (0.2, 0.25), 0.45), ((0.0, 0.58), (0.3, 0.17), 0.35),
                      ((-0.5, 0.58), (0.25, 0.17), 0.65)]
        for (x, y), (hx, hy), h in pieces:
            b.box((x, y, h * 0.5), (hx, hy, h * 0.5), I3, 0.0, 'stone_wall')
            b.ellipsoid((x, y, h), (hx * 0.9, hy * 0.9, 0.05), I3, 'moss')
        # 무너져 내린 돌
        for k in range(6):
            x, y = rng.uniform(-0.55, 0.55, 2)
            r = rng.uniform(0.08, 0.15)
            b.ellipsoid((x, y, r * 0.5), (r, r * 0.8, r * 0.6), yaw_matrix(rng.uniform(0, 180)), 'stone')
        b.ellipsoid((0.0, 0.0, 0.0), (0.6, 0.6, 0.04), I3, 'moss')
        return b

    return build


# ------------------------------------------------------------ 자연물

def dead_tree(seed):
    # 줄기 하나에서 큰 가지 셋이 갈라지고, 가지마다 잔가지 둘. 앙상한 윤곽이 읽혀야 죽은 숲이다
    def build():
        b = Builder(seed * 47.0)
        rng = np.random.default_rng(seed + 7)
        lean = normalize(vec(rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), 1.0))
        trunk_top = lean * 1.45
        b.capsule((0.0, 0.0, 0.0), trunk_top, 0.14, 0.07, 'bark')
        for k in range(4):
            a = math.radians(seed * 50 + k * 90 + rng.uniform(-25, 25))
            tilt = math.radians(rng.uniform(38, 62))
            d = normalize(vec(math.cos(a) * math.sin(tilt), math.sin(a) * math.sin(tilt), math.cos(tilt)))
            start = lean * rng.uniform(0.7, 1.35)
            length = rng.uniform(0.7, 1.0)
            end = start + d * length
            b.capsule(start, end, 0.065, 0.03, 'bark', 1)
            for j in range(2):
                a2 = a + math.radians(rng.uniform(-60, 60))
                tilt2 = math.radians(rng.uniform(15, 45))
                d2 = normalize(vec(math.cos(a2) * math.sin(tilt2), math.sin(a2) * math.sin(tilt2), math.cos(tilt2)))
                twig_start = start + d * length * rng.uniform(0.45, 0.9)
                b.capsule(twig_start, twig_start + d2 * rng.uniform(0.3, 0.5), 0.03, 0.012, 'bark', 1)
        # 뿌리
        for k in range(3):
            a = math.radians(k * 120 + rng.uniform(0, 40))
            b.capsule((0.0, 0.0, 0.12), (math.cos(a) * 0.32, math.sin(a) * 0.32, 0.0), 0.08, 0.03, 'bark')
        return b

    return build


def faceted(b, center, radius, height, seed, material):
    # 무작위 평면 여럿으로 깎은 돌덩이. 매끈한 타원은 알처럼 보였다
    rng = np.random.default_rng(seed)
    planes = [((0.0, 0.0, -1.0), 0.0), ((0.0, 0.0, 1.0), height)]
    for k in range(9):
        a = k * 2.0 * math.pi / 9.0 + rng.uniform(-0.2, 0.2)
        up = rng.uniform(0.2, 1.1)
        n = (math.cos(a), math.sin(a), up)
        planes.append(plane_through(n, (math.cos(a) * radius * rng.uniform(0.75, 1.0), math.sin(a) * radius * rng.uniform(0.75, 1.0), height * 0.35)))
    b.prims.append(Convex(b.at(center), b.rot @ yaw_matrix(rng.uniform(0, 90)), planes, radius * 1.6 + height, material))


def rock():
    b = Builder(20.0)
    faceted(b, (0.0, 0.0, 0.0), 0.36, 0.34, 3, 'stone_rock')
    faceted(b, (0.32, 0.16, 0.0), 0.18, 0.18, 4, 'stone_rock')
    return b


def pebble():
    b = Builder(0.0)
    for x, y, r in ((0.0, 0.0, 0.12), (0.18, -0.08, 0.08), (-0.12, 0.12, 0.07)):
        b.ellipsoid((x, y, r * 0.4), (r, r * 0.85, r * 0.55), yaw_matrix(x * 200), 'stone')
    return b


def thorn():
    b = Builder(0.0)
    rng = np.random.default_rng(5)
    for k in range(9):
        a = math.radians(k * 40 + rng.uniform(0, 20))
        tilt = rng.uniform(0.3, 0.9)
        end = vec(math.cos(a) * 0.4 * tilt, math.sin(a) * 0.4 * tilt, rng.uniform(0.15, 0.4))
        b.capsule((0.0, 0.0, 0.02), end, 0.025, 0.01, 'bark')
        b.ellipsoid(end * 0.7, (0.05, 0.04, 0.03), I3, 'dry_grass')
    return b


def grass():
    b = Builder(0.0)
    rng = np.random.default_rng(9)
    for k in range(9):
        x, y = rng.uniform(-0.15, 0.15, 2)
        top = vec(x * 1.8, y * 1.8, rng.uniform(0.18, 0.3))
        b.capsule((x, y, 0.0), top, 0.022, 0.006, 'dry_grass' if k % 3 else 'green')
    return b


def reeds():
    b = Builder(0.0)
    rng = np.random.default_rng(11)
    for k in range(7):
        x, y = rng.uniform(-0.12, 0.12, 2)
        h = rng.uniform(0.6, 0.85)
        top = vec(x * 1.4, y * 1.4, h)
        b.capsule((x, y, 0.0), top, 0.02, 0.012, 'green')
        if k % 2 == 0:
            b.ellipsoid(top - vec(0.0, 0.0, 0.08), (0.03, 0.03, 0.08), I3, 'leather')
    return b


def brazier():
    b = Builder(0.0)
    for k in range(3):
        a = math.radians(k * 120 + 30)
        b.capsule((math.cos(a) * 0.18, math.sin(a) * 0.18, 0.0), (0.0, 0.0, 0.42), 0.025, 0.025, 'iron')
    b.ellipsoid((0.0, 0.0, 0.48), (0.2, 0.2, 0.1), I3, 'iron')
    b.ellipsoid((0.0, 0.0, 0.58), (0.13, 0.13, 0.14), I3, 'fire')
    b.ellipsoid((0.02, 0.0, 0.66), (0.06, 0.06, 0.12), I3, 'fire_core')
    return b


def bones():
    b = Builder(30.0)
    b.ellipsoid((0.18, 0.0, 0.07), (0.09, 0.08, 0.07), I3, 'bone')
    b.decal((0.27, 0.03, 0.08), (40, 34, 30))
    for k in range(4):
        x = -0.05 - k * 0.07
        b.capsule((x, -0.12, 0.02), (x, 0.12, 0.05), 0.016, 0.016, 'bone')
    b.capsule((-0.35, 0.0, 0.02), (0.05, 0.0, 0.03), 0.02, 0.02, 'bone')
    return b


def herb():
    b = Builder(0.0)
    for k in range(6):
        a = math.radians(k * 60)
        tip = vec(math.cos(a) * 0.16, math.sin(a) * 0.16, 0.12 + (k % 2) * 0.06)
        b.capsule((0.0, 0.0, 0.0), tip, 0.035, 0.02, 'leaf')
    for x, y in ((0.05, 0.04), (-0.06, 0.02), (0.0, -0.06)):
        b.ellipsoid((x, y, 0.22), (0.03, 0.03, 0.03), I3, 'herb_glow')
    return b


def crops(variant):
    def build():
        b = Builder(0.0)
        material = 'leaf' if variant == 0 else 'dry_grass'
        for row in (-0.12, 0.12):
            for k in range(4):
                x = -0.3 + k * 0.2
                h = 0.12 if variant == 0 else 0.07
                b.capsule((x, row, 0.0), (x + 0.03, row + 0.02, h), 0.03, 0.012, material)
        return b

    return build


# ------------------------------------------------------------ 성채 소품

def tent(variant):
    def build():
        b = Builder(0.0)
        length = 0.68
        half_w = 0.62
        height = 0.95
        cloth = 'canvas' if variant == 0 else 'canvas_dark'
        flap = 'teal' if variant == 0 else 'red'
        planes = []
        for side in (1.0, -1.0):
            n = np.array([0.0, side * height, half_w])
            planes.append((n / np.linalg.norm(n), height * half_w / np.linalg.norm(n)))
        planes += [((1.0, 0.0, 0.0), length), ((-1.0, 0.0, 0.0), length), ((0.0, 0.0, -1.0), 0.0)]
        b.prims.append(Convex(b.at((0.0, 0.0, 0.0)), b.rot, planes, 1.8, cloth))
        # 앞 입구 - 안쪽 삼각형을 조금 튀어나오게
        inner = []
        for side in (1.0, -1.0):
            n = np.array([0.0, side * height * 0.6, half_w * 0.45])
            inner.append((n / np.linalg.norm(n), height * 0.6 * half_w * 0.45 / np.linalg.norm(n)))
        inner += [((1.0, 0.0, 0.0), length + 0.02), ((-1.0, 0.0, 0.0), -(length - 0.05)), ((0.0, 0.0, -1.0), 0.0)]
        b.prims.append(Convex(b.at((0.0, 0.0, 0.0)), b.rot, inner, 1.8, flap))
        b.capsule((length + 0.08, 0.0, height - 0.02), (-length - 0.08, 0.0, height - 0.02), 0.03, 0.03, 'wood_post')
        return b

    return build


def banner():
    b = Builder(0.0)
    b.capsule((0.0, 0.0, 0.0), (0.0, 0.0, 2.0), 0.035, 0.03, 'wood_post')
    b.capsule((0.0, -0.3, 1.9), (0.0, 0.3, 1.9), 0.025, 0.025, 'wood_post')
    b.box((0.03, 0.0, 1.45), (0.015, 0.27, 0.42), I3, 0.0, 'teal')
    b.ellipsoid((0.05, 0.0, 1.5), (0.01, 0.1, 0.12), I3, 'gold')
    b.ellipsoid((0.0, 0.0, 2.05), (0.05, 0.05, 0.05), I3, 'gold')
    return b


def totem():
    b = Builder(0.0)
    b.capsule((0.0, 0.0, 0.0), (0.0, 0.0, 1.55), 0.05, 0.04, 'wood_post')
    b.ellipsoid((0.04, 0.0, 1.62), (0.13, 0.11, 0.12), I3, 'bone')
    b.ellipsoid((0.12, 0.0, 1.55), (0.07, 0.08, 0.05), I3, 'bone')
    b.decal((0.16, 0.05, 1.64), (30, 24, 24))
    b.decal((0.16, -0.05, 1.64), (30, 24, 24))
    for side in (1.0, -1.0):
        b.capsule((0.0, side * 0.08, 1.7), (0.0, side * 0.24, 1.85), 0.03, 0.01, 'bone')
    for k, z in enumerate((1.3, 1.15)):
        b.box((0.06, 0.0, z), (0.01, 0.06 + k * 0.02, 0.12), I3, 0.0, 'red')
    return b


def grave():
    b = Builder(0.0)
    b.ellipsoid((-0.12, 0.0, 0.0), (0.32, 0.2, 0.07), I3, 'dirt')
    b.box((0.08, 0.0, 0.36), (0.035, 0.035, 0.36), I3, 0.0, 'grave_wood', 1)
    b.box((0.08, 0.0, 0.52), (0.03, 0.17, 0.035), I3, 0.0, 'grave_wood', 1)
    return b


def pouch():
    b = Builder(0.0)
    b.ellipsoid((0.0, 0.0, 0.1), (0.13, 0.12, 0.1), I3, 'leather')
    b.ellipsoid((0.0, 0.0, 0.21), (0.05, 0.05, 0.03), I3, 'leather')
    b.decal((0.12, 0.05, 0.14), (222, 180, 84))
    return b


def arrow():
    # 세로로 세워 그린다. 게임이 날아가는 방향으로 돌린다
    b = Builder(0.0)
    b.capsule((0.0, 0.0, 0.0), (0.0, 0.0, 0.56), 0.012, 0.012, 'wood_post', 2)
    b.capsule((0.0, 0.0, 0.56), (0.0, 0.0, 0.64), 0.03, 0.005, 'steel', 2)
    b.box((0.0, 0.0, 0.06), (0.03, 0.006, 0.06), I3, 0.0, 'awning_white', 2)
    return b


def stall():
    b = Builder(0.0)
    hx = 0.9
    hy = 0.5
    for x, y in ((hx, hy), (hx, -hy), (-hx, hy), (-hx, -hy)):
        b.capsule((x, y, 0.0), (x, y, 1.55 if x < 0 else 1.25), 0.03, 0.03, 'wood_post')
    b.box((0.0, 0.0, 0.45), (hx, hy, 0.05), I3, 0.0, 'wood')
    b.box((0.0, hy - 0.02, 0.22), (hx, 0.02, 0.2), I3, 0.0, 'wood')
    # 좌판 물건
    for k, x in enumerate((-0.6, -0.3, 0.0, 0.3, 0.6)):
        material = ('fruit', 'herb_pile', 'cork', 'fruit', 'herb_pile')[k]
        b.ellipsoid((x, 0.1, 0.56), (0.11, 0.11, 0.07), I3, material)
    # 차양 - 빨강 흰 줄
    slope = pitch_matrix(-13.0)
    for k in range(6):
        y = -hy - 0.08 + (k + 0.5) * (2 * hy + 0.16) / 6.0
        material = 'awning_red' if k % 2 == 0 else 'awning_white'
        b.box((0.0, y, 1.42), (hx + 0.12, (2 * hy + 0.16) / 12.0, 0.025), slope, 0.0, material)
    for k in range(6):
        y = -hy - 0.08 + (k + 0.5) * (2 * hy + 0.16) / 6.0
        material = 'awning_red' if k % 2 == 0 else 'awning_white'
        b.box((hx + 0.12, y, 1.18), (0.015, (2 * hy + 0.16) / 12.0, 0.08), I3, 0.0, material)
    return b


def lamp():
    b = Builder(0.0)
    b.capsule((0.0, 0.0, 0.0), (0.0, 0.0, 1.42), 0.05, 0.04, 'iron', 1)
    b.box((0.0, 0.0, 0.04), (0.1, 0.1, 0.04), I3, 0.0, 'iron')
    b.box((0.0, 0.0, 1.42), (0.1, 0.1, 0.02), I3, 0.0, 'iron')
    b.box((0.0, 0.0, 1.52), (0.085, 0.085, 0.1), I3, 0.0, 'lamp_glow')
    b.prims.append(Convex(b.at((0.0, 0.0, 1.6)), b.rot,
                          [((1.0, 0.0, 1.0), 0.08), ((-1.0, 0.0, 1.0), 0.08), ((0.0, 1.0, 1.0), 0.08), ((0.0, -1.0, 1.0), 0.08), ((0.0, 0.0, -1.0), 0.0)],
                          0.2, 'iron'))
    return b


def anvil():
    b = Builder(-20.0)
    b.capsule((0.0, 0.0, 0.0), (0.0, 0.0, 0.22), 0.22, 0.2, 'wood')
    b.box((0.0, 0.0, 0.3), (0.12, 0.09, 0.07), I3, 0.0, 'anvil_iron')
    b.box((0.0, 0.0, 0.39), (0.17, 0.12, 0.03), I3, 0.0, 'anvil_iron')
    b.box((0.0, 0.0, 0.47), (0.3, 0.14, 0.06), I3, 0.02, 'anvil_iron')
    b.capsule((0.28, 0.0, 0.49), (0.56, 0.0, 0.51), 0.07, 0.015, 'anvil_iron')
    b.box((-0.32, 0.0, 0.48), (0.06, 0.12, 0.05), I3, 0.0, 'anvil_iron')
    return b


# ------------------------------------------------------------ 떨어진 물건 · 아이콘

def sword_model(direction_vec):
    def build():
        b = Builder(0.0)
        d = normalize(np.array(direction_vec, dtype=float))
        rot = basis_from_axis(d, (0.0, 0.0, 1.0) if abs(d[2]) < 0.9 else (1.0, 0.0, 0.0))
        base = -d * 0.45
        b.capsule(base - d * 0.12, base + d * 0.04, 0.025, 0.025, 'grip', 1)
        b.ellipsoid(base - d * 0.14, (0.03, 0.03, 0.03), I3, 'gold', 1)
        b.box(base + d * 0.06, (0.1, 0.022, 0.02), rot, 0.008, 'gold', 1)
        b.box(base + d * 0.4, (0.038, 0.01, 0.33), rot, 0.006, 'blade', 2)
        b.capsule(base + d * 0.73, base + d * 0.8, 0.032, 0.008, 'blade', 2)
        return b

    return build


def cleaver():
    b = Builder(0.0)
    b.capsule((0.0, 0.0, 0.0), (0.0, 0.0, 0.16), 0.028, 0.028, 'grip', 1)
    b.box((0.0, 0.0, 0.42), (0.08, 0.015, 0.26), I3, 0.01, 'rust', 2)
    return b


def coins(count, spread):
    def build():
        b = Builder(0.0)
        rng = np.random.default_rng(count)
        for k in range(count):
            x, y = rng.uniform(-spread, spread, 2)
            z = 0.02 + (k // 3) * 0.035
            b.ellipsoid((x * (1.0 - k / count), y * (1.0 - k / count), z), (0.1, 0.1, 0.025), roll_matrix(rng.uniform(-15, 15)), 'gold')
        return b

    return build


def coin_stacks():
    # 쌓은 동전 셋 + 흩어진 것
    b = Builder(0.0)
    for (x, y), count in (((-0.1, 0.12), 5), ((0.14, 0.0), 3), ((-0.08, -0.14), 2)):
        for k in range(count):
            b.ellipsoid((x, y, 0.03 + k * 0.05), (0.11, 0.11, 0.025), I3, 'gold')
    b.ellipsoid((0.2, 0.22, 0.02), (0.1, 0.1, 0.022), roll_matrix(12.0), 'gold')
    return b


def crystal(big):
    def build():
        b = Builder(25.0)

        def gem(center, height, radius, tilt):
            # 육각 기둥 + 위아래 뾰족한 끝
            rot = pitch_matrix(tilt[0]) @ roll_matrix(tilt[1])
            planes = []
            for k in range(6):
                a = math.radians(k * 60)
                planes.append(plane_through((math.cos(a), math.sin(a), 0.0), (math.cos(a) * radius, math.sin(a) * radius, 0.0)))
                planes.append(plane_through((math.cos(a), math.sin(a), 0.9), (0.0, 0.0, height * 0.5)))
                planes.append(plane_through((math.cos(a), math.sin(a), -1.6), (0.0, 0.0, -height * 0.5)))
            b.prims.append(Convex(b.at(center), b.rot @ rot, planes, height, 'trace'))

        gem((0.0, 0.0, 0.3), 0.6, 0.12, (0.0, 0.0))
        if big:
            gem((0.1, 0.12, 0.16), 0.32, 0.07, (0.0, 25.0))
            gem((-0.1, -0.1, 0.14), 0.28, 0.06, (20.0, -15.0))
        b.decal((0.1, 0.0, 0.36), (240, 220, 255))
        return b

    return build


def potion(big):
    def build():
        b = Builder(0.0)
        b.ellipsoid((0.0, 0.0, 0.17), (0.17, 0.17, 0.17), I3, 'glass_red')
        b.capsule((0.0, 0.0, 0.3), (0.0, 0.0, 0.42), 0.06, 0.055, 'glass')
        b.capsule((0.0, 0.0, 0.43), (0.0, 0.0, 0.48), 0.065, 0.065, 'cork')
        b.decal((0.1, -0.06, 0.25), (255, 230, 230))
        return b

    return build


# ------------------------------------------------------------ 목록

# 이름 -> ( 프레임 만드는 함수들, 배율, 블록인지 )
PROPS = {
    'SPRITE_SWORD': ([sword_model((0.0, 0.0, 1.0))], 1.0, False),
    'SPRITE_CLEAVER': ([cleaver], 1.0, False),
    'SPRITE_GOLD': ([coins(5, 0.1)], 1.2, False),
    'SPRITE_DEAD_TREE': ([dead_tree(1), dead_tree(2)], 1.0, False),
    'SPRITE_ROCK': ([rock], 1.0, False),
    'SPRITE_PEBBLE': ([pebble], 1.0, False),
    'SPRITE_THORN': ([thorn], 1.0, False),
    'SPRITE_GRASS': ([grass], 1.0, False),
    'SPRITE_REEDS': ([reeds], 1.0, False),
    'SPRITE_BRAZIER': ([brazier], 1.0, False),
    'SPRITE_BONES': ([bones], 1.0, False),
    'SPRITE_BLOCK_WALL': ([block_wall], 1.0, True),
    'SPRITE_BLOCK_TOWER': ([block_tower], 1.0, True),
    'SPRITE_BLOCK_HOUSE': ([block_house(0), block_house(1)], 1.0, True),
    'SPRITE_BLOCK_RUIN': ([block_ruin(0), block_ruin(1)], 1.0, True),
    'SPRITE_TENT': ([tent(0), tent(1)], 1.0, False),
    'SPRITE_BANNER': ([banner], 1.0, False),
    'SPRITE_TOTEM': ([totem], 1.0, False),
    'SPRITE_GRAVE': ([grave], 1.0, False),
    'SPRITE_POUCH': ([pouch], 1.0, False),
    'SPRITE_HERB': ([herb], 1.0, False),
    'SPRITE_ARROW': ([arrow], 1.0, False),
    'SPRITE_STALL': ([stall], 1.0, False),
    'SPRITE_CROPS': ([crops(0), crops(1)], 1.0, False),
    'SPRITE_LAMP': ([lamp], 1.0, False),
    'SPRITE_ANVIL': ([anvil], 1.3, False),
    'SPRITE_ICON_SWORD': ([sword_model((0.55, -0.55, 0.62))], 0.92, False),
    'SPRITE_ICON_TRACE': ([crystal(True)], 1.5, False),
    'SPRITE_ICON_POTION': ([potion(True)], 1.75, False),
    'SPRITE_ICON_GOLD': ([coin_stacks], 1.55, False),
    'SPRITE_DROP_TRACE': ([crystal(False)], 1.2, False),
    'SPRITE_DROP_POTION': ([potion(False)], 1.25, False),
    'SPRITE_DROP_SWORD': ([sword_model((0.62, -0.62, 0.05))], 0.75, False),
}
