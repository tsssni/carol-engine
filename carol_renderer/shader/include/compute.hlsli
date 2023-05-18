bool GroupBorderTest(uint2 pos, uint radius)
{
    return pos.x >= radius && pos.x <= 31 - radius && pos.y >= radius && pos.y <= 31 - radius;
}

int2 GetUavId(uint2 gid, uint2 gtid, uint radius)
{
    return gid * (32 - 2 * radius) + gtid - radius;
}