#include "FileTable.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

int FileTable::search(char name[32]){
    for(int i = 0; i < table_node_vec.size(); i++){
        if(memcmp(name, table_node_vec[i].name, 32) == 0) return i;
    }
    return -1;
}

void FileTable::add(char name[32], int addr){
    if(search(name) == -1){
        for(int pos = 0; pos < table_node_vec.size(); pos++){
            if(strncmp(name, table_node_vec[pos].name, 32) < 0){
                table_node_vec.insert(table_node_vec.begin() + pos, TableNode(name, addr));
                return;
            }
        }
        table_node_vec.emplace_back(name, addr);
        return;
    }
    throw runtime_error(string("name ") + name + " already exists in FileTable"); //string + что-то = string
}

void FileTable::remove(char name[32]){
    int index = search(name);
    if(index != -1){
        table_node_vec.erase(table_node_vec.begin() + index);
        return;
    }
    throw runtime_error("error while removing an element (FileTable, remove)");
}

void FileTable::save(Parser& parser, int addr){
    int size = table_node_vec.size();
    parser.write(addr, reinterpret_cast<char*>(&size), sizeof(size));
    parser.write(addr + sizeof(size), reinterpret_cast<char*>(table_node_vec.data()), 
    size * sizeof(TableNode));
}

void FileTable::load(Parser& parser, int addr){
    int size;
    parser.read(addr, reinterpret_cast<char*>(&size), sizeof(size));
    if(size < 0 || size > 1000){
        cout << "invalid size while loading FileTable from Parser\n";
        return;
    }
    table_node_vec.resize(size); //выделяем место для вектора
    parser.read(addr + sizeof(size), reinterpret_cast<char*>(table_node_vec.data()), 
    size * sizeof(TableNode));
}
