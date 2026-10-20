# -*- coding: utf-8 -*-
# 타락자. 굽은 등, 잿빛 보라 살, 너덜한 두건, 불씨 눈, 손칼
import math

import numpy as np

from pixel3d import Material, pitch_matrix, yaw_matrix
from rig import Builder, face_point, keyed, pose, skill_times, solve_humanoid, strike_keys, vec
from timing import skill_windows

BODY = dict(
    hip_z=0.8, hip_w=0.1, thigh=0.4, shin=0.42, ankle_z=0.06,
    spine=0.44, shoulder_w=0.21, upper_arm=0.3, forearm=0.3,
    head_r=0.17,
)

SCALE = 0.95

MATERIALS = {
    'flesh': Material([(54, 46, 64), (82, 72, 94), (116, 106, 124), (150, 140, 156)], outline=(22, 16, 28)),
    'rag': Material([(30, 24, 30), (50, 40, 46), (72, 60, 62), (96, 84, 80)], outline=(14, 10, 14)),
    'hood': Material([(40, 22, 26), (66, 34, 36), (96, 48, 46)], outline=(18, 8, 10)),
    'bone': Material([(120, 112, 96), (170, 162, 142), (216, 208, 188)], outline=(50, 44, 36)),
    'rust': Material([(62, 36, 26), (98, 58, 38), (140, 86, 54), (176, 120, 80)], outline=(30, 16, 12), metal=True),
    'grip': Material([(40, 28, 24), (70, 48, 36)], outline=(18, 12, 10)),
    'ember': Material([(255, 120, 40), (255, 200, 110)], outline=(120, 30, 10), emissive=True),
}

EMBER = (255, 150, 60)


def add_cleaver(b, hand, blade_dir, roll_deg):
    from pixel3d import basis_from_axis
    rot = basis_from_axis(blade_dir) @ yaw_matrix(roll_deg)
    b.capsule(hand - blade_dir * 0.08, hand + blade_dir * 0.08, 0.032, 0.032, 'grip', 1)
    # 넓적한 칼날. 등이 두껍고 끝이 넓다
    b.box(hand + blade_dir * 0.36, (0.085, 0.016, 0.28), rot, 0.01, 'rust', 2)
    b.box(hand + blade_dir * 0.5 + rot @ vec(0.03, 0.0, 0.0), (0.075, 0.016, 0.16), rot, 0.015, 'rust', 2)


def build(p, yaw_deg):
    b = Builder(yaw_deg + p['spin'], p['tumble'], p['pivot'])
    s = solve_humanoid(p, BODY)
    up = vec(0.0, 0.0, 1.0)
    cr = s.chest_rot

    for hip, knee, ankle in ((s.hip_r, s.knee_r, s.ankle_r), (s.hip_l, s.knee_l, s.ankle_l)):
        b.capsule(hip, knee, 0.075, 0.06, 'rag')
        b.capsule(knee, ankle, 0.058, 0.045, 'flesh')
        b.ellipsoid(ankle + s.toe_dir * 0.06 - up * 0.03, (0.1, 0.055, 0.04), s.hips_rot, 'flesh')

    # 허리 누더기 ( 자락이 다리 위로 늘어진다 )
    b.ellipsoid(s.pelvis, (0.12, 0.16, 0.11), s.hips_rot, 'rag')
    b.ellipsoid(s.pelvis + s.hips_rot @ vec(0.0, 0.0, -0.1), (0.15, 0.18, 0.14), s.hips_rot @ pitch_matrix(p['lean'] * 0.4), 'rag')

    # 앙상한 몸통. 갈비뼈가 비친다
    b.ellipsoid(s.belly, (0.11, 0.14, 0.13), cr, 'flesh')
    b.ellipsoid(s.chest, (0.16, 0.22, 0.16), cr, 'flesh')
    for k in range(3):
        b.decal(s.chest + cr @ vec(0.14, 0.06, 0.04 - k * 0.05), (64, 54, 76))
        b.decal(s.chest + cr @ vec(0.14, -0.06, 0.04 - k * 0.05), (64, 54, 76))
    # 등의 혹 ( 굽은 등 )
    b.ellipsoid(s.chest + cr @ vec(-0.1, 0.0, 0.08), (0.14, 0.18, 0.14), cr, 'flesh')

    # 어깨 넝마 + 뼈 견갑. 등 뒤로 찢어진 자락이 길이가 제각각으로 늘어진다
    b.ellipsoid(s.neck + cr @ vec(-0.04, 0.0, -0.08), (0.2, 0.27, 0.11), cr, 'hood')
    for k, (side, length) in enumerate(((-0.15, 0.5), (-0.05, 0.62), (0.05, 0.44), (0.15, 0.56))):
        top = s.neck + cr @ vec(-0.17, side, -0.1)
        rag_rot = cr @ pitch_matrix(-6.0 - k * 2.0)
        b.box(top + rag_rot @ vec(0.0, 0.0, -length * 0.5), (0.025, 0.06, length * 0.5), rag_rot, 0.02, 'hood')
    b.ellipsoid(s.shoulder_r + cr @ vec(0.0, 0.0, 0.04), (0.1, 0.09, 0.07), cr, 'bone')

    for shoulder, elbow, hand in ((s.shoulder_r, s.elbow_r, s.hand_r), (s.shoulder_l, s.elbow_l, s.hand_l)):
        b.capsule(shoulder, elbow, 0.058, 0.048, 'flesh')
        b.capsule(elbow, hand, 0.05, 0.042, 'flesh')
        b.ellipsoid(hand, (0.06, 0.06, 0.055), np.eye(3), 'flesh')
    # 왼손 손톱
    claw_dir = s.hand_l - s.elbow_l
    claw_dir = claw_dir / max(np.linalg.norm(claw_dir), 1e-6)
    b.capsule(s.hand_l, s.hand_l + claw_dir * 0.12, 0.03, 0.008, 'bone', 1)

    # 머리. 두건 아래로 얼굴이 길게 나오고 눈이 불씨처럼 탄다
    hr = s.head_rot
    head = s.head + hr @ vec(0.05, 0.0, -0.03)
    face_c = head + hr @ vec(0.06, 0.0, -0.04)
    face_r = np.array([0.13, 0.115, 0.15])
    b.ellipsoid(face_c, face_r, hr, 'flesh')
    b.ellipsoid(head + hr @ vec(-0.03, 0.0, 0.03), (0.18, 0.18, 0.18), hr, 'hood')
    b.capsule(head + hr @ vec(-0.1, 0.0, 0.06), head + hr @ vec(-0.3, 0.0, -0.1), 0.09, 0.02, 'hood')
    for side in (1.0, -1.0):
        b.decal(face_point(face_c, face_r, hr, (1.0, 0.4 * side, 0.1)), EMBER)
    b.decal(face_point(face_c, face_r, hr, (1.0, 0.0, -0.45)), (40, 20, 30))

    add_cleaver(b, s.hand_r, s.sword_dir, p['sword_roll'])
    return b


# ------------------------------------------------------------ 동작

def idle(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.05 + math.sin(a) * 0.015), lean=26.0 + math.sin(a) * 2.0, side=math.sin(a * 0.5) * 3.0,
                head_pitch=10.0, head_yaw=math.sin(a) * 6.0,
                foot_r=(0.06, 0.17, 0.0), foot_l=(-0.08, -0.16, 0.0),
                hand_r=(60.0, 0.3, 0.68), hand_l=(-60.0, 0.3, 0.66 + math.sin(a) * 0.02),
                sword_yaw=50.0, sword_pitch=-55.0)


STANCE = idle(0.0)


def walk(t):
    a = t * 2.0 * math.pi
    swing = math.cos(a)
    return pose(root=(0.02, 0.0, -0.05 + abs(swing) * 0.03 - 0.03), lean=28.0, side=swing * 5.0, twist=swing * 8.0,
                head_pitch=10.0,
                foot_r=(swing * 0.3, 0.15, max(0.0, -math.sin(a)) * 0.15),
                foot_l=(-swing * 0.3, -0.15, max(0.0, math.sin(a)) * 0.15),
                hand_r=(70.0 - swing * 15.0, 0.3, 0.68), hand_l=(-70.0 - swing * 25.0, 0.3, 0.66),
                sword_yaw=80.0, sword_pitch=-50.0)


def chop(index):
    return pose(root=(-0.04, 0.0, -0.04), lean=8.0, twist=-25.0, head_pitch=0.0,
                foot_r=(0.0, 0.17, 0.0), foot_l=(-0.12, -0.16, 0.0),
                hand_r=(-30.0, 0.2, 1.55), hand_l=(-60.0, 0.32, 0.9),
                sword_yaw=-160.0, sword_pitch=50.0)


def chopped(index):
    return pose(root=(0.18, 0.0, -0.14), lean=40.0, twist=20.0, head_pitch=10.0,
                foot_r=(0.36, 0.17, 0.0), foot_l=(-0.2, -0.16, 0.0),
                hand_r=(30.0, 0.55, 0.5), hand_l=(-80.0, 0.3, 0.7),
                sword_yaw=30.0, sword_pitch=-50.0)


def side_raise(index):
    return pose(root=(-0.04, 0.0, -0.06), lean=18.0, twist=40.0,
                foot_r=(0.0, 0.18, 0.0), foot_l=(-0.12, -0.16, 0.0),
                hand_r=(110.0, 0.4, 1.05), hand_l=(-40.0, 0.3, 0.86),
                sword_yaw=150.0, sword_pitch=10.0)


def side_hit(index):
    return pose(root=(0.14, 0.0, -0.1), lean=30.0, twist=-40.0,
                foot_r=(0.3, 0.18, 0.0), foot_l=(-0.18, -0.16, 0.0),
                hand_r=(-70.0, 0.52, 0.86), hand_l=(-110.0, 0.3, 0.76),
                sword_yaw=-110.0, sword_pitch=-15.0)


def stab_raise(index):
    return pose(root=(-0.1, 0.0, -0.16), lean=30.0, twist=30.0,
                foot_r=(-0.12, 0.18, 0.0), foot_l=(0.14, -0.16, 0.0),
                hand_r=(130.0, 0.24, 0.8), hand_l=(10.0, 0.34, 0.86),
                sword_yaw=0.0, sword_pitch=-5.0)


def stab_hit(index):
    return pose(root=(0.24, 0.0, -0.2), lean=42.0, twist=-15.0,
                foot_r=(0.46, 0.16, 0.0), foot_l=(-0.34, -0.16, 0.0),
                hand_r=(4.0, 0.7, 0.8), hand_l=(-110.0, 0.3, 0.7),
                sword_yaw=0.0, sword_pitch=-8.0)


def combo_raise(index):
    if index % 2 == 0:
        return side_raise(index)
    p = side_hit(index)
    p['root'] = (0.0, 0.0, -0.06)
    return p


def combo_hit(index):
    if index % 2 == 0:
        return side_hit(index)
    p = side_raise(index)
    p['root'] = (0.16, 0.0, -0.1)
    p['hand_r'] = (100.0, 0.5, 0.86)
    p['sword_yaw'] = 130.0
    if index == 3:
        return chopped(index)
    return p


def make_skill(skill_id, raise_pose, hit_pose):
    keys = strike_keys(skill_windows(skill_id), raise_pose, hit_pose, STANCE)

    def anim(t):
        return keyed(keys, t)

    return anim, skill_times(keys, skill_windows(skill_id))


def hit(t):
    flinch = pose(root=(-0.1, 0.0, -0.04), lean=4.0, head_pitch=26.0, side=-8.0,
                  foot_r=(0.04, 0.17, 0.0), foot_l=(-0.16, -0.16, 0.0),
                  hand_r=(90.0, 0.32, 0.86), hand_l=(-100.0, 0.34, 0.92),
                  sword_yaw=130.0, sword_pitch=-20.0)
    return keyed([(0.0, flinch), (0.4, flinch), (1.0, STANCE)], t)


def stun(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.12), lean=40.0, side=math.sin(a) * 10.0, head_pitch=-20.0, head_yaw=math.sin(a) * 18.0,
                foot_r=(0.06, 0.17, 0.0), foot_l=(-0.06, -0.17, 0.0),
                hand_r=(40.0, 0.24, 0.5), hand_l=(-40.0, 0.24, 0.5),
                sword_yaw=30.0, sword_pitch=-75.0)


def parried(t):
    knocked = pose(root=(-0.14, 0.0, -0.02), lean=-10.0, head_pitch=24.0,
                   foot_r=(0.0, 0.17, 0.0), foot_l=(-0.24, -0.16, 0.0),
                   hand_r=(70.0, 0.22, 1.5), hand_l=(-90.0, 0.34, 0.96),
                   sword_yaw=160.0, sword_pitch=65.0)
    return keyed([(0.0, knocked), (0.6, knocked), (1.0, STANCE)], t)


def death(t):
    kneel = pose(root=(0.0, 0.0, -0.3), lean=44.0, head_pitch=-30.0,
                 foot_r=(0.2, 0.16, 0.0), foot_l=(-0.3, -0.16, 0.0),
                 hand_r=(40.0, 0.3, 0.36), hand_l=(-50.0, 0.28, 0.42),
                 sword_yaw=40.0, sword_pitch=-75.0)
    fall = dict(kneel)
    fall['tumble'] = -84.0
    fall['lean'] = 10.0
    fall['hand_r'] = (70.0, 0.5, 0.5)
    fall['sword_yaw'] = 80.0
    fall['sword_pitch'] = 20.0
    p = keyed([(0.0, STANCE), (0.3, kneel), (0.5, kneel), (0.85, fall), (1.0, fall)], t)
    p['pivot'] = (0.3, 0.0, 0.0)
    return p


SKILL_1 = make_skill(1, chop, chopped)
SKILL_2 = make_skill(2, side_raise, side_hit)
SKILL_3 = make_skill(3, stab_raise, stab_hit)
SKILL_4 = make_skill(4, combo_raise, combo_hit)

ANIMS = {
    'idle': (idle, [i / 4.0 for i in range(4)]),
    'run': (walk, [i / 6.0 for i in range(6)]),
    'skill_1': SKILL_1,
    'skill_2': SKILL_2,
    'skill_3': SKILL_3,
    'skill_4': SKILL_4,
    'hit': (hit, [0.0, 0.45, 0.75]),
    'stun': (stun, [i / 4.0 for i in range(4)]),
    'parried': (parried, [0.0, 0.6, 0.85]),
    'death': (death, [0.0, 0.2, 0.4, 0.6, 0.78, 1.0]),
}

LOOP_PERIOD = {'idle': 1.6, 'stun': 1.0}
LOOP_STRIDE = {'run': 1.6}

CANVAS = (112, 88, 56, 64)
