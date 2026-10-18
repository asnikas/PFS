#pragma once
#include <fstream>
#include <string>

class Parser{
    std::fstream image_file;
    std::string path;
public:
    Parser(std::string path);

    void read(int offset, char* buffer, int n);

    void write(int offset, const char* buffer, int n);

    int file_size();

    void truncate();
};
