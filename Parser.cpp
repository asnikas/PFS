#include "Parser.h"
#include <ios>
#include <stdexcept>

using namespace std;

Parser::Parser(string path) : path(path) {
    image_file.open(path, ios::in | ios::out | ios::binary); //открыть сразу на чтение и запись

    if(!image_file.is_open()){
        image_file.open(path, ios::out | ios::binary);
        image_file.close();
        image_file.open(path, ios::in | ios::binary | ios::out);
    }
}

void Parser::read(int offset, char* buffer, int n){
    if(offset < 0 || n < 0) throw runtime_error("read in Parser: invalid address " + to_string(offset) +
    " or size " + to_string(n)); //создает объект исключения runtime_error со своим текстом, 
    //запускает раскрутку стека
    //код после throw в этой ф-ии не выполняется

    image_file.clear(); //поток хранит флаги ошибок, которые не дадут корректно прочитать файл, поэтому
    //перед каждым новым чтением очищаем поток

    image_file.seekg(offset);
    image_file.read(buffer, n);

    if(image_file.gcount() != n) throw runtime_error("could not read " + to_string(n) + " bytes at address "
    + to_string(offset)); //gcount возвращает сколько байт прочитала последняя операция неформатированного 
    //чтения на этом потоке
}

void Parser::write(int offset, const char* buffer, int n){
    if(offset < 0 || n < 0) throw runtime_error("write in Parser: invalid address " + to_string(offset) +
    " or size " + to_string(n));

    image_file.clear();

    image_file.seekp(offset);
    image_file.write(buffer, n);
    image_file.flush(); //сбрасывает буфер на диск
    if(!image_file){
        throw runtime_error("could not write to Parser, " + to_string(n) +
    " bytes at address " + to_string(offset));
    }
}

int Parser::file_size(){
    image_file.clear();
    image_file.seekg(0, ios::end);  //переместиться в конец файла
    return static_cast<int>(image_file.tellg()); //спрашиваем, где курсор и возвращаем его позицию
}

void Parser::truncate(){ //стирает содержимое файла образа
    image_file.close(); //надо пересоздать файл, чтобы добавить новый флаг
    image_file.open(path, ios::in | ios::out | ios::binary | ios::trunc ); //trunc - truncate, обнуляет длину файла, отбрасывая содержимое
    if(!image_file.is_open()) throw runtime_error("could not recreate the image file " + path);
}
