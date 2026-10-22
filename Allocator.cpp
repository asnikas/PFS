#include "Allocator.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

Allocator::Allocator(Parser& parser, int next_free, int image_size){
    this->next_free = next_free;
    this->image_size = image_size;
    this->parser = &parser;
}

int Allocator::alloc(int size){
    if(size < MIN_PAYLOAD) size = MIN_PAYLOAD;

    for(int i = START_TRAVERSE; i < next_free; ){
        Header h = read_header(i);
        if(h.size >= size && h.free){
            if(h.size - size >= HEADER_SIZE + MIN_PAYLOAD + FOOTER_SIZE){
                write_block(i, size, 0);
                write_block(i + HEADER_SIZE + size + FOOTER_SIZE, h.size - size - HEADER_SIZE - FOOTER_SIZE, 1);
            }
            else write_block(i, h.size, 0);
            return i + HEADER_SIZE;
        }
        i += (HEADER_SIZE + h.size + FOOTER_SIZE);
    }
    if(next_free + HEADER_SIZE + size + FOOTER_SIZE <= image_size){
        write_block(next_free, size, 0);
        int result = next_free + HEADER_SIZE;
        next_free += (HEADER_SIZE + size + FOOTER_SIZE);
        return result;
    }
    throw runtime_error("образ закончился, текущий размер - " + to_string(image_size));
}

void Allocator::free(int address){
    if(address - HEADER_SIZE < START_TRAVERSE || address >= next_free) throw
    runtime_error("адрес не находится в пределах [START_TRAVERSE; next_free)");
    bool flag = 1;
    for(int i = START_TRAVERSE; i + HEADER_SIZE <= address; ){
        Header temp_h = read_header(i);
        if(temp_h.size < MIN_PAYLOAD) throw
        runtime_error("куча повреждена при traverse во free");
        if(i + HEADER_SIZE == address){
            flag = 0;
            break;
        }
        i += (HEADER_SIZE + temp_h.size + FOOTER_SIZE);
    }
    if(flag) throw runtime_error("адрес данных во free находится на стыке структур");
    Header h;
    h = read_header(address - HEADER_SIZE);
    Footer temp_f;
    temp_f = footer_reader(address + h.size);
    if(h.size != temp_f.size || h.free ||
        address + h.size + FOOTER_SIZE > next_free || h.size < MIN_PAYLOAD) throw
        runtime_error("нельзя очистить выбранный блок");

    if(address - HEADER_SIZE != START_TRAVERSE){
        Footer f;
        f = footer_reader(address - HEADER_SIZE - FOOTER_SIZE);
        Header bh;
        bh = read_header(address - HEADER_SIZE - FOOTER_SIZE - f.size - HEADER_SIZE);
        if(bh.free){
            address -= (HEADER_SIZE + FOOTER_SIZE + bh.size);
            bh.size += (HEADER_SIZE + h.size + FOOTER_SIZE);
            h = bh;
        }
    }

    if(address + h.size + FOOTER_SIZE < next_free){
        Header fh;
        fh = read_header(address + h.size + FOOTER_SIZE);
        if(fh.free){
            h.size += (HEADER_SIZE + fh.size + FOOTER_SIZE);
        }
    }

    vector <char> nul_vec(h.size);
    parser->write(address, reinterpret_cast<char*>(nul_vec.data()), h.size);

    if(address + h.size + FOOTER_SIZE == next_free) {
        next_free = (address - HEADER_SIZE);
        return;
    }

    write_block(address - HEADER_SIZE, h.size, 1);

}

void Allocator::write_block(int block_addr, int size, bool is_free){
    Header header;
    header.free = is_free;
    header.size = size;
    parser->write(block_addr, reinterpret_cast<char*>(&header), HEADER_SIZE);
    Footer footer;
    footer.size = size;
    parser->write(block_addr + HEADER_SIZE + size, reinterpret_cast<char*>(&footer), FOOTER_SIZE);
}

Header Allocator::read_header(int block_addr){
    Header header;
    parser->read(block_addr, reinterpret_cast<char*>(&header), HEADER_SIZE);
    return header;
}

Footer Allocator::footer_reader(int footer_addr){
    Footer footer;
    parser->read(footer_addr, reinterpret_cast<char*>(&footer), FOOTER_SIZE);
    return footer;
}

void Allocator::save(Superblock& superblock){
    superblock.alloc_free = next_free;
}
