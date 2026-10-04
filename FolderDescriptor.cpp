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

    void set_acl(Parser& parser, int uid, bool can_read, bool can_write, bool can_traverse){
        FolderACList acl;
        acl.load(parser, acl_addr);
        acl.grant(uid, can_read, can_write, can_traverse);
        acl.save(parser,acl_addr);
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
        cout << "Текущий размер: " << size << endl;
        FileTable file_table;
        file_table.load(parser, file_table_addr);

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

                int new_acl_addr = allocator.alloc(regular_file_acl.ACList.size() * sizeof(FileAccessEntry));
                int new_st_addr = allocator.alloc(regular_file_st.stream_desc_vec.size() * sizeof(StreamDescriptor));
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
                dest_ft.save(parser, dest_folder.file_table_addr);
            }
            if(type == 3){
                FolderDescriptor folder_descriptor;
                folder_descriptor.load(parser, file_addr);
                int new_folder_addr = allocator.alloc(sizeof(FolderDescriptor));

                FolderACList acl;
                acl.load(parser, folder_descriptor.acl_addr);
                int new_acl_addr = allocator.alloc(acl.ACList.size() * sizeof(FolderAccessEntry));

                FileTable file_table;
                file_table.load(parser, folder_descriptor.file_table_addr);
                int new_ft_addr = allocator.alloc(file_table.table_node_vec.size() * sizeof(TableNode));

                for(auto& tn:file_table.table_node_vec){
                    folder_descriptor.copy(parser, allocator, tn.name, new_folder_addr);
                }
                acl.save(parser, new_acl_addr);
                folder_descriptor.acl_addr = new_acl_addr;
                folder_descriptor.file_table_addr = new_ft_addr;
                folder_descriptor.save(parser, new_folder_addr);

                FolderDescriptor dest_folder;
                dest_folder.load(parser, dest_addr);
                FileTable dest_ft;
                dest_ft.load(parser, dest_folder.file_table_addr);
                dest_ft.add(name, new_folder_addr);
                dest_ft.save(parser, dest_folder.file_table_addr);
            }
        }
    }

    void move(Parser& parser, char name[32], int dest_addr){
        FolderDescriptor dest_folder;
        dest_folder.load(parser, dest_addr);
        if(file_table_addr == dest_folder.file_table_addr){
            cout << "перенос происходит в тот же файл, операция отменена" << endl;
            return;
        }

        FileTable file_table;
        file_table.load(parser, file_table_addr);
        int index = file_table.search(name);
        if(index != -1){
            FileTable dest_ft;
            dest_ft.load(parser, dest_folder.file_table_addr);

            dest_ft.add(name, file_table.table_node_vec[index].addr);
            file_table.remove(name);

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
        ft.remove(name);
        ft.save(parser, file_table_addr);
    }

    void save(Parser& parser, int addr){
        parser.write(addr, reinterpret_cast<char*>(this), sizeof(FolderDescriptor));
    }
    void load(Parser& parser, int addr){
        parser.read(addr, reinterpret_cast<char*>(this), sizeof(FolderDescriptor));
    }
};
#pragma pack(pop)