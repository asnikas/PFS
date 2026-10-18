#include "StreamDescriptor.h"
#include <cstring>

using namespace std;

StreamDescriptor::StreamDescriptor(char name[32], int addr, int size, int capacity){
    memcpy(this->name, name, 32);
    this->addr = addr;
    this->size = size; //сколько байт реально записано
    this->capacity = capacity; //сколько байт зарезервировано под поток
}

StreamDescriptor::StreamDescriptor(){} //конструктор по умолчанию
