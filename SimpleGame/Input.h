#pragma once

// 콜백에서 바로 움직이면 이동 속도가 운영체제 키 반복 설정에 묶인다.
// 눌린 상태만 기록해 두고 실제 이동은 갱신에서 한다
void SetKeyDown(unsigned char key, bool down);
void SetSpecialKeyDown(int key, bool down);
void ClearAllKeys();

bool IsKeyDown(unsigned char key);
bool IsSpecialKeyDown(int key);
