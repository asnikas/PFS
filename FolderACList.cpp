#include <iostream>
#include <vector>
#include "FolderAccessEntry.cpp"
#include "Parser.cpp"

using namespace std;

struct FolderACList{
    vector <FolderAccessEntry> ACList;

    int find_index(int uid){
        for(int i = 0; i < ACList.size(); i++){
            if(uid == ACList[i].uid) return i;
        }
        return -1;
    }

    bool search(int uid){
        return (find_index(uid) != -1);
    }

    void grant(int uid, bool can_read, bool can_write, bool can_traverse){
        int index = find_index(uid);
        if(index == -1){
            ACList.emplace_back(uid, can_read, can_write, can_traverse);
        }
        else{
            ACList[index].can_read = can_read;
            ACList[index].can_write = can_write;
            ACList[index].can_traverse = can_traverse;
        }
    }

    void remove(int uid){
        int index = find_index(uid);
        if(index != -1){
            ACList.erase(ACList.begin() + index);
        }
    }

    void save(Parser& parser, int addr){
        int size = ACList.size();
        parser.write(addr, reinterpret_cast<char*>(&size), sizeof(size));
        parser.write(addr + sizeof(size), reinterpret_cast<char*>(ACList.data()), 
        size * sizeof(FolderAccessEntry));
    }

    void load(Parser& parser, int addr){
        int size;
        parser.read(addr, reinterpret_cast<char*>(&size), sizeof(size));
        if(size < 0 || size > 1000){
            cout << "невалидный размер size для ACList при чтении из FolderACList\n";
            return;
        }
        ACList.resize(size); //выделяем место для вектора
        parser.read(addr + sizeof(size), reinterpret_cast<char*>(ACList.data()), 
        size * sizeof(FolderAccessEntry));
    }
};