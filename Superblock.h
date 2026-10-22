#pragma once
#include "Parser.h"

constexpr int MAGIC_NUM = 0x50465332; //признак самой ФС ("PFS2": формат с header/footer у блоков,
//образы старого формата "PFS1" не монтируются)

#pragma pack(push,1)
struct Superblock{
    int root_dir_addr;
    int alloc_free;
    int users_table;
    int users_table_size;
    int next_id = 0;
    int magic = 0;
    int image_size = 0;

    void init(int root_dir_add, int alloc_free, int users_table, int users_table_size);

    void save(Parser& parser, int addr);
    void load(Parser& parser, int addr);
};
#pragma pack(pop)
static_assert(sizeof(Superblock) == 28, "superblock size changed");
