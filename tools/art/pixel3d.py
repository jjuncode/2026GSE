# -*- coding: utf-8 -*-
# 부피로 세운 캐릭터를 게임과 같은 아이소 각도에서 비춰 도트로 떨군다.
# 3 배로 촘촘히 쏜 뒤 칸마다 가장 많이 맞은 색을 골라 계단을 정리하고, 툰 셰이딩 · 안쪽 경계선 · 바깥 외곽선을 입힌다
import math

import numpy as np

# 게임 투영 ( WorldPos::WorldToScreen ) 을 미터로 옮긴 것. 1 칸 = 1.5 m, 칸 폭 32 도트
PX_X = 16.0 / 1.5          # 화면 가로 = ( x - y ) * PX_X
PX_Y = 8.0 / 1.5           # 화면 세로( 아래 + ) = ( x + y ) * PX_Y - z * PX_Z
PX_Z = 2.0 * PX_Y / 0.8165  # 30 도 내려다보는 직교 카메라라 세로 높이는 0.866 배
VIEW = np.array([1.0, 1.0, 0.8165])
VIEW = VIEW / np.linalg.norm(VIEW)  # 물체에서 카메라 쪽
ORIGIN_BACK = 12.0  # 광선 출발점. 높이로 6 m - 이보다 높은 것은 잘린다 ( 4 였을 때 2 m 위가 다 잘렸다 )

LIGHT = np.array([0.25, 0.6, 0.9])
LIGHT = LIGHT / np.linalg.norm(LIGHT)  # 화면 왼쪽 위 앞에서 오는 해. 카메라 쪽 면이 밝아야 얼굴이 읽힌다

SUPER = 3
MAX_STEPS = 72
HIT_EPS = 0.004
FAR = 30.0
SPEC_BAND = 15


def normalize(v):
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.maximum(n, 1e-9)


def yaw_matrix(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]])


def pitch_matrix(deg):
    # 앞( +x )을 위( +z )로 드는 회전. 음수면 숙인다
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0.0, -s], [0.0, 1.0, 0.0], [s, 0.0, c]])


def roll_matrix(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]])


def basis_from_axis(axis, up_hint=(0.0, 0.0, 1.0)):
    # 축 하나로 3 x 3 회전을 만든다. 열 = 로컬 x, y, z. z 가 주어진 축
    z = normalize(np.array(axis, dtype=float))
    hint = normalize(np.array(up_hint, dtype=float))
    if abs(float(np.dot(z, hint))) > 0.95:
        hint = np.array([1.0, 0.0, 0.0])
    x = normalize(np.cross(hint, z))
    y = np.cross(z, x)
    return np.stack([x, y, z], axis=1)


# ------------------------------------------------------------ 거리 함수

class Prim:
    def __init__(self, material, priority=0):
        self.material = material
        self.priority = priority  # 얇은 것( 칼날 )은 덜 맞아도 남긴다


class Capsule(Prim):
    def __init__(self, a, b, r0, r1, material, priority=0):
        Prim.__init__(self, material, priority)
        self.a = np.array(a, dtype=float)
        self.b = np.array(b, dtype=float)
        self.r0 = float(r0)
        self.r1 = float(r1)

    def dist(self, p):
        ba = self.b - self.a
        pa = p - self.a
        denom = max(float(np.dot(ba, ba)), 1e-9)
        h = np.clip((pa @ ba) / denom, 0.0, 1.0)
        r = self.r0 + (self.r1 - self.r0) * h
        return np.linalg.norm(pa - ba[None, :] * h[:, None], axis=1) - r

    def bounds(self):
        r = max(self.r0, self.r1)
        return np.minimum(self.a, self.b) - r, np.maximum(self.a, self.b) + r


class Ellipsoid(Prim):
    def __init__(self, center, radii, rot, material, priority=0):
        Prim.__init__(self, material, priority)
        self.c = np.array(center, dtype=float)
        self.r = np.array(radii, dtype=float)
        self.rot = np.array(rot, dtype=float)

    def dist(self, p):
        q = (p - self.c) @ self.rot
        k0 = np.linalg.norm(q / self.r, axis=1)
        k1 = np.linalg.norm(q / (self.r * self.r), axis=1)
        return k0 * (k0 - 1.0) / np.maximum(k1, 1e-9)

    def bounds(self):
        m = float(np.max(self.r))
        return self.c - m, self.c + m


class Box(Prim):
    def __init__(self, center, half, rot, round_r, material, priority=0):
        Prim.__init__(self, material, priority)
        self.c = np.array(center, dtype=float)
        self.h = np.array(half, dtype=float)
        self.rot = np.array(rot, dtype=float)
        self.round_r = float(round_r)

    def dist(self, p):
        q = np.abs((p - self.c) @ self.rot) - (self.h - self.round_r)
        outside = np.linalg.norm(np.maximum(q, 0.0), axis=1)
        inside = np.minimum(np.max(q, axis=1), 0.0)
        return outside + inside - self.round_r

    def bounds(self):
        m = float(np.linalg.norm(self.h))
        return self.c - m, self.c + m


class Convex(Prim):
    # 평면 여럿의 교집합 ( 지붕, 천막, 수정 ). planes = [ ( 로컬 법선, 거리 ) ]. 바깥 거리는 실제보다 짧게 나와 광선이 안 넘어간다
    def __init__(self, center, rot, planes, radius, material, priority=0):
        Prim.__init__(self, material, priority)
        self.c = np.array(center, dtype=float)
        self.rot = np.array(rot, dtype=float)
        normals = np.array([normalize(np.array(n, dtype=float)) for n, _ in planes])
        self.n = normals
        self.d = np.array([float(d) for _, d in planes])
        self.radius = float(radius)

    def dist(self, p):
        q = (p - self.c) @ self.rot
        return np.max(q @ self.n.T - self.d[None, :], axis=1)

    def bounds(self):
        return self.c - self.radius, self.c + self.radius


class Material:
    def __init__(self, ramp, outline=None, metal=False, emissive=False, flat=False, pattern=None):
        self.ramp = [tuple(int(v) for v in c) for c in ramp]  # 어두운 쪽부터
        if outline is None:
            base = ramp[0]
            outline = (int(base[0] * 0.45), int(base[1] * 0.4), int(base[2] * 0.45))
        self.outline = tuple(int(v) for v in outline)
        self.metal = metal
        self.emissive = emissive
        self.flat = flat
        self.pattern = pattern  # ( 점, 법선 ) -> 밝기 단 더하기. 벽돌 줄눈, 기와, 판자


# ------------------------------------------------------------ 광선

def scene_dist(prims, p):
    best = np.full(p.shape[0], 1e9)
    index = np.full(p.shape[0], -1, dtype=int)
    for i, prim in enumerate(prims):
        d = prim.dist(p)
        closer = d < best
        best = np.where(closer, d, best)
        index = np.where(closer, i, index)
    return best, index


def ray_origins(width, height, anchor_x, anchor_y, super_n, scale=1.0):
    xs = (np.arange(width * super_n) + 0.5) / super_n - anchor_x
    ys = (np.arange(height * super_n) + 0.5) / super_n - anchor_y
    gx, gy = np.meshgrid(xs, ys)
    a = gx.ravel() / (PX_X * scale)
    b = gy.ravel() / (PX_Y * scale)
    ground = np.stack([(a + b) * 0.5, (b - a) * 0.5, np.zeros_like(a)], axis=1)
    return ground + VIEW[None, :] * ORIGIN_BACK


def project(point, anchor_x, anchor_y, scale=1.0):
    x, y, z = point
    sx = (x - y) * PX_X * scale + anchor_x
    sy = ((x + y) * PX_Y - z * PX_Z) * scale + anchor_y
    return sx, sy


def depth_of(point):
    # 광선 시작점에서 그 점까지 거리. 그린 깊이와 같은 기준
    return ORIGIN_BACK - float(point[2]) / float(VIEW[2])


def march(prims, origins):
    direction = -VIEW
    count = origins.shape[0]
    hit = np.zeros(count, dtype=bool)
    index = np.full(count, -1, dtype=int)

    # 부피 상자를 안 지나는 광선은 처음부터 버린다
    lo = np.min([p.bounds()[0] for p in prims], axis=0) - 0.05
    hi = np.max([p.bounds()[1] for p in prims], axis=0) + 0.05
    with np.errstate(divide='ignore', invalid='ignore'):
        inv = 1.0 / direction
        t0 = (lo[None, :] - origins) * inv[None, :]
        t1 = (hi[None, :] - origins) * inv[None, :]
    tmin = np.max(np.minimum(t0, t1), axis=1)
    tmax = np.min(np.maximum(t0, t1), axis=1)
    alive = tmax > np.maximum(tmin, 0.0)
    t = np.where(alive, np.maximum(tmin, 0.0), 0.0)

    for _ in range(MAX_STEPS):
        ids = np.nonzero(alive)[0]
        if ids.size == 0:
            break
        p = origins[ids] + direction[None, :] * t[ids, None]
        d, idx = scene_dist(prims, p)
        done = d < HIT_EPS
        hit[ids[done]] = True
        index[ids[done]] = idx[done]
        alive[ids[done]] = False
        t[ids] += np.maximum(d, HIT_EPS * 0.5)
        gone = (t[ids] > np.minimum(tmax[ids], FAR)) & ~done
        alive[ids[gone]] = False

    return hit, t, index


def normals(prims, points):
    e = 0.004
    k = np.array([[1, -1, -1], [-1, -1, 1], [-1, 1, -1], [1, 1, 1]], dtype=float)
    n = np.zeros_like(points)
    for row in k:
        d, _ = scene_dist(prims, points + row[None, :] * e)
        n += row[None, :] * d[:, None]
    return normalize(n)


# ------------------------------------------------------------ 그리기

def render(prims, materials, width, height, anchor_x, anchor_y, decals=(), smears=(), smear_colors=None, scale=1.0, outline=True):
    names = list(materials.keys())
    mats = [materials[n] for n in names]
    prim_mat = np.array([names.index(p.material) for p in prims])
    prim_prio = np.array([p.priority for p in prims])

    origins = ray_origins(width, height, anchor_x, anchor_y, SUPER, scale)
    hit, t, index = march(prims, origins)

    total = origins.shape[0]
    key = np.full(total, -1, dtype=int)  # 재질 x 16 + 밝기 단
    prio = np.zeros(total, dtype=int)
    hit_ids = np.nonzero(hit)[0]

    if hit_ids.size:
        points = origins[hit_ids] - VIEW[None, :] * t[hit_ids, None]
        norm = normals(prims, points)
        lambert = norm @ LIGHT
        rim = (norm @ VIEW) < 0.35
        m_ids = prim_mat[index[hit_ids]]
        bands = np.zeros(hit_ids.size, dtype=int)
        for mi, m in enumerate(mats):
            sel = m_ids == mi
            if not np.any(sel):
                continue
            steps = len(m.ramp)
            if m.emissive:
                bands[sel] = steps - 1
                continue
            if m.flat:
                bands[sel] = steps // 2
                continue
            level = 0.5 + 0.5 * lambert[sel] - 0.12 * rim[sel]
            b = (level * steps).astype(int)
            if m.pattern is not None:
                b = b + m.pattern(points[sel], norm[sel])
            b = np.clip(b, 0, steps - 1)
            if m.metal:
                # 반사 방향이 눈과 거의 맞는 곳만 번쩍. 평평한 면이 통째로 하얗게 뜨지 않게 문턱을 높게
                half = LIGHT + VIEW
                half = half / np.linalg.norm(half)
                spec = norm[sel] @ half
                b = np.where(spec > 0.985, SPEC_BAND, b)
            bands[sel] = b
        key[hit_ids] = m_ids * 16 + bands
        prio[hit_ids] = prim_prio[index[hit_ids]]

    sw = width * SUPER
    sh = height * SUPER
    depth = np.where(hit, t, np.inf)

    # 칸마다 다수결. 3 x 3 = 9 표 중 4 표 넘게 맞아야 칠하고, 얇은 것은 2 표면 남긴다
    def blocks(a):
        return a.reshape(height, SUPER, width, SUPER).transpose(0, 2, 1, 3).reshape(height * width, SUPER * SUPER)

    bk = blocks(key.reshape(sh, sw))
    bp = blocks(prio.reshape(sh, sw))
    bd = blocks(depth.reshape(sh, sw))
    covered = bk >= 0
    count = covered.sum(axis=1)
    has_prio = (bp * covered).max(axis=1) > 0
    need = np.where(has_prio, 2, 4)
    solid = count >= need

    same = (bk[:, :, None] == bk[:, None, :]) & covered[:, :, None]
    votes = same.sum(axis=2) + bp * 20
    votes = np.where(covered, votes, -1)
    pick = np.argmax(votes, axis=1)
    rows = np.arange(bk.shape[0])
    out_key = np.where(solid, bk[rows, pick], -1).reshape(height, width)
    out_depth = np.where(solid, bd[rows, pick], np.inf).reshape(height, width)

    out = np.zeros((height, width, 4), dtype=np.uint8)

    def key_color(k):
        m = mats[k // 16]
        band = k % 16
        if band == SPEC_BAND:
            return (238, 242, 250)
        return m.ramp[min(band, len(m.ramp) - 1)]

    for y, x in zip(*np.nonzero(out_key >= 0)):
        out[y, x, :3] = key_color(int(out_key[y, x]))
        out[y, x, 3] = 255

    out_mat = np.where(out_key >= 0, out_key // 16, -1)

    # 안쪽 경계선. 앞에 있는 덩어리 둘레의 뒤쪽 칸을 그 재질의 가장 어두운 색으로 - 팔과 몸이 한 덩어리로 안 뭉친다
    edges = np.zeros((height, width), dtype=bool)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        shifted = np.full((height, width), np.inf)
        dst_y = slice(max(-dy, 0), height + min(-dy, 0))
        dst_x = slice(max(-dx, 0), width + min(-dx, 0))
        src_y = slice(max(dy, 0), height + min(dy, 0))
        src_x = slice(max(dx, 0), width + min(dx, 0))
        shifted[dst_y, dst_x] = out_depth[src_y, src_x]
        with np.errstate(invalid='ignore'):
            edges |= (out_key >= 0) & np.isfinite(shifted) & (out_depth - shifted > 0.16)
    for y, x in zip(*np.nonzero(edges)):
        m = mats[out_mat[y, x]]
        if m.emissive:
            continue
        out[y, x, :3] = m.ramp[0]

    # 장식점 ( 눈, 버클 ). 그 자리 표면이 보일 때만 찍는다
    for point, decal_color in decals:
        sx, sy = project(point, anchor_x, anchor_y, scale)
        x = int(math.floor(sx))
        y = int(math.floor(sy))
        if 0 <= x < width and 0 <= y < height and out_key[y, x] >= 0:
            if depth_of(point) <= out_depth[y, x] + 0.07:
                out[y, x, :3] = decal_color

    # 잔상. 캐릭터 뒤 빈칸에만 칠한다
    smear_mask = np.zeros((height, width), dtype=bool)
    if smears:
        origins_1 = ray_origins(width, height, anchor_x, anchor_y, 1, scale)
        s_hit, _, _ = march(list(smears), origins_1)
        smear_mask = s_hit.reshape(height, width) & (out[:, :, 3] == 0)
        if smear_colors is None:
            smear_colors = [(255, 255, 255), (170, 210, 255)]
        for y, x in zip(*np.nonzero(smear_mask)):
            c = smear_colors[0]
            for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
                yy, xx = y + dy, x + dx
                if not (0 <= yy < height and 0 <= xx < width) or not smear_mask[yy, xx]:
                    c = smear_colors[1]
            out[y, x, :3] = c
            out[y, x, 3] = 255

    # 바깥 외곽선. 투명한 칸 중 몸에 닿은 칸을 닿은 재질의 외곽 색으로.
    # 이어 붙이는 블록은 끈다 - 이웃 블록 사이에 줄이 생긴다
    if outline == False:
        return out
    body = out_key >= 0
    has = np.zeros((height, width), dtype=bool)
    for y, x in zip(*np.nonzero(body)):
        m = mats[out_mat[y, x]]
        if m.emissive:
            continue
        for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
            yy, xx = y + dy, x + dx
            if 0 <= yy < height and 0 <= xx < width and out[yy, xx, 3] == 0 and not has[yy, xx]:
                out[yy, xx, :3] = m.outline
                out[yy, xx, 3] = 255
                has[yy, xx] = True

    return out
