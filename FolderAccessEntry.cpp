#pragma once
#include <iostream>

using namespace std;

struct FolderAccessEntry{
    int uid;
    bool can_read, can_write, can_traverse;

    FolderAccessEntry() {}

    FolderAccessEntry(int uid, bool can_read, bool can_write, bool can_traverse){
        this->uid = uid;
        this->can_read = can_read;
        this->can_write = can_write;
        this->can_traverse = can_traverse;
    }
};