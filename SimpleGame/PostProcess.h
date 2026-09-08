#pragma once

bool InitPostProcess(int render_width, int render_height, int window_width, int window_height);
void ShutdownPostProcess();

// 이 뒤에 그린 것은 8비트 백버퍼가 아니라 부동소수 텍스처로 들어간다.
// 그래서 1 을 넘는 색이 안 잘리고 살아남는다
void BeginScenePass();

// 밝은 부분만 뽑아 흐린 뒤 되더하고, 톤매핑과 비네팅을 걸어 저해상도 버퍼에 옮긴다.
// 끝나도 그 버퍼가 묶인 채로 남는다. UI 를 같은 격자에 얹으려면 이 뒤에 그려야 한다
void EndScenePassAndComposite();

// 저해상도 버퍼를 최근접으로 확대해 백버퍼에 올린다. 한 점이 정사각 블록이 된다
void PresentUpscaled();
