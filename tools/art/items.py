# -*- coding: utf-8 -*-
# 상점 · 의뢰 보상에 새로 붙는 아이템 그림. 소품 그림판( props.png )과 따로 Art/items.png 에 굽는다
# 이름은 ItemData.json 의 "icon": "ext:이름" 과 맞춘다
import numpy as np

import math

from pixel3d import Convex, Material, basis_from_axis, normalize, pitch_matrix, roll_matrix
from props import MATS, plane_through, pouch
from rig import Builder

I3 = np.eye(3)

ITEM_MATS = dict(MATS)
ITEM_MATS.update({
    'quilt': Material([(70, 58, 44), (104, 88, 66), (140, 120, 90), (172, 152, 116)], outline=(34, 26, 18)),
    'chain': Material([(52, 56, 64), (88, 94, 106), (130, 138, 150), (176, 184, 196)], outline=(22, 24, 30), metal=True),
    'plate': Material([(70, 76, 90), (120, 128, 146), (174, 182, 200), (226, 232, 244)], outline=(30, 32, 40), metal=True),
    'royal_blue': Material([(20, 30, 70), (34, 52, 112), (58, 84, 160), (96, 126, 200)], outline=(10, 14, 34)),
    'glass_big': Material([(90, 14, 20), (150, 24, 32), (210, 48, 52), (250, 120, 110)], outline=(40, 6, 8)),
    'shadow': Material([(14, 8, 22), (34, 18, 52), (62, 34, 92), (98, 60, 140)], outline=(6, 2, 10)),
    'shadow_core': Material([(150, 90, 230), (210, 160, 255)], outline=(40, 10, 60), emissive=True),
    'shadow_gem': Material([(24, 10, 40), (56, 24, 92), (104, 54, 168), (176, 120, 240)], outline=(10, 4, 18), metal=True),
})


def sword(direction_vec, blade_len, blade_half, guard_half, guard_mat, pommel_mat, gem=None):
    def build():
        b = Builder(0.0)
        d = normalize(np.array(direction_vec, dtype=float))
        rot = basis_from_axis(d, (0.0, 0.0, 1.0) if abs(d[2]) < 0.9 else (1.0, 0.0, 0.0))
        base = -d * (blade_len * 0.55)
        b.capsule(base - d * 0.12, base + d * 0.04, 0.025, 0.025, 'grip', 1)
        b.ellipsoid(base - d * 0.14, (0.034, 0.034, 0.034), I3, pommel_mat, 1)
        b.box(base + d * 0.06, (guard_half, 0.024, 0.022), rot, 0.008, guard_mat, 1)
        b.box(base + d * (0.06 + blade_len * 0.5), (blade_half, 0.01, blade_len * 0.5), rot, 0.006, 'blade', 2)
        b.capsule(base + d * (0.06 + blade_len), base + d * (0.1 + blade_len), blade_half * 0.85, 0.008, 'blade', 2)
        if gem is not None:
            b.decal(base + d * 0.06, gem)
        return b

    return build


def armor(body_mat, shoulder_mat, trim_mat, belt_mat):
    # 앞에서 본 몸통 - 가슴판 하나, 어깨 둘, 허리띠
    def build():
        b = Builder(0.0)
        # 정면이 화면 쪽 ( +x +y 사이 ) 을 보게 45 도 돌린 축
        side = np.array([0.7071, -0.7071, 0.0])
        front = np.array([0.7071, 0.7071, 0.0])
        b.ellipsoid((0.0, 0.0, 0.3), (0.15, 0.15, 0.24), I3, body_mat, 1)
        for k in (-1.0, 1.0):
            b.ellipsoid(tuple(side * 0.17 * k + np.array([0.0, 0.0, 0.45])), (0.1, 0.1, 0.075), I3, shoulder_mat, 2)
        b.ellipsoid((0.0, 0.0, 0.13), (0.155, 0.155, 0.04), I3, belt_mat, 2)
        b.capsule(tuple(front * 0.13 + np.array([0.0, 0.0, 0.2])), tuple(front * 0.13 + np.array([0.0, 0.0, 0.47])), 0.02, 0.02, trim_mat, 3)
        return b

    return build


def big_potion():
    b = Builder(0.0)
    b.ellipsoid((0.0, 0.0, 0.2), (0.21, 0.21, 0.2), I3, 'glass_big')
    b.capsule((0.0, 0.0, 0.36), (0.0, 0.0, 0.5), 0.07, 0.065, 'glass')
    b.capsule((0.0, 0.0, 0.51), (0.0, 0.0, 0.57), 0.08, 0.08, 'gold')
    b.decal((0.12, -0.07, 0.29), (255, 236, 236))
    return b


def shadow_essence():
    # 검은 연기 덩어리 셋 가운데 보랏빛 심지
    b = Builder(0.0)
    b.ellipsoid((0.0, 0.0, 0.2), (0.17, 0.17, 0.16), I3, 'shadow', 1)
    b.ellipsoid((0.06, -0.08, 0.34), (0.1, 0.1, 0.1), I3, 'shadow', 1)
    b.ellipsoid((-0.07, 0.07, 0.36), (0.07, 0.07, 0.08), I3, 'shadow', 1)
    b.ellipsoid((0.1, 0.0, 0.22), (0.07, 0.07, 0.07), I3, 'shadow_core', 2)
    return b


def shadow_crystal():
    # 육각 기둥 결정 둘. 소품 crystal 과 같은 짜임에 그림자 재질
    b = Builder(25.0)

    def gem(center, height, radius, tilt):
        rot = pitch_matrix(tilt[0]) @ roll_matrix(tilt[1])
        planes = []
        for k in range(6):
            a = math.radians(k * 60)
            planes.append(plane_through((math.cos(a), math.sin(a), 0.0), (math.cos(a) * radius, math.sin(a) * radius, 0.0)))
            planes.append(plane_through((math.cos(a), math.sin(a), 0.9), (0.0, 0.0, height * 0.5)))
            planes.append(plane_through((math.cos(a), math.sin(a), -1.6), (0.0, 0.0, -height * 0.5)))
        b.prims.append(Convex(b.at(center), b.rot @ rot, planes, height, 'shadow_gem'))

    gem((0.0, 0.0, 0.3), 0.62, 0.13, (0.0, 0.0))
    gem((0.11, 0.12, 0.16), 0.32, 0.07, (0.0, 25.0))
    b.decal((0.1, 0.0, 0.38), (220, 180, 255))
    return b


ICON_SWORD_DIR = (0.55, -0.55, 0.62)

# 이름 -> ( 만드는 함수, 배율 ). 아이콘은 전부 16 x 16 칸에 맞춘다
ICONS = [
    ('icon_sword_steel', sword(ICON_SWORD_DIR, 0.62, 0.045, 0.12, 'iron', 'iron'), 1.05),
    ('icon_sword_knight', sword(ICON_SWORD_DIR, 0.66, 0.05, 0.17, 'gold', 'gold'), 0.98),
    ('icon_sword_royal', sword(ICON_SWORD_DIR, 0.66, 0.055, 0.18, 'gold', 'royal_blue', (120, 200, 255)), 0.98),
    ('icon_armor_quilt', armor('quilt', 'quilt', 'leather', 'leather'), 1.55),
    ('icon_armor_chain', armor('chain', 'plate', 'leather', 'leather'), 1.55),
    ('icon_armor_plate', armor('plate', 'plate', 'gold', 'royal_blue'), 1.55),
    ('icon_potion_big', big_potion, 1.5),
    ('icon_pouch', pouch, 2.6),
    ('icon_shadow_essence', shadow_essence, 1.6),
    ('icon_shadow_crystal', shadow_crystal, 1.45),
]


# ------------------------------------------------------------ 상점 간판 ( 2D )

WOOD_DARK = (46, 32, 22)
WOOD = (92, 66, 42)
WOOD_LIGHT = (128, 94, 60)
IRON = (60, 62, 70)
INK = (18, 14, 12)


def signboard(icon_name):
    # 기둥 하나에 팔을 내고 판자를 매단다. 판자 가운데에 파는 물건 아이콘
    def frames():
        import vfx
        c = vfx.Canvas(26, 34)
        c.line(4, 2, 4, 33, WOOD_DARK, width=3)
        c.line(4, 3, 4, 33, WOOD, width=1)
        c.line(4, 4, 23, 4, WOOD_DARK, width=2)
        c.line(9, 4, 9, 8, IRON)
        c.line(20, 4, 20, 8, IRON)
        for y in range(8, 25):
            for x in range(6, 25):
                color = WOOD
                if y in (8, 24) or x in (6, 24):
                    color = WOOD_DARK
                elif (y - 8) % 6 == 0:
                    color = WOOD_DARK
                elif x == 7 or y == 9:
                    color = WOOD_LIGHT
                c.put(x, y, color)
        c.outline(INK)
        icon = ICON_CACHE.get(icon_name)
        if icon is not None:
            h, w = icon.shape[:2]
            ox = 15 - w // 2
            oy = 16 - h // 2
            for yy in range(h):
                for xx in range(w):
                    if icon[yy, xx, 3] > 0:
                        c.px[oy + yy, ox + xx, :3] = icon[yy, xx, :3]
                        c.px[oy + yy, ox + xx, 3] = 255
        return [c.px]

    return frames


# 굽는 쪽이 아이콘을 먼저 구워 여기 채운다
ICON_CACHE = {}

FLAT = [
    ('sign_weapon', signboard('icon_sword_steel')),
    ('sign_armor', signboard('icon_armor_chain')),
    ('sign_potion', signboard('icon_potion_big')),
    ('sign_general', signboard('icon_pouch')),
]


# ------------------------------------------------------------ 모험가 등급 휘장 ( 2D, RankData.json 순서 )

RANK_COLORS = [
    ((92, 56, 30), (176, 112, 62), (226, 160, 104)),     # 브론즈
    ((88, 96, 108), (176, 184, 196), (236, 240, 246)),   # 실버
    ((120, 84, 20), (226, 182, 60), (255, 236, 150)),    # 골드
    ((40, 104, 104), (128, 214, 206), (210, 255, 250)),  # 플래티넘
    ((20, 92, 44), (70, 196, 110), (170, 255, 190)),     # 에메랄드
    ((50, 84, 150), (130, 182, 255), (226, 240, 255)),   # 다이아
    ((110, 14, 30), (226, 52, 76), (255, 160, 170)),     # 루비
]


def rank_badges():
    import vfx
    out = []
    for dark, mid, light in RANK_COLORS:
        c = vfx.Canvas(16, 18)
        # 방패 - 위는 곧고 아래로 갈수록 좁아져 끝이 뾰족하다
        for y in range(1, 17):
            half = 6.5
            if y > 9:
                half = 6.5 * (1.0 - (y - 9) / 8.0)
            for x in range(16):
                dx = abs(x - 7.5)
                if dx > half:
                    continue
                color = mid
                if dx > half - 1.2 or y == 1:
                    color = dark
                elif x < 7.5 and y < 8:
                    color = light
                c.put(x, y, color)
        # 가운데 칼 한 자루
        c.line(7.5, 3, 7.5, 12, (250, 250, 250))
        c.line(5.5, 10, 9.5, 10, dark)
        c.outline(INK)
        out.append(c.px)
    return out


FLAT.append(('rank_badge', rank_badges))


def skill_stone():
    # 푸른 룬이 새겨진 납작한 돌
    b = Builder(0.0)
    b.ellipsoid((0.0, 0.0, 0.16), (0.2, 0.17, 0.14), I3, 'stone', 1)
    b.decal((0.12, -0.06, 0.2), (120, 200, 255))
    b.decal((0.13, 0.04, 0.16), (120, 200, 255))
    b.decal((0.1, 0.0, 0.26), (200, 240, 255))
    return b


def boss_stone():
    # 그림자 결정에 붉은 심지 - 보스의 힘
    b = Builder(15.0)
    b.ellipsoid((0.0, 0.0, 0.18), (0.18, 0.18, 0.2), I3, 'shadow_gem', 1)
    b.ellipsoid((0.08, 0.0, 0.22), (0.06, 0.06, 0.07), I3, 'red', 2)
    b.decal((0.12, -0.05, 0.3), (255, 160, 170))
    return b


ICONS.append(('icon_skill_stone', skill_stone, 1.6))
ICONS.append(('icon_boss_stone', boss_stone, 1.6))
