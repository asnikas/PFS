#include "CommonFileDescriptor.h"
#include "StreamTable.h"
#include "FileACList.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace std;

void CommonFileDescriptor::init(int owner_id, int stream_table_addr, 
    int stream_table_size, int acl_addr, int acl_size){
    created_d = time(nullptr);
    modified_d = created_d;
    this->owner_id = owner_id;
    this->stream_table_addr = stream_table_addr;
    this->stream_table_size = stream_table_size;
    this->acl_addr = acl_addr;
    this->acl_size = acl_size;
    this->size = 0;
    this->type = 1;
}

bool CommonFileDescriptor::can_read(Parser& parser, int uid){
    FileACList acl;
    acl.load(parser, acl_addr);

    int index = acl.find_index(uid);
    if (index != -1){
        return acl.ACList[index].can_read;
    }
    return false;
}

bool CommonFileDescriptor::can_write(Parser& parser, int uid){
    FileACList acl;
    acl.load(parser, acl_addr);

    int index = acl.find_index(uid);
    if (index != -1){
        return acl.ACList[index].can_write;
    }
    return false;
}

void CommonFileDescriptor::get_info(Parser& parser){
    cout << "File size: " << size <<endl;
    cout << "ACL size: " << acl_size <<endl;
    cout << "Stream table size: " << stream_table_size <<endl;
    cout << "ACL: \n";
    FileACList acl;
    acl.load(parser, acl_addr);
    for(auto el:acl.ACList){
        cout << "User id: " << el.uid << 
        " Can read: " << el.can_read << 
        " Can write: " << el.can_write << endl;
    }
    cout << "Created date: " << created_d << endl;
    cout << "Last time modified: " << modified_d << endl;
}

vector <char> CommonFileDescriptor::read(Parser& parser, int uid){
    if(can_read(parser, uid)){
        StreamTable table;
        table.load(parser, stream_table_addr);
        char name[32] = {};
        strcpy(name, "MAIN");
        int index = table.search(name);
        if(index != -1){
            int addr = table.stream_desc_vec[index].addr;
            int size = table.stream_desc_vec[index].size;
            vector <char> buffer(size);
            parser.read(addr, buffer.data(), size);
            return buffer;
        }
        throw runtime_error("поток MAIN не найден в файле");
    }
    throw runtime_error("текущий пользователь не может прочитать файл");
}

void CommonFileDescriptor::write(Parser& parser, int uid, vector<char> buffer, Allocator& allocator){
    if(can_write(parser, uid)){
        modified_d = time(nullptr);
        StreamTable table;
        table.load(parser, stream_table_addr);
        char name[32] = {};
        strcpy(name, "MAIN");
        int index = table.search(name);
        if(index != -1){
            int size = buffer.size();
            int old_addr = -1; //старый блок потока, если поток пришлось перевыделить
            if(size > table.stream_desc_vec[index].capacity){
                int new_addr = allocator.alloc(size);
                old_addr = table.stream_desc_vec[index].addr;
                table.stream_desc_vec[index].addr = new_addr;
                table.stream_desc_vec[index].capacity = size;
            }
            table.stream_desc_vec[index].size = size;
            this->size = size;
            table.save(parser, stream_table_addr);
            parser.write(table.stream_desc_vec[index].addr, buffer.data(), size);
            if(old_addr != -1) allocator.free(old_addr); //старые данные не нужны, их заменил buffer
            return;
        }
        throw runtime_error("поток MAIN не найден в файле");
    }
    throw runtime_error("текущий пользователь не может писать в этот файл");
}

void CommonFileDescriptor::save(Parser& parser, int addr){
    parser.write(addr, reinterpret_cast<char*>(this), 
    sizeof(CommonFileDescriptor)); //this - указатель на сам объект
}

void CommonFileDescriptor::load(Parser& parser, int addr){
    parser.read(addr, reinterpret_cast<char*>(this), 
    sizeof(CommonFileDescriptor)); //this - указатель на сам объект
}
