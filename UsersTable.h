#pragma once
#include <vector>
#include "User.h"
#include "Parser.h"
#include "Superblock.h"

struct UsersTable{
    std::vector<User> users_vec;
    Superblock& superblock;

    UsersTable(Superblock& superblock);

    int search(int id);

    int search_uid_by_name(char name[32]);

    void add(char name[32], uint8_t key[32]);

    void remove(int id);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
