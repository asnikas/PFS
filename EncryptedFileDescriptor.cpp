#include "EncryptedFileDescriptor.h"
#include "AuthorizedUsers.h"
#include "StreamTable.h"
#include "FileACList.h"
#include <cstring> //позволяет работать с С-строками и сырой памятью, ф-ии унаследованы от С,
//string - это класс С++, а cstring - набор ф-ий
#include <iostream>
#include <stdexcept>

using namespace std;

void EncryptedFileDescriptor::init(int owner_id, int stream_table_addr, int stream_table_size, int acl_addr, int acl_size, int authorized_usr_addr){
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

void EncryptedFileDescriptor::get_info(Parser& parser){

    CommonFileDescriptor::get_info(parser);

    cout << "This is an encrypted file" << endl;
    
    AuthorizedUsers au;
    au.load(parser, authorized_usr_addr);
    cout << "Authorized users: " << au.AuthorizedUsers.size() << endl;
}

bool EncryptedFileDescriptor::set_acl(Parser& parser, Allocator& allocator, Encryption& encryption, int actor_uid, int uid, bool can_read, bool can_write, vector <uint8_t> actor_key, vector <uint8_t> user_key){

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

            int old_symkey_addr = symkey.addr;
            if(!table.grow(parser, allocator, symkey_index, (new_index + 1) * 32)) return false;
            parser.write(symkey.addr + new_index * 32, reinterpret_cast<char*>(new_slot.data()), 32);
            symkey.size = (new_index + 1) * 32;
            table.save(parser, stream_table_addr);
            if(symkey.addr != old_symkey_addr) allocator.free(old_symkey_addr); //grow уже скопировал старые слоты
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

vector <char> EncryptedFileDescriptor::read(Parser& parser, Encryption& encryption, int uid, vector <uint8_t> user_key){
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
            if(m_index == -1) throw runtime_error("MAIN stream not found when reading the encrypted file");
            int m_addr = table.stream_desc_vec[m_index].addr;
            int m_size = table.stream_desc_vec[m_index].size;
            vector <uint8_t> data_buffer(m_size);
            parser.read(m_addr, reinterpret_cast<char*>(data_buffer.data()), m_size);

            data_buffer = encryption.gamma(data_buffer, file_key);

            return vector<char>(data_buffer.begin(), data_buffer.end());
        }
        throw runtime_error("SYMKEY stream not found or the user cannot decrypt the file");
    }
    throw runtime_error("current user cannot read this encrypted file");
}

void EncryptedFileDescriptor::write(Parser& parser, Encryption& encryption, int uid, vector<char> buffer, Allocator& allocator, vector<vector <uint8_t>> all_user_keys){

    if(!can_write(parser, uid)) throw 
    runtime_error("current user cannot write to this encrypted file");

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
    runtime_error("MAIN or SYMKEY stream index is unknown, or the user has no access to the file");
    if(all_user_keys.size() < n) throw 
    runtime_error("number of user keys does not match the number of users with access to the file");

    vector <uint8_t> file_key = encryption.generate_key();
    vector <uint8_t> data_buffer(buffer.begin(), buffer.end());
    data_buffer = encryption.gamma(data_buffer, file_key);
    int size = data_buffer.size();

    //grow меняет адрес потока, если он не помещается; старые блоки освобождаем после table.save
    int old_main_addr = table.stream_desc_vec[main_index].addr;
    int old_symkey_addr = table.stream_desc_vec[symkey_index].addr;

    try{
        // сначала место под оба потока, потом запись
        if(!table.grow(parser, allocator, main_index, size)) throw
        runtime_error("could not grow the MAIN stream: the image is out of space");
        if(!table.grow(parser, allocator, symkey_index, n * 32)) throw
        runtime_error("could not grow the SYMKEY stream: the image is out of space");

        table.stream_desc_vec[main_index].size = size;
        table.stream_desc_vec[symkey_index].size = n * 32;

        parser.write(table.stream_desc_vec[main_index].addr, reinterpret_cast<char*>(data_buffer.data()), size);
        for(int i = 0; i < n; i++){
            vector <uint8_t> slot = encryption.gamma(file_key, all_user_keys[i]);
            parser.write(table.stream_desc_vec[symkey_index].addr + i * 32, reinterpret_cast<char*>(slot.data()), 32);
        }
    }
    catch(...){ //таблица потоков еще не сохранена: новые блоки от grow ни к чему не привязаны
        //(например, MAIN вырос, а на SYMKEY закончилось место)
        if(table.stream_desc_vec[main_index].addr != old_main_addr) allocator.free(table.stream_desc_vec[main_index].addr);
        if(table.stream_desc_vec[symkey_index].addr != old_symkey_addr) allocator.free(table.stream_desc_vec[symkey_index].addr);
        throw;
    }
    table.save(parser, stream_table_addr);
    if(table.stream_desc_vec[main_index].addr != old_main_addr) allocator.free(old_main_addr);
    if(table.stream_desc_vec[symkey_index].addr != old_symkey_addr) allocator.free(old_symkey_addr);

    this->size = size;
    modified_d = time(nullptr);
}

void EncryptedFileDescriptor::save(Parser& parser, int addr){
    parser.write(addr, reinterpret_cast<char*>(this), 
    sizeof(EncryptedFileDescriptor)); //this - указатель на сам объект
}

void EncryptedFileDescriptor::load(Parser& parser, int addr){
    parser.read(addr, reinterpret_cast<char*>(this), 
    sizeof(EncryptedFileDescriptor)); //this - указатель на сам объект
}
