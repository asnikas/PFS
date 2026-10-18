#pragma once
#include <cstdint>
#include <vector>
#include "CommonFileDescriptor.h"
#include "Parser.h"
#include "Allocator.h"
#include "Encryption.h"

#pragma pack(push, 1)
struct EncryptedFileDescriptor : CommonFileDescriptor{
    int authorized_usr_addr;

    void init(int owner_id, int stream_table_addr, int stream_table_size, int acl_addr,
        int acl_size, int authorized_usr_addr);

    void get_info(Parser& parser);

        // actor_* - тот, кто выдаёт права (у него уже есть доступ к ключу файла)
    // после вызова описатель нужно сохранить: acl_addr и authorized_usr_addr могут измениться
    bool set_acl(Parser& parser, Allocator& allocator, Encryption& encryption, int actor_uid,
        int uid, bool can_read, bool can_write, std::vector <uint8_t> actor_key,
        std::vector <uint8_t> user_key);

    std::vector <char> read(Parser& parser, Encryption& encryption, int uid,
        std::vector <uint8_t> user_key);

    void write(Parser& parser, Encryption& encryption, int uid, std::vector<char> buffer,
        Allocator& allocator, std::vector<std::vector <uint8_t>> all_user_keys);

    void save(Parser& parser, int addr);

    void load(Parser& parser, int addr);
};
#pragma pack(pop)
