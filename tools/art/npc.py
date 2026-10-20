# -*- coding: utf-8 -*-
# NPC 다섯 직업. 같은 사람 뼈대에 옷 · 머리 · 등짐 · 손에 든 것만 갈아 끼운다.
# 색과 소품은 옛 도트 그림 ( tools/sprite_art.py 의 *_TOP ) 을 따른다
import math

import numpy as np

from pixel3d import Material, basis_from_axis, normalize, pitch_matrix, yaw_matrix
from rig import Builder, direction, face_point, keyed, pose, solve_humanoid, vec

BODY = dict(
    hip_z=0.84, hip_w=0.10, thigh=0.42, shin=0.42, ankle_z=0.07,
    spine=0.46, shoulder_w=0.21, upper_arm=0.27, forearm=0.26,
    head_r=0.19,
)

SCALE = 0.95  # 플레이어와 같은 키

COMMON = {
    'skin': Material([(118, 74, 62), (168, 116, 92), (210, 160, 126), (236, 196, 160)], outline=(60, 34, 30)),
    'cloth': Material([(28, 26, 34), (44, 42, 50), (64, 60, 66), (88, 84, 88)], outline=(14, 12, 18)),
    'boot': Material([(30, 22, 22), (52, 38, 32), (78, 56, 42)], outline=(14, 10, 10)),
    'leather': Material([(54, 36, 30), (88, 60, 42), (124, 88, 58), (164, 122, 82)], outline=(26, 16, 14)),
    'strap': Material([(34, 24, 22), (58, 40, 32), (82, 58, 42)], outline=(20, 12, 12)),
    'wood': Material([(56, 38, 26), (90, 62, 40), (128, 92, 58)], outline=(26, 16, 10)),
    'steel': Material([(52, 56, 70), (96, 104, 120), (152, 162, 178), (204, 212, 224)], outline=(22, 24, 32), metal=True),
    'gold': Material([(108, 70, 30), (168, 120, 48), (222, 180, 84)], outline=(50, 30, 14)),
}

LOOKS = {
    'hunter': dict(
        torso='moss', sleeve='moss', legs='cloth', head='hood', hood='hood', back='quiver', weapon='bow',
        mats={
            'moss': Material([(38, 48, 32), (58, 72, 44), (86, 104, 64), (114, 134, 84)], outline=(16, 20, 12)),
            'hood': Material([(54, 36, 28), (88, 62, 40), (124, 90, 58), (160, 124, 84)], outline=(24, 14, 10)),
            'string': Material([(200, 190, 160)], outline=(70, 60, 50), flat=True),
            'feather': Material([(170, 168, 160), (220, 218, 208)], outline=(70, 66, 60)),
        }),
    'raider': dict(
        torso='leather', sleeve='rag', legs='rag', head='mask', hood='rag', back=None, weapon='hatchet', hunch=8.0,
        mats={
            'rag': Material([(28, 26, 30), (44, 40, 44), (62, 58, 60), (82, 78, 78)], outline=(12, 10, 12)),
            'red': Material([(70, 16, 18), (112, 30, 30), (150, 44, 40)], outline=(30, 6, 8)),
        }),
    'merchant': dict(
        torso='robe', sleeve='robe', legs='robe', head='straw', hood=None, back='pack', weapon='staff', robe=True,
        mats={
            'robe': Material([(52, 26, 48), (80, 40, 72), (116, 62, 104), (148, 90, 132)], outline=(22, 10, 20)),
            'straw': Material([(110, 80, 36), (160, 124, 60), (214, 178, 96), (236, 208, 130)], outline=(50, 34, 14)),
            'canvas': Material([(84, 78, 64), (126, 118, 98), (166, 156, 132)], outline=(36, 32, 26)),
        }),
    'gatherer': dict(
        torso='linen', sleeve='linen', legs='linen', head='hood', hood='linen', back='basket', weapon='sickle', scarf='moss',
        mats={
            'linen': Material([(86, 80, 68), (128, 120, 102), (168, 158, 134), (198, 188, 162)], outline=(36, 32, 26)),
            'moss': Material([(38, 56, 34), (58, 84, 48), (86, 118, 66)], outline=(16, 24, 12)),
            'wicker': Material([(76, 54, 30), (116, 86, 48), (154, 118, 68)], outline=(34, 22, 10)),
            'herb': Material([(40, 90, 48), (72, 136, 72), (110, 176, 96)], outline=(16, 36, 18)),
        }),
    'warden': dict(
        torso='mail', sleeve='mail', legs='cloth', head='helmet', hood=None, back='cape', weapon='spear',
        mats={
            'mail': Material([(46, 48, 56), (72, 76, 86), (102, 106, 118), (132, 136, 148)], outline=(18, 18, 24)),
            'cloak': Material([(20, 48, 54), (30, 78, 78), (46, 112, 104), (80, 152, 134)], outline=(12, 22, 26)),
        }),
}

EYE = (24, 20, 26)


def materials_for(job):
    mats = dict(COMMON)
    mats.update(LOOKS[job]['mats'])
    return mats


# ------------------------------------------------------------ 소품

def add_spear(b, hand, d):
    b.capsule(hand - d * 0.7, hand + d * 1.15, 0.024, 0.022, 'wood', 1)
    b.capsule(hand + d * 1.13, hand + d * 1.38, 0.05, 0.006, 'steel', 2)


def add_hatchet(b, hand, d):
    b.capsule(hand - d * 0.08, hand + d * 0.42, 0.026, 0.024, 'wood', 1)
    side = normalize(np.cross(d, vec(0.0, 0.0, 1.0)))
    if np.linalg.norm(np.cross(d, vec(0.0, 0.0, 1.0))) < 1e-3:
        side = vec(0.0, 1.0, 0.0)
    rot = basis_from_axis(d, side)
    b.box(hand + d * 0.36 + rot @ vec(0.0, -0.08, 0.0), (0.1, 0.07, 0.014), rot @ yaw_matrix(90.0), 0.01, 'steel', 2)


def add_sickle(b, hand, d):
    b.capsule(hand - d * 0.06, hand + d * 0.16, 0.024, 0.022, 'wood', 1)
    up = normalize(np.cross(d, normalize(np.cross(vec(0.0, 0.0, 1.0), d) + vec(1e-4, 0.0, 0.0))))
    prev = hand + d * 0.16
    for k in range(1, 6):
        t = k / 5.0
        a = t * 2.4
        point = hand + d * (0.16 + math.sin(a) * 0.16) + up * (1.0 - math.cos(a)) * 0.12
        b.capsule(prev, point, 0.018, 0.012, 'steel', 2)
        prev = point


def add_staff(b, hand, d):
    b.capsule(hand - d * 0.4, hand + d * 1.0, 0.026, 0.022, 'wood', 1)


def add_bow(b, hand, forward, draw_point):
    # 활은 왼손에. 몸통이 앞으로 휘고 시위는 두 끝을 잇는다. 당기면 시위가 오른손까지
    up = vec(0.0, 0.0, 1.0)
    forward = normalize(forward - up * float(np.dot(forward, up)) + vec(1e-4, 0.0, 0.0))
    points = []
    for k in range(7):
        t = -1.0 + 2.0 * k / 6.0
        a = t * 1.05
        points.append(hand + up * math.sin(a) * 0.44 - forward * (1.0 - math.cos(a)) * 0.2)
    for p0, p1 in zip(points, points[1:]):
        b.capsule(p0, p1, 0.024, 0.024, 'wood', 1)
    top = points[-1]
    bottom = points[0]
    if draw_point is None:
        b.capsule(top, bottom, 0.006, 0.006, 'string', 2)
    else:
        b.capsule(top, draw_point, 0.006, 0.006, 'string', 2)
        b.capsule(draw_point, bottom, 0.006, 0.006, 'string', 2)
        # 걸어 둔 화살
        b.capsule(draw_point, draw_point + forward * 0.62, 0.01, 0.01, 'wood', 2)
        b.capsule(draw_point + forward * 0.6, draw_point + forward * 0.68, 0.022, 0.004, 'steel', 2)


# ------------------------------------------------------------ 몸

def make_build(job):
    look = LOOKS[job]

    def build(p, yaw_deg):
        b = Builder(yaw_deg + p['spin'], p['tumble'], p['pivot'])
        s = solve_humanoid(p, BODY)
        up = vec(0.0, 0.0, 1.0)
        cr = s.chest_rot
        hr = s.head_rot

        # 다리
        for hip, knee, ankle in ((s.hip_r, s.knee_r, s.ankle_r), (s.hip_l, s.knee_l, s.ankle_l)):
            b.capsule(hip, knee, 0.08, 0.066, look['legs'])
            b.capsule(knee, ankle, 0.07, 0.058, 'boot')
            b.ellipsoid(ankle + s.toe_dir * 0.07 - up * 0.035, (0.11, 0.062, 0.048), s.hips_rot, 'boot')

        # 허리와 몸통
        b.ellipsoid(s.pelvis, (0.13, 0.17, 0.12), s.hips_rot, look['legs'])
        b.ellipsoid(s.pelvis + cr @ vec(0.0, 0.0, 0.05), (0.14, 0.18, 0.035), s.hips_rot, 'strap')
        if look.get('robe'):
            # 발목까지 내려오는 겉옷
            b.ellipsoid(s.pelvis + s.hips_rot @ vec(0.0, 0.0, -0.36), (0.2, 0.23, 0.44), s.hips_rot, look['torso'])
        else:
            b.ellipsoid(s.pelvis + s.hips_rot @ vec(0.0, 0.0, -0.06), (0.15, 0.19, 0.12), s.hips_rot @ pitch_matrix(p['lean'] * 0.3), look['torso'])
        b.ellipsoid(s.belly, (0.13, 0.17, 0.14), cr, look['torso'])
        b.ellipsoid(s.chest, (0.15, 0.2, 0.15), cr, look['torso'])
        if look.get('scarf'):
            b.ellipsoid(s.neck + cr @ vec(0.0, 0.0, -0.05), (0.13, 0.15, 0.06), cr, look['scarf'])

        # 등짐
        back = look.get('back')
        if back == 'quiver':
            q_rot = cr @ pitch_matrix(-25.0) @ yaw_matrix(0.0)
            q_c = s.chest + cr @ vec(-0.17, -0.06, 0.0)
            b.box(q_c, (0.05, 0.06, 0.2), q_rot, 0.03, 'leather')
            for k in (-0.03, 0.0, 0.03):
                tip = q_c + q_rot @ vec(0.0, k, 0.2)
                b.capsule(tip, tip + q_rot @ vec(0.0, 0.0, 0.08), 0.022, 0.012, 'feather')
        elif back == 'pack':
            p_c = s.chest + cr @ vec(-0.24, 0.0, -0.04)
            b.box(p_c, (0.1, 0.18, 0.24), cr, 0.04, 'canvas')
            b.ellipsoid(p_c + cr @ vec(0.0, 0.0, 0.27), (0.08, 0.2, 0.07), cr @ pitch_matrix(90.0) @ yaw_matrix(90.0), 'leather')
            b.capsule(p_c + cr @ vec(-0.1, 0.17, -0.25), p_c + cr @ vec(-0.1, 0.17, 0.32), 0.018, 0.018, 'wood')
            b.capsule(p_c + cr @ vec(-0.1, -0.17, -0.25), p_c + cr @ vec(-0.1, -0.17, 0.32), 0.018, 0.018, 'wood')
        elif back == 'basket':
            k_c = s.chest + cr @ vec(-0.23, 0.0, -0.02)
            b.ellipsoid(k_c, (0.13, 0.17, 0.2), cr, 'wicker')
            for k, side in enumerate((-0.08, 0.0, 0.08)):
                sprout = k_c + cr @ vec(0.0, side, 0.16)
                b.capsule(sprout, sprout + cr @ vec(-0.04, side * 0.5, 0.12 + 0.03 * k), 0.035, 0.012, 'herb')
        elif back == 'cape':
            cape_rot = cr @ yaw_matrix(p['cape_side']) @ pitch_matrix(-p['cape'])
            cape_top = s.neck + cr @ vec(-0.13, 0.0, -0.05)
            b.box(cape_top + cape_rot @ vec(-0.02, 0.0, -0.3), (0.028, 0.2, 0.31), cape_rot, 0.02, 'cloak')
            b.ellipsoid(s.neck + cr @ vec(-0.02, 0.0, -0.06), (0.16, 0.24, 0.08), cr, 'cloak')

        # 팔
        for shoulder, elbow, hand in ((s.shoulder_r, s.elbow_r, s.hand_r), (s.shoulder_l, s.elbow_l, s.hand_l)):
            b.capsule(shoulder, elbow, 0.062, 0.052, look['sleeve'])
            b.capsule(elbow, hand, 0.056, 0.047, look['sleeve'])
            b.ellipsoid(hand, (0.052, 0.052, 0.052), np.eye(3), 'skin')
            if job == 'warden':
                b.ellipsoid(shoulder + cr @ vec(0.0, 0.0, 0.03), (0.09, 0.09, 0.07), cr, 'steel')

        # 머리
        head = s.head
        face_c = head + hr @ vec(0.07, 0.0, -0.03)
        face_r = np.array([0.13, 0.125, 0.135])
        b.ellipsoid(face_c, face_r, hr, 'skin')
        kind = look['head']
        if kind == 'hood':
            b.ellipsoid(head + hr @ vec(-0.035, 0.0, 0.015), (0.195, 0.19, 0.195), hr, look['hood'])
            b.capsule(head + hr @ vec(-0.11, 0.0, 0.08), head + hr @ vec(-0.24, 0.0, -0.04), 0.09, 0.03, look['hood'])
        elif kind == 'mask':
            b.ellipsoid(head + hr @ vec(-0.035, 0.0, 0.015), (0.195, 0.19, 0.195), hr, look['hood'])
            # 코 아래를 가린 붉은 두건
            b.ellipsoid(face_c + hr @ vec(0.02, 0.0, -0.06), (0.12, 0.13, 0.07), hr, 'red')
            b.capsule(head + hr @ vec(-0.16, 0.06, -0.06), head + hr @ vec(-0.3, 0.1, -0.16), 0.03, 0.015, 'red')
        elif kind == 'straw':
            b.ellipsoid(head + hr @ vec(-0.01, 0.0, 0.1), (0.15, 0.15, 0.1), hr, 'straw')
            b.ellipsoid(head + hr @ vec(-0.01, 0.0, 0.08), (0.34, 0.34, 0.035), hr, 'straw')
            b.ellipsoid(head + hr @ vec(-0.06, 0.0, -0.02), (0.15, 0.16, 0.15), hr, 'cloth')
        elif kind == 'helmet':
            b.ellipsoid(head + hr @ vec(-0.02, 0.0, 0.05), (0.19, 0.185, 0.17), hr, 'steel')
            b.ellipsoid(head + hr @ vec(-0.02, 0.0, -0.04), (0.205, 0.2, 0.03), hr, 'steel')
            b.capsule(head + hr @ vec(0.17, 0.0, 0.04), head + hr @ vec(0.2, 0.0, -0.06), 0.018, 0.014, 'steel')
        for side in (1.0, -1.0):
            b.decal(face_point(face_c, face_r, hr, (1.0, 0.42 * side, 0.04)), EYE)

        # 손에 든 것
        weapon = look['weapon']
        d = s.sword_dir
        if weapon == 'spear':
            add_spear(b, s.hand_r, d)
        elif weapon == 'hatchet':
            add_hatchet(b, s.hand_r, d)
        elif weapon == 'sickle':
            add_sickle(b, s.hand_r, d)
        elif weapon == 'staff':
            add_staff(b, s.hand_r, d)
        elif weapon == 'bow':
            draw_point = None
            if p['glow'] > 0.5:
                draw_point = s.hand_r
            # 왼팔을 앞으로 뻗었으면 그 방향으로 겨눈다
            bow_forward = s.chest_rot @ vec(1.0, 0.0, 0.0)
            if p['hand_l'][1] > 0.45:
                bow_forward = s.hand_l - s.shoulder_l
            add_bow(b, s.hand_l, bow_forward, draw_point)
        return b

    return build


# ------------------------------------------------------------ 동작

def stance_for(job):
    look = LOOKS[job]
    hunch = look.get('hunch', 0.0)
    weapon = look['weapon']
    sword_yaw = 30.0
    sword_pitch = -60.0
    hand_r = (50.0, 0.3, 0.78)
    if weapon == 'spear':
        # 창은 세워 든다
        sword_yaw = 10.0
        sword_pitch = 80.0
        hand_r = (55.0, 0.3, 0.9)
    elif weapon == 'staff':
        sword_yaw = 20.0
        sword_pitch = -84.0
        hand_r = (40.0, 0.36, 1.0)
    elif weapon == 'bow':
        sword_pitch = -60.0
    return pose(root=(0.0, 0.0, -0.02), lean=7.0 + hunch, foot_r=(0.06, 0.15, 0.0), foot_l=(-0.05, -0.14, 0.0),
                hand_r=hand_r, hand_l=(-60.0, 0.28, 0.8), sword_yaw=sword_yaw, sword_pitch=sword_pitch, cape=6.0)


def make_anims(job):
    look = LOOKS[job]
    weapon = look['weapon']
    stance = stance_for(job)

    def idle(t):
        breath = math.sin(t * 2.0 * math.pi)
        p = dict(stance)
        p['root'] = (0.0, 0.0, -0.02 + breath * 0.01)
        p['lean'] = stance['lean'] + breath * 1.5
        p['head_yaw'] = math.sin(t * 2.0 * math.pi) * 8.0
        return p

    def run(t):
        a = t * 2.0 * math.pi
        swing = math.cos(a)
        p = dict(stance)
        p['root'] = (0.02, 0.0, abs(swing) * 0.04 - 0.05)
        p['lean'] = stance['lean'] + 8.0
        p['twist'] = swing * 6.0
        p['hip_yaw'] = -swing * 8.0
        p['foot_r'] = (swing * 0.3 + 0.02, 0.12, max(0.0, -math.sin(a)) * 0.16)
        p['foot_l'] = (-swing * 0.3 + 0.02, -0.12, max(0.0, math.sin(a)) * 0.16)
        p['hand_l'] = (-75.0 - swing * 30.0, 0.28, 0.82)
        if weapon not in ('spear', 'staff'):
            p['hand_r'] = (65.0 - swing * 25.0, 0.3, 0.8)
        p['cape'] = 20.0 + math.sin(a * 2.0) * 4.0
        return p

    # 공격 - 0 에서 바로 내지르고 ( 피해는 그 순간 들어간다 ) 남은 시간은 회수
    if weapon == 'bow':
        aim = dict(stance)
        aim.update(lean=4.0, twist=-70.0, foot_r=(-0.12, 0.16, 0.0), foot_l=(0.14, -0.14, 0.0),
                   hand_l=(0.0, 0.55, 1.28), hand_r=(-10.0, 0.08, 1.32), glow=1.0, sword_pitch=0.0, sword_yaw=0.0)
        loose = dict(aim)
        loose.update(hand_r=(-150.0, 0.18, 1.3), glow=0.0)
        attack_keys = [(0.0, aim), (0.12, aim), (0.2, loose), (0.6, loose), (1.0, stance)]
    elif weapon == 'spear':
        pull = dict(stance)
        pull.update(root=(-0.04, 0.0, -0.06), lean=10.0, twist=30.0, hand_r=(150.0, 0.2, 1.0), hand_l=(10.0, 0.3, 1.0),
                    sword_yaw=0.0, sword_pitch=4.0, foot_r=(-0.08, 0.16, 0.0), foot_l=(0.12, -0.14, 0.0))
        jab = dict(pull)
        jab.update(root=(0.14, 0.0, -0.1), lean=24.0, twist=-10.0, hand_r=(4.0, 0.6, 1.0), hand_l=(-20.0, 0.45, 1.0),
                   foot_r=(0.3, 0.16, 0.0), foot_l=(-0.26, -0.14, 0.0))
        attack_keys = [(0.0, pull), (0.15, jab), (0.55, jab), (1.0, stance)]
    elif weapon == 'staff':
        lift = dict(stance)
        lift.update(twist=40.0, hand_r=(110.0, 0.3, 1.3), sword_yaw=120.0, sword_pitch=40.0)
        swing_pose = dict(stance)
        swing_pose.update(root=(0.1, 0.0, -0.06), lean=22.0, twist=-35.0, hand_r=(-40.0, 0.5, 0.9), sword_yaw=-60.0, sword_pitch=-10.0,
                          foot_r=(0.24, 0.15, 0.0), foot_l=(-0.18, -0.14, 0.0))
        attack_keys = [(0.0, lift), (0.15, swing_pose), (0.55, swing_pose), (1.0, stance)]
    else:
        # 손도끼 · 낫 - 위에서 비스듬히 내려친다
        raised = dict(stance)
        raised.update(root=(-0.03, 0.0, -0.04), twist=-25.0, hand_r=(-20.0, 0.22, 1.5), sword_yaw=-150.0, sword_pitch=45.0)
        chop = dict(stance)
        chop.update(root=(0.14, 0.0, -0.12), lean=34.0 + look.get('hunch', 0.0), twist=25.0, hand_r=(30.0, 0.55, 0.55),
                    sword_yaw=40.0, sword_pitch=-45.0, foot_r=(0.3, 0.16, 0.0), foot_l=(-0.2, -0.14, 0.0))
        attack_keys = [(0.0, raised), (0.15, chop), (0.55, chop), (1.0, stance)]

    def attack(t):
        return keyed(attack_keys, t)

    def hit(t):
        flinch = dict(stance)
        flinch.update(root=(-0.08, 0.0, -0.04), lean=-12.0, head_pitch=18.0, side=6.0, hand_l=(-90.0, 0.32, 0.98))
        return keyed([(0.0, flinch), (0.4, flinch), (1.0, stance)], t)

    def work(t):
        # 쪼그려 앉아 땅에서 캔다. 두 손이 번갈아 내려간다
        a = t * 2.0 * math.pi
        p = dict(stance)
        p.update(root=(0.0, 0.0, -0.36), lean=42.0, head_pitch=-20.0,
                 foot_r=(0.16, 0.17, 0.0), foot_l=(-0.12, -0.16, 0.0),
                 hand_r=(25.0, 0.5, 0.18 + max(0.0, math.sin(a)) * 0.14), hand_l=(-25.0, 0.5, 0.18 + max(0.0, -math.sin(a)) * 0.14),
                 sword_yaw=60.0, sword_pitch=-30.0)
        return p

    def death(t):
        kneel = dict(stance)
        kneel.update(root=(0.0, 0.0, -0.3), lean=36.0, head_pitch=-30.0, foot_r=(0.2, 0.15, 0.0), foot_l=(-0.3, -0.15, 0.0),
                     hand_r=(40.0, 0.3, 0.4), hand_l=(-50.0, 0.28, 0.46))
        fall = dict(kneel)
        fall.update(tumble=-82.0, lean=10.0, head_pitch=10.0, hand_r=(70.0, 0.5, 0.55))
        p = keyed([(0.0, stance), (0.3, kneel), (0.5, kneel), (0.85, fall), (1.0, fall)], t)
        p['pivot'] = (0.3, 0.0, 0.0)
        return p

    return {
        'idle': (idle, [i / 4.0 for i in range(4)]),
        'run': (run, [i / 6.0 for i in range(6)]),
        'attack': (attack, [0.0, 0.1, 0.2, 0.45, 0.7, 0.85]),
        'hit': (hit, [0.0, 0.45, 0.75]),
        'work': (work, [i / 4.0 for i in range(4)]),
        'death': (death, [0.0, 0.25, 0.45, 0.65, 0.85, 1.0]),
    }


def make_module(job):
    return dict(
        SCALE=SCALE,
        MATERIALS=materials_for(job),
        build=make_build(job),
        ANIMS=make_anims(job),
        LOOP_PERIOD={'idle': 1.8, 'work': 0.9},
        LOOP_STRIDE={'run': 1.8},
        CANVAS=(96, 80, 48, 60),
    )
