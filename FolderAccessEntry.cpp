#include "FolderAccessEntry.h"

using namespace std;

FolderAccessEntry::FolderAccessEntry(){}

FolderAccessEntry::FolderAccessEntry(int uid, bool can_read, bool can_write, bool can_traverse){
    this->uid = uid;
    this->can_read = can_read;
    this->can_write = can_write;
    this->can_traverse = can_traverse;
}
