#pragma once
#include <iostream>
#include <cstring>
#include <cstdint> //типы целых чисел с точно известным размером (uint8_t - 1 байт)

using namespace std;

#pragma pack(push,1)
struct Descriptor{
    int type;
    uint64_t size;
};
#pragma pack(pop)