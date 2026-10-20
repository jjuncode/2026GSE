# -*- coding: utf-8 -*-
# 스킬 판정 창을 게임 데이터에서 읽어 그림 시간과 맞춘다
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, 'SimpleGame', 'Data')

_skills = None
_lengths = None


def _load():
    global _skills, _lengths
    if _skills is not None:
        return
    with open(os.path.join(DATA, 'SkillData.json'), encoding='utf-8') as f:
        raw = json.load(f)
    items = raw if isinstance(raw, list) else list(raw.values())[0]
    _skills = {s['skill_id']: s for s in items}
    with open(os.path.join(DATA, 'AnimationData.json'), encoding='utf-8') as f:
        anims = json.load(f)['animations']
    _lengths = {a['name']: a['length'] for a in anims}


def skill_length(skill_id):
    _load()
    skill = _skills[skill_id]
    return float(_lengths.get(skill.get('anim_id', ''), 1.0))


def skill_windows(skill_id):
    # [ ( 시작, 끝 ) ... ] 동작 길이에 대한 비율
    _load()
    length = skill_length(skill_id)
    return [(w['start_sec'] / length, w['end_sec'] / length) for w in _skills[skill_id]['hit_windows']]


def phase_to_time(skill_id, t):
    # 0 ~ 0.4 예비 / 0.4 ~ 0.7 판정 / 0.7 ~ 1 회수 로 짠 그림 시각을 실제 동작 비율로
    windows = skill_windows(skill_id)
    if not windows:
        return t
    start = windows[0][0]
    end = windows[-1][1]
    if t <= 0.4:
        return t / 0.4 * start
    if t <= 0.7:
        return start + (t - 0.4) / 0.3 * (end - start)
    return end + (t - 0.7) / 0.3 * (1.0 - end)
