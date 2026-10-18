#pragma once
#include <string>
#include "Parser.h"

struct Resolver{
    int resolve(Parser& parser, std::string path, int current_address, char folder_name[32],
        int uid);
};
