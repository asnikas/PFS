#include "Resolver.h"
#include "FileTable.h"
#include "FolderDescriptor.h"
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace std;

std::vector<std::pair<int, std::string>> Resolver::resolve(Parser& parser, string path, int current_address, char folder_name[32], int uid){
    vector <string> addr_parts; //в векторе нельзя <char[32]>, т.к это не присваиваемый тип
    std::vector<std::pair<int, std::string>> steps;
    string word;
    stringstream ss(path);
    while(getline(ss, word, '/')){
        if(!word.empty()) addr_parts.push_back(word);
    }

    FileTable table;
    FolderDescriptor folder;
    int type;
    for(int i = 0; i < addr_parts.size(); i++){
        parser.read(current_address, reinterpret_cast<char*>(&type), sizeof(int));
        if(type != 3){
            throw runtime_error("trying to pass through a file in resolver");
        }
        folder.load(parser, current_address);

        if(!folder.can_traverse(parser, uid)){
            throw runtime_error("user with uid " + to_string(uid) + " cannot traverse directory " + string(folder_name));
        }
        table.load(parser, folder.file_table_addr);

        char buffer[32] = {};
        strncpy(buffer, addr_parts[i].c_str(), sizeof(buffer) - 1);
        int index = table.search(buffer);

        if(index == -1){
            throw runtime_error("no item " + addr_parts[i] + " in directory " + string(folder_name));
        }
        current_address = table.table_node_vec[index].addr;
        memcpy(folder_name, table.table_node_vec[index].name, 32);
        steps.push_back({current_address, folder_name});
    }
    parser.read(current_address, reinterpret_cast<char*>(&type), sizeof(int));
    if(type == 3){
        folder.load(parser, current_address);
        if(!folder.can_traverse(parser, uid)){
            throw runtime_error("the target of the path is a directory the user cannot enter");
        }
    }

    return steps;
}
