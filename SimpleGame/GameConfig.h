#pragma once

constexpr int32 WINDOW_SIZE_X = 1280;
constexpr int32 WINDOW_SIZE_Y = 720;

// 월드를 이 배율만큼 작게 그린 뒤 최근접으로 키운다. 바닥·물체·빛·안개가 같은 격자에 얹힌다.
// 3 이면 사람 키 21 도트가 63 픽셀( 화면 높이 9 퍼센트 ). 2 였을 때 42 픽셀이라 예비 동작이 안 읽혔다
constexpr int32 PIXEL_SCALE = 3;
constexpr int32 RENDER_SIZE_X = (WINDOW_SIZE_X + PIXEL_SCALE - 1) / PIXEL_SCALE;
constexpr int32 RENDER_SIZE_Y = (WINDOW_SIZE_Y + PIXEL_SCALE - 1) / PIXEL_SCALE;

// 글자와 창은 두 배 격자로 백버퍼에 바로 그린다. 세 배면 굴림 12 가 36 픽셀이라 창에 몇 줄 안 들어간다
constexpr int32 UI_SCALE = 2;
constexpr float UI_SPRITE_SCALE = static_cast<float>(UI_SCALE) / static_cast<float>(PIXEL_SCALE);
