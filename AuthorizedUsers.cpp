#include "AuthorizedUsers.h"
#include <iostream>

using namespace std;

int AuthorizedUsers::search(int uid){
    for(int i = 0; i < AuthorizedUsers.size(); i++){
        if(AuthorizedUsers[i] == uid) return i;
    }
    return -1;
}

void AuthorizedUsers::add(int uid){
    if(search(uid) == -1) AuthorizedUsers.push_back(uid);
}

void AuthorizedUsers::remove(int uid){
    int index = search(uid);
    if(index != -1) AuthorizedUsers.erase(AuthorizedUsers.begin() + index);
    else cout << "No such user to remove from authorized users" << endl;
}

void AuthorizedUsers::save(Parser& parser, int addr){
    int size = AuthorizedUsers.size();
    parser.write(addr, reinterpret_cast<char*>(&size), sizeof(size));
    parser.write(addr + sizeof(size), reinterpret_cast<char*>(AuthorizedUsers.data()),
    size * sizeof(int));
}

void AuthorizedUsers::load(Parser& parser, int addr){
    int size;
    parser.read(addr, reinterpret_cast<char*>(&size), sizeof(size));
    AuthorizedUsers.resize(size); //выделяем место для вектора
    parser.read(addr + sizeof(size), reinterpret_cast<char*>(AuthorizedUsers.data()), 
    size * sizeof(int));
}
