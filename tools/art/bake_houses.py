# -*- coding: utf-8 -*-
# 여러 칸짜리 집을 한 채로 굽고 칸마다 나눈다. python bake_houses.py [미리보기.png]
# 칸 하나짜리 블록으로 세우면 지붕이 칸마다 따로 솟는다 ( 2 x 2 집이 피라미드 넷 ).
# 한 채를 통째로 렌더한 뒤 화면 점마다 "광선이 처음 맞힌 표면이 어느 칸 위인가" 로 칸 조각에 나눈다.
# 게임이 칸마다 깊이 정렬해 그리면 조각이 원래 한 채로 맞물린다 - 앞 칸 표면은 앞 칸 몫이라 나중에 그려진다
import json
import multiprocessing
import os
import sys
import time

import numpy as np
from PIL import Image

import props
from pixel3d import Convex, Material, PX_X, PX_Y, SUPER, VIEW, march, normals, ray_origins, shade
from rig import Builder

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(ROOT, 'SimpleGame', 'Art')

TILE = 1.5
HALF = TILE * 0.5
SIZES = [(2, 2), (3, 2), (2, 3), (3, 3)]  # ZoneGen PickHouseSize 와 같은 넷
ROOF_COUNT = 2
CANVAS_W = 200
CANVAS_H = 200
ANCHOR_X = CANVAS_W // 2
ANCHOR_Y = CANVAS_H - 70  # 집 한가운데 바닥이 여기. 앞 칸 꼭짓점이 그 아래로 내려간다
FOOT_BELOW = 8  # 칸 가운데에서 앞 꼭짓점까지 ( 칸 높이 16 의 반 )
WALL_H = 0.95
ROOF_SLOPE = 0.6
EAVE = 0.09
I3 = np.eye(3)

MATS = dict(props.MATS)
MATS['roof_slate'] = Material([(40, 38, 44), (60, 58, 66), (84, 82, 92), (110, 108, 118)], outline=(18, 16, 22), pattern=props.tiles(0.1))
MATS['paper'] = Material([(170, 162, 140), (214, 206, 182), (240, 234, 214)], outline=(70, 62, 48), flat=True)
MATS['paper_old'] = Material([(150, 128, 84), (190, 166, 112), (214, 192, 136)], outline=(66, 52, 30), flat=True)
ROOFS = ['roof', 'roof_slate']


def hip_roof(b, size_x, size_y, material):
    # 네 경사면의 교집합. 긴 쪽으로 용마루가 선다
    ax = size_x * HALF + EAVE
    ay = size_y * HALF + EAVE
    s = ROOF_SLOPE
    length = float(np.sqrt(s * s + 1.0))
    planes = [
        ((s, 0.0, 1.0), s * ax / length), ((-s, 0.0, 1.0), s * ax / length),
        ((0.0, s, 1.0), s * ay / length), ((0.0, -s, 1.0), s * ay / length),
        ((1.0, 0.0, 0.0), ax), ((-1.0, 0.0, 0.0), ax), ((0.0, 1.0, 0.0), ay), ((0.0, -1.0, 0.0), ay),
        ((0.0, 0.0, -1.0), 0.07),
    ]
    rise = s * min(ax, ay)
    b.prims.append(Convex(b.at((0.0, 0.0, WALL_H)), b.rot, planes, max(ax, ay) * 1.5 + rise, material))
    return rise


def tile_center(size_x, size_y, i, j):
    return (i + 0.5) * TILE - size_x * HALF, (j + 0.5) * TILE - size_y * HALF


def house(size_x, size_y, door_col, roof):
    b = Builder(0.0)
    hx = size_x * HALF - 0.02
    hy = size_y * HALF - 0.02
    b.box((0.0, 0.0, WALL_H * 0.5), (hx, hy, WALL_H * 0.5), I3, 0.0, 'plaster')

    # 모서리 기둥 · 칸 경계 샛기둥 · 허리 들보. 긴 벽이 한 판으로 밋밋하지 않게
    for x, y in ((hx, hy), (hx, -hy), (-hx, hy)):
        b.box((x, y, WALL_H * 0.5), (0.05, 0.05, WALL_H * 0.5), I3, 0.0, 'timber')
    for i in range(1, size_x):
        x = i * TILE - size_x * HALF
        b.box((x, hy, WALL_H * 0.5), (0.04, 0.03, WALL_H * 0.5), I3, 0.0, 'timber')
    for j in range(1, size_y):
        y = j * TILE - size_y * HALF
        b.box((hx, y, WALL_H * 0.5), (0.03, 0.04, WALL_H * 0.5), I3, 0.0, 'timber')
    b.box((0.0, hy, WALL_H - 0.05), (hx, 0.03, 0.04), I3, 0.0, 'timber')
    b.box((hx, 0.0, WALL_H - 0.05), (0.03, hy, 0.04), I3, 0.0, 'timber')

    rise = hip_roof(b, size_x, size_y, ROOFS[roof])

    # 세 칸 집은 굴뚝. 용마루 뒤쪽에 박아 앞에서 머리만 보이게
    if max(size_x, size_y) >= 3:
        b.box((-hx * 0.35, -hy * 0.35, WALL_H + rise * 0.75), (0.13, 0.13, rise * 0.5 + 0.12), I3, 0.0, 'stone_wall')

    # 문은 카메라 쪽 면 ( +y ) door_col 칸. 나머지 칸은 창 둘
    for i in range(size_x):
        cx, _ = tile_center(size_x, size_y, i, size_y - 1)
        if i == door_col:
            b.box((cx - 0.15, hy + 0.01, 0.36), (0.18, 0.02, 0.36), I3, 0.0, 'door')
            b.box((cx + 0.35, hy + 0.01, 0.52), (0.1, 0.02, 0.13), I3, 0.0, 'window')
            continue
        b.box((cx - 0.3, hy + 0.01, 0.52), (0.12, 0.02, 0.13), I3, 0.0, 'window')
        b.box((cx + 0.28, hy + 0.01, 0.52), (0.12, 0.02, 0.13), I3, 0.0, 'window')

    for j in range(size_y):
        _, cy = tile_center(size_x, size_y, size_x - 1, j)
        b.box((hx + 0.01, cy - 0.28, 0.52), (0.02, 0.12, 0.13), I3, 0.0, 'window')
        b.box((hx + 0.01, cy + 0.3, 0.52), (0.02, 0.12, 0.13), I3, 0.0, 'window')

    return b


def notice_board():
    # 마을 게시판 - 기둥 둘에 판자, 작은 박공 지붕, 덕지덕지 붙은 종이. 카메라 쪽 ( +x +y 사이 ) 을 본다
    # 사람 키 ( 23 도트 ) 보다 커야 광장에서 게시판으로 읽힌다
    b = Builder(45.0)
    for side in (-0.62, 0.62):
        b.box((0.0, side, 0.95), (0.06, 0.06, 0.95), I3, 0.0, 'wood_post')
    b.box((0.02, 0.0, 1.32), (0.04, 0.66, 0.44), I3, 0.0, 'wood')
    b.box((0.03, 0.0, 0.86), (0.05, 0.7, 0.03), I3, 0.0, 'timber')
    # 박공 지붕 판자 둘. 용마루에서 앞뒤로 기운다
    for side in (1.0, -1.0):
        angle = side * 0.6
        tilt = np.array([[np.cos(angle), 0.0, np.sin(angle)], [0.0, 1.0, 0.0], [-np.sin(angle), 0.0, np.cos(angle)]])
        b.box((0.02 + side * 0.15, 0.0, 1.9), (0.18, 0.76, 0.025), tilt, 0.0, 'roof')
    # 붙은 종이 - 새 것은 희고 묵은 것은 누렇다. 붉은 밀랍 하나
    notes = [(-0.42, 1.55, 0.13, 0.11, 'paper'), (-0.12, 1.6, 0.11, 0.09, 'paper_old'), (0.18, 1.52, 0.14, 0.12, 'paper'),
             (0.46, 1.58, 0.1, 0.1, 'paper_old'), (-0.36, 1.12, 0.12, 0.12, 'paper_old'), (-0.04, 1.18, 0.13, 0.1, 'paper'),
             (0.3, 1.1, 0.12, 0.11, 'paper')]
    for y, z, hy, hz, material in notes:
        b.box((0.07, y, z), (0.008, hy, hz), I3, 0.0, material)
    b.box((0.08, 0.18, 1.6), (0.006, 0.035, 0.035), I3, 0.0, 'red')
    return b


def render_prop(builder):
    prims = builder.prims
    names = list(MATS.keys())
    prim_mat = np.array([names.index(p.material) for p in prims])
    prim_prio = np.array([p.priority for p in prims])
    width = 72
    height = 96
    anchor_x = width // 2
    anchor_y = height - 14
    origins = ray_origins(width, height, anchor_x, anchor_y, SUPER)
    hit, t, index = march(prims, origins)
    hit_ids = np.nonzero(hit)[0]
    points = origins[hit_ids] - VIEW[None, :] * t[hit_ids, None]
    norm = np.zeros_like(points)
    if hit_ids.size:
        norm = normals(prims, points)
    depth = np.where(hit, t, np.inf)
    image = shade(MATS, width, height, anchor_x, anchor_y, depth, hit_ids, prim_mat[index[hit_ids]], prim_prio[index[hit_ids]], points, norm, outline=True)
    ys, xs = np.nonzero(image[:, :, 3] > 0)
    half = int(max(anchor_x - xs.min(), xs.max() + 1 - anchor_x))
    return image[int(ys.min()):anchor_y + 1, anchor_x - half:anchor_x + half]


def render_house(job):
    # pixel3d.render 와 같은 순서에 칸 주인 표만 더 남긴다
    size_x, size_y, door_col, roof = job
    b = house(size_x, size_y, door_col, roof)
    prims = b.prims
    names = list(MATS.keys())
    prim_mat = np.array([names.index(p.material) for p in prims])
    prim_prio = np.array([p.priority for p in prims])

    origins = ray_origins(CANVAS_W, CANVAS_H, ANCHOR_X, ANCHOR_Y, SUPER)
    hit, t, index = march(prims, origins)
    hit_ids = np.nonzero(hit)[0]
    points = origins[hit_ids] - VIEW[None, :] * t[hit_ids, None]
    norm = np.zeros_like(points)
    if hit_ids.size:
        norm = normals(prims, points)
    depth = np.where(hit, t, np.inf)

    image = shade(MATS, CANVAS_W, CANVAS_H, ANCHOR_X, ANCHOR_Y, depth, hit_ids, prim_mat[index[hit_ids]], prim_prio[index[hit_ids]], points, norm, outline=False)

    # 광선마다 맞힌 점이 선 칸. 처마처럼 바닥 밖으로 나간 것은 가장자리 칸 몫
    owner = np.full(origins.shape[0], -1, dtype=int)
    if hit_ids.size:
        ix = np.clip(np.floor((points[:, 0] + size_x * HALF) / TILE).astype(int), 0, size_x - 1)
        iy = np.clip(np.floor((points[:, 1] + size_y * HALF) / TILE).astype(int), 0, size_y - 1)
        owner[hit_ids] = iy * size_x + ix

    # 칠한 점마다 표 많은 칸이 주인
    blocks = owner.reshape(CANVAS_H, SUPER, CANVAS_W, SUPER).transpose(0, 2, 1, 3).reshape(CANVAS_H * CANVAS_W, SUPER * SUPER)
    votes = np.stack([(blocks == tile).sum(axis=1) for tile in range(size_x * size_y)], axis=1)
    pixel_owner = np.where(votes.max(axis=1) > 0, np.argmax(votes, axis=1), -1).reshape(CANVAS_H, CANVAS_W)
    pixel_owner = np.where(image[:, :, 3] > 0, pixel_owner, -1)
    return image, pixel_owner


def split_pieces(job, image, pixel_owner):
    size_x, size_y, door_col, roof = job
    pieces = []
    for j in range(size_y):
        for i in range(size_x):
            cx, cy = tile_center(size_x, size_y, i, j)
            foot_x = int(round((cx - cy) * PX_X)) + ANCHOR_X
            foot_y = int(round((cx + cy) * PX_Y)) + ANCHOR_Y + FOOT_BELOW
            mask = pixel_owner == j * size_x + i
            ys, xs = np.nonzero(mask)
            if ys.size == 0:
                continue
            half = int(max(foot_x - xs.min(), xs.max() + 1 - foot_x))
            top = int(ys.min())
            # 칸 앞 꼭짓점 아래는 자른다. 옛 블록도 같은 자리에서 잘랐다 ( bake_props crop_frames )
            bottom = foot_y
            piece = np.zeros((bottom - top, half * 2, 4), dtype=np.uint8)
            sub = image[top:bottom, foot_x - half:foot_x + half]
            sub_mask = mask[top:bottom, foot_x - half:foot_x + half]
            piece[sub_mask] = sub[sub_mask]
            pieces.append(dict(tile=(i, j), image=piece))
    return pieces


def pack(images):
    area = sum((im.shape[0] + 1) * (im.shape[1] + 1) for im in images)
    width = 256
    while width * width < area * 1.3:
        width *= 2
    order = sorted(range(len(images)), key=lambda k: -images[k].shape[0])
    places = {}
    x = 1
    y = 1
    shelf = 0
    for k in order:
        h, w = images[k].shape[:2]
        if x + w + 1 > width:
            x = 1
            y += shelf + 1
            shelf = 0
        places[k] = (x, y)
        x += w + 1
        shelf = max(shelf, h)
    height = 64
    while height < y + shelf + 1:
        height *= 2
    sheet = np.zeros((height, width, 4), dtype=np.uint8)
    for k, (px, py) in places.items():
        im = images[k]
        sheet[py:py + im.shape[0], px:px + im.shape[1]] = im
    return sheet, places


def main():
    begin = time.time()
    jobs = []
    for size_x, size_y in SIZES:
        for roof in range(ROOF_COUNT):
            for door_col in range(size_x):
                jobs.append((size_x, size_y, door_col, roof))

    with multiprocessing.Pool(max(1, multiprocessing.cpu_count() - 1)) as pool:
        results = pool.map(render_house, jobs)

    entries = []
    images = []
    wholes = []
    for job, (image, pixel_owner) in zip(jobs, results):
        wholes.append(image)
        for piece in split_pieces(job, image, pixel_owner):
            entries.append(dict(size=[job[0], job[1]], door=job[2], roof=job[3], tile=list(piece['tile'])))
            images.append(piece['image'])

    # 마을 소품도 같은 판에. 발밑은 아래변 가운데
    prop_names = ['board']
    prop_images = [render_prop(notice_board())]

    sheet, places = pack(images + prop_images)
    for k, entry in enumerate(entries):
        h, w = images[k].shape[:2]
        entry['rect'] = [int(places[k][0]), int(places[k][1]), int(w), int(h)]

    prop_entries = []
    for k, name in enumerate(prop_names):
        slot = len(images) + k
        h, w = prop_images[k].shape[:2]
        prop_entries.append(dict(name=name, rect=[int(places[slot][0]), int(places[slot][1]), int(w), int(h)]))

    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray(sheet, 'RGBA').save(os.path.join(OUT_DIR, 'houses.png'), optimize=True)
    with open(os.path.join(OUT_DIR, 'houses.json'), 'w', encoding='utf-8', newline=chr(10)) as f:
        json.dump(dict(width=int(sheet.shape[1]), height=int(sheet.shape[0]), pieces=entries, props=prop_entries), f, separators=(',', ':'))

    print('houses', len(jobs), 'pieces', len(entries), 'sheet', sheet.shape[1], 'x', sheet.shape[0], 'sec', round(time.time() - begin, 1))

    if len(sys.argv) > 1:
        preview(jobs, wholes, entries, images, sys.argv[1])


def preview(jobs, wholes, entries, images, path):
    # 위 줄은 통째로 구운 집, 아래 줄은 조각을 칸 자리에 다시 맞춰 붙인 것. 둘이 같아야 한다
    zoom = 3
    cols = len(jobs)
    out = np.zeros((CANVAS_H * 2, CANVAS_W * cols, 4), dtype=np.uint8)
    out[:, :, :3] = (96, 84, 66)
    out[:, :, 3] = 255
    for c, (job, whole) in enumerate(zip(jobs, wholes)):
        region = out[0:CANVAS_H, c * CANVAS_W:(c + 1) * CANVAS_W]
        alpha = whole[:, :, 3:4] / 255.0
        region[:, :, :3] = (region[:, :, :3] * (1 - alpha) + whole[:, :, :3] * alpha).astype(np.uint8)

        size_x, size_y, door_col, roof = job
        # 게임처럼 x + y 작은 칸부터 덮어 그린다
        tiles = [(e, im) for e, im in zip(entries, images) if tuple(e['size']) == (size_x, size_y) and e['door'] == door_col and e['roof'] == roof]
        tiles.sort(key=lambda pair: pair[0]['tile'][0] + pair[0]['tile'][1])
        for e, im in tiles:
            i, j = e['tile']
            cx, cy = tile_center(size_x, size_y, i, j)
            foot_x = int(round((cx - cy) * PX_X)) + ANCHOR_X + c * CANVAS_W
            foot_y = int(round((cx + cy) * PX_Y)) + ANCHOR_Y + FOOT_BELOW + CANVAS_H
            h, w = im.shape[:2]
            region = out[foot_y - h:foot_y, foot_x - w // 2:foot_x - w // 2 + w]
            a = im[:, :, 3:4] / 255.0
            region[:, :, :3] = (region[:, :, :3] * (1 - a) + im[:, :, :3] * a).astype(np.uint8)
    Image.fromarray(out, 'RGBA').resize((out.shape[1] * zoom, out.shape[0] * zoom), Image.NEAREST).save(path)
    print('preview', path)


if __name__ == '__main__':
    main()
