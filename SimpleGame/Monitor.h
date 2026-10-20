#pragma once

struct ZoneLayout;

// 브라우저 모니터. http://127.0.0.1:포트/ 가 Monitor/index.html 을 주고, /events 로 JSON 메시지를 흘린다
// 소켓은 모니터 스레드만 만진다. 게임 스레드는 상태를 문자열로 구워 넘기기만 한다
bool InitMonitor();
void ShutdownMonitor();

void BuildMonitorZone(const ZoneLayout& layout);	// 구역을 새로 만들 때마다. 지도와 거점 이름
void UpdateMonitor(float real_seconds);

bool OpenMonitorPage();	// 기본 브라우저로. 서버가 꺼져 있으면 false
