#include "FileSystem.h"
#include "UsersTable.h"
#include "User.h"
#include "FolderACList.h"
#include "FileACList.h"
#include "FileTable.h"
#include "FolderDescriptor.h"
#include "CommonFileDescriptor.h"
#include "EncryptedFileDescriptor.h"
#include "StreamTable.h"
#include "AuthorizedUsers.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace std;

const int MIN_IMAGE_SIZE = 4096; //4 КБ

FileSystem::FileSystem(Parser& parser, Allocator& allocator,
    Superblock& superblock) : parser(parser), allocator(allocator), superblock(superblock) {}

void FileSystem::mkfs(int image_size, char admin_name[32]){ //создать фс

    if(image_size < MIN_IMAGE_SIZE) throw 
        runtime_error("минимальный размер системы должен быть 4096 Б, Вы ввели " + 
            to_string(image_size) + " Б");

    if(mounted){
        UsersTable users_table = UsersTable(superblock);
        users_table.load(parser, superblock.users_table);
        if(curr_uid != users_table.users_vec[0].id) throw 
        runtime_error("Выполнить mkfs для mounted системы может только админ");
    }

    check_name(admin_name);

    allocator = Allocator(parser, sizeof(Superblock), image_size);

    parser.truncate(); //стереть старый образ целиком
    superblock.next_id = 0;

    UsersTable users_table = UsersTable(superblock); //передаем суперблок, т.к это 1 из полей UsersTable
    users_table.add(admin_name, encryption.generate_key().data());

    int users_table_addr = allocator.alloc(users_table.users_vec.size() * sizeof(User));
    users_table.save(parser, users_table_addr); //добавляем админа
    user_name = "N/A";

    int root_acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FolderAccessEntry));
    int root_ft_addr = allocator.alloc(sizeof(int));
    int root_addr = allocator.alloc(sizeof(FolderDescriptor)); //адрес корневого каталога

    if(users_table_addr == -1 || root_acl_addr == -1 || root_ft_addr == -1 || root_addr == -1){
        cout << "alloc вернул -1 при mkfs в FileSystem" << endl;
        return;
    }

    FolderACList root_acl = FolderACList();
    root_acl.grant(users_table.users_vec[0].id, true, true, true);
    root_acl.save(parser, root_acl_addr);

    FileTable root_ft = FileTable();
    root_ft.save(parser, root_ft_addr);

    FolderDescriptor root_folder = FolderDescriptor();
    root_folder.init(root_acl_addr, 1, root_ft_addr, 0);
    root_folder.save(parser, root_addr);

    superblock.init(root_addr, allocator.next_free, users_table_addr, users_table.users_vec.size());
    superblock.magic = MAGIC_NUM;
    superblock.image_size = image_size;
    superblock.save(parser, 0);

    curr_folder = root_addr;
    curr_uid = -1;
    memcpy(curr_folder_name, "ROOT", 5); //5 т.к в конце надо поставить \0
    path_stack.clear();
    path_stack.push_back({superblock.root_dir_addr, "/"});
    mounted = true;
}

void FileSystem::sync(){ //обновляем alloc_free у суперблока
    if(!mounted) return;
    allocator.save(superblock);
    superblock.save(parser, 0);
}

void FileSystem::logout(){
    user_name = "N/A";
    curr_uid = -1;

    curr_folder = superblock.root_dir_addr;
    memset(curr_folder_name, 0, 32); 
    memcpy(curr_folder_name, "ROOT", 5);

    path_stack.clear();
    path_stack.push_back({curr_folder, "/"});
}

string FileSystem::pwd(){
    string addr;
    if(path_stack.size() == 0) throw runtime_error("адресная строка пустая (pwd)");
    if(path_stack.size() == 1){
        return "/";
    }
    for(int i = 1; i < path_stack.size(); i++){
        addr += ("/" + path_stack[i].second);
    }
    return addr;
}

bool FileSystem::mount(){
    if(parser.file_size() < (int)sizeof(Superblock)) return false;

    Superblock spb;
    spb.load(parser, 0);
    if(spb.magic != MAGIC_NUM) return false;

    superblock = spb;
    allocator.next_free = superblock.alloc_free;
    allocator.image_size = superblock.image_size;

    curr_folder = superblock.root_dir_addr;
    memset(curr_folder_name, 0, 32); //заполнили переменную curr_folder_name нулями
    memcpy(curr_folder_name, "ROOT", 5);
    mounted = true;

    path_stack.clear();
    path_stack.push_back({curr_folder, "/"});

    return true;
}

void FileSystem::login(char name[32]){
    UsersTable us_table(superblock);
    us_table.load(parser, superblock.users_table);
    int id = us_table.search_uid_by_name(name);
    if(id != -1) {
        curr_uid = id;
        curr_folder = superblock.root_dir_addr;
        memset(curr_folder_name, 0, 32); 
        memcpy(curr_folder_name, "ROOT", 5);

        path_stack.clear();
        path_stack.push_back({curr_folder, "/"});
        user_name = string(name);
    }
    else throw runtime_error(string("пользователь с именем ") + name + " не найден");
}

void FileSystem::register_user(char name[32]){
    UsersTable us_table(superblock);
    us_table.load(parser, superblock.users_table);
    if(curr_uid != us_table.users_vec[0].id) throw 
    runtime_error("Только админ может регистрировать новых пользователей");
    check_name(name);
    if(us_table.search_uid_by_name(name) == -1){
        //до любых изменений: UsersTable::add меняет superblock.next_id, UsersTable::save - superblock.users_table,
        //а sync() в меню сохраняет суперблок даже после ошибки, поэтому при откате их надо вернуть
        int old_next_id = superblock.next_id;
        int old_users_table_addr = superblock.users_table;
        int old_users_table_size = superblock.users_table_size;
        int new_addr = -1, new_acl_addr = -1, old_acl_addr = -1;

        try{
            us_table.add(name, encryption.generate_key().data());
            new_addr = allocator.alloc(us_table.users_vec.size() * sizeof(User));
            us_table.save(parser, new_addr);
            superblock.users_table = new_addr;
            superblock.users_table_size = us_table.users_vec.size();
            superblock.save(parser, 0);

            FolderDescriptor fd;
            fd.load(parser, superblock.root_dir_addr);
            FolderACList fd_acl;
            fd_acl.load(parser, fd.acl_addr);
            fd_acl.grant(us_table.users_vec.back().id, 1, 0, 1);
            new_acl_addr = allocator.alloc(sizeof(int) + fd_acl.ACList.size() * sizeof(FolderAccessEntry));
            old_acl_addr = fd.acl_addr;
            fd.acl_addr = new_acl_addr;
            fd.acl_size = fd_acl.ACList.size();
            //сначала содержимое нового ACL, потом дескриптор корня, который на него ссылается
            fd_acl.save(parser, new_acl_addr);
            fd.save(parser, superblock.root_dir_addr);
        }
        catch(...){ //пользователь не зарегистрирован: возвращаем суперблок и освобождаем новые блоки
            superblock.next_id = old_next_id;
            superblock.users_table = old_users_table_addr;
            superblock.users_table_size = old_users_table_size;
            superblock.save(parser, 0);
            if(new_addr != -1) allocator.free(new_addr);
            if(new_acl_addr != -1) allocator.free(new_acl_addr);
            throw;
        }
        allocator.free(old_users_table_addr);
        if(old_acl_addr != -1) allocator.free(old_acl_addr);
    }
    else{
        throw runtime_error(string("В системе уже есть пользователь с именем ") + name);
    }
}

void FileSystem::cd(string path, bool to_root){
    if(to_root){
        curr_folder = superblock.root_dir_addr;
        memset(curr_folder_name, 0, 32);
        memcpy(curr_folder_name, "ROOT", 5);
        path_stack = {{curr_folder, "/"}};
        return;
    }
    if(path == ".."){
        if(path_stack.size() == 1) throw runtime_error("пользователь уже находится в корне");
        path_stack.pop_back();
        curr_folder = path_stack.back().first;
        memset(curr_folder_name, 0, 32);
        strncpy(curr_folder_name, path_stack.back().second.c_str(), 31);
        return;
    }
    char temp_folder_name[32];
    memcpy(temp_folder_name, curr_folder_name, 32);
    std::vector<std::pair<int, std::string>> steps = resolver.resolve(parser, path, curr_folder, temp_folder_name, curr_uid);
    if(steps.empty()) throw runtime_error("команда " + path + " оставляет пользователя в текущем каталоге");
    int addr = steps.back().first;
    if(addr != -1){
        int type;
        parser.read(addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 3) {
            curr_folder = addr;
            memcpy(curr_folder_name, temp_folder_name, 32);
            path_stack.insert(path_stack.end(), steps.begin(), steps.end());
            return;
        }
        throw runtime_error("Попытались открыть файл вместо каталога в cd");
    }
}

void FileSystem::ls(){
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    if(fd.can_read(parser, curr_uid)) fd.show(parser);
    else throw runtime_error(string("текущий пользователь не может прочитать каталог ") + 
    curr_folder_name);
}

void FileSystem::create(char file_name[32], bool can_read, bool can_write){
    check_name(file_name);

    FileTable ft;
    FolderDescriptor folder;
    folder.load(parser, curr_folder);
    if(!folder.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может писать в каталог ") + curr_folder_name);
    ft.load(parser, folder.file_table_addr);
    if(ft.search(file_name) == -1){

        int acl_addr = -1, st_addr = -1, main_addr = -1, addr = -1, new_ft_addr = -1, old_ft_addr = -1;

        try{
            acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FileAccessEntry));
            st_addr = allocator.alloc(sizeof(int) + 1 * sizeof(StreamDescriptor));
            main_addr = allocator.alloc(0);
            addr = allocator.alloc(sizeof(CommonFileDescriptor));

            FileACList acl = FileACList();
            acl.grant(curr_uid, can_read, can_write);
            acl.save(parser, acl_addr);

            StreamTable table = StreamTable();
            char buf_table[32] = {};
            strncpy(buf_table, "MAIN", 31);
            table.add(buf_table, main_addr, 0, 0);
            table.save(parser, st_addr);

            CommonFileDescriptor file;
            file.init(curr_uid, st_addr, 1, acl_addr, 1);
            file.save(parser, addr);

            ft.add(file_name, addr);
            new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));

            old_ft_addr = folder.file_table_addr;
            folder.file_table_addr = new_ft_addr;
            folder.file_table_size = ft.table_node_vec.size();
            ft.save(parser, new_ft_addr);
            folder.save(parser, curr_folder);
        }
        catch(...){ //поймать любой тип ошибки
            if(acl_addr != -1)allocator.free(acl_addr);
            if(st_addr != -1) allocator.free(st_addr);
            if(main_addr != -1) allocator.free(main_addr);  
            if(addr != -1) allocator.free(addr);
            if(new_ft_addr != -1) allocator.free(new_ft_addr);
            throw;
        }
        if(old_ft_addr != -1) allocator.free(old_ft_addr);
        return;
    }
    throw runtime_error("при создании обычного файла выяснилось, что файл с таким именем уже есть");
}

void FileSystem::create_encrypted(char file_name[32], bool can_read, bool can_write){
    check_name(file_name);

    FileTable ft;
    FolderDescriptor folder;
    folder.load(parser, curr_folder);
    if(!folder.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может писать в каталог ") + curr_folder_name);
    ft.load(parser, folder.file_table_addr);
    if(ft.search(file_name) == -1){

        int acl_addr = -1, st_addr = -1, main_addr = -1, symkey_addr = -1, addr = -1, auth_usrs_addr = -1;
        int new_ft_addr = -1, old_ft_addr = -1;

        try{
            acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FileAccessEntry));
            st_addr = allocator.alloc(sizeof(int) + 2 * sizeof(StreamDescriptor));
            main_addr = allocator.alloc(0);
            symkey_addr = allocator.alloc(0);
            addr = allocator.alloc(sizeof(EncryptedFileDescriptor));
            auth_usrs_addr = allocator.alloc(sizeof(int) + 1 * sizeof(int));

            AuthorizedUsers usrs = AuthorizedUsers(); //новый объект, load сделать нельзя, так как прочитается мусор
            usrs.add(curr_uid);
            usrs.save(parser, auth_usrs_addr);

            FileACList acl = FileACList();
            acl.grant(curr_uid, can_read, can_write);
            acl.save(parser, acl_addr);

            StreamTable table = StreamTable();
            char buf_table[32] = {};
            strncpy(buf_table, "MAIN", 31);
            table.add(buf_table, main_addr, 0, 0);
            strncpy(buf_table, "SYMKEY", 31);
            table.add(buf_table, symkey_addr, 0, 0);
            table.save(parser, st_addr);

            EncryptedFileDescriptor file;
            file.init(curr_uid, st_addr, 2, acl_addr, 1, auth_usrs_addr);
            file.save(parser, addr);

            ft.add(file_name, addr);
            new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));

            old_ft_addr = folder.file_table_addr;
            folder.file_table_addr = new_ft_addr;
            folder.file_table_size = ft.table_node_vec.size();
            ft.save(parser, new_ft_addr);
            folder.save(parser, curr_folder);
        }
        catch(...){ //поймать любой тип ошибки
            if(acl_addr != -1) allocator.free(acl_addr);
            if(st_addr != -1) allocator.free(st_addr);
            if(main_addr != -1) allocator.free(main_addr);
            if(symkey_addr != -1) allocator.free(symkey_addr);
            if(addr != -1) allocator.free(addr);
            if(auth_usrs_addr != -1) allocator.free(auth_usrs_addr);
            if(new_ft_addr != -1) allocator.free(new_ft_addr);
            throw;
        }
        if(old_ft_addr != -1) allocator.free(old_ft_addr);
        return;
    }

    throw runtime_error("при создании зашифрованного файла выяснилось, что файл с таким именем уже есть");
}

void FileSystem::mkdir(char folder_name[32], bool can_read, bool can_write, bool can_traverse){
    check_name(folder_name);
    FileTable ft;
    FolderDescriptor initial_folder;
    initial_folder.load(parser, curr_folder);
    if(!initial_folder.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может писать в каталог ") + curr_folder_name);
    ft.load(parser, initial_folder.file_table_addr);
    if(ft.search(folder_name) == -1){

        int acl_addr = -1, ft_addr = -1, addr = -1, new_ft_addr = -1, old_ft_addr = -1;

        try{
            acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FolderAccessEntry));
            ft_addr = allocator.alloc(sizeof(int) + 0 * sizeof(TableNode));
            addr = allocator.alloc(sizeof(FolderDescriptor));

            FolderACList acl = FolderACList();
            acl.grant(curr_uid, can_read, can_write, can_traverse);
            acl.save(parser, acl_addr);

            FolderDescriptor folder;
            folder.init(acl_addr, 1, ft_addr, 0);
            folder.save(parser, addr);

            FileTable new_ft = FileTable(); //создает объект вызовом автоматического конструктора,
            //преимущество над FileTable new_ft; в том, что может занулять скалярные переменные
            new_ft.save(parser, ft_addr);

            ft.add(folder_name, addr);
            new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));

            old_ft_addr = initial_folder.file_table_addr;
            initial_folder.file_table_addr = new_ft_addr;
            initial_folder.file_table_size = ft.table_node_vec.size();
            ft.save(parser, new_ft_addr);
            initial_folder.save(parser, curr_folder);
        }
        catch(...){ //поймать любой тип ошибки
            if(acl_addr != -1) allocator.free(acl_addr);
            if(ft_addr != -1) allocator.free(ft_addr);
            if(addr != -1) allocator.free(addr);
            if(new_ft_addr != -1) allocator.free(new_ft_addr);
            throw;
        }
        if(old_ft_addr != -1) allocator.free(old_ft_addr);
        return;

    }
    throw runtime_error("при создании подкаталога выяснилось, что каталог с таким именем уже есть");
}

vector <char> FileSystem::read(char name[32]){
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    FileTable ft;
    ft.load(parser, fd.file_table_addr);
    int index = ft.search(name);
    if(index != -1){
        int type;
        parser.read(ft.table_node_vec[index].addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 1){
            CommonFileDescriptor cfd;
            cfd.load(parser, ft.table_node_vec[index].addr);
            return cfd.read(parser, curr_uid);
        }
        else if(type == 2){
            EncryptedFileDescriptor enfd;
            enfd.load(parser, ft.table_node_vec[index].addr);
            UsersTable ut(superblock);
            ut.load(parser, superblock.users_table);
            uint8_t usr_key[32];
            int usr_key_index = ut.search(curr_uid);
            if(usr_key_index == -1) throw runtime_error(string("не найден пользователь при чтении зашифрованного файла") + name);
            memcpy(usr_key, ut.users_vec[usr_key_index].key, 32);
            
            return enfd.read(parser, encryption, curr_uid, vector(usr_key, usr_key + 32 * sizeof(uint8_t)));
        }

        else{
            throw runtime_error("попытались открыть каталог вместо файла");
        }
    }
    throw runtime_error(string("объект ") + name + " не найден в текущем каталоге");
}

void FileSystem::write(char name[32], vector <char> buffer){
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    FileTable ft;
    ft.load(parser, fd.file_table_addr);
    int index = ft.search(name);
    if(index != -1){
        int type;
        parser.read(ft.table_node_vec[index].addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 1){
            CommonFileDescriptor cfd;
            cfd.load(parser, ft.table_node_vec[index].addr);
            cfd.write(parser, curr_uid, buffer, allocator);
            cfd.save(parser, ft.table_node_vec[index].addr);
        }
        else if(type == 2){
            vector<vector <uint8_t>> all_user_keys;
            EncryptedFileDescriptor enfd;
            enfd.load(parser, ft.table_node_vec[index].addr);
            AuthorizedUsers auth_usrs;
            auth_usrs.load(parser, enfd.authorized_usr_addr);
            UsersTable ut(superblock);
            ut.load(parser, superblock.users_table);
            for(auto uid : auth_usrs.AuthorizedUsers){
                int usr_index = ut.search(uid);
                if(usr_index == -1) throw runtime_error(string("пользователь не найден при попытке записать в файл ") + name);
                all_user_keys.push_back(vector(ut.users_vec[usr_index].key, ut.users_vec[usr_index].key + 32 * sizeof(uint8_t)));
            }
            enfd.write(parser, encryption, curr_uid, buffer, allocator, all_user_keys);
            enfd.save(parser, ft.table_node_vec[index].addr);
        }
        else{
            throw runtime_error("попытались записать в каталог вместо файла");
        }
        return;
    }
    throw runtime_error("файл не найден");
}

void FileSystem::get_info(char name[32]){
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    FileTable ft;
    ft.load(parser, fd.file_table_addr);
    int index = ft.search(name);
    if(index != -1){
        int type;
        parser.read(ft.table_node_vec[index].addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 1){
            CommonFileDescriptor cfd;
            cfd.load(parser, ft.table_node_vec[index].addr);
            cfd.get_info(parser);
        }
        else if(type == 2){
            EncryptedFileDescriptor enfd;
            enfd.load(parser, ft.table_node_vec[index].addr);
            enfd.get_info(parser);
        }
        else if(type == 3){
            FolderDescriptor fd;
            fd.load(parser, ft.table_node_vec[index].addr);
            fd.get_info(parser);

        }
        return;
    }
    throw runtime_error(string("файл ") + name + " для операции get_info() не найден");
}

void FileSystem::remove(char name[32]){
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    if(!fd.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может удалять из каталога ") + curr_folder_name);
    fd.remove(parser, allocator, name);
}

void FileSystem::rename(char name[32], char new_name[32]){
    check_name(new_name);
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    if(!fd.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может переименовывать объекты каталога ") + curr_folder_name);
    fd.rename(parser, name, new_name);
}

void FileSystem::move(char name[32], std::string path){
    char buf[32] = {};
    int dest_addr;
    strncpy(buf, "ROOT", 31);
    std::vector<std::pair<int, std::string>> steps = resolver.resolve(parser, path, 
        superblock.root_dir_addr, buf, curr_uid);

    if(steps.size() == 0) dest_addr = superblock.root_dir_addr;

    else dest_addr = steps.back().first;

    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    int type;
    parser.read(dest_addr, reinterpret_cast<char*>(&type), sizeof(int));
    if(type != 3) throw runtime_error("Выбранный файл не является каталогом");
    FolderDescriptor dest_fd;
    dest_fd.load(parser, dest_addr);

    if(!fd.can_write(parser, curr_uid) || !dest_fd.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может переместить объект из каталога ") + curr_folder_name);
    fd.move(allocator, parser, name, dest_addr);
}

void FileSystem::copy(char name[32], std::string path){
    char buf[32] = {};
    int dest_addr;
    strncpy(buf, "ROOT", 31);
    std::vector<std::pair<int, std::string>> steps = resolver.resolve(parser, path, 
        superblock.root_dir_addr, buf, curr_uid);

    if(steps.size() == 0) dest_addr = superblock.root_dir_addr;

    else dest_addr = steps.back().first;
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    int type;
    parser.read(dest_addr, reinterpret_cast<char*>(&type), sizeof(int));
    if(type != 3) throw runtime_error("Выбранный файл не является каталогом");
    FolderDescriptor dest_folder;
    dest_folder.load(parser, dest_addr);
    if(!fd.can_write(parser, curr_uid) || !dest_folder.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может скопировать объект каталога ") + curr_folder_name);
    fd.copy(parser, allocator, name, dest_addr);
}

void FileSystem::set_acl(char user_name[32], char name[32], bool can_read, bool can_write, bool can_traverse){

    UsersTable users_table = UsersTable(superblock);
    users_table.load(parser, superblock.users_table);

    int uid = users_table.search_uid_by_name(user_name);

    if(uid == -1){
        throw runtime_error(string("Для set_acl пользователя с именем ") + user_name + " нет в системе");
    }

    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    FileTable ft;
    ft.load(parser, fd.file_table_addr);
    int index = ft.search(name);
    if(index != -1){
        int type;
        parser.read(ft.table_node_vec[index].addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 1){
            CommonFileDescriptor cfd;
            cfd.load(parser, ft.table_node_vec[index].addr);

            if(cfd.owner_id != curr_uid) throw 
            runtime_error(string("текущий пользователь не является владельцем обычного файла ") + name);

            FileACList acl;
            acl.load(parser, cfd.acl_addr);

            bool is_new = !acl.search(uid);          // search возвращает bool
            acl.grant(uid, can_read, can_write);

            int addr = cfd.acl_addr;
            int old_acl_addr = cfd.acl_addr;
            if(is_new){
                addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FileAccessEntry));
                if(addr == -1) return;               // образ закончился
                cfd.acl_addr = addr;
                cfd.acl_size = acl.ACList.size();
                cfd.save(parser, ft.table_node_vec[index].addr);
            }
            acl.save(parser, addr);
            if(is_new) allocator.free(old_acl_addr);
        }

        if(type == 2){
            EncryptedFileDescriptor enfd;
            enfd.load(parser, ft.table_node_vec[index].addr);

            if(enfd.owner_id != curr_uid) throw 
            runtime_error(string("текущий пользователь не является владельцем зашифрованного файла ") 
            + name);

            UsersTable ut = UsersTable(superblock);
            ut.load(parser, superblock.users_table);
            int actor_idx = ut.search(curr_uid);
            int user_idx = ut.search(uid);
            if(actor_idx == -1 || user_idx == -1) return;
            vector <uint8_t> actor_key(ut.users_vec[actor_idx].key, ut.users_vec[actor_idx].key + 32);
            vector <uint8_t> user_key(ut.users_vec[user_idx].key, ut.users_vec[user_idx].key + 32);

            int old_auth_addr = enfd.authorized_usr_addr;
            int old_acl_addr = enfd.acl_addr;
            bool saved = false;
            try{
                if(enfd.set_acl(parser, allocator, encryption, curr_uid, uid, can_read, can_write, actor_key, user_key)){
                    enfd.save(parser, ft.table_node_vec[index].addr);
                    saved = true;
                }
            }
            catch(...){ //set_acl мог успеть выделить новый список авторизованных, а потом упасть на alloc под ACL
                if(enfd.authorized_usr_addr != old_auth_addr) allocator.free(enfd.authorized_usr_addr);
                if(enfd.acl_addr != old_acl_addr) allocator.free(enfd.acl_addr);
                throw;
            }
            if(saved){
                //старые блоки освобождаем только после того, как дескриптор сохранен с новыми адресами
                if(enfd.authorized_usr_addr != old_auth_addr) allocator.free(old_auth_addr);
                if(enfd.acl_addr != old_acl_addr) allocator.free(old_acl_addr);
            }
        }

        if(type == 3){
            if(curr_uid != users_table.users_vec[0].id) throw 
            runtime_error("только админ может менять права для пользователей в каталоге");
            FolderDescriptor fod;
            fod.load(parser, ft.table_node_vec[index].addr);

            FolderACList acl;
            acl.load(parser, fod.acl_addr);

            bool is_new = !acl.search(uid);
            acl.grant(uid, can_read, can_write, can_traverse);

            int addr = fod.acl_addr;
            int old_acl_addr = fod.acl_addr;
            if(is_new){
                addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FolderAccessEntry));
                if(addr == -1) return;
                fod.acl_addr = addr;
                fod.acl_size = acl.ACList.size();
                fod.save(parser, ft.table_node_vec[index].addr);
            }
            acl.save(parser, addr);
            if(is_new) allocator.free(old_acl_addr);
        }
        return;
    }
    throw runtime_error(string("файл ") + name + " не найден");
}

void FileSystem::check_name(const string& name){
    if(name.size() == 0) throw runtime_error("название длины 0 недопустимо");
    if(name.size() > 31) throw runtime_error("название больше длины 31 недопустимо");
    if(name == "." || name == "..") throw runtime_error("название '.' и '..' недопустимо");

    const string allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-";
    if(name.find_first_not_of(allowed) != string::npos) throw 
    runtime_error("в имени допустимы только латиница, цифры и . _ -");
}

void FileSystem::stats(){
    int free_blocks_count = 0, taken_blocks_count = 0, max_free_block_size = 0, sum_bytes_taken = 0,
    sum_bytes_non_taken = 0;
    for(int i = sizeof(Superblock); i < allocator.next_free; ){
        Header header = allocator.read_header(i);
        if(header.free) {
            free_blocks_count++;
            sum_bytes_non_taken += header.size;
            if(header.size > max_free_block_size) max_free_block_size = header.size;
        }
        else {
            taken_blocks_count++;
            sum_bytes_taken += header.size;
        }
        i += (sizeof(Header) + header.size + sizeof(Footer));
    }
    cout << "Image size: " << allocator.image_size << " bytes" << endl;
    cout << "Superblock size: " << sizeof(Superblock) << " bytes" << endl;
    cout << "Amount of free blocks: " << free_blocks_count << endl;
    cout << "Amount of taken blocks: " << taken_blocks_count << endl;
    cout << "Max free block size: " << max_free_block_size << " bytes" << endl;
    cout << "Sum of taken bytes: " << sum_bytes_taken << endl;
    cout << "Sum of non-taken bytes: " << sum_bytes_non_taken << endl;
    cout << "Tail available for allocation: " << 
    max(allocator.image_size - allocator.next_free - 9, 0) << endl;
    cout << "Sum of service bytes (header + footer): " << 
    (free_blocks_count + taken_blocks_count) * 9 << endl;
    cout << "Biggest block available: " << 
    max(max_free_block_size, max(allocator.image_size - allocator.next_free - 9, 0)) << endl;
    cout << "Fragmentation: ";
    if(!sum_bytes_non_taken) cout << "0 %, there are no free blocks" << endl;
    else cout << 100 - (double)max_free_block_size / (double)sum_bytes_non_taken * 100 << " %" << endl;
    cout << "Percent of capacity used: " << 
    ((sizeof(Superblock) + sum_bytes_taken + free_blocks_count * 9 +
    ((double)taken_blocks_count * 9))/(double)allocator.image_size)*100 << " %" << endl;

    UsersTable users_table = UsersTable(superblock);
    users_table.load(parser, superblock.users_table);
    cout << "Users table" << endl;
    for(auto el:users_table.users_vec){
        cout << "Username: " << el.name << " Id: " << el.id << endl;
    }
}