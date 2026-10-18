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

    allocator = Allocator(sizeof(Superblock), image_size);

    parser.truncate(); //стереть старый образ целиком
    superblock.next_id = 0;

    UsersTable users_table = UsersTable(superblock); //передаем суперблок, т.к это 1 из полей UsersTable
    users_table.add(admin_name, encryption.generate_key().data());

    int users_table_addr = allocator.alloc(users_table.users_vec.size() * sizeof(User));
    users_table.save(parser, users_table_addr); //добавляем админа

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
    mounted = true;
}

void FileSystem::sync(){ //обновляем alloc_free у суперблока
    if(!mounted) return;
    allocator.save(superblock);
    superblock.save(parser, 0);
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
        us_table.add(name, encryption.generate_key().data());
        int new_addr = allocator.alloc(us_table.users_vec.size() * sizeof(User));
        if(new_addr == -1){
            throw runtime_error("alloc вернул -1 для register_user в FileSystem");
        }
        us_table.save(parser, new_addr);
        superblock.users_table = new_addr;
        superblock.users_table_size = us_table.users_vec.size();
        superblock.save(parser, 0);

        FolderDescriptor fd;
        fd.load(parser, superblock.root_dir_addr);
        FolderACList fd_acl;
        fd_acl.load(parser, fd.acl_addr);
        fd_acl.grant(us_table.users_vec.back().id, 1, 0, 1);
        int new_acl_addr = allocator.alloc(sizeof(int) + fd_acl.ACList.size() * sizeof(FolderAccessEntry));
        if(new_acl_addr == -1) return;
        fd.acl_addr = new_acl_addr; 
        fd.acl_size = fd_acl.ACList.size();
        fd.save(parser, superblock.root_dir_addr);
        fd_acl.save(parser, new_acl_addr);
    }
    else{
        throw runtime_error(string("В системе уже есть пользователь с именем") + name);
    }
}

void FileSystem::cd(string path, bool to_root){
    if(to_root){
        curr_folder = superblock.root_dir_addr;
        memcpy(curr_folder_name, "ROOT", 5);
        return;
    }
    char temp_folder_name[32];
    memcpy(temp_folder_name, curr_folder_name, 32);
    int addr = resolver.resolve(parser, path, curr_folder, temp_folder_name, curr_uid);
    if(addr != -1){
        int type;
        parser.read(addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 3) {
            curr_folder = addr;
            memcpy(curr_folder_name, temp_folder_name, 32);
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

        int acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FileAccessEntry));
        int st_addr = allocator.alloc(sizeof(int) + 1 * sizeof(StreamDescriptor));
        int main_addr = allocator.alloc(0);
        int addr = allocator.alloc(sizeof(CommonFileDescriptor));

        if(acl_addr == -1 || st_addr == -1 || main_addr == -1 || addr == -1){
            cout << "alloc вернул -1 для create в FileSystem" << endl;
            return;
        }

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
        int new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));
        if(new_ft_addr == -1){
            return;
        }
        folder.file_table_addr = new_ft_addr;
        folder.file_table_size = ft.table_node_vec.size();
        ft.save(parser, new_ft_addr);
        folder.save(parser, curr_folder);
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

        int acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FileAccessEntry));
        int st_addr = allocator.alloc(sizeof(int) + 2 * sizeof(StreamDescriptor));
        int main_addr = allocator.alloc(0);
        int symkey_addr = allocator.alloc(0);
        int addr = allocator.alloc(sizeof(EncryptedFileDescriptor));
        int auth_usrs_addr = allocator.alloc(sizeof(int) + 1 * sizeof(int));

        if(acl_addr == -1 || st_addr == -1 || main_addr == -1 || symkey_addr == -1 || addr == -1 || auth_usrs_addr == -1){
            cout << "allocator вернул -1 для create_encrypted в FileSystem" << endl;
            return;
        }

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
        int new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));
        if(new_ft_addr == -1){
            return;
        }
        folder.file_table_addr = new_ft_addr;
        folder.file_table_size = ft.table_node_vec.size();
        ft.save(parser, new_ft_addr);
        folder.save(parser, curr_folder);
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

        int acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FolderAccessEntry));
        int ft_addr = allocator.alloc(sizeof(int) + 0 * sizeof(TableNode));
        int addr = allocator.alloc(sizeof(FolderDescriptor));

        if(acl_addr == -1 || ft_addr == -1 || addr == -1){
            cout << "allocator вернул -1 для mkdir в FileSystem" << endl;
            return;
        }

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
        int new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));
        if(new_ft_addr == -1){
            return;
        }
        initial_folder.file_table_addr = new_ft_addr;
        initial_folder.file_table_size = ft.table_node_vec.size();
        ft.save(parser, new_ft_addr);
        initial_folder.save(parser, curr_folder);
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
        else{
            throw runtime_error("у каталога нет метода get_info()");
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
    fd.remove(parser, name);
}

void FileSystem::rename(char name[32], char new_name[32]){
    check_name(new_name);
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    if(!fd.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может переименовывать объекты каталога ") + curr_folder_name);
    fd.rename(parser, name, new_name);
}

void FileSystem::move(char name[32], int dest_addr){
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

void FileSystem::copy(char name[32], int dest_addr){
    FolderDescriptor fd;
    fd.load(parser, curr_folder);
    int type;
    parser.read(dest_addr, reinterpret_cast<char*>(&type), sizeof(int));
    if(type != 3) throw runtime_error("Выбранный файл не является каталогом");
    if(!fd.can_write(parser, curr_uid)) throw runtime_error(string
        ("текущий пользователь не может скопировать объект каталога ") + curr_folder_name);
    fd.copy(parser, allocator, name, dest_addr);
}

void FileSystem::set_acl(int uid, char name[32], bool can_read, bool can_write, bool can_traverse){

    UsersTable users_table = UsersTable(superblock);
    users_table.load(parser, superblock.users_table);
    if(users_table.search(uid) == -1){
        throw runtime_error("Для set_acl пользователя с uid " + to_string(uid) +  " нет в системе");
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
            if(is_new){
                addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FileAccessEntry));
                if(addr == -1) return;               // образ закончился
                cfd.acl_addr = addr;
                cfd.acl_size = acl.ACList.size();
                cfd.save(parser, ft.table_node_vec[index].addr);
            }
            acl.save(parser, addr);
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

            if(enfd.set_acl(parser, allocator, encryption, curr_uid, uid, can_read, can_write, actor_key, user_key)){
                enfd.save(parser, ft.table_node_vec[index].addr);
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
            if(is_new){
                addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FolderAccessEntry));
                if(addr == -1) return;
                fod.acl_addr = addr;
                fod.acl_size = acl.ACList.size();
                fod.save(parser, ft.table_node_vec[index].addr);
            }
            acl.save(parser, addr);
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
