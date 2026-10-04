#include <iostream>
#include <string>
#include <sstream>
#include "FileSystem.cpp"

//istringstream - берем обычную string и оборачиваем ее так, чтобы с ней можно было работать, как и с cin, 

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

            istringstream iss(input); //оборачиваем строку так, чтобы из нее можно было читать, как из cin, поток, который читает из готовой строки
            string command;
            iss >> command; //читает первое слово до пробела в command, остаток строки остается внутри iss

            if(command != "") dispatch(command, iss);
        }
    }

    void dispatch(string command, istringstream& args){ //istringstream нельзя передавать по значению, только по ссылке или указателю
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

    void handle_read(istringstream& args){
        string name;
        if(!(args >> name)){ 
            cout << "неправильные входные данные для read" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);

        optional<vector <char>> output_vec = fs.read(buffer);
        if(!output_vec.has_value()){
            cout << "не удалось прочитать файл" << endl;
            return;
        }
        vector<char> result = move(output_vec.value()); //достает сам vector<char> из optional 
        //move пермещает байты вектора, избавляя нас от копии

        cout.write(result.data(), result.size());
        cout << endl;
    }

    void handle_write(istringstream& args){
        string name;
        if(!(args >> name)){ 
            cout << "неправильные входные данные для write" << endl;
            return;
        }

        string data;
        getline(args, data); //забираем все, что осталось после имени

        vector <char> data_buffer(data.begin(), data.end());

        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);
        fs.write(buffer, data_buffer);
    }

    void handle_get_info(istringstream& args){
        string name;
        if(!(args >> name)){ 
            cout << "неправильные входные данные для get_info" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);
        fs.get_info(buffer);
    }

    void handle_remove(istringstream& args){
        string name;
        if(!(args >> name)){ 
            cout << "неправильные входные данные для remove" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);
        fs.remove(buffer);
    }

    void handle_rename(istringstream& args){
        string name, new_name;
        if(!(args >> name >> new_name)){ 
            cout << "неправильные входные данные для rename" << endl;
            return;
        }

        char buffer[32] = {}, buffer2[32] = {};
        strncpy(buffer, name.c_str(), 31);
        strncpy(buffer2, new_name.c_str(), 31);

        fs.rename(buffer, buffer2);
    }

    void handle_move(istringstream& args){
        string name;
        int dest_addr;

        if(!(args >> name >> dest_addr)){ 
            cout << "неправильные входные данные для move" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);
        fs.move(buffer, dest_addr);
    }

    void handle_copy(istringstream& args){
        string name;
        int dest_addr;

        if(!(args >> name >> dest_addr)){ 
            cout << "неправильные входные данные для copy" << endl;
            return;
        }
        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);
        fs.copy(buffer, dest_addr);
    }

    void handle_set_acl(istringstream& args){
        int uid;
        string name;
        bool can_read, can_write, can_traverse;

        if(!(args >> uid >> name >> can_read >> can_write)){ 
            cout << "неправильные входные данные для set_acl" << endl;
            return;
        }

        char buffer[32] = {}; 
        strncpy(buffer, name.c_str(), 31);

        if(args >> can_traverse){
            fs.set_acl(uid, buffer, can_read, can_write, can_traverse);
            return;
        }
        fs.set_acl(uid, buffer, can_read, can_write);

    }
};