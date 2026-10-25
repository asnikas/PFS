#pragma once
#include <sstream>
#include "FileSystem.h"

//istringstream - берем обычную string и оборачиваем ее так, чтобы с ней можно было работать, как и с cin

struct Menu{
    FileSystem& fs;
    bool running = true;
    bool is_interactive;

    Menu(FileSystem& fs, bool is_interactive);

    void run();

    void dispatch(std::string command, std::istringstream& args);

    void handle_mkfs(std::istringstream& args);

    void handle_login(std::istringstream& args);

    void handle_register(std::istringstream& args);

    void handle_cd(std::istringstream& args);

    void handle_ls(std::istringstream& args);

    void handle_create(std::istringstream& args);

    void handle_create_encrypted(std::istringstream& args);

    void handle_mkdir(std::istringstream& args);

    void handle_read(std::istringstream& args);

    void handle_write(std::istringstream& args);

    void handle_get_info(std::istringstream& args);

    void handle_remove(std::istringstream& args);

    void handle_rename(std::istringstream& args);

    void handle_move(std::istringstream& args);

    void handle_copy(std::istringstream& args);

    void handle_set_acl(std::istringstream& args);

    void handle_stats(std::istringstream& args);

    void handle_pwd(std::istringstream& args);
};