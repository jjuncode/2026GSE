# -*- coding: utf-8 -*-
# ZoneGen.cpp 를 그대로 옮긴 오프라인 검증. 연결 보장과 칸 종류 비율만 본다
import collections
import math

M = 0xFFFFFFFF
N = 96
BORDER = 2
OPEN, CLIFF, WATER, ROCK, PALM = 0, 1, 2, 3, 4


def hz(seed, x, y):
    h = (seed * 2654435761) & M
    h ^= ((x & M) * 374761393) & M
    h = ((h ^ (h >> 13)) * 1274126177) & M
    h ^= ((y & M) * 668265263) & M
    h = ((h ^ (h >> 16)) * 2246822519) & M
    return h ^ (h >> 15)


def unit(h):
    return (h & 0xFFFF) / 65535.0


def idx(x, y):
    return y * N + x


def inside(x, y):
    return 0 <= x < N and 0 <= y < N


def border(x, y):
    return x < BORDER or y < BORDER or x >= N - BORDER or y >= N - BORDER


def is_open(t, x, y):
    return inside(x, y) and t[idx(x, y)] == OPEN


def measure(t, s):
    d = [-1] * (N * N)
    if not is_open(t, *s):
        return d
    q = collections.deque([s])
    d[idx(*s)] = 0
    while q:
        cx, cy = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = cx + dx, cy + dy
            if is_open(t, nx, ny) and d[idx(nx, ny)] < 0:
                d[idx(nx, ny)] = d[idx(cx, cy)] + 1
                q.append((nx, ny))
    return d


def carve_circle(t, c, r):
    for oy in range(-r, r + 1):
        for ox in range(-r, r + 1):
            if ox * ox + oy * oy > r * r:
                continue
            x, y = c[0] + ox, c[1] + oy
            if inside(x, y) and not border(x, y):
                t[idx(x, y)] = OPEN


def sign(v):
    return (v > 0) - (v < 0)


def isolated(t, x, y):
    return all(is_open(t, x + ox, y + oy) for oy in (-1, 0, 1) for ox in (-1, 0, 1))


def gen(seed):
    t = [OPEN] * (N * N)
    spawn = (N // 2, N // 2)
    for y in range(N):
        for x in range(N):
            t[idx(x, y)] = CLIFF if border(x, y) or unit(hz(seed, x, y)) < 0.43 else OPEN
    for _ in range(4):
        src = t[:]
        for y in range(BORDER, N - BORDER):
            for x in range(BORDER, N - BORDER):
                c = sum(1 for oy in (-1, 0, 1) for ox in (-1, 0, 1) if (ox or oy) and not is_open(src, x + ox, y + oy))
                if c >= 5:
                    t[idx(x, y)] = CLIFF
                elif c <= 3:
                    t[idx(x, y)] = OPEN
    carve_circle(t, spawn, 5)
    for _ in range(256):
        sd = measure(t, spawn)
        orphan = next(((x, y) for y in range(N) for x in range(N) if is_open(t, x, y) and sd[idx(x, y)] < 0), None)
        if orphan is None:
            break
        rd = measure(t, orphan)
        if sum(1 for v in rd if v >= 0) < 24:
            for i, v in enumerate(rd):
                if v >= 0:
                    t[i] = CLIFF
            continue
        cx, cy = orphan
        mx = True
        for _g in range(N * 4):
            i = idx(cx, cy)
            if sd[i] >= 0:
                break
            t[i] = OPEN
            sx, sy = (cx, cy + 1) if mx else (cx + 1, cy)
            if not border(sx, sy):
                t[idx(sx, sy)] = OPEN
            dx, dy = spawn[0] - cx, spawn[1] - cy
            if dx == 0 and dy == 0:
                break
            if abs(dx) >= abs(dy):
                cx += sign(dx)
                mx = True
            else:
                cy += sign(dy)
                mx = False
    d = measure(t, spawn)
    for i in range(N * N):
        if t[i] == OPEN and d[i] < 0:
            t[i] = CLIFF
    d = measure(t, spawn)
    best = max(range(N * N), key=lambda i: d[i])
    boss = (best % N, best // N)
    carve_circle(t, boss, 4)

    # 분류
    visited = [False] * (N * N)
    ponds = 0
    for y in range(N):
        for x in range(N):
            if visited[idx(x, y)] or t[idx(x, y)] != CLIFF:
                continue
            region = []
            touches = False
            q = collections.deque([(x, y)])
            visited[idx(x, y)] = True
            while q:
                cx, cy = q.popleft()
                region.append((cx, cy))
                touches = touches or border(cx, cy)
                for oy in (-1, 0, 1):
                    for ox in (-1, 0, 1):
                        nx, ny = cx + ox, cy + oy
                        if inside(nx, ny) and not visited[idx(nx, ny)] and t[idx(nx, ny)] == CLIFF:
                            visited[idx(nx, ny)] = True
                            q.append((nx, ny))
            if touches:
                continue
            kind = CLIFF
            if len(region) <= 3:
                kind = ROCK
            elif 12 <= len(region) <= 180 and unit(hz((seed + 404) & M, region[0][0], region[0][1])) < 0.3:
                kind = WATER
                ponds += 1
            for cx, cy in region:
                t[idx(cx, cy)] = kind

    palms = 0
    for y in range(N):
        for x in range(N):
            if not isolated(t, x, y):
                continue
            near = any(inside(x + ox, y + oy) and t[idx(x + ox, y + oy)] == WATER for oy in range(-2, 3) for ox in range(-2, 3))
            if near and unit(hz((seed + 303) & M, x, y)) < 0.12:
                t[idx(x, y)] = PALM
                palms += 1

    d = measure(t, spawn)
    open_count = sum(1 for v in t if v == OPEN)
    reach = sum(1 for v in d if v >= 0)
    counts = collections.Counter(t)
    return dict(reach_ok=open_count == reach, open=round(counts[OPEN] / (N * N), 2), cliff=round(counts[CLIFF] / (N * N), 2),
                water=counts[WATER], ponds=ponds, rock=counts[ROCK], palms=palms)


for seed in [1, 42, 2026, 99999, 123456789]:
    print(seed, gen(seed))
