# -*- coding: utf-8 -*-
import os

PALETTE = [
    ('k', (26, 20, 16)), ('a', (176, 180, 188)), ('A', (112, 116, 126)), ('d', (60, 62, 70)),
    ('R', (130, 30, 28)), ('y', (226, 184, 72)), ('Y', (150, 112, 40)), ('b', (122, 84, 50)),
    ('B', (78, 52, 32)), ('s', (206, 160, 120)), ('g', (118, 140, 94)), ('G', (72, 90, 60)),
    ('e', (255, 70, 40)), ('z', (96, 82, 66)), ('w', (232, 224, 200)), ('W', (166, 156, 132)),
    ('p', (172, 86, 186)), ('P', (112, 50, 126)), ('m', (66, 26, 78)), ('o', (255, 152, 42)),
    ('O', (255, 228, 132)), ('i', (238, 240, 248)), ('I', (170, 176, 190)), ('F', (62, 190, 104)),
    ('E', (136, 255, 166)), ('v', (246, 246, 252)), ('V', (192, 194, 214)), ('j', (104, 172, 255)),
    ('J', (48, 98, 212)), ('#', (255, 255, 255)), ('l', (104, 138, 62)), ('L', (64, 94, 40)),
    ('f', (38, 60, 28)), ('t', (144, 108, 68)), ('T', (98, 72, 46)), ('c', (168, 150, 118)),
    ('C', (118, 102, 80)), ('x', (78, 66, 52)), ('q', (178, 156, 92)), ('Q', (122, 106, 60)),
    ('u', (92, 98, 54)), ('n', (100, 124, 68)), ('N', (64, 82, 44)),
]

PLAYER_TOP = [
    "....kkkkk....",
    "...kaaaaak...",
    "...kaAAAak...",
    "...kAkkkAk...",
    "...kAAAAAk...",
    "....kdddk....",
    "..kkaaaaakk..",
    ".kRkaAAAakak.",
    ".kRkaAyAakak.",
    ".kRkaAAAaksk.",
    ".kRkdaaadkk..",
    ".kRkbbybbk...",
    ".kRkdAAAdk...",
    ".kRRkAAAk....",
]
PLAYER_STAND = PLAYER_TOP + [
    "..kRkAkAk....",
    "...kkAkAk....",
    "....kAkAk....",
    "....kAkAk....",
    "....kdkdk....",
    "...kddkddk...",
    "...kkk.kkk...",
]
PLAYER_WALK = PLAYER_TOP + [
    "..kRkAkAk....",
    "...kAk.kAk...",
    "..kAk...kAk..",
    "..kAk...kAk..",
    "..kdk...kdk..",
    ".kddk...kddk.",
    ".kkk.....kkk.",
]

ZOMBIE_TOP = [
    "...kkkk.....",
    "..kggggk....",
    "..kgGegk....",
    "..kggGgk....",
    "...kGGk.....",
    "..kzzzzkkk..",
    ".kzzgzzzggk.",
    ".kzzzzzk.kgk",
    ".kzGzzzk....",
    "..kzzzzk....",
    "..kzzGzk....",
    "..kzzzzk....",
    "..kGGGGk....",
]
ZOMBIE_STAND = ZOMBIE_TOP + [
    "..kGk.kGk...",
    "..kGk.kGk...",
    "..kgk.kgk...",
    "..kGk.kGk...",
    ".kGGk.kGGk..",
    ".kkk..kkk...",
]
ZOMBIE_WALK = ZOMBIE_TOP + [
    "..kGk.kGk...",
    ".kGk...kGk..",
    ".kgk...kgk..",
    ".kGk...kGk..",
    "kGGk...kGGk.",
    "kkk.....kkk.",
]

SKELETON_TOP = [
    "...kkkk....",
    "..kwwwwk...",
    "..kwewek...",
    "..kwwwwk...",
    "...kWkWk...",
    "....kwk....",
    "..kkwwwkk..",
    ".kwkWwWkwk.",
    ".kwkwWwkwk.",
    ".kwkWwWkwk.",
    ".kWk.w.kWk.",
    ".kwk.w.kwk.",
    "..k.kWk.k..",
    "...kWwWk...",
]
SKELETON_STAND = SKELETON_TOP + [
    "...kw.wk...",
    "...kw.wk...",
    "...kW.Wk...",
    "...kw.wk...",
    "..kww.wwk..",
    "..kkk.kkk..",
]
SKELETON_WALK = SKELETON_TOP + [
    "...kw.wk...",
    "..kw...wk..",
    "..kW...Wk..",
    "..kw...wk..",
    ".kww...wwk.",
    ".kkk...kkk.",
]

BOSS_TOP = [
    ".............kk.......",
    "............kPk.......",
    "......kkk..kPk........",
    ".....kpppkkppk........",
    "....kppPPppppkk.......",
    "...kppPmmPppepk.......",
    "..kppPmmmmPpppk.......",
    "..kpPmmmmmmPppkkk.....",
    ".kppmmmmmmmmPppppk....",
    ".kpPmmmmmmmmmPpPPk....",
    ".kpPmmmmmmmmmmPk.kk...",
    "kppPmmmmmmmmmPpk.kpk..",
    "kpPmmmmmmmmmPppkkppk..",
    "kpPmmmmmmmmPpppppPk...",
    ".kPmmmmmmmPPkkkkkk....",
    ".kPPmmmmmPPk..........",
    "..kPPPPPPPk...........",
]
BOSS_STAND = BOSS_TOP + [
    "..kPk.kPPk............",
    "..kPk..kPk............",
    ".kPPk..kPPk...........",
    ".kmmk..kmmk...........",
    ".kkk...kkk............",
]
BOSS_WALK = BOSS_TOP + [
    "..kPk..kPk............",
    ".kPk....kPk...........",
    ".kPk....kPPk..........",
    "kmmk....kmmk..........",
    "kkk......kkk..........",
]

SWORD = [
    "..i..",
    ".iIi.",
    ".iIi.",
    ".iIi.",
    ".iIi.",
    ".iIi.",
    ".iIi.",
    ".iIi.",
    ".iIi.",
    "yYyYy",
    "..b..",
    "..B..",
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
    ".FEF.",
    "FEEEF",
    "EE#EE",
    "FEEEF",
    ".FEF.",
]
DUST = [
    "..v..",
    ".vVv.",
    "vV#Vv",
    ".vVv.",
    "..v..",
]
FRAGMENT = [
    "..j..",
    ".jJj.",
    "jJ#Jj",
    ".jJj.",
    "..j..",
]
CORE = [
    "..o..",
    ".oOo.",
    "oO#Oo",
    ".oOo.",
    "..o..",
]

PALM = [
    "........ll.........",
    "...ll..lLLl..ll....",
    "..lLLl.lLLl.lLLl...",
    ".lLffLllLLllLffLl..",
    "lLf..fLLLLLLf..fLl.",
    "lf....fLLTLf....fl.",
    "l.....lLfTfLl.....l",
    "......lf.T.fl......",
    ".....lf..T..fl.....",
    "....lf...t...fl....",
    "....f....tT...f....",
    ".........tT........",
    ".........tT........",
    "........ktT........",
    "........tTk........",
    "........tT.........",
    "........tTk........",
    "........ktT........",
    "........tT.........",
    "........tTk........",
    "........ktT........",
    "........tT.........",
    "........tTk........",
    ".......ktT.........",
    ".......tTk.........",
    ".......tT..........",
    ".......tTk.........",
    ".......ktT.........",
    ".......tT..........",
    ".......tTk.........",
    "......ktTT.........",
    "......kttTk........",
    ".....kttTTTk.......",
]
ROCK = [
    "...kkkk....",
    "..kcccCkk..",
    ".kccccCCCk.",
    "kcccCCCCxCk",
    "kCCCCCxxxxk",
    "kxCCxxxxxxk",
    ".kkkkkkkkk.",
]
PEBBLE = [
    ".kkkk..",
    "kccCCk.",
    "kcCCxxk",
    "kCxxxxk",
    ".kkkkk.",
]
BUSH = [
    "..q...Q..q.",
    ".qQ..qQ.qQ.",
    "..qQ.QqqQ..",
    ".Q.qQQuQ.q.",
    "..QquuuQq..",
    ".QuuuQuuuQ.",
    "..uuuuuuu..",
]
GRASS = [
    ".q..q..",
    "..qQ.q.",
    ".QqQq..",
    "..uQu..",
    ".uuuuu.",
]
REEDS = [
    ".n...n.",
    ".N.n.N.",
    ".n.N.n.",
    "nN.n.Nn",
    "Nn.N.nN",
    ".N.n.N.",
    ".n.N.n.",
    "nN.n.Nn",
    ".NnNnN.",
    ".nNnNn.",
    "NNnNnNN",
    ".NNNNN.",
]
BRAZIER = [
    "...O...",
    "..OoO..",
    ".oOOoo.",
    ".ooOoo.",
    "kYyyyYk",
    ".kYYYk.",
    "..kAk..",
    "..kAk..",
    ".kAAAk.",
    "kkkkkkk",
]
BONES = [
    "w.....w",
    ".wWWWw.",
    "w.....w",
]

SPRITES = [
    ('SPRITE_PLAYER', [PLAYER_STAND, PLAYER_WALK]),
    ('SPRITE_ZOMBIE', [ZOMBIE_STAND, ZOMBIE_WALK]),
    ('SPRITE_SKELETON', [SKELETON_STAND, SKELETON_WALK]),
    ('SPRITE_BOSS', [BOSS_STAND, BOSS_WALK]),
    ('SPRITE_SWORD', [SWORD]),
    ('SPRITE_GOLD', [GOLD]),
    ('SPRITE_EXP_ORB', [EXP_ORB]),
    ('SPRITE_DUST', [DUST]),
    ('SPRITE_FRAGMENT', [FRAGMENT]),
    ('SPRITE_CORE', [CORE]),
    ('SPRITE_PALM', [PALM]),
    ('SPRITE_ROCK', [ROCK]),
    ('SPRITE_PEBBLE', [PEBBLE]),
    ('SPRITE_BUSH', [BUSH]),
    ('SPRITE_GRASS', [GRASS]),
    ('SPRITE_REEDS', [REEDS]),
    ('SPRITE_BRAZIER', [BRAZIER]),
    ('SPRITE_BONES', [BONES]),
]


def main():
    keys = {k for k, _ in PALETTE} | {'.'}
    errors = 0
    for name, frames in SPRITES:
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

    used = set()
    for _, frames in SPRITES:
        for frame in frames:
            for row in frame:
                used |= set(row)
    used.discard('.')
    print('unused palette:', sorted(keys - used - {'.'}))
    print('errors', errors)
    if errors:
        return

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
    for name, frames in SPRITES:
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


main()
