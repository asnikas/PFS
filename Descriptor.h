#pragma once
#include <cstdint> //типы целых чисел с точно известным размером (uint8_t - 1 байт)

#pragma pack(push,1)
struct Descriptor{
    int type;
    uint64_t size;
};
#pragma pack(pop)
