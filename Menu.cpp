#include "Menu.h"
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#ifndef NOMINMAX 
#define NOMINMAX 
#endif
#endif

using namespace std;

Menu::Menu(FileSystem& fs, bool is_interactive) : fs(fs) {this->is_interactive = is_interactive;
    #ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(CP_UTF8);
    #endif
}

void Menu::run(){
    string input;
    while(running){

        try{
            if(is_interactive) cout << "\033[32m" << fs.user_name + ": " + fs.pwd() + "> " << "\033[0m";
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
                if(is_interactive) cout << "\033[31m" << "error: " << e.what() << "\033[0m" << endl;
                else cout << "error: " << e.what() << endl;
            }
            try{
                fs.sync();
            }
            catch(const exception& e){
                if(is_interactive) cout << "\033[31m" << "ошибка: " << e.what() << "\033[0m" << endl;
                else cout << "ошибка: " << e.what() << endl;
            }
        }
    }
}

void Menu::dispatch(string command, istringstream& args){ //istringstream нельзя передавать по значению, только по ссылке или указателю
    if(!fs.mounted && command != "mkfs" && command != "exit" && command != "help"){
        if(is_interactive) cout << "\033[31m" << "образ не подключен, выполните mkfs" << "\033[0m" << endl;
        else cout << "образ не подключен, выполните mkfs" << endl;
        return;
    }
    if(fs.curr_uid == -1){
        if(command != "login" && command != "register" 
            && command != "mkfs" && command != "exit" && command != "help"){
            if(is_interactive) cout << "\033[31m" << "сначала выполните login" << "\033[0m" << endl;
            else cout << "сначала выполните login" << endl;
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
    else if(command == "import") handle_import(args);
    else if(command == "export") handle_export(args);
    else if(command == "logout") handle_logout(args);
    else if(command == "help") handle_help(args);
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
    string user_name, file_name;
    bool can_read, can_write, can_traverse;

    if(!(args >> user_name >> file_name >> can_read >> can_write)){ 
        cout << "неправильные входные данные для set_acl" << endl;
        return;
    }

    char buffer[32] = {}, buffer2[32] = {}; 
    strncpy(buffer, user_name.c_str(), 31);
    strncpy(buffer2, file_name.c_str(), 31);

    if(args >> can_traverse){
        fs.set_acl(buffer, buffer2, can_read, can_write, can_traverse);
        return;
    }
    fs.set_acl(buffer, buffer2, can_read, can_write);

}

void Menu::handle_stats(istringstream& args){
    fs.stats();
}

void Menu::handle_pwd(istringstream& args){
    cout << fs.pwd() << endl;
}

void Menu::handle_import(std::istringstream& args){
    string path, name;
    if(!(args >> std::quoted(path))){ //читает путь как единую строку с пробелами, пока он находится в кавычках
        cout << "неправильный входные данные для import" << endl;
        return;
    }
    if(!(args >> name)){
        size_t pos = path.find_last_of("/\\"); // "\" - это служенбный символ в с++, поэтому для его обозначения
        // надо писать "\\"
        if(pos == std::string::npos) name = path;
        else name = path.substr(pos + 1);
    }
    fstream import_file;
    import_file.open(path, ios::in | ios::binary);

    if(!import_file) throw runtime_error("не удалось открыть файл " + path);
    import_file.seekg(0, std::ios::end);
    int size = import_file.tellg();
    if(size < 0) throw runtime_error("размер импортируемого файла меньше 0");
    import_file.seekg(0, std::ios::beg);
    std::vector<char>file_data(size);
    import_file.read(file_data.data(), size);

    if(!import_file) throw runtime_error("не удалось прочитать данные в import");

    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);

    fs.create(buffer, 1, 1);

    try{
        fs.write(buffer, file_data);
    }
    catch(...){
        fs.remove(buffer);
        throw;
    }
}

void Menu::handle_export(std::istringstream& args){
    string path, name;
    if(!(args >> std::quoted(path) >> name)){
        cout << "неправильный входные данные для export" << endl;
        return;
    }

    char buffer[32] = {}; 
    strncpy(buffer, name.c_str(), 31);

    vector <char> file_data = fs.read(buffer);

    fstream export_file;
    export_file.open(path, ios::out | ios::binary);

    if(!export_file) throw runtime_error("не удалось открыть файл " + path);

    export_file.write(file_data.data(), file_data.size());

    if(!export_file) throw runtime_error("не удалось записать файл"); //как работает !export_file
}

void Menu::handle_logout(std::istringstream& args){
    fs.logout();
}

void Menu::handle_help(std::istringstream& args){
    cout << R"HELP(
        mkfs <size> <admin>                   create image / создать образ
        login <name>                          log in / войти
        logout                                log out / выйти из пользователя
        register <name>                       add user (admin) / добавить пользователя (админ)
        cd <dir>                              change dir (.. up, / root) / сменить каталог (.. вверх, / корень)
        pwd                                   print current path / показать текущий путь
        ls                                    list directory / показать содержимое каталога
        create <name> <r> <w>                 create file / создать файл
        create_encrypted <name> <r> <w>       create encrypted file / создать зашифрованный файл
        mkdir <name> <r> <w> <x>              create directory / создать каталог
        read <name>                           print file / вывести файл
        write <name> <text>                   overwrite file / записать в файл
        get_info <name>                       file info / информация о файле
        rename <name> <new_name>              rename / переименовать
        remove <name>                         delete file or empty dir / удалить файл или пустой каталог
        copy <name> <path>                    copy to dir / копировать в каталог
        move <name> <path>                               move to dir / переместить в каталог
        set_acl <user_name> <file_name> <r> <w> [<x>]    set access rights / задать права доступа
        import <path> [name]                             load host file / загрузить файл с диска
        export <path> <name>                             save file to host / сохранить файл на диск
        stats                                 image usage / занятость образа
        help                                  this help / эта справка
        exit                                  quit / выход

        NOTES / ВАЖНО
        Flags: 1 yes, 0 no; r read, w write, x traverse.
          Флаги: 1 да, 0 нет; r чтение, w запись, x проход.
        Names: letters, digits, . _ - (up to 31 chars).
          Имена: буквы, цифры, . _ - (до 31 символа).
        cd works from the current dir; copy/move <path> is a FULL path from root, e.g. docs/secret, or / for root.
          cd идёт от текущего каталога; в copy/move <path> это ПОЛНЫЙ путь от корня, например docs/secret, или / для корня.
        import/export <path> is a path on your real disk; if it has spaces put it in quotes and use / inside: "C:/My Files/a.jpg".
          import/export <path> это путь на вашем диске; если в нём есть пробелы, возьмите его в кавычки и используйте / внутри: "C:/My Files/a.jpg".
        import without <name> uses the file name from the path; export overwrites an existing host file.
          import без <name> берёт имя файла из пути; export перезаписывает существующий файл на диске.
        set_acl: files - only the owner; directories - only admin (use <x> for directories).
          set_acl: для файлов только владелец, для каталогов только админ (<x> указывается для каталогов); <uid> это номер пользователя (админ 0, затем 1, 2, ... по порядку register).
        Before login only login, register, mkfs, help, exit are available.
          До login доступны только login, register, mkfs, help, exit.
        )HELP" << endl;

}