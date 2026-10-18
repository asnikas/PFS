#include "Allocator.h"
#include <iostream>
#include <stdexcept>

using namespace std;

Allocator::Allocator(int next_free, int image_size){
    this->next_free = next_free;
    this->image_size = image_size;
}

int Allocator::alloc(int size){
    if(next_free + size > image_size) {
        throw runtime_error("образ закончился, текущий размер - " + to_string(image_size));
    }
    int addr = next_free;
    next_free += size;
    return addr;
}

void Allocator::save(Superblock& superblock){
    superblock.alloc_free = next_free;
}
