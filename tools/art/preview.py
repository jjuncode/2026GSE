# -*- coding: utf-8 -*-
# 확인용. python preview.py 캐릭터 동작[,동작] 방향[,방향] 출력.png
import importlib
import sys
import time

import numpy as np
from PIL import Image

from pixel3d import render

DIR_YAW = [-45.0 + 45.0 * i for i in range(8)]  # 0 = 화면 오른쪽, 시계 방향


def main():
    module = importlib.import_module(sys.argv[1])
    anim_names = sys.argv[2].split(',')
    dirs = [int(x) for x in sys.argv[3].split(',')]
    out_path = sys.argv[4]
    zoom = 4
    if len(sys.argv) > 5:
        zoom = int(sys.argv[5])
    only_first = len(sys.argv) > 6

    width, height, anchor_x, anchor_y = module.CANVAS
    rows = []
    begin = time.time()
    count = 0
    for name in anim_names:
        fn, times = module.ANIMS[name]
        for d in dirs:
            row = []
            for t in times[:1] if only_first else times:
                b = module.build(fn(t), DIR_YAW[d])
                img = render(b.prims, module.MATERIALS, width, height, anchor_x, anchor_y, b.decals, scale=module.SCALE)
                row.append(img)
                count += 1
            rows.append(row)
    elapsed = time.time() - begin

    if only_first:
        rows = [[r[0] for r in rows]]
    cols = max(len(r) for r in rows)
    sheet = np.zeros((height * len(rows), width * cols, 4), dtype=np.uint8)
    sheet[:, :, :3] = (58, 54, 50)
    sheet[:, :, 3] = 255
    # 바닥 칸 무늬
    for ri, row in enumerate(rows):
        for ci, img in enumerate(row):
            y0 = ri * height
            x0 = ci * width
            cell = sheet[y0:y0 + height, x0:x0 + width]
            cell[anchor_y, :, :3] = (72, 68, 62)
            alpha = img[:, :, 3:4] / 255.0
            cell[:, :, :3] = (cell[:, :, :3] * (1 - alpha) + img[:, :, :3] * alpha).astype(np.uint8)
    big = Image.fromarray(sheet, 'RGBA').resize((sheet.shape[1] * zoom, sheet.shape[0] * zoom), Image.NEAREST)
    big.save(out_path)
    print('frames', count, 'sec', round(elapsed, 2), 'per frame', round(elapsed / max(count, 1), 3))


if __name__ == '__main__':
    main()
