#pragma once

// 고정 간격 하나로 갱신해야 빠른 기계와 느린 기계에서 결과가 같다
constexpr float FIXED_STEP_SECONDS = 1.f / 60.f;

// 이 한도를 넘긴 시간은 버린다. 안 버리면 밀린 만큼 갱신이 늘고
// 갱신이 느려져 더 밀리는 악순환에 빠진다
constexpr float MAX_FRAME_SECONDS = 0.25f;

void ResetFrameClock();

// 직전 호출로부터 흐른 시간(초). 한도가 이미 적용된 값이다
float PopElapsedSeconds();
