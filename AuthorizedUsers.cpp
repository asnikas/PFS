#pragma once
#include <iostream>
#include <vector>
#include "Parser.cpp"

using namespace std;

struct AuthorizedUsers{
    vector <int> AuthorizedUsers;

    int search(int uid){
        for(int i = 0; i < AuthorizedUsers.size(); i++){
            if(AuthorizedUsers[i] == uid) return i;
        }
        return -1;
    }

    void add(int uid){
        if(search(uid) == -1) AuthorizedUsers.push_back(uid);
    }

    void remove(int uid){
        int index = search(uid);
        if(index != -1) AuthorizedUsers.erase(AuthorizedUsers.begin() + index);
        else cout << "Такого пользователя для remove в authorized users нет" << endl;
    }

    void save(Parser& parser, int addr){
        int size = AuthorizedUsers.size();
        parser.write(addr, reinterpret_cast<char*>(&size), sizeof(size));
        parser.write(addr + sizeof(size), reinterpret_cast<char*>(AuthorizedUsers.data()),
        size * sizeof(int));
    }

    void load(Parser& parser, int addr){
        int size;
        parser.read(addr, reinterpret_cast<char*>(&size), sizeof(size));
        AuthorizedUsers.resize(size); //выделяем место для вектора
        parser.read(addr + sizeof(size), reinterpret_cast<char*>(AuthorizedUsers.data()), 
        size * sizeof(int));
    }
};