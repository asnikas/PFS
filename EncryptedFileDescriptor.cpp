#include <iostream>
#include <ctime>
#include <vector>
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

    optional <vector <char>> read(Parser& parser, Encryption& encryption, int uid, vector <uint8_t> user_key){
        if(can_read(parser, uid)){
            StreamTable table;
            AuthorizedUsers auth_users;
            table.load(parser, stream_table_addr);
            auth_users.load(parser, authorized_usr_addr);

            char name[32] = {};
            strcpy(name, "SYMKEY");
            int symkey_index = table.search(name);
            int user_index = auth_users.search(uid);

            if(symkey_index != -1 && user_index != -1){
                int addr = table.stream_desc_vec[symkey_index].addr;
                vector <uint8_t> symkey_buffer(32);
                parser.read(addr + (user_index * 32), reinterpret_cast<char*>(symkey_buffer.data()), 32);
                vector <uint8_t> file_key = encryption.gamma(symkey_buffer, user_key);

                strcpy(name, "MAIN");
                int m_index = table.search(name);
                int m_addr = table.stream_desc_vec[m_index].addr;
                int m_size = table.stream_desc_vec[m_index].size;
                vector <uint8_t> data_buffer(m_size);
                parser.read(m_addr, reinterpret_cast<char*>(data_buffer.data()), m_size);

                data_buffer = encryption.gamma(data_buffer, file_key);

                return vector<char>(data_buffer.begin(), data_buffer.end());
            }
        }
        return nullopt; //сигнал "не прочитано" вместо пустого вектора
    }

    void write(Parser& parser, Encryption& encryption, int uid, vector<char> buffer, Allocator& allocator, 
        vector<vector <uint8_t>> all_user_keys){

        if(can_write(parser, uid)){
            modified_d = time(nullptr);
            StreamTable table;
            AuthorizedUsers auth_users;
            table.load(parser, stream_table_addr);
            auth_users.load(parser, authorized_usr_addr);
            char name[32] = {};
            strcpy(name, "MAIN");
            int index = table.search(name);
            int user_index = auth_users.search(uid);

            if(index != -1 && user_index != -1){

                vector <uint8_t> data_buffer(buffer.begin(), buffer.end());
                vector<uint8_t> file_key = encryption.generate_key();

                data_buffer = encryption.gamma(data_buffer, file_key);
                
                int size = data_buffer.size();
                if(size > table.stream_desc_vec[index].capacity){
                    int new_addr = allocator.alloc(size);
                    table.stream_desc_vec[index].addr = new_addr;  
                    table.stream_desc_vec[index].capacity = size; 
                }

                table.stream_desc_vec[index].size = size;
                this->size = size;
                table.save(parser, stream_table_addr);
                parser.write(table.stream_desc_vec[index].addr, reinterpret_cast<char*>(data_buffer.data()), size);

                strcpy(name, "SYMKEY");
                int symkey_index = table.search(name);
                if(symkey_index != -1){
                    vector <uint8_t> symkey_buffer(32);
                    int symkey_addr = table.stream_desc_vec[symkey_index].addr;
                    for(int i = 0; i < auth_users.AuthorizedUsers.size(); i++){
                        symkey_buffer = encryption.gamma(file_key, all_user_keys[i]);
                        parser.write(symkey_addr + (i * 32), reinterpret_cast<char*>(symkey_buffer.data()), 32);
                    }
                }
            }
        }
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