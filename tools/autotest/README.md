# 시험 장치

게임을 사람 손 없이 돌려 보고 화면을 찍는 개발용 장치다. **평소 빌드에는 안 들어간다** — 붙였다가 확인이 끝나면 뗀다.
환경 변수 `SG_AUTOTEST` 가 있을 때만 돌고, 정해진 시각에 조작을 넣고 프레임 전체를 `SimpleGame/Logs/cap_*.bmp` 로 떨군다.
창 캡처는 화면 배율과 권한 때문에 믿을 수 없어서 프레임버퍼를 직접 읽는다.

## 순서

```
python tools/autotest/hook.py on          # 붙이기 ( SimpleGame.cpp · Scene.h/.cpp · vcxproj 에 걸고 소스를 복사 )
MSBuild SimpleGame.sln -p:Configuration=Release -p:Platform=x64
powershell -File tools/autotest/run.ps1 -config Release -seconds 8 -mode showcase
python tools/autotest/contact.py show_ 결과.png 5      # 캡처를 한 장에 모아 보기
python tools/autotest/hook.py off         # 떼기 ( 붙인 동안 장치를 고쳤으면 tools 쪽으로 먼저 돌려놓는다 )
# Debug · Release 를 다시 빌드
```

## 모드

| 모드 | 하는 일 | 캡처 |
|---|---|---|
| `combat` | 타락자 · 소거식귀를 앞에 세우고 공격 창 0.1 초 전에 패링, 일부러 맞기, 가방 · 스탯 · 강화 창 | 상태가 바뀌는 순간 |
| `skills` | 레벨업 · 스킬 해금 · 회전베기 · 돌진 · 도약 · 대쉬 · 가드 · 물약 | 스킬마다 한 장 |
| `gallery` | 몬스터 넷을 옆에 세우고 가드를 든다 | 1.1 초마다 |
| `tour` | 거점마다 옮겨 다닌다 ( 성 밖 소품 확인 ) | 거점마다 한 장 |
| `showcase` | 초소로 옮겨 몬스터 없이 평타 셋 -> 대쉬 -> 회전베기 | 0.05 초 연사 |

동작 · 효과는 `showcase` 로 본다. 마을에선 NPC 에 가리고, 몬스터를 세우면 맞아서 동작이 끊긴다.

## 주의

- 스케줄은 프레임 수가 아니라 실제 시간이다. 창이 비활성이면 수직동기가 풀려 수백 fps 로 돈다
- 1 초마다 체력 · 상태 · 쿨을 콘솔에 찍는다 ( `Logs/autotest_구성.txt` ). 로그만으로 흐름을 판정할 수 있다
- 창을 여닫는 틱과 캡처를 같은 틱에 넣으면 다음 상태가 찍힌다 — 열고, 몇 틱 뒤 찍고, 그다음에 닫는다
