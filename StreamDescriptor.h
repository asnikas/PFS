#pragma once

struct StreamDescriptor{
    char name[32];
    int addr, size, capacity;

    StreamDescriptor(char name[32], int addr, int size, int capacity);

    StreamDescriptor();
};
