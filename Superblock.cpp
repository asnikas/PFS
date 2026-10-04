#include <iostream>

using namespace std;

#pragma once
#pragma pack(push,1)
struct Superblock{
    int root_dir_addr;
    int alloc_free;
    int users_table;
    int users_table_size;
    int next_id = 0;

    void init(int root_dir_add, int alloc_free, int users_table, int users_table_size){
        this->root_dir_addr = root_dir_add;
        this->alloc_free = alloc_free;
        this->users_table = users_table;
        this->users_table_size = users_table_size;
    }

    void save(Parser& parser, int addr){
        parser.write(addr, reinterpret_cast<char*>(this), sizeof(Superblock));
    }
    void load(Parser& parser, int addr){
        parser.read(addr, reinterpret_cast<char*>(this), sizeof(Superblock));
    }
};
#pragma pack(pop)
static_assert(sizeof(Superblock) == 20, "superblock size changed");