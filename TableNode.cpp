#include <iostream>
#include <cstring>

using namespace std;

struct TableNode{
    char name[32];
    int addr;

    TableNode(char name[32], int addr){
        memcpy(this->name, name, 32);
        this->addr = addr;
    }

    TableNode(){}
};