#include "Superblock.h"

using namespace std;

void Superblock::init(int root_dir_add, int alloc_free, int users_table, int users_table_size){
    this->root_dir_addr = root_dir_add;
    this->alloc_free = alloc_free;
    this->users_table = users_table;
    this->users_table_size = users_table_size;
}

void Superblock::save(Parser& parser, int addr){
    parser.write(addr, reinterpret_cast<char*>(this), sizeof(Superblock));
}

void Superblock::load(Parser& parser, int addr){
    parser.read(addr, reinterpret_cast<char*>(this), sizeof(Superblock));
}
