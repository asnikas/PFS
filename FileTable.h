#pragma once
#include <vector>
#include "TableNode.h"
#include "Parser.h"

struct FileTable{
    std::vector<TableNode> table_node_vec;

    int search(char name[32]);

    void add(char name[32], int addr);

    void remove(char name[32]);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
