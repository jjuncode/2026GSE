#pragma once

// 프레임을 갈라 재는 구간. 드로우콜과 CPU 시간이 지금 열린 구간 몫으로 적힌다.
// 겹치지 않게 짠다 - 바닥과 안개는 씬 구간 밖에서 자기 구간을 연다
constexpr int32 PROFILE_STAGE_UPDATE = 0;
constexpr int32 PROFILE_STAGE_GROUND = 1;
constexpr int32 PROFILE_STAGE_SCENE = 2;
constexpr int32 PROFILE_STAGE_FOG = 3;
constexpr int32 PROFILE_STAGE_POST = 4;
constexpr int32 PROFILE_STAGE_OVERLAY = 5;
constexpr int32 PROFILE_STAGE_HUD = 6;
constexpr int32 PROFILE_STAGE_COUNT = 7;

// 배치가 끊긴 이유. 어느 쪽이 큰지 봐야 다음에 뭘 손볼지 정해진다
constexpr int32 FLUSH_REASON_STATE = 0;		// 셰이더나 텍스처가 바뀜
constexpr int32 FLUSH_REASON_FULL = 1;		// 인스턴스 칸이 참
constexpr int32 FLUSH_REASON_LAYER = 2;		// 다른 층이 직접 그리려고 비움
constexpr int32 FLUSH_REASON_COUNT = 3;

// Logs/ 밑에 이번 실행분 csv 를 연다. 실패해도 콘솔 보고는 그대로 돈다
bool InitProfiler();
void ShutdownProfiler();

// 그래픽 카드와 드라이버를 csv 머리글과 콘솔에 한 번 남긴다.
// 이게 없으면 나중에 남은 수치가 어느 기계 것인지 알 수 없다
void LogGraphicsCaps();

void BeginProfileStage(int32 stage);
void EndProfileStage();

// glDraw* 직전에 부른다
void CountDrawCall();

// 모으기 전 개수. 이게 드로우콜보다 훨씬 크면 배치가 먹고 있다는 뜻
void CountQuadSubmit();
void CountBatchFlush(int32 reason, int32 instance_count);

// 깊이 정렬에 들어간 액터 수. 드로우콜과 따로 움직여서 갈라 잰다
void CountSceneActor(int32 count);

// GDI 로 새로 구운 글자. 게임 중 0 이 아니면 그 프레임이 튄다
void CountGlyphRaster();

// 자산을 캐시에서 읽었는지 새로 구웠는지. 시작 로그에만 쓴다
void LogAssetLoad(const char* name, bool from_cache, size_t byte_count, float milliseconds);

// 프레임 끝에서 한 번. 1 초마다 모아서 콘솔과 csv 에 낸다
void EndProfileFrame();

// P 키. 구간별 상세 블록을 콘솔에 같이 낼지
void ToggleProfileDetail();
