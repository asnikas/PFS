#pragma once
#include <vector>
#include "FolderAccessEntry.h"
#include "Parser.h"

struct FolderACList{
    std::vector <FolderAccessEntry> ACList;

    int find_index(int uid);

    bool search(int uid);

    void grant(int uid, bool can_read, bool can_write, bool can_traverse);

    void remove(int uid);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
