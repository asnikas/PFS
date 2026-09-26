#include <iostream>
#include <vector>
#include <cstring>
#include "StreamDescriptor.cpp"
#include "Parser.cpp"

using namespace std;

struct StreamTable{
    vector <StreamDescriptor> stream_desc_vec;

    int search(char name[32]){
        for(int i = 0; i < stream_desc_vec.size(); i++){
            if(memcmp(name, stream_desc_vec[i].name, 32) == 0) return i;
        }
        return -1;
    }

    void add(char name[32], int addr, int size, int capacity){
        if(search(name) == -1){
            stream_desc_vec.emplace_back(name, addr, size, capacity);
        }
    }

    void remove(char name[32]){
        int index = search(name);
        if(index != -1){
            stream_desc_vec.erase(stream_desc_vec.begin() + index);
        }
    }

    void save(Parser& parser, int addr){
        int size = stream_desc_vec.size();
        parser.write(addr, reinterpret_cast<char*>(&size), sizeof(size));
        parser.write(addr + sizeof(size), reinterpret_cast<char*>(stream_desc_vec.data()),
        size * sizeof(StreamDescriptor));
    }

    void load(Parser& parser, int addr){
        int size;
        parser.read(addr, reinterpret_cast<char*>(&size), sizeof(size));
        if(size < 0 || size > 1000){
            cout << "невалидный размер size для StreamTable при чтении из Parser\n";
            return;
        }
        stream_desc_vec.resize(size); //выделяем место для вектора
        parser.read(addr + sizeof(size), reinterpret_cast<char*>(stream_desc_vec.data()), 
        size * sizeof(StreamDescriptor));
    }
};