#include "FileACList.h"
#include <iostream>

using namespace std;

int FileACList::find_index(int uid){
    for(int i = 0; i < ACList.size(); i++){
        if(uid == ACList[i].uid) return i;
    }
    return -1;
}

bool FileACList::search(int uid){
    return (find_index(uid) != -1);
}

void FileACList::grant(int uid, bool can_read, bool can_write){
    int index = find_index(uid);
    if(index == -1){
        ACList.emplace_back(uid, can_read, can_write);
    }
    else{
        ACList[index].can_read = can_read;
        ACList[index].can_write = can_write;
    }
}

void FileACList::remove(int uid){
    int index = find_index(uid);
    if(index != -1){
        ACList.erase(ACList.begin() + index);
    }
}

void FileACList::save(Parser& parser, int addr){
    int size = ACList.size();
    parser.write(addr, reinterpret_cast<char*>(&size), sizeof(size));
    parser.write(addr + sizeof(size), reinterpret_cast<char*>(ACList.data()), 
    size * sizeof(FileAccessEntry));
}

void FileACList::load(Parser& parser, int addr){
    int size;
    parser.read(addr, reinterpret_cast<char*>(&size), sizeof(size));
    if(size < 0 || size > 1000){
        cout << "невалидный размер size для ACList при чтении из FileACList\n";
        return;
    }
    ACList.resize(size); //выделяем место для вектора
    parser.read(addr + sizeof(size), reinterpret_cast<char*>(ACList.data()), 
    size * sizeof(FileAccessEntry));
}
