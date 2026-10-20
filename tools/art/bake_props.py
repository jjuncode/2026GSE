# -*- coding: utf-8 -*-
# 소품 아틀라스 굽기. python bake_props.py [미리보기.png]
# SpriteId.h 의 SPRITE_ 순서대로 한 장에 싣는다. 그림마다 프레임 크기는 같고 발밑은 아래변 가운데
import json
import multiprocessing
import os
import re
import sys
import time

import numpy as np
from PIL import Image

import icons2d
import props
from pixel3d import render

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(ROOT, 'SimpleGame', 'Art')
SPRITE_ID_H = os.path.join(ROOT, 'SimpleGame', 'SpriteId.h')

# 캐릭터 자리는 시트의 정면 대기 첫 장으로 채운다 ( 시트가 빠졌을 때 쓰는 대체 그림 )
FROM_SHEET = {
    'SPRITE_PLAYER': 'player', 'SPRITE_BLIGHTED': 'blighted', 'SPRITE_HOUND': 'hound', 'SPRITE_DEVOURER': 'devourer',
    'SPRITE_GATEBREAKER': 'gatebreaker', 'SPRITE_NPC_HUNTER': 'npc_hunter', 'SPRITE_NPC_RAIDER': 'npc_raider',
    'SPRITE_NPC_MERCHANT': 'npc_merchant', 'SPRITE_NPC_GATHERER': 'npc_gatherer', 'SPRITE_NPC_WARDEN': 'npc_warden',
}

# 가방 칸 아이콘은 16 x 16 고정 ( HUD 가 그 크기로 자리를 잡는다 )
FIXED_SIZE = {'SPRITE_ICON_SWORD': (16, 16), 'SPRITE_ICON_TRACE': (16, 16), 'SPRITE_ICON_POTION': (16, 16), 'SPRITE_ICON_GOLD': (16, 16)}

BLOCK_WIDTH = 36
CANVAS = 160


def read_sprite_ids():
    text = open(SPRITE_ID_H, encoding='utf-8-sig').read()
    found = re.findall(r'constexpr int32 (SPRITE_[A-Z0-9_]+) = (\d+);', text)
    ids = sorted((int(v), name) for name, v in found if name not in ('SPRITE_NONE', 'SPRITE_COUNT'))
    count = int(re.search(r'SPRITE_COUNT = (\d+);', text).group(1))
    names = [name for _, name in ids]
    assert len(names) == count and [i for i, _ in ids] == list(range(count)), 'SpriteId.h 번호가 비었다'
    return names


def render_prop(job):
    name, index = job
    builders, scale, is_block = props.PROPS[name]
    b = builders[index]()
    anchor_x = CANVAS // 2
    anchor_y = CANVAS - 24
    img = render(b.prims, props.MATS, CANVAS, CANVAS, anchor_x, anchor_y, b.decals, scale=scale, outline=not is_block)
    return img, anchor_x, anchor_y


def crop_frames(images, anchors, is_block):
    # 발밑이 아래변 가운데에 오게 자른다. 프레임끼리 크기를 맞춘다
    top = CANVAS
    bottom = 0
    half = 0
    for img, (ax, ay) in zip(images, anchors):
        alpha = img[:, :, 3] > 0
        if not alpha.any():
            continue
        ys = np.nonzero(alpha.any(axis=1))[0]
        xs = np.nonzero(alpha.any(axis=0))[0]
        top = min(top, ys[0])
        bottom = max(bottom, ys[-1] + 1, ay)
        half = max(half, ax - xs[0], xs[-1] + 1 - ax)
    out = []
    for img, (ax, ay) in zip(images, anchors):
        if is_block:
            # 칸 앞 꼭짓점이 아래변. 칸 가운데에서 8 칸 아래
            out.append(img[top:ay + 8, ax - BLOCK_WIDTH // 2:ax + BLOCK_WIDTH // 2])
        else:
            out.append(img[top:bottom, ax - half:ax + half])
    return out


def fit_fixed(frame, size):
    w, h = size
    fh, fw = frame.shape[:2]
    canvas = np.zeros((h, w, 4), dtype=np.uint8)
    if fw > w or fh > h:
        print('  [warn] icon bigger than slot', fw, fh)
        frame = frame[max(0, fh - h):, max(0, (fw - w) // 2):max(0, (fw - w) // 2) + w]
        fh, fw = frame.shape[:2]
    y0 = h - fh - (h - fh) // 2
    x0 = (w - fw) // 2
    canvas[y0:y0 + fh, x0:x0 + fw] = frame
    return canvas


def sheet_frame(sheet_name):
    manifest = json.load(open(os.path.join(OUT_DIR, sheet_name + '.json'), encoding='utf-8'))
    image = np.array(Image.open(os.path.join(OUT_DIR, sheet_name + '.png')).convert('RGBA'))
    index = manifest['anims']['idle']['dirs'][2][0]
    x, y, w, h, ax, ay = manifest['frames'][index]
    frame = image[y:y + h, x:x + w]
    half = max(ax, w - ax)
    padded = np.zeros((ay, half * 2, 4), dtype=np.uint8)
    rows = min(h, ay)
    padded[0:rows, half - ax:half - ax + w] = frame[0:rows]
    return padded


def pack(sprites):
    area = sum((f.shape[0] + 1) * (f.shape[1] + 1) for _, frames in sprites for f in frames)
    width = 256
    while width * width < area * 1.3:
        width *= 2
    order = []
    for si, (_, frames) in enumerate(sprites):
        for fi, frame in enumerate(frames):
            order.append((frame.shape[0], si, fi))
    order.sort(key=lambda o: -o[0])
    places = {}
    x = 1
    y = 1
    shelf = 0
    for h, si, fi in order:
        w = sprites[si][1][fi].shape[1]
        if x + w + 1 > width:
            x = 1
            y += shelf + 1
            shelf = 0
        places[(si, fi)] = (x, y)
        x += w + 1
        shelf = max(shelf, h)
    height = 64
    while height < y + shelf + 1:
        height *= 2
    sheet = np.zeros((height, width, 4), dtype=np.uint8)
    for (si, fi), (px, py) in places.items():
        frame = sprites[si][1][fi]
        sheet[py:py + frame.shape[0], px:px + frame.shape[1]] = frame
    return sheet, places


def main():
    begin = time.time()
    names = read_sprite_ids()

    jobs = []
    for name in names:
        if name in props.PROPS:
            for index in range(len(props.PROPS[name][0])):
                jobs.append((name, index))
    with multiprocessing.Pool(max(1, multiprocessing.cpu_count() - 1)) as pool:
        results = pool.map(render_prop, jobs)
    rendered = {}
    for (name, index), result in zip(jobs, results):
        rendered.setdefault(name, []).append(result)

    sprites = []
    for name in names:
        if name in props.PROPS:
            images = [r[0] for r in rendered[name]]
            anchors = [(r[1], r[2]) for r in rendered[name]]
            frames = crop_frames(images, anchors, props.PROPS[name][2])
        elif name in icons2d.FLAT:
            frames = [fn() for fn in icons2d.FLAT[name]]
        elif name in FROM_SHEET:
            frames = [sheet_frame(FROM_SHEET[name])]
        else:
            raise RuntimeError('no art for ' + name)
        if name in FIXED_SIZE:
            frames = [fit_fixed(f, FIXED_SIZE[name]) for f in frames]
        sprites.append((name, frames))

    sheet, places = pack(sprites)
    entries = []
    for si, (name, frames) in enumerate(sprites):
        h, w = frames[0].shape[:2]
        entries.append(dict(name=name, w=int(w), h=int(h), frames=[list(places[(si, fi)]) for fi in range(len(frames))]))

    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray(sheet, 'RGBA').save(os.path.join(OUT_DIR, 'props.png'), optimize=True)
    with open(os.path.join(OUT_DIR, 'props.json'), 'w', encoding='utf-8', newline=chr(10)) as f:
        json.dump(dict(width=int(sheet.shape[1]), height=int(sheet.shape[0]), sprites=entries), f, separators=(',', ':'))

    print('props', len(sprites), 'sheet', sheet.shape[1], 'x', sheet.shape[0], 'sec', round(time.time() - begin, 1))
    for name, frames in sprites:
        h, w = frames[0].shape[:2]
        print('  ', name, w, 'x', h, 'frames', len(frames))

    if len(sys.argv) > 1:
        preview(sprites, sys.argv[1])


def preview(sprites, path):
    zoom = 3
    cells = []
    for name, frames in sprites:
        for frame in frames:
            cells.append(frame)
    cols = 12
    cw = max(f.shape[1] for f in cells) + 4
    ch = max(f.shape[0] for f in cells) + 4
    rows = (len(cells) + cols - 1) // cols
    out = np.zeros((rows * ch, cols * cw, 4), dtype=np.uint8)
    out[:, :, :3] = (150, 134, 104)
    out[:, :, 3] = 255
    for i, frame in enumerate(cells):
        r, c = divmod(i, cols)
        y0 = r * ch + ch - 2 - frame.shape[0]
        x0 = c * cw + (cw - frame.shape[1]) // 2
        region = out[y0:y0 + frame.shape[0], x0:x0 + frame.shape[1]]
        alpha = frame[:, :, 3:4] / 255.0
        region[:, :, :3] = (region[:, :, :3] * (1 - alpha) + frame[:, :, :3] * alpha).astype(np.uint8)
    Image.fromarray(out, 'RGBA').resize((out.shape[1] * zoom, out.shape[0] * zoom), Image.NEAREST).save(path)
    print('preview', path)


if __name__ == '__main__':
    main()
