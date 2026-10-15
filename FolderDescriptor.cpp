#pragma once
#include <iostream>
#include <vector>
#include <ctime>
#include "Parser.cpp"
#include "Descriptor.cpp"
#include "FileTable.cpp"
#include "FolderACList.cpp"
#include "CommonFileDescriptor.cpp"
#include "StreamTable.cpp"
#include "FileACList.cpp"
#include "Allocator.cpp"
#include "FileAccessEntry.cpp"
#include "FolderAccessEntry.cpp"
#include "StreamDescriptor.cpp"
#include "AuthorizedUsers.cpp"
#include "EncryptedFileDescriptor.cpp"

#pragma pack(push, 1)
struct FolderDescriptor : Descriptor{
    int acl_addr, acl_size, file_table_addr, file_table_size;

    void init(int acl_addr, int acl_size, int file_table_addr, int file_table_size){
        // this->folder_addr = folder_addr;
        this->acl_addr = acl_addr;
        this->acl_size = acl_size;
        this->file_table_addr = file_table_addr;
        this->file_table_size = file_table_size;
        type = 3;
        size = 0;
    }

    bool can_read(Parser& parser, int uid){
        FolderACList acl;
        acl.load(parser, acl_addr);

        int index = acl.find_index(uid);
        if(index != -1){
            return acl.ACList[index].can_read;
        }
        return false;
    }

    bool can_write(Parser& parser, int uid){
        FolderACList acl;
        acl.load(parser, acl_addr);

        int index = acl.find_index(uid);
        if(index != -1){
            return acl.ACList[index].can_write;
        }
        return false;
    }

    bool can_traverse(Parser& parser, int uid){
        FolderACList acl;
        acl.load(parser, acl_addr);

        int index = acl.find_index(uid);
        if(index != -1){
            return acl.ACList[index].can_traverse;
        }
        return false;
    }

    void show(Parser& parser){
        FileTable file_table;
        file_table.load(parser, file_table_addr);

        cout << "Текущий размер: " << file_table.table_node_vec.size() << endl;

        for(auto el:file_table.table_node_vec){
            cout << el.name << ' ' << el.addr << endl;
        }
    }

    void copy(Parser& parser, Allocator& allocator, char name[32], int dest_addr){
        FileTable file_table;
        file_table.load(parser, file_table_addr);
        int index = file_table.search(name);
        int file_addr;
        if(index != -1){
            file_addr = file_table.table_node_vec[index].addr;
            int type;
            parser.read(file_addr, reinterpret_cast<char*>(&type), sizeof(type));
            if(type == 1){
                CommonFileDescriptor regular_file;
                regular_file.load(parser, file_addr);
                regular_file.created_d = time(nullptr);
                FileACList regular_file_acl;
                regular_file_acl.load(parser, regular_file.acl_addr);
                StreamTable regular_file_st;
                regular_file_st.load(parser, regular_file.stream_table_addr);

                int new_acl_addr = allocator.alloc(sizeof(int) + regular_file_acl.ACList.size() * sizeof(FileAccessEntry));
                int new_st_addr = allocator.alloc(sizeof(int) + regular_file_st.stream_desc_vec.size() * sizeof(StreamDescriptor));
                int new_file_addr = allocator.alloc(sizeof(CommonFileDescriptor));
                
                regular_file_acl.save(parser, new_acl_addr);
                for(auto& el:regular_file_st.stream_desc_vec){
                    int new_addr = allocator.alloc(el.capacity);
                    vector<char>buffer(el.size);
                    parser.read(el.addr, buffer.data(), el.size);
                    parser.write(new_addr, buffer.data(), el.size);
                    el.addr = new_addr;
                }

                regular_file_st.save(parser, new_st_addr);
                regular_file.acl_addr = new_acl_addr;
                regular_file.stream_table_addr = new_st_addr;
                regular_file.save(parser, new_file_addr);

                FolderDescriptor dest_folder;
                dest_folder.load(parser, dest_addr);
                FileTable dest_ft;
                dest_ft.load(parser, dest_folder.file_table_addr);
                dest_ft.add(name, new_file_addr);

                int new_dest_ft_addr = allocator.alloc(sizeof(int) + 
                dest_ft.table_node_vec.size() * sizeof(TableNode));

                if(new_dest_ft_addr == -1){
                    cout << "образ закончился в методе copy класса FolderDescriptor" << endl;
                    return;
                }
                dest_folder.file_table_addr = new_dest_ft_addr;
                dest_folder.file_table_size = dest_ft.table_node_vec.size();

                dest_folder.save(parser, dest_addr);
                dest_ft.save(parser, dest_folder.file_table_addr);
            }
            if(type == 2){
                EncryptedFileDescriptor encrypted_file;
                encrypted_file.load(parser, file_addr);
                encrypted_file.created_d = time(nullptr);
                FileACList encrypted_file_acl;
                encrypted_file_acl.load(parser, encrypted_file.acl_addr);
                StreamTable encrypted_file_st;
                encrypted_file_st.load(parser, encrypted_file.stream_table_addr);
                AuthorizedUsers auth_users;
                auth_users.load(parser, encrypted_file.authorized_usr_addr);

                int new_acl_addr = allocator.alloc(sizeof(int) + encrypted_file_acl.ACList.size() * sizeof(FileAccessEntry));
                int new_st_addr = allocator.alloc(sizeof(int) + encrypted_file_st.stream_desc_vec.size() * sizeof(StreamDescriptor));
                int new_file_addr = allocator.alloc(sizeof(EncryptedFileDescriptor));
                int new_authorized_usr_address = allocator.alloc(sizeof(int) + auth_users.AuthorizedUsers.size() * sizeof(int));
                
                encrypted_file_acl.save(parser, new_acl_addr);
                auth_users.save(parser, new_authorized_usr_address);
                encrypted_file.authorized_usr_addr = new_authorized_usr_address;


                for(auto& el:encrypted_file_st.stream_desc_vec){
                    int new_addr = allocator.alloc(el.capacity);
                    vector<char>buffer(el.size);
                    parser.read(el.addr, buffer.data(), el.size);
                    parser.write(new_addr, buffer.data(), el.size);
                    el.addr = new_addr;
                }

                encrypted_file_st.save(parser, new_st_addr);
                encrypted_file.acl_addr = new_acl_addr;
                encrypted_file.stream_table_addr = new_st_addr;
                encrypted_file.save(parser, new_file_addr);

                FolderDescriptor dest_folder;
                dest_folder.load(parser, dest_addr);
                FileTable dest_ft;
                dest_ft.load(parser, dest_folder.file_table_addr);
                dest_ft.add(name, new_file_addr);

                int new_dest_ft_addr = allocator.alloc(sizeof(int) + 
                dest_ft.table_node_vec.size() * sizeof(TableNode));

                if(new_dest_ft_addr == -1){
                    cout << "образ закончился в методе copy класса FolderDescriptor" << endl;
                    return;
                }
                dest_folder.file_table_addr = new_dest_ft_addr;
                dest_folder.file_table_size = dest_ft.table_node_vec.size();

                dest_folder.save(parser, dest_addr);
                dest_ft.save(parser, dest_folder.file_table_addr);
            }
            if(type == 3){

                int type;
                parser.read(dest_addr, reinterpret_cast<char*>(&type), sizeof(int));
                if(type != 3) throw runtime_error("dest_addr " + to_string(dest_addr) + 
                " в FolderDescriptor copy для каталога указывает не на каталог");

                FolderDescriptor dest_folder;
                dest_folder.load(parser, dest_addr);
                FileTable dest_ft;
                dest_ft.load(parser, dest_folder.file_table_addr);

                if(dest_ft.search(name) != -1) throw 
                runtime_error("В destination каталоге уже есть объект с таким именем");

                if(is_inside(parser, file_addr, dest_addr)) throw 
                runtime_error("нельзя скопировать файл в один из его внутренних файлов");

                FolderDescriptor folder_descriptor;
                folder_descriptor.load(parser, file_addr);
                int new_folder_addr = allocator.alloc(sizeof(FolderDescriptor));

                FolderACList acl;
                acl.load(parser, folder_descriptor.acl_addr);
                int new_acl_addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FolderAccessEntry));

                FileTable file_table;
                file_table.load(parser, folder_descriptor.file_table_addr);
                int new_ft_addr = allocator.alloc(sizeof(int) + file_table.table_node_vec.size() * sizeof(TableNode));

                FolderDescriptor new_folder_descriptor = folder_descriptor;

                new_folder_descriptor.acl_addr = new_acl_addr;
                new_folder_descriptor.file_table_addr = new_ft_addr;
                acl.save(parser, new_acl_addr);
                FileTable new_ft; //пустая таблица, создается без записей
                new_ft.save(parser, new_ft_addr);
                new_folder_descriptor.save(parser, new_folder_addr);

                for(auto& tn:file_table.table_node_vec){
                    folder_descriptor.copy(parser, allocator, tn.name, new_folder_addr);
                }

                
                dest_ft.add(name, new_folder_addr);

                int new_dest_ft_addr = allocator.alloc(sizeof(int) + 
                dest_ft.table_node_vec.size() * sizeof(TableNode));

                if(new_dest_ft_addr == -1){
                    cout << "образ закончился в методе copy класса FolderDescriptor" << endl;
                    return;
                }
                dest_folder.file_table_addr = new_dest_ft_addr;
                dest_folder.file_table_size = dest_ft.table_node_vec.size();

                dest_folder.save(parser, dest_addr);
                dest_ft.save(parser, dest_folder.file_table_addr);
            }
        }
        else{
            throw runtime_error(string("объект ") + name + " не найден в текущем каталоге");
        }
    }

    void move(Allocator& allocator, Parser& parser, char name[32], int dest_addr){
        int dest_type;
        parser.read(dest_addr, reinterpret_cast<char*>(&dest_type), sizeof(int));
        // if(dest_type != 3) throw runtime_error("Выбранный файл не является каталогом");

        FolderDescriptor dest_folder;
        dest_folder.load(parser, dest_addr);
        if(file_table_addr == dest_folder.file_table_addr){
            throw runtime_error("перенос происходит в тот же файл, операция отменена");
        }

        FileTable file_table;
        file_table.load(parser, file_table_addr);
        int index = file_table.search(name);
        if(index != -1){
            if(is_inside(parser, file_table.table_node_vec[index].addr, dest_addr)) throw runtime_error("Нельзя перенести файл во внутренний файл");
            FileTable dest_ft;
            dest_ft.load(parser, dest_folder.file_table_addr);

            dest_ft.add(name, file_table.table_node_vec[index].addr);
            file_table.remove(name);

            int new_dest_ft_addr = allocator.alloc(sizeof(int) + 
            dest_ft.table_node_vec.size() * sizeof(TableNode));

            if(new_dest_ft_addr == -1){
                cout << "образ закончился в методе move класса FolderDescriptor" << endl;
                return;
            }
            dest_folder.file_table_addr = new_dest_ft_addr;
            dest_folder.file_table_size = dest_ft.table_node_vec.size();

            dest_folder.save(parser, dest_addr);
            dest_ft.save(parser, dest_folder.file_table_addr);
            file_table.save(parser, file_table_addr);
        }
    }

    void rename(Parser& parser, char name[32], char new_name[32]){
        FileTable ftl;
        ftl.load(parser, file_table_addr);
        int index = ftl.search(name);
        if(index != -1){
            int addr = ftl.table_node_vec[index].addr;
            ftl.remove(name);
            ftl.add(new_name, addr);
            ftl.save(parser, file_table_addr);
        }
    }

    void add(Parser& parser, char name[32], int addr){
        FileTable ft;
        ft.load(parser, file_table_addr);
        ft.add(name, addr);
        ft.save(parser, file_table_addr);
    }

    void remove(Parser& parser, char name[32]){
        FileTable ft;
        ft.load(parser, file_table_addr);
        int index = ft.search(name);
        if(index == -1) throw runtime_error(string("объект ") + name + " отсутствует в данном каталоге");
        int type;
        parser.read(ft.table_node_vec[index].addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type == 3){
            FolderDescriptor fd;
            fd.load(parser, ft.table_node_vec[index].addr);
            FileTable file_table;
            file_table.load(parser, fd.file_table_addr);
            if(file_table.table_node_vec.size() != 0) throw 
            runtime_error("нельзя удалить каталог, содержащий в себе элементы, удалите все элементы каталога");
        }
        
        ft.remove(name);
        ft.save(parser, file_table_addr);
    }

    bool is_inside(Parser& parser, int root_addr, int folder_addr){
        if(root_addr == folder_addr) return true;

        int type;
        parser.read(root_addr, reinterpret_cast<char*>(&type), sizeof(int));
        if(type != 3) return false;

        FolderDescriptor fd;
        fd.load(parser, root_addr);

        FileTable ft;
        ft.load(parser, fd.file_table_addr);

        for(auto el:ft.table_node_vec){
            if(is_inside(parser, el.addr, folder_addr)) return true;
        }
        return false;
    }

    void save(Parser& parser, int addr){
        parser.write(addr, reinterpret_cast<char*>(this), sizeof(FolderDescriptor));
    }
    void load(Parser& parser, int addr){
        parser.read(addr, reinterpret_cast<char*>(this), sizeof(FolderDescriptor));
    }
};
#pragma pack(pop)