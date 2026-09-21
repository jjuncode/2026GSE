#pragma once

#include "WorldPos.h"

// 콜백에서 바로 움직이면 이동 속도가 운영체제 키 반복 설정에 묶인다.
// 눌린 상태만 기록해 두고 실제 이동은 갱신에서 한다
void SetKeyDown(uint8 key, bool down);
void SetSpecialKeyDown(int32 key, bool down);
void ClearAllKeys();

bool IsKeyDown(uint8 key);
bool IsSpecialKeyDown(int32 key);

// glut 창 좌표 ( 좌상단 원점 ) 를 화면 중앙 원점으로 바꿔 둔다
void SetMouseWindowPos(int32 x, int32 y);
ScreenPos GetMouseScreenPos();

// 누른 틱과 뗀 틱이 한 프레임에 같이 오면 눌림 상태만으로는 클릭을 놓친다
void SetAttackHeld(bool held);
void QueueAttack();
bool ConsumeAttackRequest();	// 대기 중이거나 누르고 있으면 true
