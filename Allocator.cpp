#include <iostream>
#include "Superblock.cpp"

using namespace std;

struct Allocator{
    int next_free;
    int image_size; //размер образа

    Allocator(int next_free, int image_size){
        this->next_free = next_free;
        this->image_size = image_size;
    }

    int alloc(int size){
        if(next_free + size > image_size) {
            cout << "образ закончился\n";
            return -1;
        }
        int addr = next_free;
        next_free += size;
        return addr;
    }

    void save(Superblock& superblock){
        superblock.alloc_free = next_free;
    }
};