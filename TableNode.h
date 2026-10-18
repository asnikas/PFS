#pragma once

struct TableNode{
    char name[32];
    int addr;

    TableNode(char name[32], int addr);

    TableNode();
};
