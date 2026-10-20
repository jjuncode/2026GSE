#pragma once

#include <string>

// 임시 시험 장치. 환경 변수 SG_AUTOTEST 가 있을 때만 돈다
void InitAutoTest();
bool IsAutoTestOn();
void UpdateAutoTest(float real_seconds);
bool PopCaptureRequest(std::string* name);
void SaveFrameBmp(const std::string& name, int32 width, int32 height);
