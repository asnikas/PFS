#pragma once
#include <cstdint>

struct User{
    int id;
    char name[32];
    uint8_t key[32];

    User(int id, char name[32], uint8_t key[32]);

    User();
};
