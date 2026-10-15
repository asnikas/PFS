#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include "User.cpp"
#include "Parser.cpp"
#include "Superblock.cpp"

using namespace std;

struct UsersTable{
    vector<User> users_vec;
    Superblock& superblock;

    UsersTable(Superblock& superblock) : superblock(superblock) {}

    int search(int id){
        for(int i = 0; i < users_vec.size(); i++){
            if(id == users_vec[i].id) return i;
        }
        return -1;
    }

    int search_uid_by_name(char name[32]){
        for(int i = 0; i < users_vec.size(); i++){
            if(memcmp(users_vec[i].name, name, 32) == 0) return users_vec[i].id;
        }
        return -1;
    }

    void add(char name[32], uint8_t key[32]){
        if(search(superblock.next_id) == -1){
            users_vec.emplace_back(superblock.next_id, name, key);
            superblock.next_id++;
        }
    }

    void remove(int id){
        int index = search(id);
        if(index != -1){
            users_vec.erase(users_vec.begin() + index);
        }
    }

    void save(Parser& parser, int addr){
        int size = users_vec.size();
        superblock.users_table_size = size;
        parser.write(addr, reinterpret_cast<char*>(users_vec.data()), 
        size * sizeof(User));
        superblock.users_table = addr;
    }

    void load(Parser& parser, int addr){
        int size = superblock.users_table_size;
        if(size < 0 || size > 1000){
            cout << "невалидный размер size для UsersTable при чтении из Superblock\n";
            return;
        }
        users_vec.resize(size); //выделяем место для вектора
        parser.read(addr, reinterpret_cast<char*>(users_vec.data()), 
        size * sizeof(User));
    }
};