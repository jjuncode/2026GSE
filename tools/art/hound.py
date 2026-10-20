# -*- coding: utf-8 -*-
# 재늑대. 잿빛 털, 목 갈기, 불씨 눈. 네 발 짐승이라 뼈대를 따로 둔다
import math

import numpy as np

from pixel3d import Material, pitch_matrix, roll_matrix, yaw_matrix
from rig import Builder, face_point, keyed, two_bone, vec
from timing import skill_windows

SCALE = 0.95

MATERIALS = {
    'fur': Material([(46, 42, 44), (74, 70, 70), (110, 106, 104), (150, 144, 138)], outline=(18, 16, 18)),
    'fur_dark': Material([(28, 26, 28), (46, 42, 44), (68, 64, 64)], outline=(12, 10, 12)),
    'ash': Material([(120, 116, 110), (166, 160, 152), (204, 198, 188)], outline=(50, 46, 44)),
    'mane': Material([(62, 58, 60), (96, 92, 90), (132, 126, 120), (168, 162, 154)], outline=(24, 22, 24)),
    'mouth': Material([(70, 24, 28), (110, 40, 40)], outline=(30, 8, 10)),
    'tooth': Material([(200, 194, 176), (240, 236, 220)], outline=(70, 64, 56), flat=True),
    'claw': Material([(30, 26, 26), (60, 54, 52)], outline=(10, 8, 8)),
}

EMBER = (255, 150, 60)

BASE = dict(
    root=(0.0, 0.0, 0.0),
    pitch=0.0,          # 몸 앞쪽 들기 ( + 위 )
    roll=0.0,
    neck=20.0,          # 목 들기
    head_pitch=0.0,
    head_yaw=0.0,
    jaw=4.0,
    tail=20.0,          # 꼬리 들기
    tail_side=0.0,
    fr=(0.4, 0.18, 0.0), fl=(0.4, -0.18, 0.0),     # 앞발
    hr=(-0.45, 0.18, 0.0), hl=(-0.45, -0.18, 0.0),  # 뒷발
    tumble=0.0,
    fall=0.0,           # 옆으로 쓰러짐
    pivot=(0.0, 0.0, 0.3),
)


def hpose(**changes):
    p = dict(BASE)
    for key, value in changes.items():
        assert key in BASE, key
        p[key] = value
    return p


def build(p, yaw_deg):
    b = Builder(yaw_deg, p['tumble'], p['pivot'], p['fall'])
    root = vec(*p['root'])
    body_rot = pitch_matrix(p['pitch']) @ roll_matrix(p['roll'])
    center = root + vec(0.0, 0.0, 0.64)
    shoulder = center + body_rot @ vec(0.36, 0.0, 0.04)
    hip = center + body_rot @ vec(-0.4, 0.0, 0.0)

    # 몸통. 가슴이 깊고 허리가 잘록하다
    b.ellipsoid(shoulder + body_rot @ vec(-0.02, 0.0, -0.04), (0.27, 0.22, 0.25), body_rot, 'fur')
    b.ellipsoid(center + body_rot @ vec(-0.12, 0.0, 0.0), (0.3, 0.17, 0.17), body_rot, 'fur')
    b.ellipsoid(hip + body_rot @ vec(0.02, 0.0, 0.0), (0.2, 0.18, 0.19), body_rot, 'fur')
    # 등줄기 짙은 털
    b.ellipsoid(center + body_rot @ vec(-0.05, 0.0, 0.12), (0.42, 0.09, 0.07), body_rot, 'fur_dark')
    # 배 밝은 털
    b.ellipsoid(shoulder + body_rot @ vec(0.06, 0.0, -0.18), (0.16, 0.14, 0.12), body_rot, 'ash')

    # 다리. 앞다리는 팔꿈치가 뒤로, 뒷다리는 발목이 뒤로 꺾인다
    legs = (
        (shoulder + body_rot @ vec(0.02, 0.14, -0.12), p['fr'], 0.3, 0.33, vec(-1.0, 0.0, 0.0)),
        (shoulder + body_rot @ vec(0.02, -0.14, -0.12), p['fl'], 0.3, 0.33, vec(-1.0, 0.0, 0.0)),
        (hip + body_rot @ vec(0.0, 0.13, -0.08), p['hr'], 0.3, 0.37, vec(-1.0, 0.0, 0.3)),
        (hip + body_rot @ vec(0.0, -0.13, -0.08), p['hl'], 0.3, 0.37, vec(-1.0, 0.0, 0.3)),
    )
    for index, (top, foot, upper, lower, pole) in enumerate(legs):
        ankle = root + vec(*foot) + vec(0.0, 0.0, 0.05)
        joint, end = two_bone(top, ankle, upper, lower, pole)
        if index < 2:
            b.capsule(top, joint, 0.1, 0.065, 'fur')
        else:
            b.capsule(top, joint, 0.13, 0.07, 'fur')
        b.capsule(joint, end, 0.058, 0.046, 'fur_dark')
        b.ellipsoid(end + vec(0.045, 0.0, -0.02), (0.07, 0.05, 0.04), np.eye(3), 'fur_dark')
        b.capsule(end + vec(0.09, 0.0, -0.03), end + vec(0.12, 0.0, -0.045), 0.015, 0.008, 'claw', 1)

    # 목과 갈기
    neck_rot = body_rot @ pitch_matrix(p['neck']) @ yaw_matrix(p['head_yaw'] * 0.5)
    neck_top = shoulder + neck_rot @ vec(0.26, 0.0, 0.1)
    b.capsule(shoulder + body_rot @ vec(0.05, 0.0, 0.0), neck_top, 0.16, 0.12, 'fur')
    b.ellipsoid(shoulder + neck_rot @ vec(0.1, 0.0, 0.08), (0.2, 0.23, 0.18), neck_rot, 'mane')
    for k in range(4):
        tuft = shoulder + neck_rot @ vec(-0.04 + k * 0.08, 0.0, 0.18 - k * 0.01)
        b.capsule(tuft, tuft + neck_rot @ vec(-0.1, 0.0, 0.07), 0.05, 0.015, 'mane')

    # 머리
    head_rot = neck_rot @ yaw_matrix(p['head_yaw'] * 0.5) @ pitch_matrix(p['head_pitch'] - p['neck'] * 0.8)
    skull = neck_top + head_rot @ vec(0.06, 0.0, 0.02)
    b.ellipsoid(skull, (0.14, 0.12, 0.11), head_rot, 'fur')
    snout_base = skull + head_rot @ vec(0.1, 0.0, -0.02)
    snout_tip = skull + head_rot @ vec(0.3, 0.0, -0.04)
    b.capsule(snout_base, snout_tip, 0.07, 0.045, 'fur')
    b.ellipsoid(snout_tip + head_rot @ vec(0.02, 0.0, 0.0), (0.035, 0.035, 0.03), head_rot, 'fur_dark')
    # 턱. 벌리면 붉은 입안과 이빨이 보인다
    jaw_rot = head_rot @ pitch_matrix(-p['jaw'])
    jaw_base = skull + head_rot @ vec(0.06, 0.0, -0.07)
    b.capsule(jaw_base, jaw_base + jaw_rot @ vec(0.22, 0.0, -0.01), 0.05, 0.035, 'fur')
    if p['jaw'] > 12.0:
        b.ellipsoid(jaw_base + jaw_rot @ vec(0.12, 0.0, 0.035), (0.11, 0.045, 0.03), jaw_rot, 'mouth')
        for side in (1.0, -1.0):
            b.capsule(snout_tip + head_rot @ vec(-0.02, 0.03 * side, -0.035), snout_tip + head_rot @ vec(-0.02, 0.03 * side, -0.075), 0.012, 0.006, 'tooth', 1)
            low_tip = jaw_base + jaw_rot @ vec(0.2, 0.025 * side, 0.02)
            b.capsule(low_tip, low_tip + jaw_rot @ vec(0.0, 0.0, 0.035), 0.012, 0.006, 'tooth', 1)
    # 귀
    for side in (1.0, -1.0):
        ear = skull + head_rot @ vec(-0.04, 0.07 * side, 0.09)
        b.capsule(ear, ear + head_rot @ vec(-0.06, 0.02 * side, 0.12), 0.045, 0.01, 'fur_dark')
        b.decal(face_point(skull, np.array([0.14, 0.12, 0.11]), head_rot, (0.85, 0.5 * side, 0.22)), EMBER)

    # 꼬리
    tail_rot = body_rot @ yaw_matrix(p['tail_side']) @ pitch_matrix(-p['tail'])
    tail_base = hip + body_rot @ vec(-0.15, 0.0, 0.06)
    tail_mid = tail_base + tail_rot @ vec(-0.22, 0.0, 0.0)
    tail_end = tail_mid + tail_rot @ pitch_matrix(-p['tail'] * 0.5) @ vec(-0.22, 0.0, -0.06)
    b.capsule(tail_base, tail_mid, 0.07, 0.08, 'fur')
    b.capsule(tail_mid, tail_end, 0.08, 0.03, 'fur_dark')
    return b


# ------------------------------------------------------------ 동작

def idle(t):
    a = t * 2.0 * math.pi
    return hpose(root=(0.0, 0.0, math.sin(a) * 0.01), pitch=2.0, neck=24.0 + math.sin(a) * 3.0,
                 head_pitch=-10.0, tail=10.0, tail_side=math.sin(a) * 18.0, jaw=6.0 + math.sin(a) * 4.0)


STANCE = idle(0.0)


def run(t):
    # 뛰기. 앞발 둘, 뒷발 둘이 조금 어긋나며 같이 움직인다
    a = t * 2.0 * math.pi
    stride = 0.4

    def leg(phase, base_x):
        x = base_x + math.cos(a + phase) * stride
        z = max(0.0, math.sin(a + phase)) * 0.2
        return x, z

    fr_x, fr_z = leg(0.0, 0.42)
    fl_x, fl_z = leg(0.5, 0.42)
    hr_x, hr_z = leg(math.pi, -0.42)
    hl_x, hl_z = leg(math.pi + 0.5, -0.42)
    return hpose(root=(0.0, 0.0, -0.02 + math.sin(a * 2.0) * 0.035), pitch=math.sin(a) * 7.0, neck=14.0 - math.sin(a) * 6.0,
                 head_pitch=-6.0, tail=6.0, tail_side=0.0, jaw=14.0,
                 fr=(fr_x, 0.15, fr_z), fl=(fl_x, -0.15, fl_z), hr=(hr_x, 0.15, hr_z), hl=(hl_x, -0.15, hl_z))


CROUCH = hpose(root=(-0.08, 0.0, -0.16), pitch=-6.0, neck=6.0, head_pitch=-14.0, tail=-10.0, jaw=20.0,
               fr=(0.36, 0.18, 0.0), fl=(0.4, -0.18, 0.0), hr=(-0.38, 0.18, 0.0), hl=(-0.36, -0.18, 0.0))


def bite_keys():
    start, end = skill_windows(11)[0]
    lunge = hpose(root=(0.3, 0.0, -0.04), pitch=-4.0, neck=4.0, head_pitch=-8.0, tail=30.0, jaw=50.0,
                  fr=(0.66, 0.16, 0.0), fl=(0.56, -0.16, 0.06), hr=(-0.24, 0.17, 0.0), hl=(-0.3, -0.17, 0.0))
    snap = dict(lunge)
    snap['jaw'] = 2.0
    snap['root'] = (0.34, 0.0, -0.06)
    snap['head_pitch'] = -12.0
    return [(0.0, STANCE), (start * 0.75, CROUCH), (start, lunge), (end, snap), (end + 0.12, snap), (1.0, STANCE)]


def pounce_keys():
    start, end = skill_windows(12)[0]
    air = hpose(root=(0.3, 0.0, 0.42), pitch=14.0, neck=26.0, head_pitch=-20.0, tail=20.0, jaw=60.0,
                fr=(0.75, 0.16, 0.5), fl=(0.7, -0.16, 0.48), hr=(-0.6, 0.17, 0.35), hl=(-0.62, -0.17, 0.35))
    land = hpose(root=(0.5, 0.0, -0.14), pitch=-12.0, neck=0.0, head_pitch=-14.0, tail=30.0, jaw=4.0,
                 fr=(0.86, 0.18, 0.0), fl=(0.8, -0.18, 0.0), hr=(0.0, 0.17, 0.0), hl=(-0.06, -0.17, 0.0))
    deep = dict(CROUCH)
    deep['root'] = (-0.14, 0.0, -0.22)
    return [(0.0, STANCE), (start * 0.5, deep), (start * 0.8, deep), (start, air), (end, land), (end + 0.1, land), (1.0, STANCE)]


def keys_anim(keys, extra):
    def anim(t):
        return keyed(keys, t)

    times = set(round(k[0], 3) for k in keys[:-1])
    for value in extra:
        times.add(round(value, 3))
    return anim, sorted(times)


def hit(t):
    flinch = hpose(root=(-0.1, 0.0, -0.04), pitch=8.0, neck=36.0, head_pitch=20.0, tail=-20.0, jaw=26.0,
                   fr=(0.32, 0.18, 0.0), fl=(0.36, -0.18, 0.0), hr=(-0.5, 0.18, 0.0), hl=(-0.48, -0.18, 0.0))
    return keyed([(0.0, flinch), (0.4, flinch), (1.0, STANCE)], t)


def stun(t):
    a = t * 2.0 * math.pi
    return hpose(root=(0.0, 0.0, -0.1), pitch=-8.0, roll=math.sin(a) * 6.0, neck=-10.0, head_pitch=-10.0,
                 head_yaw=math.sin(a) * 20.0, tail=-30.0, jaw=10.0,
                 fr=(0.36, 0.2, 0.0), fl=(0.4, -0.2, 0.0), hr=(-0.42, 0.2, 0.0), hl=(-0.4, -0.2, 0.0))


def parried(t):
    knocked = hpose(root=(-0.16, 0.0, 0.0), pitch=18.0, neck=40.0, head_pitch=26.0, tail=-10.0, jaw=30.0,
                    fr=(0.26, 0.18, 0.12), fl=(0.3, -0.18, 0.06), hr=(-0.5, 0.18, 0.0), hl=(-0.5, -0.18, 0.0))
    return keyed([(0.0, knocked), (0.6, knocked), (1.0, STANCE)], t)


def death(t):
    down = hpose(root=(0.0, 0.0, -0.4), pitch=0.0, neck=-6.0, head_pitch=-6.0, tail=-20.0, jaw=20.0,
                 fr=(0.5, 0.3, 0.0), fl=(0.46, -0.1, 0.0), hr=(-0.5, 0.3, 0.0), hl=(-0.46, -0.1, 0.0),
                 fall=80.0, pivot=(0.0, -0.1, 0.0))
    p = keyed([(0.0, STANCE), (0.25, hit(0.0)), (0.75, down), (1.0, down)], t)
    p['pivot'] = (0.0, -0.1, 0.0)
    return p


SKILL_11 = keys_anim(bite_keys(), [(skill_windows(11)[0][0] + skill_windows(11)[0][1]) * 0.5])
SKILL_12 = keys_anim(pounce_keys(), [(skill_windows(12)[0][0] + skill_windows(12)[0][1]) * 0.5, skill_windows(12)[0][0] * 0.9])

ANIMS = {
    'idle': (idle, [i / 4.0 for i in range(4)]),
    'run': (run, [i / 6.0 for i in range(6)]),
    'skill_11': SKILL_11,
    'skill_12': SKILL_12,
    'hit': (hit, [0.0, 0.45, 0.75]),
    'stun': (stun, [i / 4.0 for i in range(4)]),
    'parried': (parried, [0.0, 0.6, 0.85]),
    'death': (death, [0.0, 0.25, 0.45, 0.65, 1.0]),
}

LOOP_PERIOD = {'idle': 1.2, 'stun': 0.9}
LOOP_STRIDE = {'run': 2.6}

CANVAS = (112, 88, 56, 60)
