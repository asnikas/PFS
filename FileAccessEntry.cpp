#include <iostream>

using namespace std;

struct FileAccessEntry{
    int uid;
    bool can_read, can_write;

    FileAccessEntry(int uid, bool can_read, bool can_write){
        this->uid = uid;
        this->can_read = can_read;
        this->can_write = can_write;
    }
};