#include <iostream>
#include <cstring>

using namespace std;

#pragma pack(push,1)
struct Descriptor{
    int type;
    uint64_t size;
};
#pragma pack(pop)