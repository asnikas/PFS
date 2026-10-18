#pragma once
#include <vector>
#include "Parser.h"

struct AuthorizedUsers{
    std::vector <int> AuthorizedUsers;

    int search(int uid);

    void add(int uid);

    void remove(int uid);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
