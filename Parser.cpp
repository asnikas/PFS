#pragma once
#include <iostream>
#include <fstream>
#include <ios>

using namespace std;

class Parser{
    fstream image_file;
public:
    Parser(string path){
        image_file.open(path, ios::in | ios::out | ios::binary); //открыть сразу на чтение и запись

        if(!image_file.is_open()){
            image_file.open(path, ios::out | ios::binary);
            image_file.close();
            image_file.open(path, ios::in | ios::binary | ios::out);
        }
    }

    void read(int offset, char* buffer, int n){
        image_file.seekg(offset);
        image_file.read(buffer, n);
    }

    void write(int offset, const char* buffer, int n){
        image_file.seekp(offset);
        image_file.write(buffer, n);
        image_file.flush(); //сбрасывает буфер на диск
    }

};
