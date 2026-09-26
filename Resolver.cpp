#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>
#include "Parser.cpp"
#include "Superblock.cpp"
#include "FileTable.cpp"
#include "FolderDescriptor.cpp"
#include "TableNode.cpp"

using namespace std;

struct Resolver{
    int resolve(Parser& parser, string path, int current_address, char folder_name[32], int uid){
        vector <string> addr_parts;
        string word;
        stringstream ss(path);
        while(getline(ss, word, '/')){
            if(!word.empty()) addr_parts.push_back(word);
        }

        FileTable table;
        FolderDescriptor folder;
        for(int i = 0; i < addr_parts.size(); i++){
            int type;
            parser.read(current_address, reinterpret_cast<char*>(&type), sizeof(int));
            if(type != 3){
                cout << "пытаемся пройти сквозь файл в resolver" << endl;
                return -1;
            }
            folder.load(parser, current_address);

            if(!folder.can_traverse(parser, uid)){
                cout << "пользователь с uid " << uid << " не может проходить через каталог " << string(folder_name) << endl;
                return -1;
            }
            table.load(parser, folder.file_table_addr);

            char buffer[32] = {};
            strncpy(buffer, addr_parts[i].c_str(), sizeof(buffer) - 1);
            int index = table.search(buffer);

            if(index == -1){
                cout << "нет элемента " << addr_parts[i] << " в каталоге " << string(folder_name) << endl;
                return -1;
            }
            current_address = table.table_node_vec[index].addr;
            memcpy(folder_name, table.table_node_vec[index].name, 32);
        }
        return current_address;
    }   
};