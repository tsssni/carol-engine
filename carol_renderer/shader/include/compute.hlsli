#ifndef BORDER_RADIUS
#define BORDER_RADIUS 0
#endif

bool UavBorderTest(int2 pos, uint2 size)
{
    return pos.x >= 0 && pos.x < size.x && pos.y >= 0 && pos.y < size.y;
}

bool GroupBorderTest(uint2 pos)
{
    return pos.x >= BORDER_RADIUS && pos.x <= 31 - BORDER_RADIUS && pos.y >= BORDER_RADIUS && pos.y <= 31 - BORDER_RADIUS;
}

int2 GetUavId(uint2 gid, uint2 gtid)
{
    return gid * (32 - 2 * BORDER_RADIUS) + gtid - BORDER_RADIUS;
}