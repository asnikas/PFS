#include "TableNode.h"
#include <cstring>

using namespace std;

TableNode::TableNode(char name[32], int addr){
    memcpy(this->name, name, 32);
    this->addr = addr;
}

TableNode::TableNode(){}
