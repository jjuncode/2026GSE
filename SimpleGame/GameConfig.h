#pragma once

constexpr int32 WINDOW_SIZE_X = 1280;
constexpr int32 WINDOW_SIZE_Y = 720;

// 씬을 이 배율만큼 작게 그린 뒤 최근접으로 키운다.
// 바닥·물체·빛·안개·UI 가 전부 같은 격자에 얹혀야 도트로 읽힌다
constexpr int32 PIXEL_SCALE = 2;
constexpr int32 RENDER_SIZE_X = WINDOW_SIZE_X / PIXEL_SCALE;
constexpr int32 RENDER_SIZE_Y = WINDOW_SIZE_Y / PIXEL_SCALE;
