#pragma once

#include "WorldPos.h"

// 칸 단위 막힘 표. 청크마다 한 장씩 든다
void ClearWorld();
void SetTileBlocked(const WorldPos& pos, bool blocked);
bool IsTileBlocked(const WorldPos& pos);
bool IsBodyBlocked(const WorldPos& pos, float radius);	// 몸 사각형 모서리 넷 검사

// 한 틱 이동량이 한 칸보다 작다는 전제 ( 끝점만 본다 )
void MoveWithCollision(WorldPos* pos, float radius, float delta_x, float delta_y);
