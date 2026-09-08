#pragma once

bool InitPostProcess(int screen_width, int screen_height);
void ShutdownPostProcess();

// 이 뒤에 그린 것은 8비트 백버퍼가 아니라 부동소수 텍스처로 들어간다.
// 그래서 1 을 넘는 색이 안 잘리고 살아남는다
void BeginScenePass();

// 밝은 부분만 뽑아 흐린 뒤 되더하고, 톤매핑과 비네팅을 걸어 백버퍼에 옮긴다
void EndScenePassAndComposite();
