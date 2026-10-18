#pragma once
#include <vector>
#include "FileAccessEntry.h"
#include "Parser.h"

struct FileACList{
    std::vector <FileAccessEntry> ACList;

    int find_index(int uid);

    bool search(int uid);

    void grant(int uid, bool can_read, bool can_write);

    void remove(int uid);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
