#pragma once
#include <iostream>
#include <ctime>
#include <vector>
#include <optional> //дает тип optional<T>, означающий значение типа T или ничего
#include <cstring> //позволяет работать с С-строками и сырой памятью, ф-ии унаследованы от С, 
//string - это класс С++, а cstring - набор ф-ий
#include "CommonFileDescriptor.cpp"
#include "StreamTable.cpp"
#include "FileACList.cpp"
#include "Descriptor.cpp"
#include "Parser.cpp"
#include "Allocator.cpp"
#include "AuthorizedUsers.cpp"
#include "User.cpp"
#include "Encryption.cpp"

using namespace std;

#pragma pack(push, 1)
struct EncryptedFileDescriptor : CommonFileDescriptor{
    int authorized_usr_addr;


    void init(int owner_id, int stream_table_addr, int stream_table_size, int acl_addr, int acl_size, int authorized_usr_addr){
        created_d = time(nullptr);
        modified_d = created_d;
        this->owner_id = owner_id;
        this->stream_table_addr = stream_table_addr;
        this->stream_table_size = stream_table_size;
        this->acl_addr = acl_addr;  
        this->acl_size = acl_size;
        this->size = 0;
        this->type = 2;
        this->authorized_usr_addr = authorized_usr_addr;
    }

    void get_info(Parser& parser){

        CommonFileDescriptor::get_info(parser);

        cout << "Это зашифрованный файл" << endl;
        
        AuthorizedUsers au;
        au.load(parser, authorized_usr_addr);
        cout << "Авторизованных пользователей: " << au.AuthorizedUsers.size() << endl;
    }

        // actor_* - тот, кто выдаёт права (у него уже есть доступ к ключу файла)
    // после вызова описатель нужно сохранить: acl_addr и authorized_usr_addr могут измениться
    bool set_acl(Parser& parser, Allocator& allocator, Encryption& encryption, int actor_uid, int uid,
        bool can_read, bool can_write, vector <uint8_t> actor_key, vector <uint8_t> user_key){

        FileACList acl;
        AuthorizedUsers auth_users;
        acl.load(parser, acl_addr);
        auth_users.load(parser, authorized_usr_addr);

        bool new_acl_entry = !acl.search(uid);

        if(auth_users.search(uid) == -1){
            StreamTable table;
            table.load(parser, stream_table_addr);
            char name[32] = {};
            strncpy(name, "SYMKEY", 31);
            int symkey_index = table.search(name);
            int actor_index = auth_users.search(actor_uid);
            if(symkey_index == -1 || actor_index == -1) return false;

            int new_index = auth_users.AuthorizedUsers.size();
            StreamDescriptor& symkey = table.stream_desc_vec[symkey_index];

            if(symkey.size > 0){ // файл уже записан, ключ существует, выдаём его новому пользователю
                vector <uint8_t> slot(32);
                parser.read(symkey.addr + actor_index * 32, reinterpret_cast<char*>(slot.data()), 32);
                vector <uint8_t> file_key = encryption.gamma(slot, actor_key);
                vector <uint8_t> new_slot = encryption.gamma(file_key, user_key);

                if(!table.grow(parser, allocator, symkey_index, (new_index + 1) * 32)) return false;
                parser.write(symkey.addr + new_index * 32, reinterpret_cast<char*>(new_slot.data()), 32);
                symkey.size = (new_index + 1) * 32;
                table.save(parser, stream_table_addr);
            } // иначе ключа ещё нет: слоты запишет первый write

            auth_users.add(uid);
            int new_auth_addr = allocator.alloc(sizeof(int) + auth_users.AuthorizedUsers.size() * sizeof(int));
            if(new_auth_addr == -1) return false;
            auth_users.save(parser, new_auth_addr);
            authorized_usr_addr = new_auth_addr;
        }

        acl.grant(uid, can_read, can_write);
        int addr = acl_addr;
        if(new_acl_entry){
            addr = allocator.alloc(sizeof(int) + acl.ACList.size() * sizeof(FileAccessEntry));
            if(addr == -1) return false;
            acl_addr = addr;
            acl_size = acl.ACList.size();
        }
        acl.save(parser, addr);
        return true;
    }


    vector <char> read(Parser& parser, Encryption& encryption, int uid, vector <uint8_t> user_key){
        if(can_read(parser, uid)){
            StreamTable table;
            AuthorizedUsers auth_users;
            table.load(parser, stream_table_addr);
            auth_users.load(parser, authorized_usr_addr);

            char name[32] = {};
            strncpy(name, "SYMKEY", 31);
            int symkey_index = table.search(name);
            int user_index = auth_users.search(uid);

            if(symkey_index != -1 && user_index != -1){
                int addr = table.stream_desc_vec[symkey_index].addr;
                vector <uint8_t> symkey_buffer(32);
                parser.read(addr + (user_index * 32), reinterpret_cast<char*>(symkey_buffer.data()), 32);
                vector <uint8_t> file_key = encryption.gamma(symkey_buffer, user_key);

                strncpy(name, "MAIN", 31);
                int m_index = table.search(name);
                if(m_index == -1) throw runtime_error("не найден поток MAIN при прочтении зшифрованного файла");
                int m_addr = table.stream_desc_vec[m_index].addr;
                int m_size = table.stream_desc_vec[m_index].size;
                vector <uint8_t> data_buffer(m_size);
                parser.read(m_addr, reinterpret_cast<char*>(data_buffer.data()), m_size);

                data_buffer = encryption.gamma(data_buffer, file_key);

                return vector<char>(data_buffer.begin(), data_buffer.end());
            }
            throw runtime_error("не найжен поток SYMEKY или пользователь не может расшифровать файл");
        }
        throw runtime_error("текущий пользователь не может читать из зашифрованного файла");
    }

    void write(Parser& parser, Encryption& encryption, int uid, vector<char> buffer, Allocator& allocator, 
        vector<vector <uint8_t>> all_user_keys){

        if(!can_write(parser, uid)) throw 
        runtime_error("текущий пользователь не может записать в encrypted file descriptor");

        StreamTable table;
        AuthorizedUsers auth_users;
        table.load(parser, stream_table_addr);
        auth_users.load(parser, authorized_usr_addr);

        char name[32] = {};
        strncpy(name, "MAIN", 31);
        int main_index = table.search(name);
        strncpy(name, "SYMKEY", 31);
        int symkey_index = table.search(name);
        int n = auth_users.AuthorizedUsers.size();

        if(main_index == -1 || symkey_index == -1 || auth_users.search(uid) == -1) throw
        runtime_error("не известен индекс потока MAIN, или SYMKEY, или пользователь не имеет доступа к файлу");
        if(all_user_keys.size() < n) throw 
        runtime_error("кол-во ключей пользователей не соответствует кол-ву пользоватлей, имеющих доступ к файлу");

        vector <uint8_t> file_key = encryption.generate_key();
        vector <uint8_t> data_buffer(buffer.begin(), buffer.end());
        data_buffer = encryption.gamma(data_buffer, file_key);
        int size = data_buffer.size();

        // сначала место под оба потока, потом запись
        if(!table.grow(parser, allocator, main_index, size)) throw 
        runtime_error("не получилось увеличить поток MAIN, закончилось место в образе");
        if(!table.grow(parser, allocator, symkey_index, n * 32)) throw 
        runtime_error("не получилось увеличить поток SYMKEY, закончилось место в образе");

        table.stream_desc_vec[main_index].size = size;
        table.stream_desc_vec[symkey_index].size = n * 32;

        parser.write(table.stream_desc_vec[main_index].addr, reinterpret_cast<char*>(data_buffer.data()), size);
        for(int i = 0; i < n; i++){
            vector <uint8_t> slot = encryption.gamma(file_key, all_user_keys[i]);
            parser.write(table.stream_desc_vec[symkey_index].addr + i * 32, reinterpret_cast<char*>(slot.data()), 32);
        }
        table.save(parser, stream_table_addr);

        this->size = size;
        modified_d = time(nullptr);
    }


    void save(Parser& parser, int addr){
        parser.write(addr, reinterpret_cast<char*>(this), 
        sizeof(EncryptedFileDescriptor)); //this - указатель на сам объект
    }

    void load(Parser& parser, int addr){
        parser.read(addr, reinterpret_cast<char*>(this), 
        sizeof(EncryptedFileDescriptor)); //this - указатель на сам объект
    }
};
#pragma pack(pop)