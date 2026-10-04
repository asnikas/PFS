#include <iostream>
#include "Parser.cpp"
#include "Allocator.cpp"
#include "Superblock.cpp"
#include "Encryption.cpp"
#include "Resolver.cpp"
#include "UsersTable.cpp"
#include "User.cpp"
#include "FolderACList.cpp"
#include "FileTable.cpp"
#include "FolderDescriptor.cpp"
#include "EncryptedFileDescriptor.cpp"

using namespace std;

struct FileSystem{
    int curr_uid, curr_folder;
    char curr_folder_name[32] = {};
    Parser& parser;
    Allocator& allocator;
    Superblock& superblock;
    Encryption encryption;
    Resolver resolver;

    void mkfs(int image_size, char admin_name[32]){ //создать фс
        allocator = Allocator(sizeof(Superblock), image_size);

        UsersTable users_table = UsersTable(superblock); //передаем суперблок, т.к это 1 из полей UsersTable
        users_table.add(admin_name, encryption.generate_key().data());

        int users_table_addr = allocator.alloc(users_table.users_vec.size() * sizeof(User));
        users_table.save(parser, users_table_addr); //добавляем админа

        int root_acl_addr = allocator.alloc(sizeof(FolderACList));
        int root_ft_addr = allocator.alloc(sizeof(FileTable));
        int root_addr = allocator.alloc(sizeof(FolderDescriptor)); //адрес корневого каталога

        FolderACList root_acl = FolderACList();
        root_acl.grant(0, true, true, true);
        root_acl.save(parser, root_acl_addr);

        FileTable root_ft = FileTable();
        root_ft.save(parser, root_ft_addr);

        FolderDescriptor root_folder = FolderDescriptor();
        root_folder.init(root_acl_addr, 1, root_ft_addr, 0);
        root_folder.save(parser, root_addr);

        superblock.init(root_addr, allocator.next_free, users_table_addr, users_table.users_vec.size());
        superblock.save(parser, 0);

        curr_folder = root_addr;
        curr_uid = -1;
        memcpy(curr_folder_name, "ROOT", 5); //5 т.к в конце надо поставить \0
    }

    void login(char name[32]){
        UsersTable us_table(superblock);
        us_table.load(parser, superblock.users_table);
        int id = us_table.search_uid_by_name(name);
        if(id != -1) curr_uid = id;
    }

    void register_user(char name[32]){
        UsersTable us_table(superblock);
        us_table.load(parser, superblock.users_table);
        if(us_table.search_uid_by_name(name) == -1){
            us_table.add(name, encryption.generate_key().data());
            int new_addr = allocator.alloc(us_table.users_vec.size() * sizeof(User));
            us_table.save(parser, new_addr);
            superblock.users_table = new_addr;
            superblock.users_table_size = us_table.users_vec.size();
            superblock.save(parser, 0);
        }
        else{
            cout << "В системе уже есть пользователь с таким же именем" << endl;
        }
    }

    void cd(string path, bool to_root = false){
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
            cout << "Попытались открыть файл вместо каталога в cd"<< endl;
        }
    }

    void ls(){
        FolderDescriptor fd;
        fd.load(parser, curr_folder);
        fd.show(parser);
    }

    void create(char file_name[32], bool can_read, bool can_write){

        FileTable ft;
        FolderDescriptor folder;
        folder.load(parser, curr_folder);
        ft.load(parser, folder.file_table_addr);
        if(ft.search(file_name) == -1){

            int acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FileAccessEntry));
            int st_addr = allocator.alloc(sizeof(int) + 1 * sizeof(StreamDescriptor));
            int main_addr = allocator.alloc(0);
            int addr = allocator.alloc(sizeof(CommonFileDescriptor));

            FileACList acl = FileACList();
            acl.grant(curr_uid, can_read, can_write);
            acl.save(parser, acl_addr);

            StreamTable table = StreamTable();
            table.add("MAIN", main_addr, 0, 0);
            table.save(parser, st_addr);

            CommonFileDescriptor file;
            file.init(curr_uid, st_addr, 1, acl_addr, 1);
            file.save(parser, addr);

        
            ft.add(file_name, addr);
            int new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));
            folder.file_table_addr = new_ft_addr;
            folder.file_table_size = ft.table_node_vec.size();
            ft.save(parser, new_ft_addr);
            folder.save(parser, curr_folder);
            return;
        }
        cout << "при создании обычного файла выяснилось, что файл с таким именем уже есть" << endl;
    }

    void create_encrypted(char file_name[32], bool can_read, bool can_write){

        FileTable ft;
        FolderDescriptor folder;
        folder.load(parser, curr_folder);
        ft.load(parser, folder.file_table_addr);
        if(ft.search(file_name) == -1){

            int acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FileAccessEntry));
            int st_addr = allocator.alloc(sizeof(int) + 2 * sizeof(StreamDescriptor));
            int main_addr = allocator.alloc(0);
            int symkey_addr = allocator.alloc(0);
            int addr = allocator.alloc(sizeof(EncryptedFileDescriptor));
            int auth_usrs_addr = allocator.alloc(sizeof(int) + 1 * sizeof(int));

            AuthorizedUsers usrs = AuthorizedUsers(); //новый объект, load сделать нельзя, так как прочитается мусор
            usrs.add(curr_uid);
            usrs.save(parser, auth_usrs_addr);

            FileACList acl = FileACList();
            acl.grant(curr_uid, can_read, can_write);
            acl.save(parser, acl_addr);

            StreamTable table = StreamTable();
            table.add("MAIN", main_addr, 0, 0);
            table.add("SYMKEY", symkey_addr, 0, 0);
            table.save(parser, st_addr);

            EncryptedFileDescriptor file;
            file.init(curr_uid, st_addr, 2, acl_addr, 1, auth_usrs_addr);
            file.save(parser, addr);
            
            ft.add(file_name, addr);
            int new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));
            folder.file_table_addr = new_ft_addr;
            folder.file_table_size = ft.table_node_vec.size();
            ft.save(parser, new_ft_addr);
            folder.save(parser, curr_folder);
            return;
        }
        
        cout << "при создании зашифрованного файла выяснилось, что файл с таким именем уже есть" << endl;
    }

    void mkdir(char folder_name[32], bool can_read, bool can_write, bool can_traverse){
        FileTable ft;
        FolderDescriptor initial_folder;
        initial_folder.load(parser, curr_folder);
        ft.load(parser, initial_folder.file_table_addr);
        if(ft.search(folder_name) == -1){

            int acl_addr = allocator.alloc(sizeof(int) + 1 * sizeof(FolderAccessEntry));
            int ft_addr = allocator.alloc(sizeof(int) + 0 * sizeof(TableNode));
            int addr = allocator.alloc(sizeof(FolderDescriptor));

            FolderACList acl = FolderACList();
            acl.grant(curr_uid, can_read, can_write, can_traverse);
            acl.save(parser, acl_addr);

            FolderDescriptor folder;
            folder.init(acl_addr, 1, ft_addr, 0);
            folder.save(parser, addr);

            ft.add(folder_name, addr);
            int new_ft_addr = allocator.alloc(sizeof(int) + ft.table_node_vec.size() * sizeof(TableNode));
            folder.file_table_addr = new_ft_addr;
            folder.file_table_size = ft.table_node_vec.size();
            ft.save(parser, new_ft_addr);
            folder.save(parser, curr_folder);
            return;

        }
        cout << "при создании подкаталога выяснилось, что каталог с таким именем уже есть" << endl;
    }

    optional <vector <char>> read(char name[32]){
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
                memcpy(usr_key, ut.users_vec[ut.search(curr_uid)].key, 32);
                
                return enfd.read(parser, encryption, curr_uid, vector(usr_key, usr_key + 32 * sizeof(uint8_t)));
            }

            else{
                cout << "попытались открыть каталог вместо файла" << endl;
            }
        }
        return nullopt;
    }

    void write(char name[32], vector <char> buffer){
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
                    all_user_keys.push_back(vector(ut.users_vec[usr_index].key, ut.users_vec[usr_index].key + 32 * sizeof(uint8_t)));
                }
                enfd.write(parser, encryption, curr_uid, buffer, allocator, all_user_keys);
            }
            else{
                cout << "попытались записать в каталог вместо файла" << endl;
                return;
            }
            return;
        }
        cout << "файл не найден" << endl;
    }

    void get_info(char name[32]){
        FolderDescriptor fd;
        fd.load(parser, curr_folder);
        FileTable ft;
        ft.load(parser, fd.file_table_addr);
        int index = ft.search(name);
        if(index != -1){
            int type;
            parser.read(ft.table_node_vec[index].addr, reinterpret_cast<char*>(&type), sizeof(int));
            if(type == 1){
                CommonFileDescriptor fd;
                fd.load(parser, ft.table_node_vec[index].addr);
                fd.get_info(parser);
            }
            else if(type == 2){
                EncryptedFileDescriptor enfd;
                enfd.load(parser, ft.table_node_vec[index].addr);
                enfd.get_info(parser);
            }
            else{
                cout << "у каталога нет метода get_info()" << endl;
            }
            return;
        }
        cout << "такого файла не найдено" << endl;
    }

    void remove(char name[32]){
        FolderDescriptor fd;
        fd.load(parser, curr_folder);
        fd.remove(parser, name);
    }

    void rename(char name[32], char new_name[32]){
        FolderDescriptor fd;
        fd.load(parser, curr_folder);
        fd.rename(parser, name, new_name);
    }

    void move(char name[32], int dest_addr){
        FolderDescriptor fd;
        fd.load(parser, curr_folder);
        fd.move(parser, name, dest_addr);
    }

    void copy(char name[32], int dest_addr){
        FolderDescriptor fd;
        fd.load(parser, curr_folder);
        fd.copy(parser, allocator, name, dest_addr);
    }

    void set_acl(int uid, char name[32], bool can_read, bool can_write, bool can_traverse = false){

        UsersTable users_table = UsersTable(superblock);
        users_table.load(parser, superblock.users_table);
        if(users_table.search(uid) == -1){
            cout << "Для set_acl пользователя с таким uid нет в системе" << endl;
            return;
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
                cfd.set_acl(parser, uid, can_read, can_write);
            }
            if(type == 2){
                EncryptedFileDescriptor enfd;
                enfd.load(parser, ft.table_node_vec[index].addr);

                UsersTable ut = UsersTable(superblock);
                ut.load(parser, superblock.users_table);
                vector <uint8_t> owner_key = vector(ut.users_vec[ut.search(curr_uid)].key, ut.users_vec[ut.search(curr_uid)].key + 32);
                vector <uint8_t> user_key = vector(ut.users_vec[ut.search(uid)].key, ut.users_vec[ut.search(uid)].key + 32);

                enfd.set_acl(parser, encryption, uid, can_read, can_write, owner_key, user_key);
            }

            if(type == 3){
                FolderDescriptor fod;
                fod.load(parser, ft.table_node_vec[index].addr);
                fod.set_acl(parser, uid, can_read, can_write, can_traverse);
            }
        }
    }
};