#pragma once

bool InitFog(int32 screen_width, int32 screen_height, float pixel_scale);
void ShutdownFog();

// 화면 덮는 사각형 한 장. 값 노이즈라 사각형 안개와 달리 경계선이 안 생긴다
void DrawFogLayer(float scroll);
