#include "User.h"
#include <cstring> //дает ф-ии для работы с сырой памятью

using namespace std;

User::User(int id, char name[32], uint8_t key[32]){
    this->id = id;
    memcpy(this->name, name, 32);
    memcpy(this->key, key, 32); //копирует байты напрямую
}

User::User(){}
