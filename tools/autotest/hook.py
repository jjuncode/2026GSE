# -*- coding: utf-8 -*-
# 시험 장치를 게임에 붙이고 뗀다. python hook.py on | off
# on  - AutoTest.cpp/.h 를 SimpleGame 에 복사하고, 시작 · 고정 간격 갱신 · 프레임 끝에 걸고, 프로젝트에 넣는다
# off - 전부 되돌리고 복사본과 캡처 BMP 를 지운다. 확인이 끝나면 반드시 off 하고 Debug · Release 를 다시 빌드한다
import glob
import io
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'SimpleGame')
CRLF = chr(13) + chr(10)
NL = chr(10)

# ( 파일, 붙이기 전, 붙인 뒤 )
HOOKS = [
    ('SimpleGame.cpp',
     '#include "UiWindows.h"' + NL,
     '#include "UiWindows.h"' + NL + '#include "AutoTest.h"' + NL),
    ('SimpleGame.cpp',
     '\t\tApplyInput(FIXED_STEP_SECONDS);' + NL + '\t\tUpdateScene(FIXED_STEP_SECONDS);',
     '\t\tApplyInput(FIXED_STEP_SECONDS);' + NL + '\t\tUpdateAutoTest(FIXED_STEP_SECONDS);' + NL + '\t\tUpdateScene(FIXED_STEP_SECONDS);'),
    ('SimpleGame.cpp',
     '\tDrawHud(g_renderer);' + NL + NL + '\t// 프레임 경계.',
     '\tDrawHud(g_renderer);' + NL + '\tFlushRenderBatch();' + NL + NL +
     '\tstd::string capture_name;' + NL +
     '\twhile (PopCaptureRequest(&capture_name))' + NL +
     '\t{' + NL +
     '\t\tSaveFrameBmp(capture_name, WINDOW_SIZE_X, WINDOW_SIZE_Y);' + NL +
     '\t}' + NL + NL + '\t// 프레임 경계.'),
    ('SimpleGame.cpp',
     '\tInitScene();' + NL + '\tResetFrameClock();',
     '\tInitScene();' + NL + '\tInitAutoTest();' + NL + '\tResetFrameClock();'),
    ('Scene.h',
     'bool IsPlayerNearAnvil();',
     'bool IsPlayerNearAnvil();' + NL + 'WorldPos GetAnvilPos();'),
]

ANVIL_FUNC = NL + 'WorldPos GetAnvilPos()' + NL + '{' + NL + '\treturn g_anvil_pos;' + NL + '}' + NL
PROJECT_ANCHOR = '    <ClCompile Include="Combat.cpp" />' + CRLF
PROJECT_LINE = '    <ClCompile Include="AutoTest.cpp" />' + CRLF


def read(name):
    return io.open(os.path.join(GAME, name), encoding='utf-8-sig', newline='').read()


def write(name, text):
    io.open(os.path.join(GAME, name), 'w', encoding='utf-8-sig', newline='').write(text)


def is_on():
    return '#include "AutoTest.h"' in read('SimpleGame.cpp')


def hook_on():
    if is_on():
        sys.exit('already on')

    # 전부 메모리에서 바꾼 뒤 한 번에 쓴다 - 중간에 실패하면 아무것도 안 바뀐다
    texts = {}
    for name, before, after in HOOKS:
        text = texts.get(name, read(name))
        assert text.count(before) == 1, (name, before[:60], text.count(before))
        texts[name] = text.replace(before, after)

    scene = read('Scene.cpp')
    assert 'WorldPos GetAnvilPos()' not in scene
    texts['Scene.cpp'] = scene.rstrip(NL) + NL + ANVIL_FUNC

    project = io.open(os.path.join(GAME, 'SimpleGame.vcxproj'), 'rb').read().decode('utf-8-sig')
    assert project.count(PROJECT_ANCHOR) == 1 and PROJECT_LINE not in project

    for name, text in texts.items():
        write(name, text)
    io.open(os.path.join(GAME, 'SimpleGame.vcxproj'), 'wb').write((chr(0xFEFF) + project.replace(PROJECT_ANCHOR, PROJECT_ANCHOR + PROJECT_LINE)).encode('utf-8'))
    shutil.copyfile(os.path.join(HERE, 'AutoTest.cpp'), os.path.join(GAME, 'AutoTest.cpp'))
    shutil.copyfile(os.path.join(HERE, 'AutoTest.h'), os.path.join(GAME, 'AutoTest.h'))
    print('autotest on')


def hook_off():
    if is_on() == False:
        sys.exit('already off')

    # 붙인 사이에 장치를 고쳤으면 tools 쪽으로 먼저 돌려놓는다
    for name in ('AutoTest.cpp', 'AutoTest.h'):
        shutil.copyfile(os.path.join(GAME, name), os.path.join(HERE, name))

    texts = {}
    for name, before, after in HOOKS:
        text = texts.get(name, read(name))
        assert text.count(after) == 1, (name, after[:60], text.count(after))
        texts[name] = text.replace(after, before)

    scene = read('Scene.cpp')
    assert scene.count(ANVIL_FUNC) == 1
    texts['Scene.cpp'] = scene.replace(ANVIL_FUNC, '')

    project = io.open(os.path.join(GAME, 'SimpleGame.vcxproj'), 'rb').read().decode('utf-8-sig')
    assert project.count(PROJECT_LINE) == 1

    for name, text in texts.items():
        write(name, text)
    io.open(os.path.join(GAME, 'SimpleGame.vcxproj'), 'wb').write((chr(0xFEFF) + project.replace(PROJECT_LINE, '')).encode('utf-8'))
    os.remove(os.path.join(GAME, 'AutoTest.cpp'))
    os.remove(os.path.join(GAME, 'AutoTest.h'))
    for path in glob.glob(os.path.join(GAME, 'Logs', 'cap_*.bmp')):
        os.remove(path)
    print('autotest off')


if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] not in ('on', 'off'):
        sys.exit('python hook.py on | off')
    if sys.argv[1] == 'on':
        hook_on()
    else:
        hook_off()
