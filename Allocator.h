#pragma once //сообщаем компилятору, что если файл подключали в другом .cpp, то больше его не читать
//(избежать дублирования)
#include "Superblock.h"
#include "Parser.h"

//Куча - отрезок образа [START_TRAVERSE; next_free), сплошная цепочка блоков без промежутков.
//Блок: [Header: size + free][данные: size байт][Footer: size]
//alloc возвращает и free принимает адрес данных (адрес блока + HEADER_SIZE).
//Свободные блоки ищутся обходом по header'ам (first-fit), большой блок режется,
//соседние свободные блоки сливаются, свободный блок в конце кучи возвращается хвосту (next_free сдвигается назад).

#pragma pack(push, 1)
struct Header{
    int size;
    bool free;
};

struct Footer{
    int size;
};
#pragma pack(pop)
static_assert(sizeof(Header) == 5, "header size changed");
static_assert(sizeof(Footer) == 4, "footer size changed");

const int MIN_PAYLOAD = 8;
const int START_TRAVERSE = sizeof(Superblock);
const int HEADER_SIZE = sizeof(Header);
const int FOOTER_SIZE = sizeof(Footer);

struct Allocator{
    int next_free, image_size;
    Parser* parser;

    Allocator(Parser& parser, int next_free, int image_size);

    int alloc(int size);

    void free(int address);

    void write_block(int block_addr, int size, bool is_free);

    Header read_header(int block_addr);

    Footer footer_reader(int footer_addr);

    void save(Superblock& superblock);
};
