# -*- coding: utf-8 -*-
# 사람형 뼈대. 자세 값( 발 · 손 위치, 허리 숙임 )을 받아 관절을 IK 로 풀고 부피 덩어리를 세운다.
# 좌표는 캐릭터 기준 +x 앞, +y 오른쪽, +z 위. 게임 화면 투영이 거울상이라 오른쪽을 +y 로 둬야 화면에서 오른손잡이로 보인다
import math

import numpy as np

from pixel3d import Box, Capsule, Ellipsoid, basis_from_axis, normalize, pitch_matrix, roll_matrix, yaw_matrix


def vec(*values):
    return np.array(values, dtype=float)


def clamp01(t):
    return min(max(t, 0.0), 1.0)


def smooth(t):
    t = clamp01(t)
    return t * t * (3.0 - 2.0 * t)


def ease_out(t):
    t = clamp01(t)
    return 1.0 - (1.0 - t) * (1.0 - t)


def ease_in(t):
    t = clamp01(t)
    return t * t


def two_bone(root, target, upper, lower, pole):
    # 팔꿈치 · 무릎 자리. 끝이 안 닿으면 쭉 편 채로 그쪽을 향한다
    delta = target - root
    length = float(np.linalg.norm(delta))
    direction = delta / max(length, 1e-9)
    reach = min(max(length, 1e-4), (upper + lower) * 0.999)
    along = (upper * upper + reach * reach - lower * lower) / (2.0 * reach)
    side = math.sqrt(max(upper * upper - along * along, 0.0))
    bend = pole - direction * float(np.dot(pole, direction))
    if np.linalg.norm(bend) < 1e-6:
        bend = np.cross(direction, vec(0.0, 0.0, 1.0))
    bend = normalize(bend)
    joint = root + direction * along + bend * side
    end = root + direction * reach
    return joint, end


def polar(yaw_deg, dist, z):
    a = math.radians(yaw_deg)
    return vec(math.cos(a) * dist, math.sin(a) * dist, z)


def direction(yaw_deg, pitch_deg):
    y = math.radians(yaw_deg)
    p = math.radians(pitch_deg)
    return vec(math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))


# ------------------------------------------------------------ 자세

# 손은 골반 세로축 기준 극좌표 ( 각도, 거리, 높이 ) 라 휘두르는 사이 프레임이 몸을 뚫지 않고 호를 그린다
BASE_POSE = dict(
    root=(0.0, 0.0, 0.0),     # 골반 이동 ( 들썩임, 내딛기 )
    hip_yaw=0.0,
    lean=6.0,                 # 앞으로 숙임
    side=0.0,                 # 옆 기울임 ( + 오른쪽 )
    twist=0.0,                # 가슴 비틀기 ( + 오른쪽 )
    head_yaw=0.0,
    head_pitch=0.0,
    foot_r=(0.03, 0.14, 0.0),
    foot_l=(-0.03, -0.14, 0.0),
    hand_r=(55.0, 0.30, 0.78),
    hand_l=(-75.0, 0.28, 0.80),
    sword_yaw=25.0,           # 칼날 방향 ( + 오른쪽 )
    sword_pitch=-35.0,        # 칼날 들기 ( + 위 )
    sword_roll=0.0,
    cape=12.0,                # 망토가 뒤로 들린 각
    cape_side=0.0,
    glow=0.0,                 # 칼날 빛 ( 패링 창 )
    two_hand=0.0,             # 왼손을 손잡이로 ( 0 ~ 1 )
    spin=0.0,                 # 몸 전체 회전 ( 회전베기 )
    tumble=0.0,               # 몸 전체 앞뒤 굴림 ( 구르기, 쓰러짐 ). - 면 앞으로
    pivot=(0.0, 0.0, 0.5),    # 굴림 중심
)


def pose(**changes):
    result = dict(BASE_POSE)
    for key, value in changes.items():
        assert key in BASE_POSE, key
        result[key] = value
    return result


def mix_value(a, b, t):
    if a is None or b is None:
        if t < 0.5:
            return a
        return b
    if isinstance(a, tuple):
        return tuple(x + (y - x) * t for x, y in zip(a, b))
    return a + (b - a) * t


def mix(a, b, t):
    return {key: mix_value(a[key], b[key], t) for key in a}


def keyed(keys, t, ease=smooth):
    # keys = [ ( 시각, 자세 ) ... ] 시각 오름차순
    if t <= keys[0][0]:
        return dict(keys[0][1])
    for (t0, p0), (t1, p1) in zip(keys, keys[1:]):
        if t <= t1:
            u = (t - t0) / max(t1 - t0, 1e-9)
            return mix(p0, p1, ease(u))
    return dict(keys[-1][1])


# ------------------------------------------------------------ 덩어리 쌓기

class Builder:
    def __init__(self, yaw_deg, tumble_deg=0.0, pivot=(0.0, 0.0, 0.5), roll_deg=0.0):
        self.pivot = np.array(pivot, dtype=float)
        self.rot = yaw_matrix(yaw_deg) @ pitch_matrix(tumble_deg) @ roll_matrix(roll_deg)
        self.yaw_rot = yaw_matrix(yaw_deg)
        self.prims = []
        self.decals = []
        self.smears = []

    def at(self, p):
        local = np.asarray(p, dtype=float) - self.pivot
        return self.rot @ local + self.yaw_rot @ self.pivot

    def capsule(self, a, b, r0, r1, material, priority=0):
        self.prims.append(Capsule(self.at(a), self.at(b), r0, r1, material, priority))

    def ellipsoid(self, center, radii, local_rot, material, priority=0):
        self.prims.append(Ellipsoid(self.at(center), radii, self.rot @ local_rot, material, priority))

    def box(self, center, half, local_rot, round_r, material, priority=0):
        self.prims.append(Box(self.at(center), half, self.rot @ local_rot, round_r, material, priority))

    def decal(self, point, color):
        self.decals.append((self.at(point), color))

    def smear_box(self, center, half, local_rot):
        self.smears.append(Box(self.at(center), half, self.rot @ local_rot, 0.0, 'smear'))


class Skeleton:
    # 관절 위치만 푼 것. 덩어리 모양은 캐릭터마다 다르게 입힌다
    pass


def solve_humanoid(p, body):
    s = Skeleton()
    hips_rot = yaw_matrix(p['hip_yaw'])
    chest_rot = yaw_matrix(p['hip_yaw'] + p['twist']) @ roll_matrix(p['side']) @ pitch_matrix(-p['lean'])

    s.pelvis = vec(*p['root']) + vec(0.0, 0.0, body['hip_z'])
    s.hips_rot = hips_rot
    s.chest_rot = chest_rot
    s.belly = s.pelvis + chest_rot @ vec(0.0, 0.0, body['spine'] * 0.45)
    s.chest = s.pelvis + chest_rot @ vec(0.0, 0.0, body['spine'] * 0.8)
    s.neck = s.pelvis + chest_rot @ vec(0.0, 0.0, body['spine'] + 0.04)

    head_rot = chest_rot @ yaw_matrix(p['head_yaw'] - p['twist'] * 0.5) @ pitch_matrix(p['head_pitch'] + p['lean'] * 0.6)
    s.head_rot = head_rot
    s.head = s.neck + head_rot @ vec(0.0, 0.0, body['head_r'] * 0.95)

    shoulder_w = body['shoulder_w']
    s.shoulder_r = s.pelvis + chest_rot @ vec(-0.01, shoulder_w, body['spine'] - 0.03)
    s.shoulder_l = s.pelvis + chest_rot @ vec(-0.01, -shoulder_w, body['spine'] - 0.03)

    hand_r = s.pelvis + polar(*p['hand_r']) - vec(0.0, 0.0, body['hip_z'])
    hand_l = s.pelvis + polar(*p['hand_l']) - vec(0.0, 0.0, body['hip_z'])
    hand_r[2] = p['hand_r'][2] + p['root'][2]
    hand_l[2] = p['hand_l'][2] + p['root'][2]

    s.sword_dir = direction(p['sword_yaw'], p['sword_pitch'])
    grip = hand_r - s.sword_dir * 0.11
    two = p['two_hand']
    hand_l = hand_l * (1.0 - two) + grip * two

    elbow_pole_r = chest_rot @ vec(-0.6, 0.7, -0.5)
    elbow_pole_l = chest_rot @ vec(-0.6, -0.7, -0.5)
    s.elbow_r, s.hand_r = two_bone(s.shoulder_r, hand_r, body['upper_arm'], body['forearm'], elbow_pole_r)
    s.elbow_l, s.hand_l = two_bone(s.shoulder_l, hand_l, body['upper_arm'], body['forearm'], elbow_pole_l)

    hip_w = body['hip_w']
    s.hip_r = s.pelvis + hips_rot @ vec(0.0, hip_w, -0.04)
    s.hip_l = s.pelvis + hips_rot @ vec(0.0, -hip_w, -0.04)
    ankle_r = vec(*p['foot_r']) + vec(0.0, 0.0, body['ankle_z'])
    ankle_l = vec(*p['foot_l']) + vec(0.0, 0.0, body['ankle_z'])
    knee_pole_r = hips_rot @ vec(1.0, 0.25, 0.0)
    knee_pole_l = hips_rot @ vec(1.0, -0.25, 0.0)
    s.knee_r, s.ankle_r = two_bone(s.hip_r, ankle_r, body['thigh'], body['shin'], knee_pole_r)
    s.knee_l, s.ankle_l = two_bone(s.hip_l, ankle_l, body['thigh'], body['shin'], knee_pole_l)

    # 발끝은 골반이 보는 쪽
    s.toe_dir = hips_rot @ vec(1.0, 0.0, 0.0)

    return s


def face_point(center, radii, rot, local_dir):
    # 타원체 겉면 위의 점 ( 눈 찍을 자리 )
    d = normalize(np.array(local_dir, dtype=float))
    k = 1.0 / math.sqrt(float(np.sum((d / radii) ** 2)))
    return center + rot @ (d * k)


def add_sword(b, hand, blade_dir, roll_deg, mats, blade_len, glow):
    rot = basis_from_axis(blade_dir)
    rot = rot @ yaw_matrix(roll_deg)
    blade_mat = mats['blade']
    if glow > 0.5:
        blade_mat = mats['blade_glow']
    b.capsule(hand - blade_dir * 0.07, hand + blade_dir * 0.07, 0.03, 0.03, mats['grip'], 1)
    b.ellipsoid(hand - blade_dir * 0.09, (0.035, 0.035, 0.035), np.eye(3), mats['guard'], 1)
    b.box(hand + blade_dir * 0.085, (0.11, 0.028, 0.022), rot, 0.008, mats['guard'], 1)
    b.box(hand + blade_dir * (0.10 + blade_len * 0.5), (0.042, 0.012, blade_len * 0.5), rot, 0.008, blade_mat, 2)
    # 칼끝은 가늘게
    b.capsule(hand + blade_dir * (0.10 + blade_len), hand + blade_dir * (0.10 + blade_len + 0.07), 0.035, 0.01, blade_mat, 2)


# ------------------------------------------------------------ 스킬 시간표

def strike_keys(windows, raise_pose, hit_pose, stance):
    # 판정 창마다 들어 올림 -> 휘두름 을 끼워 넣는다
    keys = [(0.0, stance)]
    for index, (start, end) in enumerate(windows):
        r = raise_pose(index)
        h = hit_pose(index)
        before = start - 0.04
        if index == 0:
            keys.append((max(start * 0.7, 0.02), r))
        else:
            keys.append((max(before, keys[-1][0] + 0.01), r))
        keys.append((start, r))
        keys.append((end, h))
        keys.append((end + 0.03, h))
    keys.append((min(keys[-1][0] + 0.15, 0.97), stance))
    keys.append((1.0, stance))
    keys.sort(key=lambda k: k[0])
    return keys


def skill_times(keys, windows):
    # 열쇠 자세마다 한 장 + 휘두르는 한가운데 한 장
    times = set(round(k[0], 3) for k in keys[:-1])
    for start, end in windows:
        times.add(round((start + end) * 0.5, 3))
    return sorted(times)
