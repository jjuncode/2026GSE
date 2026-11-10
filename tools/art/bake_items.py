# -*- coding: utf-8 -*-
# 새 아이템 그림판 굽기. python bake_items.py [미리보기.png]
# 소품 그림판( props.png )은 SpriteId.h 번호와 묶여 있어 건드리지 않고, 이름으로 찾는 그림을 여기 따로 싣는다.
# 게임은 Art/items.json 의 이름으로 그림 번호를 찾는다 ( ItemData.json "icon": "ext:이름" )
import json
import os
import sys
import time

from PIL import Image

import items
from bake_props import CANVAS, crop_frames, fit_fixed, pack, preview
from pixel3d import render

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(ROOT, 'SimpleGame', 'Art')
ICON_SIZE = (16, 16)


def render_icon(build, scale):
    b = build()
    anchor_x = CANVAS // 2
    anchor_y = CANVAS - 24
    img = render(b.prims, items.ITEM_MATS, CANVAS, CANVAS, anchor_x, anchor_y, b.decals, scale=scale, outline=True)
    return img, (anchor_x, anchor_y)


def main():
    begin = time.time()
    sprites = []
    for name, build, scale in items.ICONS:
        img, anchor = render_icon(build, scale)
        frames = crop_frames([img], [anchor], False)
        print('  ', name, 'raw', frames[0].shape[1], 'x', frames[0].shape[0])
        sprites.append((name, [fit_fixed(f, ICON_SIZE) for f in frames]))
        items.ICON_CACHE[name] = sprites[-1][1][0]

    for name, frames in items.FLAT:
        sprites.append((name, frames()))

    sheet, places = pack(sprites)
    entries = []
    for si, (name, frames) in enumerate(sprites):
        h, w = frames[0].shape[:2]
        entries.append(dict(name=name, w=int(w), h=int(h), frames=[list(places[(si, fi)]) for fi in range(len(frames))]))

    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray(sheet, 'RGBA').save(os.path.join(OUT_DIR, 'items.png'), optimize=True)
    with open(os.path.join(OUT_DIR, 'items.json'), 'w', encoding='utf-8', newline=chr(10)) as f:
        json.dump(dict(width=int(sheet.shape[1]), height=int(sheet.shape[0]), sprites=entries), f, separators=(',', ':'))

    print('items', len(sprites), 'sheet', sheet.shape[1], 'x', sheet.shape[0], 'sec', round(time.time() - begin, 1))

    if len(sys.argv) > 1:
        preview(sprites, sys.argv[1])


if __name__ == '__main__':
    main()
