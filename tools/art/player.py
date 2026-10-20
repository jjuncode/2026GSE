# -*- coding: utf-8 -*-
# 개척대 검사. 청록 두건 망토 + 가죽 갑옷 + 외날 장검
import math

import numpy as np

from pixel3d import Material, pitch_matrix, yaw_matrix
from rig import Builder, add_sword, face_point, keyed, pose, smooth, solve_humanoid, vec

BODY = dict(
    hip_z=0.84, hip_w=0.10, thigh=0.42, shin=0.42, ankle_z=0.07,
    spine=0.46, shoulder_w=0.22, upper_arm=0.27, forearm=0.26,
    head_r=0.20,
)

SCALE = 0.95  # 옛 NPC ( 21 칸 ) 보다 머리 하나쯤 큰 23 칸
BLADE_LEN = 0.66

MATERIALS = {
    'cloak': Material([(20, 48, 54), (30, 78, 78), (46, 112, 104), (80, 152, 134)], outline=(12, 22, 26)),
    'leather': Material([(54, 36, 30), (88, 60, 42), (124, 88, 58), (164, 122, 82)], outline=(26, 16, 14)),
    'strap': Material([(34, 24, 22), (58, 40, 32), (82, 58, 42)], outline=(20, 12, 12)),
    'cloth': Material([(28, 26, 34), (44, 42, 50), (64, 60, 66), (88, 84, 88)], outline=(14, 12, 18)),
    'boot': Material([(30, 22, 22), (52, 38, 32), (78, 56, 42)], outline=(14, 10, 10)),
    'skin': Material([(118, 74, 62), (168, 116, 92), (210, 160, 126), (236, 196, 160)], outline=(60, 34, 30)),
    'steel': Material([(52, 56, 70), (96, 104, 120), (152, 162, 178), (204, 212, 224)], outline=(22, 24, 32), metal=True),
    'blade': Material([(110, 118, 136), (168, 178, 196), (214, 222, 236)], outline=(30, 34, 46), metal=True),
    'blade_glow': Material([(190, 236, 255), (236, 250, 255)], outline=(60, 120, 170), emissive=True),
    'blade_gold': Material([(255, 214, 120), (255, 246, 200)], outline=(150, 80, 20), emissive=True),
    'gold': Material([(108, 70, 30), (168, 120, 48), (222, 180, 84), (250, 226, 150)], outline=(50, 30, 14), metal=True),
    'grip': Material([(40, 28, 24), (70, 48, 36)], outline=(18, 12, 10)),
}

EYE = (24, 20, 26)


def build(p, yaw_deg):
    b = Builder(yaw_deg + p['spin'], p['tumble'], p['pivot'])
    s = solve_humanoid(p, BODY)
    up = vec(0.0, 0.0, 1.0)
    chest_rot = s.chest_rot

    # 다리
    for hip, knee, ankle in ((s.hip_r, s.knee_r, s.ankle_r), (s.hip_l, s.knee_l, s.ankle_l)):
        b.capsule(hip, knee, 0.085, 0.07, 'cloth')
        b.capsule(knee, ankle, 0.075, 0.062, 'boot')
        b.ellipsoid(knee + (knee - hip) * 0.05 + s.toe_dir * 0.03, (0.07, 0.07, 0.05), s.hips_rot, 'boot')
        toe = ankle + s.toe_dir * 0.07 - up * 0.035
        b.ellipsoid(toe, (0.12, 0.065, 0.05), s.hips_rot, 'boot')

    # 골반 · 허리
    b.ellipsoid(s.pelvis, (0.13, 0.17, 0.12), s.hips_rot, 'cloth')
    b.ellipsoid(s.pelvis + chest_rot @ vec(0.0, 0.0, 0.05), (0.14, 0.18, 0.04), s.hips_rot, 'strap')
    b.decal(s.pelvis + chest_rot @ vec(0.145, 0.0, 0.05), (234, 196, 96))
    # 가죽 갑옷 자락 - 허벅지 위를 덮는다
    b.ellipsoid(s.pelvis + s.hips_rot @ vec(0.0, 0.0, -0.06), (0.15, 0.19, 0.12), s.hips_rot @ pitch_matrix(p['lean'] * 0.3), 'leather')

    # 몸통
    b.ellipsoid(s.belly, (0.13, 0.17, 0.14), chest_rot, 'leather')
    b.ellipsoid(s.chest, (0.155, 0.21, 0.16), chest_rot, 'leather')
    # 가슴 띠 ( 오른어깨에서 왼허리로 )
    b.capsule(s.chest + chest_rot @ vec(0.13, 0.14, 0.11), s.belly + chest_rot @ vec(0.12, -0.14, -0.05), 0.025, 0.025, 'strap')

    # 망토. 어깨 뒤에서 내려와 뒤로 들린다
    cape_rot = chest_rot @ yaw_matrix(p['cape_side']) @ pitch_matrix(-p['cape'])
    cape_top = s.neck + chest_rot @ vec(-0.13, 0.0, -0.05)
    cape_mid = cape_top + cape_rot @ vec(-0.04, 0.0, -0.36)
    b.box(cape_top + cape_rot @ vec(-0.02, 0.0, -0.18), (0.03, 0.2, 0.19), cape_rot, 0.02, 'cloak')
    low_rot = cape_rot @ pitch_matrix(-p['cape'] * 0.5)
    b.box(cape_mid + low_rot @ vec(-0.01, 0.0, -0.14), (0.028, 0.21, 0.16), low_rot, 0.02, 'cloak')

    # 어깨 망토 ( 두건과 이어지는 짧은 덮개 )
    b.ellipsoid(s.neck + chest_rot @ vec(-0.02, 0.0, -0.06), (0.17, 0.25, 0.09), chest_rot, 'cloak')

    # 팔
    for shoulder, elbow, hand, armor in ((s.shoulder_r, s.elbow_r, s.hand_r, 'steel'), (s.shoulder_l, s.elbow_l, s.hand_l, 'leather')):
        b.capsule(shoulder, elbow, 0.065, 0.055, 'cloth')
        b.capsule(elbow, hand, 0.06, 0.05, 'leather')
        b.ellipsoid(hand, (0.055, 0.055, 0.055), np.eye(3), 'strap')
        b.ellipsoid(shoulder + chest_rot @ vec(0.0, 0.0, 0.03), (0.10, 0.10, 0.075), chest_rot, armor)

    # 머리. 두건 안으로 얼굴이 조금 나온다
    head = s.head
    hr = s.head_rot
    face_c = head + hr @ vec(0.075, 0.0, -0.035)
    face_r = np.array([0.135, 0.13, 0.14])
    b.ellipsoid(face_c, face_r, hr, 'skin')
    b.ellipsoid(head + hr @ vec(-0.035, 0.0, 0.015), (0.205, 0.2, 0.205), hr, 'cloak')
    # 두건 끝 ( 뒤로 처진 고깔 )
    b.capsule(head + hr @ vec(-0.12, 0.0, 0.08), head + hr @ vec(-0.27, 0.0, -0.04), 0.1, 0.03, 'cloak')
    for side in (1.0, -1.0):
        eye = face_point(face_c, face_r, hr, (1.0, 0.42 * side, 0.02))
        b.decal(eye, EYE)

    # 칼
    blade = 'blade'
    if p['glow'] > 1.5:
        blade = 'blade_gold'
    elif p['glow'] > 0.5:
        blade = 'blade_glow'
    add_sword(b, s.hand_r, s.sword_dir, p['sword_roll'], dict(blade=blade, blade_glow=blade, guard='gold', grip='grip'), BLADE_LEN, 0.0)

    return b


# ------------------------------------------------------------ 기본 자세

def idle(t):
    breath = math.sin(t * 2.0 * math.pi)
    return pose(root=(0.0, 0.0, -0.02 + breath * 0.012), lean=7.0 + breath * 1.5,
                foot_r=(0.08, 0.15, 0.0), foot_l=(-0.06, -0.14, 0.0),
                hand_r=(48.0, 0.31, 0.76 + breath * 0.012), hand_l=(-70.0, 0.27, 0.80 + breath * 0.01),
                sword_yaw=40.0, sword_pitch=-62.0, cape=8.0 + breath * 2.0)


STANCE = idle(0.0)


def run_leg(phase, side):
    # 한 다리 한 바퀴. 0 에 발이 앞에서 땅에 닿고, 0.38 까지 뒤로 밀며 디딘다.
    # 그 뒤 뒤꿈치가 엉덩이 쪽으로 차올랐다가 무릎이 앞으로 끌려 나온다 - 이 궤적이 없으면 발이 바닥을 쓸며 잔발로 보인다
    phase = phase - math.floor(phase)
    width = 0.12 * side
    if phase < 0.38:
        u = phase / 0.38
        return (0.2 - 0.56 * u, width, 0.0)
    u = (phase - 0.38) / 0.62
    keys = [(0.0, (-0.36, 0.0)), (0.3, (-0.3, 0.34)), (0.62, (0.18, 0.3)), (0.86, (0.26, 0.08)), (1.0, (0.2, 0.0))]
    for (u0, (x0, z0)), (u1, (x1, z1)) in zip(keys, keys[1:]):
        if u <= u1:
            k = smooth((u - u0) / (u1 - u0))
            return (x0 + (x1 - x0) * k, width, z0 + (z1 - z0) * k)
    return (0.2, width, 0.0)


def run(t):
    a = t * 2.0 * math.pi
    # 디딘 발 한가운데서 가장 낮고, 두 발이 다 뜬 순간 가장 높다
    bob = -0.045 - 0.05 * math.cos(4.0 * math.pi * (t - 0.19))
    swing = math.cos(a)
    return pose(root=(0.04, 0.0, bob), lean=20.0, twist=swing * 12.0, hip_yaw=-swing * 10.0,
                foot_r=run_leg(t, 1.0), foot_l=run_leg(t + 0.5, -1.0),
                hand_r=(95.0 - swing * 20.0, 0.32, 0.8), hand_l=(-70.0 - swing * 45.0, 0.3, 0.86 + swing * 0.08),
                sword_yaw=160.0, sword_pitch=-18.0, cape=26.0 + math.sin(a * 2.0) * 7.0, cape_side=swing * 8.0)


GUARD = pose(root=(0.0, 0.0, -0.07), lean=12.0, twist=-20.0,
             foot_r=(0.16, 0.17, 0.0), foot_l=(-0.14, -0.15, 0.0),
             hand_r=(20.0, 0.36, 0.98), hand_l=(-20.0, 0.32, 0.96),
             sword_yaw=-75.0, sword_pitch=32.0, sword_roll=90.0, cape=12.0)


def guard(t):
    return dict(GUARD)


def guard_walk(t):
    a = t * 2.0 * math.pi
    stride = 0.2
    p = dict(GUARD)
    p['foot_r'] = (math.cos(a) * stride + 0.1, 0.17, max(0.0, -math.sin(a)) * 0.07)
    p['foot_l'] = (-math.cos(a) * stride - 0.08, -0.15, max(0.0, math.sin(a)) * 0.07)
    p['root'] = (0.0, 0.0, -0.07 + abs(math.cos(a)) * 0.025)
    return p


def parry(t):
    p = dict(GUARD)
    p['glow'] = 1.0
    p['hand_r'] = (10.0, 0.42, 1.04)
    p['lean'] = 16.0
    return p


def parry_success(t):
    # 맞부딪쳐 칼을 밀어 올렸다가 다시 겨눈다
    push = dict(GUARD)
    push['hand_r'] = (0.0, 0.48, 1.16)
    push['sword_yaw'] = -30.0
    push['sword_pitch'] = 58.0
    push['glow'] = 1.0
    push['lean'] = 20.0
    push['root'] = (0.06, 0.0, -0.04)
    push['cape'] = 22.0
    keys = [(0.0, push), (0.35, push), (1.0, GUARD)]
    p = keyed(keys, t)
    if t < 0.5:
        p['glow'] = 1.0
    return p


# ------------------------------------------------------------ 공격
# 시간 0 ~ 0.4 예비 / 0.4 ~ 0.7 휘둘러 판정 / 0.7 ~ 1 회수. 게임이 판정 창에 맞춰 늘이고 줄인다

ATTACK_TIMES = [0.0, 0.2, 0.36, 0.47, 0.57, 0.67, 0.8, 0.92]
SLASH_TIMES = [0.0, 0.16, 0.34, 0.44, 0.52, 0.6, 0.68, 0.78, 0.9]


def sign_of(value):
    if value > 0:
        return 1.0
    return -1.0


def slash(from_yaw, to_yaw, height_from, height_to, pitch_from, pitch_to, step, crouch=0.0):
    # 감기 -> 내딛기 -> 휘두르기 -> 넘겨 휘감기 -> 회수. 몸통이 칼보다 먼저 돌고 칼이 늦게 따라온다
    sf = sign_of(from_yaw)
    st = sign_of(to_yaw)
    mid_h = (height_from + height_to) * 0.5
    mid_p = (pitch_from + pitch_to) * 0.5
    coil = pose(root=(-0.07, 0.0, -0.07 - crouch), lean=6.0, twist=from_yaw * 0.5, side=-sf * 4.0, head_yaw=-from_yaw * 0.35,
                foot_r=(0.0, 0.17, 0.0), foot_l=(-0.16, -0.16, 0.0),
                hand_r=(from_yaw + sf * 22.0, 0.34, height_from), hand_l=(from_yaw * 0.4 - 30.0, 0.27, 0.92),
                sword_yaw=from_yaw + sf * 80.0, sword_pitch=pitch_from + 12.0, cape=6.0)
    loaded = dict(coil)
    loaded['root'] = (-0.09, 0.0, -0.11 - crouch)
    loaded['twist'] = from_yaw * 0.62
    loaded['hand_r'] = (from_yaw + sf * 30.0, 0.32, height_from + 0.04)
    loaded['foot_r'] = (0.12, 0.17, 0.08)
    launch = pose(root=(step * 0.45, 0.0, -0.11 - crouch), lean=14.0, twist=from_yaw * 0.25, head_yaw=0.0,
                  foot_r=(0.34, 0.17, 0.04), foot_l=(-0.22, -0.16, 0.0),
                  hand_r=(from_yaw * 0.55, 0.5, mid_h + 0.05), hand_l=(-70.0, 0.26, 0.86),
                  sword_yaw=from_yaw * 0.75 + sf * 15.0, sword_pitch=mid_p + 4.0, cape=16.0)
    mid = pose(root=(step * 0.85, 0.0, -0.14 - crouch), lean=24.0, twist=0.0,
               foot_r=(0.44, 0.17, 0.0), foot_l=(-0.28, -0.16, 0.0),
               hand_r=(0.0, 0.64, mid_h), hand_l=(-105.0, 0.27, 0.82),
               sword_yaw=0.0, sword_pitch=mid_p - 2.0, cape=24.0)
    strike = pose(root=(step, 0.0, -0.13 - crouch), lean=22.0, twist=to_yaw * 0.55, head_yaw=to_yaw * 0.15,
                  foot_r=(0.44, 0.17, 0.0), foot_l=(-0.28, -0.16, 0.0),
                  hand_r=(to_yaw, 0.54, height_to), hand_l=(-125.0, 0.28, 0.84),
                  sword_yaw=to_yaw + st * 38.0, sword_pitch=pitch_to, cape=28.0, cape_side=-st * 14.0)
    follow = dict(strike)
    follow['twist'] = to_yaw * 0.72
    follow['lean'] = 18.0
    follow['hand_r'] = (to_yaw + st * 22.0, 0.42, height_to + 0.02)
    follow['sword_yaw'] = to_yaw + st * 72.0
    follow['sword_pitch'] = pitch_to - 12.0
    settle = pose(root=(step, 0.0, -0.05), lean=11.0, twist=to_yaw * 0.2,
                  foot_r=(0.3, 0.16, 0.0), foot_l=(-0.18, -0.15, 0.0),
                  hand_r=(to_yaw * 0.5 + 25.0, 0.36, 0.8), hand_l=(-75.0, 0.27, 0.8),
                  sword_yaw=to_yaw * 0.4 + 30.0, sword_pitch=-45.0, cape=10.0)
    keys = [(0.0, STANCE), (0.16, coil), (0.38, loaded), (0.44, launch), (0.52, mid), (0.6, strike), (0.7, follow), (0.84, settle), (1.0, settle)]

    def anim(t):
        return keyed(keys, t)

    return anim


def thrust(glow, lunge, low):
    # 찌르기 ( 돌진 베기, 반격 ). 칼을 허리 옆으로 당겼다가 앞으로 뻗는다
    pull = pose(root=(-0.06, 0.0, -0.1 - low), lean=14.0, twist=35.0,
                foot_r=(-0.1, 0.16, 0.0), foot_l=(0.12, -0.15, 0.0),
                hand_r=(120.0, 0.24, 0.92), hand_l=(10.0, 0.3, 0.98),
                sword_yaw=5.0, sword_pitch=4.0, cape=12.0, glow=glow)
    reach = pose(root=(lunge, 0.0, -0.14 - low), lean=30.0, twist=-15.0,
                 foot_r=(0.42, 0.15, 0.0), foot_l=(-0.32, -0.14, 0.0),
                 hand_r=(4.0, 0.66, 0.98), hand_l=(-110.0, 0.26, 0.86),
                 sword_yaw=0.0, sword_pitch=-2.0, cape=34.0, glow=glow)
    hold = dict(reach)
    hold['hand_r'] = (2.0, 0.62, 0.96)
    hold['cape'] = 26.0
    settle = dict(reach)
    settle['root'] = (lunge, 0.0, -0.06)
    settle['lean'] = 12.0
    settle['hand_r'] = (40.0, 0.36, 0.8)
    settle['sword_pitch'] = -40.0
    settle['sword_yaw'] = 30.0
    settle['cape'] = 12.0
    settle['glow'] = 0.0
    keys = [(0.0, STANCE), (0.3, pull), (0.4, pull), (0.48, reach), (0.7, hold), (0.85, settle), (1.0, settle)]

    def anim(t):
        return keyed(keys, t)

    return anim


SPIN_TIMES = [0.0, 0.2, 0.36] + [0.4 + 0.3 * k / 8.0 for k in range(8)] + [0.75, 0.9]


def spin_pose(u, crouch):
    # 칼을 옆으로 쭉 뻗고 몸을 안쪽으로 기울인다. 왼팔은 반대로 뻗어 균형
    return pose(root=(0.04, 0.0, -0.12 - crouch), lean=14.0, side=-12.0, twist=12.0,
                foot_r=(0.1, 0.24, 0.04 * max(0.0, math.sin(u * 4.0 * math.pi))), foot_l=(-0.12, -0.24, 0.04 * max(0.0, -math.sin(u * 4.0 * math.pi))),
                hand_r=(88.0, 0.6, 0.96), hand_l=(-95.0, 0.34, 1.0),
                sword_yaw=92.0, sword_pitch=4.0, cape=30.0, cape_side=-38.0)


def spin_combo(t):
    # 한 바퀴 돌며 벤다. 판정 창 동안 오른쪽으로 360 도
    windup = pose(root=(-0.04, 0.0, -0.14), lean=16.0, twist=-55.0, head_yaw=30.0,
                  foot_r=(0.04, 0.22, 0.0), foot_l=(-0.08, -0.22, 0.0),
                  hand_r=(-80.0, 0.42, 0.92), hand_l=(-120.0, 0.28, 0.92),
                  sword_yaw=-160.0, sword_pitch=8.0, cape=8.0)
    settle = pose(root=(0.08, 0.0, -0.05), lean=12.0, twist=20.0,
                  foot_r=(0.1, 0.2, 0.0), foot_l=(-0.1, -0.2, 0.0),
                  hand_r=(70.0, 0.4, 0.8), hand_l=(-70.0, 0.27, 0.8),
                  sword_yaw=70.0, sword_pitch=-40.0, cape=10.0)
    if t < 0.4:
        return keyed([(0.0, STANCE), (0.3, windup), (0.4, windup)], t)
    if t < 0.7:
        u = (t - 0.4) / 0.3
        p = spin_pose(u, 0.02)
        p['spin'] = 360.0 * u
        return p
    return keyed([(0.7, spin_pose(1.0, 0.02)), (0.85, settle), (1.0, settle)], t)


def whirl(t):
    # 회전베기 반복 구간. 한 바퀴를 12 장으로
    p = spin_pose(t, 0.0)
    p['spin'] = 360.0 * t
    return p


def leap(t):
    # 뛰어올라 내려찍기. 판정 창이 착지
    crouch = pose(root=(-0.04, 0.0, -0.2), lean=24.0, twist=0.0,
                  foot_r=(0.06, 0.16, 0.0), foot_l=(-0.08, -0.15, 0.0),
                  hand_r=(70.0, 0.3, 0.7), hand_l=(-70.0, 0.3, 0.7),
                  sword_yaw=0.0, sword_pitch=-160.0, cape=10.0)
    air = pose(root=(0.1, 0.0, 0.5), lean=-6.0, twist=0.0,
               foot_r=(0.0, 0.14, 0.42), foot_l=(-0.2, -0.14, 0.36),
               hand_r=(0.0, 0.1, 1.72), hand_l=(0.0, 0.1, 1.7), two_hand=1.0,
               sword_yaw=0.0, sword_pitch=-220.0, cape=6.0)
    slam = pose(root=(0.2, 0.0, -0.22), lean=34.0, twist=0.0,
                foot_r=(0.36, 0.17, 0.0), foot_l=(-0.2, -0.16, 0.0),
                hand_r=(0.0, 0.62, 0.56), hand_l=(0.0, 0.58, 0.6), two_hand=1.0,
                sword_yaw=0.0, sword_pitch=-398.0, cape=40.0)
    settle = dict(slam)
    settle['root'] = (0.2, 0.0, -0.08)
    settle['lean'] = 14.0
    settle['two_hand'] = 0.0
    settle['hand_r'] = (40.0, 0.36, 0.78)
    settle['hand_l'] = (-70.0, 0.27, 0.8)
    settle['sword_yaw'] = 40.0
    settle['sword_pitch'] = -415.0
    settle['cape'] = 10.0
    keys = [(0.0, STANCE), (0.12, crouch), (0.33, air), (0.4, air), (0.5, slam), (0.72, slam), (0.88, settle), (1.0, settle)]
    return keyed(keys, t)


# ------------------------------------------------------------ 회피 · 피격

DODGE_TIMES = [0.0, 0.1, 0.26, 0.46, 0.66, 0.82]


def dodge(t):
    # 웅크림 -> 튀어나감 ( 뒷다리를 끝까지 편다 ) -> 미끄러지듯 날아감 -> 앞발로 제동 -> 회복
    crouch = pose(root=(-0.04, 0.0, -0.2), lean=30.0, head_pitch=-6.0,
                  foot_r=(0.08, 0.15, 0.0), foot_l=(-0.06, -0.14, 0.0),
                  hand_r=(120.0, 0.3, 0.8), hand_l=(-120.0, 0.3, 0.84),
                  sword_yaw=175.0, sword_pitch=-15.0, cape=12.0)
    launch = pose(root=(0.16, 0.0, -0.1), lean=36.0, head_pitch=10.0,
                  foot_r=(0.34, 0.14, 0.2), foot_l=(-0.58, -0.12, 0.12),
                  hand_r=(150.0, 0.36, 0.86), hand_l=(-150.0, 0.34, 0.92),
                  sword_yaw=180.0, sword_pitch=-6.0, cape=36.0)
    glide = pose(root=(0.18, 0.0, -0.06), lean=30.0, head_pitch=8.0,
                 foot_r=(0.28, 0.14, 0.26), foot_l=(-0.42, -0.12, 0.3),
                 hand_r=(155.0, 0.38, 0.9), hand_l=(-155.0, 0.36, 0.95),
                 sword_yaw=182.0, sword_pitch=-2.0, cape=34.0, cape_side=6.0)
    brake = pose(root=(0.06, 0.0, -0.17), lean=2.0, head_pitch=-4.0,
                 foot_r=(0.5, 0.16, 0.0), foot_l=(-0.22, -0.15, 0.0),
                 hand_r=(60.0, 0.36, 0.86), hand_l=(-70.0, 0.32, 0.9),
                 sword_yaw=60.0, sword_pitch=-30.0, cape=6.0)
    keys = [(0.0, crouch), (0.1, crouch), (0.26, launch), (0.5, glide), (0.7, brake), (0.84, brake), (1.0, STANCE)]
    return keyed(keys, t)


def roll(t):
    # 몸을 공처럼 말아 앞으로 한 바퀴
    ball = pose(root=(0.0, 0.0, -0.42), lean=60.0, head_pitch=-30.0,
                foot_r=(0.12, 0.13, 0.16), foot_l=(0.06, -0.13, 0.2),
                hand_r=(60.0, 0.28, 0.52), hand_l=(-60.0, 0.28, 0.52),
                sword_yaw=170.0, sword_pitch=-10.0, cape=20.0, pivot=(0.0, 0.0, 0.42))
    if t < 0.15:
        return keyed([(0.0, STANCE), (0.15, ball)], t)
    if t < 0.8:
        u = (t - 0.15) / 0.65
        p = dict(ball)
        p['tumble'] = -360.0 * smooth(u)
        p['root'] = (0.0, 0.0, -0.42 + math.sin(u * math.pi) * 0.06)
        return p
    return keyed([(0.8, ball), (1.0, STANCE)], t)


def hit(t):
    flinch = pose(root=(-0.08, 0.0, -0.06), lean=-14.0, head_pitch=18.0, side=6.0,
                  foot_r=(0.02, 0.16, 0.0), foot_l=(-0.14, -0.15, 0.0),
                  hand_r=(80.0, 0.32, 0.9), hand_l=(-90.0, 0.32, 0.98),
                  sword_yaw=120.0, sword_pitch=-30.0, cape=-10.0)
    keys = [(0.0, flinch), (0.4, flinch), (1.0, STANCE)]
    return keyed(keys, t)


def stun(t):
    a = t * 2.0 * math.pi
    return pose(root=(0.0, 0.0, -0.1), lean=26.0, side=math.sin(a) * 8.0, head_pitch=-25.0, head_yaw=math.sin(a) * 14.0,
                foot_r=(0.06, 0.16, 0.0), foot_l=(-0.04, -0.16, 0.0),
                hand_r=(40.0, 0.24, 0.62), hand_l=(-40.0, 0.24, 0.64),
                sword_yaw=30.0, sword_pitch=-70.0, cape=6.0, cape_side=math.sin(a) * 5.0)


def parried(t):
    # 패링당해 칼이 튕겨 올라간다
    knocked = pose(root=(-0.12, 0.0, -0.04), lean=-16.0, head_pitch=14.0,
                   foot_r=(0.0, 0.16, 0.0), foot_l=(-0.2, -0.15, 0.0),
                   hand_r=(60.0, 0.24, 1.46), hand_l=(-80.0, 0.3, 0.96),
                   sword_yaw=150.0, sword_pitch=62.0, cape=-12.0)
    keys = [(0.0, knocked), (0.6, knocked), (1.0, STANCE)]
    return keyed(keys, t)


def death(t):
    kneel = pose(root=(-0.02, 0.0, -0.3), lean=34.0, head_pitch=-30.0,
                 foot_r=(0.2, 0.15, 0.0), foot_l=(-0.3, -0.15, 0.0),
                 hand_r=(40.0, 0.3, 0.42), hand_l=(-50.0, 0.28, 0.5),
                 sword_yaw=40.0, sword_pitch=-75.0, cape=4.0)
    fall = dict(kneel)
    fall['tumble'] = -82.0
    fall['pivot'] = (0.3, 0.0, 0.0)
    fall['lean'] = 10.0
    fall['head_pitch'] = 10.0
    fall['hand_r'] = (60.0, 0.5, 0.6)
    fall['sword_yaw'] = 70.0
    fall['sword_pitch'] = 30.0
    keys = [(0.0, STANCE), (0.35, kneel), (0.55, kneel), (0.85, fall), (1.0, fall)]
    p = keyed(keys, t)
    p['pivot'] = (0.3, 0.0, 0.0)
    return p


ANIMS = {
    'idle': (idle, [i / 6.0 for i in range(6)]),
    'run': (run, [i / 8.0 for i in range(8)]),
    'guard': (guard, [0.0]),
    'guard_walk': (guard_walk, [i / 6.0 for i in range(6)]),
    'parry': (parry, [0.0]),
    'parry_success': (parry_success, [0.0, 0.3, 0.6, 0.85]),
    'skill_101': (slash(-95.0, 80.0, 1.04, 0.8, 8.0, -12.0, 0.14), SLASH_TIMES),
    'skill_102': (slash(95.0, -80.0, 0.98, 0.86, 8.0, -12.0, 0.14), SLASH_TIMES),
    'skill_104': (slash(80.0, -75.0, 1.42, 0.6, 55.0, -35.0, 0.2, 0.04), SLASH_TIMES),
    'skill_105': (spin_combo, SPIN_TIMES),
    'skill_106': (thrust(0.0, 0.2, 0.0), ATTACK_TIMES),
    'skill_107': (whirl, [i / 12.0 for i in range(12)]),
    'skill_108': (leap, [0.0, 0.12, 0.24, 0.36, 0.45, 0.55, 0.7, 0.85]),
    'skill_109': (thrust(2.0, 0.24, 0.04), ATTACK_TIMES),
    'skill_110': (dodge, DODGE_TIMES),
    'skill_111': (roll, [0.0, 0.15, 0.3, 0.45, 0.6, 0.72, 0.9]),
    'hit': (hit, [0.0, 0.45, 0.75]),
    'stun': (stun, [i / 4.0 for i in range(4)]),
    'parried': (parried, [0.0, 0.6, 0.85]),
    'death': (death, [0.0, 0.25, 0.45, 0.65, 0.8, 1.0]),
}

# 0 ~ 0.4 예비 / 0.4 ~ 0.7 판정 / 0.7 ~ 1 회수 로 짠 것. 굽을 때 스킬 판정 창 시각으로 옮긴다
PHASE_ANIMS = {
    'skill_101': 101, 'skill_102': 102, 'skill_104': 104, 'skill_105': 105,
    'skill_106': 106, 'skill_108': 108, 'skill_109': 109,
}

# 반복 동작 한 바퀴 시간. run 계열은 초 대신 한 바퀴에 걷는 칸 수
LOOP_PERIOD = {'idle': 1.4, 'stun': 0.9, 'skill_107': 0.32}
# 초속 4 칸에 한 바퀴 2.6 칸이면 초당 3 걸음 ( 1.2 칸일 땐 6.7 걸음이라 잔발로 보였다 )
LOOP_STRIDE = {'run': 2.6, 'guard_walk': 1.1}

CANVAS = (112, 88, 56, 64)  # 폭, 높이, 발 x, 발 y
