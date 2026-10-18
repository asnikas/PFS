#pragma once //сообщаем компилятору, что если файл подключали в другом .cpp, то больше его не читать
//(избежать дублирования)
#include "Superblock.h"

struct Allocator{
    int next_free;
    int image_size; //размер образа

    Allocator(int next_free, int image_size);

    int alloc(int size);

    void save(Superblock& superblock);
};
