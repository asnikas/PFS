#include <iostream>
#include <string>
#include "FileSystem.cpp"

using namespace std;

struct Menu{

    FileSystem& fs;
    bool running = true;

    Menu(FileSystem& fs) : fs(fs) {}

    void run(){
        string input;
        while(running){
            cout << "> ";
            getline(cin, input); //читаем всю строку целиком

            istringstream iss(input); //оборачиваем строку так, чтобы из нее можно было читать, как из cin
            string command;
            iss >> command; //читает первое слово до пробела в command, остаток строки остается внутри iss

            if(command != "") dispatch(command, iss);
        }
    }

    void dispatch(string command, istringstream& args){
        if(fs.curr_uid == -1){
            if(command != "login" && command != "register" && command != "mkfs" && command != "exit"){
                cout << "сначала выполните login" << endl;
                return;
            }
        }
        if(command == "mkfs") handle_mkfs(args);
        else if(command == "login") handle_login(args);
        else if(command == "register") handle_register(args);
        else if(command == "cd") handle_cd(args);
        else if(command == "ls") handle_ls(args);
        else if(command == "create") handle_create(args);
        else if(command == "create_encrypted") handle_create_encrypted(args);
        else if(command == "mkdir") handle_mkdir(args);
        else if(command == "read") handle_read(args);
        else if(command == "write") handle_write(args);
        else if(command == "get_info") handle_get_info(args);
        else if(command == "remove") handle_remove(args);
        else if(command == "rename") handle_rename(args);
        else if(command == "move") handle_move(args);
        else if(command == "copy") handle_copy(args);
        else if(command == "set_acl") handle_set_acl(args);
        else if(command == "exit") running = false;
        else cout << "неизвестная команда: " << command << endl;
    }

    void handle_mkfs(istringstream& args){
        int image_size;
        string admin_name;

        if(!(args >> image_size >> admin_name)){ //пытаемся вычитать из остатка строки сначала число, потом слово
            cout << "неправильные входные данные для mkfs" << endl;
            return;
        }

        char name[32] = {};
        strncpy(name, admin_name.c_str(), 31);

        fs.mkfs(image_size, name);
    }

    void handle_login(istringstream& args){
        string name;
        if(!(args >> name)){ 
            cout << "неправильные входные данные для login" << endl;
            return;
        }

        char buffer[32] = {}; //?
        strncpy(buffer, name.c_str(), 31);

        fs.login(buffer);

    }

    void handle_register(istringstream& args){
        string name;
        if(!(args >> name)){ 
            cout << "неправильные входные данные для register_user" << endl;
            return;
        }
        char buffer[32] = {}; //?
        strncpy(buffer, name.c_str(), 31);

        fs.register_user(buffer);
    }

    void handle_cd(istringstream& args){
        string path;
        if(!(args >> path)){ 
            cout << "неправильные входные данные для cd" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, path.c_str(), 31);

        if(path == ".." || path == "/"){
            fs.cd(buffer, true);
        }

        else fs.cd(buffer);
    }

    void handle_ls(istringstream& args){
        fs.ls();
    }

    void handle_create(istringstream& args){
        string path;
        bool can_read, can_write;
        if(!(args >> path >> can_read >> can_write)){ 
            cout << "неправильные входные данные для create" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, path.c_str(), 31);

        fs.create(buffer, can_read, can_write);
    }

    void handle_create_encrypted(istringstream& args){
        string path;
        bool can_read, can_write;
        if(!(args >> path >> can_read >> can_write)){ 
            cout << "неправильные входные данные для create_encrypted" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, path.c_str(), 31);

        fs.create_encrypted(buffer, can_read, can_write);
    }

    void handle_mkdir(istringstream& args){
        string path;
        bool can_read, can_write, can_traverse;
        if(!(args >> path >> can_read >> can_write >> can_traverse)){ 
            cout << "неправильные входные данные для mkdir" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, path.c_str(), 31);

        fs.mkdir(buffer, can_read, can_write, can_traverse);
    }
};