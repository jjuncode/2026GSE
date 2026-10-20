# -*- coding: utf-8 -*-
# 캐릭터 시트 굽기. python bake.py [이름 ...]
# 동작 x 8 방향을 다 그려 빈 테두리를 잘라 한 장에 싣고, 프레임 위치와 발 기준점을 JSON 으로 남긴다
import hashlib
import importlib
import json
import multiprocessing
import os
import sys
import time

import numpy as np
from PIL import Image

from pixel3d import render
from timing import phase_to_time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(ROOT, 'SimpleGame', 'Art')

DIR_YAW = [-45.0 + 45.0 * i for i in range(8)]  # 0 = 화면 오른쪽에서 시계 방향. C++ GetDir8 과 같은 순서
PAD = 1

CHARACTERS = ['player', 'blighted', 'hound', 'devourer', 'gatebreaker', 'npc_hunter', 'npc_raider', 'npc_merchant', 'npc_gatherer', 'npc_warden']


def render_job(job):
    name, anim, d, t = job
    module = importlib.import_module(name)
    fn = module.ANIMS[anim][0]
    b = module.build(fn(t), DIR_YAW[d])
    width, height, anchor_x, anchor_y = module.CANVAS
    img = render(b.prims, module.MATERIALS, width, height, anchor_x, anchor_y, b.decals, b.smears, scale=module.SCALE)
    return img


def trim(img, anchor_x, anchor_y):
    alpha = img[:, :, 3] > 0
    if not alpha.any():
        return np.zeros((1, 1, 4), dtype=np.uint8), 0, 0
    ys = np.nonzero(alpha.any(axis=1))[0]
    xs = np.nonzero(alpha.any(axis=0))[0]
    y0, y1 = ys[0], ys[-1] + 1
    x0, x1 = xs[0], xs[-1] + 1
    # 캔버스 끝에 닿았으면 잘린 것
    if x0 == 0 or y0 == 0 or x1 == img.shape[1] or y1 == img.shape[0]:
        print('  [warn] frame touches canvas edge', x0, y0, x1, y1)
    return img[y0:y1, x0:x1], anchor_x - x0, anchor_y - y0


def pack(frames):
    # 높이 순 선반 쌓기. 폭은 정사각형에 가깝게
    area = sum((f.shape[0] + PAD) * (f.shape[1] + PAD) for f in frames)
    sheet_width = 256
    while sheet_width * sheet_width < area * 1.2:
        sheet_width *= 2
    order = sorted(range(len(frames)), key=lambda i: -frames[i].shape[0])
    places = [None] * len(frames)
    x = PAD
    y = PAD
    shelf = 0
    for i in order:
        h, w = frames[i].shape[:2]
        if x + w + PAD > sheet_width:
            x = PAD
            y += shelf + PAD
            shelf = 0
        places[i] = (x, y)
        x += w + PAD
        shelf = max(shelf, h)
    height = y + shelf + PAD
    size = 64
    while size < height:
        size *= 2
    sheet = np.zeros((size, sheet_width, 4), dtype=np.uint8)
    for i, (px, py) in enumerate(places):
        h, w = frames[i].shape[:2]
        sheet[py:py + h, px:px + w] = frames[i]
    return sheet, places


def bake(name, pool):
    begin = time.time()
    module = importlib.import_module(name)
    width, height, anchor_x, anchor_y = module.CANVAS
    jobs = []
    for anim, (fn, times) in module.ANIMS.items():
        for d in range(8):
            for t in times:
                jobs.append((name, anim, d, t))
    images = pool.map(render_job, jobs, chunksize=4)

    # 같은 그림은 한 번만 싣는다
    unique = []
    unique_offsets = []
    seen = {}
    job_frame = []
    for img in images:
        cut, ox, oy = trim(img, anchor_x, anchor_y)
        key = hashlib.md5(cut.tobytes() + bytes(str((cut.shape, ox, oy)), 'ascii')).hexdigest()
        if key not in seen:
            seen[key] = len(unique)
            unique.append(cut)
            unique_offsets.append((ox, oy))
        job_frame.append(seen[key])

    sheet, places = pack(unique)

    anims = {}
    cursor = 0
    phase = getattr(module, 'PHASE_ANIMS', {})
    periods = getattr(module, 'LOOP_PERIOD', {})
    strides = getattr(module, 'LOOP_STRIDE', {})
    for anim, (fn, times) in module.ANIMS.items():
        dirs = []
        for d in range(8):
            dirs.append(job_frame[cursor:cursor + len(times)])
            cursor += len(times)
        entry = dict(mode='once', times=[round(t, 4) for t in times], dirs=dirs)
        if anim in phase:
            entry['times'] = [round(phase_to_time(phase[anim], t), 4) for t in times]
        elif anim in periods:
            entry['mode'] = 'loop'
            entry['period'] = periods[anim]
        elif anim in strides:
            entry['mode'] = 'stride'
            entry['period'] = strides[anim]
        anims[anim] = entry

    frames = []
    for i, cut in enumerate(unique):
        h, w = cut.shape[:2]
        px, py = places[i]
        ox, oy = unique_offsets[i]
        frames.append([int(px), int(py), int(w), int(h), int(ox), int(oy)])

    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray(sheet, 'RGBA').save(os.path.join(OUT_DIR, name + '.png'), optimize=True)
    manifest = dict(name=name, width=int(sheet.shape[1]), height=int(sheet.shape[0]), frames=frames, anims=anims)
    with open(os.path.join(OUT_DIR, name + '.json'), 'w', encoding='utf-8', newline=chr(10)) as f:
        json.dump(manifest, f, separators=(',', ':'))
    print(name, 'jobs', len(jobs), 'unique', len(unique), 'sheet', sheet.shape[1], 'x', sheet.shape[0], 'sec', round(time.time() - begin, 1))


def main():
    names = sys.argv[1:] or CHARACTERS
    with multiprocessing.Pool(max(1, multiprocessing.cpu_count() - 1)) as pool:
        for name in names:
            bake(name, pool)


if __name__ == '__main__':
    main()
