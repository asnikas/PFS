#include "Menu.h"
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace std;

Menu::Menu(FileSystem& fs, bool is_interactive) : fs(fs) {this->is_interactive = is_interactive;}

void Menu::run(){
    string input;
    while(running){

        try{
            if(is_interactive) cout << fs.user_name + ": " + fs.pwd() + "> ";
        }
        catch(...){
            cout << "> ";
        }

        if(!getline(cin, input)) { //getline возвращает поток, а поток в условии дает false, 
                //если в нем есть флаг ошибки
                break;
        }

        istringstream iss(input); //оборачиваем строку так, чтобы из нее можно было читать, как из cin, поток, который читает из готовой строки
        string command;
        iss >> command; //читает первое слово до пробела в command, остаток строки остается внутри iss

        if(command != "") {
            try{
                dispatch(command, iss);
            }
            catch(const exception& e){ //передаем сюда тип данных runtime_error 
                //(который содержит еще и текст сообщения), если параметр ф-ии не будет ссылкой, то текст 
                //ошибки потеряется (e.what() вернет чет типа std::exception)
                cout << "error: " << e.what() << endl;
            }
            try{
                fs.sync();
            }
            catch(const exception& e){
                cout << "ошибка: " << e.what() << endl;
            }
        }
    }
}

void Menu::dispatch(string command, istringstream& args){ //istringstream нельзя передавать по значению, только по ссылке или указателю
    if(!fs.mounted && command != "mkfs" && command != "exit"){
        cout << "образ не подключен, выполните mkfs" << endl;
        return;
    }
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
    else if(command == "stats") handle_stats(args);
    else if(command == "pwd") handle_pwd(args);
    else if(command == "exit") running = false;
    else cout << "неизвестная команда: " << command << endl;
}

void Menu::handle_mkfs(istringstream& args){
    int image_size;
    string admin_name;

    if(!(args >> image_size >> admin_name)){ //пытаемся вычитать из остатка строки сначала число, потом слово
        cout << "неправильные входные данные для mkfs" << endl;
        return;
    }

    char name[32] = {};
    fs.check_name(admin_name);
    strncpy(name, admin_name.c_str(), 31);

    fs.mkfs(image_size, name);
}

void Menu::handle_login(istringstream& args){
    string name;
    if(!(args >> name)){ 
        cout << "неправильные входные данные для login" << endl;
        return;
    }

    char buffer[32] = {}; //?
    strncpy(buffer, name.c_str(), 31);

    fs.login(buffer);

}

void Menu::handle_register(istringstream& args){
    string name;
    if(!(args >> name)){ 
        cout << "неправильные входные данные для register_user" << endl;
        return;
    }
    char buffer[32] = {}; //?
    fs.check_name(name);
    strncpy(buffer, name.c_str(), 31);

    fs.register_user(buffer);
}

void Menu::handle_cd(istringstream& args){
    string path;
    if(!(args >> path)){ 
        cout << "неправильные входные данные для cd" << endl;
        return;
    }

    if(path == "/"){
        fs.cd(path, true);
    }

    else fs.cd(path);
}

void Menu::handle_ls(istringstream& args){
    fs.ls();
}

void Menu::handle_create(istringstream& args){
    string path;
    bool can_read, can_write;
    if(!(args >> path >> can_read >> can_write)){ 
        cout << "неправильные входные данные для create" << endl;
        return;
    }
    char buffer[32] = {}; 
    fs.check_name(path);
    strncpy(buffer, path.c_str(), 31);

    fs.create(buffer, can_read, can_write);
}

void Menu::handle_create_encrypted(istringstream& args){
    string path;
    bool can_read, can_write;
    if(!(args >> path >> can_read >> can_write)){ 
        cout << "неправильные входные данные для create_encrypted" << endl;
        return;
    }
    char buffer[32] = {}; 
    fs.check_name(path);
    strncpy(buffer, path.c_str(), 31);

    fs.create_encrypted(buffer, can_read, can_write);
}

void Menu::handle_mkdir(istringstream& args){
    string path;
    bool can_read, can_write, can_traverse;
    if(!(args >> path >> can_read >> can_write >> can_traverse)){ 
        cout << "неправильные входные данные для mkdir" << endl;
        return;
    }
    char buffer[32] = {}; 
    fs.check_name(path);
    strncpy(buffer, path.c_str(), 31);

    fs.mkdir(buffer, can_read, can_write, can_traverse);
}

void Menu::handle_read(istringstream& args){
    string name;
    if(!(args >> name)){ 
        throw runtime_error("неправильные входные данные для read");
    }
    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);

    vector <char> output_vec = fs.read(buffer);
    //vector<char> result = move(output_vec.value()); //достает сам vector<char> из optional 
    //move пермещает байты вектора, избавляя нас от копии

    cout.write(output_vec.data(), output_vec.size());
    cout << endl;
}

void Menu::handle_write(istringstream& args){
    string name;
    if(!(args >> name)){ 
        cout << "неправильные входные данные для write" << endl;
        return;
    }

    string data;
    getline(args >> ws, data); //забираем все, что осталось после имени
    //ws - манипулятор, вычитывающий все подряд идущие пробельные символы и 
    //останавливающийся на первом непробельном

    vector <char> data_buffer(data.begin(), data.end());

    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);
    fs.write(buffer, data_buffer);
}

void Menu::handle_get_info(istringstream& args){
    string name;
    if(!(args >> name)){ 
        cout << "неправильные входные данные для get_info" << endl;
        return;
    }
    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);
    fs.get_info(buffer);
}

void Menu::handle_remove(istringstream& args){
    string name;
    if(!(args >> name)){ 
        cout << "неправильные входные данные для remove" << endl;
        return;
    }
    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);
    fs.remove(buffer);
}

void Menu::handle_rename(istringstream& args){
    string name, new_name;
    if(!(args >> name >> new_name)){ 
        cout << "неправильные входные данные для rename" << endl;
        return;
    }

    char buffer[32] = {}, buffer2[32] = {};
    fs.check_name(new_name);
    strncpy(buffer, name.c_str(), 31);
    strncpy(buffer2, new_name.c_str(), 31);

    fs.rename(buffer, buffer2);
}

void Menu::handle_move(istringstream& args){
    string name;
    string destination_path;

    if(!(args >> name >> destination_path)){ 
        cout << "неправильные входные данные для move" << endl;
        return;
    }
    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);
    fs.move(buffer, destination_path);
}

void Menu::handle_copy(istringstream& args){
    string name;
    string destination_path;

    if(!(args >> name >> destination_path)){ 
        cout << "неправильные входные данные для copy" << endl;
        return;
    }
    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);
    fs.copy(buffer, destination_path);
}

void Menu::handle_set_acl(istringstream& args){
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

void Menu::handle_stats(istringstream& args){
    fs.stats();
}

void Menu::handle_pwd(istringstream& args){
    cout << fs.pwd() << endl;
}