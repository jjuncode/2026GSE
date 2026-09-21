#pragma once

// glDrawArrays 직전에 부른다
void CountDrawCall();

// 프레임 끝에서 한 번. 1 초마다 모아서 콘솔에 낸다
void EndProfileFrame();
