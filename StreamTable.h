#pragma once
#include <vector>
#include "StreamDescriptor.h"
#include "Parser.h"
#include "Allocator.h"

struct StreamTable{
    std::vector <StreamDescriptor> stream_desc_vec;

    int search(char name[32]);

    void add(char name[32], int addr, int size, int capacity);

    void remove(char name[32]);

    bool grow(Parser& parser, Allocator& allocator, int index, int need);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
