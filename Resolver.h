#pragma once
#include <string>
#include <vector>
#include <utility>
#include "Parser.h"

struct Resolver{
    std::vector<std::pair<int, std::string>> resolve(Parser& parser, std::string path, int current_address, char folder_name[32],
        int uid);
};
