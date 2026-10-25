#include "FolderDescriptor.h"
#include "FileTable.h"
#include "FolderACList.h"
#include "CommonFileDescriptor.h"
#include "EncryptedFileDescriptor.h"
#include "StreamTable.h"
#include "FileACList.h"
#include "AuthorizedUsers.h"
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

void FolderDescriptor::init(int acl_addr, int acl_size, int file_table_addr, int file_table_size){
    // this->folder_addr = folder_addr;
    this->acl_addr = acl_addr;
    this->acl_size = acl_size;
    this->file_table_addr = file_table_addr;
    this->file_table_size = file_table_size;
    type = 3;
    size = 0;
}

bool FolderDescriptor::can_read(Parser& parser, int uid){
    FolderACList acl;
    acl.load(parser, acl_addr);

    int index = acl.find_index(uid);
    if(index != -1){
        return acl.ACList[index].can_read;
    }
    return false;
}

bool FolderDescriptor::can_write(Parser& parser, int uid){
    FolderACList acl;
    acl.load(parser, acl_addr);

    int index = acl.find_index(uid);
    if(index != -1){
        return acl.ACList[index].can_write;
    }
    return false;
}

bool FolderDescriptor::can_traverse(Parser& parser, int uid){
    FolderACList acl;
    acl.load(parser, acl_addr);

    int index = acl.find_index(uid);
    if(index != -1){
        return acl.ACList[index].can_traverse;
    }
    return false;
}

void FolderDescriptor::show(Parser& parser){
    FileTable file_table;
    file_table.load(parser, file_table_addr);

    cout << "Текущий размер: " << file_table.table_node_vec.size() << endl;

    for(auto el:file_table.table_node_vec){
        cout << el.name << ' ' << el.addr << endl;
    }
}

void FolderDescriptor::copy(Parser& parser, Allocator& allocator, char name[32], int dest_addr){
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

            int new_acl_addr = -1, new_st_addr = -1, new_file_addr = -1, new_dest_ft_addr = -1, old_dest_ft_addr = -1;
            vector<int> new_stream_addrs; //блоки данных скопированных потоков

            try{
                new_acl_addr = allocator.alloc(sizeof(int) + regular_file_acl.ACList.size() * sizeof(FileAccessEntry));
                new_st_addr = allocator.alloc(sizeof(int) + regular_file_st.stream_desc_vec.size() * sizeof(StreamDescriptor));
                new_file_addr = allocator.alloc(sizeof(CommonFileDescriptor));

                regular_file_acl.save(parser, new_acl_addr);
                for(auto& el:regular_file_st.stream_desc_vec){
                    int new_addr = allocator.alloc(el.capacity);
                    new_stream_addrs.push_back(new_addr);
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
                dest_ft.add(name, new_file_addr); //бросает, если имя в dest уже занято

                new_dest_ft_addr = allocator.alloc(sizeof(int) +
                dest_ft.table_node_vec.size() * sizeof(TableNode));

                old_dest_ft_addr = dest_folder.file_table_addr;
                dest_folder.file_table_addr = new_dest_ft_addr;
                dest_folder.file_table_size = dest_ft.table_node_vec.size();

                //сначала содержимое новой таблицы, потом дескриптор, который на нее ссылается
                dest_ft.save(parser, dest_folder.file_table_addr);
                dest_folder.save(parser, dest_addr);
            }
            catch(...){ //копия еще не привязана к dest, освобождаем все, что успели выделить
                if(new_acl_addr != -1) allocator.free(new_acl_addr);
                if(new_st_addr != -1) allocator.free(new_st_addr);
                if(new_file_addr != -1) allocator.free(new_file_addr);
                for(int a : new_stream_addrs) allocator.free(a);
                if(new_dest_ft_addr != -1) allocator.free(new_dest_ft_addr);
                throw;
            }
            if(old_dest_ft_addr != -1) allocator.free(old_dest_ft_addr);
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

            int new_acl_addr = -1, new_st_addr = -1, new_file_addr = -1, new_authorized_usr_address = -1;
            int new_dest_ft_addr = -1, old_dest_ft_addr = -1;
            vector<int> new_stream_addrs; //блоки данных скопированных потоков (MAIN и SYMKEY)

            try{
                new_acl_addr = allocator.alloc(sizeof(int) + encrypted_file_acl.ACList.size() * sizeof(FileAccessEntry));
                new_st_addr = allocator.alloc(sizeof(int) + encrypted_file_st.stream_desc_vec.size() * sizeof(StreamDescriptor));
                new_file_addr = allocator.alloc(sizeof(EncryptedFileDescriptor));
                new_authorized_usr_address = allocator.alloc(sizeof(int) + auth_users.AuthorizedUsers.size() * sizeof(int));

                encrypted_file_acl.save(parser, new_acl_addr);
                auth_users.save(parser, new_authorized_usr_address);
                encrypted_file.authorized_usr_addr = new_authorized_usr_address;


                for(auto& el:encrypted_file_st.stream_desc_vec){
                    int new_addr = allocator.alloc(el.capacity);
                    new_stream_addrs.push_back(new_addr);
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
                dest_ft.add(name, new_file_addr); //бросает, если имя в dest уже занято

                new_dest_ft_addr = allocator.alloc(sizeof(int) +
                dest_ft.table_node_vec.size() * sizeof(TableNode));

                old_dest_ft_addr = dest_folder.file_table_addr;
                dest_folder.file_table_addr = new_dest_ft_addr;
                dest_folder.file_table_size = dest_ft.table_node_vec.size();

                //сначала содержимое новой таблицы, потом дескриптор, который на нее ссылается
                dest_ft.save(parser, dest_folder.file_table_addr);
                dest_folder.save(parser, dest_addr);
            }
            catch(...){ //копия еще не привязана к dest, освобождаем все, что успели выделить
                if(new_acl_addr != -1) allocator.free(new_acl_addr);
                if(new_st_addr != -1) allocator.free(new_st_addr);
                if(new_file_addr != -1) allocator.free(new_file_addr);
                if(new_authorized_usr_address != -1) allocator.free(new_authorized_usr_address);
                for(int a : new_stream_addrs) allocator.free(a);
                if(new_dest_ft_addr != -1) allocator.free(new_dest_ft_addr);
                throw;
            }
            if(old_dest_ft_addr != -1) allocator.free(old_dest_ft_addr);
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
            FolderACList acl;
            acl.load(parser, folder_descriptor.acl_addr);
            FileTable file_table;
            file_table.load(parser, folder_descriptor.file_table_addr);

            int new_folder_addr = -1, new_acl_addr = -1, new_ft_addr = -1;
            int new_dest_ft_addr = -1, old_dest_ft_addr = -1;
            bool new_folder_saved = false; //после сохранения дескриптора копию удаляем целиком через free_object

            try{
                new_folder_addr = allocator.alloc(sizeof(FolderDescriptor));
                new_acl_addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FolderAccessEntry));
                new_ft_addr = allocator.alloc(sizeof(int) + file_table.table_node_vec.size() * sizeof(TableNode));

                FolderDescriptor new_folder_descriptor = folder_descriptor;

                new_folder_descriptor.acl_addr = new_acl_addr;
                new_folder_descriptor.file_table_addr = new_ft_addr;
                acl.save(parser, new_acl_addr);
                FileTable new_ft; //пустая таблица, создается без записей
                new_ft.save(parser, new_ft_addr);
                new_folder_descriptor.save(parser, new_folder_addr);
                new_folder_saved = true;

                //каждый copy потомка перевыделяет таблицу новой папки, поэтому new_ft_addr после первого
                //потомка уже не действителен; актуальный адрес таблицы лежит в дескрипторе новой папки
                for(auto& tn:file_table.table_node_vec){
                    folder_descriptor.copy(parser, allocator, tn.name, new_folder_addr);
                }


                dest_ft.add(name, new_folder_addr);

                new_dest_ft_addr = allocator.alloc(sizeof(int) +
                dest_ft.table_node_vec.size() * sizeof(TableNode));

                old_dest_ft_addr = dest_folder.file_table_addr;
                dest_folder.file_table_addr = new_dest_ft_addr;
                dest_folder.file_table_size = dest_ft.table_node_vec.size();

                //сначала содержимое новой таблицы, потом дескриптор, который на нее ссылается
                dest_ft.save(parser, dest_folder.file_table_addr);
                dest_folder.save(parser, dest_addr);
            }
            catch(...){ //новая папка еще не привязана к dest
                if(new_folder_saved){
                    free_object(parser, allocator, new_folder_addr); //папка, ее ACL, таблица и все скопированные потомки
                }
                else{
                    if(new_folder_addr != -1) allocator.free(new_folder_addr);
                    if(new_acl_addr != -1) allocator.free(new_acl_addr);
                    if(new_ft_addr != -1) allocator.free(new_ft_addr);
                }
                if(new_dest_ft_addr != -1) allocator.free(new_dest_ft_addr);
                throw;
            }
            if(old_dest_ft_addr != -1) allocator.free(old_dest_ft_addr);
        }
    }
    else{
        throw runtime_error(string("объект ") + name + " не найден в текущем каталоге");
    }
}

void FolderDescriptor::move(Allocator& allocator, Parser& parser, char name[32], int dest_addr){
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
        int old_dest_ft_addr = dest_folder.file_table_addr;
        dest_folder.file_table_addr = new_dest_ft_addr;
        dest_folder.file_table_size = dest_ft.table_node_vec.size();

        dest_folder.save(parser, dest_addr);
        dest_ft.save(parser, dest_folder.file_table_addr);
        file_table.save(parser, file_table_addr);
        allocator.free(old_dest_ft_addr);
    }
    else{
        throw runtime_error("название " + string(name) + " не найдено в каталоге при операции move");
    }
}

void FolderDescriptor::rename(Parser& parser, char name[32], char new_name[32]){
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

void FolderDescriptor::add(Parser& parser, char name[32], int addr){
    FileTable ft;
    ft.load(parser, file_table_addr);
    ft.add(name, addr);
    ft.save(parser, file_table_addr);
}

void FolderDescriptor::remove(Parser& parser, Allocator& allocator, char name[32]){
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

    int obj_addr = ft.table_node_vec[index].addr; //после ft.remove индекс уже не действителен
    ft.remove(name);
    ft.save(parser, file_table_addr);

    free_object(parser, allocator, obj_addr); //объект больше не виден из каталога, освобождаем все его блоки
}

void FolderDescriptor::free_object(Parser& parser, Allocator& allocator, int addr){
    //сначала загружаем структуры в память, потом освобождаем, т.к. free зануляет блок
    int type;
    parser.read(addr, reinterpret_cast<char*>(&type), sizeof(int));
    if(type == 1){
        CommonFileDescriptor cfd;
        cfd.load(parser, addr);
        StreamTable st;
        st.load(parser, cfd.stream_table_addr);
        for(auto& s : st.stream_desc_vec) allocator.free(s.addr);
        allocator.free(cfd.stream_table_addr);
        allocator.free(cfd.acl_addr);
        allocator.free(addr);
    }
    else if(type == 2){
        EncryptedFileDescriptor enfd;
        enfd.load(parser, addr);
        StreamTable st;
        st.load(parser, enfd.stream_table_addr);
        for(auto& s : st.stream_desc_vec) allocator.free(s.addr); //MAIN и SYMKEY
        allocator.free(enfd.stream_table_addr);
        allocator.free(enfd.acl_addr);
        allocator.free(enfd.authorized_usr_addr);
        allocator.free(addr);
    }
    else if(type == 3){ //каталог освобождается вместе со всем содержимым
        FolderDescriptor fd;
        fd.load(parser, addr);
        FileTable ft;
        ft.load(parser, fd.file_table_addr);
        for(auto& tn : ft.table_node_vec) free_object(parser, allocator, tn.addr);
        allocator.free(fd.file_table_addr);
        allocator.free(fd.acl_addr);
        allocator.free(addr);
    }
}

bool FolderDescriptor::is_inside(Parser& parser, int root_addr, int folder_addr){
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

void FolderDescriptor::save(Parser& parser, int addr){
    parser.write(addr, reinterpret_cast<char*>(this), sizeof(FolderDescriptor));
}

void FolderDescriptor::load(Parser& parser, int addr){
    parser.read(addr, reinterpret_cast<char*>(this), sizeof(FolderDescriptor));
}
