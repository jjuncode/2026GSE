# -*- coding: utf-8 -*-
# 캡처 BMP 를 가운데만 잘라 키워 한 장에 모은다.
# python contact.py 이름앞부분 출력.png [열 수] [시작 번호] [끝 번호]
# 예) python contact.py show_ C:/tmp/attack.png 5 0 15
import glob
import os
import re
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
LOGS = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'SimpleGame', 'Logs')

HALF_WIDTH = 150  # 창 픽셀. 플레이어 둘레만 본다
ABOVE = 130
BELOW = 60
ZOOM = 1.5


def number_of(path):
    found = re.findall(r'(\d+)', os.path.basename(path))
    if found:
        return int(found[-1])
    return 0


def main():
    if len(sys.argv) < 3:
        sys.exit('python contact.py 이름앞부분 출력.png [열 수] [시작] [끝]')

    prefix = sys.argv[1]
    out_path = sys.argv[2]
    columns = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    files = sorted(glob.glob(os.path.join(LOGS, 'cap_' + prefix + '*.bmp')), key=number_of)
    begin = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    end = int(sys.argv[5]) if len(sys.argv) > 5 else len(files)
    files = files[begin:end]
    if not files:
        sys.exit('no captures for ' + prefix)

    cell_w = int(HALF_WIDTH * 2 * ZOOM)
    cell_h = int((ABOVE + BELOW) * ZOOM)
    cells = []
    for path in files:
        image = Image.open(path)
        width, height = image.size
        box = (width // 2 - HALF_WIDTH, height // 2 - ABOVE, width // 2 + HALF_WIDTH, height // 2 + BELOW)
        cells.append(image.crop(box).resize((cell_w, cell_h), Image.NEAREST))

    rows = (len(cells) + columns - 1) // columns
    sheet = Image.new('RGB', (cell_w * columns, cell_h * rows))
    for index, cell in enumerate(cells):
        sheet.paste(cell, ((index % columns) * cell_w, (index // columns) * cell_h))
    sheet.save(out_path)
    print(len(cells), 'captures ->', out_path)


if __name__ == '__main__':
    main()
