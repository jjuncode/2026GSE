# -*- coding: utf-8 -*-
# 성문파쇄자. 성문을 부수던 공성 거인. 녹슨 쇠판을 덧댄 돌 몸, 가슴 속 불씨 핵, 쇠뭉치 주먹
import math

import numpy as np

from pixel3d import Material, pitch_matrix
from rig import Builder, face_point, keyed, pose, skill_times, solve_humanoid, strike_keys, vec
from timing import skill_windows

BODY = dict(
    hip_z=0.98, hip_w=0.22, thigh=0.48, shin=0.5, ankle_z=0.1,
    spine=0.7, shoulder_w=0.5, upper_arm=0.5, forearm=0.5,
    head_r=0.2,
)

SCALE = 1.75  # 보스. 옛 그림 높이 ( 54 칸 ) 에 맞춘다

MATERIALS = {
    'stone': Material([(46, 42, 44), (72, 68, 66), (104, 98, 92), (136, 130, 120)], outline=(18, 16, 18)),
    'iron': Material([(28, 28, 36), (50, 52, 62), (80, 84, 96), (116, 120, 134)], outline=(10, 10, 14), metal=True),
    'rust': Material([(66, 34, 24), (106, 56, 34), (150, 86, 50)], outline=(30, 14, 10)),
    'core': Material([(255, 120, 40), (255, 206, 120)], outline=(130, 40, 10), emissive=True),
    'chain': Material([(50, 50, 58), (90, 92, 104)], outline=(18, 18, 24), metal=True),
}

EMBER = (255, 170, 80)


def build(p, yaw_deg):
    b = Builder(yaw_deg + p['spin'], p['tumble'], p['pivot'])
    s = solve_humanoid(p, BODY)
    cr = s.chest_rot
    up = vec(0.0, 0.0, 1.0)

    # 기둥 같은 다리와 쇠 정강이
    for hip, knee, ankle in ((s.hip_r, s.knee_r, s.ankle_r), (s.hip_l, s.knee_l, s.ankle_l)):
        b.capsule(hip, knee, 0.19, 0.15, 'stone')
        b.capsule(knee, ankle, 0.15, 0.13, 'iron')
        b.ellipsoid(knee + s.toe_dir * 0.08, (0.13, 0.14, 0.11), s.hips_rot, 'iron')
        b.ellipsoid(ankle + s.toe_dir * 0.1 - up * 0.05, (0.22, 0.15, 0.08), s.hips_rot, 'iron')

    # 허리 쇠띠와 늘어진 사슬
    b.ellipsoid(s.pelvis, (0.28, 0.34, 0.2), s.hips_rot, 'stone')
    b.ellipsoid(s.pelvis + cr @ vec(0.0, 0.0, 0.1), (0.3, 0.36, 0.08), s.hips_rot, 'rust')
    for side in (1.0, -1.0):
        top = s.pelvis + s.hips_rot @ vec(0.2, 0.18 * side, 0.06)
        b.capsule(top, top + vec(0.04, 0.0, -0.36), 0.035, 0.035, 'chain')

    # 통짜 가슴. 쇠판 사이로 핵이 탄다
    b.ellipsoid(s.belly, (0.3, 0.38, 0.3), cr, 'stone')
    b.ellipsoid(s.chest, (0.36, 0.52, 0.34), cr, 'stone')
    b.box(s.chest + cr @ vec(0.24, 0.17, 0.02), (0.1, 0.16, 0.24), cr @ pitch_matrix(-10.0), 0.03, 'iron')
    b.box(s.chest + cr @ vec(0.24, -0.17, 0.02), (0.1, 0.16, 0.24), cr @ pitch_matrix(-10.0), 0.03, 'iron')
    b.ellipsoid(s.chest + cr @ vec(0.3, 0.0, -0.04), (0.06, 0.08, 0.14), cr, 'core')
    # 등에 진 성문 조각
    b.box(s.chest + cr @ vec(-0.36, 0.0, -0.04), (0.06, 0.38, 0.3), cr @ pitch_matrix(12.0), 0.02, 'rust')

    # 어깨 쇠 견갑 ( 커다란 사발 )
    for shoulder in (s.shoulder_r, s.shoulder_l):
        b.ellipsoid(shoulder + cr @ vec(0.0, 0.0, 0.06), (0.21, 0.2, 0.15), cr, 'iron')

    # 팔과 쇠뭉치 주먹
    for shoulder, elbow, hand in ((s.shoulder_r, s.elbow_r, s.hand_r), (s.shoulder_l, s.elbow_l, s.hand_l)):
        b.capsule(shoulder, elbow, 0.17, 0.14, 'stone')
        b.capsule(elbow, hand, 0.16, 0.18, 'rust')
        b.box(hand, (0.17, 0.17, 0.17), cr, 0.06, 'iron')
        b.ellipsoid(elbow, (0.12, 0.12, 0.12), np.eye(3), 'iron')

    # 작은 투구 머리. 틈으로 불씨 눈
    hr = s.head_rot
    head = s.head + hr @ vec(0.16, 0.0, 0.0)
    helm_r = np.array([0.22, 0.21, 0.24])
    b.ellipsoid(head, helm_r, hr, 'iron')
    # 투구 볏과 양 옆 뿔
    b.box(head + hr @ vec(-0.02, 0.0, 0.22), (0.14, 0.03, 0.07), hr, 0.02, 'rust')
    for side in (1.0, -1.0):
        horn = head + hr @ vec(0.0, 0.18 * side, 0.1)
        b.capsule(horn, horn + hr @ vec(0.06, 0.16 * side, 0.18), 0.06, 0.015, 'stone', 1)
    for side in (1.0, -1.0):
        b.decal(face_point(head, helm_r, hr, (1.0, 0.3 * side, 0.0)), EMBER)
    return b


# ------------------------------------------------------------ 동작

def idle(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.04 + math.sin(a) * 0.02), lean=14.0 + math.sin(a) * 1.5, head_pitch=0.0,
                foot_r=(0.06, 0.32, 0.0), foot_l=(-0.06, -0.32, 0.0),
                hand_r=(50.0, 0.62, 0.66 + math.sin(a) * 0.02), hand_l=(-50.0, 0.62, 0.68),
                sword_pitch=-90.0)


STANCE = idle(0.0)


def walk(t):
    a = t * 2.0 * math.pi
    swing = math.cos(a)
    return pose(root=(0.0, 0.0, -0.08 + abs(swing) * 0.05), lean=18.0, side=swing * 4.0, twist=swing * 8.0,
                foot_r=(swing * 0.36, 0.32, max(0.0, -math.sin(a)) * 0.18),
                foot_l=(-swing * 0.36, -0.32, max(0.0, math.sin(a)) * 0.18),
                hand_r=(50.0 - swing * 20.0, 0.62, 0.68), hand_l=(-50.0 - swing * 20.0, 0.62, 0.7),
                sword_pitch=-90.0)


def punch_raise(index):
    if index == 0:
        return pose(root=(-0.08, 0.0, -0.08), lean=10.0, twist=40.0,
                    foot_r=(-0.06, 0.32, 0.0), foot_l=(0.12, -0.32, 0.0),
                    hand_r=(120.0, 0.46, 1.4), hand_l=(-30.0, 0.62, 1.0), sword_pitch=-90.0)
    if index == 1:
        return pose(root=(0.06, 0.0, -0.08), lean=12.0, twist=-40.0,
                    foot_r=(0.12, 0.32, 0.0), foot_l=(-0.06, -0.32, 0.0),
                    hand_r=(30.0, 0.62, 1.0), hand_l=(-120.0, 0.46, 1.4), sword_pitch=-90.0)
    return quake_raise(index)


def punch_hit(index):
    if index == 0:
        return pose(root=(0.14, 0.0, -0.14), lean=26.0, twist=-25.0,
                    foot_r=(0.06, 0.32, 0.0), foot_l=(0.26, -0.32, 0.0),
                    hand_r=(6.0, 1.1, 0.8), hand_l=(-60.0, 0.5, 0.9), sword_pitch=-90.0)
    if index == 1:
        return pose(root=(0.26, 0.0, -0.14), lean=26.0, twist=25.0,
                    foot_r=(0.34, 0.32, 0.0), foot_l=(0.02, -0.32, 0.0),
                    hand_r=(60.0, 0.5, 0.9), hand_l=(-6.0, 1.1, 0.8), sword_pitch=-90.0)
    p = quake_hit(index)
    p['root'] = (0.36, 0.0, -0.3)
    return p


def quake_raise(index):
    return pose(root=(-0.06, 0.0, 0.06), lean=-8.0, head_pitch=24.0,
                foot_r=(0.04, 0.34, 0.0), foot_l=(-0.08, -0.34, 0.0),
                hand_r=(10.0, 0.26, 2.7), hand_l=(-10.0, 0.26, 2.7), sword_pitch=-90.0)


def quake_hit(index):
    return pose(root=(0.18, 0.0, -0.32), lean=50.0, head_pitch=-6.0,
                foot_r=(0.24, 0.36, 0.0), foot_l=(-0.3, -0.34, 0.0),
                hand_r=(8.0, 1.04, 0.16), hand_l=(-8.0, 1.04, 0.16), sword_pitch=-90.0)


def charge(t):
    # 어깨를 앞세워 달린다. 판정 창 내내 이 반복
    start, end = skill_windows(33)[0]
    lowered = pose(root=(0.0, 0.0, -0.16), lean=38.0, twist=-30.0, head_pitch=-6.0,
                   foot_r=(0.2, 0.34, 0.0), foot_l=(-0.3, -0.32, 0.0),
                   hand_r=(60.0, 0.5, 0.8), hand_l=(-30.0, 0.6, 1.0), sword_pitch=-90.0)
    if t < start:
        return keyed([(0.0, STANCE), (start * 0.6, lowered), (start, lowered)], t)
    if t <= end:
        u = (t - start) / max(end - start, 1e-6)
        a = u * 4.0 * math.pi
        p = dict(lowered)
        p['foot_r'] = (math.cos(a) * 0.36, 0.34, max(0.0, -math.sin(a)) * 0.18)
        p['foot_l'] = (-math.cos(a) * 0.36, -0.32, max(0.0, math.sin(a)) * 0.18)
        p['root'] = (0.1, 0.0, -0.16 + abs(math.cos(a)) * 0.06)
        return p
    return keyed([(end, lowered), (min(end + 0.2, 0.98), STANCE), (1.0, STANCE)], t)


def grab_raise(index):
    return pose(root=(-0.1, 0.0, -0.1), lean=12.0, twist=30.0,
                foot_r=(-0.06, 0.32, 0.0), foot_l=(0.14, -0.32, 0.0),
                hand_r=(90.0, 0.8, 1.6), hand_l=(-50.0, 0.6, 0.8), sword_pitch=-90.0)


def grab_hit(index):
    return pose(root=(0.3, 0.0, -0.16), lean=34.0, twist=-10.0,
                foot_r=(0.4, 0.32, 0.0), foot_l=(-0.3, -0.32, 0.0),
                hand_r=(4.0, 1.12, 1.0), hand_l=(-70.0, 0.6, 0.8), sword_pitch=-90.0)


def make_skill(skill_id, raise_pose, hit_pose):
    windows = skill_windows(skill_id)
    keys = strike_keys(windows, raise_pose, hit_pose, STANCE)

    def anim(t):
        return keyed(keys, t)

    return anim, skill_times(keys, windows)


def hit(t):
    flinch = pose(root=(-0.08, 0.0, -0.02), lean=6.0, head_pitch=18.0, side=-4.0,
                  foot_r=(0.06, 0.32, 0.0), foot_l=(-0.14, -0.32, 0.0),
                  hand_r=(70.0, 0.6, 0.8), hand_l=(-70.0, 0.6, 0.84), sword_pitch=-90.0)
    return keyed([(0.0, flinch), (0.4, flinch), (1.0, STANCE)], t)


def stun(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.2), lean=34.0, side=math.sin(a) * 6.0, head_pitch=-20.0, head_yaw=math.sin(a) * 12.0,
                foot_r=(0.06, 0.34, 0.0), foot_l=(-0.06, -0.34, 0.0),
                hand_r=(30.0, 0.66, 0.3), hand_l=(-30.0, 0.66, 0.3), sword_pitch=-90.0)


def parried(t):
    knocked = pose(root=(-0.16, 0.0, 0.0), lean=-6.0, head_pitch=26.0,
                   foot_r=(0.0, 0.32, 0.0), foot_l=(-0.3, -0.32, 0.0),
                   hand_r=(90.0, 0.6, 1.9), hand_l=(-80.0, 0.6, 1.4), sword_pitch=-90.0)
    return keyed([(0.0, knocked), (0.6, knocked), (1.0, STANCE)], t)


def death(t):
    kneel = pose(root=(0.0, 0.0, -0.42), lean=40.0, head_pitch=-24.0,
                 foot_r=(0.3, 0.32, 0.0), foot_l=(-0.36, -0.32, 0.0),
                 hand_r=(40.0, 0.66, 0.2), hand_l=(-40.0, 0.66, 0.2), sword_pitch=-90.0)
    fall = dict(kneel)
    fall['tumble'] = -78.0
    fall['lean'] = 12.0
    p = keyed([(0.0, STANCE), (0.3, kneel), (0.55, kneel), (0.85, fall), (1.0, fall)], t)
    p['pivot'] = (0.6, 0.0, 0.0)
    return p


def charge_times():
    start, end = skill_windows(33)[0]
    times = [0.0, start * 0.6]
    for k in range(8):
        times.append(start + (end - start) * k / 8.0)
    times.append(end + 0.1)
    times.append(min(end + 0.2, 0.98))
    return times


ANIMS = {
    'idle': (idle, [i / 4.0 for i in range(4)]),
    'run': (walk, [i / 6.0 for i in range(6)]),
    'skill_31': make_skill(31, punch_raise, punch_hit),
    'skill_32': make_skill(32, quake_raise, quake_hit),
    'skill_33': (charge, charge_times()),
    'skill_34': make_skill(34, grab_raise, grab_hit),
    'hit': (hit, [0.0, 0.45, 0.75]),
    'stun': (stun, [i / 4.0 for i in range(4)]),
    'parried': (parried, [0.0, 0.6, 0.85]),
    'death': (death, [0.0, 0.2, 0.4, 0.6, 0.78, 1.0]),
}

LOOP_PERIOD = {'idle': 2.2, 'stun': 1.3}
LOOP_STRIDE = {'run': 2.4}

CANVAS = (320, 272, 160, 210)
