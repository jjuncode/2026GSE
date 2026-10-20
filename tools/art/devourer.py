# -*- coding: utf-8 -*-
# 소거식귀. 부푼 창백한 살덩이, 땅에 끌리는 긴 팔과 갈고리 손톱, 가슴까지 찢어진 아가리
import math

import numpy as np

from pixel3d import Material, pitch_matrix
from rig import Builder, face_point, keyed, pose, skill_times, solve_humanoid, strike_keys, vec
from timing import skill_windows

BODY = dict(
    hip_z=0.86, hip_w=0.17, thigh=0.42, shin=0.44, ankle_z=0.08,
    spine=0.62, shoulder_w=0.38, upper_arm=0.5, forearm=0.5,
    head_r=0.24,
)

SCALE = 1.45  # 정예. 충돌 반경이 사람의 두 배라 키도 1.6 배쯤

MATERIALS = {
    'flesh': Material([(104, 76, 70), (150, 116, 104), (196, 164, 146), (226, 200, 182)], outline=(46, 28, 26)),
    'vein': Material([(86, 54, 60), (120, 76, 80), (150, 100, 100)], outline=(40, 20, 24)),
    'mouth': Material([(52, 14, 20), (92, 30, 34), (124, 50, 50)], outline=(24, 6, 8)),
    'tooth': Material([(196, 188, 166), (240, 234, 216)], outline=(80, 70, 56), flat=True),
    'claw': Material([(40, 32, 34), (78, 66, 64), (110, 98, 92)], outline=(14, 10, 10)),
    'rag': Material([(36, 30, 32), (58, 50, 50), (82, 72, 68)], outline=(16, 12, 14)),
    'bone': Material([(120, 110, 92), (176, 166, 144), (222, 214, 194)], outline=(52, 44, 36)),
}

EYE = (255, 210, 170)


def build(p, yaw_deg):
    b = Builder(yaw_deg + p['spin'], p['tumble'], p['pivot'])
    s = solve_humanoid(p, BODY)
    cr = s.chest_rot
    up = vec(0.0, 0.0, 1.0)

    # 짧고 굵은 다리
    for hip, knee, ankle in ((s.hip_r, s.knee_r, s.ankle_r), (s.hip_l, s.knee_l, s.ankle_l)):
        b.capsule(hip, knee, 0.15, 0.11, 'flesh')
        b.capsule(knee, ankle, 0.1, 0.08, 'flesh')
        b.ellipsoid(ankle + s.toe_dir * 0.08 - up * 0.04, (0.16, 0.1, 0.06), s.hips_rot, 'vein')

    # 허리 누더기 띠
    b.ellipsoid(s.pelvis, (0.24, 0.3, 0.16), s.hips_rot, 'rag')

    # 부푼 배와 굽은 등. 배 한가운데를 아가리가 세로로 가른다
    b.ellipsoid(s.belly, (0.3, 0.34, 0.3), cr, 'flesh')
    b.ellipsoid(s.chest + cr @ vec(-0.04, 0.0, 0.04), (0.3, 0.42, 0.28), cr, 'flesh')
    b.ellipsoid(s.chest + cr @ vec(-0.2, 0.0, 0.14), (0.24, 0.34, 0.22), cr, 'flesh')
    # 등뼈가 살을 뚫고 솟았다
    for k in range(4):
        spine_root = s.chest + cr @ vec(-0.34 + k * 0.02, 0.0, 0.26 - k * 0.15)
        b.capsule(spine_root, spine_root + cr @ vec(-0.12, 0.0, 0.1), 0.05, 0.012, 'bone', 1)

    maw_open = p['glow']
    maw_c = s.belly + cr @ vec(0.26, 0.0, 0.1)
    maw_r = np.array([0.08, 0.12 + maw_open * 0.06, 0.2 + maw_open * 0.08])
    b.ellipsoid(maw_c, maw_r, cr, 'mouth')
    for k in range(5):
        z = -0.16 + k * 0.08
        for side in (1.0, -1.0):
            root = maw_c + cr @ vec(0.04, (maw_r[1] - 0.01) * side, z * (1.0 + maw_open * 0.4))
            b.capsule(root, root + cr @ vec(0.03, -0.05 * side, 0.0), 0.022, 0.006, 'tooth', 1)

    # 어깨 혹
    for shoulder in (s.shoulder_r, s.shoulder_l):
        b.ellipsoid(shoulder + cr @ vec(-0.02, 0.0, 0.06), (0.18, 0.16, 0.15), cr, 'flesh')

    # 긴 팔. 손끝에 갈고리 손톱 셋
    for shoulder, elbow, hand, side in ((s.shoulder_r, s.elbow_r, s.hand_r, 1.0), (s.shoulder_l, s.elbow_l, s.hand_l, -1.0)):
        b.capsule(shoulder, elbow, 0.12, 0.1, 'flesh')
        b.capsule(elbow, hand, 0.1, 0.085, 'flesh')
        b.ellipsoid(hand, (0.13, 0.12, 0.11), np.eye(3), 'vein')
        reach = hand - elbow
        reach = reach / max(np.linalg.norm(reach), 1e-6)
        across = np.cross(reach, up)
        if np.linalg.norm(across) < 1e-3:
            across = vec(0.0, 1.0, 0.0)
        across = across / np.linalg.norm(across)
        for k in (-1.0, 0.0, 1.0):
            tip_root = hand + reach * 0.08 + across * k * 0.06
            b.capsule(tip_root, tip_root + reach * 0.2 - up * 0.06, 0.032, 0.008, 'claw', 1)

    # 작은 머리. 눈은 희뿌옇게 빛나는 점 둘
    hr = s.head_rot
    head = s.head + hr @ vec(0.14, 0.0, -0.04)
    head_r = np.array([0.2, 0.18, 0.17])
    b.ellipsoid(head, head_r, hr, 'flesh')
    jaw_rot = hr @ pitch_matrix(-10.0 - maw_open * 20.0)
    b.ellipsoid(head + jaw_rot @ vec(0.08, 0.0, -0.12), (0.15, 0.14, 0.08), jaw_rot, 'flesh')
    for side in (1.0, -1.0):
        socket = face_point(head, head_r, hr, (1.0, 0.36 * side, 0.2))
        b.decal(socket, EYE)
    return b


# ------------------------------------------------------------ 동작

def idle(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.06 + math.sin(a) * 0.02), lean=30.0 + math.sin(a) * 2.0, head_pitch=6.0,
                foot_r=(0.06, 0.26, 0.0), foot_l=(-0.06, -0.26, 0.0),
                hand_r=(40.0, 0.5, 0.3 + math.sin(a) * 0.03), hand_l=(-40.0, 0.5, 0.32),
                sword_pitch=-90.0, glow=0.1 + math.sin(a) * 0.1)


STANCE = idle(0.0)


def walk(t):
    a = t * 2.0 * math.pi
    swing = math.cos(a)
    return pose(root=(0.0, 0.0, -0.08 + abs(swing) * 0.04), lean=32.0, side=swing * 7.0, twist=swing * 10.0,
                head_pitch=6.0,
                foot_r=(swing * 0.32, 0.26, max(0.0, -math.sin(a)) * 0.16),
                foot_l=(-swing * 0.32, -0.26, max(0.0, math.sin(a)) * 0.16),
                hand_r=(40.0 - swing * 20.0, 0.5, 0.32), hand_l=(-40.0 - swing * 20.0, 0.5, 0.34),
                sword_pitch=-90.0, glow=0.2)


def slam_raise(index):
    return pose(root=(-0.08, 0.0, 0.0), lean=-6.0, head_pitch=20.0,
                foot_r=(0.02, 0.28, 0.0), foot_l=(-0.12, -0.26, 0.0),
                hand_r=(20.0, 0.2, 2.3), hand_l=(-20.0, 0.2, 2.3),
                sword_pitch=-90.0, glow=1.0)


def slam_hit(index):
    return pose(root=(0.22, 0.0, -0.26), lean=56.0, head_pitch=-10.0,
                foot_r=(0.3, 0.28, 0.0), foot_l=(-0.3, -0.26, 0.0),
                hand_r=(10.0, 1.0, 0.1), hand_l=(-10.0, 1.0, 0.1),
                sword_pitch=-90.0, glow=0.6)


def sweep_raise(index):
    return pose(root=(-0.06, 0.0, -0.06), lean=20.0, twist=-45.0,
                foot_r=(0.0, 0.28, 0.0), foot_l=(-0.14, -0.26, 0.0),
                hand_r=(-100.0, 0.62, 1.2), hand_l=(-130.0, 0.5, 0.5),
                sword_pitch=-90.0, glow=0.5)


def sweep_hit(index):
    return pose(root=(0.12, 0.0, -0.14), lean=34.0, twist=50.0,
                foot_r=(0.24, 0.28, 0.0), foot_l=(-0.2, -0.26, 0.0),
                hand_r=(110.0, 1.0, 0.6), hand_l=(-60.0, 0.5, 0.4),
                sword_pitch=-90.0, glow=0.5)


def grab_raise(index):
    return pose(root=(-0.12, 0.0, -0.12), lean=24.0, head_pitch=20.0,
                foot_r=(-0.06, 0.28, 0.0), foot_l=(0.1, -0.26, 0.0),
                hand_r=(70.0, 0.66, 1.3), hand_l=(-70.0, 0.66, 1.3),
                sword_pitch=-90.0, glow=1.0)


def grab_hit(index):
    return pose(root=(0.36, 0.0, -0.22), lean=50.0, head_pitch=10.0,
                foot_r=(0.5, 0.28, 0.0), foot_l=(-0.36, -0.26, 0.0),
                hand_r=(14.0, 1.08, 0.9), hand_l=(-14.0, 1.08, 0.9),
                sword_pitch=-90.0, glow=1.0)


def make_skill(skill_id, raise_pose, hit_pose):
    windows = skill_windows(skill_id)
    keys = strike_keys(windows, raise_pose, hit_pose, STANCE)

    def anim(t):
        return keyed(keys, t)

    return anim, skill_times(keys, windows)


def hit(t):
    flinch = pose(root=(-0.1, 0.0, -0.02), lean=12.0, head_pitch=24.0, side=-6.0,
                  foot_r=(0.06, 0.26, 0.0), foot_l=(-0.16, -0.26, 0.0),
                  hand_r=(60.0, 0.5, 0.5), hand_l=(-60.0, 0.5, 0.56),
                  sword_pitch=-90.0, glow=0.6)
    return keyed([(0.0, flinch), (0.4, flinch), (1.0, STANCE)], t)


def stun(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.16), lean=44.0, side=math.sin(a) * 8.0, head_pitch=-20.0, head_yaw=math.sin(a) * 16.0,
                foot_r=(0.06, 0.28, 0.0), foot_l=(-0.06, -0.28, 0.0),
                hand_r=(30.0, 0.5, 0.12), hand_l=(-30.0, 0.5, 0.12),
                sword_pitch=-90.0, glow=0.3)


def parried(t):
    knocked = pose(root=(-0.16, 0.0, 0.0), lean=-4.0, head_pitch=30.0,
                   foot_r=(0.0, 0.28, 0.0), foot_l=(-0.28, -0.26, 0.0),
                   hand_r=(80.0, 0.5, 1.6), hand_l=(-80.0, 0.5, 1.5),
                   sword_pitch=-90.0, glow=1.0)
    return keyed([(0.0, knocked), (0.6, knocked), (1.0, STANCE)], t)


def death(t):
    kneel = pose(root=(0.0, 0.0, -0.36), lean=50.0, head_pitch=-20.0,
                 foot_r=(0.24, 0.28, 0.0), foot_l=(-0.3, -0.26, 0.0),
                 hand_r=(40.0, 0.5, 0.05), hand_l=(-40.0, 0.5, 0.05),
                 sword_pitch=-90.0, glow=1.0)
    fall = dict(kneel)
    fall['tumble'] = -80.0
    fall['lean'] = 16.0
    fall['glow'] = 0.4
    p = keyed([(0.0, STANCE), (0.3, kneel), (0.5, kneel), (0.85, fall), (1.0, fall)], t)
    p['pivot'] = (0.45, 0.0, 0.0)
    return p


ANIMS = {
    'idle': (idle, [i / 4.0 for i in range(4)]),
    'run': (walk, [i / 6.0 for i in range(6)]),
    'skill_21': make_skill(21, slam_raise, slam_hit),
    'skill_22': make_skill(22, sweep_raise, sweep_hit),
    'skill_23': make_skill(23, grab_raise, grab_hit),
    'hit': (hit, [0.0, 0.45, 0.75]),
    'stun': (stun, [i / 4.0 for i in range(4)]),
    'parried': (parried, [0.0, 0.6, 0.85]),
    'death': (death, [0.0, 0.2, 0.4, 0.6, 0.78, 1.0]),
}

LOOP_PERIOD = {'idle': 2.0, 'stun': 1.2}
LOOP_STRIDE = {'run': 2.0}

CANVAS = (224, 176, 112, 136)
