#pragma once
#include <string>
#include <vector>
#include "Parser.h"
#include "Allocator.h"
#include "Superblock.h"
#include "Encryption.h"
#include "Resolver.h"

struct FileSystem{
    int curr_uid = -1, curr_folder = -1;
    char curr_folder_name[32] = {};
    Parser& parser;
    Allocator& allocator;
    Superblock& superblock;
    Encryption encryption;
    Resolver resolver;
    bool mounted = false; //подключен ли образ?

    FileSystem(Parser& parser, Allocator& allocator, Superblock& superblock);

    void mkfs(int image_size, char admin_name[32]);

    void sync();

    bool mount();

    void login(char name[32]);

    void register_user(char name[32]);

    void cd(std::string path, bool to_root = false);

    void ls();

    void create(char file_name[32], bool can_read, bool can_write);

    void create_encrypted(char file_name[32], bool can_read, bool can_write);

    void mkdir(char folder_name[32], bool can_read, bool can_write, bool can_traverse);

    std::vector <char> read(char name[32]);

    void write(char name[32], std::vector <char> buffer);

    void get_info(char name[32]);

    void remove(char name[32]);

    void rename(char name[32], char new_name[32]);

    void move(char name[32], int dest_addr);

    void copy(char name[32], int dest_addr);

    void set_acl(int uid, char name[32], bool can_read, bool can_write, bool can_traverse = false);

    void check_name(const std::string& name);
};
