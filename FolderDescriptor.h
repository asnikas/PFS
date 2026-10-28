#pragma once
#include "Descriptor.h"
#include "Parser.h"
#include "Allocator.h"

#pragma pack(push, 1)
struct FolderDescriptor : Descriptor{
    int acl_addr, acl_size, file_table_addr, file_table_size;

    void init(int acl_addr, int acl_size, int file_table_addr, int file_table_size);

    bool can_read(Parser& parser, int uid);

    bool can_write(Parser& parser, int uid);

    bool can_traverse(Parser& parser, int uid);

    void show(Parser& parser);

    void copy(Parser& parser, Allocator& allocator, char name[32], int dest_addr);

    void move(Allocator& allocator, Parser& parser, char name[32], int dest_addr);

    void rename(Parser& parser, char name[32], char new_name[32]);

    void add(Parser& parser, char name[32], int addr);

    void remove(Parser& parser, Allocator& allocator, char name[32]);

    //освобождает все блоки объекта по адресу addr (для каталога - рекурсивно вместе с содержимым)
    void free_object(Parser& parser, Allocator& allocator, int addr);

    bool is_inside(Parser& parser, int root_addr, int folder_addr);

    void get_info(Parser& parser);

    void save(Parser& parser, int addr);
    void load(Parser& parser, int addr);
};
#pragma pack(pop)
