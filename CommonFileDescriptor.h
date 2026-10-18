#pragma once
#include <ctime>
#include <vector>
#include "Descriptor.h"
#include "Parser.h"
#include "Allocator.h"

#pragma pack(push,1)
struct CommonFileDescriptor : Descriptor{
    time_t created_d, modified_d;
    int owner_id, stream_table_addr, stream_table_size, acl_addr, acl_size;

    void init(int owner_id, int stream_table_addr, int stream_table_size, int acl_addr,
        int acl_size);

    bool can_read(Parser& parser, int uid);

    bool can_write(Parser& parser, int uid);

    void get_info(Parser& parser);

    std::vector <char> read(Parser& parser, int uid);

    void write(Parser& parser, int uid, std::vector<char> buffer, Allocator& allocator);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
#pragma pack(pop)
